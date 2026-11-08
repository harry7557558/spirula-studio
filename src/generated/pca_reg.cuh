#pragma once

#include "generated/slang.cuh"

inline __device__ bool all_0(bool3  x_0)
{
    bool result_0 = true;
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
        if(result_0)
        {
            result_0 = (bool((_slang_vector_get_element(x_0, i_0))));
        }
        else
        {
            result_0 = false;
        }
        i_0 = i_0 + int(1);
    }
    return result_0;
}

inline __device__ uint pca_order_key(float f_0)
{
    uint u_0 = (F32_asuint((f_0)));
    uint _S1;
    if((u_0 & 2147483648U) != 0U)
    {
        _S1 = ~u_0;
    }
    else
    {
        _S1 = u_0 | 2147483648U;
    }
    return _S1;
}

inline __device__ float pca_order_float(uint u_1)
{
    uint _S2;
    if((u_1 & 2147483648U) != 0U)
    {
        _S2 = u_1 & 2147483647U;
    }
    else
    {
        _S2 = ~u_1;
    }
    return (U32_asfloat((_S2)));
}

inline __device__ float3  min_0(float3  x_1, float3  y_0)
{
    float3  result_1;
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
        *_slang_vector_get_element_ptr(&result_1, i_1) = (F32_min((_slang_vector_get_element(x_1, i_1)), (_slang_vector_get_element(y_0, i_1))));
        i_1 = i_1 + int(1);
    }
    return result_1;
}

struct DiffPair_float_0
{
    float primal_0;
    float differential_0;
};

inline __device__ void _d_max_0(DiffPair_float_0 * dpx_0, DiffPair_float_0 * dpy_0, float dOut_0)
{
    DiffPair_float_0 _S3 = *dpx_0;
    float _S4;
    if(((*dpx_0).primal_0) > ((*dpy_0).primal_0))
    {
        _S4 = dOut_0;
    }
    else
    {
        if(((*dpx_0).primal_0) < ((*dpy_0).primal_0))
        {
            _S4 = 0.0f;
        }
        else
        {
            _S4 = 0.5f * dOut_0;
        }
    }
    dpx_0->primal_0 = _S3.primal_0;
    dpx_0->differential_0 = _S4;
    DiffPair_float_0 _S5 = *dpy_0;
    if(((*dpy_0).primal_0) > (_S3.primal_0))
    {
        _S4 = dOut_0;
    }
    else
    {
        if(((*dpy_0).primal_0) < ((*dpx_0).primal_0))
        {
            _S4 = 0.0f;
        }
        else
        {
            _S4 = 0.5f * dOut_0;
        }
    }
    dpy_0->primal_0 = _S5.primal_0;
    dpy_0->differential_0 = _S4;
    return;
}

inline __device__ float3  max_0(float3  x_2, float3  y_1)
{
    float3  result_2;
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
        *_slang_vector_get_element_ptr(&result_2, i_2) = (F32_max((_slang_vector_get_element(x_2, i_2)), (_slang_vector_get_element(y_1, i_2))));
        i_2 = i_2 + int(1);
    }
    return result_2;
}

inline __device__ float3  clamp_0(float3  x_3, float3  minBound_0, float3  maxBound_0)
{
    return min_0(max_0(x_3, minBound_0), maxBound_0);
}

inline __device__ uint pca_expand10_0(uint v_0)
{
    return ((((((((v_0 & 1023U) * 65537U) & 4278190335U) * 257U) & 251719695U) * 17U) & 3272356035U) * 5U) & 1227133513U;
}

inline __device__ uint2  pca_morton63(float3  p_0, float3  lo_0, float inv_side_0)
{
    float3  u_2 = clamp_0((p_0 - lo_0) * make_float3 (inv_side_0), make_float3 (0.0f), make_float3 (1.0f)) * make_float3 (2.097151e+06f);
    uint x_4 = uint(u_2.x);
    uint y_2 = uint(u_2.y);
    uint z_0 = uint(u_2.z);
    return make_uint2 (((((pca_expand10_0(x_4)) | ((pca_expand10_0(y_2)) << int(1))) | ((pca_expand10_0(z_0)) << int(2))) | (((x_4 >> int(10)) & 1U) << int(30))) | (((y_2 >> int(10)) & 1U) << int(31)), ((((z_0 >> int(10)) & 1U) | ((pca_expand10_0(x_4 >> int(11))) << int(1))) | ((pca_expand10_0(y_2 >> int(11))) << int(2))) | ((pca_expand10_0(z_0 >> int(11))) << int(3)));
}

inline __device__ uint2  pca_reg_key(float3  p_1, uint * box_0)
{
    float _S6 = pca_order_float(*(box_0 + int(0)));
    float _S7 = pca_order_float(*(box_0 + int(1)));
    float _S8 = pca_order_float(*(box_0 + int(2)));
    return pca_morton63(p_1, make_float3 (_S6, _S7, _S8), 1.0f / (F32_max(((F32_max(((F32_max((pca_order_float(*(box_0 + int(3))) - _S6), (pca_order_float(*(box_0 + int(4))) - _S7)))), (pca_order_float(*(box_0 + int(5))) - _S8)))), (1.00000000317107685e-30f))));
}

inline __device__ float3  pca_load3_0(float * p_2, uint i_3)
{
    uint _S9 = 3U * i_3;
    return make_float3 (*(p_2 + _S9), *(p_2 + (_S9 + 1U)), *(p_2 + (_S9 + 2U)));
}

inline __device__ void pca_reg_gather(uint r_0, int * perm_0, float * means_0, float4  * sorted_0, int * perm_out_0, uint write_perm_0)
{
    int * _S10 = perm_0 + r_0;
    int id_0 = *_S10;
    float4  * _S11 = sorted_0 + r_0;
    float3  _S12 = pca_load3_0(means_0, uint(*_S10));
    *_S11 = make_float4 (_S12.x, _S12.y, _S12.z, 0.0f);
    if(write_perm_0 != 0U)
    {
        *(perm_out_0 + r_0) = id_0;
    }
    return;
}

inline __device__ uint3  pca_reg_delta_0(uint i_4, uint n_0, float4  * sorted_1, uint * box_1)
{
    if(i_4 >= (n_0 - 1U))
    {
        return make_uint3 (4294967295U);
    }
    uint2  a_0 = pca_reg_key(float3 {(*(sorted_1 + i_4)).x, (*(sorted_1 + i_4)).y, (*(sorted_1 + i_4)).z}, box_1);
    uint _S13 = i_4 + 1U;
    uint2  b_0 = pca_reg_key(float3 {(*(sorted_1 + _S13)).x, (*(sorted_1 + _S13)).y, (*(sorted_1 + _S13)).z}, box_1);
    return make_uint3 ((a_0.y) ^ (b_0.y), (a_0.x) ^ (b_0.x), i_4 ^ _S13);
}

inline __device__ bool pca_reg_less_0(uint3  a_1, uint3  b_1)
{
    uint _S14 = a_1.x;
    uint _S15 = b_1.x;
    bool _S16;
    if(_S14 != _S15)
    {
        _S16 = _S14 < _S15;
    }
    else
    {
        uint _S17 = a_1.y;
        uint _S18 = b_1.y;
        if(_S17 != _S18)
        {
            _S16 = _S17 < _S18;
        }
        else
        {
            _S16 = (a_1.z) < (b_1.z);
        }
    }
    return _S16;
}

inline __device__ void pca_reg_build(uint i_5, uint n_1, float4  * sorted_2, uint * box_2, float4  * nodes_0, uint * node_bits_0, int4  * meta_0, int * ranges_0)
{
    int _S19 = int(i_5 | 2147483648U);
    float3  lo_1 = float3 {(*(sorted_2 + i_5)).x, (*(sorted_2 + i_5)).y, (*(sorted_2 + i_5)).z};
    uint3  _S20 = pca_reg_delta_0(i_5 - 1U, n_1, sorted_2, box_2);
    uint3  _S21 = pca_reg_delta_0(i_5, n_1, sorted_2, box_2);
    uint3  dr_0 = _S21;
    uint3  dl_0 = _S20;
    uint last_0 = i_5;
    uint first_0 = i_5;
    int current_0 = _S19;
    float3  lo_2 = lo_1;
    float3  hi_0 = lo_1;
    for(;;)
    {
        bool left_0 = pca_reg_less_0(dr_0, dl_0);
        uint p_3;
        if(left_0)
        {
            p_3 = last_0;
        }
        else
        {
            p_3 = first_0 - 1U;
        }
        if(left_0)
        {
            *&((meta_0 + p_3)->x) = current_0;
        }
        else
        {
            *&((meta_0 + p_3)->y) = current_0;
        }
        __threadfence();
        int * _S22 = ranges_0 + p_3;
        uint _S23;
        if(left_0)
        {
            _S23 = first_0;
        }
        else
        {
            _S23 = last_0;
        }
        int other_0 = atomicCAS(_S22, int(-1), int(_S23));
        if(other_0 == int(-1))
        {
            return;
        }
        if(left_0)
        {
            last_0 = uint(other_0);
        }
        else
        {
            first_0 = uint(other_0);
        }
        __threadfence();
        int sib_0;
        if(left_0)
        {
            int _S24 = atomicOr(&((meta_0 + p_3)->y), int(0));
            sib_0 = _S24;
        }
        else
        {
            int _S25 = atomicOr(&((meta_0 + p_3)->x), int(0));
            sib_0 = _S25;
        }
        uint _S26 = uint(sib_0);
        if((_S26 & 2147483648U) != 0U)
        {
            float3  q_0 = float3 {(*(sorted_2 + (_S26 & 2147483647U))).x, (*(sorted_2 + (_S26 & 2147483647U))).y, (*(sorted_2 + (_S26 & 2147483647U))).z};
            float3  _S27 = max_0(hi_0, q_0);
            lo_2 = min_0(lo_2, q_0);
            hi_0 = _S27;
        }
        else
        {
            uint _S28 = 8U * _S26;
            uint b0_0 = atomicOr(node_bits_0 + _S28, 0U);
            uint b1_0 = atomicOr(node_bits_0 + (_S28 + 1U), 0U);
            uint b2_0 = atomicOr(node_bits_0 + (_S28 + 2U), 0U);
            uint b4_0 = atomicOr(node_bits_0 + (_S28 + 4U), 0U);
            uint b5_0 = atomicOr(node_bits_0 + (_S28 + 5U), 0U);
            uint b6_0 = atomicOr(node_bits_0 + (_S28 + 6U), 0U);
            float3  _S29 = max_0(hi_0, make_float3 ((U32_asfloat((b4_0))), (U32_asfloat((b5_0))), (U32_asfloat((b6_0)))));
            lo_2 = min_0(lo_2, make_float3 ((U32_asfloat((b0_0))), (U32_asfloat((b1_0))), (U32_asfloat((b2_0)))));
            hi_0 = _S29;
        }
        uint _S30 = 2U * p_3;
        *(nodes_0 + _S30) = make_float4 (lo_2.x, lo_2.y, lo_2.z, 0.0f);
        *(nodes_0 + (_S30 + 1U)) = make_float4 (hi_0.x, hi_0.y, hi_0.z, 0.0f);
        int4  * _S31 = meta_0 + p_3;
        *&(_S31->z) = int(first_0);
        *&(_S31->w) = int(last_0);
        int _S32 = int(p_3);
        bool _S33;
        if(first_0 == 0U)
        {
            _S33 = last_0 == (n_1 - 1U);
        }
        else
        {
            _S33 = false;
        }
        if(_S33)
        {
            *(ranges_0 + (n_1 - 1U)) = _S32;
            return;
        }
        if(left_0)
        {
            uint3  _S34 = pca_reg_delta_0(last_0, n_1, sorted_2, box_2);
            dr_0 = _S34;
        }
        else
        {
            uint3  _S35 = pca_reg_delta_0(first_0 - 1U, n_1, sorted_2, box_2);
            dl_0 = _S35;
        }
        current_0 = _S32;
    }
}

struct PcaKnn_0
{
    FixedArray<float, 8>  d_0;
    FixedArray<uint, 8>  id_1;
    float worst_0;
};

inline __device__ uint PcaKnn_count_0(PcaKnn_0 * this_0, uint k_0)
{
    bool _S36;
    if(0U < k_0)
    {
        _S36 = (this_0->d_0[int(0)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S36 = false;
    }
    uint c_0;
    if(_S36)
    {
        c_0 = 1U;
    }
    else
    {
        c_0 = 0U;
    }
    if(1U < k_0)
    {
        _S36 = (this_0->d_0[int(1)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S36 = false;
    }
    if(_S36)
    {
        c_0 = c_0 + 1U;
    }
    if(2U < k_0)
    {
        _S36 = (this_0->d_0[int(2)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S36 = false;
    }
    if(_S36)
    {
        c_0 = c_0 + 1U;
    }
    if(3U < k_0)
    {
        _S36 = (this_0->d_0[int(3)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S36 = false;
    }
    if(_S36)
    {
        c_0 = c_0 + 1U;
    }
    if(4U < k_0)
    {
        _S36 = (this_0->d_0[int(4)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S36 = false;
    }
    if(_S36)
    {
        c_0 = c_0 + 1U;
    }
    if(5U < k_0)
    {
        _S36 = (this_0->d_0[int(5)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S36 = false;
    }
    if(_S36)
    {
        c_0 = c_0 + 1U;
    }
    if(6U < k_0)
    {
        _S36 = (this_0->d_0[int(6)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S36 = false;
    }
    if(_S36)
    {
        c_0 = c_0 + 1U;
    }
    if(7U < k_0)
    {
        _S36 = (this_0->d_0[int(7)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S36 = false;
    }
    if(_S36)
    {
        c_0 = c_0 + 1U;
    }
    return c_0;
}

struct PcaKnn_1
{
    FixedArray<float, 16>  d_0;
    FixedArray<uint, 16>  id_1;
    float worst_0;
};

inline __device__ uint PcaKnn_count_1(PcaKnn_1 * this_1, uint k_1)
{
    bool _S37;
    if(0U < k_1)
    {
        _S37 = (this_1->d_0[int(0)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S37 = false;
    }
    uint c_1;
    if(_S37)
    {
        c_1 = 1U;
    }
    else
    {
        c_1 = 0U;
    }
    if(1U < k_1)
    {
        _S37 = (this_1->d_0[int(1)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S37 = false;
    }
    if(_S37)
    {
        c_1 = c_1 + 1U;
    }
    if(2U < k_1)
    {
        _S37 = (this_1->d_0[int(2)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S37 = false;
    }
    if(_S37)
    {
        c_1 = c_1 + 1U;
    }
    if(3U < k_1)
    {
        _S37 = (this_1->d_0[int(3)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S37 = false;
    }
    if(_S37)
    {
        c_1 = c_1 + 1U;
    }
    if(4U < k_1)
    {
        _S37 = (this_1->d_0[int(4)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S37 = false;
    }
    if(_S37)
    {
        c_1 = c_1 + 1U;
    }
    if(5U < k_1)
    {
        _S37 = (this_1->d_0[int(5)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S37 = false;
    }
    if(_S37)
    {
        c_1 = c_1 + 1U;
    }
    if(6U < k_1)
    {
        _S37 = (this_1->d_0[int(6)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S37 = false;
    }
    if(_S37)
    {
        c_1 = c_1 + 1U;
    }
    if(7U < k_1)
    {
        _S37 = (this_1->d_0[int(7)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S37 = false;
    }
    if(_S37)
    {
        c_1 = c_1 + 1U;
    }
    if(8U < k_1)
    {
        _S37 = (this_1->d_0[int(8)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S37 = false;
    }
    if(_S37)
    {
        c_1 = c_1 + 1U;
    }
    if(9U < k_1)
    {
        _S37 = (this_1->d_0[int(9)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S37 = false;
    }
    if(_S37)
    {
        c_1 = c_1 + 1U;
    }
    if(10U < k_1)
    {
        _S37 = (this_1->d_0[int(10)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S37 = false;
    }
    if(_S37)
    {
        c_1 = c_1 + 1U;
    }
    if(11U < k_1)
    {
        _S37 = (this_1->d_0[int(11)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S37 = false;
    }
    if(_S37)
    {
        c_1 = c_1 + 1U;
    }
    if(12U < k_1)
    {
        _S37 = (this_1->d_0[int(12)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S37 = false;
    }
    if(_S37)
    {
        c_1 = c_1 + 1U;
    }
    if(13U < k_1)
    {
        _S37 = (this_1->d_0[int(13)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S37 = false;
    }
    if(_S37)
    {
        c_1 = c_1 + 1U;
    }
    if(14U < k_1)
    {
        _S37 = (this_1->d_0[int(14)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S37 = false;
    }
    if(_S37)
    {
        c_1 = c_1 + 1U;
    }
    if(15U < k_1)
    {
        _S37 = (this_1->d_0[int(15)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S37 = false;
    }
    if(_S37)
    {
        c_1 = c_1 + 1U;
    }
    return c_1;
}

struct PcaKnn_2
{
    FixedArray<float, 32>  d_0;
    FixedArray<uint, 32>  id_1;
    float worst_0;
};

inline __device__ uint PcaKnn_count_2(PcaKnn_2 * this_2, uint k_2)
{
    bool _S38;
    if(0U < k_2)
    {
        _S38 = (this_2->d_0[int(0)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    uint c_2;
    if(_S38)
    {
        c_2 = 1U;
    }
    else
    {
        c_2 = 0U;
    }
    if(1U < k_2)
    {
        _S38 = (this_2->d_0[int(1)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(2U < k_2)
    {
        _S38 = (this_2->d_0[int(2)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(3U < k_2)
    {
        _S38 = (this_2->d_0[int(3)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(4U < k_2)
    {
        _S38 = (this_2->d_0[int(4)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(5U < k_2)
    {
        _S38 = (this_2->d_0[int(5)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(6U < k_2)
    {
        _S38 = (this_2->d_0[int(6)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(7U < k_2)
    {
        _S38 = (this_2->d_0[int(7)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(8U < k_2)
    {
        _S38 = (this_2->d_0[int(8)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(9U < k_2)
    {
        _S38 = (this_2->d_0[int(9)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(10U < k_2)
    {
        _S38 = (this_2->d_0[int(10)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(11U < k_2)
    {
        _S38 = (this_2->d_0[int(11)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(12U < k_2)
    {
        _S38 = (this_2->d_0[int(12)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(13U < k_2)
    {
        _S38 = (this_2->d_0[int(13)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(14U < k_2)
    {
        _S38 = (this_2->d_0[int(14)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(15U < k_2)
    {
        _S38 = (this_2->d_0[int(15)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(16U < k_2)
    {
        _S38 = (this_2->d_0[int(16)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(17U < k_2)
    {
        _S38 = (this_2->d_0[int(17)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(18U < k_2)
    {
        _S38 = (this_2->d_0[int(18)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(19U < k_2)
    {
        _S38 = (this_2->d_0[int(19)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(20U < k_2)
    {
        _S38 = (this_2->d_0[int(20)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(21U < k_2)
    {
        _S38 = (this_2->d_0[int(21)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(22U < k_2)
    {
        _S38 = (this_2->d_0[int(22)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(23U < k_2)
    {
        _S38 = (this_2->d_0[int(23)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(24U < k_2)
    {
        _S38 = (this_2->d_0[int(24)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(25U < k_2)
    {
        _S38 = (this_2->d_0[int(25)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(26U < k_2)
    {
        _S38 = (this_2->d_0[int(26)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(27U < k_2)
    {
        _S38 = (this_2->d_0[int(27)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(28U < k_2)
    {
        _S38 = (this_2->d_0[int(28)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(29U < k_2)
    {
        _S38 = (this_2->d_0[int(29)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(30U < k_2)
    {
        _S38 = (this_2->d_0[int(30)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    if(31U < k_2)
    {
        _S38 = (this_2->d_0[int(31)]) < 3.00000000549775576e+38f;
    }
    else
    {
        _S38 = false;
    }
    if(_S38)
    {
        c_2 = c_2 + 1U;
    }
    return c_2;
}

inline __device__ void PcaKnn_insert_0(PcaKnn_0 * this_3, uint k_3, float dist_0, uint i_6)
{
    bool take_0 = dist_0 < this_3->d_0[int(0)];
    float td_0 = this_3->d_0[int(0)];
    uint ti_0 = this_3->id_1[int(0)];
    float _S39;
    if(take_0)
    {
        _S39 = dist_0;
    }
    else
    {
        _S39 = td_0;
    }
    this_3->d_0[int(0)] = _S39;
    uint _S40;
    if(take_0)
    {
        _S40 = i_6;
    }
    else
    {
        _S40 = ti_0;
    }
    this_3->id_1[int(0)] = _S40;
    if(take_0)
    {
        _S39 = td_0;
    }
    else
    {
        _S39 = dist_0;
    }
    if(take_0)
    {
        _S40 = ti_0;
    }
    else
    {
        _S40 = i_6;
    }
    bool take_1 = _S39 < this_3->d_0[int(1)];
    float td_1 = this_3->d_0[int(1)];
    uint ti_1 = this_3->id_1[int(1)];
    float _S41;
    if(take_1)
    {
        _S41 = _S39;
    }
    else
    {
        _S41 = td_1;
    }
    this_3->d_0[int(1)] = _S41;
    uint _S42;
    if(take_1)
    {
        _S42 = _S40;
    }
    else
    {
        _S42 = ti_1;
    }
    this_3->id_1[int(1)] = _S42;
    if(take_1)
    {
        _S39 = td_1;
    }
    if(take_1)
    {
        _S40 = ti_1;
    }
    bool take_2 = _S39 < this_3->d_0[int(2)];
    float td_2 = this_3->d_0[int(2)];
    uint ti_2 = this_3->id_1[int(2)];
    if(take_2)
    {
        _S41 = _S39;
    }
    else
    {
        _S41 = td_2;
    }
    this_3->d_0[int(2)] = _S41;
    if(take_2)
    {
        _S42 = _S40;
    }
    else
    {
        _S42 = ti_2;
    }
    this_3->id_1[int(2)] = _S42;
    if(take_2)
    {
        _S39 = td_2;
    }
    if(take_2)
    {
        _S40 = ti_2;
    }
    bool take_3 = _S39 < this_3->d_0[int(3)];
    float td_3 = this_3->d_0[int(3)];
    uint ti_3 = this_3->id_1[int(3)];
    if(take_3)
    {
        _S41 = _S39;
    }
    else
    {
        _S41 = td_3;
    }
    this_3->d_0[int(3)] = _S41;
    if(take_3)
    {
        _S42 = _S40;
    }
    else
    {
        _S42 = ti_3;
    }
    this_3->id_1[int(3)] = _S42;
    if(take_3)
    {
        _S39 = td_3;
    }
    if(take_3)
    {
        _S40 = ti_3;
    }
    bool take_4 = _S39 < this_3->d_0[int(4)];
    float td_4 = this_3->d_0[int(4)];
    uint ti_4 = this_3->id_1[int(4)];
    if(take_4)
    {
        _S41 = _S39;
    }
    else
    {
        _S41 = td_4;
    }
    this_3->d_0[int(4)] = _S41;
    if(take_4)
    {
        _S42 = _S40;
    }
    else
    {
        _S42 = ti_4;
    }
    this_3->id_1[int(4)] = _S42;
    if(take_4)
    {
        _S39 = td_4;
    }
    if(take_4)
    {
        _S40 = ti_4;
    }
    bool take_5 = _S39 < this_3->d_0[int(5)];
    float td_5 = this_3->d_0[int(5)];
    uint ti_5 = this_3->id_1[int(5)];
    if(take_5)
    {
        _S41 = _S39;
    }
    else
    {
        _S41 = td_5;
    }
    this_3->d_0[int(5)] = _S41;
    if(take_5)
    {
        _S42 = _S40;
    }
    else
    {
        _S42 = ti_5;
    }
    this_3->id_1[int(5)] = _S42;
    if(take_5)
    {
        _S39 = td_5;
    }
    if(take_5)
    {
        _S40 = ti_5;
    }
    bool take_6 = _S39 < this_3->d_0[int(6)];
    float td_6 = this_3->d_0[int(6)];
    uint ti_6 = this_3->id_1[int(6)];
    if(take_6)
    {
        _S41 = _S39;
    }
    else
    {
        _S41 = td_6;
    }
    this_3->d_0[int(6)] = _S41;
    if(take_6)
    {
        _S42 = _S40;
    }
    else
    {
        _S42 = ti_6;
    }
    this_3->id_1[int(6)] = _S42;
    if(take_6)
    {
        _S39 = td_6;
    }
    if(take_6)
    {
        _S40 = ti_6;
    }
    bool take_7 = _S39 < this_3->d_0[int(7)];
    float td_7 = this_3->d_0[int(7)];
    uint ti_7 = this_3->id_1[int(7)];
    if(take_7)
    {
    }
    else
    {
        _S39 = td_7;
    }
    this_3->d_0[int(7)] = _S39;
    if(take_7)
    {
    }
    else
    {
        _S40 = ti_7;
    }
    this_3->id_1[int(7)] = _S40;
    if(1U == k_3)
    {
        this_3->worst_0 = this_3->d_0[int(0)];
    }
    if(2U == k_3)
    {
        this_3->worst_0 = this_3->d_0[int(1)];
    }
    if(3U == k_3)
    {
        this_3->worst_0 = this_3->d_0[int(2)];
    }
    if(4U == k_3)
    {
        this_3->worst_0 = this_3->d_0[int(3)];
    }
    if(5U == k_3)
    {
        this_3->worst_0 = this_3->d_0[int(4)];
    }
    if(6U == k_3)
    {
        this_3->worst_0 = this_3->d_0[int(5)];
    }
    if(7U == k_3)
    {
        this_3->worst_0 = this_3->d_0[int(6)];
    }
    if(8U == k_3)
    {
        this_3->worst_0 = this_3->d_0[int(7)];
    }
    return;
}

inline __device__ void PcaKnn_insert_1(PcaKnn_1 * this_4, uint k_4, float dist_1, uint i_7)
{
    bool take_8 = dist_1 < this_4->d_0[int(0)];
    float td_8 = this_4->d_0[int(0)];
    uint ti_8 = this_4->id_1[int(0)];
    float _S43;
    if(take_8)
    {
        _S43 = dist_1;
    }
    else
    {
        _S43 = td_8;
    }
    this_4->d_0[int(0)] = _S43;
    uint _S44;
    if(take_8)
    {
        _S44 = i_7;
    }
    else
    {
        _S44 = ti_8;
    }
    this_4->id_1[int(0)] = _S44;
    if(take_8)
    {
        _S43 = td_8;
    }
    else
    {
        _S43 = dist_1;
    }
    if(take_8)
    {
        _S44 = ti_8;
    }
    else
    {
        _S44 = i_7;
    }
    bool take_9 = _S43 < this_4->d_0[int(1)];
    float td_9 = this_4->d_0[int(1)];
    uint ti_9 = this_4->id_1[int(1)];
    float _S45;
    if(take_9)
    {
        _S45 = _S43;
    }
    else
    {
        _S45 = td_9;
    }
    this_4->d_0[int(1)] = _S45;
    uint _S46;
    if(take_9)
    {
        _S46 = _S44;
    }
    else
    {
        _S46 = ti_9;
    }
    this_4->id_1[int(1)] = _S46;
    if(take_9)
    {
        _S43 = td_9;
    }
    if(take_9)
    {
        _S44 = ti_9;
    }
    bool take_10 = _S43 < this_4->d_0[int(2)];
    float td_10 = this_4->d_0[int(2)];
    uint ti_10 = this_4->id_1[int(2)];
    if(take_10)
    {
        _S45 = _S43;
    }
    else
    {
        _S45 = td_10;
    }
    this_4->d_0[int(2)] = _S45;
    if(take_10)
    {
        _S46 = _S44;
    }
    else
    {
        _S46 = ti_10;
    }
    this_4->id_1[int(2)] = _S46;
    if(take_10)
    {
        _S43 = td_10;
    }
    if(take_10)
    {
        _S44 = ti_10;
    }
    bool take_11 = _S43 < this_4->d_0[int(3)];
    float td_11 = this_4->d_0[int(3)];
    uint ti_11 = this_4->id_1[int(3)];
    if(take_11)
    {
        _S45 = _S43;
    }
    else
    {
        _S45 = td_11;
    }
    this_4->d_0[int(3)] = _S45;
    if(take_11)
    {
        _S46 = _S44;
    }
    else
    {
        _S46 = ti_11;
    }
    this_4->id_1[int(3)] = _S46;
    if(take_11)
    {
        _S43 = td_11;
    }
    if(take_11)
    {
        _S44 = ti_11;
    }
    bool take_12 = _S43 < this_4->d_0[int(4)];
    float td_12 = this_4->d_0[int(4)];
    uint ti_12 = this_4->id_1[int(4)];
    if(take_12)
    {
        _S45 = _S43;
    }
    else
    {
        _S45 = td_12;
    }
    this_4->d_0[int(4)] = _S45;
    if(take_12)
    {
        _S46 = _S44;
    }
    else
    {
        _S46 = ti_12;
    }
    this_4->id_1[int(4)] = _S46;
    if(take_12)
    {
        _S43 = td_12;
    }
    if(take_12)
    {
        _S44 = ti_12;
    }
    bool take_13 = _S43 < this_4->d_0[int(5)];
    float td_13 = this_4->d_0[int(5)];
    uint ti_13 = this_4->id_1[int(5)];
    if(take_13)
    {
        _S45 = _S43;
    }
    else
    {
        _S45 = td_13;
    }
    this_4->d_0[int(5)] = _S45;
    if(take_13)
    {
        _S46 = _S44;
    }
    else
    {
        _S46 = ti_13;
    }
    this_4->id_1[int(5)] = _S46;
    if(take_13)
    {
        _S43 = td_13;
    }
    if(take_13)
    {
        _S44 = ti_13;
    }
    bool take_14 = _S43 < this_4->d_0[int(6)];
    float td_14 = this_4->d_0[int(6)];
    uint ti_14 = this_4->id_1[int(6)];
    if(take_14)
    {
        _S45 = _S43;
    }
    else
    {
        _S45 = td_14;
    }
    this_4->d_0[int(6)] = _S45;
    if(take_14)
    {
        _S46 = _S44;
    }
    else
    {
        _S46 = ti_14;
    }
    this_4->id_1[int(6)] = _S46;
    if(take_14)
    {
        _S43 = td_14;
    }
    if(take_14)
    {
        _S44 = ti_14;
    }
    bool take_15 = _S43 < this_4->d_0[int(7)];
    float td_15 = this_4->d_0[int(7)];
    uint ti_15 = this_4->id_1[int(7)];
    if(take_15)
    {
        _S45 = _S43;
    }
    else
    {
        _S45 = td_15;
    }
    this_4->d_0[int(7)] = _S45;
    if(take_15)
    {
        _S46 = _S44;
    }
    else
    {
        _S46 = ti_15;
    }
    this_4->id_1[int(7)] = _S46;
    if(take_15)
    {
        _S43 = td_15;
    }
    if(take_15)
    {
        _S44 = ti_15;
    }
    bool take_16 = _S43 < this_4->d_0[int(8)];
    float td_16 = this_4->d_0[int(8)];
    uint ti_16 = this_4->id_1[int(8)];
    if(take_16)
    {
        _S45 = _S43;
    }
    else
    {
        _S45 = td_16;
    }
    this_4->d_0[int(8)] = _S45;
    if(take_16)
    {
        _S46 = _S44;
    }
    else
    {
        _S46 = ti_16;
    }
    this_4->id_1[int(8)] = _S46;
    if(take_16)
    {
        _S43 = td_16;
    }
    if(take_16)
    {
        _S44 = ti_16;
    }
    bool take_17 = _S43 < this_4->d_0[int(9)];
    float td_17 = this_4->d_0[int(9)];
    uint ti_17 = this_4->id_1[int(9)];
    if(take_17)
    {
        _S45 = _S43;
    }
    else
    {
        _S45 = td_17;
    }
    this_4->d_0[int(9)] = _S45;
    if(take_17)
    {
        _S46 = _S44;
    }
    else
    {
        _S46 = ti_17;
    }
    this_4->id_1[int(9)] = _S46;
    if(take_17)
    {
        _S43 = td_17;
    }
    if(take_17)
    {
        _S44 = ti_17;
    }
    bool take_18 = _S43 < this_4->d_0[int(10)];
    float td_18 = this_4->d_0[int(10)];
    uint ti_18 = this_4->id_1[int(10)];
    if(take_18)
    {
        _S45 = _S43;
    }
    else
    {
        _S45 = td_18;
    }
    this_4->d_0[int(10)] = _S45;
    if(take_18)
    {
        _S46 = _S44;
    }
    else
    {
        _S46 = ti_18;
    }
    this_4->id_1[int(10)] = _S46;
    if(take_18)
    {
        _S43 = td_18;
    }
    if(take_18)
    {
        _S44 = ti_18;
    }
    bool take_19 = _S43 < this_4->d_0[int(11)];
    float td_19 = this_4->d_0[int(11)];
    uint ti_19 = this_4->id_1[int(11)];
    if(take_19)
    {
        _S45 = _S43;
    }
    else
    {
        _S45 = td_19;
    }
    this_4->d_0[int(11)] = _S45;
    if(take_19)
    {
        _S46 = _S44;
    }
    else
    {
        _S46 = ti_19;
    }
    this_4->id_1[int(11)] = _S46;
    if(take_19)
    {
        _S43 = td_19;
    }
    if(take_19)
    {
        _S44 = ti_19;
    }
    bool take_20 = _S43 < this_4->d_0[int(12)];
    float td_20 = this_4->d_0[int(12)];
    uint ti_20 = this_4->id_1[int(12)];
    if(take_20)
    {
        _S45 = _S43;
    }
    else
    {
        _S45 = td_20;
    }
    this_4->d_0[int(12)] = _S45;
    if(take_20)
    {
        _S46 = _S44;
    }
    else
    {
        _S46 = ti_20;
    }
    this_4->id_1[int(12)] = _S46;
    if(take_20)
    {
        _S43 = td_20;
    }
    if(take_20)
    {
        _S44 = ti_20;
    }
    bool take_21 = _S43 < this_4->d_0[int(13)];
    float td_21 = this_4->d_0[int(13)];
    uint ti_21 = this_4->id_1[int(13)];
    if(take_21)
    {
        _S45 = _S43;
    }
    else
    {
        _S45 = td_21;
    }
    this_4->d_0[int(13)] = _S45;
    if(take_21)
    {
        _S46 = _S44;
    }
    else
    {
        _S46 = ti_21;
    }
    this_4->id_1[int(13)] = _S46;
    if(take_21)
    {
        _S43 = td_21;
    }
    if(take_21)
    {
        _S44 = ti_21;
    }
    bool take_22 = _S43 < this_4->d_0[int(14)];
    float td_22 = this_4->d_0[int(14)];
    uint ti_22 = this_4->id_1[int(14)];
    if(take_22)
    {
        _S45 = _S43;
    }
    else
    {
        _S45 = td_22;
    }
    this_4->d_0[int(14)] = _S45;
    if(take_22)
    {
        _S46 = _S44;
    }
    else
    {
        _S46 = ti_22;
    }
    this_4->id_1[int(14)] = _S46;
    if(take_22)
    {
        _S43 = td_22;
    }
    if(take_22)
    {
        _S44 = ti_22;
    }
    bool take_23 = _S43 < this_4->d_0[int(15)];
    float td_23 = this_4->d_0[int(15)];
    uint ti_23 = this_4->id_1[int(15)];
    if(take_23)
    {
    }
    else
    {
        _S43 = td_23;
    }
    this_4->d_0[int(15)] = _S43;
    if(take_23)
    {
    }
    else
    {
        _S44 = ti_23;
    }
    this_4->id_1[int(15)] = _S44;
    if(1U == k_4)
    {
        this_4->worst_0 = this_4->d_0[int(0)];
    }
    if(2U == k_4)
    {
        this_4->worst_0 = this_4->d_0[int(1)];
    }
    if(3U == k_4)
    {
        this_4->worst_0 = this_4->d_0[int(2)];
    }
    if(4U == k_4)
    {
        this_4->worst_0 = this_4->d_0[int(3)];
    }
    if(5U == k_4)
    {
        this_4->worst_0 = this_4->d_0[int(4)];
    }
    if(6U == k_4)
    {
        this_4->worst_0 = this_4->d_0[int(5)];
    }
    if(7U == k_4)
    {
        this_4->worst_0 = this_4->d_0[int(6)];
    }
    if(8U == k_4)
    {
        this_4->worst_0 = this_4->d_0[int(7)];
    }
    if(9U == k_4)
    {
        this_4->worst_0 = this_4->d_0[int(8)];
    }
    if(10U == k_4)
    {
        this_4->worst_0 = this_4->d_0[int(9)];
    }
    if(11U == k_4)
    {
        this_4->worst_0 = this_4->d_0[int(10)];
    }
    if(12U == k_4)
    {
        this_4->worst_0 = this_4->d_0[int(11)];
    }
    if(13U == k_4)
    {
        this_4->worst_0 = this_4->d_0[int(12)];
    }
    if(14U == k_4)
    {
        this_4->worst_0 = this_4->d_0[int(13)];
    }
    if(15U == k_4)
    {
        this_4->worst_0 = this_4->d_0[int(14)];
    }
    if(16U == k_4)
    {
        this_4->worst_0 = this_4->d_0[int(15)];
    }
    return;
}

inline __device__ void PcaKnn_insert_2(PcaKnn_2 * this_5, uint k_5, float dist_2, uint i_8)
{
    bool take_24 = dist_2 < this_5->d_0[int(0)];
    float td_24 = this_5->d_0[int(0)];
    uint ti_24 = this_5->id_1[int(0)];
    float _S47;
    if(take_24)
    {
        _S47 = dist_2;
    }
    else
    {
        _S47 = td_24;
    }
    this_5->d_0[int(0)] = _S47;
    uint _S48;
    if(take_24)
    {
        _S48 = i_8;
    }
    else
    {
        _S48 = ti_24;
    }
    this_5->id_1[int(0)] = _S48;
    if(take_24)
    {
        _S47 = td_24;
    }
    else
    {
        _S47 = dist_2;
    }
    if(take_24)
    {
        _S48 = ti_24;
    }
    else
    {
        _S48 = i_8;
    }
    bool take_25 = _S47 < this_5->d_0[int(1)];
    float td_25 = this_5->d_0[int(1)];
    uint ti_25 = this_5->id_1[int(1)];
    float _S49;
    if(take_25)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_25;
    }
    this_5->d_0[int(1)] = _S49;
    uint _S50;
    if(take_25)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_25;
    }
    this_5->id_1[int(1)] = _S50;
    if(take_25)
    {
        _S47 = td_25;
    }
    if(take_25)
    {
        _S48 = ti_25;
    }
    bool take_26 = _S47 < this_5->d_0[int(2)];
    float td_26 = this_5->d_0[int(2)];
    uint ti_26 = this_5->id_1[int(2)];
    if(take_26)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_26;
    }
    this_5->d_0[int(2)] = _S49;
    if(take_26)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_26;
    }
    this_5->id_1[int(2)] = _S50;
    if(take_26)
    {
        _S47 = td_26;
    }
    if(take_26)
    {
        _S48 = ti_26;
    }
    bool take_27 = _S47 < this_5->d_0[int(3)];
    float td_27 = this_5->d_0[int(3)];
    uint ti_27 = this_5->id_1[int(3)];
    if(take_27)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_27;
    }
    this_5->d_0[int(3)] = _S49;
    if(take_27)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_27;
    }
    this_5->id_1[int(3)] = _S50;
    if(take_27)
    {
        _S47 = td_27;
    }
    if(take_27)
    {
        _S48 = ti_27;
    }
    bool take_28 = _S47 < this_5->d_0[int(4)];
    float td_28 = this_5->d_0[int(4)];
    uint ti_28 = this_5->id_1[int(4)];
    if(take_28)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_28;
    }
    this_5->d_0[int(4)] = _S49;
    if(take_28)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_28;
    }
    this_5->id_1[int(4)] = _S50;
    if(take_28)
    {
        _S47 = td_28;
    }
    if(take_28)
    {
        _S48 = ti_28;
    }
    bool take_29 = _S47 < this_5->d_0[int(5)];
    float td_29 = this_5->d_0[int(5)];
    uint ti_29 = this_5->id_1[int(5)];
    if(take_29)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_29;
    }
    this_5->d_0[int(5)] = _S49;
    if(take_29)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_29;
    }
    this_5->id_1[int(5)] = _S50;
    if(take_29)
    {
        _S47 = td_29;
    }
    if(take_29)
    {
        _S48 = ti_29;
    }
    bool take_30 = _S47 < this_5->d_0[int(6)];
    float td_30 = this_5->d_0[int(6)];
    uint ti_30 = this_5->id_1[int(6)];
    if(take_30)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_30;
    }
    this_5->d_0[int(6)] = _S49;
    if(take_30)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_30;
    }
    this_5->id_1[int(6)] = _S50;
    if(take_30)
    {
        _S47 = td_30;
    }
    if(take_30)
    {
        _S48 = ti_30;
    }
    bool take_31 = _S47 < this_5->d_0[int(7)];
    float td_31 = this_5->d_0[int(7)];
    uint ti_31 = this_5->id_1[int(7)];
    if(take_31)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_31;
    }
    this_5->d_0[int(7)] = _S49;
    if(take_31)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_31;
    }
    this_5->id_1[int(7)] = _S50;
    if(take_31)
    {
        _S47 = td_31;
    }
    if(take_31)
    {
        _S48 = ti_31;
    }
    bool take_32 = _S47 < this_5->d_0[int(8)];
    float td_32 = this_5->d_0[int(8)];
    uint ti_32 = this_5->id_1[int(8)];
    if(take_32)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_32;
    }
    this_5->d_0[int(8)] = _S49;
    if(take_32)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_32;
    }
    this_5->id_1[int(8)] = _S50;
    if(take_32)
    {
        _S47 = td_32;
    }
    if(take_32)
    {
        _S48 = ti_32;
    }
    bool take_33 = _S47 < this_5->d_0[int(9)];
    float td_33 = this_5->d_0[int(9)];
    uint ti_33 = this_5->id_1[int(9)];
    if(take_33)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_33;
    }
    this_5->d_0[int(9)] = _S49;
    if(take_33)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_33;
    }
    this_5->id_1[int(9)] = _S50;
    if(take_33)
    {
        _S47 = td_33;
    }
    if(take_33)
    {
        _S48 = ti_33;
    }
    bool take_34 = _S47 < this_5->d_0[int(10)];
    float td_34 = this_5->d_0[int(10)];
    uint ti_34 = this_5->id_1[int(10)];
    if(take_34)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_34;
    }
    this_5->d_0[int(10)] = _S49;
    if(take_34)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_34;
    }
    this_5->id_1[int(10)] = _S50;
    if(take_34)
    {
        _S47 = td_34;
    }
    if(take_34)
    {
        _S48 = ti_34;
    }
    bool take_35 = _S47 < this_5->d_0[int(11)];
    float td_35 = this_5->d_0[int(11)];
    uint ti_35 = this_5->id_1[int(11)];
    if(take_35)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_35;
    }
    this_5->d_0[int(11)] = _S49;
    if(take_35)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_35;
    }
    this_5->id_1[int(11)] = _S50;
    if(take_35)
    {
        _S47 = td_35;
    }
    if(take_35)
    {
        _S48 = ti_35;
    }
    bool take_36 = _S47 < this_5->d_0[int(12)];
    float td_36 = this_5->d_0[int(12)];
    uint ti_36 = this_5->id_1[int(12)];
    if(take_36)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_36;
    }
    this_5->d_0[int(12)] = _S49;
    if(take_36)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_36;
    }
    this_5->id_1[int(12)] = _S50;
    if(take_36)
    {
        _S47 = td_36;
    }
    if(take_36)
    {
        _S48 = ti_36;
    }
    bool take_37 = _S47 < this_5->d_0[int(13)];
    float td_37 = this_5->d_0[int(13)];
    uint ti_37 = this_5->id_1[int(13)];
    if(take_37)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_37;
    }
    this_5->d_0[int(13)] = _S49;
    if(take_37)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_37;
    }
    this_5->id_1[int(13)] = _S50;
    if(take_37)
    {
        _S47 = td_37;
    }
    if(take_37)
    {
        _S48 = ti_37;
    }
    bool take_38 = _S47 < this_5->d_0[int(14)];
    float td_38 = this_5->d_0[int(14)];
    uint ti_38 = this_5->id_1[int(14)];
    if(take_38)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_38;
    }
    this_5->d_0[int(14)] = _S49;
    if(take_38)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_38;
    }
    this_5->id_1[int(14)] = _S50;
    if(take_38)
    {
        _S47 = td_38;
    }
    if(take_38)
    {
        _S48 = ti_38;
    }
    bool take_39 = _S47 < this_5->d_0[int(15)];
    float td_39 = this_5->d_0[int(15)];
    uint ti_39 = this_5->id_1[int(15)];
    if(take_39)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_39;
    }
    this_5->d_0[int(15)] = _S49;
    if(take_39)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_39;
    }
    this_5->id_1[int(15)] = _S50;
    if(take_39)
    {
        _S47 = td_39;
    }
    if(take_39)
    {
        _S48 = ti_39;
    }
    bool take_40 = _S47 < this_5->d_0[int(16)];
    float td_40 = this_5->d_0[int(16)];
    uint ti_40 = this_5->id_1[int(16)];
    if(take_40)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_40;
    }
    this_5->d_0[int(16)] = _S49;
    if(take_40)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_40;
    }
    this_5->id_1[int(16)] = _S50;
    if(take_40)
    {
        _S47 = td_40;
    }
    if(take_40)
    {
        _S48 = ti_40;
    }
    bool take_41 = _S47 < this_5->d_0[int(17)];
    float td_41 = this_5->d_0[int(17)];
    uint ti_41 = this_5->id_1[int(17)];
    if(take_41)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_41;
    }
    this_5->d_0[int(17)] = _S49;
    if(take_41)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_41;
    }
    this_5->id_1[int(17)] = _S50;
    if(take_41)
    {
        _S47 = td_41;
    }
    if(take_41)
    {
        _S48 = ti_41;
    }
    bool take_42 = _S47 < this_5->d_0[int(18)];
    float td_42 = this_5->d_0[int(18)];
    uint ti_42 = this_5->id_1[int(18)];
    if(take_42)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_42;
    }
    this_5->d_0[int(18)] = _S49;
    if(take_42)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_42;
    }
    this_5->id_1[int(18)] = _S50;
    if(take_42)
    {
        _S47 = td_42;
    }
    if(take_42)
    {
        _S48 = ti_42;
    }
    bool take_43 = _S47 < this_5->d_0[int(19)];
    float td_43 = this_5->d_0[int(19)];
    uint ti_43 = this_5->id_1[int(19)];
    if(take_43)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_43;
    }
    this_5->d_0[int(19)] = _S49;
    if(take_43)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_43;
    }
    this_5->id_1[int(19)] = _S50;
    if(take_43)
    {
        _S47 = td_43;
    }
    if(take_43)
    {
        _S48 = ti_43;
    }
    bool take_44 = _S47 < this_5->d_0[int(20)];
    float td_44 = this_5->d_0[int(20)];
    uint ti_44 = this_5->id_1[int(20)];
    if(take_44)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_44;
    }
    this_5->d_0[int(20)] = _S49;
    if(take_44)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_44;
    }
    this_5->id_1[int(20)] = _S50;
    if(take_44)
    {
        _S47 = td_44;
    }
    if(take_44)
    {
        _S48 = ti_44;
    }
    bool take_45 = _S47 < this_5->d_0[int(21)];
    float td_45 = this_5->d_0[int(21)];
    uint ti_45 = this_5->id_1[int(21)];
    if(take_45)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_45;
    }
    this_5->d_0[int(21)] = _S49;
    if(take_45)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_45;
    }
    this_5->id_1[int(21)] = _S50;
    if(take_45)
    {
        _S47 = td_45;
    }
    if(take_45)
    {
        _S48 = ti_45;
    }
    bool take_46 = _S47 < this_5->d_0[int(22)];
    float td_46 = this_5->d_0[int(22)];
    uint ti_46 = this_5->id_1[int(22)];
    if(take_46)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_46;
    }
    this_5->d_0[int(22)] = _S49;
    if(take_46)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_46;
    }
    this_5->id_1[int(22)] = _S50;
    if(take_46)
    {
        _S47 = td_46;
    }
    if(take_46)
    {
        _S48 = ti_46;
    }
    bool take_47 = _S47 < this_5->d_0[int(23)];
    float td_47 = this_5->d_0[int(23)];
    uint ti_47 = this_5->id_1[int(23)];
    if(take_47)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_47;
    }
    this_5->d_0[int(23)] = _S49;
    if(take_47)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_47;
    }
    this_5->id_1[int(23)] = _S50;
    if(take_47)
    {
        _S47 = td_47;
    }
    if(take_47)
    {
        _S48 = ti_47;
    }
    bool take_48 = _S47 < this_5->d_0[int(24)];
    float td_48 = this_5->d_0[int(24)];
    uint ti_48 = this_5->id_1[int(24)];
    if(take_48)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_48;
    }
    this_5->d_0[int(24)] = _S49;
    if(take_48)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_48;
    }
    this_5->id_1[int(24)] = _S50;
    if(take_48)
    {
        _S47 = td_48;
    }
    if(take_48)
    {
        _S48 = ti_48;
    }
    bool take_49 = _S47 < this_5->d_0[int(25)];
    float td_49 = this_5->d_0[int(25)];
    uint ti_49 = this_5->id_1[int(25)];
    if(take_49)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_49;
    }
    this_5->d_0[int(25)] = _S49;
    if(take_49)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_49;
    }
    this_5->id_1[int(25)] = _S50;
    if(take_49)
    {
        _S47 = td_49;
    }
    if(take_49)
    {
        _S48 = ti_49;
    }
    bool take_50 = _S47 < this_5->d_0[int(26)];
    float td_50 = this_5->d_0[int(26)];
    uint ti_50 = this_5->id_1[int(26)];
    if(take_50)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_50;
    }
    this_5->d_0[int(26)] = _S49;
    if(take_50)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_50;
    }
    this_5->id_1[int(26)] = _S50;
    if(take_50)
    {
        _S47 = td_50;
    }
    if(take_50)
    {
        _S48 = ti_50;
    }
    bool take_51 = _S47 < this_5->d_0[int(27)];
    float td_51 = this_5->d_0[int(27)];
    uint ti_51 = this_5->id_1[int(27)];
    if(take_51)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_51;
    }
    this_5->d_0[int(27)] = _S49;
    if(take_51)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_51;
    }
    this_5->id_1[int(27)] = _S50;
    if(take_51)
    {
        _S47 = td_51;
    }
    if(take_51)
    {
        _S48 = ti_51;
    }
    bool take_52 = _S47 < this_5->d_0[int(28)];
    float td_52 = this_5->d_0[int(28)];
    uint ti_52 = this_5->id_1[int(28)];
    if(take_52)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_52;
    }
    this_5->d_0[int(28)] = _S49;
    if(take_52)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_52;
    }
    this_5->id_1[int(28)] = _S50;
    if(take_52)
    {
        _S47 = td_52;
    }
    if(take_52)
    {
        _S48 = ti_52;
    }
    bool take_53 = _S47 < this_5->d_0[int(29)];
    float td_53 = this_5->d_0[int(29)];
    uint ti_53 = this_5->id_1[int(29)];
    if(take_53)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_53;
    }
    this_5->d_0[int(29)] = _S49;
    if(take_53)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_53;
    }
    this_5->id_1[int(29)] = _S50;
    if(take_53)
    {
        _S47 = td_53;
    }
    if(take_53)
    {
        _S48 = ti_53;
    }
    bool take_54 = _S47 < this_5->d_0[int(30)];
    float td_54 = this_5->d_0[int(30)];
    uint ti_54 = this_5->id_1[int(30)];
    if(take_54)
    {
        _S49 = _S47;
    }
    else
    {
        _S49 = td_54;
    }
    this_5->d_0[int(30)] = _S49;
    if(take_54)
    {
        _S50 = _S48;
    }
    else
    {
        _S50 = ti_54;
    }
    this_5->id_1[int(30)] = _S50;
    if(take_54)
    {
        _S47 = td_54;
    }
    if(take_54)
    {
        _S48 = ti_54;
    }
    bool take_55 = _S47 < this_5->d_0[int(31)];
    float td_55 = this_5->d_0[int(31)];
    uint ti_55 = this_5->id_1[int(31)];
    if(take_55)
    {
    }
    else
    {
        _S47 = td_55;
    }
    this_5->d_0[int(31)] = _S47;
    if(take_55)
    {
    }
    else
    {
        _S48 = ti_55;
    }
    this_5->id_1[int(31)] = _S48;
    if(1U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(0)];
    }
    if(2U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(1)];
    }
    if(3U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(2)];
    }
    if(4U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(3)];
    }
    if(5U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(4)];
    }
    if(6U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(5)];
    }
    if(7U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(6)];
    }
    if(8U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(7)];
    }
    if(9U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(8)];
    }
    if(10U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(9)];
    }
    if(11U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(10)];
    }
    if(12U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(11)];
    }
    if(13U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(12)];
    }
    if(14U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(13)];
    }
    if(15U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(14)];
    }
    if(16U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(15)];
    }
    if(17U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(16)];
    }
    if(18U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(17)];
    }
    if(19U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(18)];
    }
    if(20U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(19)];
    }
    if(21U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(20)];
    }
    if(22U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(21)];
    }
    if(23U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(22)];
    }
    if(24U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(23)];
    }
    if(25U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(24)];
    }
    if(26U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(25)];
    }
    if(27U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(26)];
    }
    if(28U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(27)];
    }
    if(29U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(28)];
    }
    if(30U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(29)];
    }
    if(31U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(30)];
    }
    if(32U == k_5)
    {
        this_5->worst_0 = this_5->d_0[int(31)];
    }
    return;
}

struct DiffPair_vectorx3Cfloatx2C3x3E_0
{
    float3  primal_0;
    float3  differential_0;
};

inline __device__ void _d_dot_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_1, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpy_1, float dOut_1)
{
    float3  x_d_result_0;
    *&((&x_d_result_0)->x) = (*dpy_1).primal_0.x * dOut_1;
    float3  y_d_result_0;
    *&((&y_d_result_0)->x) = (*dpx_1).primal_0.x * dOut_1;
    *&((&x_d_result_0)->y) = (*dpy_1).primal_0.y * dOut_1;
    *&((&y_d_result_0)->y) = (*dpx_1).primal_0.y * dOut_1;
    *&((&x_d_result_0)->z) = (*dpy_1).primal_0.z * dOut_1;
    *&((&y_d_result_0)->z) = (*dpx_1).primal_0.z * dOut_1;
    dpx_1->primal_0 = (*dpx_1).primal_0;
    dpx_1->differential_0 = x_d_result_0;
    dpy_1->primal_0 = (*dpy_1).primal_0;
    dpy_1->differential_0 = y_d_result_0;
    return;
}

struct DiffPair_vectorx3Cfloatx2C4x3E_0
{
    float4  primal_0;
    float4  differential_0;
};

inline __device__ void _d_dot_1(DiffPair_vectorx3Cfloatx2C4x3E_0 * dpx_2, DiffPair_vectorx3Cfloatx2C4x3E_0 * dpy_2, float dOut_2)
{
    float4  x_d_result_1;
    *&((&x_d_result_1)->x) = (*dpy_2).primal_0.x * dOut_2;
    float4  y_d_result_1;
    *&((&y_d_result_1)->x) = (*dpx_2).primal_0.x * dOut_2;
    *&((&x_d_result_1)->y) = (*dpy_2).primal_0.y * dOut_2;
    *&((&y_d_result_1)->y) = (*dpx_2).primal_0.y * dOut_2;
    *&((&x_d_result_1)->z) = (*dpy_2).primal_0.z * dOut_2;
    *&((&y_d_result_1)->z) = (*dpx_2).primal_0.z * dOut_2;
    *&((&x_d_result_1)->w) = (*dpy_2).primal_0.w * dOut_2;
    *&((&y_d_result_1)->w) = (*dpx_2).primal_0.w * dOut_2;
    dpx_2->primal_0 = (*dpx_2).primal_0;
    dpx_2->differential_0 = x_d_result_1;
    dpy_2->primal_0 = (*dpy_2).primal_0;
    dpy_2->differential_0 = y_d_result_1;
    return;
}

inline __device__ float dot_0(float3  x_5, float3  y_3)
{
    int i_9 = int(0);
    float result_3 = 0.0f;
    for(;;)
    {
        if(i_9 < int(3))
        {
        }
        else
        {
            break;
        }
        float result_4 = result_3 + _slang_vector_get_element(x_5, i_9) * _slang_vector_get_element(y_3, i_9);
        i_9 = i_9 + int(1);
        result_3 = result_4;
    }
    return result_3;
}

inline __device__ float dot_1(float4  x_6, float4  y_4)
{
    int i_10 = int(0);
    float result_5 = 0.0f;
    for(;;)
    {
        if(i_10 < int(4))
        {
        }
        else
        {
            break;
        }
        float result_6 = result_5 + _slang_vector_get_element(x_6, i_10) * _slang_vector_get_element(y_4, i_10);
        i_10 = i_10 + int(1);
        result_5 = result_6;
    }
    return result_5;
}

inline __device__ void PcaKnn_scan_0(PcaKnn_0 * this_6, float3  q_1, uint self_0, uint first_1, uint last_1, uint skip0_0, uint skip1_0, float4  * sorted_3, uint k_6)
{
    uint i_11 = first_1;
    for(;;)
    {
        if(i_11 < last_1)
        {
        }
        else
        {
            break;
        }
        float3  e_0 = float3 {(*(sorted_3 + i_11)).x, (*(sorted_3 + i_11)).y, (*(sorted_3 + i_11)).z} - q_1;
        bool skip_0;
        if(i_11 == self_0)
        {
            skip_0 = true;
        }
        else
        {
            if(i_11 >= skip0_0)
            {
                skip_0 = i_11 < skip1_0;
            }
            else
            {
                skip_0 = false;
            }
        }
        float dist_3;
        if(skip_0)
        {
            dist_3 = 3.00000000549775576e+38f;
        }
        else
        {
            dist_3 = dot_0(e_0, e_0);
        }
        if(dist_3 < (this_6->worst_0))
        {
            PcaKnn_insert_0(this_6, k_6, dist_3, i_11);
        }
        i_11 = i_11 + 1U;
    }
    return;
}

inline __device__ void PcaKnn_scan_1(PcaKnn_1 * this_7, float3  q_2, uint self_1, uint first_2, uint last_2, uint skip0_1, uint skip1_1, float4  * sorted_4, uint k_7)
{
    uint i_12 = first_2;
    for(;;)
    {
        if(i_12 < last_2)
        {
        }
        else
        {
            break;
        }
        float3  e_1 = float3 {(*(sorted_4 + i_12)).x, (*(sorted_4 + i_12)).y, (*(sorted_4 + i_12)).z} - q_2;
        bool skip_1;
        if(i_12 == self_1)
        {
            skip_1 = true;
        }
        else
        {
            if(i_12 >= skip0_1)
            {
                skip_1 = i_12 < skip1_1;
            }
            else
            {
                skip_1 = false;
            }
        }
        float dist_4;
        if(skip_1)
        {
            dist_4 = 3.00000000549775576e+38f;
        }
        else
        {
            dist_4 = dot_0(e_1, e_1);
        }
        if(dist_4 < (this_7->worst_0))
        {
            PcaKnn_insert_1(this_7, k_7, dist_4, i_12);
        }
        i_12 = i_12 + 1U;
    }
    return;
}

inline __device__ void PcaKnn_scan_2(PcaKnn_2 * this_8, float3  q_3, uint self_2, uint first_3, uint last_3, uint skip0_2, uint skip1_2, float4  * sorted_5, uint k_8)
{
    uint i_13 = first_3;
    for(;;)
    {
        if(i_13 < last_3)
        {
        }
        else
        {
            break;
        }
        float3  e_2 = float3 {(*(sorted_5 + i_13)).x, (*(sorted_5 + i_13)).y, (*(sorted_5 + i_13)).z} - q_3;
        bool skip_2;
        if(i_13 == self_2)
        {
            skip_2 = true;
        }
        else
        {
            if(i_13 >= skip0_2)
            {
                skip_2 = i_13 < skip1_2;
            }
            else
            {
                skip_2 = false;
            }
        }
        float dist_5;
        if(skip_2)
        {
            dist_5 = 3.00000000549775576e+38f;
        }
        else
        {
            dist_5 = dot_0(e_2, e_2);
        }
        if(dist_5 < (this_8->worst_0))
        {
            PcaKnn_insert_2(this_8, k_8, dist_5, i_13);
        }
        i_13 = i_13 + 1U;
    }
    return;
}

inline __device__ float pca_box_d2_0(float3  p_4, float4  lo_3, float4  hi_1)
{
    float3  d_1 = max_0(max_0(float3 {lo_3.x, lo_3.y, lo_3.z} - p_4, p_4 - float3 {hi_1.x, hi_1.y, hi_1.z}), make_float3 (0.0f));
    return dot_0(d_1, d_1);
}

inline __device__ void pca_knn_tree_0(uint r_1, uint n_2, uint k_9, float4  * sorted_6, float4  * nodes_1, int4  * meta_1, int * ranges_1, PcaKnn_0 * h_0)
{
    float3  q_4 = float3 {(*(sorted_6 + r_1)).x, (*(sorted_6 + r_1)).y, (*(sorted_6 + r_1)).z};
    uint _S51 = 2U * (k_9 + 1U);
    uint w0_0;
    if(r_1 > _S51)
    {
        w0_0 = r_1 - _S51;
    }
    else
    {
        w0_0 = 0U;
    }
    uint _S52 = (U32_min((r_1 + _S51 + 1U), (n_2)));
    PcaKnn_scan_0(h_0, q_4, r_1, w0_0, _S52, 0U, 0U, sorted_6, k_9);
    FixedArray<uint, 96>  stack_0;
    stack_0[int(0)] = uint(*(ranges_1 + (n_2 - 1U)));
    FixedArray<float, 96>  stack_d2_0;
    stack_d2_0[int(0)] = 0.0f;
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
        uint node_0 = stack_0[sp_1];
        bool _S53 = ((__any_sync(0xffffffffu, ((stack_d2_0[sp_1]) < (h_0->worst_0))) != 0));
        if(!_S53)
        {
            sp_0 = sp_1;
            continue;
        }
        int4  * _S54 = meta_1 + node_0;
        uint a_2 = uint((*_S54).x);
        uint b_2 = uint((*_S54).y);
        int _S55 = (*_S54).w;
        int _S56 = (*_S54).z;
        uint first_4;
        uint last_4;
        uint a_3;
        uint b_3;
        bool _S57;
        float da_0;
        float db_0;
        if(uint(_S55 - _S56) < 16U)
        {
            uint _S58 = uint(_S55) + 1U;
            first_4 = uint(_S56);
            last_4 = _S58;
            da_0 = 3.00000000549775576e+38f;
            db_0 = 3.00000000549775576e+38f;
            a_3 = 0U;
            b_3 = 0U;
        }
        else
        {
            bool la_0 = (a_2 & 2147483648U) != 0U;
            bool lb_0 = (b_2 & 2147483648U) != 0U;
            if(la_0)
            {
                first_4 = a_2 & 2147483647U;
            }
            else
            {
                first_4 = b_2 & 2147483647U;
            }
            if(la_0)
            {
                _S57 = true;
            }
            else
            {
                _S57 = lb_0;
            }
            if(_S57)
            {
                if(lb_0)
                {
                    last_4 = b_2 & 2147483647U;
                }
                else
                {
                    last_4 = first_4;
                }
                last_4 = last_4 + 1U;
            }
            else
            {
                last_4 = first_4;
            }
            if(!la_0)
            {
                uint _S59 = 2U * a_2;
                da_0 = pca_box_d2_0(q_4, *(nodes_1 + _S59), *(nodes_1 + (_S59 + 1U)));
            }
            else
            {
                da_0 = 3.00000000549775576e+38f;
            }
            if(!lb_0)
            {
                uint _S60 = 2U * b_2;
                db_0 = pca_box_d2_0(q_4, *(nodes_1 + _S60), *(nodes_1 + (_S60 + 1U)));
            }
            else
            {
                db_0 = 3.00000000549775576e+38f;
            }
            a_3 = a_2;
            b_3 = b_2;
        }
        PcaKnn_scan_0(h_0, q_4, r_1, first_4, last_4, w0_0, _S52, sorted_6, k_9);
        bool need_a_0 = ((__any_sync(0xffffffffu, (da_0 < (h_0->worst_0))) != 0));
        bool need_b_0 = ((__any_sync(0xffffffffu, (db_0 < (h_0->worst_0))) != 0));
        uint _S61 = (__popc(__ballot_sync(0xffffffffu, (da_0 <= db_0))));
        uint _S62 = 2U * _S61;
        uint _S63 = (__popc(__ballot_sync(0xffffffffu, (true))));
        bool a_near_0 = _S62 >= _S63;
        uint near_0;
        if(a_near_0)
        {
            near_0 = a_3;
        }
        else
        {
            near_0 = b_3;
        }
        uint far_0;
        if(a_near_0)
        {
            far_0 = b_3;
        }
        else
        {
            far_0 = a_3;
        }
        float dn_0;
        if(a_near_0)
        {
            dn_0 = da_0;
        }
        else
        {
            dn_0 = db_0;
        }
        float df_0;
        if(a_near_0)
        {
            df_0 = db_0;
        }
        else
        {
            df_0 = da_0;
        }
        if(a_near_0)
        {
            _S57 = need_b_0;
        }
        else
        {
            _S57 = need_a_0;
        }
        uint sp_2;
        if(_S57)
        {
            stack_0[sp_1] = far_0;
            stack_d2_0[sp_1] = df_0;
            sp_2 = sp_1 + 1U;
        }
        else
        {
            sp_2 = sp_1;
        }
        bool _S64;
        if(a_near_0)
        {
            _S64 = need_a_0;
        }
        else
        {
            _S64 = need_b_0;
        }
        if(_S64)
        {
            stack_0[sp_2] = near_0;
            stack_d2_0[sp_2] = dn_0;
            sp_0 = sp_2 + 1U;
        }
        else
        {
            sp_0 = sp_2;
        }
    }
    return;
}

inline __device__ void pca_knn_tree_1(uint r_2, uint n_3, uint k_10, float4  * sorted_7, float4  * nodes_2, int4  * meta_2, int * ranges_2, PcaKnn_1 * h_1)
{
    float3  q_5 = float3 {(*(sorted_7 + r_2)).x, (*(sorted_7 + r_2)).y, (*(sorted_7 + r_2)).z};
    uint _S65 = 2U * (k_10 + 1U);
    uint w0_1;
    if(r_2 > _S65)
    {
        w0_1 = r_2 - _S65;
    }
    else
    {
        w0_1 = 0U;
    }
    uint _S66 = (U32_min((r_2 + _S65 + 1U), (n_3)));
    PcaKnn_scan_1(h_1, q_5, r_2, w0_1, _S66, 0U, 0U, sorted_7, k_10);
    FixedArray<uint, 96>  stack_1;
    stack_1[int(0)] = uint(*(ranges_2 + (n_3 - 1U)));
    FixedArray<float, 96>  stack_d2_1;
    stack_d2_1[int(0)] = 0.0f;
    uint sp_3 = 1U;
    for(;;)
    {
        if(sp_3 > 0U)
        {
        }
        else
        {
            break;
        }
        uint sp_4 = sp_3 - 1U;
        uint node_1 = stack_1[sp_4];
        bool _S67 = ((__any_sync(0xffffffffu, ((stack_d2_1[sp_4]) < (h_1->worst_0))) != 0));
        if(!_S67)
        {
            sp_3 = sp_4;
            continue;
        }
        int4  * _S68 = meta_2 + node_1;
        uint a_4 = uint((*_S68).x);
        uint b_4 = uint((*_S68).y);
        int _S69 = (*_S68).w;
        int _S70 = (*_S68).z;
        uint first_5;
        uint last_5;
        uint a_5;
        uint b_5;
        bool _S71;
        float da_1;
        float db_1;
        if(uint(_S69 - _S70) < 16U)
        {
            uint _S72 = uint(_S69) + 1U;
            first_5 = uint(_S70);
            last_5 = _S72;
            da_1 = 3.00000000549775576e+38f;
            db_1 = 3.00000000549775576e+38f;
            a_5 = 0U;
            b_5 = 0U;
        }
        else
        {
            bool la_1 = (a_4 & 2147483648U) != 0U;
            bool lb_1 = (b_4 & 2147483648U) != 0U;
            if(la_1)
            {
                first_5 = a_4 & 2147483647U;
            }
            else
            {
                first_5 = b_4 & 2147483647U;
            }
            if(la_1)
            {
                _S71 = true;
            }
            else
            {
                _S71 = lb_1;
            }
            if(_S71)
            {
                if(lb_1)
                {
                    last_5 = b_4 & 2147483647U;
                }
                else
                {
                    last_5 = first_5;
                }
                last_5 = last_5 + 1U;
            }
            else
            {
                last_5 = first_5;
            }
            if(!la_1)
            {
                uint _S73 = 2U * a_4;
                da_1 = pca_box_d2_0(q_5, *(nodes_2 + _S73), *(nodes_2 + (_S73 + 1U)));
            }
            else
            {
                da_1 = 3.00000000549775576e+38f;
            }
            if(!lb_1)
            {
                uint _S74 = 2U * b_4;
                db_1 = pca_box_d2_0(q_5, *(nodes_2 + _S74), *(nodes_2 + (_S74 + 1U)));
            }
            else
            {
                db_1 = 3.00000000549775576e+38f;
            }
            a_5 = a_4;
            b_5 = b_4;
        }
        PcaKnn_scan_1(h_1, q_5, r_2, first_5, last_5, w0_1, _S66, sorted_7, k_10);
        bool need_a_1 = ((__any_sync(0xffffffffu, (da_1 < (h_1->worst_0))) != 0));
        bool need_b_1 = ((__any_sync(0xffffffffu, (db_1 < (h_1->worst_0))) != 0));
        uint _S75 = (__popc(__ballot_sync(0xffffffffu, (da_1 <= db_1))));
        uint _S76 = 2U * _S75;
        uint _S77 = (__popc(__ballot_sync(0xffffffffu, (true))));
        bool a_near_1 = _S76 >= _S77;
        uint near_1;
        if(a_near_1)
        {
            near_1 = a_5;
        }
        else
        {
            near_1 = b_5;
        }
        uint far_1;
        if(a_near_1)
        {
            far_1 = b_5;
        }
        else
        {
            far_1 = a_5;
        }
        float dn_1;
        if(a_near_1)
        {
            dn_1 = da_1;
        }
        else
        {
            dn_1 = db_1;
        }
        float df_1;
        if(a_near_1)
        {
            df_1 = db_1;
        }
        else
        {
            df_1 = da_1;
        }
        if(a_near_1)
        {
            _S71 = need_b_1;
        }
        else
        {
            _S71 = need_a_1;
        }
        uint sp_5;
        if(_S71)
        {
            stack_1[sp_4] = far_1;
            stack_d2_1[sp_4] = df_1;
            sp_5 = sp_4 + 1U;
        }
        else
        {
            sp_5 = sp_4;
        }
        bool _S78;
        if(a_near_1)
        {
            _S78 = need_a_1;
        }
        else
        {
            _S78 = need_b_1;
        }
        if(_S78)
        {
            stack_1[sp_5] = near_1;
            stack_d2_1[sp_5] = dn_1;
            sp_3 = sp_5 + 1U;
        }
        else
        {
            sp_3 = sp_5;
        }
    }
    return;
}

inline __device__ void pca_knn_tree_2(uint r_3, uint n_4, uint k_11, float4  * sorted_8, float4  * nodes_3, int4  * meta_3, int * ranges_3, PcaKnn_2 * h_2)
{
    float3  q_6 = float3 {(*(sorted_8 + r_3)).x, (*(sorted_8 + r_3)).y, (*(sorted_8 + r_3)).z};
    uint _S79 = 2U * (k_11 + 1U);
    uint w0_2;
    if(r_3 > _S79)
    {
        w0_2 = r_3 - _S79;
    }
    else
    {
        w0_2 = 0U;
    }
    uint _S80 = (U32_min((r_3 + _S79 + 1U), (n_4)));
    PcaKnn_scan_2(h_2, q_6, r_3, w0_2, _S80, 0U, 0U, sorted_8, k_11);
    FixedArray<uint, 96>  stack_2;
    stack_2[int(0)] = uint(*(ranges_3 + (n_4 - 1U)));
    FixedArray<float, 96>  stack_d2_2;
    stack_d2_2[int(0)] = 0.0f;
    uint sp_6 = 1U;
    for(;;)
    {
        if(sp_6 > 0U)
        {
        }
        else
        {
            break;
        }
        uint sp_7 = sp_6 - 1U;
        uint node_2 = stack_2[sp_7];
        bool _S81 = ((__any_sync(0xffffffffu, ((stack_d2_2[sp_7]) < (h_2->worst_0))) != 0));
        if(!_S81)
        {
            sp_6 = sp_7;
            continue;
        }
        int4  * _S82 = meta_3 + node_2;
        uint a_6 = uint((*_S82).x);
        uint b_6 = uint((*_S82).y);
        int _S83 = (*_S82).w;
        int _S84 = (*_S82).z;
        uint first_6;
        uint last_6;
        uint a_7;
        uint b_7;
        bool _S85;
        float da_2;
        float db_2;
        if(uint(_S83 - _S84) < 16U)
        {
            uint _S86 = uint(_S83) + 1U;
            first_6 = uint(_S84);
            last_6 = _S86;
            da_2 = 3.00000000549775576e+38f;
            db_2 = 3.00000000549775576e+38f;
            a_7 = 0U;
            b_7 = 0U;
        }
        else
        {
            bool la_2 = (a_6 & 2147483648U) != 0U;
            bool lb_2 = (b_6 & 2147483648U) != 0U;
            if(la_2)
            {
                first_6 = a_6 & 2147483647U;
            }
            else
            {
                first_6 = b_6 & 2147483647U;
            }
            if(la_2)
            {
                _S85 = true;
            }
            else
            {
                _S85 = lb_2;
            }
            if(_S85)
            {
                if(lb_2)
                {
                    last_6 = b_6 & 2147483647U;
                }
                else
                {
                    last_6 = first_6;
                }
                last_6 = last_6 + 1U;
            }
            else
            {
                last_6 = first_6;
            }
            if(!la_2)
            {
                uint _S87 = 2U * a_6;
                da_2 = pca_box_d2_0(q_6, *(nodes_3 + _S87), *(nodes_3 + (_S87 + 1U)));
            }
            else
            {
                da_2 = 3.00000000549775576e+38f;
            }
            if(!lb_2)
            {
                uint _S88 = 2U * b_6;
                db_2 = pca_box_d2_0(q_6, *(nodes_3 + _S88), *(nodes_3 + (_S88 + 1U)));
            }
            else
            {
                db_2 = 3.00000000549775576e+38f;
            }
            a_7 = a_6;
            b_7 = b_6;
        }
        PcaKnn_scan_2(h_2, q_6, r_3, first_6, last_6, w0_2, _S80, sorted_8, k_11);
        bool need_a_2 = ((__any_sync(0xffffffffu, (da_2 < (h_2->worst_0))) != 0));
        bool need_b_2 = ((__any_sync(0xffffffffu, (db_2 < (h_2->worst_0))) != 0));
        uint _S89 = (__popc(__ballot_sync(0xffffffffu, (da_2 <= db_2))));
        uint _S90 = 2U * _S89;
        uint _S91 = (__popc(__ballot_sync(0xffffffffu, (true))));
        bool a_near_2 = _S90 >= _S91;
        uint near_2;
        if(a_near_2)
        {
            near_2 = a_7;
        }
        else
        {
            near_2 = b_7;
        }
        uint far_2;
        if(a_near_2)
        {
            far_2 = b_7;
        }
        else
        {
            far_2 = a_7;
        }
        float dn_2;
        if(a_near_2)
        {
            dn_2 = da_2;
        }
        else
        {
            dn_2 = db_2;
        }
        float df_2;
        if(a_near_2)
        {
            df_2 = db_2;
        }
        else
        {
            df_2 = da_2;
        }
        if(a_near_2)
        {
            _S85 = need_b_2;
        }
        else
        {
            _S85 = need_a_2;
        }
        uint sp_8;
        if(_S85)
        {
            stack_2[sp_7] = far_2;
            stack_d2_2[sp_7] = df_2;
            sp_8 = sp_7 + 1U;
        }
        else
        {
            sp_8 = sp_7;
        }
        bool _S92;
        if(a_near_2)
        {
            _S92 = need_a_2;
        }
        else
        {
            _S92 = need_b_2;
        }
        if(_S92)
        {
            stack_2[sp_8] = near_2;
            stack_d2_2[sp_8] = dn_2;
            sp_6 = sp_8 + 1U;
        }
        else
        {
            sp_6 = sp_8;
        }
    }
    return;
}

inline __device__ void PcaKnn_reset_0(PcaKnn_0 * this_9)
{
    this_9->d_0[int(0)] = 3.00000000549775576e+38f;
    this_9->id_1[int(0)] = 0U;
    this_9->d_0[int(1)] = 3.00000000549775576e+38f;
    this_9->id_1[int(1)] = 0U;
    this_9->d_0[int(2)] = 3.00000000549775576e+38f;
    this_9->id_1[int(2)] = 0U;
    this_9->d_0[int(3)] = 3.00000000549775576e+38f;
    this_9->id_1[int(3)] = 0U;
    this_9->d_0[int(4)] = 3.00000000549775576e+38f;
    this_9->id_1[int(4)] = 0U;
    this_9->d_0[int(5)] = 3.00000000549775576e+38f;
    this_9->id_1[int(5)] = 0U;
    this_9->d_0[int(6)] = 3.00000000549775576e+38f;
    this_9->id_1[int(6)] = 0U;
    this_9->d_0[int(7)] = 3.00000000549775576e+38f;
    this_9->id_1[int(7)] = 0U;
    this_9->worst_0 = 3.00000000549775576e+38f;
    return;
}

inline __device__ void PcaKnn_reset_1(PcaKnn_1 * this_10)
{
    this_10->d_0[int(0)] = 3.00000000549775576e+38f;
    this_10->id_1[int(0)] = 0U;
    this_10->d_0[int(1)] = 3.00000000549775576e+38f;
    this_10->id_1[int(1)] = 0U;
    this_10->d_0[int(2)] = 3.00000000549775576e+38f;
    this_10->id_1[int(2)] = 0U;
    this_10->d_0[int(3)] = 3.00000000549775576e+38f;
    this_10->id_1[int(3)] = 0U;
    this_10->d_0[int(4)] = 3.00000000549775576e+38f;
    this_10->id_1[int(4)] = 0U;
    this_10->d_0[int(5)] = 3.00000000549775576e+38f;
    this_10->id_1[int(5)] = 0U;
    this_10->d_0[int(6)] = 3.00000000549775576e+38f;
    this_10->id_1[int(6)] = 0U;
    this_10->d_0[int(7)] = 3.00000000549775576e+38f;
    this_10->id_1[int(7)] = 0U;
    this_10->d_0[int(8)] = 3.00000000549775576e+38f;
    this_10->id_1[int(8)] = 0U;
    this_10->d_0[int(9)] = 3.00000000549775576e+38f;
    this_10->id_1[int(9)] = 0U;
    this_10->d_0[int(10)] = 3.00000000549775576e+38f;
    this_10->id_1[int(10)] = 0U;
    this_10->d_0[int(11)] = 3.00000000549775576e+38f;
    this_10->id_1[int(11)] = 0U;
    this_10->d_0[int(12)] = 3.00000000549775576e+38f;
    this_10->id_1[int(12)] = 0U;
    this_10->d_0[int(13)] = 3.00000000549775576e+38f;
    this_10->id_1[int(13)] = 0U;
    this_10->d_0[int(14)] = 3.00000000549775576e+38f;
    this_10->id_1[int(14)] = 0U;
    this_10->d_0[int(15)] = 3.00000000549775576e+38f;
    this_10->id_1[int(15)] = 0U;
    this_10->worst_0 = 3.00000000549775576e+38f;
    return;
}

inline __device__ void PcaKnn_reset_2(PcaKnn_2 * this_11)
{
    this_11->d_0[int(0)] = 3.00000000549775576e+38f;
    this_11->id_1[int(0)] = 0U;
    this_11->d_0[int(1)] = 3.00000000549775576e+38f;
    this_11->id_1[int(1)] = 0U;
    this_11->d_0[int(2)] = 3.00000000549775576e+38f;
    this_11->id_1[int(2)] = 0U;
    this_11->d_0[int(3)] = 3.00000000549775576e+38f;
    this_11->id_1[int(3)] = 0U;
    this_11->d_0[int(4)] = 3.00000000549775576e+38f;
    this_11->id_1[int(4)] = 0U;
    this_11->d_0[int(5)] = 3.00000000549775576e+38f;
    this_11->id_1[int(5)] = 0U;
    this_11->d_0[int(6)] = 3.00000000549775576e+38f;
    this_11->id_1[int(6)] = 0U;
    this_11->d_0[int(7)] = 3.00000000549775576e+38f;
    this_11->id_1[int(7)] = 0U;
    this_11->d_0[int(8)] = 3.00000000549775576e+38f;
    this_11->id_1[int(8)] = 0U;
    this_11->d_0[int(9)] = 3.00000000549775576e+38f;
    this_11->id_1[int(9)] = 0U;
    this_11->d_0[int(10)] = 3.00000000549775576e+38f;
    this_11->id_1[int(10)] = 0U;
    this_11->d_0[int(11)] = 3.00000000549775576e+38f;
    this_11->id_1[int(11)] = 0U;
    this_11->d_0[int(12)] = 3.00000000549775576e+38f;
    this_11->id_1[int(12)] = 0U;
    this_11->d_0[int(13)] = 3.00000000549775576e+38f;
    this_11->id_1[int(13)] = 0U;
    this_11->d_0[int(14)] = 3.00000000549775576e+38f;
    this_11->id_1[int(14)] = 0U;
    this_11->d_0[int(15)] = 3.00000000549775576e+38f;
    this_11->id_1[int(15)] = 0U;
    this_11->d_0[int(16)] = 3.00000000549775576e+38f;
    this_11->id_1[int(16)] = 0U;
    this_11->d_0[int(17)] = 3.00000000549775576e+38f;
    this_11->id_1[int(17)] = 0U;
    this_11->d_0[int(18)] = 3.00000000549775576e+38f;
    this_11->id_1[int(18)] = 0U;
    this_11->d_0[int(19)] = 3.00000000549775576e+38f;
    this_11->id_1[int(19)] = 0U;
    this_11->d_0[int(20)] = 3.00000000549775576e+38f;
    this_11->id_1[int(20)] = 0U;
    this_11->d_0[int(21)] = 3.00000000549775576e+38f;
    this_11->id_1[int(21)] = 0U;
    this_11->d_0[int(22)] = 3.00000000549775576e+38f;
    this_11->id_1[int(22)] = 0U;
    this_11->d_0[int(23)] = 3.00000000549775576e+38f;
    this_11->id_1[int(23)] = 0U;
    this_11->d_0[int(24)] = 3.00000000549775576e+38f;
    this_11->id_1[int(24)] = 0U;
    this_11->d_0[int(25)] = 3.00000000549775576e+38f;
    this_11->id_1[int(25)] = 0U;
    this_11->d_0[int(26)] = 3.00000000549775576e+38f;
    this_11->id_1[int(26)] = 0U;
    this_11->d_0[int(27)] = 3.00000000549775576e+38f;
    this_11->id_1[int(27)] = 0U;
    this_11->d_0[int(28)] = 3.00000000549775576e+38f;
    this_11->id_1[int(28)] = 0U;
    this_11->d_0[int(29)] = 3.00000000549775576e+38f;
    this_11->id_1[int(29)] = 0U;
    this_11->d_0[int(30)] = 3.00000000549775576e+38f;
    this_11->id_1[int(30)] = 0U;
    this_11->d_0[int(31)] = 3.00000000549775576e+38f;
    this_11->id_1[int(31)] = 0U;
    this_11->worst_0 = 3.00000000549775576e+38f;
    return;
}

inline __device__ void _d_abs_0(DiffPair_float_0 * dpx_3, float dOut_3)
{
    float _S93 = _slang_select(((*dpx_3).primal_0) > 0.0f, 1.0f,_slang_select(((*dpx_3).primal_0) == 0.0f, 0.0f,-1.0f)) * dOut_3;
    dpx_3->primal_0 = (*dpx_3).primal_0;
    dpx_3->differential_0 = _S93;
    return;
}

inline __device__ void _d_rsqrt_0(DiffPair_float_0 * dpx_4, float dOut_4)
{
    float _S94 = -0.5f / ((*dpx_4).primal_0 * (F32_sqrt(((*dpx_4).primal_0)))) * dOut_4;
    dpx_4->primal_0 = (*dpx_4).primal_0;
    dpx_4->differential_0 = _S94;
    return;
}

inline __device__ void pca_jacobi_rot_0(float * app_0, float * aqq_0, float * apq_0, float * arp_0, float * arq_0, float3  * vp_0, float3  * vq_0)
{
    bool _S95;
    if(!((F32_abs((*apq_0))) > 1.00000000317107685e-30f))
    {
        _S95 = true;
    }
    else
    {
        _S95 = (F32_abs((*apq_0))) <= (9.999999960041972e-13f * ((F32_abs((*app_0))) + (F32_abs((*aqq_0)))));
    }
    if(_S95)
    {
        return;
    }
    float theta_0 = (*aqq_0 - *app_0) / (2.0f * *apq_0);
    float _S96;
    if(theta_0 >= 0.0f)
    {
        _S96 = 1.0f;
    }
    else
    {
        _S96 = -1.0f;
    }
    float t_0 = _S96 / ((F32_abs((theta_0))) + (F32_sqrt((theta_0 * theta_0 + 1.0f))));
    float c_3 = (F32_rsqrt((t_0 * t_0 + 1.0f)));
    float s_0 = t_0 * c_3;
    *app_0 = *app_0 - t_0 * *apq_0;
    *aqq_0 = *aqq_0 + t_0 * *apq_0;
    *apq_0 = 0.0f;
    float rp_0 = *arp_0;
    float rq_0 = *arq_0;
    *arp_0 = c_3 * *arp_0 - s_0 * *arq_0;
    *arq_0 = s_0 * rp_0 + c_3 * rq_0;
    float3  p0_0 = *vp_0;
    float3  q0_0 = *vq_0;
    *vp_0 = make_float3 (c_3) * *vp_0 - make_float3 (s_0) * *vq_0;
    *vq_0 = make_float3 (s_0) * p0_0 + make_float3 (c_3) * q0_0;
    return;
}

inline __device__ void pca_sym3_eigen_0(float3  d_2, float3  o_0, float3  * ev_0, float3  * e0_0, float3  * e1_0, float3  * e2_0)
{
    float a00_0 = d_2.x;
    float a11_0 = d_2.y;
    float a22_0 = d_2.z;
    float a01_0 = o_0.x;
    float a02_0 = o_0.y;
    float a12_0 = o_0.z;
    float3  v0_0 = make_float3 (1.0f, 0.0f, 0.0f);
    float3  v1_0 = make_float3 (0.0f, 1.0f, 0.0f);
    float3  v2_0 = make_float3 (0.0f, 0.0f, 1.0f);
    int sweep_0 = int(0);
    for(;;)
    {
        if(sweep_0 < int(5))
        {
        }
        else
        {
            break;
        }
        pca_jacobi_rot_0(&a00_0, &a11_0, &a01_0, &a02_0, &a12_0, &v0_0, &v1_0);
        pca_jacobi_rot_0(&a00_0, &a22_0, &a02_0, &a01_0, &a12_0, &v0_0, &v2_0);
        pca_jacobi_rot_0(&a11_0, &a22_0, &a12_0, &a01_0, &a02_0, &v1_0, &v2_0);
        sweep_0 = sweep_0 + int(1);
    }
    float3  l_0 = make_float3 (a00_0, a11_0, a22_0);
    if(a00_0 > a11_0)
    {
        float2  _S97 = float2 {l_0.y, l_0.x};
        l_0.x = _S97.x;
        l_0.y = _S97.y;
        float3  t_1 = v0_0;
        v0_0 = v1_0;
        v1_0 = t_1;
    }
    if((l_0.y) > (l_0.z))
    {
        float2  _S98 = float2 {l_0.z, l_0.y};
        l_0.y = _S98.x;
        l_0.z = _S98.y;
        float3  t_2 = v1_0;
        v1_0 = v2_0;
        v2_0 = t_2;
    }
    if((l_0.x) > (l_0.y))
    {
        float2  _S99 = float2 {l_0.y, l_0.x};
        l_0.x = _S99.x;
        l_0.y = _S99.y;
        float3  t_3 = v0_0;
        v0_0 = v1_0;
        v1_0 = t_3;
    }
    *ev_0 = l_0;
    *e0_0 = v0_0;
    *e1_0 = v1_0;
    *e2_0 = v2_0;
    return;
}

inline __device__ bool3  isfinite_0(float3  x_7)
{
    bool3  result_7;
    int i_14 = int(0);
    for(;;)
    {
        if(i_14 < int(3))
        {
        }
        else
        {
            break;
        }
        *_slang_vector_get_element_ptr(&result_7, i_14) = (F32_isfinite((_slang_vector_get_element(x_7, i_14))));
        i_14 = i_14 + int(1);
    }
    return result_7;
}

inline __device__ float3  mul_0(Matrix<float, 3, 3>  left_1, float3  right_0)
{
    float3  result_8;
    int i_15 = int(0);
    for(;;)
    {
        if(i_15 < int(3))
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
            float sum_1 = sum_0 + _slang_vector_get_element(left_1.rows[i_15], j_0) * _slang_vector_get_element(right_0, j_0);
            j_0 = j_0 + int(1);
            sum_0 = sum_1;
        }
        *_slang_vector_get_element_ptr(&result_8, i_15) = sum_0;
        i_15 = i_15 + int(1);
    }
    return result_8;
}

inline __device__ float pca_axis_align_0(float4  quat_0, float3  v_1, uint axis_0)
{
    float inv_0 = (F32_rsqrt(((F32_max((dot_1(quat_0, quat_0)), (9.99999968265522539e-21f))))));
    float w_0 = quat_0.x * inv_0;
    float x_8 = quat_0.y * inv_0;
    float y_5 = quat_0.z * inv_0;
    float z_1 = quat_0.w * inv_0;
    float3  c_4;
    if(axis_0 == 0U)
    {
        c_4 = make_float3 (1.0f - 2.0f * (y_5 * y_5 + z_1 * z_1), 2.0f * (x_8 * y_5 + z_1 * w_0), 2.0f * (x_8 * z_1 - y_5 * w_0));
    }
    else
    {
        if(axis_0 == 1U)
        {
            c_4 = make_float3 (2.0f * (x_8 * y_5 - z_1 * w_0), 1.0f - 2.0f * (x_8 * x_8 + z_1 * z_1), 2.0f * (y_5 * z_1 + x_8 * w_0));
        }
        else
        {
            c_4 = make_float3 (2.0f * (x_8 * z_1 + y_5 * w_0), 2.0f * (y_5 * z_1 - x_8 * w_0), 1.0f - 2.0f * (x_8 * x_8 + y_5 * y_5));
        }
    }
    return (F32_abs((dot_0(c_4, v_1))));
}

inline __device__ float s_primal_ctx_dot_0(float4  _S100, float4  _S101)
{
    return dot_1(_S100, _S101);
}

inline __device__ float s_primal_ctx_rsqrt_0(float _S102)
{
    return (F32_rsqrt((_S102)));
}

inline __device__ float s_primal_ctx_dot_1(float3  _S103, float3  _S104)
{
    return dot_0(_S103, _S104);
}

inline __device__ void s_bwd_prop_abs_0(DiffPair_float_0 * _S105, float _S106)
{
    _d_abs_0(_S105, _S106);
    return;
}

inline __device__ void s_bwd_prop_dot_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S107, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S108, float _S109)
{
    _d_dot_0(_S107, _S108, _S109);
    return;
}

inline __device__ void s_bwd_prop_rsqrt_0(DiffPair_float_0 * _S110, float _S111)
{
    _d_rsqrt_0(_S110, _S111);
    return;
}

inline __device__ void s_bwd_prop_dot_1(DiffPair_vectorx3Cfloatx2C4x3E_0 * _S112, DiffPair_vectorx3Cfloatx2C4x3E_0 * _S113, float _S114)
{
    _d_dot_1(_S112, _S113, _S114);
    return;
}

inline __device__ void s_bwd_prop_pca_axis_align_0(DiffPair_vectorx3Cfloatx2C4x3E_0 * dpquat_0, float3  v_2, uint axis_1, float _s_dOut_0)
{
    DiffPair_vectorx3Cfloatx2C4x3E_0 _S115 = *dpquat_0;
    float4  _S116 = (*dpquat_0).primal_0;
    float _S117 = s_primal_ctx_dot_0(_S116, _S116);
    float _S118 = (F32_max((_S117), (9.99999968265522539e-21f)));
    float _S119 = s_primal_ctx_rsqrt_0(_S118);
    float _S120 = (*dpquat_0).primal_0.x;
    float w_1 = _S120 * _S119;
    float _S121 = (*dpquat_0).primal_0.y;
    float x_9 = _S121 * _S119;
    float _S122 = (*dpquat_0).primal_0.z;
    float y_6 = _S122 * _S119;
    float _S123 = (*dpquat_0).primal_0.w;
    float z_2 = _S123 * _S119;
    bool _S124 = axis_1 == 0U;
    float3  c_5;
    bool _S125;
    if(_S124)
    {
        c_5 = make_float3 (1.0f - 2.0f * (y_6 * y_6 + z_2 * z_2), 2.0f * (x_9 * y_6 + z_2 * w_1), 2.0f * (x_9 * z_2 - y_6 * w_1));
        _S125 = false;
    }
    else
    {
        bool _S126 = axis_1 == 1U;
        if(_S126)
        {
            c_5 = make_float3 (2.0f * (x_9 * y_6 - z_2 * w_1), 1.0f - 2.0f * (x_9 * x_9 + z_2 * z_2), 2.0f * (y_6 * z_2 + x_9 * w_1));
        }
        else
        {
            c_5 = make_float3 (2.0f * (x_9 * z_2 + y_6 * w_1), 2.0f * (y_6 * z_2 - x_9 * w_1), 1.0f - 2.0f * (x_9 * x_9 + y_6 * y_6));
        }
        _S125 = _S126;
    }
    float _S127 = s_primal_ctx_dot_1(c_5, v_2);
    DiffPair_float_0 _S128;
    (&_S128)->primal_0 = _S127;
    (&_S128)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S128, _s_dOut_0);
    float3  _S129 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S130;
    (&_S130)->primal_0 = c_5;
    (&_S130)->differential_0 = _S129;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S131;
    (&_S131)->primal_0 = v_2;
    (&_S131)->differential_0 = _S129;
    s_bwd_prop_dot_0(&_S130, &_S131, _S128.differential_0);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S132 = _S130;
    float _S133;
    float _S134;
    float _S135;
    float _S136;
    if(_S124)
    {
        float _S137 = 2.0f * _S132.differential_0.z;
        float _S138 = - _S137;
        float _S139 = 2.0f * _S132.differential_0.y;
        float _S140 = 2.0f * - _S132.differential_0.x;
        float _S141 = z_2 * _S140;
        float _S142 = y_6 * _S140;
        float _S143 = z_2 * _S137 + y_6 * _S139;
        float _S144 = w_1 * _S138 + x_9 * _S139 + _S142 + _S142;
        float _S145 = y_6 * _S138 + z_2 * _S139;
        _S133 = x_9 * _S137 + w_1 * _S139 + _S141 + _S141;
        _S134 = _S144;
        _S135 = _S143;
        _S136 = _S145;
    }
    else
    {
        if(_S125)
        {
            float _S146 = 2.0f * _S132.differential_0.z;
            float _S147 = 2.0f * - _S132.differential_0.y;
            float _S148 = z_2 * _S147;
            float _S149 = x_9 * _S147;
            float _S150 = 2.0f * _S132.differential_0.x;
            float _S151 = - _S150;
            float _S152 = z_2 * _S146 + x_9 * _S150;
            float _S153 = w_1 * _S146 + _S149 + _S149 + y_6 * _S150;
            float _S154 = x_9 * _S146 + z_2 * _S151;
            _S133 = y_6 * _S146 + _S148 + _S148 + w_1 * _S151;
            _S134 = _S152;
            _S135 = _S153;
            _S136 = _S154;
        }
        else
        {
            float _S155 = 2.0f * - _S132.differential_0.z;
            float _S156 = y_6 * _S155;
            float _S157 = x_9 * _S155;
            float _S158 = 2.0f * _S132.differential_0.y;
            float _S159 = - _S158;
            float _S160 = 2.0f * _S132.differential_0.x;
            float _S161 = _S157 + _S157 + w_1 * _S159 + z_2 * _S160;
            float _S162 = _S156 + _S156 + z_2 * _S158 + w_1 * _S160;
            float _S163 = x_9 * _S159 + y_6 * _S160;
            _S133 = y_6 * _S158 + x_9 * _S160;
            _S134 = _S162;
            _S135 = _S161;
            _S136 = _S163;
        }
    }
    float _S164 = _S119 * _S133;
    float _S165 = _S119 * _S134;
    float _S166 = _S119 * _S135;
    float _S167 = _S119 * _S136;
    float _S168 = _S123 * _S133 + _S122 * _S134 + _S121 * _S135 + _S120 * _S136;
    DiffPair_float_0 _S169;
    (&_S169)->primal_0 = _S118;
    (&_S169)->differential_0 = 0.0f;
    s_bwd_prop_rsqrt_0(&_S169, _S168);
    DiffPair_float_0 _S170;
    (&_S170)->primal_0 = _S117;
    (&_S170)->differential_0 = 0.0f;
    DiffPair_float_0 _S171;
    (&_S171)->primal_0 = 9.99999968265522539e-21f;
    (&_S171)->differential_0 = 0.0f;
    _d_max_0(&_S170, &_S171, _S169.differential_0);
    float4  _S172 = make_float4 (0.0f);
    DiffPair_vectorx3Cfloatx2C4x3E_0 _S173;
    (&_S173)->primal_0 = _S115.primal_0;
    (&_S173)->differential_0 = _S172;
    DiffPair_vectorx3Cfloatx2C4x3E_0 _S174;
    (&_S174)->primal_0 = _S115.primal_0;
    (&_S174)->differential_0 = _S172;
    s_bwd_prop_dot_1(&_S173, &_S174, _S170.differential_0);
    float4  _S175 = _S174.differential_0 + _S173.differential_0 + make_float4 (_S167, _S166, _S165, _S164);
    dpquat_0->primal_0 = (*dpquat_0).primal_0;
    dpquat_0->differential_0 = _S175;
    return;
}

inline __device__ void s_bwd_pca_axis_align_0(DiffPair_vectorx3Cfloatx2C4x3E_0 * _S176, float3  _S177, uint _S178, float _S179)
{
    s_bwd_prop_pca_axis_align_0(_S176, _S177, _S178, _S179);
    return;
}

inline __device__ float3  pca_reg_owner_0(uint r_4, uint mode_0, uint n_5, uint k_12, uint window_0, float * means_1, float * quats_0, float * scales_0, int * perm_1, float4  * sorted_9, float4  * nodes_4, int4  * meta_4, int * ranges_4, int * nbrs_0, uint store_list_0, float * radii_0, uint gate_means_0, float * v_means_0, float * v_quats_0, float * v_scales_0, float w_plane_0, float w_tangent_0, float w_normal_0, float w_surfel_0)
{
    bool _S180;
    bool _S181;
    bool _S182;
    bool _S183;
    bool _S184;
    bool _S185;
    bool _S186;
    bool _S187;
    bool _S188;
    PcaKnn_0 h_3;
    PcaKnn_reset_0(&h_3);
    bool by_rank_0 = mode_0 != 3U;
    uint self_3 = uint(*(perm_1 + r_4));
    float3  q_7;
    if(by_rank_0)
    {
        q_7 = float3 {(*(sorted_9 + r_4)).x, (*(sorted_9 + r_4)).y, (*(sorted_9 + r_4)).z};
    }
    else
    {
        float3  _S189 = pca_load3_0(means_1, self_3);
        q_7 = _S189;
    }
    int id_2;
    uint axis_2;
    bool scatter_0;
    if(mode_0 == 0U)
    {
        pca_knn_tree_0(r_4, n_5, k_12, sorted_9, nodes_4, meta_4, ranges_4, &h_3);
    }
    else
    {
        if(mode_0 == 2U)
        {
            uint half_0 = (U32_max((window_0), (k_12 + 1U))) / 2U;
            if(r_4 > half_0)
            {
                axis_2 = r_4 - half_0;
            }
            else
            {
                axis_2 = 0U;
            }
            PcaKnn_scan_0(&h_3, q_7, r_4, axis_2, (U32_min((r_4 + half_0 + 1U), (n_5))), 0U, 0U, sorted_9, k_12);
        }
        else
        {
            if(mode_0 == 1U)
            {
                if(r_4 > (k_12 / 2U))
                {
                    axis_2 = r_4 - k_12 / 2U;
                }
                else
                {
                    axis_2 = 0U;
                }
                if((axis_2 + k_12 + 1U) > n_5)
                {
                    uint _S190 = k_12 + 1U;
                    if(n_5 > _S190)
                    {
                        axis_2 = n_5 - _S190;
                    }
                    else
                    {
                        axis_2 = 0U;
                    }
                }
                if(axis_2 >= r_4)
                {
                    id_2 = int(1);
                }
                else
                {
                    id_2 = int(0);
                }
                uint i_16 = axis_2 + uint(id_2);
                if(0U < k_12)
                {
                    scatter_0 = i_16 < n_5;
                }
                else
                {
                    scatter_0 = false;
                }
                if(scatter_0)
                {
                    (&h_3)->d_0[int(0)] = 0.0f;
                    (&h_3)->id_1[int(0)] = i_16;
                }
                uint i_17 = axis_2 + 1U;
                if(i_17 >= r_4)
                {
                    id_2 = int(1);
                }
                else
                {
                    id_2 = int(0);
                }
                uint i_18 = i_17 + uint(id_2);
                if(1U < k_12)
                {
                    scatter_0 = i_18 < n_5;
                }
                else
                {
                    scatter_0 = false;
                }
                if(scatter_0)
                {
                    (&h_3)->d_0[int(1)] = 0.0f;
                    (&h_3)->id_1[int(1)] = i_18;
                }
                uint i_19 = axis_2 + 2U;
                if(i_19 >= r_4)
                {
                    id_2 = int(1);
                }
                else
                {
                    id_2 = int(0);
                }
                uint i_20 = i_19 + uint(id_2);
                if(2U < k_12)
                {
                    scatter_0 = i_20 < n_5;
                }
                else
                {
                    scatter_0 = false;
                }
                if(scatter_0)
                {
                    (&h_3)->d_0[int(2)] = 0.0f;
                    (&h_3)->id_1[int(2)] = i_20;
                }
                uint i_21 = axis_2 + 3U;
                if(i_21 >= r_4)
                {
                    id_2 = int(1);
                }
                else
                {
                    id_2 = int(0);
                }
                uint i_22 = i_21 + uint(id_2);
                if(3U < k_12)
                {
                    scatter_0 = i_22 < n_5;
                }
                else
                {
                    scatter_0 = false;
                }
                if(scatter_0)
                {
                    (&h_3)->d_0[int(3)] = 0.0f;
                    (&h_3)->id_1[int(3)] = i_22;
                }
                uint i_23 = axis_2 + 4U;
                if(i_23 >= r_4)
                {
                    id_2 = int(1);
                }
                else
                {
                    id_2 = int(0);
                }
                uint i_24 = i_23 + uint(id_2);
                if(4U < k_12)
                {
                    scatter_0 = i_24 < n_5;
                }
                else
                {
                    scatter_0 = false;
                }
                if(scatter_0)
                {
                    (&h_3)->d_0[int(4)] = 0.0f;
                    (&h_3)->id_1[int(4)] = i_24;
                }
                uint i_25 = axis_2 + 5U;
                if(i_25 >= r_4)
                {
                    id_2 = int(1);
                }
                else
                {
                    id_2 = int(0);
                }
                uint i_26 = i_25 + uint(id_2);
                if(5U < k_12)
                {
                    scatter_0 = i_26 < n_5;
                }
                else
                {
                    scatter_0 = false;
                }
                if(scatter_0)
                {
                    (&h_3)->d_0[int(5)] = 0.0f;
                    (&h_3)->id_1[int(5)] = i_26;
                }
                uint i_27 = axis_2 + 6U;
                if(i_27 >= r_4)
                {
                    id_2 = int(1);
                }
                else
                {
                    id_2 = int(0);
                }
                uint i_28 = i_27 + uint(id_2);
                if(6U < k_12)
                {
                    scatter_0 = i_28 < n_5;
                }
                else
                {
                    scatter_0 = false;
                }
                if(scatter_0)
                {
                    (&h_3)->d_0[int(6)] = 0.0f;
                    (&h_3)->id_1[int(6)] = i_28;
                }
                uint i_29 = axis_2 + 7U;
                if(i_29 >= r_4)
                {
                    id_2 = int(1);
                }
                else
                {
                    id_2 = int(0);
                }
                uint i_30 = i_29 + uint(id_2);
                if(7U < k_12)
                {
                    scatter_0 = i_30 < n_5;
                }
                else
                {
                    scatter_0 = false;
                }
                if(scatter_0)
                {
                    (&h_3)->d_0[int(7)] = 0.0f;
                    (&h_3)->id_1[int(7)] = i_30;
                }
            }
            else
            {
                if(0U < k_12)
                {
                    id_2 = *(nbrs_0 + r_4 * k_12);
                }
                else
                {
                    id_2 = int(-1);
                }
                if(id_2 >= int(0))
                {
                    (&h_3)->d_0[int(0)] = 0.0f;
                    (&h_3)->id_1[int(0)] = uint(id_2);
                }
                if(1U < k_12)
                {
                    id_2 = *(nbrs_0 + (r_4 * k_12 + 1U));
                }
                else
                {
                    id_2 = int(-1);
                }
                if(id_2 >= int(0))
                {
                    (&h_3)->d_0[int(1)] = 0.0f;
                    (&h_3)->id_1[int(1)] = uint(id_2);
                }
                if(2U < k_12)
                {
                    id_2 = *(nbrs_0 + (r_4 * k_12 + 2U));
                }
                else
                {
                    id_2 = int(-1);
                }
                if(id_2 >= int(0))
                {
                    (&h_3)->d_0[int(2)] = 0.0f;
                    (&h_3)->id_1[int(2)] = uint(id_2);
                }
                if(3U < k_12)
                {
                    id_2 = *(nbrs_0 + (r_4 * k_12 + 3U));
                }
                else
                {
                    id_2 = int(-1);
                }
                if(id_2 >= int(0))
                {
                    (&h_3)->d_0[int(3)] = 0.0f;
                    (&h_3)->id_1[int(3)] = uint(id_2);
                }
                if(4U < k_12)
                {
                    id_2 = *(nbrs_0 + (r_4 * k_12 + 4U));
                }
                else
                {
                    id_2 = int(-1);
                }
                if(id_2 >= int(0))
                {
                    (&h_3)->d_0[int(4)] = 0.0f;
                    (&h_3)->id_1[int(4)] = uint(id_2);
                }
                if(5U < k_12)
                {
                    id_2 = *(nbrs_0 + (r_4 * k_12 + 5U));
                }
                else
                {
                    id_2 = int(-1);
                }
                if(id_2 >= int(0))
                {
                    (&h_3)->d_0[int(5)] = 0.0f;
                    (&h_3)->id_1[int(5)] = uint(id_2);
                }
                if(6U < k_12)
                {
                    id_2 = *(nbrs_0 + (r_4 * k_12 + 6U));
                }
                else
                {
                    id_2 = int(-1);
                }
                if(id_2 >= int(0))
                {
                    (&h_3)->d_0[int(6)] = 0.0f;
                    (&h_3)->id_1[int(6)] = uint(id_2);
                }
                if(7U < k_12)
                {
                    id_2 = *(nbrs_0 + (r_4 * k_12 + 7U));
                }
                else
                {
                    id_2 = int(-1);
                }
                if(id_2 >= int(0))
                {
                    (&h_3)->d_0[int(7)] = 0.0f;
                    (&h_3)->id_1[int(7)] = uint(id_2);
                }
            }
        }
    }
    PcaKnn_0 _S191 = h_3;
    uint _S192 = PcaKnn_count_0(&_S191, k_12);
    if(store_list_0 != 0U)
    {
        if(0U < k_12)
        {
            int * _S193 = nbrs_0 + r_4 * k_12;
            if(((&h_3)->d_0[int(0)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_0)
                {
                    id_2 = *(perm_1 + (&h_3)->id_1[int(0)]);
                }
                else
                {
                    id_2 = int((&h_3)->id_1[int(0)]);
                }
            }
            else
            {
                id_2 = int(-1);
            }
            *_S193 = id_2;
        }
        if(1U < k_12)
        {
            int * _S194 = nbrs_0 + (r_4 * k_12 + 1U);
            if(((&h_3)->d_0[int(1)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_0)
                {
                    id_2 = *(perm_1 + (&h_3)->id_1[int(1)]);
                }
                else
                {
                    id_2 = int((&h_3)->id_1[int(1)]);
                }
            }
            else
            {
                id_2 = int(-1);
            }
            *_S194 = id_2;
        }
        if(2U < k_12)
        {
            int * _S195 = nbrs_0 + (r_4 * k_12 + 2U);
            if(((&h_3)->d_0[int(2)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_0)
                {
                    id_2 = *(perm_1 + (&h_3)->id_1[int(2)]);
                }
                else
                {
                    id_2 = int((&h_3)->id_1[int(2)]);
                }
            }
            else
            {
                id_2 = int(-1);
            }
            *_S195 = id_2;
        }
        if(3U < k_12)
        {
            int * _S196 = nbrs_0 + (r_4 * k_12 + 3U);
            if(((&h_3)->d_0[int(3)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_0)
                {
                    id_2 = *(perm_1 + (&h_3)->id_1[int(3)]);
                }
                else
                {
                    id_2 = int((&h_3)->id_1[int(3)]);
                }
            }
            else
            {
                id_2 = int(-1);
            }
            *_S196 = id_2;
        }
        if(4U < k_12)
        {
            int * _S197 = nbrs_0 + (r_4 * k_12 + 4U);
            if(((&h_3)->d_0[int(4)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_0)
                {
                    id_2 = *(perm_1 + (&h_3)->id_1[int(4)]);
                }
                else
                {
                    id_2 = int((&h_3)->id_1[int(4)]);
                }
            }
            else
            {
                id_2 = int(-1);
            }
            *_S197 = id_2;
        }
        if(5U < k_12)
        {
            int * _S198 = nbrs_0 + (r_4 * k_12 + 5U);
            if(((&h_3)->d_0[int(5)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_0)
                {
                    id_2 = *(perm_1 + (&h_3)->id_1[int(5)]);
                }
                else
                {
                    id_2 = int((&h_3)->id_1[int(5)]);
                }
            }
            else
            {
                id_2 = int(-1);
            }
            *_S198 = id_2;
        }
        if(6U < k_12)
        {
            int * _S199 = nbrs_0 + (r_4 * k_12 + 6U);
            if(((&h_3)->d_0[int(6)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_0)
                {
                    id_2 = *(perm_1 + (&h_3)->id_1[int(6)]);
                }
                else
                {
                    id_2 = int((&h_3)->id_1[int(6)]);
                }
            }
            else
            {
                id_2 = int(-1);
            }
            *_S199 = id_2;
        }
        if(7U < k_12)
        {
            int * _S200 = nbrs_0 + (r_4 * k_12 + 7U);
            if(((&h_3)->d_0[int(7)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_0)
                {
                    id_2 = *(perm_1 + (&h_3)->id_1[int(7)]);
                }
                else
                {
                    id_2 = int((&h_3)->id_1[int(7)]);
                }
            }
            else
            {
                id_2 = int(-1);
            }
            *_S200 = id_2;
        }
    }
    if(_S192 < 2U)
    {
        return make_float3 (0.0f);
    }
    float3  so_0;
    float3  sd_0;
    float3  s1_0;
    float3  _S201 = make_float3 (0.0f);
    for(;;)
    {
        for(;;)
        {
            for(;;)
            {
                bool _S202 = 0U >= k_12;
                _S180 = _S202;
                if(_S202)
                {
                    scatter_0 = true;
                }
                else
                {
                    scatter_0 = !(((&h_3)->d_0[int(0)]) < 3.00000000549775576e+38f);
                }
                if(scatter_0)
                {
                    s1_0 = _S201;
                    sd_0 = _S201;
                    so_0 = _S201;
                    break;
                }
                if(by_rank_0)
                {
                    s1_0 = float3 {(*(sorted_9 + (&h_3)->id_1[int(0)])).x, (*(sorted_9 + (&h_3)->id_1[int(0)])).y, (*(sorted_9 + (&h_3)->id_1[int(0)])).z};
                }
                else
                {
                    float3  _S203 = pca_load3_0(means_1, (&h_3)->id_1[int(0)]);
                    s1_0 = _S203;
                }
                float3  d_3 = s1_0 - q_7;
                float3  _S204 = d_3 * d_3;
                float _S205 = d_3.x;
                float _S206 = d_3.y;
                float _S207 = d_3.z;
                float3  _S208 = make_float3 (_S205 * _S206, _S205 * _S207, _S206 * _S207);
                s1_0 = d_3;
                sd_0 = _S204;
                so_0 = _S208;
                break;
            }
            break;
        }
        float3  _S209;
        for(;;)
        {
            for(;;)
            {
                bool _S210 = 1U >= k_12;
                _S181 = _S210;
                if(_S210)
                {
                    scatter_0 = true;
                }
                else
                {
                    scatter_0 = !(((&h_3)->d_0[int(1)]) < 3.00000000549775576e+38f);
                }
                if(scatter_0)
                {
                    break;
                }
                if(by_rank_0)
                {
                    _S209 = float3 {(*(sorted_9 + (&h_3)->id_1[int(1)])).x, (*(sorted_9 + (&h_3)->id_1[int(1)])).y, (*(sorted_9 + (&h_3)->id_1[int(1)])).z};
                }
                else
                {
                    float3  _S211 = pca_load3_0(means_1, (&h_3)->id_1[int(1)]);
                    _S209 = _S211;
                }
                float3  d_4 = _S209 - q_7;
                float3  sd_1 = sd_0 + d_4 * d_4;
                float _S212 = d_4.x;
                float _S213 = d_4.y;
                float _S214 = d_4.z;
                float3  so_1 = so_0 + make_float3 (_S212 * _S213, _S212 * _S214, _S213 * _S214);
                s1_0 = s1_0 + d_4;
                sd_0 = sd_1;
                so_0 = so_1;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S215 = 2U >= k_12;
                _S182 = _S215;
                if(_S215)
                {
                    scatter_0 = true;
                }
                else
                {
                    scatter_0 = !(((&h_3)->d_0[int(2)]) < 3.00000000549775576e+38f);
                }
                if(scatter_0)
                {
                    break;
                }
                if(by_rank_0)
                {
                    _S209 = float3 {(*(sorted_9 + (&h_3)->id_1[int(2)])).x, (*(sorted_9 + (&h_3)->id_1[int(2)])).y, (*(sorted_9 + (&h_3)->id_1[int(2)])).z};
                }
                else
                {
                    float3  _S216 = pca_load3_0(means_1, (&h_3)->id_1[int(2)]);
                    _S209 = _S216;
                }
                float3  d_5 = _S209 - q_7;
                float3  sd_2 = sd_0 + d_5 * d_5;
                float _S217 = d_5.x;
                float _S218 = d_5.y;
                float _S219 = d_5.z;
                float3  so_2 = so_0 + make_float3 (_S217 * _S218, _S217 * _S219, _S218 * _S219);
                s1_0 = s1_0 + d_5;
                sd_0 = sd_2;
                so_0 = so_2;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S220 = 3U >= k_12;
                _S183 = _S220;
                if(_S220)
                {
                    scatter_0 = true;
                }
                else
                {
                    scatter_0 = !(((&h_3)->d_0[int(3)]) < 3.00000000549775576e+38f);
                }
                if(scatter_0)
                {
                    break;
                }
                if(by_rank_0)
                {
                    _S209 = float3 {(*(sorted_9 + (&h_3)->id_1[int(3)])).x, (*(sorted_9 + (&h_3)->id_1[int(3)])).y, (*(sorted_9 + (&h_3)->id_1[int(3)])).z};
                }
                else
                {
                    float3  _S221 = pca_load3_0(means_1, (&h_3)->id_1[int(3)]);
                    _S209 = _S221;
                }
                float3  d_6 = _S209 - q_7;
                float3  sd_3 = sd_0 + d_6 * d_6;
                float _S222 = d_6.x;
                float _S223 = d_6.y;
                float _S224 = d_6.z;
                float3  so_3 = so_0 + make_float3 (_S222 * _S223, _S222 * _S224, _S223 * _S224);
                s1_0 = s1_0 + d_6;
                sd_0 = sd_3;
                so_0 = so_3;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S225 = 4U >= k_12;
                _S184 = _S225;
                if(_S225)
                {
                    scatter_0 = true;
                }
                else
                {
                    scatter_0 = !(((&h_3)->d_0[int(4)]) < 3.00000000549775576e+38f);
                }
                if(scatter_0)
                {
                    break;
                }
                if(by_rank_0)
                {
                    _S209 = float3 {(*(sorted_9 + (&h_3)->id_1[int(4)])).x, (*(sorted_9 + (&h_3)->id_1[int(4)])).y, (*(sorted_9 + (&h_3)->id_1[int(4)])).z};
                }
                else
                {
                    float3  _S226 = pca_load3_0(means_1, (&h_3)->id_1[int(4)]);
                    _S209 = _S226;
                }
                float3  d_7 = _S209 - q_7;
                float3  sd_4 = sd_0 + d_7 * d_7;
                float _S227 = d_7.x;
                float _S228 = d_7.y;
                float _S229 = d_7.z;
                float3  so_4 = so_0 + make_float3 (_S227 * _S228, _S227 * _S229, _S228 * _S229);
                s1_0 = s1_0 + d_7;
                sd_0 = sd_4;
                so_0 = so_4;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S230 = 5U >= k_12;
                _S185 = _S230;
                if(_S230)
                {
                    scatter_0 = true;
                }
                else
                {
                    scatter_0 = !(((&h_3)->d_0[int(5)]) < 3.00000000549775576e+38f);
                }
                if(scatter_0)
                {
                    break;
                }
                if(by_rank_0)
                {
                    _S209 = float3 {(*(sorted_9 + (&h_3)->id_1[int(5)])).x, (*(sorted_9 + (&h_3)->id_1[int(5)])).y, (*(sorted_9 + (&h_3)->id_1[int(5)])).z};
                }
                else
                {
                    float3  _S231 = pca_load3_0(means_1, (&h_3)->id_1[int(5)]);
                    _S209 = _S231;
                }
                float3  d_8 = _S209 - q_7;
                float3  sd_5 = sd_0 + d_8 * d_8;
                float _S232 = d_8.x;
                float _S233 = d_8.y;
                float _S234 = d_8.z;
                float3  so_5 = so_0 + make_float3 (_S232 * _S233, _S232 * _S234, _S233 * _S234);
                s1_0 = s1_0 + d_8;
                sd_0 = sd_5;
                so_0 = so_5;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S235 = 6U >= k_12;
                _S186 = _S235;
                if(_S235)
                {
                    scatter_0 = true;
                }
                else
                {
                    scatter_0 = !(((&h_3)->d_0[int(6)]) < 3.00000000549775576e+38f);
                }
                if(scatter_0)
                {
                    break;
                }
                if(by_rank_0)
                {
                    _S209 = float3 {(*(sorted_9 + (&h_3)->id_1[int(6)])).x, (*(sorted_9 + (&h_3)->id_1[int(6)])).y, (*(sorted_9 + (&h_3)->id_1[int(6)])).z};
                }
                else
                {
                    float3  _S236 = pca_load3_0(means_1, (&h_3)->id_1[int(6)]);
                    _S209 = _S236;
                }
                float3  d_9 = _S209 - q_7;
                float3  sd_6 = sd_0 + d_9 * d_9;
                float _S237 = d_9.x;
                float _S238 = d_9.y;
                float _S239 = d_9.z;
                float3  so_6 = so_0 + make_float3 (_S237 * _S238, _S237 * _S239, _S238 * _S239);
                s1_0 = s1_0 + d_9;
                sd_0 = sd_6;
                so_0 = so_6;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S240 = 7U >= k_12;
                _S187 = _S240;
                if(_S240)
                {
                    scatter_0 = true;
                }
                else
                {
                    scatter_0 = !(((&h_3)->d_0[int(7)]) < 3.00000000549775576e+38f);
                }
                if(scatter_0)
                {
                    break;
                }
                if(by_rank_0)
                {
                    _S209 = float3 {(*(sorted_9 + (&h_3)->id_1[int(7)])).x, (*(sorted_9 + (&h_3)->id_1[int(7)])).y, (*(sorted_9 + (&h_3)->id_1[int(7)])).z};
                }
                else
                {
                    float3  _S241 = pca_load3_0(means_1, (&h_3)->id_1[int(7)]);
                    _S209 = _S241;
                }
                float3  d_10 = _S209 - q_7;
                float3  sd_7 = sd_0 + d_10 * d_10;
                float _S242 = d_10.x;
                float _S243 = d_10.y;
                float _S244 = d_10.z;
                float3  so_7 = so_0 + make_float3 (_S242 * _S243, _S242 * _S244, _S243 * _S244);
                s1_0 = s1_0 + d_10;
                sd_0 = sd_7;
                so_0 = so_7;
                break;
            }
            break;
        }
        break;
    }
    float inv_k_0 = 1.0f / float(_S192 + 1U);
    float3  mean_0 = s1_0 * make_float3 (inv_k_0);
    float _S245 = mean_0.x;
    float _S246 = mean_0.y;
    float _S247 = mean_0.z;
    float3  ev_1;
    float3  e0_1;
    float3  e1_1;
    float3  e2_1;
    pca_sym3_eigen_0(sd_0 * make_float3 (inv_k_0) - mean_0 * mean_0, so_0 * make_float3 (inv_k_0) - make_float3 (_S245 * _S246, _S245 * _S247, _S246 * _S247), &ev_1, &e0_1, &e1_1, &e2_1);
    if(!all_0(isfinite_0(ev_1)))
    {
        scatter_0 = true;
    }
    else
    {
        scatter_0 = !all_0(isfinite_0(e0_1));
    }
    if(scatter_0)
    {
        return _S201;
    }
    float _S248 = 2.0f * inv_k_0;
    Matrix<float, 3, 3>  _S249 = makeMatrix<float, 3, 3> (_S248 * w_plane_0) * makeMatrix<float, 3, 3> (make_float3 (e0_1.x) * e0_1, make_float3 (e0_1.y) * e0_1, make_float3 (e0_1.z) * e0_1) + makeMatrix<float, 3, 3> (_S248 * w_tangent_0) * (makeMatrix<float, 3, 3> (make_float3 (e2_1.x) * e2_1, make_float3 (e2_1.y) * e2_1, make_float3 (e2_1.z) * e2_1) - makeMatrix<float, 3, 3> (make_float3 (e1_1.x) * e1_1, make_float3 (e1_1.y) * e1_1, make_float3 (e1_1.z) * e1_1));
    float3  v_3 = mul_0(_S249, - mean_0);
    if(w_plane_0 != 0.0f)
    {
        scatter_0 = true;
    }
    else
    {
        scatter_0 = w_tangent_0 != 0.0f;
    }
    bool _S250;
    if(scatter_0)
    {
        if(gate_means_0 == 0U)
        {
            _S250 = true;
        }
        else
        {
            _S250 = (*(radii_0 + self_3)) > 0.0f;
        }
    }
    else
    {
        _S250 = false;
    }
    if(_S250)
    {
        float * _S251 = v_means_0 + 3U * self_3;
        float _S252 = atomicAdd(_S251 + int(0), v_3.x);
        float _S253 = atomicAdd(_S251 + int(1) + int(0), v_3.y);
        float _S254 = atomicAdd(_S251 + int(2) + int(0), v_3.z);
    }
    for(;;)
    {
        for(;;)
        {
            for(;;)
            {
                bool _S255 = !scatter_0;
                _S188 = _S255;
                if(_S255)
                {
                    scatter_0 = true;
                }
                else
                {
                    scatter_0 = _S180;
                }
                if(scatter_0)
                {
                    scatter_0 = true;
                }
                else
                {
                    scatter_0 = !(((&h_3)->d_0[int(0)]) < 3.00000000549775576e+38f);
                }
                if(scatter_0)
                {
                    break;
                }
                if(by_rank_0)
                {
                    axis_2 = uint(*(perm_1 + (&h_3)->id_1[int(0)]));
                }
                else
                {
                    axis_2 = (&h_3)->id_1[int(0)];
                }
                if(gate_means_0 != 0U)
                {
                    scatter_0 = !((*(radii_0 + axis_2)) > 0.0f);
                }
                else
                {
                    scatter_0 = false;
                }
                if(scatter_0)
                {
                    break;
                }
                if(by_rank_0)
                {
                    s1_0 = float3 {(*(sorted_9 + (&h_3)->id_1[int(0)])).x, (*(sorted_9 + (&h_3)->id_1[int(0)])).y, (*(sorted_9 + (&h_3)->id_1[int(0)])).z};
                }
                else
                {
                    float3  _S256 = pca_load3_0(means_1, (&h_3)->id_1[int(0)]);
                    s1_0 = _S256;
                }
                float3  v_4 = mul_0(_S249, s1_0 - q_7 - mean_0);
                float * _S257 = v_means_0 + 3U * axis_2;
                float _S258 = atomicAdd(_S257 + int(0), v_4.x);
                float _S259 = atomicAdd(_S257 + int(1) + int(0), v_4.y);
                float _S260 = atomicAdd(_S257 + int(2) + int(0), v_4.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S188)
                {
                    scatter_0 = true;
                }
                else
                {
                    scatter_0 = _S181;
                }
                if(scatter_0)
                {
                    scatter_0 = true;
                }
                else
                {
                    scatter_0 = !(((&h_3)->d_0[int(1)]) < 3.00000000549775576e+38f);
                }
                if(scatter_0)
                {
                    break;
                }
                if(by_rank_0)
                {
                    axis_2 = uint(*(perm_1 + (&h_3)->id_1[int(1)]));
                }
                else
                {
                    axis_2 = (&h_3)->id_1[int(1)];
                }
                if(gate_means_0 != 0U)
                {
                    scatter_0 = !((*(radii_0 + axis_2)) > 0.0f);
                }
                else
                {
                    scatter_0 = false;
                }
                if(scatter_0)
                {
                    break;
                }
                if(by_rank_0)
                {
                    s1_0 = float3 {(*(sorted_9 + (&h_3)->id_1[int(1)])).x, (*(sorted_9 + (&h_3)->id_1[int(1)])).y, (*(sorted_9 + (&h_3)->id_1[int(1)])).z};
                }
                else
                {
                    float3  _S261 = pca_load3_0(means_1, (&h_3)->id_1[int(1)]);
                    s1_0 = _S261;
                }
                float3  v_5 = mul_0(_S249, s1_0 - q_7 - mean_0);
                float * _S262 = v_means_0 + 3U * axis_2;
                float _S263 = atomicAdd(_S262 + int(0), v_5.x);
                float _S264 = atomicAdd(_S262 + int(1) + int(0), v_5.y);
                float _S265 = atomicAdd(_S262 + int(2) + int(0), v_5.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S188)
                {
                    scatter_0 = true;
                }
                else
                {
                    scatter_0 = _S182;
                }
                if(scatter_0)
                {
                    scatter_0 = true;
                }
                else
                {
                    scatter_0 = !(((&h_3)->d_0[int(2)]) < 3.00000000549775576e+38f);
                }
                if(scatter_0)
                {
                    break;
                }
                if(by_rank_0)
                {
                    axis_2 = uint(*(perm_1 + (&h_3)->id_1[int(2)]));
                }
                else
                {
                    axis_2 = (&h_3)->id_1[int(2)];
                }
                if(gate_means_0 != 0U)
                {
                    scatter_0 = !((*(radii_0 + axis_2)) > 0.0f);
                }
                else
                {
                    scatter_0 = false;
                }
                if(scatter_0)
                {
                    break;
                }
                if(by_rank_0)
                {
                    s1_0 = float3 {(*(sorted_9 + (&h_3)->id_1[int(2)])).x, (*(sorted_9 + (&h_3)->id_1[int(2)])).y, (*(sorted_9 + (&h_3)->id_1[int(2)])).z};
                }
                else
                {
                    float3  _S266 = pca_load3_0(means_1, (&h_3)->id_1[int(2)]);
                    s1_0 = _S266;
                }
                float3  v_6 = mul_0(_S249, s1_0 - q_7 - mean_0);
                float * _S267 = v_means_0 + 3U * axis_2;
                float _S268 = atomicAdd(_S267 + int(0), v_6.x);
                float _S269 = atomicAdd(_S267 + int(1) + int(0), v_6.y);
                float _S270 = atomicAdd(_S267 + int(2) + int(0), v_6.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S188)
                {
                    scatter_0 = true;
                }
                else
                {
                    scatter_0 = _S183;
                }
                if(scatter_0)
                {
                    scatter_0 = true;
                }
                else
                {
                    scatter_0 = !(((&h_3)->d_0[int(3)]) < 3.00000000549775576e+38f);
                }
                if(scatter_0)
                {
                    break;
                }
                if(by_rank_0)
                {
                    axis_2 = uint(*(perm_1 + (&h_3)->id_1[int(3)]));
                }
                else
                {
                    axis_2 = (&h_3)->id_1[int(3)];
                }
                if(gate_means_0 != 0U)
                {
                    scatter_0 = !((*(radii_0 + axis_2)) > 0.0f);
                }
                else
                {
                    scatter_0 = false;
                }
                if(scatter_0)
                {
                    break;
                }
                if(by_rank_0)
                {
                    s1_0 = float3 {(*(sorted_9 + (&h_3)->id_1[int(3)])).x, (*(sorted_9 + (&h_3)->id_1[int(3)])).y, (*(sorted_9 + (&h_3)->id_1[int(3)])).z};
                }
                else
                {
                    float3  _S271 = pca_load3_0(means_1, (&h_3)->id_1[int(3)]);
                    s1_0 = _S271;
                }
                float3  v_7 = mul_0(_S249, s1_0 - q_7 - mean_0);
                float * _S272 = v_means_0 + 3U * axis_2;
                float _S273 = atomicAdd(_S272 + int(0), v_7.x);
                float _S274 = atomicAdd(_S272 + int(1) + int(0), v_7.y);
                float _S275 = atomicAdd(_S272 + int(2) + int(0), v_7.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S188)
                {
                    scatter_0 = true;
                }
                else
                {
                    scatter_0 = _S184;
                }
                if(scatter_0)
                {
                    scatter_0 = true;
                }
                else
                {
                    scatter_0 = !(((&h_3)->d_0[int(4)]) < 3.00000000549775576e+38f);
                }
                if(scatter_0)
                {
                    break;
                }
                if(by_rank_0)
                {
                    axis_2 = uint(*(perm_1 + (&h_3)->id_1[int(4)]));
                }
                else
                {
                    axis_2 = (&h_3)->id_1[int(4)];
                }
                if(gate_means_0 != 0U)
                {
                    scatter_0 = !((*(radii_0 + axis_2)) > 0.0f);
                }
                else
                {
                    scatter_0 = false;
                }
                if(scatter_0)
                {
                    break;
                }
                if(by_rank_0)
                {
                    s1_0 = float3 {(*(sorted_9 + (&h_3)->id_1[int(4)])).x, (*(sorted_9 + (&h_3)->id_1[int(4)])).y, (*(sorted_9 + (&h_3)->id_1[int(4)])).z};
                }
                else
                {
                    float3  _S276 = pca_load3_0(means_1, (&h_3)->id_1[int(4)]);
                    s1_0 = _S276;
                }
                float3  v_8 = mul_0(_S249, s1_0 - q_7 - mean_0);
                float * _S277 = v_means_0 + 3U * axis_2;
                float _S278 = atomicAdd(_S277 + int(0), v_8.x);
                float _S279 = atomicAdd(_S277 + int(1) + int(0), v_8.y);
                float _S280 = atomicAdd(_S277 + int(2) + int(0), v_8.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S188)
                {
                    scatter_0 = true;
                }
                else
                {
                    scatter_0 = _S185;
                }
                if(scatter_0)
                {
                    scatter_0 = true;
                }
                else
                {
                    scatter_0 = !(((&h_3)->d_0[int(5)]) < 3.00000000549775576e+38f);
                }
                if(scatter_0)
                {
                    break;
                }
                if(by_rank_0)
                {
                    axis_2 = uint(*(perm_1 + (&h_3)->id_1[int(5)]));
                }
                else
                {
                    axis_2 = (&h_3)->id_1[int(5)];
                }
                if(gate_means_0 != 0U)
                {
                    scatter_0 = !((*(radii_0 + axis_2)) > 0.0f);
                }
                else
                {
                    scatter_0 = false;
                }
                if(scatter_0)
                {
                    break;
                }
                if(by_rank_0)
                {
                    s1_0 = float3 {(*(sorted_9 + (&h_3)->id_1[int(5)])).x, (*(sorted_9 + (&h_3)->id_1[int(5)])).y, (*(sorted_9 + (&h_3)->id_1[int(5)])).z};
                }
                else
                {
                    float3  _S281 = pca_load3_0(means_1, (&h_3)->id_1[int(5)]);
                    s1_0 = _S281;
                }
                float3  v_9 = mul_0(_S249, s1_0 - q_7 - mean_0);
                float * _S282 = v_means_0 + 3U * axis_2;
                float _S283 = atomicAdd(_S282 + int(0), v_9.x);
                float _S284 = atomicAdd(_S282 + int(1) + int(0), v_9.y);
                float _S285 = atomicAdd(_S282 + int(2) + int(0), v_9.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S188)
                {
                    scatter_0 = true;
                }
                else
                {
                    scatter_0 = _S186;
                }
                if(scatter_0)
                {
                    scatter_0 = true;
                }
                else
                {
                    scatter_0 = !(((&h_3)->d_0[int(6)]) < 3.00000000549775576e+38f);
                }
                if(scatter_0)
                {
                    break;
                }
                if(by_rank_0)
                {
                    axis_2 = uint(*(perm_1 + (&h_3)->id_1[int(6)]));
                }
                else
                {
                    axis_2 = (&h_3)->id_1[int(6)];
                }
                if(gate_means_0 != 0U)
                {
                    scatter_0 = !((*(radii_0 + axis_2)) > 0.0f);
                }
                else
                {
                    scatter_0 = false;
                }
                if(scatter_0)
                {
                    break;
                }
                if(by_rank_0)
                {
                    s1_0 = float3 {(*(sorted_9 + (&h_3)->id_1[int(6)])).x, (*(sorted_9 + (&h_3)->id_1[int(6)])).y, (*(sorted_9 + (&h_3)->id_1[int(6)])).z};
                }
                else
                {
                    float3  _S286 = pca_load3_0(means_1, (&h_3)->id_1[int(6)]);
                    s1_0 = _S286;
                }
                float3  v_10 = mul_0(_S249, s1_0 - q_7 - mean_0);
                float * _S287 = v_means_0 + 3U * axis_2;
                float _S288 = atomicAdd(_S287 + int(0), v_10.x);
                float _S289 = atomicAdd(_S287 + int(1) + int(0), v_10.y);
                float _S290 = atomicAdd(_S287 + int(2) + int(0), v_10.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S188)
                {
                    scatter_0 = true;
                }
                else
                {
                    scatter_0 = _S187;
                }
                if(scatter_0)
                {
                    scatter_0 = true;
                }
                else
                {
                    scatter_0 = !(((&h_3)->d_0[int(7)]) < 3.00000000549775576e+38f);
                }
                if(scatter_0)
                {
                    break;
                }
                if(by_rank_0)
                {
                    axis_2 = uint(*(perm_1 + (&h_3)->id_1[int(7)]));
                }
                else
                {
                    axis_2 = (&h_3)->id_1[int(7)];
                }
                if(gate_means_0 != 0U)
                {
                    scatter_0 = !((*(radii_0 + axis_2)) > 0.0f);
                }
                else
                {
                    scatter_0 = false;
                }
                if(scatter_0)
                {
                    break;
                }
                if(by_rank_0)
                {
                    s1_0 = float3 {(*(sorted_9 + (&h_3)->id_1[int(7)])).x, (*(sorted_9 + (&h_3)->id_1[int(7)])).y, (*(sorted_9 + (&h_3)->id_1[int(7)])).z};
                }
                else
                {
                    float3  _S291 = pca_load3_0(means_1, (&h_3)->id_1[int(7)]);
                    s1_0 = _S291;
                }
                float3  v_11 = mul_0(_S249, s1_0 - q_7 - mean_0);
                float * _S292 = v_means_0 + 3U * axis_2;
                float _S293 = atomicAdd(_S292 + int(0), v_11.x);
                float _S294 = atomicAdd(_S292 + int(1) + int(0), v_11.y);
                float _S295 = atomicAdd(_S292 + int(2) + int(0), v_11.z);
                break;
            }
            break;
        }
        break;
    }
    float3  ls_0 = pca_load3_0(scales_0, self_3);
    float _S296 = ls_0.x;
    float _S297 = ls_0.y;
    if(_S296 <= _S297)
    {
        if(_S296 <= (ls_0.z))
        {
            axis_2 = 0U;
        }
        else
        {
            axis_2 = 2U;
        }
    }
    else
    {
        if(_S297 <= (ls_0.z))
        {
            axis_2 = 1U;
        }
        else
        {
            axis_2 = 2U;
        }
    }
    uint _S298 = 4U * self_3;
    float4  quat_1 = make_float4 (*(quats_0 + _S298), *(quats_0 + (_S298 + 1U)), *(quats_0 + (_S298 + 2U)), *(quats_0 + (_S298 + 3U)));
    float align_0 = pca_axis_align_0(quat_1, e0_1, axis_2);
    if(w_normal_0 != 0.0f)
    {
        float4  _S299 = make_float4 (0.0f);
        DiffPair_vectorx3Cfloatx2C4x3E_0 dq_0;
        (&dq_0)->primal_0 = quat_1;
        (&dq_0)->differential_0 = _S299;
        s_bwd_pca_axis_align_0(&dq_0, e0_1, axis_2, - w_normal_0);
        uint c_6 = 0U;
        for(;;)
        {
            if(c_6 < 4U)
            {
            }
            else
            {
                break;
            }
            float * _S300 = v_quats_0 + (_S298 + c_6);
            *_S300 = *_S300 + _slang_vector_get_element(dq_0.differential_0, c_6);
            c_6 = c_6 + 1U;
        }
    }
    if(w_surfel_0 != 0.0f)
    {
        float smin_0;
        if(axis_2 == 0U)
        {
            smin_0 = _S296;
        }
        else
        {
            if(axis_2 == 1U)
            {
                smin_0 = _S297;
            }
            else
            {
                smin_0 = ls_0.z;
            }
        }
        float * _S301 = v_scales_0 + (3U * self_3 + axis_2);
        *_S301 = *_S301 + w_surfel_0 * (F32_exp((smin_0)));
    }
    return make_float3 (ev_1.x, ev_1.z - ev_1.y, 1.0f - align_0);
}

inline __device__ float3  pca_reg_owner_1(uint r_5, uint mode_1, uint n_6, uint k_13, uint window_1, float * means_2, float * quats_1, float * scales_1, int * perm_2, float4  * sorted_10, float4  * nodes_5, int4  * meta_5, int * ranges_5, int * nbrs_1, uint store_list_1, float * radii_1, uint gate_means_1, float * v_means_1, float * v_quats_1, float * v_scales_1, float w_plane_1, float w_tangent_1, float w_normal_1, float w_surfel_1)
{
    bool _S302;
    bool _S303;
    bool _S304;
    bool _S305;
    bool _S306;
    bool _S307;
    bool _S308;
    bool _S309;
    bool _S310;
    bool _S311;
    bool _S312;
    bool _S313;
    bool _S314;
    bool _S315;
    bool _S316;
    bool _S317;
    bool _S318;
    PcaKnn_1 h_4;
    PcaKnn_reset_1(&h_4);
    bool by_rank_1 = mode_1 != 3U;
    uint self_4 = uint(*(perm_2 + r_5));
    float3  q_8;
    if(by_rank_1)
    {
        q_8 = float3 {(*(sorted_10 + r_5)).x, (*(sorted_10 + r_5)).y, (*(sorted_10 + r_5)).z};
    }
    else
    {
        float3  _S319 = pca_load3_0(means_2, self_4);
        q_8 = _S319;
    }
    int id_3;
    uint axis_3;
    bool scatter_1;
    if(mode_1 == 0U)
    {
        pca_knn_tree_1(r_5, n_6, k_13, sorted_10, nodes_5, meta_5, ranges_5, &h_4);
    }
    else
    {
        if(mode_1 == 2U)
        {
            uint half_1 = (U32_max((window_1), (k_13 + 1U))) / 2U;
            if(r_5 > half_1)
            {
                axis_3 = r_5 - half_1;
            }
            else
            {
                axis_3 = 0U;
            }
            PcaKnn_scan_1(&h_4, q_8, r_5, axis_3, (U32_min((r_5 + half_1 + 1U), (n_6))), 0U, 0U, sorted_10, k_13);
        }
        else
        {
            if(mode_1 == 1U)
            {
                if(r_5 > (k_13 / 2U))
                {
                    axis_3 = r_5 - k_13 / 2U;
                }
                else
                {
                    axis_3 = 0U;
                }
                if((axis_3 + k_13 + 1U) > n_6)
                {
                    uint _S320 = k_13 + 1U;
                    if(n_6 > _S320)
                    {
                        axis_3 = n_6 - _S320;
                    }
                    else
                    {
                        axis_3 = 0U;
                    }
                }
                if(axis_3 >= r_5)
                {
                    id_3 = int(1);
                }
                else
                {
                    id_3 = int(0);
                }
                uint i_31 = axis_3 + uint(id_3);
                if(0U < k_13)
                {
                    scatter_1 = i_31 < n_6;
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    (&h_4)->d_0[int(0)] = 0.0f;
                    (&h_4)->id_1[int(0)] = i_31;
                }
                uint i_32 = axis_3 + 1U;
                if(i_32 >= r_5)
                {
                    id_3 = int(1);
                }
                else
                {
                    id_3 = int(0);
                }
                uint i_33 = i_32 + uint(id_3);
                if(1U < k_13)
                {
                    scatter_1 = i_33 < n_6;
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    (&h_4)->d_0[int(1)] = 0.0f;
                    (&h_4)->id_1[int(1)] = i_33;
                }
                uint i_34 = axis_3 + 2U;
                if(i_34 >= r_5)
                {
                    id_3 = int(1);
                }
                else
                {
                    id_3 = int(0);
                }
                uint i_35 = i_34 + uint(id_3);
                if(2U < k_13)
                {
                    scatter_1 = i_35 < n_6;
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    (&h_4)->d_0[int(2)] = 0.0f;
                    (&h_4)->id_1[int(2)] = i_35;
                }
                uint i_36 = axis_3 + 3U;
                if(i_36 >= r_5)
                {
                    id_3 = int(1);
                }
                else
                {
                    id_3 = int(0);
                }
                uint i_37 = i_36 + uint(id_3);
                if(3U < k_13)
                {
                    scatter_1 = i_37 < n_6;
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    (&h_4)->d_0[int(3)] = 0.0f;
                    (&h_4)->id_1[int(3)] = i_37;
                }
                uint i_38 = axis_3 + 4U;
                if(i_38 >= r_5)
                {
                    id_3 = int(1);
                }
                else
                {
                    id_3 = int(0);
                }
                uint i_39 = i_38 + uint(id_3);
                if(4U < k_13)
                {
                    scatter_1 = i_39 < n_6;
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    (&h_4)->d_0[int(4)] = 0.0f;
                    (&h_4)->id_1[int(4)] = i_39;
                }
                uint i_40 = axis_3 + 5U;
                if(i_40 >= r_5)
                {
                    id_3 = int(1);
                }
                else
                {
                    id_3 = int(0);
                }
                uint i_41 = i_40 + uint(id_3);
                if(5U < k_13)
                {
                    scatter_1 = i_41 < n_6;
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    (&h_4)->d_0[int(5)] = 0.0f;
                    (&h_4)->id_1[int(5)] = i_41;
                }
                uint i_42 = axis_3 + 6U;
                if(i_42 >= r_5)
                {
                    id_3 = int(1);
                }
                else
                {
                    id_3 = int(0);
                }
                uint i_43 = i_42 + uint(id_3);
                if(6U < k_13)
                {
                    scatter_1 = i_43 < n_6;
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    (&h_4)->d_0[int(6)] = 0.0f;
                    (&h_4)->id_1[int(6)] = i_43;
                }
                uint i_44 = axis_3 + 7U;
                if(i_44 >= r_5)
                {
                    id_3 = int(1);
                }
                else
                {
                    id_3 = int(0);
                }
                uint i_45 = i_44 + uint(id_3);
                if(7U < k_13)
                {
                    scatter_1 = i_45 < n_6;
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    (&h_4)->d_0[int(7)] = 0.0f;
                    (&h_4)->id_1[int(7)] = i_45;
                }
                uint i_46 = axis_3 + 8U;
                if(i_46 >= r_5)
                {
                    id_3 = int(1);
                }
                else
                {
                    id_3 = int(0);
                }
                uint i_47 = i_46 + uint(id_3);
                if(8U < k_13)
                {
                    scatter_1 = i_47 < n_6;
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    (&h_4)->d_0[int(8)] = 0.0f;
                    (&h_4)->id_1[int(8)] = i_47;
                }
                uint i_48 = axis_3 + 9U;
                if(i_48 >= r_5)
                {
                    id_3 = int(1);
                }
                else
                {
                    id_3 = int(0);
                }
                uint i_49 = i_48 + uint(id_3);
                if(9U < k_13)
                {
                    scatter_1 = i_49 < n_6;
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    (&h_4)->d_0[int(9)] = 0.0f;
                    (&h_4)->id_1[int(9)] = i_49;
                }
                uint i_50 = axis_3 + 10U;
                if(i_50 >= r_5)
                {
                    id_3 = int(1);
                }
                else
                {
                    id_3 = int(0);
                }
                uint i_51 = i_50 + uint(id_3);
                if(10U < k_13)
                {
                    scatter_1 = i_51 < n_6;
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    (&h_4)->d_0[int(10)] = 0.0f;
                    (&h_4)->id_1[int(10)] = i_51;
                }
                uint i_52 = axis_3 + 11U;
                if(i_52 >= r_5)
                {
                    id_3 = int(1);
                }
                else
                {
                    id_3 = int(0);
                }
                uint i_53 = i_52 + uint(id_3);
                if(11U < k_13)
                {
                    scatter_1 = i_53 < n_6;
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    (&h_4)->d_0[int(11)] = 0.0f;
                    (&h_4)->id_1[int(11)] = i_53;
                }
                uint i_54 = axis_3 + 12U;
                if(i_54 >= r_5)
                {
                    id_3 = int(1);
                }
                else
                {
                    id_3 = int(0);
                }
                uint i_55 = i_54 + uint(id_3);
                if(12U < k_13)
                {
                    scatter_1 = i_55 < n_6;
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    (&h_4)->d_0[int(12)] = 0.0f;
                    (&h_4)->id_1[int(12)] = i_55;
                }
                uint i_56 = axis_3 + 13U;
                if(i_56 >= r_5)
                {
                    id_3 = int(1);
                }
                else
                {
                    id_3 = int(0);
                }
                uint i_57 = i_56 + uint(id_3);
                if(13U < k_13)
                {
                    scatter_1 = i_57 < n_6;
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    (&h_4)->d_0[int(13)] = 0.0f;
                    (&h_4)->id_1[int(13)] = i_57;
                }
                uint i_58 = axis_3 + 14U;
                if(i_58 >= r_5)
                {
                    id_3 = int(1);
                }
                else
                {
                    id_3 = int(0);
                }
                uint i_59 = i_58 + uint(id_3);
                if(14U < k_13)
                {
                    scatter_1 = i_59 < n_6;
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    (&h_4)->d_0[int(14)] = 0.0f;
                    (&h_4)->id_1[int(14)] = i_59;
                }
                uint i_60 = axis_3 + 15U;
                if(i_60 >= r_5)
                {
                    id_3 = int(1);
                }
                else
                {
                    id_3 = int(0);
                }
                uint i_61 = i_60 + uint(id_3);
                if(15U < k_13)
                {
                    scatter_1 = i_61 < n_6;
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    (&h_4)->d_0[int(15)] = 0.0f;
                    (&h_4)->id_1[int(15)] = i_61;
                }
            }
            else
            {
                if(0U < k_13)
                {
                    id_3 = *(nbrs_1 + r_5 * k_13);
                }
                else
                {
                    id_3 = int(-1);
                }
                if(id_3 >= int(0))
                {
                    (&h_4)->d_0[int(0)] = 0.0f;
                    (&h_4)->id_1[int(0)] = uint(id_3);
                }
                if(1U < k_13)
                {
                    id_3 = *(nbrs_1 + (r_5 * k_13 + 1U));
                }
                else
                {
                    id_3 = int(-1);
                }
                if(id_3 >= int(0))
                {
                    (&h_4)->d_0[int(1)] = 0.0f;
                    (&h_4)->id_1[int(1)] = uint(id_3);
                }
                if(2U < k_13)
                {
                    id_3 = *(nbrs_1 + (r_5 * k_13 + 2U));
                }
                else
                {
                    id_3 = int(-1);
                }
                if(id_3 >= int(0))
                {
                    (&h_4)->d_0[int(2)] = 0.0f;
                    (&h_4)->id_1[int(2)] = uint(id_3);
                }
                if(3U < k_13)
                {
                    id_3 = *(nbrs_1 + (r_5 * k_13 + 3U));
                }
                else
                {
                    id_3 = int(-1);
                }
                if(id_3 >= int(0))
                {
                    (&h_4)->d_0[int(3)] = 0.0f;
                    (&h_4)->id_1[int(3)] = uint(id_3);
                }
                if(4U < k_13)
                {
                    id_3 = *(nbrs_1 + (r_5 * k_13 + 4U));
                }
                else
                {
                    id_3 = int(-1);
                }
                if(id_3 >= int(0))
                {
                    (&h_4)->d_0[int(4)] = 0.0f;
                    (&h_4)->id_1[int(4)] = uint(id_3);
                }
                if(5U < k_13)
                {
                    id_3 = *(nbrs_1 + (r_5 * k_13 + 5U));
                }
                else
                {
                    id_3 = int(-1);
                }
                if(id_3 >= int(0))
                {
                    (&h_4)->d_0[int(5)] = 0.0f;
                    (&h_4)->id_1[int(5)] = uint(id_3);
                }
                if(6U < k_13)
                {
                    id_3 = *(nbrs_1 + (r_5 * k_13 + 6U));
                }
                else
                {
                    id_3 = int(-1);
                }
                if(id_3 >= int(0))
                {
                    (&h_4)->d_0[int(6)] = 0.0f;
                    (&h_4)->id_1[int(6)] = uint(id_3);
                }
                if(7U < k_13)
                {
                    id_3 = *(nbrs_1 + (r_5 * k_13 + 7U));
                }
                else
                {
                    id_3 = int(-1);
                }
                if(id_3 >= int(0))
                {
                    (&h_4)->d_0[int(7)] = 0.0f;
                    (&h_4)->id_1[int(7)] = uint(id_3);
                }
                if(8U < k_13)
                {
                    id_3 = *(nbrs_1 + (r_5 * k_13 + 8U));
                }
                else
                {
                    id_3 = int(-1);
                }
                if(id_3 >= int(0))
                {
                    (&h_4)->d_0[int(8)] = 0.0f;
                    (&h_4)->id_1[int(8)] = uint(id_3);
                }
                if(9U < k_13)
                {
                    id_3 = *(nbrs_1 + (r_5 * k_13 + 9U));
                }
                else
                {
                    id_3 = int(-1);
                }
                if(id_3 >= int(0))
                {
                    (&h_4)->d_0[int(9)] = 0.0f;
                    (&h_4)->id_1[int(9)] = uint(id_3);
                }
                if(10U < k_13)
                {
                    id_3 = *(nbrs_1 + (r_5 * k_13 + 10U));
                }
                else
                {
                    id_3 = int(-1);
                }
                if(id_3 >= int(0))
                {
                    (&h_4)->d_0[int(10)] = 0.0f;
                    (&h_4)->id_1[int(10)] = uint(id_3);
                }
                if(11U < k_13)
                {
                    id_3 = *(nbrs_1 + (r_5 * k_13 + 11U));
                }
                else
                {
                    id_3 = int(-1);
                }
                if(id_3 >= int(0))
                {
                    (&h_4)->d_0[int(11)] = 0.0f;
                    (&h_4)->id_1[int(11)] = uint(id_3);
                }
                if(12U < k_13)
                {
                    id_3 = *(nbrs_1 + (r_5 * k_13 + 12U));
                }
                else
                {
                    id_3 = int(-1);
                }
                if(id_3 >= int(0))
                {
                    (&h_4)->d_0[int(12)] = 0.0f;
                    (&h_4)->id_1[int(12)] = uint(id_3);
                }
                if(13U < k_13)
                {
                    id_3 = *(nbrs_1 + (r_5 * k_13 + 13U));
                }
                else
                {
                    id_3 = int(-1);
                }
                if(id_3 >= int(0))
                {
                    (&h_4)->d_0[int(13)] = 0.0f;
                    (&h_4)->id_1[int(13)] = uint(id_3);
                }
                if(14U < k_13)
                {
                    id_3 = *(nbrs_1 + (r_5 * k_13 + 14U));
                }
                else
                {
                    id_3 = int(-1);
                }
                if(id_3 >= int(0))
                {
                    (&h_4)->d_0[int(14)] = 0.0f;
                    (&h_4)->id_1[int(14)] = uint(id_3);
                }
                if(15U < k_13)
                {
                    id_3 = *(nbrs_1 + (r_5 * k_13 + 15U));
                }
                else
                {
                    id_3 = int(-1);
                }
                if(id_3 >= int(0))
                {
                    (&h_4)->d_0[int(15)] = 0.0f;
                    (&h_4)->id_1[int(15)] = uint(id_3);
                }
            }
        }
    }
    PcaKnn_1 _S321 = h_4;
    uint _S322 = PcaKnn_count_1(&_S321, k_13);
    if(store_list_1 != 0U)
    {
        if(0U < k_13)
        {
            int * _S323 = nbrs_1 + r_5 * k_13;
            if(((&h_4)->d_0[int(0)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_1)
                {
                    id_3 = *(perm_2 + (&h_4)->id_1[int(0)]);
                }
                else
                {
                    id_3 = int((&h_4)->id_1[int(0)]);
                }
            }
            else
            {
                id_3 = int(-1);
            }
            *_S323 = id_3;
        }
        if(1U < k_13)
        {
            int * _S324 = nbrs_1 + (r_5 * k_13 + 1U);
            if(((&h_4)->d_0[int(1)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_1)
                {
                    id_3 = *(perm_2 + (&h_4)->id_1[int(1)]);
                }
                else
                {
                    id_3 = int((&h_4)->id_1[int(1)]);
                }
            }
            else
            {
                id_3 = int(-1);
            }
            *_S324 = id_3;
        }
        if(2U < k_13)
        {
            int * _S325 = nbrs_1 + (r_5 * k_13 + 2U);
            if(((&h_4)->d_0[int(2)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_1)
                {
                    id_3 = *(perm_2 + (&h_4)->id_1[int(2)]);
                }
                else
                {
                    id_3 = int((&h_4)->id_1[int(2)]);
                }
            }
            else
            {
                id_3 = int(-1);
            }
            *_S325 = id_3;
        }
        if(3U < k_13)
        {
            int * _S326 = nbrs_1 + (r_5 * k_13 + 3U);
            if(((&h_4)->d_0[int(3)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_1)
                {
                    id_3 = *(perm_2 + (&h_4)->id_1[int(3)]);
                }
                else
                {
                    id_3 = int((&h_4)->id_1[int(3)]);
                }
            }
            else
            {
                id_3 = int(-1);
            }
            *_S326 = id_3;
        }
        if(4U < k_13)
        {
            int * _S327 = nbrs_1 + (r_5 * k_13 + 4U);
            if(((&h_4)->d_0[int(4)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_1)
                {
                    id_3 = *(perm_2 + (&h_4)->id_1[int(4)]);
                }
                else
                {
                    id_3 = int((&h_4)->id_1[int(4)]);
                }
            }
            else
            {
                id_3 = int(-1);
            }
            *_S327 = id_3;
        }
        if(5U < k_13)
        {
            int * _S328 = nbrs_1 + (r_5 * k_13 + 5U);
            if(((&h_4)->d_0[int(5)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_1)
                {
                    id_3 = *(perm_2 + (&h_4)->id_1[int(5)]);
                }
                else
                {
                    id_3 = int((&h_4)->id_1[int(5)]);
                }
            }
            else
            {
                id_3 = int(-1);
            }
            *_S328 = id_3;
        }
        if(6U < k_13)
        {
            int * _S329 = nbrs_1 + (r_5 * k_13 + 6U);
            if(((&h_4)->d_0[int(6)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_1)
                {
                    id_3 = *(perm_2 + (&h_4)->id_1[int(6)]);
                }
                else
                {
                    id_3 = int((&h_4)->id_1[int(6)]);
                }
            }
            else
            {
                id_3 = int(-1);
            }
            *_S329 = id_3;
        }
        if(7U < k_13)
        {
            int * _S330 = nbrs_1 + (r_5 * k_13 + 7U);
            if(((&h_4)->d_0[int(7)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_1)
                {
                    id_3 = *(perm_2 + (&h_4)->id_1[int(7)]);
                }
                else
                {
                    id_3 = int((&h_4)->id_1[int(7)]);
                }
            }
            else
            {
                id_3 = int(-1);
            }
            *_S330 = id_3;
        }
        if(8U < k_13)
        {
            int * _S331 = nbrs_1 + (r_5 * k_13 + 8U);
            if(((&h_4)->d_0[int(8)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_1)
                {
                    id_3 = *(perm_2 + (&h_4)->id_1[int(8)]);
                }
                else
                {
                    id_3 = int((&h_4)->id_1[int(8)]);
                }
            }
            else
            {
                id_3 = int(-1);
            }
            *_S331 = id_3;
        }
        if(9U < k_13)
        {
            int * _S332 = nbrs_1 + (r_5 * k_13 + 9U);
            if(((&h_4)->d_0[int(9)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_1)
                {
                    id_3 = *(perm_2 + (&h_4)->id_1[int(9)]);
                }
                else
                {
                    id_3 = int((&h_4)->id_1[int(9)]);
                }
            }
            else
            {
                id_3 = int(-1);
            }
            *_S332 = id_3;
        }
        if(10U < k_13)
        {
            int * _S333 = nbrs_1 + (r_5 * k_13 + 10U);
            if(((&h_4)->d_0[int(10)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_1)
                {
                    id_3 = *(perm_2 + (&h_4)->id_1[int(10)]);
                }
                else
                {
                    id_3 = int((&h_4)->id_1[int(10)]);
                }
            }
            else
            {
                id_3 = int(-1);
            }
            *_S333 = id_3;
        }
        if(11U < k_13)
        {
            int * _S334 = nbrs_1 + (r_5 * k_13 + 11U);
            if(((&h_4)->d_0[int(11)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_1)
                {
                    id_3 = *(perm_2 + (&h_4)->id_1[int(11)]);
                }
                else
                {
                    id_3 = int((&h_4)->id_1[int(11)]);
                }
            }
            else
            {
                id_3 = int(-1);
            }
            *_S334 = id_3;
        }
        if(12U < k_13)
        {
            int * _S335 = nbrs_1 + (r_5 * k_13 + 12U);
            if(((&h_4)->d_0[int(12)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_1)
                {
                    id_3 = *(perm_2 + (&h_4)->id_1[int(12)]);
                }
                else
                {
                    id_3 = int((&h_4)->id_1[int(12)]);
                }
            }
            else
            {
                id_3 = int(-1);
            }
            *_S335 = id_3;
        }
        if(13U < k_13)
        {
            int * _S336 = nbrs_1 + (r_5 * k_13 + 13U);
            if(((&h_4)->d_0[int(13)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_1)
                {
                    id_3 = *(perm_2 + (&h_4)->id_1[int(13)]);
                }
                else
                {
                    id_3 = int((&h_4)->id_1[int(13)]);
                }
            }
            else
            {
                id_3 = int(-1);
            }
            *_S336 = id_3;
        }
        if(14U < k_13)
        {
            int * _S337 = nbrs_1 + (r_5 * k_13 + 14U);
            if(((&h_4)->d_0[int(14)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_1)
                {
                    id_3 = *(perm_2 + (&h_4)->id_1[int(14)]);
                }
                else
                {
                    id_3 = int((&h_4)->id_1[int(14)]);
                }
            }
            else
            {
                id_3 = int(-1);
            }
            *_S337 = id_3;
        }
        if(15U < k_13)
        {
            int * _S338 = nbrs_1 + (r_5 * k_13 + 15U);
            if(((&h_4)->d_0[int(15)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_1)
                {
                    id_3 = *(perm_2 + (&h_4)->id_1[int(15)]);
                }
                else
                {
                    id_3 = int((&h_4)->id_1[int(15)]);
                }
            }
            else
            {
                id_3 = int(-1);
            }
            *_S338 = id_3;
        }
    }
    if(_S322 < 2U)
    {
        return make_float3 (0.0f);
    }
    float3  so_8;
    float3  sd_8;
    float3  s1_1;
    float3  _S339 = make_float3 (0.0f);
    for(;;)
    {
        for(;;)
        {
            for(;;)
            {
                bool _S340 = 0U >= k_13;
                _S302 = _S340;
                if(_S340)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(0)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    s1_1 = _S339;
                    sd_8 = _S339;
                    so_8 = _S339;
                    break;
                }
                if(by_rank_1)
                {
                    s1_1 = float3 {(*(sorted_10 + (&h_4)->id_1[int(0)])).x, (*(sorted_10 + (&h_4)->id_1[int(0)])).y, (*(sorted_10 + (&h_4)->id_1[int(0)])).z};
                }
                else
                {
                    float3  _S341 = pca_load3_0(means_2, (&h_4)->id_1[int(0)]);
                    s1_1 = _S341;
                }
                float3  d_11 = s1_1 - q_8;
                float3  _S342 = d_11 * d_11;
                float _S343 = d_11.x;
                float _S344 = d_11.y;
                float _S345 = d_11.z;
                float3  _S346 = make_float3 (_S343 * _S344, _S343 * _S345, _S344 * _S345);
                s1_1 = d_11;
                sd_8 = _S342;
                so_8 = _S346;
                break;
            }
            break;
        }
        float3  _S347;
        for(;;)
        {
            for(;;)
            {
                bool _S348 = 1U >= k_13;
                _S303 = _S348;
                if(_S348)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(1)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    _S347 = float3 {(*(sorted_10 + (&h_4)->id_1[int(1)])).x, (*(sorted_10 + (&h_4)->id_1[int(1)])).y, (*(sorted_10 + (&h_4)->id_1[int(1)])).z};
                }
                else
                {
                    float3  _S349 = pca_load3_0(means_2, (&h_4)->id_1[int(1)]);
                    _S347 = _S349;
                }
                float3  d_12 = _S347 - q_8;
                float3  sd_9 = sd_8 + d_12 * d_12;
                float _S350 = d_12.x;
                float _S351 = d_12.y;
                float _S352 = d_12.z;
                float3  so_9 = so_8 + make_float3 (_S350 * _S351, _S350 * _S352, _S351 * _S352);
                s1_1 = s1_1 + d_12;
                sd_8 = sd_9;
                so_8 = so_9;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S353 = 2U >= k_13;
                _S304 = _S353;
                if(_S353)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(2)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    _S347 = float3 {(*(sorted_10 + (&h_4)->id_1[int(2)])).x, (*(sorted_10 + (&h_4)->id_1[int(2)])).y, (*(sorted_10 + (&h_4)->id_1[int(2)])).z};
                }
                else
                {
                    float3  _S354 = pca_load3_0(means_2, (&h_4)->id_1[int(2)]);
                    _S347 = _S354;
                }
                float3  d_13 = _S347 - q_8;
                float3  sd_10 = sd_8 + d_13 * d_13;
                float _S355 = d_13.x;
                float _S356 = d_13.y;
                float _S357 = d_13.z;
                float3  so_10 = so_8 + make_float3 (_S355 * _S356, _S355 * _S357, _S356 * _S357);
                s1_1 = s1_1 + d_13;
                sd_8 = sd_10;
                so_8 = so_10;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S358 = 3U >= k_13;
                _S305 = _S358;
                if(_S358)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(3)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    _S347 = float3 {(*(sorted_10 + (&h_4)->id_1[int(3)])).x, (*(sorted_10 + (&h_4)->id_1[int(3)])).y, (*(sorted_10 + (&h_4)->id_1[int(3)])).z};
                }
                else
                {
                    float3  _S359 = pca_load3_0(means_2, (&h_4)->id_1[int(3)]);
                    _S347 = _S359;
                }
                float3  d_14 = _S347 - q_8;
                float3  sd_11 = sd_8 + d_14 * d_14;
                float _S360 = d_14.x;
                float _S361 = d_14.y;
                float _S362 = d_14.z;
                float3  so_11 = so_8 + make_float3 (_S360 * _S361, _S360 * _S362, _S361 * _S362);
                s1_1 = s1_1 + d_14;
                sd_8 = sd_11;
                so_8 = so_11;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S363 = 4U >= k_13;
                _S306 = _S363;
                if(_S363)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(4)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    _S347 = float3 {(*(sorted_10 + (&h_4)->id_1[int(4)])).x, (*(sorted_10 + (&h_4)->id_1[int(4)])).y, (*(sorted_10 + (&h_4)->id_1[int(4)])).z};
                }
                else
                {
                    float3  _S364 = pca_load3_0(means_2, (&h_4)->id_1[int(4)]);
                    _S347 = _S364;
                }
                float3  d_15 = _S347 - q_8;
                float3  sd_12 = sd_8 + d_15 * d_15;
                float _S365 = d_15.x;
                float _S366 = d_15.y;
                float _S367 = d_15.z;
                float3  so_12 = so_8 + make_float3 (_S365 * _S366, _S365 * _S367, _S366 * _S367);
                s1_1 = s1_1 + d_15;
                sd_8 = sd_12;
                so_8 = so_12;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S368 = 5U >= k_13;
                _S307 = _S368;
                if(_S368)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(5)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    _S347 = float3 {(*(sorted_10 + (&h_4)->id_1[int(5)])).x, (*(sorted_10 + (&h_4)->id_1[int(5)])).y, (*(sorted_10 + (&h_4)->id_1[int(5)])).z};
                }
                else
                {
                    float3  _S369 = pca_load3_0(means_2, (&h_4)->id_1[int(5)]);
                    _S347 = _S369;
                }
                float3  d_16 = _S347 - q_8;
                float3  sd_13 = sd_8 + d_16 * d_16;
                float _S370 = d_16.x;
                float _S371 = d_16.y;
                float _S372 = d_16.z;
                float3  so_13 = so_8 + make_float3 (_S370 * _S371, _S370 * _S372, _S371 * _S372);
                s1_1 = s1_1 + d_16;
                sd_8 = sd_13;
                so_8 = so_13;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S373 = 6U >= k_13;
                _S308 = _S373;
                if(_S373)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(6)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    _S347 = float3 {(*(sorted_10 + (&h_4)->id_1[int(6)])).x, (*(sorted_10 + (&h_4)->id_1[int(6)])).y, (*(sorted_10 + (&h_4)->id_1[int(6)])).z};
                }
                else
                {
                    float3  _S374 = pca_load3_0(means_2, (&h_4)->id_1[int(6)]);
                    _S347 = _S374;
                }
                float3  d_17 = _S347 - q_8;
                float3  sd_14 = sd_8 + d_17 * d_17;
                float _S375 = d_17.x;
                float _S376 = d_17.y;
                float _S377 = d_17.z;
                float3  so_14 = so_8 + make_float3 (_S375 * _S376, _S375 * _S377, _S376 * _S377);
                s1_1 = s1_1 + d_17;
                sd_8 = sd_14;
                so_8 = so_14;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S378 = 7U >= k_13;
                _S309 = _S378;
                if(_S378)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(7)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    _S347 = float3 {(*(sorted_10 + (&h_4)->id_1[int(7)])).x, (*(sorted_10 + (&h_4)->id_1[int(7)])).y, (*(sorted_10 + (&h_4)->id_1[int(7)])).z};
                }
                else
                {
                    float3  _S379 = pca_load3_0(means_2, (&h_4)->id_1[int(7)]);
                    _S347 = _S379;
                }
                float3  d_18 = _S347 - q_8;
                float3  sd_15 = sd_8 + d_18 * d_18;
                float _S380 = d_18.x;
                float _S381 = d_18.y;
                float _S382 = d_18.z;
                float3  so_15 = so_8 + make_float3 (_S380 * _S381, _S380 * _S382, _S381 * _S382);
                s1_1 = s1_1 + d_18;
                sd_8 = sd_15;
                so_8 = so_15;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S383 = 8U >= k_13;
                _S310 = _S383;
                if(_S383)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(8)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    _S347 = float3 {(*(sorted_10 + (&h_4)->id_1[int(8)])).x, (*(sorted_10 + (&h_4)->id_1[int(8)])).y, (*(sorted_10 + (&h_4)->id_1[int(8)])).z};
                }
                else
                {
                    float3  _S384 = pca_load3_0(means_2, (&h_4)->id_1[int(8)]);
                    _S347 = _S384;
                }
                float3  d_19 = _S347 - q_8;
                float3  sd_16 = sd_8 + d_19 * d_19;
                float _S385 = d_19.x;
                float _S386 = d_19.y;
                float _S387 = d_19.z;
                float3  so_16 = so_8 + make_float3 (_S385 * _S386, _S385 * _S387, _S386 * _S387);
                s1_1 = s1_1 + d_19;
                sd_8 = sd_16;
                so_8 = so_16;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S388 = 9U >= k_13;
                _S311 = _S388;
                if(_S388)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(9)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    _S347 = float3 {(*(sorted_10 + (&h_4)->id_1[int(9)])).x, (*(sorted_10 + (&h_4)->id_1[int(9)])).y, (*(sorted_10 + (&h_4)->id_1[int(9)])).z};
                }
                else
                {
                    float3  _S389 = pca_load3_0(means_2, (&h_4)->id_1[int(9)]);
                    _S347 = _S389;
                }
                float3  d_20 = _S347 - q_8;
                float3  sd_17 = sd_8 + d_20 * d_20;
                float _S390 = d_20.x;
                float _S391 = d_20.y;
                float _S392 = d_20.z;
                float3  so_17 = so_8 + make_float3 (_S390 * _S391, _S390 * _S392, _S391 * _S392);
                s1_1 = s1_1 + d_20;
                sd_8 = sd_17;
                so_8 = so_17;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S393 = 10U >= k_13;
                _S312 = _S393;
                if(_S393)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(10)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    _S347 = float3 {(*(sorted_10 + (&h_4)->id_1[int(10)])).x, (*(sorted_10 + (&h_4)->id_1[int(10)])).y, (*(sorted_10 + (&h_4)->id_1[int(10)])).z};
                }
                else
                {
                    float3  _S394 = pca_load3_0(means_2, (&h_4)->id_1[int(10)]);
                    _S347 = _S394;
                }
                float3  d_21 = _S347 - q_8;
                float3  sd_18 = sd_8 + d_21 * d_21;
                float _S395 = d_21.x;
                float _S396 = d_21.y;
                float _S397 = d_21.z;
                float3  so_18 = so_8 + make_float3 (_S395 * _S396, _S395 * _S397, _S396 * _S397);
                s1_1 = s1_1 + d_21;
                sd_8 = sd_18;
                so_8 = so_18;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S398 = 11U >= k_13;
                _S313 = _S398;
                if(_S398)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(11)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    _S347 = float3 {(*(sorted_10 + (&h_4)->id_1[int(11)])).x, (*(sorted_10 + (&h_4)->id_1[int(11)])).y, (*(sorted_10 + (&h_4)->id_1[int(11)])).z};
                }
                else
                {
                    float3  _S399 = pca_load3_0(means_2, (&h_4)->id_1[int(11)]);
                    _S347 = _S399;
                }
                float3  d_22 = _S347 - q_8;
                float3  sd_19 = sd_8 + d_22 * d_22;
                float _S400 = d_22.x;
                float _S401 = d_22.y;
                float _S402 = d_22.z;
                float3  so_19 = so_8 + make_float3 (_S400 * _S401, _S400 * _S402, _S401 * _S402);
                s1_1 = s1_1 + d_22;
                sd_8 = sd_19;
                so_8 = so_19;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S403 = 12U >= k_13;
                _S314 = _S403;
                if(_S403)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(12)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    _S347 = float3 {(*(sorted_10 + (&h_4)->id_1[int(12)])).x, (*(sorted_10 + (&h_4)->id_1[int(12)])).y, (*(sorted_10 + (&h_4)->id_1[int(12)])).z};
                }
                else
                {
                    float3  _S404 = pca_load3_0(means_2, (&h_4)->id_1[int(12)]);
                    _S347 = _S404;
                }
                float3  d_23 = _S347 - q_8;
                float3  sd_20 = sd_8 + d_23 * d_23;
                float _S405 = d_23.x;
                float _S406 = d_23.y;
                float _S407 = d_23.z;
                float3  so_20 = so_8 + make_float3 (_S405 * _S406, _S405 * _S407, _S406 * _S407);
                s1_1 = s1_1 + d_23;
                sd_8 = sd_20;
                so_8 = so_20;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S408 = 13U >= k_13;
                _S315 = _S408;
                if(_S408)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(13)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    _S347 = float3 {(*(sorted_10 + (&h_4)->id_1[int(13)])).x, (*(sorted_10 + (&h_4)->id_1[int(13)])).y, (*(sorted_10 + (&h_4)->id_1[int(13)])).z};
                }
                else
                {
                    float3  _S409 = pca_load3_0(means_2, (&h_4)->id_1[int(13)]);
                    _S347 = _S409;
                }
                float3  d_24 = _S347 - q_8;
                float3  sd_21 = sd_8 + d_24 * d_24;
                float _S410 = d_24.x;
                float _S411 = d_24.y;
                float _S412 = d_24.z;
                float3  so_21 = so_8 + make_float3 (_S410 * _S411, _S410 * _S412, _S411 * _S412);
                s1_1 = s1_1 + d_24;
                sd_8 = sd_21;
                so_8 = so_21;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S413 = 14U >= k_13;
                _S316 = _S413;
                if(_S413)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(14)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    _S347 = float3 {(*(sorted_10 + (&h_4)->id_1[int(14)])).x, (*(sorted_10 + (&h_4)->id_1[int(14)])).y, (*(sorted_10 + (&h_4)->id_1[int(14)])).z};
                }
                else
                {
                    float3  _S414 = pca_load3_0(means_2, (&h_4)->id_1[int(14)]);
                    _S347 = _S414;
                }
                float3  d_25 = _S347 - q_8;
                float3  sd_22 = sd_8 + d_25 * d_25;
                float _S415 = d_25.x;
                float _S416 = d_25.y;
                float _S417 = d_25.z;
                float3  so_22 = so_8 + make_float3 (_S415 * _S416, _S415 * _S417, _S416 * _S417);
                s1_1 = s1_1 + d_25;
                sd_8 = sd_22;
                so_8 = so_22;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S418 = 15U >= k_13;
                _S317 = _S418;
                if(_S418)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(15)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    _S347 = float3 {(*(sorted_10 + (&h_4)->id_1[int(15)])).x, (*(sorted_10 + (&h_4)->id_1[int(15)])).y, (*(sorted_10 + (&h_4)->id_1[int(15)])).z};
                }
                else
                {
                    float3  _S419 = pca_load3_0(means_2, (&h_4)->id_1[int(15)]);
                    _S347 = _S419;
                }
                float3  d_26 = _S347 - q_8;
                float3  sd_23 = sd_8 + d_26 * d_26;
                float _S420 = d_26.x;
                float _S421 = d_26.y;
                float _S422 = d_26.z;
                float3  so_23 = so_8 + make_float3 (_S420 * _S421, _S420 * _S422, _S421 * _S422);
                s1_1 = s1_1 + d_26;
                sd_8 = sd_23;
                so_8 = so_23;
                break;
            }
            break;
        }
        break;
    }
    float inv_k_1 = 1.0f / float(_S322 + 1U);
    float3  mean_1 = s1_1 * make_float3 (inv_k_1);
    float _S423 = mean_1.x;
    float _S424 = mean_1.y;
    float _S425 = mean_1.z;
    float3  ev_2;
    float3  e0_2;
    float3  e1_2;
    float3  e2_2;
    pca_sym3_eigen_0(sd_8 * make_float3 (inv_k_1) - mean_1 * mean_1, so_8 * make_float3 (inv_k_1) - make_float3 (_S423 * _S424, _S423 * _S425, _S424 * _S425), &ev_2, &e0_2, &e1_2, &e2_2);
    if(!all_0(isfinite_0(ev_2)))
    {
        scatter_1 = true;
    }
    else
    {
        scatter_1 = !all_0(isfinite_0(e0_2));
    }
    if(scatter_1)
    {
        return _S339;
    }
    float _S426 = 2.0f * inv_k_1;
    Matrix<float, 3, 3>  _S427 = makeMatrix<float, 3, 3> (_S426 * w_plane_1) * makeMatrix<float, 3, 3> (make_float3 (e0_2.x) * e0_2, make_float3 (e0_2.y) * e0_2, make_float3 (e0_2.z) * e0_2) + makeMatrix<float, 3, 3> (_S426 * w_tangent_1) * (makeMatrix<float, 3, 3> (make_float3 (e2_2.x) * e2_2, make_float3 (e2_2.y) * e2_2, make_float3 (e2_2.z) * e2_2) - makeMatrix<float, 3, 3> (make_float3 (e1_2.x) * e1_2, make_float3 (e1_2.y) * e1_2, make_float3 (e1_2.z) * e1_2));
    float3  v_12 = mul_0(_S427, - mean_1);
    if(w_plane_1 != 0.0f)
    {
        scatter_1 = true;
    }
    else
    {
        scatter_1 = w_tangent_1 != 0.0f;
    }
    bool _S428;
    if(scatter_1)
    {
        if(gate_means_1 == 0U)
        {
            _S428 = true;
        }
        else
        {
            _S428 = (*(radii_1 + self_4)) > 0.0f;
        }
    }
    else
    {
        _S428 = false;
    }
    if(_S428)
    {
        float * _S429 = v_means_1 + 3U * self_4;
        float _S430 = atomicAdd(_S429 + int(0), v_12.x);
        float _S431 = atomicAdd(_S429 + int(1) + int(0), v_12.y);
        float _S432 = atomicAdd(_S429 + int(2) + int(0), v_12.z);
    }
    for(;;)
    {
        for(;;)
        {
            for(;;)
            {
                bool _S433 = !scatter_1;
                _S318 = _S433;
                if(_S433)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = _S302;
                }
                if(scatter_1)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(0)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    axis_3 = uint(*(perm_2 + (&h_4)->id_1[int(0)]));
                }
                else
                {
                    axis_3 = (&h_4)->id_1[int(0)];
                }
                if(gate_means_1 != 0U)
                {
                    scatter_1 = !((*(radii_1 + axis_3)) > 0.0f);
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    s1_1 = float3 {(*(sorted_10 + (&h_4)->id_1[int(0)])).x, (*(sorted_10 + (&h_4)->id_1[int(0)])).y, (*(sorted_10 + (&h_4)->id_1[int(0)])).z};
                }
                else
                {
                    float3  _S434 = pca_load3_0(means_2, (&h_4)->id_1[int(0)]);
                    s1_1 = _S434;
                }
                float3  v_13 = mul_0(_S427, s1_1 - q_8 - mean_1);
                float * _S435 = v_means_1 + 3U * axis_3;
                float _S436 = atomicAdd(_S435 + int(0), v_13.x);
                float _S437 = atomicAdd(_S435 + int(1) + int(0), v_13.y);
                float _S438 = atomicAdd(_S435 + int(2) + int(0), v_13.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S318)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = _S303;
                }
                if(scatter_1)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(1)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    axis_3 = uint(*(perm_2 + (&h_4)->id_1[int(1)]));
                }
                else
                {
                    axis_3 = (&h_4)->id_1[int(1)];
                }
                if(gate_means_1 != 0U)
                {
                    scatter_1 = !((*(radii_1 + axis_3)) > 0.0f);
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    s1_1 = float3 {(*(sorted_10 + (&h_4)->id_1[int(1)])).x, (*(sorted_10 + (&h_4)->id_1[int(1)])).y, (*(sorted_10 + (&h_4)->id_1[int(1)])).z};
                }
                else
                {
                    float3  _S439 = pca_load3_0(means_2, (&h_4)->id_1[int(1)]);
                    s1_1 = _S439;
                }
                float3  v_14 = mul_0(_S427, s1_1 - q_8 - mean_1);
                float * _S440 = v_means_1 + 3U * axis_3;
                float _S441 = atomicAdd(_S440 + int(0), v_14.x);
                float _S442 = atomicAdd(_S440 + int(1) + int(0), v_14.y);
                float _S443 = atomicAdd(_S440 + int(2) + int(0), v_14.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S318)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = _S304;
                }
                if(scatter_1)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(2)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    axis_3 = uint(*(perm_2 + (&h_4)->id_1[int(2)]));
                }
                else
                {
                    axis_3 = (&h_4)->id_1[int(2)];
                }
                if(gate_means_1 != 0U)
                {
                    scatter_1 = !((*(radii_1 + axis_3)) > 0.0f);
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    s1_1 = float3 {(*(sorted_10 + (&h_4)->id_1[int(2)])).x, (*(sorted_10 + (&h_4)->id_1[int(2)])).y, (*(sorted_10 + (&h_4)->id_1[int(2)])).z};
                }
                else
                {
                    float3  _S444 = pca_load3_0(means_2, (&h_4)->id_1[int(2)]);
                    s1_1 = _S444;
                }
                float3  v_15 = mul_0(_S427, s1_1 - q_8 - mean_1);
                float * _S445 = v_means_1 + 3U * axis_3;
                float _S446 = atomicAdd(_S445 + int(0), v_15.x);
                float _S447 = atomicAdd(_S445 + int(1) + int(0), v_15.y);
                float _S448 = atomicAdd(_S445 + int(2) + int(0), v_15.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S318)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = _S305;
                }
                if(scatter_1)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(3)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    axis_3 = uint(*(perm_2 + (&h_4)->id_1[int(3)]));
                }
                else
                {
                    axis_3 = (&h_4)->id_1[int(3)];
                }
                if(gate_means_1 != 0U)
                {
                    scatter_1 = !((*(radii_1 + axis_3)) > 0.0f);
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    s1_1 = float3 {(*(sorted_10 + (&h_4)->id_1[int(3)])).x, (*(sorted_10 + (&h_4)->id_1[int(3)])).y, (*(sorted_10 + (&h_4)->id_1[int(3)])).z};
                }
                else
                {
                    float3  _S449 = pca_load3_0(means_2, (&h_4)->id_1[int(3)]);
                    s1_1 = _S449;
                }
                float3  v_16 = mul_0(_S427, s1_1 - q_8 - mean_1);
                float * _S450 = v_means_1 + 3U * axis_3;
                float _S451 = atomicAdd(_S450 + int(0), v_16.x);
                float _S452 = atomicAdd(_S450 + int(1) + int(0), v_16.y);
                float _S453 = atomicAdd(_S450 + int(2) + int(0), v_16.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S318)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = _S306;
                }
                if(scatter_1)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(4)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    axis_3 = uint(*(perm_2 + (&h_4)->id_1[int(4)]));
                }
                else
                {
                    axis_3 = (&h_4)->id_1[int(4)];
                }
                if(gate_means_1 != 0U)
                {
                    scatter_1 = !((*(radii_1 + axis_3)) > 0.0f);
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    s1_1 = float3 {(*(sorted_10 + (&h_4)->id_1[int(4)])).x, (*(sorted_10 + (&h_4)->id_1[int(4)])).y, (*(sorted_10 + (&h_4)->id_1[int(4)])).z};
                }
                else
                {
                    float3  _S454 = pca_load3_0(means_2, (&h_4)->id_1[int(4)]);
                    s1_1 = _S454;
                }
                float3  v_17 = mul_0(_S427, s1_1 - q_8 - mean_1);
                float * _S455 = v_means_1 + 3U * axis_3;
                float _S456 = atomicAdd(_S455 + int(0), v_17.x);
                float _S457 = atomicAdd(_S455 + int(1) + int(0), v_17.y);
                float _S458 = atomicAdd(_S455 + int(2) + int(0), v_17.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S318)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = _S307;
                }
                if(scatter_1)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(5)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    axis_3 = uint(*(perm_2 + (&h_4)->id_1[int(5)]));
                }
                else
                {
                    axis_3 = (&h_4)->id_1[int(5)];
                }
                if(gate_means_1 != 0U)
                {
                    scatter_1 = !((*(radii_1 + axis_3)) > 0.0f);
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    s1_1 = float3 {(*(sorted_10 + (&h_4)->id_1[int(5)])).x, (*(sorted_10 + (&h_4)->id_1[int(5)])).y, (*(sorted_10 + (&h_4)->id_1[int(5)])).z};
                }
                else
                {
                    float3  _S459 = pca_load3_0(means_2, (&h_4)->id_1[int(5)]);
                    s1_1 = _S459;
                }
                float3  v_18 = mul_0(_S427, s1_1 - q_8 - mean_1);
                float * _S460 = v_means_1 + 3U * axis_3;
                float _S461 = atomicAdd(_S460 + int(0), v_18.x);
                float _S462 = atomicAdd(_S460 + int(1) + int(0), v_18.y);
                float _S463 = atomicAdd(_S460 + int(2) + int(0), v_18.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S318)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = _S308;
                }
                if(scatter_1)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(6)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    axis_3 = uint(*(perm_2 + (&h_4)->id_1[int(6)]));
                }
                else
                {
                    axis_3 = (&h_4)->id_1[int(6)];
                }
                if(gate_means_1 != 0U)
                {
                    scatter_1 = !((*(radii_1 + axis_3)) > 0.0f);
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    s1_1 = float3 {(*(sorted_10 + (&h_4)->id_1[int(6)])).x, (*(sorted_10 + (&h_4)->id_1[int(6)])).y, (*(sorted_10 + (&h_4)->id_1[int(6)])).z};
                }
                else
                {
                    float3  _S464 = pca_load3_0(means_2, (&h_4)->id_1[int(6)]);
                    s1_1 = _S464;
                }
                float3  v_19 = mul_0(_S427, s1_1 - q_8 - mean_1);
                float * _S465 = v_means_1 + 3U * axis_3;
                float _S466 = atomicAdd(_S465 + int(0), v_19.x);
                float _S467 = atomicAdd(_S465 + int(1) + int(0), v_19.y);
                float _S468 = atomicAdd(_S465 + int(2) + int(0), v_19.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S318)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = _S309;
                }
                if(scatter_1)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(7)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    axis_3 = uint(*(perm_2 + (&h_4)->id_1[int(7)]));
                }
                else
                {
                    axis_3 = (&h_4)->id_1[int(7)];
                }
                if(gate_means_1 != 0U)
                {
                    scatter_1 = !((*(radii_1 + axis_3)) > 0.0f);
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    s1_1 = float3 {(*(sorted_10 + (&h_4)->id_1[int(7)])).x, (*(sorted_10 + (&h_4)->id_1[int(7)])).y, (*(sorted_10 + (&h_4)->id_1[int(7)])).z};
                }
                else
                {
                    float3  _S469 = pca_load3_0(means_2, (&h_4)->id_1[int(7)]);
                    s1_1 = _S469;
                }
                float3  v_20 = mul_0(_S427, s1_1 - q_8 - mean_1);
                float * _S470 = v_means_1 + 3U * axis_3;
                float _S471 = atomicAdd(_S470 + int(0), v_20.x);
                float _S472 = atomicAdd(_S470 + int(1) + int(0), v_20.y);
                float _S473 = atomicAdd(_S470 + int(2) + int(0), v_20.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S318)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = _S310;
                }
                if(scatter_1)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(8)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    axis_3 = uint(*(perm_2 + (&h_4)->id_1[int(8)]));
                }
                else
                {
                    axis_3 = (&h_4)->id_1[int(8)];
                }
                if(gate_means_1 != 0U)
                {
                    scatter_1 = !((*(radii_1 + axis_3)) > 0.0f);
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    s1_1 = float3 {(*(sorted_10 + (&h_4)->id_1[int(8)])).x, (*(sorted_10 + (&h_4)->id_1[int(8)])).y, (*(sorted_10 + (&h_4)->id_1[int(8)])).z};
                }
                else
                {
                    float3  _S474 = pca_load3_0(means_2, (&h_4)->id_1[int(8)]);
                    s1_1 = _S474;
                }
                float3  v_21 = mul_0(_S427, s1_1 - q_8 - mean_1);
                float * _S475 = v_means_1 + 3U * axis_3;
                float _S476 = atomicAdd(_S475 + int(0), v_21.x);
                float _S477 = atomicAdd(_S475 + int(1) + int(0), v_21.y);
                float _S478 = atomicAdd(_S475 + int(2) + int(0), v_21.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S318)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = _S311;
                }
                if(scatter_1)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(9)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    axis_3 = uint(*(perm_2 + (&h_4)->id_1[int(9)]));
                }
                else
                {
                    axis_3 = (&h_4)->id_1[int(9)];
                }
                if(gate_means_1 != 0U)
                {
                    scatter_1 = !((*(radii_1 + axis_3)) > 0.0f);
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    s1_1 = float3 {(*(sorted_10 + (&h_4)->id_1[int(9)])).x, (*(sorted_10 + (&h_4)->id_1[int(9)])).y, (*(sorted_10 + (&h_4)->id_1[int(9)])).z};
                }
                else
                {
                    float3  _S479 = pca_load3_0(means_2, (&h_4)->id_1[int(9)]);
                    s1_1 = _S479;
                }
                float3  v_22 = mul_0(_S427, s1_1 - q_8 - mean_1);
                float * _S480 = v_means_1 + 3U * axis_3;
                float _S481 = atomicAdd(_S480 + int(0), v_22.x);
                float _S482 = atomicAdd(_S480 + int(1) + int(0), v_22.y);
                float _S483 = atomicAdd(_S480 + int(2) + int(0), v_22.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S318)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = _S312;
                }
                if(scatter_1)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(10)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    axis_3 = uint(*(perm_2 + (&h_4)->id_1[int(10)]));
                }
                else
                {
                    axis_3 = (&h_4)->id_1[int(10)];
                }
                if(gate_means_1 != 0U)
                {
                    scatter_1 = !((*(radii_1 + axis_3)) > 0.0f);
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    s1_1 = float3 {(*(sorted_10 + (&h_4)->id_1[int(10)])).x, (*(sorted_10 + (&h_4)->id_1[int(10)])).y, (*(sorted_10 + (&h_4)->id_1[int(10)])).z};
                }
                else
                {
                    float3  _S484 = pca_load3_0(means_2, (&h_4)->id_1[int(10)]);
                    s1_1 = _S484;
                }
                float3  v_23 = mul_0(_S427, s1_1 - q_8 - mean_1);
                float * _S485 = v_means_1 + 3U * axis_3;
                float _S486 = atomicAdd(_S485 + int(0), v_23.x);
                float _S487 = atomicAdd(_S485 + int(1) + int(0), v_23.y);
                float _S488 = atomicAdd(_S485 + int(2) + int(0), v_23.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S318)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = _S313;
                }
                if(scatter_1)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(11)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    axis_3 = uint(*(perm_2 + (&h_4)->id_1[int(11)]));
                }
                else
                {
                    axis_3 = (&h_4)->id_1[int(11)];
                }
                if(gate_means_1 != 0U)
                {
                    scatter_1 = !((*(radii_1 + axis_3)) > 0.0f);
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    s1_1 = float3 {(*(sorted_10 + (&h_4)->id_1[int(11)])).x, (*(sorted_10 + (&h_4)->id_1[int(11)])).y, (*(sorted_10 + (&h_4)->id_1[int(11)])).z};
                }
                else
                {
                    float3  _S489 = pca_load3_0(means_2, (&h_4)->id_1[int(11)]);
                    s1_1 = _S489;
                }
                float3  v_24 = mul_0(_S427, s1_1 - q_8 - mean_1);
                float * _S490 = v_means_1 + 3U * axis_3;
                float _S491 = atomicAdd(_S490 + int(0), v_24.x);
                float _S492 = atomicAdd(_S490 + int(1) + int(0), v_24.y);
                float _S493 = atomicAdd(_S490 + int(2) + int(0), v_24.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S318)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = _S314;
                }
                if(scatter_1)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(12)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    axis_3 = uint(*(perm_2 + (&h_4)->id_1[int(12)]));
                }
                else
                {
                    axis_3 = (&h_4)->id_1[int(12)];
                }
                if(gate_means_1 != 0U)
                {
                    scatter_1 = !((*(radii_1 + axis_3)) > 0.0f);
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    s1_1 = float3 {(*(sorted_10 + (&h_4)->id_1[int(12)])).x, (*(sorted_10 + (&h_4)->id_1[int(12)])).y, (*(sorted_10 + (&h_4)->id_1[int(12)])).z};
                }
                else
                {
                    float3  _S494 = pca_load3_0(means_2, (&h_4)->id_1[int(12)]);
                    s1_1 = _S494;
                }
                float3  v_25 = mul_0(_S427, s1_1 - q_8 - mean_1);
                float * _S495 = v_means_1 + 3U * axis_3;
                float _S496 = atomicAdd(_S495 + int(0), v_25.x);
                float _S497 = atomicAdd(_S495 + int(1) + int(0), v_25.y);
                float _S498 = atomicAdd(_S495 + int(2) + int(0), v_25.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S318)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = _S315;
                }
                if(scatter_1)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(13)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    axis_3 = uint(*(perm_2 + (&h_4)->id_1[int(13)]));
                }
                else
                {
                    axis_3 = (&h_4)->id_1[int(13)];
                }
                if(gate_means_1 != 0U)
                {
                    scatter_1 = !((*(radii_1 + axis_3)) > 0.0f);
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    s1_1 = float3 {(*(sorted_10 + (&h_4)->id_1[int(13)])).x, (*(sorted_10 + (&h_4)->id_1[int(13)])).y, (*(sorted_10 + (&h_4)->id_1[int(13)])).z};
                }
                else
                {
                    float3  _S499 = pca_load3_0(means_2, (&h_4)->id_1[int(13)]);
                    s1_1 = _S499;
                }
                float3  v_26 = mul_0(_S427, s1_1 - q_8 - mean_1);
                float * _S500 = v_means_1 + 3U * axis_3;
                float _S501 = atomicAdd(_S500 + int(0), v_26.x);
                float _S502 = atomicAdd(_S500 + int(1) + int(0), v_26.y);
                float _S503 = atomicAdd(_S500 + int(2) + int(0), v_26.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S318)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = _S316;
                }
                if(scatter_1)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(14)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    axis_3 = uint(*(perm_2 + (&h_4)->id_1[int(14)]));
                }
                else
                {
                    axis_3 = (&h_4)->id_1[int(14)];
                }
                if(gate_means_1 != 0U)
                {
                    scatter_1 = !((*(radii_1 + axis_3)) > 0.0f);
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    s1_1 = float3 {(*(sorted_10 + (&h_4)->id_1[int(14)])).x, (*(sorted_10 + (&h_4)->id_1[int(14)])).y, (*(sorted_10 + (&h_4)->id_1[int(14)])).z};
                }
                else
                {
                    float3  _S504 = pca_load3_0(means_2, (&h_4)->id_1[int(14)]);
                    s1_1 = _S504;
                }
                float3  v_27 = mul_0(_S427, s1_1 - q_8 - mean_1);
                float * _S505 = v_means_1 + 3U * axis_3;
                float _S506 = atomicAdd(_S505 + int(0), v_27.x);
                float _S507 = atomicAdd(_S505 + int(1) + int(0), v_27.y);
                float _S508 = atomicAdd(_S505 + int(2) + int(0), v_27.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S318)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = _S317;
                }
                if(scatter_1)
                {
                    scatter_1 = true;
                }
                else
                {
                    scatter_1 = !(((&h_4)->d_0[int(15)]) < 3.00000000549775576e+38f);
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    axis_3 = uint(*(perm_2 + (&h_4)->id_1[int(15)]));
                }
                else
                {
                    axis_3 = (&h_4)->id_1[int(15)];
                }
                if(gate_means_1 != 0U)
                {
                    scatter_1 = !((*(radii_1 + axis_3)) > 0.0f);
                }
                else
                {
                    scatter_1 = false;
                }
                if(scatter_1)
                {
                    break;
                }
                if(by_rank_1)
                {
                    s1_1 = float3 {(*(sorted_10 + (&h_4)->id_1[int(15)])).x, (*(sorted_10 + (&h_4)->id_1[int(15)])).y, (*(sorted_10 + (&h_4)->id_1[int(15)])).z};
                }
                else
                {
                    float3  _S509 = pca_load3_0(means_2, (&h_4)->id_1[int(15)]);
                    s1_1 = _S509;
                }
                float3  v_28 = mul_0(_S427, s1_1 - q_8 - mean_1);
                float * _S510 = v_means_1 + 3U * axis_3;
                float _S511 = atomicAdd(_S510 + int(0), v_28.x);
                float _S512 = atomicAdd(_S510 + int(1) + int(0), v_28.y);
                float _S513 = atomicAdd(_S510 + int(2) + int(0), v_28.z);
                break;
            }
            break;
        }
        break;
    }
    float3  ls_1 = pca_load3_0(scales_1, self_4);
    float _S514 = ls_1.x;
    float _S515 = ls_1.y;
    if(_S514 <= _S515)
    {
        if(_S514 <= (ls_1.z))
        {
            axis_3 = 0U;
        }
        else
        {
            axis_3 = 2U;
        }
    }
    else
    {
        if(_S515 <= (ls_1.z))
        {
            axis_3 = 1U;
        }
        else
        {
            axis_3 = 2U;
        }
    }
    uint _S516 = 4U * self_4;
    float4  quat_2 = make_float4 (*(quats_1 + _S516), *(quats_1 + (_S516 + 1U)), *(quats_1 + (_S516 + 2U)), *(quats_1 + (_S516 + 3U)));
    float align_1 = pca_axis_align_0(quat_2, e0_2, axis_3);
    if(w_normal_1 != 0.0f)
    {
        float4  _S517 = make_float4 (0.0f);
        DiffPair_vectorx3Cfloatx2C4x3E_0 dq_1;
        (&dq_1)->primal_0 = quat_2;
        (&dq_1)->differential_0 = _S517;
        s_bwd_pca_axis_align_0(&dq_1, e0_2, axis_3, - w_normal_1);
        uint c_7 = 0U;
        for(;;)
        {
            if(c_7 < 4U)
            {
            }
            else
            {
                break;
            }
            float * _S518 = v_quats_1 + (_S516 + c_7);
            *_S518 = *_S518 + _slang_vector_get_element(dq_1.differential_0, c_7);
            c_7 = c_7 + 1U;
        }
    }
    if(w_surfel_1 != 0.0f)
    {
        float smin_1;
        if(axis_3 == 0U)
        {
            smin_1 = _S514;
        }
        else
        {
            if(axis_3 == 1U)
            {
                smin_1 = _S515;
            }
            else
            {
                smin_1 = ls_1.z;
            }
        }
        float * _S519 = v_scales_1 + (3U * self_4 + axis_3);
        *_S519 = *_S519 + w_surfel_1 * (F32_exp((smin_1)));
    }
    return make_float3 (ev_2.x, ev_2.z - ev_2.y, 1.0f - align_1);
}

inline __device__ float3  pca_reg_owner_2(uint r_6, uint mode_2, uint n_7, uint k_14, uint window_2, float * means_3, float * quats_2, float * scales_2, int * perm_3, float4  * sorted_11, float4  * nodes_6, int4  * meta_6, int * ranges_6, int * nbrs_2, uint store_list_2, float * radii_2, uint gate_means_2, float * v_means_2, float * v_quats_2, float * v_scales_2, float w_plane_2, float w_tangent_2, float w_normal_2, float w_surfel_2)
{
    bool _S520;
    bool _S521;
    bool _S522;
    bool _S523;
    bool _S524;
    bool _S525;
    bool _S526;
    bool _S527;
    bool _S528;
    bool _S529;
    bool _S530;
    bool _S531;
    bool _S532;
    bool _S533;
    bool _S534;
    bool _S535;
    bool _S536;
    bool _S537;
    bool _S538;
    bool _S539;
    bool _S540;
    bool _S541;
    bool _S542;
    bool _S543;
    bool _S544;
    bool _S545;
    bool _S546;
    bool _S547;
    bool _S548;
    bool _S549;
    bool _S550;
    bool _S551;
    bool _S552;
    PcaKnn_2 h_5;
    PcaKnn_reset_2(&h_5);
    bool by_rank_2 = mode_2 != 3U;
    uint self_5 = uint(*(perm_3 + r_6));
    float3  q_9;
    if(by_rank_2)
    {
        q_9 = float3 {(*(sorted_11 + r_6)).x, (*(sorted_11 + r_6)).y, (*(sorted_11 + r_6)).z};
    }
    else
    {
        float3  _S553 = pca_load3_0(means_3, self_5);
        q_9 = _S553;
    }
    int id_4;
    uint axis_4;
    bool scatter_2;
    if(mode_2 == 0U)
    {
        pca_knn_tree_2(r_6, n_7, k_14, sorted_11, nodes_6, meta_6, ranges_6, &h_5);
    }
    else
    {
        if(mode_2 == 2U)
        {
            uint half_2 = (U32_max((window_2), (k_14 + 1U))) / 2U;
            if(r_6 > half_2)
            {
                axis_4 = r_6 - half_2;
            }
            else
            {
                axis_4 = 0U;
            }
            PcaKnn_scan_2(&h_5, q_9, r_6, axis_4, (U32_min((r_6 + half_2 + 1U), (n_7))), 0U, 0U, sorted_11, k_14);
        }
        else
        {
            if(mode_2 == 1U)
            {
                if(r_6 > (k_14 / 2U))
                {
                    axis_4 = r_6 - k_14 / 2U;
                }
                else
                {
                    axis_4 = 0U;
                }
                if((axis_4 + k_14 + 1U) > n_7)
                {
                    uint _S554 = k_14 + 1U;
                    if(n_7 > _S554)
                    {
                        axis_4 = n_7 - _S554;
                    }
                    else
                    {
                        axis_4 = 0U;
                    }
                }
                if(axis_4 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_62 = axis_4 + uint(id_4);
                if(0U < k_14)
                {
                    scatter_2 = i_62 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(0)] = 0.0f;
                    (&h_5)->id_1[int(0)] = i_62;
                }
                uint i_63 = axis_4 + 1U;
                if(i_63 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_64 = i_63 + uint(id_4);
                if(1U < k_14)
                {
                    scatter_2 = i_64 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(1)] = 0.0f;
                    (&h_5)->id_1[int(1)] = i_64;
                }
                uint i_65 = axis_4 + 2U;
                if(i_65 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_66 = i_65 + uint(id_4);
                if(2U < k_14)
                {
                    scatter_2 = i_66 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(2)] = 0.0f;
                    (&h_5)->id_1[int(2)] = i_66;
                }
                uint i_67 = axis_4 + 3U;
                if(i_67 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_68 = i_67 + uint(id_4);
                if(3U < k_14)
                {
                    scatter_2 = i_68 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(3)] = 0.0f;
                    (&h_5)->id_1[int(3)] = i_68;
                }
                uint i_69 = axis_4 + 4U;
                if(i_69 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_70 = i_69 + uint(id_4);
                if(4U < k_14)
                {
                    scatter_2 = i_70 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(4)] = 0.0f;
                    (&h_5)->id_1[int(4)] = i_70;
                }
                uint i_71 = axis_4 + 5U;
                if(i_71 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_72 = i_71 + uint(id_4);
                if(5U < k_14)
                {
                    scatter_2 = i_72 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(5)] = 0.0f;
                    (&h_5)->id_1[int(5)] = i_72;
                }
                uint i_73 = axis_4 + 6U;
                if(i_73 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_74 = i_73 + uint(id_4);
                if(6U < k_14)
                {
                    scatter_2 = i_74 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(6)] = 0.0f;
                    (&h_5)->id_1[int(6)] = i_74;
                }
                uint i_75 = axis_4 + 7U;
                if(i_75 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_76 = i_75 + uint(id_4);
                if(7U < k_14)
                {
                    scatter_2 = i_76 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(7)] = 0.0f;
                    (&h_5)->id_1[int(7)] = i_76;
                }
                uint i_77 = axis_4 + 8U;
                if(i_77 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_78 = i_77 + uint(id_4);
                if(8U < k_14)
                {
                    scatter_2 = i_78 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(8)] = 0.0f;
                    (&h_5)->id_1[int(8)] = i_78;
                }
                uint i_79 = axis_4 + 9U;
                if(i_79 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_80 = i_79 + uint(id_4);
                if(9U < k_14)
                {
                    scatter_2 = i_80 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(9)] = 0.0f;
                    (&h_5)->id_1[int(9)] = i_80;
                }
                uint i_81 = axis_4 + 10U;
                if(i_81 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_82 = i_81 + uint(id_4);
                if(10U < k_14)
                {
                    scatter_2 = i_82 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(10)] = 0.0f;
                    (&h_5)->id_1[int(10)] = i_82;
                }
                uint i_83 = axis_4 + 11U;
                if(i_83 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_84 = i_83 + uint(id_4);
                if(11U < k_14)
                {
                    scatter_2 = i_84 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(11)] = 0.0f;
                    (&h_5)->id_1[int(11)] = i_84;
                }
                uint i_85 = axis_4 + 12U;
                if(i_85 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_86 = i_85 + uint(id_4);
                if(12U < k_14)
                {
                    scatter_2 = i_86 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(12)] = 0.0f;
                    (&h_5)->id_1[int(12)] = i_86;
                }
                uint i_87 = axis_4 + 13U;
                if(i_87 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_88 = i_87 + uint(id_4);
                if(13U < k_14)
                {
                    scatter_2 = i_88 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(13)] = 0.0f;
                    (&h_5)->id_1[int(13)] = i_88;
                }
                uint i_89 = axis_4 + 14U;
                if(i_89 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_90 = i_89 + uint(id_4);
                if(14U < k_14)
                {
                    scatter_2 = i_90 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(14)] = 0.0f;
                    (&h_5)->id_1[int(14)] = i_90;
                }
                uint i_91 = axis_4 + 15U;
                if(i_91 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_92 = i_91 + uint(id_4);
                if(15U < k_14)
                {
                    scatter_2 = i_92 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(15)] = 0.0f;
                    (&h_5)->id_1[int(15)] = i_92;
                }
                uint i_93 = axis_4 + 16U;
                if(i_93 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_94 = i_93 + uint(id_4);
                if(16U < k_14)
                {
                    scatter_2 = i_94 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(16)] = 0.0f;
                    (&h_5)->id_1[int(16)] = i_94;
                }
                uint i_95 = axis_4 + 17U;
                if(i_95 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_96 = i_95 + uint(id_4);
                if(17U < k_14)
                {
                    scatter_2 = i_96 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(17)] = 0.0f;
                    (&h_5)->id_1[int(17)] = i_96;
                }
                uint i_97 = axis_4 + 18U;
                if(i_97 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_98 = i_97 + uint(id_4);
                if(18U < k_14)
                {
                    scatter_2 = i_98 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(18)] = 0.0f;
                    (&h_5)->id_1[int(18)] = i_98;
                }
                uint i_99 = axis_4 + 19U;
                if(i_99 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_100 = i_99 + uint(id_4);
                if(19U < k_14)
                {
                    scatter_2 = i_100 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(19)] = 0.0f;
                    (&h_5)->id_1[int(19)] = i_100;
                }
                uint i_101 = axis_4 + 20U;
                if(i_101 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_102 = i_101 + uint(id_4);
                if(20U < k_14)
                {
                    scatter_2 = i_102 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(20)] = 0.0f;
                    (&h_5)->id_1[int(20)] = i_102;
                }
                uint i_103 = axis_4 + 21U;
                if(i_103 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_104 = i_103 + uint(id_4);
                if(21U < k_14)
                {
                    scatter_2 = i_104 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(21)] = 0.0f;
                    (&h_5)->id_1[int(21)] = i_104;
                }
                uint i_105 = axis_4 + 22U;
                if(i_105 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_106 = i_105 + uint(id_4);
                if(22U < k_14)
                {
                    scatter_2 = i_106 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(22)] = 0.0f;
                    (&h_5)->id_1[int(22)] = i_106;
                }
                uint i_107 = axis_4 + 23U;
                if(i_107 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_108 = i_107 + uint(id_4);
                if(23U < k_14)
                {
                    scatter_2 = i_108 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(23)] = 0.0f;
                    (&h_5)->id_1[int(23)] = i_108;
                }
                uint i_109 = axis_4 + 24U;
                if(i_109 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_110 = i_109 + uint(id_4);
                if(24U < k_14)
                {
                    scatter_2 = i_110 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(24)] = 0.0f;
                    (&h_5)->id_1[int(24)] = i_110;
                }
                uint i_111 = axis_4 + 25U;
                if(i_111 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_112 = i_111 + uint(id_4);
                if(25U < k_14)
                {
                    scatter_2 = i_112 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(25)] = 0.0f;
                    (&h_5)->id_1[int(25)] = i_112;
                }
                uint i_113 = axis_4 + 26U;
                if(i_113 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_114 = i_113 + uint(id_4);
                if(26U < k_14)
                {
                    scatter_2 = i_114 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(26)] = 0.0f;
                    (&h_5)->id_1[int(26)] = i_114;
                }
                uint i_115 = axis_4 + 27U;
                if(i_115 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_116 = i_115 + uint(id_4);
                if(27U < k_14)
                {
                    scatter_2 = i_116 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(27)] = 0.0f;
                    (&h_5)->id_1[int(27)] = i_116;
                }
                uint i_117 = axis_4 + 28U;
                if(i_117 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_118 = i_117 + uint(id_4);
                if(28U < k_14)
                {
                    scatter_2 = i_118 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(28)] = 0.0f;
                    (&h_5)->id_1[int(28)] = i_118;
                }
                uint i_119 = axis_4 + 29U;
                if(i_119 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_120 = i_119 + uint(id_4);
                if(29U < k_14)
                {
                    scatter_2 = i_120 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(29)] = 0.0f;
                    (&h_5)->id_1[int(29)] = i_120;
                }
                uint i_121 = axis_4 + 30U;
                if(i_121 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_122 = i_121 + uint(id_4);
                if(30U < k_14)
                {
                    scatter_2 = i_122 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(30)] = 0.0f;
                    (&h_5)->id_1[int(30)] = i_122;
                }
                uint i_123 = axis_4 + 31U;
                if(i_123 >= r_6)
                {
                    id_4 = int(1);
                }
                else
                {
                    id_4 = int(0);
                }
                uint i_124 = i_123 + uint(id_4);
                if(31U < k_14)
                {
                    scatter_2 = i_124 < n_7;
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    (&h_5)->d_0[int(31)] = 0.0f;
                    (&h_5)->id_1[int(31)] = i_124;
                }
            }
            else
            {
                if(0U < k_14)
                {
                    id_4 = *(nbrs_2 + r_6 * k_14);
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(0)] = 0.0f;
                    (&h_5)->id_1[int(0)] = uint(id_4);
                }
                if(1U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 1U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(1)] = 0.0f;
                    (&h_5)->id_1[int(1)] = uint(id_4);
                }
                if(2U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 2U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(2)] = 0.0f;
                    (&h_5)->id_1[int(2)] = uint(id_4);
                }
                if(3U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 3U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(3)] = 0.0f;
                    (&h_5)->id_1[int(3)] = uint(id_4);
                }
                if(4U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 4U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(4)] = 0.0f;
                    (&h_5)->id_1[int(4)] = uint(id_4);
                }
                if(5U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 5U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(5)] = 0.0f;
                    (&h_5)->id_1[int(5)] = uint(id_4);
                }
                if(6U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 6U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(6)] = 0.0f;
                    (&h_5)->id_1[int(6)] = uint(id_4);
                }
                if(7U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 7U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(7)] = 0.0f;
                    (&h_5)->id_1[int(7)] = uint(id_4);
                }
                if(8U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 8U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(8)] = 0.0f;
                    (&h_5)->id_1[int(8)] = uint(id_4);
                }
                if(9U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 9U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(9)] = 0.0f;
                    (&h_5)->id_1[int(9)] = uint(id_4);
                }
                if(10U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 10U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(10)] = 0.0f;
                    (&h_5)->id_1[int(10)] = uint(id_4);
                }
                if(11U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 11U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(11)] = 0.0f;
                    (&h_5)->id_1[int(11)] = uint(id_4);
                }
                if(12U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 12U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(12)] = 0.0f;
                    (&h_5)->id_1[int(12)] = uint(id_4);
                }
                if(13U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 13U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(13)] = 0.0f;
                    (&h_5)->id_1[int(13)] = uint(id_4);
                }
                if(14U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 14U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(14)] = 0.0f;
                    (&h_5)->id_1[int(14)] = uint(id_4);
                }
                if(15U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 15U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(15)] = 0.0f;
                    (&h_5)->id_1[int(15)] = uint(id_4);
                }
                if(16U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 16U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(16)] = 0.0f;
                    (&h_5)->id_1[int(16)] = uint(id_4);
                }
                if(17U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 17U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(17)] = 0.0f;
                    (&h_5)->id_1[int(17)] = uint(id_4);
                }
                if(18U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 18U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(18)] = 0.0f;
                    (&h_5)->id_1[int(18)] = uint(id_4);
                }
                if(19U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 19U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(19)] = 0.0f;
                    (&h_5)->id_1[int(19)] = uint(id_4);
                }
                if(20U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 20U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(20)] = 0.0f;
                    (&h_5)->id_1[int(20)] = uint(id_4);
                }
                if(21U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 21U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(21)] = 0.0f;
                    (&h_5)->id_1[int(21)] = uint(id_4);
                }
                if(22U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 22U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(22)] = 0.0f;
                    (&h_5)->id_1[int(22)] = uint(id_4);
                }
                if(23U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 23U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(23)] = 0.0f;
                    (&h_5)->id_1[int(23)] = uint(id_4);
                }
                if(24U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 24U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(24)] = 0.0f;
                    (&h_5)->id_1[int(24)] = uint(id_4);
                }
                if(25U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 25U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(25)] = 0.0f;
                    (&h_5)->id_1[int(25)] = uint(id_4);
                }
                if(26U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 26U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(26)] = 0.0f;
                    (&h_5)->id_1[int(26)] = uint(id_4);
                }
                if(27U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 27U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(27)] = 0.0f;
                    (&h_5)->id_1[int(27)] = uint(id_4);
                }
                if(28U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 28U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(28)] = 0.0f;
                    (&h_5)->id_1[int(28)] = uint(id_4);
                }
                if(29U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 29U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(29)] = 0.0f;
                    (&h_5)->id_1[int(29)] = uint(id_4);
                }
                if(30U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 30U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(30)] = 0.0f;
                    (&h_5)->id_1[int(30)] = uint(id_4);
                }
                if(31U < k_14)
                {
                    id_4 = *(nbrs_2 + (r_6 * k_14 + 31U));
                }
                else
                {
                    id_4 = int(-1);
                }
                if(id_4 >= int(0))
                {
                    (&h_5)->d_0[int(31)] = 0.0f;
                    (&h_5)->id_1[int(31)] = uint(id_4);
                }
            }
        }
    }
    PcaKnn_2 _S555 = h_5;
    uint _S556 = PcaKnn_count_2(&_S555, k_14);
    if(store_list_2 != 0U)
    {
        if(0U < k_14)
        {
            int * _S557 = nbrs_2 + r_6 * k_14;
            if(((&h_5)->d_0[int(0)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(0)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(0)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S557 = id_4;
        }
        if(1U < k_14)
        {
            int * _S558 = nbrs_2 + (r_6 * k_14 + 1U);
            if(((&h_5)->d_0[int(1)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(1)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(1)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S558 = id_4;
        }
        if(2U < k_14)
        {
            int * _S559 = nbrs_2 + (r_6 * k_14 + 2U);
            if(((&h_5)->d_0[int(2)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(2)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(2)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S559 = id_4;
        }
        if(3U < k_14)
        {
            int * _S560 = nbrs_2 + (r_6 * k_14 + 3U);
            if(((&h_5)->d_0[int(3)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(3)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(3)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S560 = id_4;
        }
        if(4U < k_14)
        {
            int * _S561 = nbrs_2 + (r_6 * k_14 + 4U);
            if(((&h_5)->d_0[int(4)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(4)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(4)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S561 = id_4;
        }
        if(5U < k_14)
        {
            int * _S562 = nbrs_2 + (r_6 * k_14 + 5U);
            if(((&h_5)->d_0[int(5)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(5)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(5)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S562 = id_4;
        }
        if(6U < k_14)
        {
            int * _S563 = nbrs_2 + (r_6 * k_14 + 6U);
            if(((&h_5)->d_0[int(6)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(6)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(6)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S563 = id_4;
        }
        if(7U < k_14)
        {
            int * _S564 = nbrs_2 + (r_6 * k_14 + 7U);
            if(((&h_5)->d_0[int(7)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(7)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(7)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S564 = id_4;
        }
        if(8U < k_14)
        {
            int * _S565 = nbrs_2 + (r_6 * k_14 + 8U);
            if(((&h_5)->d_0[int(8)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(8)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(8)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S565 = id_4;
        }
        if(9U < k_14)
        {
            int * _S566 = nbrs_2 + (r_6 * k_14 + 9U);
            if(((&h_5)->d_0[int(9)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(9)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(9)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S566 = id_4;
        }
        if(10U < k_14)
        {
            int * _S567 = nbrs_2 + (r_6 * k_14 + 10U);
            if(((&h_5)->d_0[int(10)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(10)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(10)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S567 = id_4;
        }
        if(11U < k_14)
        {
            int * _S568 = nbrs_2 + (r_6 * k_14 + 11U);
            if(((&h_5)->d_0[int(11)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(11)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(11)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S568 = id_4;
        }
        if(12U < k_14)
        {
            int * _S569 = nbrs_2 + (r_6 * k_14 + 12U);
            if(((&h_5)->d_0[int(12)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(12)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(12)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S569 = id_4;
        }
        if(13U < k_14)
        {
            int * _S570 = nbrs_2 + (r_6 * k_14 + 13U);
            if(((&h_5)->d_0[int(13)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(13)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(13)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S570 = id_4;
        }
        if(14U < k_14)
        {
            int * _S571 = nbrs_2 + (r_6 * k_14 + 14U);
            if(((&h_5)->d_0[int(14)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(14)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(14)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S571 = id_4;
        }
        if(15U < k_14)
        {
            int * _S572 = nbrs_2 + (r_6 * k_14 + 15U);
            if(((&h_5)->d_0[int(15)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(15)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(15)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S572 = id_4;
        }
        if(16U < k_14)
        {
            int * _S573 = nbrs_2 + (r_6 * k_14 + 16U);
            if(((&h_5)->d_0[int(16)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(16)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(16)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S573 = id_4;
        }
        if(17U < k_14)
        {
            int * _S574 = nbrs_2 + (r_6 * k_14 + 17U);
            if(((&h_5)->d_0[int(17)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(17)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(17)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S574 = id_4;
        }
        if(18U < k_14)
        {
            int * _S575 = nbrs_2 + (r_6 * k_14 + 18U);
            if(((&h_5)->d_0[int(18)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(18)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(18)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S575 = id_4;
        }
        if(19U < k_14)
        {
            int * _S576 = nbrs_2 + (r_6 * k_14 + 19U);
            if(((&h_5)->d_0[int(19)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(19)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(19)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S576 = id_4;
        }
        if(20U < k_14)
        {
            int * _S577 = nbrs_2 + (r_6 * k_14 + 20U);
            if(((&h_5)->d_0[int(20)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(20)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(20)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S577 = id_4;
        }
        if(21U < k_14)
        {
            int * _S578 = nbrs_2 + (r_6 * k_14 + 21U);
            if(((&h_5)->d_0[int(21)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(21)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(21)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S578 = id_4;
        }
        if(22U < k_14)
        {
            int * _S579 = nbrs_2 + (r_6 * k_14 + 22U);
            if(((&h_5)->d_0[int(22)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(22)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(22)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S579 = id_4;
        }
        if(23U < k_14)
        {
            int * _S580 = nbrs_2 + (r_6 * k_14 + 23U);
            if(((&h_5)->d_0[int(23)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(23)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(23)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S580 = id_4;
        }
        if(24U < k_14)
        {
            int * _S581 = nbrs_2 + (r_6 * k_14 + 24U);
            if(((&h_5)->d_0[int(24)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(24)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(24)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S581 = id_4;
        }
        if(25U < k_14)
        {
            int * _S582 = nbrs_2 + (r_6 * k_14 + 25U);
            if(((&h_5)->d_0[int(25)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(25)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(25)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S582 = id_4;
        }
        if(26U < k_14)
        {
            int * _S583 = nbrs_2 + (r_6 * k_14 + 26U);
            if(((&h_5)->d_0[int(26)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(26)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(26)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S583 = id_4;
        }
        if(27U < k_14)
        {
            int * _S584 = nbrs_2 + (r_6 * k_14 + 27U);
            if(((&h_5)->d_0[int(27)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(27)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(27)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S584 = id_4;
        }
        if(28U < k_14)
        {
            int * _S585 = nbrs_2 + (r_6 * k_14 + 28U);
            if(((&h_5)->d_0[int(28)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(28)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(28)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S585 = id_4;
        }
        if(29U < k_14)
        {
            int * _S586 = nbrs_2 + (r_6 * k_14 + 29U);
            if(((&h_5)->d_0[int(29)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(29)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(29)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S586 = id_4;
        }
        if(30U < k_14)
        {
            int * _S587 = nbrs_2 + (r_6 * k_14 + 30U);
            if(((&h_5)->d_0[int(30)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(30)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(30)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S587 = id_4;
        }
        if(31U < k_14)
        {
            int * _S588 = nbrs_2 + (r_6 * k_14 + 31U);
            if(((&h_5)->d_0[int(31)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_2)
                {
                    id_4 = *(perm_3 + (&h_5)->id_1[int(31)]);
                }
                else
                {
                    id_4 = int((&h_5)->id_1[int(31)]);
                }
            }
            else
            {
                id_4 = int(-1);
            }
            *_S588 = id_4;
        }
    }
    if(_S556 < 2U)
    {
        return make_float3 (0.0f);
    }
    float3  so_24;
    float3  sd_24;
    float3  s1_2;
    float3  _S589 = make_float3 (0.0f);
    for(;;)
    {
        for(;;)
        {
            for(;;)
            {
                bool _S590 = 0U >= k_14;
                _S520 = _S590;
                if(_S590)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(0)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    s1_2 = _S589;
                    sd_24 = _S589;
                    so_24 = _S589;
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(0)])).x, (*(sorted_11 + (&h_5)->id_1[int(0)])).y, (*(sorted_11 + (&h_5)->id_1[int(0)])).z};
                }
                else
                {
                    float3  _S591 = pca_load3_0(means_3, (&h_5)->id_1[int(0)]);
                    s1_2 = _S591;
                }
                float3  d_27 = s1_2 - q_9;
                float3  _S592 = d_27 * d_27;
                float _S593 = d_27.x;
                float _S594 = d_27.y;
                float _S595 = d_27.z;
                float3  _S596 = make_float3 (_S593 * _S594, _S593 * _S595, _S594 * _S595);
                s1_2 = d_27;
                sd_24 = _S592;
                so_24 = _S596;
                break;
            }
            break;
        }
        float3  _S597;
        for(;;)
        {
            for(;;)
            {
                bool _S598 = 1U >= k_14;
                _S521 = _S598;
                if(_S598)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(1)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(1)])).x, (*(sorted_11 + (&h_5)->id_1[int(1)])).y, (*(sorted_11 + (&h_5)->id_1[int(1)])).z};
                }
                else
                {
                    float3  _S599 = pca_load3_0(means_3, (&h_5)->id_1[int(1)]);
                    _S597 = _S599;
                }
                float3  d_28 = _S597 - q_9;
                float3  sd_25 = sd_24 + d_28 * d_28;
                float _S600 = d_28.x;
                float _S601 = d_28.y;
                float _S602 = d_28.z;
                float3  so_25 = so_24 + make_float3 (_S600 * _S601, _S600 * _S602, _S601 * _S602);
                s1_2 = s1_2 + d_28;
                sd_24 = sd_25;
                so_24 = so_25;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S603 = 2U >= k_14;
                _S522 = _S603;
                if(_S603)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(2)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(2)])).x, (*(sorted_11 + (&h_5)->id_1[int(2)])).y, (*(sorted_11 + (&h_5)->id_1[int(2)])).z};
                }
                else
                {
                    float3  _S604 = pca_load3_0(means_3, (&h_5)->id_1[int(2)]);
                    _S597 = _S604;
                }
                float3  d_29 = _S597 - q_9;
                float3  sd_26 = sd_24 + d_29 * d_29;
                float _S605 = d_29.x;
                float _S606 = d_29.y;
                float _S607 = d_29.z;
                float3  so_26 = so_24 + make_float3 (_S605 * _S606, _S605 * _S607, _S606 * _S607);
                s1_2 = s1_2 + d_29;
                sd_24 = sd_26;
                so_24 = so_26;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S608 = 3U >= k_14;
                _S523 = _S608;
                if(_S608)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(3)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(3)])).x, (*(sorted_11 + (&h_5)->id_1[int(3)])).y, (*(sorted_11 + (&h_5)->id_1[int(3)])).z};
                }
                else
                {
                    float3  _S609 = pca_load3_0(means_3, (&h_5)->id_1[int(3)]);
                    _S597 = _S609;
                }
                float3  d_30 = _S597 - q_9;
                float3  sd_27 = sd_24 + d_30 * d_30;
                float _S610 = d_30.x;
                float _S611 = d_30.y;
                float _S612 = d_30.z;
                float3  so_27 = so_24 + make_float3 (_S610 * _S611, _S610 * _S612, _S611 * _S612);
                s1_2 = s1_2 + d_30;
                sd_24 = sd_27;
                so_24 = so_27;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S613 = 4U >= k_14;
                _S524 = _S613;
                if(_S613)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(4)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(4)])).x, (*(sorted_11 + (&h_5)->id_1[int(4)])).y, (*(sorted_11 + (&h_5)->id_1[int(4)])).z};
                }
                else
                {
                    float3  _S614 = pca_load3_0(means_3, (&h_5)->id_1[int(4)]);
                    _S597 = _S614;
                }
                float3  d_31 = _S597 - q_9;
                float3  sd_28 = sd_24 + d_31 * d_31;
                float _S615 = d_31.x;
                float _S616 = d_31.y;
                float _S617 = d_31.z;
                float3  so_28 = so_24 + make_float3 (_S615 * _S616, _S615 * _S617, _S616 * _S617);
                s1_2 = s1_2 + d_31;
                sd_24 = sd_28;
                so_24 = so_28;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S618 = 5U >= k_14;
                _S525 = _S618;
                if(_S618)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(5)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(5)])).x, (*(sorted_11 + (&h_5)->id_1[int(5)])).y, (*(sorted_11 + (&h_5)->id_1[int(5)])).z};
                }
                else
                {
                    float3  _S619 = pca_load3_0(means_3, (&h_5)->id_1[int(5)]);
                    _S597 = _S619;
                }
                float3  d_32 = _S597 - q_9;
                float3  sd_29 = sd_24 + d_32 * d_32;
                float _S620 = d_32.x;
                float _S621 = d_32.y;
                float _S622 = d_32.z;
                float3  so_29 = so_24 + make_float3 (_S620 * _S621, _S620 * _S622, _S621 * _S622);
                s1_2 = s1_2 + d_32;
                sd_24 = sd_29;
                so_24 = so_29;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S623 = 6U >= k_14;
                _S526 = _S623;
                if(_S623)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(6)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(6)])).x, (*(sorted_11 + (&h_5)->id_1[int(6)])).y, (*(sorted_11 + (&h_5)->id_1[int(6)])).z};
                }
                else
                {
                    float3  _S624 = pca_load3_0(means_3, (&h_5)->id_1[int(6)]);
                    _S597 = _S624;
                }
                float3  d_33 = _S597 - q_9;
                float3  sd_30 = sd_24 + d_33 * d_33;
                float _S625 = d_33.x;
                float _S626 = d_33.y;
                float _S627 = d_33.z;
                float3  so_30 = so_24 + make_float3 (_S625 * _S626, _S625 * _S627, _S626 * _S627);
                s1_2 = s1_2 + d_33;
                sd_24 = sd_30;
                so_24 = so_30;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S628 = 7U >= k_14;
                _S527 = _S628;
                if(_S628)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(7)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(7)])).x, (*(sorted_11 + (&h_5)->id_1[int(7)])).y, (*(sorted_11 + (&h_5)->id_1[int(7)])).z};
                }
                else
                {
                    float3  _S629 = pca_load3_0(means_3, (&h_5)->id_1[int(7)]);
                    _S597 = _S629;
                }
                float3  d_34 = _S597 - q_9;
                float3  sd_31 = sd_24 + d_34 * d_34;
                float _S630 = d_34.x;
                float _S631 = d_34.y;
                float _S632 = d_34.z;
                float3  so_31 = so_24 + make_float3 (_S630 * _S631, _S630 * _S632, _S631 * _S632);
                s1_2 = s1_2 + d_34;
                sd_24 = sd_31;
                so_24 = so_31;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S633 = 8U >= k_14;
                _S528 = _S633;
                if(_S633)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(8)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(8)])).x, (*(sorted_11 + (&h_5)->id_1[int(8)])).y, (*(sorted_11 + (&h_5)->id_1[int(8)])).z};
                }
                else
                {
                    float3  _S634 = pca_load3_0(means_3, (&h_5)->id_1[int(8)]);
                    _S597 = _S634;
                }
                float3  d_35 = _S597 - q_9;
                float3  sd_32 = sd_24 + d_35 * d_35;
                float _S635 = d_35.x;
                float _S636 = d_35.y;
                float _S637 = d_35.z;
                float3  so_32 = so_24 + make_float3 (_S635 * _S636, _S635 * _S637, _S636 * _S637);
                s1_2 = s1_2 + d_35;
                sd_24 = sd_32;
                so_24 = so_32;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S638 = 9U >= k_14;
                _S529 = _S638;
                if(_S638)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(9)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(9)])).x, (*(sorted_11 + (&h_5)->id_1[int(9)])).y, (*(sorted_11 + (&h_5)->id_1[int(9)])).z};
                }
                else
                {
                    float3  _S639 = pca_load3_0(means_3, (&h_5)->id_1[int(9)]);
                    _S597 = _S639;
                }
                float3  d_36 = _S597 - q_9;
                float3  sd_33 = sd_24 + d_36 * d_36;
                float _S640 = d_36.x;
                float _S641 = d_36.y;
                float _S642 = d_36.z;
                float3  so_33 = so_24 + make_float3 (_S640 * _S641, _S640 * _S642, _S641 * _S642);
                s1_2 = s1_2 + d_36;
                sd_24 = sd_33;
                so_24 = so_33;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S643 = 10U >= k_14;
                _S530 = _S643;
                if(_S643)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(10)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(10)])).x, (*(sorted_11 + (&h_5)->id_1[int(10)])).y, (*(sorted_11 + (&h_5)->id_1[int(10)])).z};
                }
                else
                {
                    float3  _S644 = pca_load3_0(means_3, (&h_5)->id_1[int(10)]);
                    _S597 = _S644;
                }
                float3  d_37 = _S597 - q_9;
                float3  sd_34 = sd_24 + d_37 * d_37;
                float _S645 = d_37.x;
                float _S646 = d_37.y;
                float _S647 = d_37.z;
                float3  so_34 = so_24 + make_float3 (_S645 * _S646, _S645 * _S647, _S646 * _S647);
                s1_2 = s1_2 + d_37;
                sd_24 = sd_34;
                so_24 = so_34;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S648 = 11U >= k_14;
                _S531 = _S648;
                if(_S648)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(11)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(11)])).x, (*(sorted_11 + (&h_5)->id_1[int(11)])).y, (*(sorted_11 + (&h_5)->id_1[int(11)])).z};
                }
                else
                {
                    float3  _S649 = pca_load3_0(means_3, (&h_5)->id_1[int(11)]);
                    _S597 = _S649;
                }
                float3  d_38 = _S597 - q_9;
                float3  sd_35 = sd_24 + d_38 * d_38;
                float _S650 = d_38.x;
                float _S651 = d_38.y;
                float _S652 = d_38.z;
                float3  so_35 = so_24 + make_float3 (_S650 * _S651, _S650 * _S652, _S651 * _S652);
                s1_2 = s1_2 + d_38;
                sd_24 = sd_35;
                so_24 = so_35;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S653 = 12U >= k_14;
                _S532 = _S653;
                if(_S653)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(12)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(12)])).x, (*(sorted_11 + (&h_5)->id_1[int(12)])).y, (*(sorted_11 + (&h_5)->id_1[int(12)])).z};
                }
                else
                {
                    float3  _S654 = pca_load3_0(means_3, (&h_5)->id_1[int(12)]);
                    _S597 = _S654;
                }
                float3  d_39 = _S597 - q_9;
                float3  sd_36 = sd_24 + d_39 * d_39;
                float _S655 = d_39.x;
                float _S656 = d_39.y;
                float _S657 = d_39.z;
                float3  so_36 = so_24 + make_float3 (_S655 * _S656, _S655 * _S657, _S656 * _S657);
                s1_2 = s1_2 + d_39;
                sd_24 = sd_36;
                so_24 = so_36;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S658 = 13U >= k_14;
                _S533 = _S658;
                if(_S658)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(13)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(13)])).x, (*(sorted_11 + (&h_5)->id_1[int(13)])).y, (*(sorted_11 + (&h_5)->id_1[int(13)])).z};
                }
                else
                {
                    float3  _S659 = pca_load3_0(means_3, (&h_5)->id_1[int(13)]);
                    _S597 = _S659;
                }
                float3  d_40 = _S597 - q_9;
                float3  sd_37 = sd_24 + d_40 * d_40;
                float _S660 = d_40.x;
                float _S661 = d_40.y;
                float _S662 = d_40.z;
                float3  so_37 = so_24 + make_float3 (_S660 * _S661, _S660 * _S662, _S661 * _S662);
                s1_2 = s1_2 + d_40;
                sd_24 = sd_37;
                so_24 = so_37;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S663 = 14U >= k_14;
                _S534 = _S663;
                if(_S663)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(14)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(14)])).x, (*(sorted_11 + (&h_5)->id_1[int(14)])).y, (*(sorted_11 + (&h_5)->id_1[int(14)])).z};
                }
                else
                {
                    float3  _S664 = pca_load3_0(means_3, (&h_5)->id_1[int(14)]);
                    _S597 = _S664;
                }
                float3  d_41 = _S597 - q_9;
                float3  sd_38 = sd_24 + d_41 * d_41;
                float _S665 = d_41.x;
                float _S666 = d_41.y;
                float _S667 = d_41.z;
                float3  so_38 = so_24 + make_float3 (_S665 * _S666, _S665 * _S667, _S666 * _S667);
                s1_2 = s1_2 + d_41;
                sd_24 = sd_38;
                so_24 = so_38;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S668 = 15U >= k_14;
                _S535 = _S668;
                if(_S668)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(15)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(15)])).x, (*(sorted_11 + (&h_5)->id_1[int(15)])).y, (*(sorted_11 + (&h_5)->id_1[int(15)])).z};
                }
                else
                {
                    float3  _S669 = pca_load3_0(means_3, (&h_5)->id_1[int(15)]);
                    _S597 = _S669;
                }
                float3  d_42 = _S597 - q_9;
                float3  sd_39 = sd_24 + d_42 * d_42;
                float _S670 = d_42.x;
                float _S671 = d_42.y;
                float _S672 = d_42.z;
                float3  so_39 = so_24 + make_float3 (_S670 * _S671, _S670 * _S672, _S671 * _S672);
                s1_2 = s1_2 + d_42;
                sd_24 = sd_39;
                so_24 = so_39;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S673 = 16U >= k_14;
                _S536 = _S673;
                if(_S673)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(16)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(16)])).x, (*(sorted_11 + (&h_5)->id_1[int(16)])).y, (*(sorted_11 + (&h_5)->id_1[int(16)])).z};
                }
                else
                {
                    float3  _S674 = pca_load3_0(means_3, (&h_5)->id_1[int(16)]);
                    _S597 = _S674;
                }
                float3  d_43 = _S597 - q_9;
                float3  sd_40 = sd_24 + d_43 * d_43;
                float _S675 = d_43.x;
                float _S676 = d_43.y;
                float _S677 = d_43.z;
                float3  so_40 = so_24 + make_float3 (_S675 * _S676, _S675 * _S677, _S676 * _S677);
                s1_2 = s1_2 + d_43;
                sd_24 = sd_40;
                so_24 = so_40;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S678 = 17U >= k_14;
                _S537 = _S678;
                if(_S678)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(17)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(17)])).x, (*(sorted_11 + (&h_5)->id_1[int(17)])).y, (*(sorted_11 + (&h_5)->id_1[int(17)])).z};
                }
                else
                {
                    float3  _S679 = pca_load3_0(means_3, (&h_5)->id_1[int(17)]);
                    _S597 = _S679;
                }
                float3  d_44 = _S597 - q_9;
                float3  sd_41 = sd_24 + d_44 * d_44;
                float _S680 = d_44.x;
                float _S681 = d_44.y;
                float _S682 = d_44.z;
                float3  so_41 = so_24 + make_float3 (_S680 * _S681, _S680 * _S682, _S681 * _S682);
                s1_2 = s1_2 + d_44;
                sd_24 = sd_41;
                so_24 = so_41;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S683 = 18U >= k_14;
                _S538 = _S683;
                if(_S683)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(18)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(18)])).x, (*(sorted_11 + (&h_5)->id_1[int(18)])).y, (*(sorted_11 + (&h_5)->id_1[int(18)])).z};
                }
                else
                {
                    float3  _S684 = pca_load3_0(means_3, (&h_5)->id_1[int(18)]);
                    _S597 = _S684;
                }
                float3  d_45 = _S597 - q_9;
                float3  sd_42 = sd_24 + d_45 * d_45;
                float _S685 = d_45.x;
                float _S686 = d_45.y;
                float _S687 = d_45.z;
                float3  so_42 = so_24 + make_float3 (_S685 * _S686, _S685 * _S687, _S686 * _S687);
                s1_2 = s1_2 + d_45;
                sd_24 = sd_42;
                so_24 = so_42;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S688 = 19U >= k_14;
                _S539 = _S688;
                if(_S688)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(19)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(19)])).x, (*(sorted_11 + (&h_5)->id_1[int(19)])).y, (*(sorted_11 + (&h_5)->id_1[int(19)])).z};
                }
                else
                {
                    float3  _S689 = pca_load3_0(means_3, (&h_5)->id_1[int(19)]);
                    _S597 = _S689;
                }
                float3  d_46 = _S597 - q_9;
                float3  sd_43 = sd_24 + d_46 * d_46;
                float _S690 = d_46.x;
                float _S691 = d_46.y;
                float _S692 = d_46.z;
                float3  so_43 = so_24 + make_float3 (_S690 * _S691, _S690 * _S692, _S691 * _S692);
                s1_2 = s1_2 + d_46;
                sd_24 = sd_43;
                so_24 = so_43;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S693 = 20U >= k_14;
                _S540 = _S693;
                if(_S693)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(20)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(20)])).x, (*(sorted_11 + (&h_5)->id_1[int(20)])).y, (*(sorted_11 + (&h_5)->id_1[int(20)])).z};
                }
                else
                {
                    float3  _S694 = pca_load3_0(means_3, (&h_5)->id_1[int(20)]);
                    _S597 = _S694;
                }
                float3  d_47 = _S597 - q_9;
                float3  sd_44 = sd_24 + d_47 * d_47;
                float _S695 = d_47.x;
                float _S696 = d_47.y;
                float _S697 = d_47.z;
                float3  so_44 = so_24 + make_float3 (_S695 * _S696, _S695 * _S697, _S696 * _S697);
                s1_2 = s1_2 + d_47;
                sd_24 = sd_44;
                so_24 = so_44;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S698 = 21U >= k_14;
                _S541 = _S698;
                if(_S698)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(21)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(21)])).x, (*(sorted_11 + (&h_5)->id_1[int(21)])).y, (*(sorted_11 + (&h_5)->id_1[int(21)])).z};
                }
                else
                {
                    float3  _S699 = pca_load3_0(means_3, (&h_5)->id_1[int(21)]);
                    _S597 = _S699;
                }
                float3  d_48 = _S597 - q_9;
                float3  sd_45 = sd_24 + d_48 * d_48;
                float _S700 = d_48.x;
                float _S701 = d_48.y;
                float _S702 = d_48.z;
                float3  so_45 = so_24 + make_float3 (_S700 * _S701, _S700 * _S702, _S701 * _S702);
                s1_2 = s1_2 + d_48;
                sd_24 = sd_45;
                so_24 = so_45;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S703 = 22U >= k_14;
                _S542 = _S703;
                if(_S703)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(22)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(22)])).x, (*(sorted_11 + (&h_5)->id_1[int(22)])).y, (*(sorted_11 + (&h_5)->id_1[int(22)])).z};
                }
                else
                {
                    float3  _S704 = pca_load3_0(means_3, (&h_5)->id_1[int(22)]);
                    _S597 = _S704;
                }
                float3  d_49 = _S597 - q_9;
                float3  sd_46 = sd_24 + d_49 * d_49;
                float _S705 = d_49.x;
                float _S706 = d_49.y;
                float _S707 = d_49.z;
                float3  so_46 = so_24 + make_float3 (_S705 * _S706, _S705 * _S707, _S706 * _S707);
                s1_2 = s1_2 + d_49;
                sd_24 = sd_46;
                so_24 = so_46;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S708 = 23U >= k_14;
                _S543 = _S708;
                if(_S708)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(23)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(23)])).x, (*(sorted_11 + (&h_5)->id_1[int(23)])).y, (*(sorted_11 + (&h_5)->id_1[int(23)])).z};
                }
                else
                {
                    float3  _S709 = pca_load3_0(means_3, (&h_5)->id_1[int(23)]);
                    _S597 = _S709;
                }
                float3  d_50 = _S597 - q_9;
                float3  sd_47 = sd_24 + d_50 * d_50;
                float _S710 = d_50.x;
                float _S711 = d_50.y;
                float _S712 = d_50.z;
                float3  so_47 = so_24 + make_float3 (_S710 * _S711, _S710 * _S712, _S711 * _S712);
                s1_2 = s1_2 + d_50;
                sd_24 = sd_47;
                so_24 = so_47;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S713 = 24U >= k_14;
                _S544 = _S713;
                if(_S713)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(24)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(24)])).x, (*(sorted_11 + (&h_5)->id_1[int(24)])).y, (*(sorted_11 + (&h_5)->id_1[int(24)])).z};
                }
                else
                {
                    float3  _S714 = pca_load3_0(means_3, (&h_5)->id_1[int(24)]);
                    _S597 = _S714;
                }
                float3  d_51 = _S597 - q_9;
                float3  sd_48 = sd_24 + d_51 * d_51;
                float _S715 = d_51.x;
                float _S716 = d_51.y;
                float _S717 = d_51.z;
                float3  so_48 = so_24 + make_float3 (_S715 * _S716, _S715 * _S717, _S716 * _S717);
                s1_2 = s1_2 + d_51;
                sd_24 = sd_48;
                so_24 = so_48;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S718 = 25U >= k_14;
                _S545 = _S718;
                if(_S718)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(25)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(25)])).x, (*(sorted_11 + (&h_5)->id_1[int(25)])).y, (*(sorted_11 + (&h_5)->id_1[int(25)])).z};
                }
                else
                {
                    float3  _S719 = pca_load3_0(means_3, (&h_5)->id_1[int(25)]);
                    _S597 = _S719;
                }
                float3  d_52 = _S597 - q_9;
                float3  sd_49 = sd_24 + d_52 * d_52;
                float _S720 = d_52.x;
                float _S721 = d_52.y;
                float _S722 = d_52.z;
                float3  so_49 = so_24 + make_float3 (_S720 * _S721, _S720 * _S722, _S721 * _S722);
                s1_2 = s1_2 + d_52;
                sd_24 = sd_49;
                so_24 = so_49;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S723 = 26U >= k_14;
                _S546 = _S723;
                if(_S723)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(26)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(26)])).x, (*(sorted_11 + (&h_5)->id_1[int(26)])).y, (*(sorted_11 + (&h_5)->id_1[int(26)])).z};
                }
                else
                {
                    float3  _S724 = pca_load3_0(means_3, (&h_5)->id_1[int(26)]);
                    _S597 = _S724;
                }
                float3  d_53 = _S597 - q_9;
                float3  sd_50 = sd_24 + d_53 * d_53;
                float _S725 = d_53.x;
                float _S726 = d_53.y;
                float _S727 = d_53.z;
                float3  so_50 = so_24 + make_float3 (_S725 * _S726, _S725 * _S727, _S726 * _S727);
                s1_2 = s1_2 + d_53;
                sd_24 = sd_50;
                so_24 = so_50;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S728 = 27U >= k_14;
                _S547 = _S728;
                if(_S728)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(27)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(27)])).x, (*(sorted_11 + (&h_5)->id_1[int(27)])).y, (*(sorted_11 + (&h_5)->id_1[int(27)])).z};
                }
                else
                {
                    float3  _S729 = pca_load3_0(means_3, (&h_5)->id_1[int(27)]);
                    _S597 = _S729;
                }
                float3  d_54 = _S597 - q_9;
                float3  sd_51 = sd_24 + d_54 * d_54;
                float _S730 = d_54.x;
                float _S731 = d_54.y;
                float _S732 = d_54.z;
                float3  so_51 = so_24 + make_float3 (_S730 * _S731, _S730 * _S732, _S731 * _S732);
                s1_2 = s1_2 + d_54;
                sd_24 = sd_51;
                so_24 = so_51;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S733 = 28U >= k_14;
                _S548 = _S733;
                if(_S733)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(28)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(28)])).x, (*(sorted_11 + (&h_5)->id_1[int(28)])).y, (*(sorted_11 + (&h_5)->id_1[int(28)])).z};
                }
                else
                {
                    float3  _S734 = pca_load3_0(means_3, (&h_5)->id_1[int(28)]);
                    _S597 = _S734;
                }
                float3  d_55 = _S597 - q_9;
                float3  sd_52 = sd_24 + d_55 * d_55;
                float _S735 = d_55.x;
                float _S736 = d_55.y;
                float _S737 = d_55.z;
                float3  so_52 = so_24 + make_float3 (_S735 * _S736, _S735 * _S737, _S736 * _S737);
                s1_2 = s1_2 + d_55;
                sd_24 = sd_52;
                so_24 = so_52;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S738 = 29U >= k_14;
                _S549 = _S738;
                if(_S738)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(29)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(29)])).x, (*(sorted_11 + (&h_5)->id_1[int(29)])).y, (*(sorted_11 + (&h_5)->id_1[int(29)])).z};
                }
                else
                {
                    float3  _S739 = pca_load3_0(means_3, (&h_5)->id_1[int(29)]);
                    _S597 = _S739;
                }
                float3  d_56 = _S597 - q_9;
                float3  sd_53 = sd_24 + d_56 * d_56;
                float _S740 = d_56.x;
                float _S741 = d_56.y;
                float _S742 = d_56.z;
                float3  so_53 = so_24 + make_float3 (_S740 * _S741, _S740 * _S742, _S741 * _S742);
                s1_2 = s1_2 + d_56;
                sd_24 = sd_53;
                so_24 = so_53;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S743 = 30U >= k_14;
                _S550 = _S743;
                if(_S743)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(30)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(30)])).x, (*(sorted_11 + (&h_5)->id_1[int(30)])).y, (*(sorted_11 + (&h_5)->id_1[int(30)])).z};
                }
                else
                {
                    float3  _S744 = pca_load3_0(means_3, (&h_5)->id_1[int(30)]);
                    _S597 = _S744;
                }
                float3  d_57 = _S597 - q_9;
                float3  sd_54 = sd_24 + d_57 * d_57;
                float _S745 = d_57.x;
                float _S746 = d_57.y;
                float _S747 = d_57.z;
                float3  so_54 = so_24 + make_float3 (_S745 * _S746, _S745 * _S747, _S746 * _S747);
                s1_2 = s1_2 + d_57;
                sd_24 = sd_54;
                so_24 = so_54;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S748 = 31U >= k_14;
                _S551 = _S748;
                if(_S748)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(31)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    _S597 = float3 {(*(sorted_11 + (&h_5)->id_1[int(31)])).x, (*(sorted_11 + (&h_5)->id_1[int(31)])).y, (*(sorted_11 + (&h_5)->id_1[int(31)])).z};
                }
                else
                {
                    float3  _S749 = pca_load3_0(means_3, (&h_5)->id_1[int(31)]);
                    _S597 = _S749;
                }
                float3  d_58 = _S597 - q_9;
                float3  sd_55 = sd_24 + d_58 * d_58;
                float _S750 = d_58.x;
                float _S751 = d_58.y;
                float _S752 = d_58.z;
                float3  so_55 = so_24 + make_float3 (_S750 * _S751, _S750 * _S752, _S751 * _S752);
                s1_2 = s1_2 + d_58;
                sd_24 = sd_55;
                so_24 = so_55;
                break;
            }
            break;
        }
        break;
    }
    float inv_k_2 = 1.0f / float(_S556 + 1U);
    float3  mean_2 = s1_2 * make_float3 (inv_k_2);
    float _S753 = mean_2.x;
    float _S754 = mean_2.y;
    float _S755 = mean_2.z;
    float3  ev_3;
    float3  e0_3;
    float3  e1_3;
    float3  e2_3;
    pca_sym3_eigen_0(sd_24 * make_float3 (inv_k_2) - mean_2 * mean_2, so_24 * make_float3 (inv_k_2) - make_float3 (_S753 * _S754, _S753 * _S755, _S754 * _S755), &ev_3, &e0_3, &e1_3, &e2_3);
    if(!all_0(isfinite_0(ev_3)))
    {
        scatter_2 = true;
    }
    else
    {
        scatter_2 = !all_0(isfinite_0(e0_3));
    }
    if(scatter_2)
    {
        return _S589;
    }
    float _S756 = 2.0f * inv_k_2;
    Matrix<float, 3, 3>  _S757 = makeMatrix<float, 3, 3> (_S756 * w_plane_2) * makeMatrix<float, 3, 3> (make_float3 (e0_3.x) * e0_3, make_float3 (e0_3.y) * e0_3, make_float3 (e0_3.z) * e0_3) + makeMatrix<float, 3, 3> (_S756 * w_tangent_2) * (makeMatrix<float, 3, 3> (make_float3 (e2_3.x) * e2_3, make_float3 (e2_3.y) * e2_3, make_float3 (e2_3.z) * e2_3) - makeMatrix<float, 3, 3> (make_float3 (e1_3.x) * e1_3, make_float3 (e1_3.y) * e1_3, make_float3 (e1_3.z) * e1_3));
    float3  v_29 = mul_0(_S757, - mean_2);
    if(w_plane_2 != 0.0f)
    {
        scatter_2 = true;
    }
    else
    {
        scatter_2 = w_tangent_2 != 0.0f;
    }
    bool _S758;
    if(scatter_2)
    {
        if(gate_means_2 == 0U)
        {
            _S758 = true;
        }
        else
        {
            _S758 = (*(radii_2 + self_5)) > 0.0f;
        }
    }
    else
    {
        _S758 = false;
    }
    if(_S758)
    {
        float * _S759 = v_means_2 + 3U * self_5;
        float _S760 = atomicAdd(_S759 + int(0), v_29.x);
        float _S761 = atomicAdd(_S759 + int(1) + int(0), v_29.y);
        float _S762 = atomicAdd(_S759 + int(2) + int(0), v_29.z);
    }
    for(;;)
    {
        for(;;)
        {
            for(;;)
            {
                bool _S763 = !scatter_2;
                _S552 = _S763;
                if(_S763)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S520;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(0)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(0)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(0)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(0)])).x, (*(sorted_11 + (&h_5)->id_1[int(0)])).y, (*(sorted_11 + (&h_5)->id_1[int(0)])).z};
                }
                else
                {
                    float3  _S764 = pca_load3_0(means_3, (&h_5)->id_1[int(0)]);
                    s1_2 = _S764;
                }
                float3  v_30 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S765 = v_means_2 + 3U * axis_4;
                float _S766 = atomicAdd(_S765 + int(0), v_30.x);
                float _S767 = atomicAdd(_S765 + int(1) + int(0), v_30.y);
                float _S768 = atomicAdd(_S765 + int(2) + int(0), v_30.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S521;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(1)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(1)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(1)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(1)])).x, (*(sorted_11 + (&h_5)->id_1[int(1)])).y, (*(sorted_11 + (&h_5)->id_1[int(1)])).z};
                }
                else
                {
                    float3  _S769 = pca_load3_0(means_3, (&h_5)->id_1[int(1)]);
                    s1_2 = _S769;
                }
                float3  v_31 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S770 = v_means_2 + 3U * axis_4;
                float _S771 = atomicAdd(_S770 + int(0), v_31.x);
                float _S772 = atomicAdd(_S770 + int(1) + int(0), v_31.y);
                float _S773 = atomicAdd(_S770 + int(2) + int(0), v_31.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S522;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(2)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(2)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(2)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(2)])).x, (*(sorted_11 + (&h_5)->id_1[int(2)])).y, (*(sorted_11 + (&h_5)->id_1[int(2)])).z};
                }
                else
                {
                    float3  _S774 = pca_load3_0(means_3, (&h_5)->id_1[int(2)]);
                    s1_2 = _S774;
                }
                float3  v_32 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S775 = v_means_2 + 3U * axis_4;
                float _S776 = atomicAdd(_S775 + int(0), v_32.x);
                float _S777 = atomicAdd(_S775 + int(1) + int(0), v_32.y);
                float _S778 = atomicAdd(_S775 + int(2) + int(0), v_32.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S523;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(3)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(3)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(3)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(3)])).x, (*(sorted_11 + (&h_5)->id_1[int(3)])).y, (*(sorted_11 + (&h_5)->id_1[int(3)])).z};
                }
                else
                {
                    float3  _S779 = pca_load3_0(means_3, (&h_5)->id_1[int(3)]);
                    s1_2 = _S779;
                }
                float3  v_33 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S780 = v_means_2 + 3U * axis_4;
                float _S781 = atomicAdd(_S780 + int(0), v_33.x);
                float _S782 = atomicAdd(_S780 + int(1) + int(0), v_33.y);
                float _S783 = atomicAdd(_S780 + int(2) + int(0), v_33.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S524;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(4)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(4)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(4)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(4)])).x, (*(sorted_11 + (&h_5)->id_1[int(4)])).y, (*(sorted_11 + (&h_5)->id_1[int(4)])).z};
                }
                else
                {
                    float3  _S784 = pca_load3_0(means_3, (&h_5)->id_1[int(4)]);
                    s1_2 = _S784;
                }
                float3  v_34 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S785 = v_means_2 + 3U * axis_4;
                float _S786 = atomicAdd(_S785 + int(0), v_34.x);
                float _S787 = atomicAdd(_S785 + int(1) + int(0), v_34.y);
                float _S788 = atomicAdd(_S785 + int(2) + int(0), v_34.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S525;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(5)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(5)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(5)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(5)])).x, (*(sorted_11 + (&h_5)->id_1[int(5)])).y, (*(sorted_11 + (&h_5)->id_1[int(5)])).z};
                }
                else
                {
                    float3  _S789 = pca_load3_0(means_3, (&h_5)->id_1[int(5)]);
                    s1_2 = _S789;
                }
                float3  v_35 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S790 = v_means_2 + 3U * axis_4;
                float _S791 = atomicAdd(_S790 + int(0), v_35.x);
                float _S792 = atomicAdd(_S790 + int(1) + int(0), v_35.y);
                float _S793 = atomicAdd(_S790 + int(2) + int(0), v_35.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S526;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(6)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(6)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(6)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(6)])).x, (*(sorted_11 + (&h_5)->id_1[int(6)])).y, (*(sorted_11 + (&h_5)->id_1[int(6)])).z};
                }
                else
                {
                    float3  _S794 = pca_load3_0(means_3, (&h_5)->id_1[int(6)]);
                    s1_2 = _S794;
                }
                float3  v_36 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S795 = v_means_2 + 3U * axis_4;
                float _S796 = atomicAdd(_S795 + int(0), v_36.x);
                float _S797 = atomicAdd(_S795 + int(1) + int(0), v_36.y);
                float _S798 = atomicAdd(_S795 + int(2) + int(0), v_36.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S527;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(7)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(7)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(7)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(7)])).x, (*(sorted_11 + (&h_5)->id_1[int(7)])).y, (*(sorted_11 + (&h_5)->id_1[int(7)])).z};
                }
                else
                {
                    float3  _S799 = pca_load3_0(means_3, (&h_5)->id_1[int(7)]);
                    s1_2 = _S799;
                }
                float3  v_37 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S800 = v_means_2 + 3U * axis_4;
                float _S801 = atomicAdd(_S800 + int(0), v_37.x);
                float _S802 = atomicAdd(_S800 + int(1) + int(0), v_37.y);
                float _S803 = atomicAdd(_S800 + int(2) + int(0), v_37.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S528;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(8)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(8)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(8)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(8)])).x, (*(sorted_11 + (&h_5)->id_1[int(8)])).y, (*(sorted_11 + (&h_5)->id_1[int(8)])).z};
                }
                else
                {
                    float3  _S804 = pca_load3_0(means_3, (&h_5)->id_1[int(8)]);
                    s1_2 = _S804;
                }
                float3  v_38 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S805 = v_means_2 + 3U * axis_4;
                float _S806 = atomicAdd(_S805 + int(0), v_38.x);
                float _S807 = atomicAdd(_S805 + int(1) + int(0), v_38.y);
                float _S808 = atomicAdd(_S805 + int(2) + int(0), v_38.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S529;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(9)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(9)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(9)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(9)])).x, (*(sorted_11 + (&h_5)->id_1[int(9)])).y, (*(sorted_11 + (&h_5)->id_1[int(9)])).z};
                }
                else
                {
                    float3  _S809 = pca_load3_0(means_3, (&h_5)->id_1[int(9)]);
                    s1_2 = _S809;
                }
                float3  v_39 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S810 = v_means_2 + 3U * axis_4;
                float _S811 = atomicAdd(_S810 + int(0), v_39.x);
                float _S812 = atomicAdd(_S810 + int(1) + int(0), v_39.y);
                float _S813 = atomicAdd(_S810 + int(2) + int(0), v_39.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S530;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(10)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(10)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(10)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(10)])).x, (*(sorted_11 + (&h_5)->id_1[int(10)])).y, (*(sorted_11 + (&h_5)->id_1[int(10)])).z};
                }
                else
                {
                    float3  _S814 = pca_load3_0(means_3, (&h_5)->id_1[int(10)]);
                    s1_2 = _S814;
                }
                float3  v_40 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S815 = v_means_2 + 3U * axis_4;
                float _S816 = atomicAdd(_S815 + int(0), v_40.x);
                float _S817 = atomicAdd(_S815 + int(1) + int(0), v_40.y);
                float _S818 = atomicAdd(_S815 + int(2) + int(0), v_40.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S531;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(11)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(11)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(11)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(11)])).x, (*(sorted_11 + (&h_5)->id_1[int(11)])).y, (*(sorted_11 + (&h_5)->id_1[int(11)])).z};
                }
                else
                {
                    float3  _S819 = pca_load3_0(means_3, (&h_5)->id_1[int(11)]);
                    s1_2 = _S819;
                }
                float3  v_41 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S820 = v_means_2 + 3U * axis_4;
                float _S821 = atomicAdd(_S820 + int(0), v_41.x);
                float _S822 = atomicAdd(_S820 + int(1) + int(0), v_41.y);
                float _S823 = atomicAdd(_S820 + int(2) + int(0), v_41.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S532;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(12)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(12)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(12)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(12)])).x, (*(sorted_11 + (&h_5)->id_1[int(12)])).y, (*(sorted_11 + (&h_5)->id_1[int(12)])).z};
                }
                else
                {
                    float3  _S824 = pca_load3_0(means_3, (&h_5)->id_1[int(12)]);
                    s1_2 = _S824;
                }
                float3  v_42 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S825 = v_means_2 + 3U * axis_4;
                float _S826 = atomicAdd(_S825 + int(0), v_42.x);
                float _S827 = atomicAdd(_S825 + int(1) + int(0), v_42.y);
                float _S828 = atomicAdd(_S825 + int(2) + int(0), v_42.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S533;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(13)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(13)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(13)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(13)])).x, (*(sorted_11 + (&h_5)->id_1[int(13)])).y, (*(sorted_11 + (&h_5)->id_1[int(13)])).z};
                }
                else
                {
                    float3  _S829 = pca_load3_0(means_3, (&h_5)->id_1[int(13)]);
                    s1_2 = _S829;
                }
                float3  v_43 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S830 = v_means_2 + 3U * axis_4;
                float _S831 = atomicAdd(_S830 + int(0), v_43.x);
                float _S832 = atomicAdd(_S830 + int(1) + int(0), v_43.y);
                float _S833 = atomicAdd(_S830 + int(2) + int(0), v_43.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S534;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(14)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(14)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(14)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(14)])).x, (*(sorted_11 + (&h_5)->id_1[int(14)])).y, (*(sorted_11 + (&h_5)->id_1[int(14)])).z};
                }
                else
                {
                    float3  _S834 = pca_load3_0(means_3, (&h_5)->id_1[int(14)]);
                    s1_2 = _S834;
                }
                float3  v_44 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S835 = v_means_2 + 3U * axis_4;
                float _S836 = atomicAdd(_S835 + int(0), v_44.x);
                float _S837 = atomicAdd(_S835 + int(1) + int(0), v_44.y);
                float _S838 = atomicAdd(_S835 + int(2) + int(0), v_44.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S535;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(15)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(15)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(15)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(15)])).x, (*(sorted_11 + (&h_5)->id_1[int(15)])).y, (*(sorted_11 + (&h_5)->id_1[int(15)])).z};
                }
                else
                {
                    float3  _S839 = pca_load3_0(means_3, (&h_5)->id_1[int(15)]);
                    s1_2 = _S839;
                }
                float3  v_45 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S840 = v_means_2 + 3U * axis_4;
                float _S841 = atomicAdd(_S840 + int(0), v_45.x);
                float _S842 = atomicAdd(_S840 + int(1) + int(0), v_45.y);
                float _S843 = atomicAdd(_S840 + int(2) + int(0), v_45.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S536;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(16)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(16)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(16)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(16)])).x, (*(sorted_11 + (&h_5)->id_1[int(16)])).y, (*(sorted_11 + (&h_5)->id_1[int(16)])).z};
                }
                else
                {
                    float3  _S844 = pca_load3_0(means_3, (&h_5)->id_1[int(16)]);
                    s1_2 = _S844;
                }
                float3  v_46 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S845 = v_means_2 + 3U * axis_4;
                float _S846 = atomicAdd(_S845 + int(0), v_46.x);
                float _S847 = atomicAdd(_S845 + int(1) + int(0), v_46.y);
                float _S848 = atomicAdd(_S845 + int(2) + int(0), v_46.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S537;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(17)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(17)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(17)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(17)])).x, (*(sorted_11 + (&h_5)->id_1[int(17)])).y, (*(sorted_11 + (&h_5)->id_1[int(17)])).z};
                }
                else
                {
                    float3  _S849 = pca_load3_0(means_3, (&h_5)->id_1[int(17)]);
                    s1_2 = _S849;
                }
                float3  v_47 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S850 = v_means_2 + 3U * axis_4;
                float _S851 = atomicAdd(_S850 + int(0), v_47.x);
                float _S852 = atomicAdd(_S850 + int(1) + int(0), v_47.y);
                float _S853 = atomicAdd(_S850 + int(2) + int(0), v_47.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S538;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(18)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(18)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(18)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(18)])).x, (*(sorted_11 + (&h_5)->id_1[int(18)])).y, (*(sorted_11 + (&h_5)->id_1[int(18)])).z};
                }
                else
                {
                    float3  _S854 = pca_load3_0(means_3, (&h_5)->id_1[int(18)]);
                    s1_2 = _S854;
                }
                float3  v_48 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S855 = v_means_2 + 3U * axis_4;
                float _S856 = atomicAdd(_S855 + int(0), v_48.x);
                float _S857 = atomicAdd(_S855 + int(1) + int(0), v_48.y);
                float _S858 = atomicAdd(_S855 + int(2) + int(0), v_48.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S539;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(19)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(19)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(19)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(19)])).x, (*(sorted_11 + (&h_5)->id_1[int(19)])).y, (*(sorted_11 + (&h_5)->id_1[int(19)])).z};
                }
                else
                {
                    float3  _S859 = pca_load3_0(means_3, (&h_5)->id_1[int(19)]);
                    s1_2 = _S859;
                }
                float3  v_49 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S860 = v_means_2 + 3U * axis_4;
                float _S861 = atomicAdd(_S860 + int(0), v_49.x);
                float _S862 = atomicAdd(_S860 + int(1) + int(0), v_49.y);
                float _S863 = atomicAdd(_S860 + int(2) + int(0), v_49.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S540;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(20)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(20)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(20)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(20)])).x, (*(sorted_11 + (&h_5)->id_1[int(20)])).y, (*(sorted_11 + (&h_5)->id_1[int(20)])).z};
                }
                else
                {
                    float3  _S864 = pca_load3_0(means_3, (&h_5)->id_1[int(20)]);
                    s1_2 = _S864;
                }
                float3  v_50 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S865 = v_means_2 + 3U * axis_4;
                float _S866 = atomicAdd(_S865 + int(0), v_50.x);
                float _S867 = atomicAdd(_S865 + int(1) + int(0), v_50.y);
                float _S868 = atomicAdd(_S865 + int(2) + int(0), v_50.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S541;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(21)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(21)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(21)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(21)])).x, (*(sorted_11 + (&h_5)->id_1[int(21)])).y, (*(sorted_11 + (&h_5)->id_1[int(21)])).z};
                }
                else
                {
                    float3  _S869 = pca_load3_0(means_3, (&h_5)->id_1[int(21)]);
                    s1_2 = _S869;
                }
                float3  v_51 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S870 = v_means_2 + 3U * axis_4;
                float _S871 = atomicAdd(_S870 + int(0), v_51.x);
                float _S872 = atomicAdd(_S870 + int(1) + int(0), v_51.y);
                float _S873 = atomicAdd(_S870 + int(2) + int(0), v_51.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S542;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(22)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(22)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(22)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(22)])).x, (*(sorted_11 + (&h_5)->id_1[int(22)])).y, (*(sorted_11 + (&h_5)->id_1[int(22)])).z};
                }
                else
                {
                    float3  _S874 = pca_load3_0(means_3, (&h_5)->id_1[int(22)]);
                    s1_2 = _S874;
                }
                float3  v_52 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S875 = v_means_2 + 3U * axis_4;
                float _S876 = atomicAdd(_S875 + int(0), v_52.x);
                float _S877 = atomicAdd(_S875 + int(1) + int(0), v_52.y);
                float _S878 = atomicAdd(_S875 + int(2) + int(0), v_52.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S543;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(23)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(23)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(23)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(23)])).x, (*(sorted_11 + (&h_5)->id_1[int(23)])).y, (*(sorted_11 + (&h_5)->id_1[int(23)])).z};
                }
                else
                {
                    float3  _S879 = pca_load3_0(means_3, (&h_5)->id_1[int(23)]);
                    s1_2 = _S879;
                }
                float3  v_53 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S880 = v_means_2 + 3U * axis_4;
                float _S881 = atomicAdd(_S880 + int(0), v_53.x);
                float _S882 = atomicAdd(_S880 + int(1) + int(0), v_53.y);
                float _S883 = atomicAdd(_S880 + int(2) + int(0), v_53.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S544;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(24)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(24)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(24)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(24)])).x, (*(sorted_11 + (&h_5)->id_1[int(24)])).y, (*(sorted_11 + (&h_5)->id_1[int(24)])).z};
                }
                else
                {
                    float3  _S884 = pca_load3_0(means_3, (&h_5)->id_1[int(24)]);
                    s1_2 = _S884;
                }
                float3  v_54 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S885 = v_means_2 + 3U * axis_4;
                float _S886 = atomicAdd(_S885 + int(0), v_54.x);
                float _S887 = atomicAdd(_S885 + int(1) + int(0), v_54.y);
                float _S888 = atomicAdd(_S885 + int(2) + int(0), v_54.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S545;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(25)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(25)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(25)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(25)])).x, (*(sorted_11 + (&h_5)->id_1[int(25)])).y, (*(sorted_11 + (&h_5)->id_1[int(25)])).z};
                }
                else
                {
                    float3  _S889 = pca_load3_0(means_3, (&h_5)->id_1[int(25)]);
                    s1_2 = _S889;
                }
                float3  v_55 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S890 = v_means_2 + 3U * axis_4;
                float _S891 = atomicAdd(_S890 + int(0), v_55.x);
                float _S892 = atomicAdd(_S890 + int(1) + int(0), v_55.y);
                float _S893 = atomicAdd(_S890 + int(2) + int(0), v_55.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S546;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(26)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(26)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(26)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(26)])).x, (*(sorted_11 + (&h_5)->id_1[int(26)])).y, (*(sorted_11 + (&h_5)->id_1[int(26)])).z};
                }
                else
                {
                    float3  _S894 = pca_load3_0(means_3, (&h_5)->id_1[int(26)]);
                    s1_2 = _S894;
                }
                float3  v_56 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S895 = v_means_2 + 3U * axis_4;
                float _S896 = atomicAdd(_S895 + int(0), v_56.x);
                float _S897 = atomicAdd(_S895 + int(1) + int(0), v_56.y);
                float _S898 = atomicAdd(_S895 + int(2) + int(0), v_56.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S547;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(27)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(27)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(27)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(27)])).x, (*(sorted_11 + (&h_5)->id_1[int(27)])).y, (*(sorted_11 + (&h_5)->id_1[int(27)])).z};
                }
                else
                {
                    float3  _S899 = pca_load3_0(means_3, (&h_5)->id_1[int(27)]);
                    s1_2 = _S899;
                }
                float3  v_57 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S900 = v_means_2 + 3U * axis_4;
                float _S901 = atomicAdd(_S900 + int(0), v_57.x);
                float _S902 = atomicAdd(_S900 + int(1) + int(0), v_57.y);
                float _S903 = atomicAdd(_S900 + int(2) + int(0), v_57.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S548;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(28)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(28)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(28)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(28)])).x, (*(sorted_11 + (&h_5)->id_1[int(28)])).y, (*(sorted_11 + (&h_5)->id_1[int(28)])).z};
                }
                else
                {
                    float3  _S904 = pca_load3_0(means_3, (&h_5)->id_1[int(28)]);
                    s1_2 = _S904;
                }
                float3  v_58 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S905 = v_means_2 + 3U * axis_4;
                float _S906 = atomicAdd(_S905 + int(0), v_58.x);
                float _S907 = atomicAdd(_S905 + int(1) + int(0), v_58.y);
                float _S908 = atomicAdd(_S905 + int(2) + int(0), v_58.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S549;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(29)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(29)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(29)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(29)])).x, (*(sorted_11 + (&h_5)->id_1[int(29)])).y, (*(sorted_11 + (&h_5)->id_1[int(29)])).z};
                }
                else
                {
                    float3  _S909 = pca_load3_0(means_3, (&h_5)->id_1[int(29)]);
                    s1_2 = _S909;
                }
                float3  v_59 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S910 = v_means_2 + 3U * axis_4;
                float _S911 = atomicAdd(_S910 + int(0), v_59.x);
                float _S912 = atomicAdd(_S910 + int(1) + int(0), v_59.y);
                float _S913 = atomicAdd(_S910 + int(2) + int(0), v_59.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S550;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(30)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(30)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(30)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(30)])).x, (*(sorted_11 + (&h_5)->id_1[int(30)])).y, (*(sorted_11 + (&h_5)->id_1[int(30)])).z};
                }
                else
                {
                    float3  _S914 = pca_load3_0(means_3, (&h_5)->id_1[int(30)]);
                    s1_2 = _S914;
                }
                float3  v_60 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S915 = v_means_2 + 3U * axis_4;
                float _S916 = atomicAdd(_S915 + int(0), v_60.x);
                float _S917 = atomicAdd(_S915 + int(1) + int(0), v_60.y);
                float _S918 = atomicAdd(_S915 + int(2) + int(0), v_60.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S552)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = _S551;
                }
                if(scatter_2)
                {
                    scatter_2 = true;
                }
                else
                {
                    scatter_2 = !(((&h_5)->d_0[int(31)]) < 3.00000000549775576e+38f);
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    axis_4 = uint(*(perm_3 + (&h_5)->id_1[int(31)]));
                }
                else
                {
                    axis_4 = (&h_5)->id_1[int(31)];
                }
                if(gate_means_2 != 0U)
                {
                    scatter_2 = !((*(radii_2 + axis_4)) > 0.0f);
                }
                else
                {
                    scatter_2 = false;
                }
                if(scatter_2)
                {
                    break;
                }
                if(by_rank_2)
                {
                    s1_2 = float3 {(*(sorted_11 + (&h_5)->id_1[int(31)])).x, (*(sorted_11 + (&h_5)->id_1[int(31)])).y, (*(sorted_11 + (&h_5)->id_1[int(31)])).z};
                }
                else
                {
                    float3  _S919 = pca_load3_0(means_3, (&h_5)->id_1[int(31)]);
                    s1_2 = _S919;
                }
                float3  v_61 = mul_0(_S757, s1_2 - q_9 - mean_2);
                float * _S920 = v_means_2 + 3U * axis_4;
                float _S921 = atomicAdd(_S920 + int(0), v_61.x);
                float _S922 = atomicAdd(_S920 + int(1) + int(0), v_61.y);
                float _S923 = atomicAdd(_S920 + int(2) + int(0), v_61.z);
                break;
            }
            break;
        }
        break;
    }
    float3  ls_2 = pca_load3_0(scales_2, self_5);
    float _S924 = ls_2.x;
    float _S925 = ls_2.y;
    if(_S924 <= _S925)
    {
        if(_S924 <= (ls_2.z))
        {
            axis_4 = 0U;
        }
        else
        {
            axis_4 = 2U;
        }
    }
    else
    {
        if(_S925 <= (ls_2.z))
        {
            axis_4 = 1U;
        }
        else
        {
            axis_4 = 2U;
        }
    }
    uint _S926 = 4U * self_5;
    float4  quat_3 = make_float4 (*(quats_2 + _S926), *(quats_2 + (_S926 + 1U)), *(quats_2 + (_S926 + 2U)), *(quats_2 + (_S926 + 3U)));
    float align_2 = pca_axis_align_0(quat_3, e0_3, axis_4);
    if(w_normal_2 != 0.0f)
    {
        float4  _S927 = make_float4 (0.0f);
        DiffPair_vectorx3Cfloatx2C4x3E_0 dq_2;
        (&dq_2)->primal_0 = quat_3;
        (&dq_2)->differential_0 = _S927;
        s_bwd_pca_axis_align_0(&dq_2, e0_3, axis_4, - w_normal_2);
        uint c_8 = 0U;
        for(;;)
        {
            if(c_8 < 4U)
            {
            }
            else
            {
                break;
            }
            float * _S928 = v_quats_2 + (_S926 + c_8);
            *_S928 = *_S928 + _slang_vector_get_element(dq_2.differential_0, c_8);
            c_8 = c_8 + 1U;
        }
    }
    if(w_surfel_2 != 0.0f)
    {
        float smin_2;
        if(axis_4 == 0U)
        {
            smin_2 = _S924;
        }
        else
        {
            if(axis_4 == 1U)
            {
                smin_2 = _S925;
            }
            else
            {
                smin_2 = ls_2.z;
            }
        }
        float * _S929 = v_scales_2 + (3U * self_5 + axis_4);
        *_S929 = *_S929 + w_surfel_2 * (F32_exp((smin_2)));
    }
    return make_float3 (ev_3.x, ev_3.z - ev_3.y, 1.0f - align_2);
}

inline __device__ float3  pca_reg_owner_3(uint r_7, uint mode_3, uint n_8, uint k_15, uint window_3, float * means_4, float * quats_3, float * scales_3, int * perm_4, float4  * sorted_12, float4  * nodes_7, int4  * meta_7, int * ranges_7, int * nbrs_3, uint store_list_3, float * radii_3, uint gate_means_3, float * v_means_3, float * v_quats_3, float * v_scales_3, float w_plane_3, float w_tangent_3, float w_normal_3, float w_surfel_3)
{
    bool _S930;
    bool _S931;
    bool _S932;
    bool _S933;
    bool _S934;
    bool _S935;
    bool _S936;
    bool _S937;
    bool _S938;
    PcaKnn_0 h_6;
    PcaKnn_reset_0(&h_6);
    bool by_rank_3 = mode_3 != 3U;
    uint self_6 = uint(*(perm_4 + r_7));
    float3  q_10;
    if(by_rank_3)
    {
        q_10 = float3 {(*(sorted_12 + r_7)).x, (*(sorted_12 + r_7)).y, (*(sorted_12 + r_7)).z};
    }
    else
    {
        float3  _S939 = pca_load3_0(means_4, self_6);
        q_10 = _S939;
    }
    int id_5;
    uint axis_5;
    bool scatter_3;
    if(mode_3 == 0U)
    {
        pca_knn_tree_0(r_7, n_8, k_15, sorted_12, nodes_7, meta_7, ranges_7, &h_6);
    }
    else
    {
        if(mode_3 == 2U)
        {
            uint half_3 = (U32_max((window_3), (k_15 + 1U))) / 2U;
            if(r_7 > half_3)
            {
                axis_5 = r_7 - half_3;
            }
            else
            {
                axis_5 = 0U;
            }
            PcaKnn_scan_0(&h_6, q_10, r_7, axis_5, (U32_min((r_7 + half_3 + 1U), (n_8))), 0U, 0U, sorted_12, k_15);
        }
        else
        {
            if(mode_3 == 1U)
            {
                if(r_7 > (k_15 / 2U))
                {
                    axis_5 = r_7 - k_15 / 2U;
                }
                else
                {
                    axis_5 = 0U;
                }
                if((axis_5 + k_15 + 1U) > n_8)
                {
                    uint _S940 = k_15 + 1U;
                    if(n_8 > _S940)
                    {
                        axis_5 = n_8 - _S940;
                    }
                    else
                    {
                        axis_5 = 0U;
                    }
                }
                if(axis_5 >= r_7)
                {
                    id_5 = int(1);
                }
                else
                {
                    id_5 = int(0);
                }
                uint i_125 = axis_5 + uint(id_5);
                if(0U < k_15)
                {
                    scatter_3 = i_125 < n_8;
                }
                else
                {
                    scatter_3 = false;
                }
                if(scatter_3)
                {
                    (&h_6)->d_0[int(0)] = 0.0f;
                    (&h_6)->id_1[int(0)] = i_125;
                }
                uint i_126 = axis_5 + 1U;
                if(i_126 >= r_7)
                {
                    id_5 = int(1);
                }
                else
                {
                    id_5 = int(0);
                }
                uint i_127 = i_126 + uint(id_5);
                if(1U < k_15)
                {
                    scatter_3 = i_127 < n_8;
                }
                else
                {
                    scatter_3 = false;
                }
                if(scatter_3)
                {
                    (&h_6)->d_0[int(1)] = 0.0f;
                    (&h_6)->id_1[int(1)] = i_127;
                }
                uint i_128 = axis_5 + 2U;
                if(i_128 >= r_7)
                {
                    id_5 = int(1);
                }
                else
                {
                    id_5 = int(0);
                }
                uint i_129 = i_128 + uint(id_5);
                if(2U < k_15)
                {
                    scatter_3 = i_129 < n_8;
                }
                else
                {
                    scatter_3 = false;
                }
                if(scatter_3)
                {
                    (&h_6)->d_0[int(2)] = 0.0f;
                    (&h_6)->id_1[int(2)] = i_129;
                }
                uint i_130 = axis_5 + 3U;
                if(i_130 >= r_7)
                {
                    id_5 = int(1);
                }
                else
                {
                    id_5 = int(0);
                }
                uint i_131 = i_130 + uint(id_5);
                if(3U < k_15)
                {
                    scatter_3 = i_131 < n_8;
                }
                else
                {
                    scatter_3 = false;
                }
                if(scatter_3)
                {
                    (&h_6)->d_0[int(3)] = 0.0f;
                    (&h_6)->id_1[int(3)] = i_131;
                }
                uint i_132 = axis_5 + 4U;
                if(i_132 >= r_7)
                {
                    id_5 = int(1);
                }
                else
                {
                    id_5 = int(0);
                }
                uint i_133 = i_132 + uint(id_5);
                if(4U < k_15)
                {
                    scatter_3 = i_133 < n_8;
                }
                else
                {
                    scatter_3 = false;
                }
                if(scatter_3)
                {
                    (&h_6)->d_0[int(4)] = 0.0f;
                    (&h_6)->id_1[int(4)] = i_133;
                }
                uint i_134 = axis_5 + 5U;
                if(i_134 >= r_7)
                {
                    id_5 = int(1);
                }
                else
                {
                    id_5 = int(0);
                }
                uint i_135 = i_134 + uint(id_5);
                if(5U < k_15)
                {
                    scatter_3 = i_135 < n_8;
                }
                else
                {
                    scatter_3 = false;
                }
                if(scatter_3)
                {
                    (&h_6)->d_0[int(5)] = 0.0f;
                    (&h_6)->id_1[int(5)] = i_135;
                }
                uint i_136 = axis_5 + 6U;
                if(i_136 >= r_7)
                {
                    id_5 = int(1);
                }
                else
                {
                    id_5 = int(0);
                }
                uint i_137 = i_136 + uint(id_5);
                if(6U < k_15)
                {
                    scatter_3 = i_137 < n_8;
                }
                else
                {
                    scatter_3 = false;
                }
                if(scatter_3)
                {
                    (&h_6)->d_0[int(6)] = 0.0f;
                    (&h_6)->id_1[int(6)] = i_137;
                }
                uint i_138 = axis_5 + 7U;
                if(i_138 >= r_7)
                {
                    id_5 = int(1);
                }
                else
                {
                    id_5 = int(0);
                }
                uint i_139 = i_138 + uint(id_5);
                if(7U < k_15)
                {
                    scatter_3 = i_139 < n_8;
                }
                else
                {
                    scatter_3 = false;
                }
                if(scatter_3)
                {
                    (&h_6)->d_0[int(7)] = 0.0f;
                    (&h_6)->id_1[int(7)] = i_139;
                }
            }
            else
            {
                if(0U < k_15)
                {
                    id_5 = *(nbrs_3 + r_7 * k_15);
                }
                else
                {
                    id_5 = int(-1);
                }
                if(id_5 >= int(0))
                {
                    (&h_6)->d_0[int(0)] = 0.0f;
                    (&h_6)->id_1[int(0)] = uint(id_5);
                }
                if(1U < k_15)
                {
                    id_5 = *(nbrs_3 + (r_7 * k_15 + 1U));
                }
                else
                {
                    id_5 = int(-1);
                }
                if(id_5 >= int(0))
                {
                    (&h_6)->d_0[int(1)] = 0.0f;
                    (&h_6)->id_1[int(1)] = uint(id_5);
                }
                if(2U < k_15)
                {
                    id_5 = *(nbrs_3 + (r_7 * k_15 + 2U));
                }
                else
                {
                    id_5 = int(-1);
                }
                if(id_5 >= int(0))
                {
                    (&h_6)->d_0[int(2)] = 0.0f;
                    (&h_6)->id_1[int(2)] = uint(id_5);
                }
                if(3U < k_15)
                {
                    id_5 = *(nbrs_3 + (r_7 * k_15 + 3U));
                }
                else
                {
                    id_5 = int(-1);
                }
                if(id_5 >= int(0))
                {
                    (&h_6)->d_0[int(3)] = 0.0f;
                    (&h_6)->id_1[int(3)] = uint(id_5);
                }
                if(4U < k_15)
                {
                    id_5 = *(nbrs_3 + (r_7 * k_15 + 4U));
                }
                else
                {
                    id_5 = int(-1);
                }
                if(id_5 >= int(0))
                {
                    (&h_6)->d_0[int(4)] = 0.0f;
                    (&h_6)->id_1[int(4)] = uint(id_5);
                }
                if(5U < k_15)
                {
                    id_5 = *(nbrs_3 + (r_7 * k_15 + 5U));
                }
                else
                {
                    id_5 = int(-1);
                }
                if(id_5 >= int(0))
                {
                    (&h_6)->d_0[int(5)] = 0.0f;
                    (&h_6)->id_1[int(5)] = uint(id_5);
                }
                if(6U < k_15)
                {
                    id_5 = *(nbrs_3 + (r_7 * k_15 + 6U));
                }
                else
                {
                    id_5 = int(-1);
                }
                if(id_5 >= int(0))
                {
                    (&h_6)->d_0[int(6)] = 0.0f;
                    (&h_6)->id_1[int(6)] = uint(id_5);
                }
                if(7U < k_15)
                {
                    id_5 = *(nbrs_3 + (r_7 * k_15 + 7U));
                }
                else
                {
                    id_5 = int(-1);
                }
                if(id_5 >= int(0))
                {
                    (&h_6)->d_0[int(7)] = 0.0f;
                    (&h_6)->id_1[int(7)] = uint(id_5);
                }
            }
        }
    }
    PcaKnn_0 _S941 = h_6;
    uint _S942 = PcaKnn_count_0(&_S941, k_15);
    if(store_list_3 != 0U)
    {
        if(0U < k_15)
        {
            int * _S943 = nbrs_3 + r_7 * k_15;
            if(((&h_6)->d_0[int(0)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_3)
                {
                    id_5 = *(perm_4 + (&h_6)->id_1[int(0)]);
                }
                else
                {
                    id_5 = int((&h_6)->id_1[int(0)]);
                }
            }
            else
            {
                id_5 = int(-1);
            }
            *_S943 = id_5;
        }
        if(1U < k_15)
        {
            int * _S944 = nbrs_3 + (r_7 * k_15 + 1U);
            if(((&h_6)->d_0[int(1)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_3)
                {
                    id_5 = *(perm_4 + (&h_6)->id_1[int(1)]);
                }
                else
                {
                    id_5 = int((&h_6)->id_1[int(1)]);
                }
            }
            else
            {
                id_5 = int(-1);
            }
            *_S944 = id_5;
        }
        if(2U < k_15)
        {
            int * _S945 = nbrs_3 + (r_7 * k_15 + 2U);
            if(((&h_6)->d_0[int(2)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_3)
                {
                    id_5 = *(perm_4 + (&h_6)->id_1[int(2)]);
                }
                else
                {
                    id_5 = int((&h_6)->id_1[int(2)]);
                }
            }
            else
            {
                id_5 = int(-1);
            }
            *_S945 = id_5;
        }
        if(3U < k_15)
        {
            int * _S946 = nbrs_3 + (r_7 * k_15 + 3U);
            if(((&h_6)->d_0[int(3)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_3)
                {
                    id_5 = *(perm_4 + (&h_6)->id_1[int(3)]);
                }
                else
                {
                    id_5 = int((&h_6)->id_1[int(3)]);
                }
            }
            else
            {
                id_5 = int(-1);
            }
            *_S946 = id_5;
        }
        if(4U < k_15)
        {
            int * _S947 = nbrs_3 + (r_7 * k_15 + 4U);
            if(((&h_6)->d_0[int(4)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_3)
                {
                    id_5 = *(perm_4 + (&h_6)->id_1[int(4)]);
                }
                else
                {
                    id_5 = int((&h_6)->id_1[int(4)]);
                }
            }
            else
            {
                id_5 = int(-1);
            }
            *_S947 = id_5;
        }
        if(5U < k_15)
        {
            int * _S948 = nbrs_3 + (r_7 * k_15 + 5U);
            if(((&h_6)->d_0[int(5)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_3)
                {
                    id_5 = *(perm_4 + (&h_6)->id_1[int(5)]);
                }
                else
                {
                    id_5 = int((&h_6)->id_1[int(5)]);
                }
            }
            else
            {
                id_5 = int(-1);
            }
            *_S948 = id_5;
        }
        if(6U < k_15)
        {
            int * _S949 = nbrs_3 + (r_7 * k_15 + 6U);
            if(((&h_6)->d_0[int(6)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_3)
                {
                    id_5 = *(perm_4 + (&h_6)->id_1[int(6)]);
                }
                else
                {
                    id_5 = int((&h_6)->id_1[int(6)]);
                }
            }
            else
            {
                id_5 = int(-1);
            }
            *_S949 = id_5;
        }
        if(7U < k_15)
        {
            int * _S950 = nbrs_3 + (r_7 * k_15 + 7U);
            if(((&h_6)->d_0[int(7)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_3)
                {
                    id_5 = *(perm_4 + (&h_6)->id_1[int(7)]);
                }
                else
                {
                    id_5 = int((&h_6)->id_1[int(7)]);
                }
            }
            else
            {
                id_5 = int(-1);
            }
            *_S950 = id_5;
        }
    }
    if(_S942 < 2U)
    {
        return make_float3 (0.0f);
    }
    float3  so_56;
    float3  sd_56;
    float3  s1_3;
    float3  _S951 = make_float3 (0.0f);
    for(;;)
    {
        for(;;)
        {
            for(;;)
            {
                bool _S952 = 0U >= k_15;
                _S930 = _S952;
                if(_S952)
                {
                    scatter_3 = true;
                }
                else
                {
                    scatter_3 = !(((&h_6)->d_0[int(0)]) < 3.00000000549775576e+38f);
                }
                if(scatter_3)
                {
                    s1_3 = _S951;
                    sd_56 = _S951;
                    so_56 = _S951;
                    break;
                }
                if(by_rank_3)
                {
                    s1_3 = float3 {(*(sorted_12 + (&h_6)->id_1[int(0)])).x, (*(sorted_12 + (&h_6)->id_1[int(0)])).y, (*(sorted_12 + (&h_6)->id_1[int(0)])).z};
                }
                else
                {
                    float3  _S953 = pca_load3_0(means_4, (&h_6)->id_1[int(0)]);
                    s1_3 = _S953;
                }
                float3  d_59 = s1_3 - q_10;
                float3  _S954 = d_59 * d_59;
                float _S955 = d_59.x;
                float _S956 = d_59.y;
                float _S957 = d_59.z;
                float3  _S958 = make_float3 (_S955 * _S956, _S955 * _S957, _S956 * _S957);
                s1_3 = d_59;
                sd_56 = _S954;
                so_56 = _S958;
                break;
            }
            break;
        }
        float3  _S959;
        for(;;)
        {
            for(;;)
            {
                bool _S960 = 1U >= k_15;
                _S931 = _S960;
                if(_S960)
                {
                    scatter_3 = true;
                }
                else
                {
                    scatter_3 = !(((&h_6)->d_0[int(1)]) < 3.00000000549775576e+38f);
                }
                if(scatter_3)
                {
                    break;
                }
                if(by_rank_3)
                {
                    _S959 = float3 {(*(sorted_12 + (&h_6)->id_1[int(1)])).x, (*(sorted_12 + (&h_6)->id_1[int(1)])).y, (*(sorted_12 + (&h_6)->id_1[int(1)])).z};
                }
                else
                {
                    float3  _S961 = pca_load3_0(means_4, (&h_6)->id_1[int(1)]);
                    _S959 = _S961;
                }
                float3  d_60 = _S959 - q_10;
                float3  sd_57 = sd_56 + d_60 * d_60;
                float _S962 = d_60.x;
                float _S963 = d_60.y;
                float _S964 = d_60.z;
                float3  so_57 = so_56 + make_float3 (_S962 * _S963, _S962 * _S964, _S963 * _S964);
                s1_3 = s1_3 + d_60;
                sd_56 = sd_57;
                so_56 = so_57;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S965 = 2U >= k_15;
                _S932 = _S965;
                if(_S965)
                {
                    scatter_3 = true;
                }
                else
                {
                    scatter_3 = !(((&h_6)->d_0[int(2)]) < 3.00000000549775576e+38f);
                }
                if(scatter_3)
                {
                    break;
                }
                if(by_rank_3)
                {
                    _S959 = float3 {(*(sorted_12 + (&h_6)->id_1[int(2)])).x, (*(sorted_12 + (&h_6)->id_1[int(2)])).y, (*(sorted_12 + (&h_6)->id_1[int(2)])).z};
                }
                else
                {
                    float3  _S966 = pca_load3_0(means_4, (&h_6)->id_1[int(2)]);
                    _S959 = _S966;
                }
                float3  d_61 = _S959 - q_10;
                float3  sd_58 = sd_56 + d_61 * d_61;
                float _S967 = d_61.x;
                float _S968 = d_61.y;
                float _S969 = d_61.z;
                float3  so_58 = so_56 + make_float3 (_S967 * _S968, _S967 * _S969, _S968 * _S969);
                s1_3 = s1_3 + d_61;
                sd_56 = sd_58;
                so_56 = so_58;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S970 = 3U >= k_15;
                _S933 = _S970;
                if(_S970)
                {
                    scatter_3 = true;
                }
                else
                {
                    scatter_3 = !(((&h_6)->d_0[int(3)]) < 3.00000000549775576e+38f);
                }
                if(scatter_3)
                {
                    break;
                }
                if(by_rank_3)
                {
                    _S959 = float3 {(*(sorted_12 + (&h_6)->id_1[int(3)])).x, (*(sorted_12 + (&h_6)->id_1[int(3)])).y, (*(sorted_12 + (&h_6)->id_1[int(3)])).z};
                }
                else
                {
                    float3  _S971 = pca_load3_0(means_4, (&h_6)->id_1[int(3)]);
                    _S959 = _S971;
                }
                float3  d_62 = _S959 - q_10;
                float3  sd_59 = sd_56 + d_62 * d_62;
                float _S972 = d_62.x;
                float _S973 = d_62.y;
                float _S974 = d_62.z;
                float3  so_59 = so_56 + make_float3 (_S972 * _S973, _S972 * _S974, _S973 * _S974);
                s1_3 = s1_3 + d_62;
                sd_56 = sd_59;
                so_56 = so_59;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S975 = 4U >= k_15;
                _S934 = _S975;
                if(_S975)
                {
                    scatter_3 = true;
                }
                else
                {
                    scatter_3 = !(((&h_6)->d_0[int(4)]) < 3.00000000549775576e+38f);
                }
                if(scatter_3)
                {
                    break;
                }
                if(by_rank_3)
                {
                    _S959 = float3 {(*(sorted_12 + (&h_6)->id_1[int(4)])).x, (*(sorted_12 + (&h_6)->id_1[int(4)])).y, (*(sorted_12 + (&h_6)->id_1[int(4)])).z};
                }
                else
                {
                    float3  _S976 = pca_load3_0(means_4, (&h_6)->id_1[int(4)]);
                    _S959 = _S976;
                }
                float3  d_63 = _S959 - q_10;
                float3  sd_60 = sd_56 + d_63 * d_63;
                float _S977 = d_63.x;
                float _S978 = d_63.y;
                float _S979 = d_63.z;
                float3  so_60 = so_56 + make_float3 (_S977 * _S978, _S977 * _S979, _S978 * _S979);
                s1_3 = s1_3 + d_63;
                sd_56 = sd_60;
                so_56 = so_60;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S980 = 5U >= k_15;
                _S935 = _S980;
                if(_S980)
                {
                    scatter_3 = true;
                }
                else
                {
                    scatter_3 = !(((&h_6)->d_0[int(5)]) < 3.00000000549775576e+38f);
                }
                if(scatter_3)
                {
                    break;
                }
                if(by_rank_3)
                {
                    _S959 = float3 {(*(sorted_12 + (&h_6)->id_1[int(5)])).x, (*(sorted_12 + (&h_6)->id_1[int(5)])).y, (*(sorted_12 + (&h_6)->id_1[int(5)])).z};
                }
                else
                {
                    float3  _S981 = pca_load3_0(means_4, (&h_6)->id_1[int(5)]);
                    _S959 = _S981;
                }
                float3  d_64 = _S959 - q_10;
                float3  sd_61 = sd_56 + d_64 * d_64;
                float _S982 = d_64.x;
                float _S983 = d_64.y;
                float _S984 = d_64.z;
                float3  so_61 = so_56 + make_float3 (_S982 * _S983, _S982 * _S984, _S983 * _S984);
                s1_3 = s1_3 + d_64;
                sd_56 = sd_61;
                so_56 = so_61;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S985 = 6U >= k_15;
                _S936 = _S985;
                if(_S985)
                {
                    scatter_3 = true;
                }
                else
                {
                    scatter_3 = !(((&h_6)->d_0[int(6)]) < 3.00000000549775576e+38f);
                }
                if(scatter_3)
                {
                    break;
                }
                if(by_rank_3)
                {
                    _S959 = float3 {(*(sorted_12 + (&h_6)->id_1[int(6)])).x, (*(sorted_12 + (&h_6)->id_1[int(6)])).y, (*(sorted_12 + (&h_6)->id_1[int(6)])).z};
                }
                else
                {
                    float3  _S986 = pca_load3_0(means_4, (&h_6)->id_1[int(6)]);
                    _S959 = _S986;
                }
                float3  d_65 = _S959 - q_10;
                float3  sd_62 = sd_56 + d_65 * d_65;
                float _S987 = d_65.x;
                float _S988 = d_65.y;
                float _S989 = d_65.z;
                float3  so_62 = so_56 + make_float3 (_S987 * _S988, _S987 * _S989, _S988 * _S989);
                s1_3 = s1_3 + d_65;
                sd_56 = sd_62;
                so_56 = so_62;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S990 = 7U >= k_15;
                _S937 = _S990;
                if(_S990)
                {
                    scatter_3 = true;
                }
                else
                {
                    scatter_3 = !(((&h_6)->d_0[int(7)]) < 3.00000000549775576e+38f);
                }
                if(scatter_3)
                {
                    break;
                }
                if(by_rank_3)
                {
                    _S959 = float3 {(*(sorted_12 + (&h_6)->id_1[int(7)])).x, (*(sorted_12 + (&h_6)->id_1[int(7)])).y, (*(sorted_12 + (&h_6)->id_1[int(7)])).z};
                }
                else
                {
                    float3  _S991 = pca_load3_0(means_4, (&h_6)->id_1[int(7)]);
                    _S959 = _S991;
                }
                float3  d_66 = _S959 - q_10;
                float3  sd_63 = sd_56 + d_66 * d_66;
                float _S992 = d_66.x;
                float _S993 = d_66.y;
                float _S994 = d_66.z;
                float3  so_63 = so_56 + make_float3 (_S992 * _S993, _S992 * _S994, _S993 * _S994);
                s1_3 = s1_3 + d_66;
                sd_56 = sd_63;
                so_56 = so_63;
                break;
            }
            break;
        }
        break;
    }
    float inv_k_3 = 1.0f / float(_S942 + 1U);
    float3  mean_3 = s1_3 * make_float3 (inv_k_3);
    float _S995 = mean_3.x;
    float _S996 = mean_3.y;
    float _S997 = mean_3.z;
    float3  ev_4;
    float3  e0_4;
    float3  e1_4;
    float3  e2_4;
    pca_sym3_eigen_0(sd_56 * make_float3 (inv_k_3) - mean_3 * mean_3, so_56 * make_float3 (inv_k_3) - make_float3 (_S995 * _S996, _S995 * _S997, _S996 * _S997), &ev_4, &e0_4, &e1_4, &e2_4);
    if(!all_0(isfinite_0(ev_4)))
    {
        scatter_3 = true;
    }
    else
    {
        scatter_3 = !all_0(isfinite_0(e0_4));
    }
    if(scatter_3)
    {
        return _S951;
    }
    float t_4 = ev_4.x + ev_4.y + ev_4.z;
    float _S998;
    if(t_4 > (9.999999960041972e-13f * dot_0(q_10, q_10) + 1.00000000317107685e-30f))
    {
        _S998 = 1.0f / t_4;
    }
    else
    {
        _S998 = 0.0f;
    }
    float g_0 = 2.0f * inv_k_3 * _S998;
    Matrix<float, 3, 3>  _S999 = makeMatrix<float, 3, 3> (g_0 * w_plane_3) * makeMatrix<float, 3, 3> (make_float3 (e0_4.x) * e0_4, make_float3 (e0_4.y) * e0_4, make_float3 (e0_4.z) * e0_4) + makeMatrix<float, 3, 3> (g_0 * w_tangent_3) * (makeMatrix<float, 3, 3> (make_float3 (e2_4.x) * e2_4, make_float3 (e2_4.y) * e2_4, make_float3 (e2_4.z) * e2_4) - makeMatrix<float, 3, 3> (make_float3 (e1_4.x) * e1_4, make_float3 (e1_4.y) * e1_4, make_float3 (e1_4.z) * e1_4));
    float3  v_62 = mul_0(_S999, - mean_3);
    if(w_plane_3 != 0.0f)
    {
        scatter_3 = true;
    }
    else
    {
        scatter_3 = w_tangent_3 != 0.0f;
    }
    bool _S1000;
    if(scatter_3)
    {
        if(gate_means_3 == 0U)
        {
            _S1000 = true;
        }
        else
        {
            _S1000 = (*(radii_3 + self_6)) > 0.0f;
        }
    }
    else
    {
        _S1000 = false;
    }
    if(_S1000)
    {
        float * _S1001 = v_means_3 + 3U * self_6;
        float _S1002 = atomicAdd(_S1001 + int(0), v_62.x);
        float _S1003 = atomicAdd(_S1001 + int(1) + int(0), v_62.y);
        float _S1004 = atomicAdd(_S1001 + int(2) + int(0), v_62.z);
    }
    for(;;)
    {
        for(;;)
        {
            for(;;)
            {
                bool _S1005 = !scatter_3;
                _S938 = _S1005;
                if(_S1005)
                {
                    scatter_3 = true;
                }
                else
                {
                    scatter_3 = _S930;
                }
                if(scatter_3)
                {
                    scatter_3 = true;
                }
                else
                {
                    scatter_3 = !(((&h_6)->d_0[int(0)]) < 3.00000000549775576e+38f);
                }
                if(scatter_3)
                {
                    break;
                }
                if(by_rank_3)
                {
                    axis_5 = uint(*(perm_4 + (&h_6)->id_1[int(0)]));
                }
                else
                {
                    axis_5 = (&h_6)->id_1[int(0)];
                }
                if(gate_means_3 != 0U)
                {
                    scatter_3 = !((*(radii_3 + axis_5)) > 0.0f);
                }
                else
                {
                    scatter_3 = false;
                }
                if(scatter_3)
                {
                    break;
                }
                if(by_rank_3)
                {
                    s1_3 = float3 {(*(sorted_12 + (&h_6)->id_1[int(0)])).x, (*(sorted_12 + (&h_6)->id_1[int(0)])).y, (*(sorted_12 + (&h_6)->id_1[int(0)])).z};
                }
                else
                {
                    float3  _S1006 = pca_load3_0(means_4, (&h_6)->id_1[int(0)]);
                    s1_3 = _S1006;
                }
                float3  v_63 = mul_0(_S999, s1_3 - q_10 - mean_3);
                float * _S1007 = v_means_3 + 3U * axis_5;
                float _S1008 = atomicAdd(_S1007 + int(0), v_63.x);
                float _S1009 = atomicAdd(_S1007 + int(1) + int(0), v_63.y);
                float _S1010 = atomicAdd(_S1007 + int(2) + int(0), v_63.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S938)
                {
                    scatter_3 = true;
                }
                else
                {
                    scatter_3 = _S931;
                }
                if(scatter_3)
                {
                    scatter_3 = true;
                }
                else
                {
                    scatter_3 = !(((&h_6)->d_0[int(1)]) < 3.00000000549775576e+38f);
                }
                if(scatter_3)
                {
                    break;
                }
                if(by_rank_3)
                {
                    axis_5 = uint(*(perm_4 + (&h_6)->id_1[int(1)]));
                }
                else
                {
                    axis_5 = (&h_6)->id_1[int(1)];
                }
                if(gate_means_3 != 0U)
                {
                    scatter_3 = !((*(radii_3 + axis_5)) > 0.0f);
                }
                else
                {
                    scatter_3 = false;
                }
                if(scatter_3)
                {
                    break;
                }
                if(by_rank_3)
                {
                    s1_3 = float3 {(*(sorted_12 + (&h_6)->id_1[int(1)])).x, (*(sorted_12 + (&h_6)->id_1[int(1)])).y, (*(sorted_12 + (&h_6)->id_1[int(1)])).z};
                }
                else
                {
                    float3  _S1011 = pca_load3_0(means_4, (&h_6)->id_1[int(1)]);
                    s1_3 = _S1011;
                }
                float3  v_64 = mul_0(_S999, s1_3 - q_10 - mean_3);
                float * _S1012 = v_means_3 + 3U * axis_5;
                float _S1013 = atomicAdd(_S1012 + int(0), v_64.x);
                float _S1014 = atomicAdd(_S1012 + int(1) + int(0), v_64.y);
                float _S1015 = atomicAdd(_S1012 + int(2) + int(0), v_64.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S938)
                {
                    scatter_3 = true;
                }
                else
                {
                    scatter_3 = _S932;
                }
                if(scatter_3)
                {
                    scatter_3 = true;
                }
                else
                {
                    scatter_3 = !(((&h_6)->d_0[int(2)]) < 3.00000000549775576e+38f);
                }
                if(scatter_3)
                {
                    break;
                }
                if(by_rank_3)
                {
                    axis_5 = uint(*(perm_4 + (&h_6)->id_1[int(2)]));
                }
                else
                {
                    axis_5 = (&h_6)->id_1[int(2)];
                }
                if(gate_means_3 != 0U)
                {
                    scatter_3 = !((*(radii_3 + axis_5)) > 0.0f);
                }
                else
                {
                    scatter_3 = false;
                }
                if(scatter_3)
                {
                    break;
                }
                if(by_rank_3)
                {
                    s1_3 = float3 {(*(sorted_12 + (&h_6)->id_1[int(2)])).x, (*(sorted_12 + (&h_6)->id_1[int(2)])).y, (*(sorted_12 + (&h_6)->id_1[int(2)])).z};
                }
                else
                {
                    float3  _S1016 = pca_load3_0(means_4, (&h_6)->id_1[int(2)]);
                    s1_3 = _S1016;
                }
                float3  v_65 = mul_0(_S999, s1_3 - q_10 - mean_3);
                float * _S1017 = v_means_3 + 3U * axis_5;
                float _S1018 = atomicAdd(_S1017 + int(0), v_65.x);
                float _S1019 = atomicAdd(_S1017 + int(1) + int(0), v_65.y);
                float _S1020 = atomicAdd(_S1017 + int(2) + int(0), v_65.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S938)
                {
                    scatter_3 = true;
                }
                else
                {
                    scatter_3 = _S933;
                }
                if(scatter_3)
                {
                    scatter_3 = true;
                }
                else
                {
                    scatter_3 = !(((&h_6)->d_0[int(3)]) < 3.00000000549775576e+38f);
                }
                if(scatter_3)
                {
                    break;
                }
                if(by_rank_3)
                {
                    axis_5 = uint(*(perm_4 + (&h_6)->id_1[int(3)]));
                }
                else
                {
                    axis_5 = (&h_6)->id_1[int(3)];
                }
                if(gate_means_3 != 0U)
                {
                    scatter_3 = !((*(radii_3 + axis_5)) > 0.0f);
                }
                else
                {
                    scatter_3 = false;
                }
                if(scatter_3)
                {
                    break;
                }
                if(by_rank_3)
                {
                    s1_3 = float3 {(*(sorted_12 + (&h_6)->id_1[int(3)])).x, (*(sorted_12 + (&h_6)->id_1[int(3)])).y, (*(sorted_12 + (&h_6)->id_1[int(3)])).z};
                }
                else
                {
                    float3  _S1021 = pca_load3_0(means_4, (&h_6)->id_1[int(3)]);
                    s1_3 = _S1021;
                }
                float3  v_66 = mul_0(_S999, s1_3 - q_10 - mean_3);
                float * _S1022 = v_means_3 + 3U * axis_5;
                float _S1023 = atomicAdd(_S1022 + int(0), v_66.x);
                float _S1024 = atomicAdd(_S1022 + int(1) + int(0), v_66.y);
                float _S1025 = atomicAdd(_S1022 + int(2) + int(0), v_66.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S938)
                {
                    scatter_3 = true;
                }
                else
                {
                    scatter_3 = _S934;
                }
                if(scatter_3)
                {
                    scatter_3 = true;
                }
                else
                {
                    scatter_3 = !(((&h_6)->d_0[int(4)]) < 3.00000000549775576e+38f);
                }
                if(scatter_3)
                {
                    break;
                }
                if(by_rank_3)
                {
                    axis_5 = uint(*(perm_4 + (&h_6)->id_1[int(4)]));
                }
                else
                {
                    axis_5 = (&h_6)->id_1[int(4)];
                }
                if(gate_means_3 != 0U)
                {
                    scatter_3 = !((*(radii_3 + axis_5)) > 0.0f);
                }
                else
                {
                    scatter_3 = false;
                }
                if(scatter_3)
                {
                    break;
                }
                if(by_rank_3)
                {
                    s1_3 = float3 {(*(sorted_12 + (&h_6)->id_1[int(4)])).x, (*(sorted_12 + (&h_6)->id_1[int(4)])).y, (*(sorted_12 + (&h_6)->id_1[int(4)])).z};
                }
                else
                {
                    float3  _S1026 = pca_load3_0(means_4, (&h_6)->id_1[int(4)]);
                    s1_3 = _S1026;
                }
                float3  v_67 = mul_0(_S999, s1_3 - q_10 - mean_3);
                float * _S1027 = v_means_3 + 3U * axis_5;
                float _S1028 = atomicAdd(_S1027 + int(0), v_67.x);
                float _S1029 = atomicAdd(_S1027 + int(1) + int(0), v_67.y);
                float _S1030 = atomicAdd(_S1027 + int(2) + int(0), v_67.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S938)
                {
                    scatter_3 = true;
                }
                else
                {
                    scatter_3 = _S935;
                }
                if(scatter_3)
                {
                    scatter_3 = true;
                }
                else
                {
                    scatter_3 = !(((&h_6)->d_0[int(5)]) < 3.00000000549775576e+38f);
                }
                if(scatter_3)
                {
                    break;
                }
                if(by_rank_3)
                {
                    axis_5 = uint(*(perm_4 + (&h_6)->id_1[int(5)]));
                }
                else
                {
                    axis_5 = (&h_6)->id_1[int(5)];
                }
                if(gate_means_3 != 0U)
                {
                    scatter_3 = !((*(radii_3 + axis_5)) > 0.0f);
                }
                else
                {
                    scatter_3 = false;
                }
                if(scatter_3)
                {
                    break;
                }
                if(by_rank_3)
                {
                    s1_3 = float3 {(*(sorted_12 + (&h_6)->id_1[int(5)])).x, (*(sorted_12 + (&h_6)->id_1[int(5)])).y, (*(sorted_12 + (&h_6)->id_1[int(5)])).z};
                }
                else
                {
                    float3  _S1031 = pca_load3_0(means_4, (&h_6)->id_1[int(5)]);
                    s1_3 = _S1031;
                }
                float3  v_68 = mul_0(_S999, s1_3 - q_10 - mean_3);
                float * _S1032 = v_means_3 + 3U * axis_5;
                float _S1033 = atomicAdd(_S1032 + int(0), v_68.x);
                float _S1034 = atomicAdd(_S1032 + int(1) + int(0), v_68.y);
                float _S1035 = atomicAdd(_S1032 + int(2) + int(0), v_68.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S938)
                {
                    scatter_3 = true;
                }
                else
                {
                    scatter_3 = _S936;
                }
                if(scatter_3)
                {
                    scatter_3 = true;
                }
                else
                {
                    scatter_3 = !(((&h_6)->d_0[int(6)]) < 3.00000000549775576e+38f);
                }
                if(scatter_3)
                {
                    break;
                }
                if(by_rank_3)
                {
                    axis_5 = uint(*(perm_4 + (&h_6)->id_1[int(6)]));
                }
                else
                {
                    axis_5 = (&h_6)->id_1[int(6)];
                }
                if(gate_means_3 != 0U)
                {
                    scatter_3 = !((*(radii_3 + axis_5)) > 0.0f);
                }
                else
                {
                    scatter_3 = false;
                }
                if(scatter_3)
                {
                    break;
                }
                if(by_rank_3)
                {
                    s1_3 = float3 {(*(sorted_12 + (&h_6)->id_1[int(6)])).x, (*(sorted_12 + (&h_6)->id_1[int(6)])).y, (*(sorted_12 + (&h_6)->id_1[int(6)])).z};
                }
                else
                {
                    float3  _S1036 = pca_load3_0(means_4, (&h_6)->id_1[int(6)]);
                    s1_3 = _S1036;
                }
                float3  v_69 = mul_0(_S999, s1_3 - q_10 - mean_3);
                float * _S1037 = v_means_3 + 3U * axis_5;
                float _S1038 = atomicAdd(_S1037 + int(0), v_69.x);
                float _S1039 = atomicAdd(_S1037 + int(1) + int(0), v_69.y);
                float _S1040 = atomicAdd(_S1037 + int(2) + int(0), v_69.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S938)
                {
                    scatter_3 = true;
                }
                else
                {
                    scatter_3 = _S937;
                }
                if(scatter_3)
                {
                    scatter_3 = true;
                }
                else
                {
                    scatter_3 = !(((&h_6)->d_0[int(7)]) < 3.00000000549775576e+38f);
                }
                if(scatter_3)
                {
                    break;
                }
                if(by_rank_3)
                {
                    axis_5 = uint(*(perm_4 + (&h_6)->id_1[int(7)]));
                }
                else
                {
                    axis_5 = (&h_6)->id_1[int(7)];
                }
                if(gate_means_3 != 0U)
                {
                    scatter_3 = !((*(radii_3 + axis_5)) > 0.0f);
                }
                else
                {
                    scatter_3 = false;
                }
                if(scatter_3)
                {
                    break;
                }
                if(by_rank_3)
                {
                    s1_3 = float3 {(*(sorted_12 + (&h_6)->id_1[int(7)])).x, (*(sorted_12 + (&h_6)->id_1[int(7)])).y, (*(sorted_12 + (&h_6)->id_1[int(7)])).z};
                }
                else
                {
                    float3  _S1041 = pca_load3_0(means_4, (&h_6)->id_1[int(7)]);
                    s1_3 = _S1041;
                }
                float3  v_70 = mul_0(_S999, s1_3 - q_10 - mean_3);
                float * _S1042 = v_means_3 + 3U * axis_5;
                float _S1043 = atomicAdd(_S1042 + int(0), v_70.x);
                float _S1044 = atomicAdd(_S1042 + int(1) + int(0), v_70.y);
                float _S1045 = atomicAdd(_S1042 + int(2) + int(0), v_70.z);
                break;
            }
            break;
        }
        break;
    }
    float3  ls_3 = pca_load3_0(scales_3, self_6);
    float _S1046 = ls_3.x;
    float _S1047 = ls_3.y;
    if(_S1046 <= _S1047)
    {
        if(_S1046 <= (ls_3.z))
        {
            axis_5 = 0U;
        }
        else
        {
            axis_5 = 2U;
        }
    }
    else
    {
        if(_S1047 <= (ls_3.z))
        {
            axis_5 = 1U;
        }
        else
        {
            axis_5 = 2U;
        }
    }
    uint _S1048 = 4U * self_6;
    float4  quat_4 = make_float4 (*(quats_3 + _S1048), *(quats_3 + (_S1048 + 1U)), *(quats_3 + (_S1048 + 2U)), *(quats_3 + (_S1048 + 3U)));
    float align_3 = pca_axis_align_0(quat_4, e0_4, axis_5);
    if(w_normal_3 != 0.0f)
    {
        float4  _S1049 = make_float4 (0.0f);
        DiffPair_vectorx3Cfloatx2C4x3E_0 dq_3;
        (&dq_3)->primal_0 = quat_4;
        (&dq_3)->differential_0 = _S1049;
        s_bwd_pca_axis_align_0(&dq_3, e0_4, axis_5, - w_normal_3);
        uint c_9 = 0U;
        for(;;)
        {
            if(c_9 < 4U)
            {
            }
            else
            {
                break;
            }
            float * _S1050 = v_quats_3 + (_S1048 + c_9);
            *_S1050 = *_S1050 + _slang_vector_get_element(dq_3.differential_0, c_9);
            c_9 = c_9 + 1U;
        }
    }
    if(w_surfel_3 != 0.0f)
    {
        float smin_3;
        if(axis_5 == 0U)
        {
            smin_3 = _S1046;
        }
        else
        {
            if(axis_5 == 1U)
            {
                smin_3 = _S1047;
            }
            else
            {
                smin_3 = ls_3.z;
            }
        }
        float * _S1051 = v_scales_3 + (3U * self_6 + axis_5);
        *_S1051 = *_S1051 + w_surfel_3 * (F32_exp((smin_3 - (F32_max((_S1046), ((F32_max((_S1047), (ls_3.z)))))))));
    }
    return make_float3 (ev_4.x * _S998, (ev_4.z - ev_4.y) * _S998, 1.0f - align_3);
}

inline __device__ float3  pca_reg_owner_4(uint r_8, uint mode_4, uint n_9, uint k_16, uint window_4, float * means_5, float * quats_4, float * scales_4, int * perm_5, float4  * sorted_13, float4  * nodes_8, int4  * meta_8, int * ranges_8, int * nbrs_4, uint store_list_4, float * radii_4, uint gate_means_4, float * v_means_4, float * v_quats_4, float * v_scales_4, float w_plane_4, float w_tangent_4, float w_normal_4, float w_surfel_4)
{
    bool _S1052;
    bool _S1053;
    bool _S1054;
    bool _S1055;
    bool _S1056;
    bool _S1057;
    bool _S1058;
    bool _S1059;
    bool _S1060;
    bool _S1061;
    bool _S1062;
    bool _S1063;
    bool _S1064;
    bool _S1065;
    bool _S1066;
    bool _S1067;
    bool _S1068;
    PcaKnn_1 h_7;
    PcaKnn_reset_1(&h_7);
    bool by_rank_4 = mode_4 != 3U;
    uint self_7 = uint(*(perm_5 + r_8));
    float3  q_11;
    if(by_rank_4)
    {
        q_11 = float3 {(*(sorted_13 + r_8)).x, (*(sorted_13 + r_8)).y, (*(sorted_13 + r_8)).z};
    }
    else
    {
        float3  _S1069 = pca_load3_0(means_5, self_7);
        q_11 = _S1069;
    }
    int id_6;
    uint axis_6;
    bool scatter_4;
    if(mode_4 == 0U)
    {
        pca_knn_tree_1(r_8, n_9, k_16, sorted_13, nodes_8, meta_8, ranges_8, &h_7);
    }
    else
    {
        if(mode_4 == 2U)
        {
            uint half_4 = (U32_max((window_4), (k_16 + 1U))) / 2U;
            if(r_8 > half_4)
            {
                axis_6 = r_8 - half_4;
            }
            else
            {
                axis_6 = 0U;
            }
            PcaKnn_scan_1(&h_7, q_11, r_8, axis_6, (U32_min((r_8 + half_4 + 1U), (n_9))), 0U, 0U, sorted_13, k_16);
        }
        else
        {
            if(mode_4 == 1U)
            {
                if(r_8 > (k_16 / 2U))
                {
                    axis_6 = r_8 - k_16 / 2U;
                }
                else
                {
                    axis_6 = 0U;
                }
                if((axis_6 + k_16 + 1U) > n_9)
                {
                    uint _S1070 = k_16 + 1U;
                    if(n_9 > _S1070)
                    {
                        axis_6 = n_9 - _S1070;
                    }
                    else
                    {
                        axis_6 = 0U;
                    }
                }
                if(axis_6 >= r_8)
                {
                    id_6 = int(1);
                }
                else
                {
                    id_6 = int(0);
                }
                uint i_140 = axis_6 + uint(id_6);
                if(0U < k_16)
                {
                    scatter_4 = i_140 < n_9;
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    (&h_7)->d_0[int(0)] = 0.0f;
                    (&h_7)->id_1[int(0)] = i_140;
                }
                uint i_141 = axis_6 + 1U;
                if(i_141 >= r_8)
                {
                    id_6 = int(1);
                }
                else
                {
                    id_6 = int(0);
                }
                uint i_142 = i_141 + uint(id_6);
                if(1U < k_16)
                {
                    scatter_4 = i_142 < n_9;
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    (&h_7)->d_0[int(1)] = 0.0f;
                    (&h_7)->id_1[int(1)] = i_142;
                }
                uint i_143 = axis_6 + 2U;
                if(i_143 >= r_8)
                {
                    id_6 = int(1);
                }
                else
                {
                    id_6 = int(0);
                }
                uint i_144 = i_143 + uint(id_6);
                if(2U < k_16)
                {
                    scatter_4 = i_144 < n_9;
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    (&h_7)->d_0[int(2)] = 0.0f;
                    (&h_7)->id_1[int(2)] = i_144;
                }
                uint i_145 = axis_6 + 3U;
                if(i_145 >= r_8)
                {
                    id_6 = int(1);
                }
                else
                {
                    id_6 = int(0);
                }
                uint i_146 = i_145 + uint(id_6);
                if(3U < k_16)
                {
                    scatter_4 = i_146 < n_9;
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    (&h_7)->d_0[int(3)] = 0.0f;
                    (&h_7)->id_1[int(3)] = i_146;
                }
                uint i_147 = axis_6 + 4U;
                if(i_147 >= r_8)
                {
                    id_6 = int(1);
                }
                else
                {
                    id_6 = int(0);
                }
                uint i_148 = i_147 + uint(id_6);
                if(4U < k_16)
                {
                    scatter_4 = i_148 < n_9;
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    (&h_7)->d_0[int(4)] = 0.0f;
                    (&h_7)->id_1[int(4)] = i_148;
                }
                uint i_149 = axis_6 + 5U;
                if(i_149 >= r_8)
                {
                    id_6 = int(1);
                }
                else
                {
                    id_6 = int(0);
                }
                uint i_150 = i_149 + uint(id_6);
                if(5U < k_16)
                {
                    scatter_4 = i_150 < n_9;
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    (&h_7)->d_0[int(5)] = 0.0f;
                    (&h_7)->id_1[int(5)] = i_150;
                }
                uint i_151 = axis_6 + 6U;
                if(i_151 >= r_8)
                {
                    id_6 = int(1);
                }
                else
                {
                    id_6 = int(0);
                }
                uint i_152 = i_151 + uint(id_6);
                if(6U < k_16)
                {
                    scatter_4 = i_152 < n_9;
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    (&h_7)->d_0[int(6)] = 0.0f;
                    (&h_7)->id_1[int(6)] = i_152;
                }
                uint i_153 = axis_6 + 7U;
                if(i_153 >= r_8)
                {
                    id_6 = int(1);
                }
                else
                {
                    id_6 = int(0);
                }
                uint i_154 = i_153 + uint(id_6);
                if(7U < k_16)
                {
                    scatter_4 = i_154 < n_9;
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    (&h_7)->d_0[int(7)] = 0.0f;
                    (&h_7)->id_1[int(7)] = i_154;
                }
                uint i_155 = axis_6 + 8U;
                if(i_155 >= r_8)
                {
                    id_6 = int(1);
                }
                else
                {
                    id_6 = int(0);
                }
                uint i_156 = i_155 + uint(id_6);
                if(8U < k_16)
                {
                    scatter_4 = i_156 < n_9;
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    (&h_7)->d_0[int(8)] = 0.0f;
                    (&h_7)->id_1[int(8)] = i_156;
                }
                uint i_157 = axis_6 + 9U;
                if(i_157 >= r_8)
                {
                    id_6 = int(1);
                }
                else
                {
                    id_6 = int(0);
                }
                uint i_158 = i_157 + uint(id_6);
                if(9U < k_16)
                {
                    scatter_4 = i_158 < n_9;
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    (&h_7)->d_0[int(9)] = 0.0f;
                    (&h_7)->id_1[int(9)] = i_158;
                }
                uint i_159 = axis_6 + 10U;
                if(i_159 >= r_8)
                {
                    id_6 = int(1);
                }
                else
                {
                    id_6 = int(0);
                }
                uint i_160 = i_159 + uint(id_6);
                if(10U < k_16)
                {
                    scatter_4 = i_160 < n_9;
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    (&h_7)->d_0[int(10)] = 0.0f;
                    (&h_7)->id_1[int(10)] = i_160;
                }
                uint i_161 = axis_6 + 11U;
                if(i_161 >= r_8)
                {
                    id_6 = int(1);
                }
                else
                {
                    id_6 = int(0);
                }
                uint i_162 = i_161 + uint(id_6);
                if(11U < k_16)
                {
                    scatter_4 = i_162 < n_9;
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    (&h_7)->d_0[int(11)] = 0.0f;
                    (&h_7)->id_1[int(11)] = i_162;
                }
                uint i_163 = axis_6 + 12U;
                if(i_163 >= r_8)
                {
                    id_6 = int(1);
                }
                else
                {
                    id_6 = int(0);
                }
                uint i_164 = i_163 + uint(id_6);
                if(12U < k_16)
                {
                    scatter_4 = i_164 < n_9;
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    (&h_7)->d_0[int(12)] = 0.0f;
                    (&h_7)->id_1[int(12)] = i_164;
                }
                uint i_165 = axis_6 + 13U;
                if(i_165 >= r_8)
                {
                    id_6 = int(1);
                }
                else
                {
                    id_6 = int(0);
                }
                uint i_166 = i_165 + uint(id_6);
                if(13U < k_16)
                {
                    scatter_4 = i_166 < n_9;
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    (&h_7)->d_0[int(13)] = 0.0f;
                    (&h_7)->id_1[int(13)] = i_166;
                }
                uint i_167 = axis_6 + 14U;
                if(i_167 >= r_8)
                {
                    id_6 = int(1);
                }
                else
                {
                    id_6 = int(0);
                }
                uint i_168 = i_167 + uint(id_6);
                if(14U < k_16)
                {
                    scatter_4 = i_168 < n_9;
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    (&h_7)->d_0[int(14)] = 0.0f;
                    (&h_7)->id_1[int(14)] = i_168;
                }
                uint i_169 = axis_6 + 15U;
                if(i_169 >= r_8)
                {
                    id_6 = int(1);
                }
                else
                {
                    id_6 = int(0);
                }
                uint i_170 = i_169 + uint(id_6);
                if(15U < k_16)
                {
                    scatter_4 = i_170 < n_9;
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    (&h_7)->d_0[int(15)] = 0.0f;
                    (&h_7)->id_1[int(15)] = i_170;
                }
            }
            else
            {
                if(0U < k_16)
                {
                    id_6 = *(nbrs_4 + r_8 * k_16);
                }
                else
                {
                    id_6 = int(-1);
                }
                if(id_6 >= int(0))
                {
                    (&h_7)->d_0[int(0)] = 0.0f;
                    (&h_7)->id_1[int(0)] = uint(id_6);
                }
                if(1U < k_16)
                {
                    id_6 = *(nbrs_4 + (r_8 * k_16 + 1U));
                }
                else
                {
                    id_6 = int(-1);
                }
                if(id_6 >= int(0))
                {
                    (&h_7)->d_0[int(1)] = 0.0f;
                    (&h_7)->id_1[int(1)] = uint(id_6);
                }
                if(2U < k_16)
                {
                    id_6 = *(nbrs_4 + (r_8 * k_16 + 2U));
                }
                else
                {
                    id_6 = int(-1);
                }
                if(id_6 >= int(0))
                {
                    (&h_7)->d_0[int(2)] = 0.0f;
                    (&h_7)->id_1[int(2)] = uint(id_6);
                }
                if(3U < k_16)
                {
                    id_6 = *(nbrs_4 + (r_8 * k_16 + 3U));
                }
                else
                {
                    id_6 = int(-1);
                }
                if(id_6 >= int(0))
                {
                    (&h_7)->d_0[int(3)] = 0.0f;
                    (&h_7)->id_1[int(3)] = uint(id_6);
                }
                if(4U < k_16)
                {
                    id_6 = *(nbrs_4 + (r_8 * k_16 + 4U));
                }
                else
                {
                    id_6 = int(-1);
                }
                if(id_6 >= int(0))
                {
                    (&h_7)->d_0[int(4)] = 0.0f;
                    (&h_7)->id_1[int(4)] = uint(id_6);
                }
                if(5U < k_16)
                {
                    id_6 = *(nbrs_4 + (r_8 * k_16 + 5U));
                }
                else
                {
                    id_6 = int(-1);
                }
                if(id_6 >= int(0))
                {
                    (&h_7)->d_0[int(5)] = 0.0f;
                    (&h_7)->id_1[int(5)] = uint(id_6);
                }
                if(6U < k_16)
                {
                    id_6 = *(nbrs_4 + (r_8 * k_16 + 6U));
                }
                else
                {
                    id_6 = int(-1);
                }
                if(id_6 >= int(0))
                {
                    (&h_7)->d_0[int(6)] = 0.0f;
                    (&h_7)->id_1[int(6)] = uint(id_6);
                }
                if(7U < k_16)
                {
                    id_6 = *(nbrs_4 + (r_8 * k_16 + 7U));
                }
                else
                {
                    id_6 = int(-1);
                }
                if(id_6 >= int(0))
                {
                    (&h_7)->d_0[int(7)] = 0.0f;
                    (&h_7)->id_1[int(7)] = uint(id_6);
                }
                if(8U < k_16)
                {
                    id_6 = *(nbrs_4 + (r_8 * k_16 + 8U));
                }
                else
                {
                    id_6 = int(-1);
                }
                if(id_6 >= int(0))
                {
                    (&h_7)->d_0[int(8)] = 0.0f;
                    (&h_7)->id_1[int(8)] = uint(id_6);
                }
                if(9U < k_16)
                {
                    id_6 = *(nbrs_4 + (r_8 * k_16 + 9U));
                }
                else
                {
                    id_6 = int(-1);
                }
                if(id_6 >= int(0))
                {
                    (&h_7)->d_0[int(9)] = 0.0f;
                    (&h_7)->id_1[int(9)] = uint(id_6);
                }
                if(10U < k_16)
                {
                    id_6 = *(nbrs_4 + (r_8 * k_16 + 10U));
                }
                else
                {
                    id_6 = int(-1);
                }
                if(id_6 >= int(0))
                {
                    (&h_7)->d_0[int(10)] = 0.0f;
                    (&h_7)->id_1[int(10)] = uint(id_6);
                }
                if(11U < k_16)
                {
                    id_6 = *(nbrs_4 + (r_8 * k_16 + 11U));
                }
                else
                {
                    id_6 = int(-1);
                }
                if(id_6 >= int(0))
                {
                    (&h_7)->d_0[int(11)] = 0.0f;
                    (&h_7)->id_1[int(11)] = uint(id_6);
                }
                if(12U < k_16)
                {
                    id_6 = *(nbrs_4 + (r_8 * k_16 + 12U));
                }
                else
                {
                    id_6 = int(-1);
                }
                if(id_6 >= int(0))
                {
                    (&h_7)->d_0[int(12)] = 0.0f;
                    (&h_7)->id_1[int(12)] = uint(id_6);
                }
                if(13U < k_16)
                {
                    id_6 = *(nbrs_4 + (r_8 * k_16 + 13U));
                }
                else
                {
                    id_6 = int(-1);
                }
                if(id_6 >= int(0))
                {
                    (&h_7)->d_0[int(13)] = 0.0f;
                    (&h_7)->id_1[int(13)] = uint(id_6);
                }
                if(14U < k_16)
                {
                    id_6 = *(nbrs_4 + (r_8 * k_16 + 14U));
                }
                else
                {
                    id_6 = int(-1);
                }
                if(id_6 >= int(0))
                {
                    (&h_7)->d_0[int(14)] = 0.0f;
                    (&h_7)->id_1[int(14)] = uint(id_6);
                }
                if(15U < k_16)
                {
                    id_6 = *(nbrs_4 + (r_8 * k_16 + 15U));
                }
                else
                {
                    id_6 = int(-1);
                }
                if(id_6 >= int(0))
                {
                    (&h_7)->d_0[int(15)] = 0.0f;
                    (&h_7)->id_1[int(15)] = uint(id_6);
                }
            }
        }
    }
    PcaKnn_1 _S1071 = h_7;
    uint _S1072 = PcaKnn_count_1(&_S1071, k_16);
    if(store_list_4 != 0U)
    {
        if(0U < k_16)
        {
            int * _S1073 = nbrs_4 + r_8 * k_16;
            if(((&h_7)->d_0[int(0)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_4)
                {
                    id_6 = *(perm_5 + (&h_7)->id_1[int(0)]);
                }
                else
                {
                    id_6 = int((&h_7)->id_1[int(0)]);
                }
            }
            else
            {
                id_6 = int(-1);
            }
            *_S1073 = id_6;
        }
        if(1U < k_16)
        {
            int * _S1074 = nbrs_4 + (r_8 * k_16 + 1U);
            if(((&h_7)->d_0[int(1)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_4)
                {
                    id_6 = *(perm_5 + (&h_7)->id_1[int(1)]);
                }
                else
                {
                    id_6 = int((&h_7)->id_1[int(1)]);
                }
            }
            else
            {
                id_6 = int(-1);
            }
            *_S1074 = id_6;
        }
        if(2U < k_16)
        {
            int * _S1075 = nbrs_4 + (r_8 * k_16 + 2U);
            if(((&h_7)->d_0[int(2)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_4)
                {
                    id_6 = *(perm_5 + (&h_7)->id_1[int(2)]);
                }
                else
                {
                    id_6 = int((&h_7)->id_1[int(2)]);
                }
            }
            else
            {
                id_6 = int(-1);
            }
            *_S1075 = id_6;
        }
        if(3U < k_16)
        {
            int * _S1076 = nbrs_4 + (r_8 * k_16 + 3U);
            if(((&h_7)->d_0[int(3)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_4)
                {
                    id_6 = *(perm_5 + (&h_7)->id_1[int(3)]);
                }
                else
                {
                    id_6 = int((&h_7)->id_1[int(3)]);
                }
            }
            else
            {
                id_6 = int(-1);
            }
            *_S1076 = id_6;
        }
        if(4U < k_16)
        {
            int * _S1077 = nbrs_4 + (r_8 * k_16 + 4U);
            if(((&h_7)->d_0[int(4)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_4)
                {
                    id_6 = *(perm_5 + (&h_7)->id_1[int(4)]);
                }
                else
                {
                    id_6 = int((&h_7)->id_1[int(4)]);
                }
            }
            else
            {
                id_6 = int(-1);
            }
            *_S1077 = id_6;
        }
        if(5U < k_16)
        {
            int * _S1078 = nbrs_4 + (r_8 * k_16 + 5U);
            if(((&h_7)->d_0[int(5)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_4)
                {
                    id_6 = *(perm_5 + (&h_7)->id_1[int(5)]);
                }
                else
                {
                    id_6 = int((&h_7)->id_1[int(5)]);
                }
            }
            else
            {
                id_6 = int(-1);
            }
            *_S1078 = id_6;
        }
        if(6U < k_16)
        {
            int * _S1079 = nbrs_4 + (r_8 * k_16 + 6U);
            if(((&h_7)->d_0[int(6)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_4)
                {
                    id_6 = *(perm_5 + (&h_7)->id_1[int(6)]);
                }
                else
                {
                    id_6 = int((&h_7)->id_1[int(6)]);
                }
            }
            else
            {
                id_6 = int(-1);
            }
            *_S1079 = id_6;
        }
        if(7U < k_16)
        {
            int * _S1080 = nbrs_4 + (r_8 * k_16 + 7U);
            if(((&h_7)->d_0[int(7)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_4)
                {
                    id_6 = *(perm_5 + (&h_7)->id_1[int(7)]);
                }
                else
                {
                    id_6 = int((&h_7)->id_1[int(7)]);
                }
            }
            else
            {
                id_6 = int(-1);
            }
            *_S1080 = id_6;
        }
        if(8U < k_16)
        {
            int * _S1081 = nbrs_4 + (r_8 * k_16 + 8U);
            if(((&h_7)->d_0[int(8)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_4)
                {
                    id_6 = *(perm_5 + (&h_7)->id_1[int(8)]);
                }
                else
                {
                    id_6 = int((&h_7)->id_1[int(8)]);
                }
            }
            else
            {
                id_6 = int(-1);
            }
            *_S1081 = id_6;
        }
        if(9U < k_16)
        {
            int * _S1082 = nbrs_4 + (r_8 * k_16 + 9U);
            if(((&h_7)->d_0[int(9)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_4)
                {
                    id_6 = *(perm_5 + (&h_7)->id_1[int(9)]);
                }
                else
                {
                    id_6 = int((&h_7)->id_1[int(9)]);
                }
            }
            else
            {
                id_6 = int(-1);
            }
            *_S1082 = id_6;
        }
        if(10U < k_16)
        {
            int * _S1083 = nbrs_4 + (r_8 * k_16 + 10U);
            if(((&h_7)->d_0[int(10)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_4)
                {
                    id_6 = *(perm_5 + (&h_7)->id_1[int(10)]);
                }
                else
                {
                    id_6 = int((&h_7)->id_1[int(10)]);
                }
            }
            else
            {
                id_6 = int(-1);
            }
            *_S1083 = id_6;
        }
        if(11U < k_16)
        {
            int * _S1084 = nbrs_4 + (r_8 * k_16 + 11U);
            if(((&h_7)->d_0[int(11)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_4)
                {
                    id_6 = *(perm_5 + (&h_7)->id_1[int(11)]);
                }
                else
                {
                    id_6 = int((&h_7)->id_1[int(11)]);
                }
            }
            else
            {
                id_6 = int(-1);
            }
            *_S1084 = id_6;
        }
        if(12U < k_16)
        {
            int * _S1085 = nbrs_4 + (r_8 * k_16 + 12U);
            if(((&h_7)->d_0[int(12)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_4)
                {
                    id_6 = *(perm_5 + (&h_7)->id_1[int(12)]);
                }
                else
                {
                    id_6 = int((&h_7)->id_1[int(12)]);
                }
            }
            else
            {
                id_6 = int(-1);
            }
            *_S1085 = id_6;
        }
        if(13U < k_16)
        {
            int * _S1086 = nbrs_4 + (r_8 * k_16 + 13U);
            if(((&h_7)->d_0[int(13)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_4)
                {
                    id_6 = *(perm_5 + (&h_7)->id_1[int(13)]);
                }
                else
                {
                    id_6 = int((&h_7)->id_1[int(13)]);
                }
            }
            else
            {
                id_6 = int(-1);
            }
            *_S1086 = id_6;
        }
        if(14U < k_16)
        {
            int * _S1087 = nbrs_4 + (r_8 * k_16 + 14U);
            if(((&h_7)->d_0[int(14)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_4)
                {
                    id_6 = *(perm_5 + (&h_7)->id_1[int(14)]);
                }
                else
                {
                    id_6 = int((&h_7)->id_1[int(14)]);
                }
            }
            else
            {
                id_6 = int(-1);
            }
            *_S1087 = id_6;
        }
        if(15U < k_16)
        {
            int * _S1088 = nbrs_4 + (r_8 * k_16 + 15U);
            if(((&h_7)->d_0[int(15)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_4)
                {
                    id_6 = *(perm_5 + (&h_7)->id_1[int(15)]);
                }
                else
                {
                    id_6 = int((&h_7)->id_1[int(15)]);
                }
            }
            else
            {
                id_6 = int(-1);
            }
            *_S1088 = id_6;
        }
    }
    if(_S1072 < 2U)
    {
        return make_float3 (0.0f);
    }
    float3  so_64;
    float3  sd_64;
    float3  s1_4;
    float3  _S1089 = make_float3 (0.0f);
    for(;;)
    {
        for(;;)
        {
            for(;;)
            {
                bool _S1090 = 0U >= k_16;
                _S1052 = _S1090;
                if(_S1090)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(0)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    s1_4 = _S1089;
                    sd_64 = _S1089;
                    so_64 = _S1089;
                    break;
                }
                if(by_rank_4)
                {
                    s1_4 = float3 {(*(sorted_13 + (&h_7)->id_1[int(0)])).x, (*(sorted_13 + (&h_7)->id_1[int(0)])).y, (*(sorted_13 + (&h_7)->id_1[int(0)])).z};
                }
                else
                {
                    float3  _S1091 = pca_load3_0(means_5, (&h_7)->id_1[int(0)]);
                    s1_4 = _S1091;
                }
                float3  d_67 = s1_4 - q_11;
                float3  _S1092 = d_67 * d_67;
                float _S1093 = d_67.x;
                float _S1094 = d_67.y;
                float _S1095 = d_67.z;
                float3  _S1096 = make_float3 (_S1093 * _S1094, _S1093 * _S1095, _S1094 * _S1095);
                s1_4 = d_67;
                sd_64 = _S1092;
                so_64 = _S1096;
                break;
            }
            break;
        }
        float3  _S1097;
        for(;;)
        {
            for(;;)
            {
                bool _S1098 = 1U >= k_16;
                _S1053 = _S1098;
                if(_S1098)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(1)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    _S1097 = float3 {(*(sorted_13 + (&h_7)->id_1[int(1)])).x, (*(sorted_13 + (&h_7)->id_1[int(1)])).y, (*(sorted_13 + (&h_7)->id_1[int(1)])).z};
                }
                else
                {
                    float3  _S1099 = pca_load3_0(means_5, (&h_7)->id_1[int(1)]);
                    _S1097 = _S1099;
                }
                float3  d_68 = _S1097 - q_11;
                float3  sd_65 = sd_64 + d_68 * d_68;
                float _S1100 = d_68.x;
                float _S1101 = d_68.y;
                float _S1102 = d_68.z;
                float3  so_65 = so_64 + make_float3 (_S1100 * _S1101, _S1100 * _S1102, _S1101 * _S1102);
                s1_4 = s1_4 + d_68;
                sd_64 = sd_65;
                so_64 = so_65;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1103 = 2U >= k_16;
                _S1054 = _S1103;
                if(_S1103)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(2)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    _S1097 = float3 {(*(sorted_13 + (&h_7)->id_1[int(2)])).x, (*(sorted_13 + (&h_7)->id_1[int(2)])).y, (*(sorted_13 + (&h_7)->id_1[int(2)])).z};
                }
                else
                {
                    float3  _S1104 = pca_load3_0(means_5, (&h_7)->id_1[int(2)]);
                    _S1097 = _S1104;
                }
                float3  d_69 = _S1097 - q_11;
                float3  sd_66 = sd_64 + d_69 * d_69;
                float _S1105 = d_69.x;
                float _S1106 = d_69.y;
                float _S1107 = d_69.z;
                float3  so_66 = so_64 + make_float3 (_S1105 * _S1106, _S1105 * _S1107, _S1106 * _S1107);
                s1_4 = s1_4 + d_69;
                sd_64 = sd_66;
                so_64 = so_66;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1108 = 3U >= k_16;
                _S1055 = _S1108;
                if(_S1108)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(3)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    _S1097 = float3 {(*(sorted_13 + (&h_7)->id_1[int(3)])).x, (*(sorted_13 + (&h_7)->id_1[int(3)])).y, (*(sorted_13 + (&h_7)->id_1[int(3)])).z};
                }
                else
                {
                    float3  _S1109 = pca_load3_0(means_5, (&h_7)->id_1[int(3)]);
                    _S1097 = _S1109;
                }
                float3  d_70 = _S1097 - q_11;
                float3  sd_67 = sd_64 + d_70 * d_70;
                float _S1110 = d_70.x;
                float _S1111 = d_70.y;
                float _S1112 = d_70.z;
                float3  so_67 = so_64 + make_float3 (_S1110 * _S1111, _S1110 * _S1112, _S1111 * _S1112);
                s1_4 = s1_4 + d_70;
                sd_64 = sd_67;
                so_64 = so_67;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1113 = 4U >= k_16;
                _S1056 = _S1113;
                if(_S1113)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(4)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    _S1097 = float3 {(*(sorted_13 + (&h_7)->id_1[int(4)])).x, (*(sorted_13 + (&h_7)->id_1[int(4)])).y, (*(sorted_13 + (&h_7)->id_1[int(4)])).z};
                }
                else
                {
                    float3  _S1114 = pca_load3_0(means_5, (&h_7)->id_1[int(4)]);
                    _S1097 = _S1114;
                }
                float3  d_71 = _S1097 - q_11;
                float3  sd_68 = sd_64 + d_71 * d_71;
                float _S1115 = d_71.x;
                float _S1116 = d_71.y;
                float _S1117 = d_71.z;
                float3  so_68 = so_64 + make_float3 (_S1115 * _S1116, _S1115 * _S1117, _S1116 * _S1117);
                s1_4 = s1_4 + d_71;
                sd_64 = sd_68;
                so_64 = so_68;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1118 = 5U >= k_16;
                _S1057 = _S1118;
                if(_S1118)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(5)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    _S1097 = float3 {(*(sorted_13 + (&h_7)->id_1[int(5)])).x, (*(sorted_13 + (&h_7)->id_1[int(5)])).y, (*(sorted_13 + (&h_7)->id_1[int(5)])).z};
                }
                else
                {
                    float3  _S1119 = pca_load3_0(means_5, (&h_7)->id_1[int(5)]);
                    _S1097 = _S1119;
                }
                float3  d_72 = _S1097 - q_11;
                float3  sd_69 = sd_64 + d_72 * d_72;
                float _S1120 = d_72.x;
                float _S1121 = d_72.y;
                float _S1122 = d_72.z;
                float3  so_69 = so_64 + make_float3 (_S1120 * _S1121, _S1120 * _S1122, _S1121 * _S1122);
                s1_4 = s1_4 + d_72;
                sd_64 = sd_69;
                so_64 = so_69;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1123 = 6U >= k_16;
                _S1058 = _S1123;
                if(_S1123)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(6)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    _S1097 = float3 {(*(sorted_13 + (&h_7)->id_1[int(6)])).x, (*(sorted_13 + (&h_7)->id_1[int(6)])).y, (*(sorted_13 + (&h_7)->id_1[int(6)])).z};
                }
                else
                {
                    float3  _S1124 = pca_load3_0(means_5, (&h_7)->id_1[int(6)]);
                    _S1097 = _S1124;
                }
                float3  d_73 = _S1097 - q_11;
                float3  sd_70 = sd_64 + d_73 * d_73;
                float _S1125 = d_73.x;
                float _S1126 = d_73.y;
                float _S1127 = d_73.z;
                float3  so_70 = so_64 + make_float3 (_S1125 * _S1126, _S1125 * _S1127, _S1126 * _S1127);
                s1_4 = s1_4 + d_73;
                sd_64 = sd_70;
                so_64 = so_70;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1128 = 7U >= k_16;
                _S1059 = _S1128;
                if(_S1128)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(7)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    _S1097 = float3 {(*(sorted_13 + (&h_7)->id_1[int(7)])).x, (*(sorted_13 + (&h_7)->id_1[int(7)])).y, (*(sorted_13 + (&h_7)->id_1[int(7)])).z};
                }
                else
                {
                    float3  _S1129 = pca_load3_0(means_5, (&h_7)->id_1[int(7)]);
                    _S1097 = _S1129;
                }
                float3  d_74 = _S1097 - q_11;
                float3  sd_71 = sd_64 + d_74 * d_74;
                float _S1130 = d_74.x;
                float _S1131 = d_74.y;
                float _S1132 = d_74.z;
                float3  so_71 = so_64 + make_float3 (_S1130 * _S1131, _S1130 * _S1132, _S1131 * _S1132);
                s1_4 = s1_4 + d_74;
                sd_64 = sd_71;
                so_64 = so_71;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1133 = 8U >= k_16;
                _S1060 = _S1133;
                if(_S1133)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(8)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    _S1097 = float3 {(*(sorted_13 + (&h_7)->id_1[int(8)])).x, (*(sorted_13 + (&h_7)->id_1[int(8)])).y, (*(sorted_13 + (&h_7)->id_1[int(8)])).z};
                }
                else
                {
                    float3  _S1134 = pca_load3_0(means_5, (&h_7)->id_1[int(8)]);
                    _S1097 = _S1134;
                }
                float3  d_75 = _S1097 - q_11;
                float3  sd_72 = sd_64 + d_75 * d_75;
                float _S1135 = d_75.x;
                float _S1136 = d_75.y;
                float _S1137 = d_75.z;
                float3  so_72 = so_64 + make_float3 (_S1135 * _S1136, _S1135 * _S1137, _S1136 * _S1137);
                s1_4 = s1_4 + d_75;
                sd_64 = sd_72;
                so_64 = so_72;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1138 = 9U >= k_16;
                _S1061 = _S1138;
                if(_S1138)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(9)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    _S1097 = float3 {(*(sorted_13 + (&h_7)->id_1[int(9)])).x, (*(sorted_13 + (&h_7)->id_1[int(9)])).y, (*(sorted_13 + (&h_7)->id_1[int(9)])).z};
                }
                else
                {
                    float3  _S1139 = pca_load3_0(means_5, (&h_7)->id_1[int(9)]);
                    _S1097 = _S1139;
                }
                float3  d_76 = _S1097 - q_11;
                float3  sd_73 = sd_64 + d_76 * d_76;
                float _S1140 = d_76.x;
                float _S1141 = d_76.y;
                float _S1142 = d_76.z;
                float3  so_73 = so_64 + make_float3 (_S1140 * _S1141, _S1140 * _S1142, _S1141 * _S1142);
                s1_4 = s1_4 + d_76;
                sd_64 = sd_73;
                so_64 = so_73;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1143 = 10U >= k_16;
                _S1062 = _S1143;
                if(_S1143)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(10)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    _S1097 = float3 {(*(sorted_13 + (&h_7)->id_1[int(10)])).x, (*(sorted_13 + (&h_7)->id_1[int(10)])).y, (*(sorted_13 + (&h_7)->id_1[int(10)])).z};
                }
                else
                {
                    float3  _S1144 = pca_load3_0(means_5, (&h_7)->id_1[int(10)]);
                    _S1097 = _S1144;
                }
                float3  d_77 = _S1097 - q_11;
                float3  sd_74 = sd_64 + d_77 * d_77;
                float _S1145 = d_77.x;
                float _S1146 = d_77.y;
                float _S1147 = d_77.z;
                float3  so_74 = so_64 + make_float3 (_S1145 * _S1146, _S1145 * _S1147, _S1146 * _S1147);
                s1_4 = s1_4 + d_77;
                sd_64 = sd_74;
                so_64 = so_74;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1148 = 11U >= k_16;
                _S1063 = _S1148;
                if(_S1148)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(11)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    _S1097 = float3 {(*(sorted_13 + (&h_7)->id_1[int(11)])).x, (*(sorted_13 + (&h_7)->id_1[int(11)])).y, (*(sorted_13 + (&h_7)->id_1[int(11)])).z};
                }
                else
                {
                    float3  _S1149 = pca_load3_0(means_5, (&h_7)->id_1[int(11)]);
                    _S1097 = _S1149;
                }
                float3  d_78 = _S1097 - q_11;
                float3  sd_75 = sd_64 + d_78 * d_78;
                float _S1150 = d_78.x;
                float _S1151 = d_78.y;
                float _S1152 = d_78.z;
                float3  so_75 = so_64 + make_float3 (_S1150 * _S1151, _S1150 * _S1152, _S1151 * _S1152);
                s1_4 = s1_4 + d_78;
                sd_64 = sd_75;
                so_64 = so_75;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1153 = 12U >= k_16;
                _S1064 = _S1153;
                if(_S1153)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(12)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    _S1097 = float3 {(*(sorted_13 + (&h_7)->id_1[int(12)])).x, (*(sorted_13 + (&h_7)->id_1[int(12)])).y, (*(sorted_13 + (&h_7)->id_1[int(12)])).z};
                }
                else
                {
                    float3  _S1154 = pca_load3_0(means_5, (&h_7)->id_1[int(12)]);
                    _S1097 = _S1154;
                }
                float3  d_79 = _S1097 - q_11;
                float3  sd_76 = sd_64 + d_79 * d_79;
                float _S1155 = d_79.x;
                float _S1156 = d_79.y;
                float _S1157 = d_79.z;
                float3  so_76 = so_64 + make_float3 (_S1155 * _S1156, _S1155 * _S1157, _S1156 * _S1157);
                s1_4 = s1_4 + d_79;
                sd_64 = sd_76;
                so_64 = so_76;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1158 = 13U >= k_16;
                _S1065 = _S1158;
                if(_S1158)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(13)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    _S1097 = float3 {(*(sorted_13 + (&h_7)->id_1[int(13)])).x, (*(sorted_13 + (&h_7)->id_1[int(13)])).y, (*(sorted_13 + (&h_7)->id_1[int(13)])).z};
                }
                else
                {
                    float3  _S1159 = pca_load3_0(means_5, (&h_7)->id_1[int(13)]);
                    _S1097 = _S1159;
                }
                float3  d_80 = _S1097 - q_11;
                float3  sd_77 = sd_64 + d_80 * d_80;
                float _S1160 = d_80.x;
                float _S1161 = d_80.y;
                float _S1162 = d_80.z;
                float3  so_77 = so_64 + make_float3 (_S1160 * _S1161, _S1160 * _S1162, _S1161 * _S1162);
                s1_4 = s1_4 + d_80;
                sd_64 = sd_77;
                so_64 = so_77;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1163 = 14U >= k_16;
                _S1066 = _S1163;
                if(_S1163)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(14)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    _S1097 = float3 {(*(sorted_13 + (&h_7)->id_1[int(14)])).x, (*(sorted_13 + (&h_7)->id_1[int(14)])).y, (*(sorted_13 + (&h_7)->id_1[int(14)])).z};
                }
                else
                {
                    float3  _S1164 = pca_load3_0(means_5, (&h_7)->id_1[int(14)]);
                    _S1097 = _S1164;
                }
                float3  d_81 = _S1097 - q_11;
                float3  sd_78 = sd_64 + d_81 * d_81;
                float _S1165 = d_81.x;
                float _S1166 = d_81.y;
                float _S1167 = d_81.z;
                float3  so_78 = so_64 + make_float3 (_S1165 * _S1166, _S1165 * _S1167, _S1166 * _S1167);
                s1_4 = s1_4 + d_81;
                sd_64 = sd_78;
                so_64 = so_78;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1168 = 15U >= k_16;
                _S1067 = _S1168;
                if(_S1168)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(15)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    _S1097 = float3 {(*(sorted_13 + (&h_7)->id_1[int(15)])).x, (*(sorted_13 + (&h_7)->id_1[int(15)])).y, (*(sorted_13 + (&h_7)->id_1[int(15)])).z};
                }
                else
                {
                    float3  _S1169 = pca_load3_0(means_5, (&h_7)->id_1[int(15)]);
                    _S1097 = _S1169;
                }
                float3  d_82 = _S1097 - q_11;
                float3  sd_79 = sd_64 + d_82 * d_82;
                float _S1170 = d_82.x;
                float _S1171 = d_82.y;
                float _S1172 = d_82.z;
                float3  so_79 = so_64 + make_float3 (_S1170 * _S1171, _S1170 * _S1172, _S1171 * _S1172);
                s1_4 = s1_4 + d_82;
                sd_64 = sd_79;
                so_64 = so_79;
                break;
            }
            break;
        }
        break;
    }
    float inv_k_4 = 1.0f / float(_S1072 + 1U);
    float3  mean_4 = s1_4 * make_float3 (inv_k_4);
    float _S1173 = mean_4.x;
    float _S1174 = mean_4.y;
    float _S1175 = mean_4.z;
    float3  ev_5;
    float3  e0_5;
    float3  e1_5;
    float3  e2_5;
    pca_sym3_eigen_0(sd_64 * make_float3 (inv_k_4) - mean_4 * mean_4, so_64 * make_float3 (inv_k_4) - make_float3 (_S1173 * _S1174, _S1173 * _S1175, _S1174 * _S1175), &ev_5, &e0_5, &e1_5, &e2_5);
    if(!all_0(isfinite_0(ev_5)))
    {
        scatter_4 = true;
    }
    else
    {
        scatter_4 = !all_0(isfinite_0(e0_5));
    }
    if(scatter_4)
    {
        return _S1089;
    }
    float t_5 = ev_5.x + ev_5.y + ev_5.z;
    float _S1176;
    if(t_5 > (9.999999960041972e-13f * dot_0(q_11, q_11) + 1.00000000317107685e-30f))
    {
        _S1176 = 1.0f / t_5;
    }
    else
    {
        _S1176 = 0.0f;
    }
    float g_1 = 2.0f * inv_k_4 * _S1176;
    Matrix<float, 3, 3>  _S1177 = makeMatrix<float, 3, 3> (g_1 * w_plane_4) * makeMatrix<float, 3, 3> (make_float3 (e0_5.x) * e0_5, make_float3 (e0_5.y) * e0_5, make_float3 (e0_5.z) * e0_5) + makeMatrix<float, 3, 3> (g_1 * w_tangent_4) * (makeMatrix<float, 3, 3> (make_float3 (e2_5.x) * e2_5, make_float3 (e2_5.y) * e2_5, make_float3 (e2_5.z) * e2_5) - makeMatrix<float, 3, 3> (make_float3 (e1_5.x) * e1_5, make_float3 (e1_5.y) * e1_5, make_float3 (e1_5.z) * e1_5));
    float3  v_71 = mul_0(_S1177, - mean_4);
    if(w_plane_4 != 0.0f)
    {
        scatter_4 = true;
    }
    else
    {
        scatter_4 = w_tangent_4 != 0.0f;
    }
    bool _S1178;
    if(scatter_4)
    {
        if(gate_means_4 == 0U)
        {
            _S1178 = true;
        }
        else
        {
            _S1178 = (*(radii_4 + self_7)) > 0.0f;
        }
    }
    else
    {
        _S1178 = false;
    }
    if(_S1178)
    {
        float * _S1179 = v_means_4 + 3U * self_7;
        float _S1180 = atomicAdd(_S1179 + int(0), v_71.x);
        float _S1181 = atomicAdd(_S1179 + int(1) + int(0), v_71.y);
        float _S1182 = atomicAdd(_S1179 + int(2) + int(0), v_71.z);
    }
    for(;;)
    {
        for(;;)
        {
            for(;;)
            {
                bool _S1183 = !scatter_4;
                _S1068 = _S1183;
                if(_S1183)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = _S1052;
                }
                if(scatter_4)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(0)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    axis_6 = uint(*(perm_5 + (&h_7)->id_1[int(0)]));
                }
                else
                {
                    axis_6 = (&h_7)->id_1[int(0)];
                }
                if(gate_means_4 != 0U)
                {
                    scatter_4 = !((*(radii_4 + axis_6)) > 0.0f);
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    s1_4 = float3 {(*(sorted_13 + (&h_7)->id_1[int(0)])).x, (*(sorted_13 + (&h_7)->id_1[int(0)])).y, (*(sorted_13 + (&h_7)->id_1[int(0)])).z};
                }
                else
                {
                    float3  _S1184 = pca_load3_0(means_5, (&h_7)->id_1[int(0)]);
                    s1_4 = _S1184;
                }
                float3  v_72 = mul_0(_S1177, s1_4 - q_11 - mean_4);
                float * _S1185 = v_means_4 + 3U * axis_6;
                float _S1186 = atomicAdd(_S1185 + int(0), v_72.x);
                float _S1187 = atomicAdd(_S1185 + int(1) + int(0), v_72.y);
                float _S1188 = atomicAdd(_S1185 + int(2) + int(0), v_72.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1068)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = _S1053;
                }
                if(scatter_4)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(1)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    axis_6 = uint(*(perm_5 + (&h_7)->id_1[int(1)]));
                }
                else
                {
                    axis_6 = (&h_7)->id_1[int(1)];
                }
                if(gate_means_4 != 0U)
                {
                    scatter_4 = !((*(radii_4 + axis_6)) > 0.0f);
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    s1_4 = float3 {(*(sorted_13 + (&h_7)->id_1[int(1)])).x, (*(sorted_13 + (&h_7)->id_1[int(1)])).y, (*(sorted_13 + (&h_7)->id_1[int(1)])).z};
                }
                else
                {
                    float3  _S1189 = pca_load3_0(means_5, (&h_7)->id_1[int(1)]);
                    s1_4 = _S1189;
                }
                float3  v_73 = mul_0(_S1177, s1_4 - q_11 - mean_4);
                float * _S1190 = v_means_4 + 3U * axis_6;
                float _S1191 = atomicAdd(_S1190 + int(0), v_73.x);
                float _S1192 = atomicAdd(_S1190 + int(1) + int(0), v_73.y);
                float _S1193 = atomicAdd(_S1190 + int(2) + int(0), v_73.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1068)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = _S1054;
                }
                if(scatter_4)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(2)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    axis_6 = uint(*(perm_5 + (&h_7)->id_1[int(2)]));
                }
                else
                {
                    axis_6 = (&h_7)->id_1[int(2)];
                }
                if(gate_means_4 != 0U)
                {
                    scatter_4 = !((*(radii_4 + axis_6)) > 0.0f);
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    s1_4 = float3 {(*(sorted_13 + (&h_7)->id_1[int(2)])).x, (*(sorted_13 + (&h_7)->id_1[int(2)])).y, (*(sorted_13 + (&h_7)->id_1[int(2)])).z};
                }
                else
                {
                    float3  _S1194 = pca_load3_0(means_5, (&h_7)->id_1[int(2)]);
                    s1_4 = _S1194;
                }
                float3  v_74 = mul_0(_S1177, s1_4 - q_11 - mean_4);
                float * _S1195 = v_means_4 + 3U * axis_6;
                float _S1196 = atomicAdd(_S1195 + int(0), v_74.x);
                float _S1197 = atomicAdd(_S1195 + int(1) + int(0), v_74.y);
                float _S1198 = atomicAdd(_S1195 + int(2) + int(0), v_74.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1068)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = _S1055;
                }
                if(scatter_4)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(3)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    axis_6 = uint(*(perm_5 + (&h_7)->id_1[int(3)]));
                }
                else
                {
                    axis_6 = (&h_7)->id_1[int(3)];
                }
                if(gate_means_4 != 0U)
                {
                    scatter_4 = !((*(radii_4 + axis_6)) > 0.0f);
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    s1_4 = float3 {(*(sorted_13 + (&h_7)->id_1[int(3)])).x, (*(sorted_13 + (&h_7)->id_1[int(3)])).y, (*(sorted_13 + (&h_7)->id_1[int(3)])).z};
                }
                else
                {
                    float3  _S1199 = pca_load3_0(means_5, (&h_7)->id_1[int(3)]);
                    s1_4 = _S1199;
                }
                float3  v_75 = mul_0(_S1177, s1_4 - q_11 - mean_4);
                float * _S1200 = v_means_4 + 3U * axis_6;
                float _S1201 = atomicAdd(_S1200 + int(0), v_75.x);
                float _S1202 = atomicAdd(_S1200 + int(1) + int(0), v_75.y);
                float _S1203 = atomicAdd(_S1200 + int(2) + int(0), v_75.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1068)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = _S1056;
                }
                if(scatter_4)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(4)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    axis_6 = uint(*(perm_5 + (&h_7)->id_1[int(4)]));
                }
                else
                {
                    axis_6 = (&h_7)->id_1[int(4)];
                }
                if(gate_means_4 != 0U)
                {
                    scatter_4 = !((*(radii_4 + axis_6)) > 0.0f);
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    s1_4 = float3 {(*(sorted_13 + (&h_7)->id_1[int(4)])).x, (*(sorted_13 + (&h_7)->id_1[int(4)])).y, (*(sorted_13 + (&h_7)->id_1[int(4)])).z};
                }
                else
                {
                    float3  _S1204 = pca_load3_0(means_5, (&h_7)->id_1[int(4)]);
                    s1_4 = _S1204;
                }
                float3  v_76 = mul_0(_S1177, s1_4 - q_11 - mean_4);
                float * _S1205 = v_means_4 + 3U * axis_6;
                float _S1206 = atomicAdd(_S1205 + int(0), v_76.x);
                float _S1207 = atomicAdd(_S1205 + int(1) + int(0), v_76.y);
                float _S1208 = atomicAdd(_S1205 + int(2) + int(0), v_76.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1068)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = _S1057;
                }
                if(scatter_4)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(5)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    axis_6 = uint(*(perm_5 + (&h_7)->id_1[int(5)]));
                }
                else
                {
                    axis_6 = (&h_7)->id_1[int(5)];
                }
                if(gate_means_4 != 0U)
                {
                    scatter_4 = !((*(radii_4 + axis_6)) > 0.0f);
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    s1_4 = float3 {(*(sorted_13 + (&h_7)->id_1[int(5)])).x, (*(sorted_13 + (&h_7)->id_1[int(5)])).y, (*(sorted_13 + (&h_7)->id_1[int(5)])).z};
                }
                else
                {
                    float3  _S1209 = pca_load3_0(means_5, (&h_7)->id_1[int(5)]);
                    s1_4 = _S1209;
                }
                float3  v_77 = mul_0(_S1177, s1_4 - q_11 - mean_4);
                float * _S1210 = v_means_4 + 3U * axis_6;
                float _S1211 = atomicAdd(_S1210 + int(0), v_77.x);
                float _S1212 = atomicAdd(_S1210 + int(1) + int(0), v_77.y);
                float _S1213 = atomicAdd(_S1210 + int(2) + int(0), v_77.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1068)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = _S1058;
                }
                if(scatter_4)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(6)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    axis_6 = uint(*(perm_5 + (&h_7)->id_1[int(6)]));
                }
                else
                {
                    axis_6 = (&h_7)->id_1[int(6)];
                }
                if(gate_means_4 != 0U)
                {
                    scatter_4 = !((*(radii_4 + axis_6)) > 0.0f);
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    s1_4 = float3 {(*(sorted_13 + (&h_7)->id_1[int(6)])).x, (*(sorted_13 + (&h_7)->id_1[int(6)])).y, (*(sorted_13 + (&h_7)->id_1[int(6)])).z};
                }
                else
                {
                    float3  _S1214 = pca_load3_0(means_5, (&h_7)->id_1[int(6)]);
                    s1_4 = _S1214;
                }
                float3  v_78 = mul_0(_S1177, s1_4 - q_11 - mean_4);
                float * _S1215 = v_means_4 + 3U * axis_6;
                float _S1216 = atomicAdd(_S1215 + int(0), v_78.x);
                float _S1217 = atomicAdd(_S1215 + int(1) + int(0), v_78.y);
                float _S1218 = atomicAdd(_S1215 + int(2) + int(0), v_78.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1068)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = _S1059;
                }
                if(scatter_4)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(7)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    axis_6 = uint(*(perm_5 + (&h_7)->id_1[int(7)]));
                }
                else
                {
                    axis_6 = (&h_7)->id_1[int(7)];
                }
                if(gate_means_4 != 0U)
                {
                    scatter_4 = !((*(radii_4 + axis_6)) > 0.0f);
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    s1_4 = float3 {(*(sorted_13 + (&h_7)->id_1[int(7)])).x, (*(sorted_13 + (&h_7)->id_1[int(7)])).y, (*(sorted_13 + (&h_7)->id_1[int(7)])).z};
                }
                else
                {
                    float3  _S1219 = pca_load3_0(means_5, (&h_7)->id_1[int(7)]);
                    s1_4 = _S1219;
                }
                float3  v_79 = mul_0(_S1177, s1_4 - q_11 - mean_4);
                float * _S1220 = v_means_4 + 3U * axis_6;
                float _S1221 = atomicAdd(_S1220 + int(0), v_79.x);
                float _S1222 = atomicAdd(_S1220 + int(1) + int(0), v_79.y);
                float _S1223 = atomicAdd(_S1220 + int(2) + int(0), v_79.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1068)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = _S1060;
                }
                if(scatter_4)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(8)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    axis_6 = uint(*(perm_5 + (&h_7)->id_1[int(8)]));
                }
                else
                {
                    axis_6 = (&h_7)->id_1[int(8)];
                }
                if(gate_means_4 != 0U)
                {
                    scatter_4 = !((*(radii_4 + axis_6)) > 0.0f);
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    s1_4 = float3 {(*(sorted_13 + (&h_7)->id_1[int(8)])).x, (*(sorted_13 + (&h_7)->id_1[int(8)])).y, (*(sorted_13 + (&h_7)->id_1[int(8)])).z};
                }
                else
                {
                    float3  _S1224 = pca_load3_0(means_5, (&h_7)->id_1[int(8)]);
                    s1_4 = _S1224;
                }
                float3  v_80 = mul_0(_S1177, s1_4 - q_11 - mean_4);
                float * _S1225 = v_means_4 + 3U * axis_6;
                float _S1226 = atomicAdd(_S1225 + int(0), v_80.x);
                float _S1227 = atomicAdd(_S1225 + int(1) + int(0), v_80.y);
                float _S1228 = atomicAdd(_S1225 + int(2) + int(0), v_80.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1068)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = _S1061;
                }
                if(scatter_4)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(9)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    axis_6 = uint(*(perm_5 + (&h_7)->id_1[int(9)]));
                }
                else
                {
                    axis_6 = (&h_7)->id_1[int(9)];
                }
                if(gate_means_4 != 0U)
                {
                    scatter_4 = !((*(radii_4 + axis_6)) > 0.0f);
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    s1_4 = float3 {(*(sorted_13 + (&h_7)->id_1[int(9)])).x, (*(sorted_13 + (&h_7)->id_1[int(9)])).y, (*(sorted_13 + (&h_7)->id_1[int(9)])).z};
                }
                else
                {
                    float3  _S1229 = pca_load3_0(means_5, (&h_7)->id_1[int(9)]);
                    s1_4 = _S1229;
                }
                float3  v_81 = mul_0(_S1177, s1_4 - q_11 - mean_4);
                float * _S1230 = v_means_4 + 3U * axis_6;
                float _S1231 = atomicAdd(_S1230 + int(0), v_81.x);
                float _S1232 = atomicAdd(_S1230 + int(1) + int(0), v_81.y);
                float _S1233 = atomicAdd(_S1230 + int(2) + int(0), v_81.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1068)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = _S1062;
                }
                if(scatter_4)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(10)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    axis_6 = uint(*(perm_5 + (&h_7)->id_1[int(10)]));
                }
                else
                {
                    axis_6 = (&h_7)->id_1[int(10)];
                }
                if(gate_means_4 != 0U)
                {
                    scatter_4 = !((*(radii_4 + axis_6)) > 0.0f);
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    s1_4 = float3 {(*(sorted_13 + (&h_7)->id_1[int(10)])).x, (*(sorted_13 + (&h_7)->id_1[int(10)])).y, (*(sorted_13 + (&h_7)->id_1[int(10)])).z};
                }
                else
                {
                    float3  _S1234 = pca_load3_0(means_5, (&h_7)->id_1[int(10)]);
                    s1_4 = _S1234;
                }
                float3  v_82 = mul_0(_S1177, s1_4 - q_11 - mean_4);
                float * _S1235 = v_means_4 + 3U * axis_6;
                float _S1236 = atomicAdd(_S1235 + int(0), v_82.x);
                float _S1237 = atomicAdd(_S1235 + int(1) + int(0), v_82.y);
                float _S1238 = atomicAdd(_S1235 + int(2) + int(0), v_82.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1068)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = _S1063;
                }
                if(scatter_4)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(11)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    axis_6 = uint(*(perm_5 + (&h_7)->id_1[int(11)]));
                }
                else
                {
                    axis_6 = (&h_7)->id_1[int(11)];
                }
                if(gate_means_4 != 0U)
                {
                    scatter_4 = !((*(radii_4 + axis_6)) > 0.0f);
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    s1_4 = float3 {(*(sorted_13 + (&h_7)->id_1[int(11)])).x, (*(sorted_13 + (&h_7)->id_1[int(11)])).y, (*(sorted_13 + (&h_7)->id_1[int(11)])).z};
                }
                else
                {
                    float3  _S1239 = pca_load3_0(means_5, (&h_7)->id_1[int(11)]);
                    s1_4 = _S1239;
                }
                float3  v_83 = mul_0(_S1177, s1_4 - q_11 - mean_4);
                float * _S1240 = v_means_4 + 3U * axis_6;
                float _S1241 = atomicAdd(_S1240 + int(0), v_83.x);
                float _S1242 = atomicAdd(_S1240 + int(1) + int(0), v_83.y);
                float _S1243 = atomicAdd(_S1240 + int(2) + int(0), v_83.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1068)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = _S1064;
                }
                if(scatter_4)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(12)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    axis_6 = uint(*(perm_5 + (&h_7)->id_1[int(12)]));
                }
                else
                {
                    axis_6 = (&h_7)->id_1[int(12)];
                }
                if(gate_means_4 != 0U)
                {
                    scatter_4 = !((*(radii_4 + axis_6)) > 0.0f);
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    s1_4 = float3 {(*(sorted_13 + (&h_7)->id_1[int(12)])).x, (*(sorted_13 + (&h_7)->id_1[int(12)])).y, (*(sorted_13 + (&h_7)->id_1[int(12)])).z};
                }
                else
                {
                    float3  _S1244 = pca_load3_0(means_5, (&h_7)->id_1[int(12)]);
                    s1_4 = _S1244;
                }
                float3  v_84 = mul_0(_S1177, s1_4 - q_11 - mean_4);
                float * _S1245 = v_means_4 + 3U * axis_6;
                float _S1246 = atomicAdd(_S1245 + int(0), v_84.x);
                float _S1247 = atomicAdd(_S1245 + int(1) + int(0), v_84.y);
                float _S1248 = atomicAdd(_S1245 + int(2) + int(0), v_84.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1068)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = _S1065;
                }
                if(scatter_4)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(13)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    axis_6 = uint(*(perm_5 + (&h_7)->id_1[int(13)]));
                }
                else
                {
                    axis_6 = (&h_7)->id_1[int(13)];
                }
                if(gate_means_4 != 0U)
                {
                    scatter_4 = !((*(radii_4 + axis_6)) > 0.0f);
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    s1_4 = float3 {(*(sorted_13 + (&h_7)->id_1[int(13)])).x, (*(sorted_13 + (&h_7)->id_1[int(13)])).y, (*(sorted_13 + (&h_7)->id_1[int(13)])).z};
                }
                else
                {
                    float3  _S1249 = pca_load3_0(means_5, (&h_7)->id_1[int(13)]);
                    s1_4 = _S1249;
                }
                float3  v_85 = mul_0(_S1177, s1_4 - q_11 - mean_4);
                float * _S1250 = v_means_4 + 3U * axis_6;
                float _S1251 = atomicAdd(_S1250 + int(0), v_85.x);
                float _S1252 = atomicAdd(_S1250 + int(1) + int(0), v_85.y);
                float _S1253 = atomicAdd(_S1250 + int(2) + int(0), v_85.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1068)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = _S1066;
                }
                if(scatter_4)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(14)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    axis_6 = uint(*(perm_5 + (&h_7)->id_1[int(14)]));
                }
                else
                {
                    axis_6 = (&h_7)->id_1[int(14)];
                }
                if(gate_means_4 != 0U)
                {
                    scatter_4 = !((*(radii_4 + axis_6)) > 0.0f);
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    s1_4 = float3 {(*(sorted_13 + (&h_7)->id_1[int(14)])).x, (*(sorted_13 + (&h_7)->id_1[int(14)])).y, (*(sorted_13 + (&h_7)->id_1[int(14)])).z};
                }
                else
                {
                    float3  _S1254 = pca_load3_0(means_5, (&h_7)->id_1[int(14)]);
                    s1_4 = _S1254;
                }
                float3  v_86 = mul_0(_S1177, s1_4 - q_11 - mean_4);
                float * _S1255 = v_means_4 + 3U * axis_6;
                float _S1256 = atomicAdd(_S1255 + int(0), v_86.x);
                float _S1257 = atomicAdd(_S1255 + int(1) + int(0), v_86.y);
                float _S1258 = atomicAdd(_S1255 + int(2) + int(0), v_86.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1068)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = _S1067;
                }
                if(scatter_4)
                {
                    scatter_4 = true;
                }
                else
                {
                    scatter_4 = !(((&h_7)->d_0[int(15)]) < 3.00000000549775576e+38f);
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    axis_6 = uint(*(perm_5 + (&h_7)->id_1[int(15)]));
                }
                else
                {
                    axis_6 = (&h_7)->id_1[int(15)];
                }
                if(gate_means_4 != 0U)
                {
                    scatter_4 = !((*(radii_4 + axis_6)) > 0.0f);
                }
                else
                {
                    scatter_4 = false;
                }
                if(scatter_4)
                {
                    break;
                }
                if(by_rank_4)
                {
                    s1_4 = float3 {(*(sorted_13 + (&h_7)->id_1[int(15)])).x, (*(sorted_13 + (&h_7)->id_1[int(15)])).y, (*(sorted_13 + (&h_7)->id_1[int(15)])).z};
                }
                else
                {
                    float3  _S1259 = pca_load3_0(means_5, (&h_7)->id_1[int(15)]);
                    s1_4 = _S1259;
                }
                float3  v_87 = mul_0(_S1177, s1_4 - q_11 - mean_4);
                float * _S1260 = v_means_4 + 3U * axis_6;
                float _S1261 = atomicAdd(_S1260 + int(0), v_87.x);
                float _S1262 = atomicAdd(_S1260 + int(1) + int(0), v_87.y);
                float _S1263 = atomicAdd(_S1260 + int(2) + int(0), v_87.z);
                break;
            }
            break;
        }
        break;
    }
    float3  ls_4 = pca_load3_0(scales_4, self_7);
    float _S1264 = ls_4.x;
    float _S1265 = ls_4.y;
    if(_S1264 <= _S1265)
    {
        if(_S1264 <= (ls_4.z))
        {
            axis_6 = 0U;
        }
        else
        {
            axis_6 = 2U;
        }
    }
    else
    {
        if(_S1265 <= (ls_4.z))
        {
            axis_6 = 1U;
        }
        else
        {
            axis_6 = 2U;
        }
    }
    uint _S1266 = 4U * self_7;
    float4  quat_5 = make_float4 (*(quats_4 + _S1266), *(quats_4 + (_S1266 + 1U)), *(quats_4 + (_S1266 + 2U)), *(quats_4 + (_S1266 + 3U)));
    float align_4 = pca_axis_align_0(quat_5, e0_5, axis_6);
    if(w_normal_4 != 0.0f)
    {
        float4  _S1267 = make_float4 (0.0f);
        DiffPair_vectorx3Cfloatx2C4x3E_0 dq_4;
        (&dq_4)->primal_0 = quat_5;
        (&dq_4)->differential_0 = _S1267;
        s_bwd_pca_axis_align_0(&dq_4, e0_5, axis_6, - w_normal_4);
        uint c_10 = 0U;
        for(;;)
        {
            if(c_10 < 4U)
            {
            }
            else
            {
                break;
            }
            float * _S1268 = v_quats_4 + (_S1266 + c_10);
            *_S1268 = *_S1268 + _slang_vector_get_element(dq_4.differential_0, c_10);
            c_10 = c_10 + 1U;
        }
    }
    if(w_surfel_4 != 0.0f)
    {
        float smin_4;
        if(axis_6 == 0U)
        {
            smin_4 = _S1264;
        }
        else
        {
            if(axis_6 == 1U)
            {
                smin_4 = _S1265;
            }
            else
            {
                smin_4 = ls_4.z;
            }
        }
        float * _S1269 = v_scales_4 + (3U * self_7 + axis_6);
        *_S1269 = *_S1269 + w_surfel_4 * (F32_exp((smin_4 - (F32_max((_S1264), ((F32_max((_S1265), (ls_4.z)))))))));
    }
    return make_float3 (ev_5.x * _S1176, (ev_5.z - ev_5.y) * _S1176, 1.0f - align_4);
}

inline __device__ float3  pca_reg_owner_5(uint r_9, uint mode_5, uint n_10, uint k_17, uint window_5, float * means_6, float * quats_5, float * scales_5, int * perm_6, float4  * sorted_14, float4  * nodes_9, int4  * meta_9, int * ranges_9, int * nbrs_5, uint store_list_5, float * radii_5, uint gate_means_5, float * v_means_5, float * v_quats_5, float * v_scales_5, float w_plane_5, float w_tangent_5, float w_normal_5, float w_surfel_5)
{
    bool _S1270;
    bool _S1271;
    bool _S1272;
    bool _S1273;
    bool _S1274;
    bool _S1275;
    bool _S1276;
    bool _S1277;
    bool _S1278;
    bool _S1279;
    bool _S1280;
    bool _S1281;
    bool _S1282;
    bool _S1283;
    bool _S1284;
    bool _S1285;
    bool _S1286;
    bool _S1287;
    bool _S1288;
    bool _S1289;
    bool _S1290;
    bool _S1291;
    bool _S1292;
    bool _S1293;
    bool _S1294;
    bool _S1295;
    bool _S1296;
    bool _S1297;
    bool _S1298;
    bool _S1299;
    bool _S1300;
    bool _S1301;
    bool _S1302;
    PcaKnn_2 h_8;
    PcaKnn_reset_2(&h_8);
    bool by_rank_5 = mode_5 != 3U;
    uint self_8 = uint(*(perm_6 + r_9));
    float3  q_12;
    if(by_rank_5)
    {
        q_12 = float3 {(*(sorted_14 + r_9)).x, (*(sorted_14 + r_9)).y, (*(sorted_14 + r_9)).z};
    }
    else
    {
        float3  _S1303 = pca_load3_0(means_6, self_8);
        q_12 = _S1303;
    }
    int id_7;
    uint axis_7;
    bool scatter_5;
    if(mode_5 == 0U)
    {
        pca_knn_tree_2(r_9, n_10, k_17, sorted_14, nodes_9, meta_9, ranges_9, &h_8);
    }
    else
    {
        if(mode_5 == 2U)
        {
            uint half_5 = (U32_max((window_5), (k_17 + 1U))) / 2U;
            if(r_9 > half_5)
            {
                axis_7 = r_9 - half_5;
            }
            else
            {
                axis_7 = 0U;
            }
            PcaKnn_scan_2(&h_8, q_12, r_9, axis_7, (U32_min((r_9 + half_5 + 1U), (n_10))), 0U, 0U, sorted_14, k_17);
        }
        else
        {
            if(mode_5 == 1U)
            {
                if(r_9 > (k_17 / 2U))
                {
                    axis_7 = r_9 - k_17 / 2U;
                }
                else
                {
                    axis_7 = 0U;
                }
                if((axis_7 + k_17 + 1U) > n_10)
                {
                    uint _S1304 = k_17 + 1U;
                    if(n_10 > _S1304)
                    {
                        axis_7 = n_10 - _S1304;
                    }
                    else
                    {
                        axis_7 = 0U;
                    }
                }
                if(axis_7 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_171 = axis_7 + uint(id_7);
                if(0U < k_17)
                {
                    scatter_5 = i_171 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(0)] = 0.0f;
                    (&h_8)->id_1[int(0)] = i_171;
                }
                uint i_172 = axis_7 + 1U;
                if(i_172 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_173 = i_172 + uint(id_7);
                if(1U < k_17)
                {
                    scatter_5 = i_173 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(1)] = 0.0f;
                    (&h_8)->id_1[int(1)] = i_173;
                }
                uint i_174 = axis_7 + 2U;
                if(i_174 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_175 = i_174 + uint(id_7);
                if(2U < k_17)
                {
                    scatter_5 = i_175 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(2)] = 0.0f;
                    (&h_8)->id_1[int(2)] = i_175;
                }
                uint i_176 = axis_7 + 3U;
                if(i_176 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_177 = i_176 + uint(id_7);
                if(3U < k_17)
                {
                    scatter_5 = i_177 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(3)] = 0.0f;
                    (&h_8)->id_1[int(3)] = i_177;
                }
                uint i_178 = axis_7 + 4U;
                if(i_178 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_179 = i_178 + uint(id_7);
                if(4U < k_17)
                {
                    scatter_5 = i_179 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(4)] = 0.0f;
                    (&h_8)->id_1[int(4)] = i_179;
                }
                uint i_180 = axis_7 + 5U;
                if(i_180 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_181 = i_180 + uint(id_7);
                if(5U < k_17)
                {
                    scatter_5 = i_181 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(5)] = 0.0f;
                    (&h_8)->id_1[int(5)] = i_181;
                }
                uint i_182 = axis_7 + 6U;
                if(i_182 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_183 = i_182 + uint(id_7);
                if(6U < k_17)
                {
                    scatter_5 = i_183 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(6)] = 0.0f;
                    (&h_8)->id_1[int(6)] = i_183;
                }
                uint i_184 = axis_7 + 7U;
                if(i_184 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_185 = i_184 + uint(id_7);
                if(7U < k_17)
                {
                    scatter_5 = i_185 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(7)] = 0.0f;
                    (&h_8)->id_1[int(7)] = i_185;
                }
                uint i_186 = axis_7 + 8U;
                if(i_186 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_187 = i_186 + uint(id_7);
                if(8U < k_17)
                {
                    scatter_5 = i_187 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(8)] = 0.0f;
                    (&h_8)->id_1[int(8)] = i_187;
                }
                uint i_188 = axis_7 + 9U;
                if(i_188 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_189 = i_188 + uint(id_7);
                if(9U < k_17)
                {
                    scatter_5 = i_189 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(9)] = 0.0f;
                    (&h_8)->id_1[int(9)] = i_189;
                }
                uint i_190 = axis_7 + 10U;
                if(i_190 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_191 = i_190 + uint(id_7);
                if(10U < k_17)
                {
                    scatter_5 = i_191 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(10)] = 0.0f;
                    (&h_8)->id_1[int(10)] = i_191;
                }
                uint i_192 = axis_7 + 11U;
                if(i_192 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_193 = i_192 + uint(id_7);
                if(11U < k_17)
                {
                    scatter_5 = i_193 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(11)] = 0.0f;
                    (&h_8)->id_1[int(11)] = i_193;
                }
                uint i_194 = axis_7 + 12U;
                if(i_194 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_195 = i_194 + uint(id_7);
                if(12U < k_17)
                {
                    scatter_5 = i_195 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(12)] = 0.0f;
                    (&h_8)->id_1[int(12)] = i_195;
                }
                uint i_196 = axis_7 + 13U;
                if(i_196 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_197 = i_196 + uint(id_7);
                if(13U < k_17)
                {
                    scatter_5 = i_197 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(13)] = 0.0f;
                    (&h_8)->id_1[int(13)] = i_197;
                }
                uint i_198 = axis_7 + 14U;
                if(i_198 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_199 = i_198 + uint(id_7);
                if(14U < k_17)
                {
                    scatter_5 = i_199 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(14)] = 0.0f;
                    (&h_8)->id_1[int(14)] = i_199;
                }
                uint i_200 = axis_7 + 15U;
                if(i_200 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_201 = i_200 + uint(id_7);
                if(15U < k_17)
                {
                    scatter_5 = i_201 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(15)] = 0.0f;
                    (&h_8)->id_1[int(15)] = i_201;
                }
                uint i_202 = axis_7 + 16U;
                if(i_202 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_203 = i_202 + uint(id_7);
                if(16U < k_17)
                {
                    scatter_5 = i_203 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(16)] = 0.0f;
                    (&h_8)->id_1[int(16)] = i_203;
                }
                uint i_204 = axis_7 + 17U;
                if(i_204 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_205 = i_204 + uint(id_7);
                if(17U < k_17)
                {
                    scatter_5 = i_205 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(17)] = 0.0f;
                    (&h_8)->id_1[int(17)] = i_205;
                }
                uint i_206 = axis_7 + 18U;
                if(i_206 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_207 = i_206 + uint(id_7);
                if(18U < k_17)
                {
                    scatter_5 = i_207 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(18)] = 0.0f;
                    (&h_8)->id_1[int(18)] = i_207;
                }
                uint i_208 = axis_7 + 19U;
                if(i_208 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_209 = i_208 + uint(id_7);
                if(19U < k_17)
                {
                    scatter_5 = i_209 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(19)] = 0.0f;
                    (&h_8)->id_1[int(19)] = i_209;
                }
                uint i_210 = axis_7 + 20U;
                if(i_210 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_211 = i_210 + uint(id_7);
                if(20U < k_17)
                {
                    scatter_5 = i_211 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(20)] = 0.0f;
                    (&h_8)->id_1[int(20)] = i_211;
                }
                uint i_212 = axis_7 + 21U;
                if(i_212 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_213 = i_212 + uint(id_7);
                if(21U < k_17)
                {
                    scatter_5 = i_213 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(21)] = 0.0f;
                    (&h_8)->id_1[int(21)] = i_213;
                }
                uint i_214 = axis_7 + 22U;
                if(i_214 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_215 = i_214 + uint(id_7);
                if(22U < k_17)
                {
                    scatter_5 = i_215 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(22)] = 0.0f;
                    (&h_8)->id_1[int(22)] = i_215;
                }
                uint i_216 = axis_7 + 23U;
                if(i_216 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_217 = i_216 + uint(id_7);
                if(23U < k_17)
                {
                    scatter_5 = i_217 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(23)] = 0.0f;
                    (&h_8)->id_1[int(23)] = i_217;
                }
                uint i_218 = axis_7 + 24U;
                if(i_218 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_219 = i_218 + uint(id_7);
                if(24U < k_17)
                {
                    scatter_5 = i_219 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(24)] = 0.0f;
                    (&h_8)->id_1[int(24)] = i_219;
                }
                uint i_220 = axis_7 + 25U;
                if(i_220 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_221 = i_220 + uint(id_7);
                if(25U < k_17)
                {
                    scatter_5 = i_221 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(25)] = 0.0f;
                    (&h_8)->id_1[int(25)] = i_221;
                }
                uint i_222 = axis_7 + 26U;
                if(i_222 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_223 = i_222 + uint(id_7);
                if(26U < k_17)
                {
                    scatter_5 = i_223 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(26)] = 0.0f;
                    (&h_8)->id_1[int(26)] = i_223;
                }
                uint i_224 = axis_7 + 27U;
                if(i_224 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_225 = i_224 + uint(id_7);
                if(27U < k_17)
                {
                    scatter_5 = i_225 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(27)] = 0.0f;
                    (&h_8)->id_1[int(27)] = i_225;
                }
                uint i_226 = axis_7 + 28U;
                if(i_226 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_227 = i_226 + uint(id_7);
                if(28U < k_17)
                {
                    scatter_5 = i_227 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(28)] = 0.0f;
                    (&h_8)->id_1[int(28)] = i_227;
                }
                uint i_228 = axis_7 + 29U;
                if(i_228 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_229 = i_228 + uint(id_7);
                if(29U < k_17)
                {
                    scatter_5 = i_229 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(29)] = 0.0f;
                    (&h_8)->id_1[int(29)] = i_229;
                }
                uint i_230 = axis_7 + 30U;
                if(i_230 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_231 = i_230 + uint(id_7);
                if(30U < k_17)
                {
                    scatter_5 = i_231 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(30)] = 0.0f;
                    (&h_8)->id_1[int(30)] = i_231;
                }
                uint i_232 = axis_7 + 31U;
                if(i_232 >= r_9)
                {
                    id_7 = int(1);
                }
                else
                {
                    id_7 = int(0);
                }
                uint i_233 = i_232 + uint(id_7);
                if(31U < k_17)
                {
                    scatter_5 = i_233 < n_10;
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    (&h_8)->d_0[int(31)] = 0.0f;
                    (&h_8)->id_1[int(31)] = i_233;
                }
            }
            else
            {
                if(0U < k_17)
                {
                    id_7 = *(nbrs_5 + r_9 * k_17);
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(0)] = 0.0f;
                    (&h_8)->id_1[int(0)] = uint(id_7);
                }
                if(1U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 1U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(1)] = 0.0f;
                    (&h_8)->id_1[int(1)] = uint(id_7);
                }
                if(2U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 2U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(2)] = 0.0f;
                    (&h_8)->id_1[int(2)] = uint(id_7);
                }
                if(3U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 3U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(3)] = 0.0f;
                    (&h_8)->id_1[int(3)] = uint(id_7);
                }
                if(4U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 4U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(4)] = 0.0f;
                    (&h_8)->id_1[int(4)] = uint(id_7);
                }
                if(5U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 5U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(5)] = 0.0f;
                    (&h_8)->id_1[int(5)] = uint(id_7);
                }
                if(6U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 6U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(6)] = 0.0f;
                    (&h_8)->id_1[int(6)] = uint(id_7);
                }
                if(7U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 7U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(7)] = 0.0f;
                    (&h_8)->id_1[int(7)] = uint(id_7);
                }
                if(8U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 8U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(8)] = 0.0f;
                    (&h_8)->id_1[int(8)] = uint(id_7);
                }
                if(9U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 9U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(9)] = 0.0f;
                    (&h_8)->id_1[int(9)] = uint(id_7);
                }
                if(10U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 10U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(10)] = 0.0f;
                    (&h_8)->id_1[int(10)] = uint(id_7);
                }
                if(11U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 11U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(11)] = 0.0f;
                    (&h_8)->id_1[int(11)] = uint(id_7);
                }
                if(12U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 12U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(12)] = 0.0f;
                    (&h_8)->id_1[int(12)] = uint(id_7);
                }
                if(13U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 13U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(13)] = 0.0f;
                    (&h_8)->id_1[int(13)] = uint(id_7);
                }
                if(14U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 14U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(14)] = 0.0f;
                    (&h_8)->id_1[int(14)] = uint(id_7);
                }
                if(15U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 15U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(15)] = 0.0f;
                    (&h_8)->id_1[int(15)] = uint(id_7);
                }
                if(16U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 16U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(16)] = 0.0f;
                    (&h_8)->id_1[int(16)] = uint(id_7);
                }
                if(17U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 17U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(17)] = 0.0f;
                    (&h_8)->id_1[int(17)] = uint(id_7);
                }
                if(18U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 18U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(18)] = 0.0f;
                    (&h_8)->id_1[int(18)] = uint(id_7);
                }
                if(19U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 19U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(19)] = 0.0f;
                    (&h_8)->id_1[int(19)] = uint(id_7);
                }
                if(20U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 20U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(20)] = 0.0f;
                    (&h_8)->id_1[int(20)] = uint(id_7);
                }
                if(21U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 21U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(21)] = 0.0f;
                    (&h_8)->id_1[int(21)] = uint(id_7);
                }
                if(22U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 22U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(22)] = 0.0f;
                    (&h_8)->id_1[int(22)] = uint(id_7);
                }
                if(23U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 23U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(23)] = 0.0f;
                    (&h_8)->id_1[int(23)] = uint(id_7);
                }
                if(24U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 24U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(24)] = 0.0f;
                    (&h_8)->id_1[int(24)] = uint(id_7);
                }
                if(25U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 25U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(25)] = 0.0f;
                    (&h_8)->id_1[int(25)] = uint(id_7);
                }
                if(26U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 26U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(26)] = 0.0f;
                    (&h_8)->id_1[int(26)] = uint(id_7);
                }
                if(27U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 27U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(27)] = 0.0f;
                    (&h_8)->id_1[int(27)] = uint(id_7);
                }
                if(28U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 28U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(28)] = 0.0f;
                    (&h_8)->id_1[int(28)] = uint(id_7);
                }
                if(29U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 29U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(29)] = 0.0f;
                    (&h_8)->id_1[int(29)] = uint(id_7);
                }
                if(30U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 30U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(30)] = 0.0f;
                    (&h_8)->id_1[int(30)] = uint(id_7);
                }
                if(31U < k_17)
                {
                    id_7 = *(nbrs_5 + (r_9 * k_17 + 31U));
                }
                else
                {
                    id_7 = int(-1);
                }
                if(id_7 >= int(0))
                {
                    (&h_8)->d_0[int(31)] = 0.0f;
                    (&h_8)->id_1[int(31)] = uint(id_7);
                }
            }
        }
    }
    PcaKnn_2 _S1305 = h_8;
    uint _S1306 = PcaKnn_count_2(&_S1305, k_17);
    if(store_list_5 != 0U)
    {
        if(0U < k_17)
        {
            int * _S1307 = nbrs_5 + r_9 * k_17;
            if(((&h_8)->d_0[int(0)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(0)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(0)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1307 = id_7;
        }
        if(1U < k_17)
        {
            int * _S1308 = nbrs_5 + (r_9 * k_17 + 1U);
            if(((&h_8)->d_0[int(1)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(1)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(1)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1308 = id_7;
        }
        if(2U < k_17)
        {
            int * _S1309 = nbrs_5 + (r_9 * k_17 + 2U);
            if(((&h_8)->d_0[int(2)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(2)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(2)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1309 = id_7;
        }
        if(3U < k_17)
        {
            int * _S1310 = nbrs_5 + (r_9 * k_17 + 3U);
            if(((&h_8)->d_0[int(3)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(3)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(3)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1310 = id_7;
        }
        if(4U < k_17)
        {
            int * _S1311 = nbrs_5 + (r_9 * k_17 + 4U);
            if(((&h_8)->d_0[int(4)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(4)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(4)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1311 = id_7;
        }
        if(5U < k_17)
        {
            int * _S1312 = nbrs_5 + (r_9 * k_17 + 5U);
            if(((&h_8)->d_0[int(5)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(5)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(5)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1312 = id_7;
        }
        if(6U < k_17)
        {
            int * _S1313 = nbrs_5 + (r_9 * k_17 + 6U);
            if(((&h_8)->d_0[int(6)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(6)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(6)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1313 = id_7;
        }
        if(7U < k_17)
        {
            int * _S1314 = nbrs_5 + (r_9 * k_17 + 7U);
            if(((&h_8)->d_0[int(7)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(7)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(7)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1314 = id_7;
        }
        if(8U < k_17)
        {
            int * _S1315 = nbrs_5 + (r_9 * k_17 + 8U);
            if(((&h_8)->d_0[int(8)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(8)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(8)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1315 = id_7;
        }
        if(9U < k_17)
        {
            int * _S1316 = nbrs_5 + (r_9 * k_17 + 9U);
            if(((&h_8)->d_0[int(9)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(9)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(9)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1316 = id_7;
        }
        if(10U < k_17)
        {
            int * _S1317 = nbrs_5 + (r_9 * k_17 + 10U);
            if(((&h_8)->d_0[int(10)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(10)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(10)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1317 = id_7;
        }
        if(11U < k_17)
        {
            int * _S1318 = nbrs_5 + (r_9 * k_17 + 11U);
            if(((&h_8)->d_0[int(11)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(11)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(11)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1318 = id_7;
        }
        if(12U < k_17)
        {
            int * _S1319 = nbrs_5 + (r_9 * k_17 + 12U);
            if(((&h_8)->d_0[int(12)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(12)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(12)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1319 = id_7;
        }
        if(13U < k_17)
        {
            int * _S1320 = nbrs_5 + (r_9 * k_17 + 13U);
            if(((&h_8)->d_0[int(13)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(13)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(13)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1320 = id_7;
        }
        if(14U < k_17)
        {
            int * _S1321 = nbrs_5 + (r_9 * k_17 + 14U);
            if(((&h_8)->d_0[int(14)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(14)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(14)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1321 = id_7;
        }
        if(15U < k_17)
        {
            int * _S1322 = nbrs_5 + (r_9 * k_17 + 15U);
            if(((&h_8)->d_0[int(15)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(15)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(15)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1322 = id_7;
        }
        if(16U < k_17)
        {
            int * _S1323 = nbrs_5 + (r_9 * k_17 + 16U);
            if(((&h_8)->d_0[int(16)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(16)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(16)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1323 = id_7;
        }
        if(17U < k_17)
        {
            int * _S1324 = nbrs_5 + (r_9 * k_17 + 17U);
            if(((&h_8)->d_0[int(17)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(17)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(17)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1324 = id_7;
        }
        if(18U < k_17)
        {
            int * _S1325 = nbrs_5 + (r_9 * k_17 + 18U);
            if(((&h_8)->d_0[int(18)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(18)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(18)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1325 = id_7;
        }
        if(19U < k_17)
        {
            int * _S1326 = nbrs_5 + (r_9 * k_17 + 19U);
            if(((&h_8)->d_0[int(19)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(19)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(19)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1326 = id_7;
        }
        if(20U < k_17)
        {
            int * _S1327 = nbrs_5 + (r_9 * k_17 + 20U);
            if(((&h_8)->d_0[int(20)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(20)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(20)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1327 = id_7;
        }
        if(21U < k_17)
        {
            int * _S1328 = nbrs_5 + (r_9 * k_17 + 21U);
            if(((&h_8)->d_0[int(21)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(21)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(21)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1328 = id_7;
        }
        if(22U < k_17)
        {
            int * _S1329 = nbrs_5 + (r_9 * k_17 + 22U);
            if(((&h_8)->d_0[int(22)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(22)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(22)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1329 = id_7;
        }
        if(23U < k_17)
        {
            int * _S1330 = nbrs_5 + (r_9 * k_17 + 23U);
            if(((&h_8)->d_0[int(23)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(23)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(23)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1330 = id_7;
        }
        if(24U < k_17)
        {
            int * _S1331 = nbrs_5 + (r_9 * k_17 + 24U);
            if(((&h_8)->d_0[int(24)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(24)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(24)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1331 = id_7;
        }
        if(25U < k_17)
        {
            int * _S1332 = nbrs_5 + (r_9 * k_17 + 25U);
            if(((&h_8)->d_0[int(25)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(25)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(25)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1332 = id_7;
        }
        if(26U < k_17)
        {
            int * _S1333 = nbrs_5 + (r_9 * k_17 + 26U);
            if(((&h_8)->d_0[int(26)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(26)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(26)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1333 = id_7;
        }
        if(27U < k_17)
        {
            int * _S1334 = nbrs_5 + (r_9 * k_17 + 27U);
            if(((&h_8)->d_0[int(27)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(27)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(27)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1334 = id_7;
        }
        if(28U < k_17)
        {
            int * _S1335 = nbrs_5 + (r_9 * k_17 + 28U);
            if(((&h_8)->d_0[int(28)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(28)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(28)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1335 = id_7;
        }
        if(29U < k_17)
        {
            int * _S1336 = nbrs_5 + (r_9 * k_17 + 29U);
            if(((&h_8)->d_0[int(29)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(29)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(29)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1336 = id_7;
        }
        if(30U < k_17)
        {
            int * _S1337 = nbrs_5 + (r_9 * k_17 + 30U);
            if(((&h_8)->d_0[int(30)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(30)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(30)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1337 = id_7;
        }
        if(31U < k_17)
        {
            int * _S1338 = nbrs_5 + (r_9 * k_17 + 31U);
            if(((&h_8)->d_0[int(31)]) < 3.00000000549775576e+38f)
            {
                if(by_rank_5)
                {
                    id_7 = *(perm_6 + (&h_8)->id_1[int(31)]);
                }
                else
                {
                    id_7 = int((&h_8)->id_1[int(31)]);
                }
            }
            else
            {
                id_7 = int(-1);
            }
            *_S1338 = id_7;
        }
    }
    if(_S1306 < 2U)
    {
        return make_float3 (0.0f);
    }
    float3  so_80;
    float3  sd_80;
    float3  s1_5;
    float3  _S1339 = make_float3 (0.0f);
    for(;;)
    {
        for(;;)
        {
            for(;;)
            {
                bool _S1340 = 0U >= k_17;
                _S1270 = _S1340;
                if(_S1340)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(0)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    s1_5 = _S1339;
                    sd_80 = _S1339;
                    so_80 = _S1339;
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(0)])).x, (*(sorted_14 + (&h_8)->id_1[int(0)])).y, (*(sorted_14 + (&h_8)->id_1[int(0)])).z};
                }
                else
                {
                    float3  _S1341 = pca_load3_0(means_6, (&h_8)->id_1[int(0)]);
                    s1_5 = _S1341;
                }
                float3  d_83 = s1_5 - q_12;
                float3  _S1342 = d_83 * d_83;
                float _S1343 = d_83.x;
                float _S1344 = d_83.y;
                float _S1345 = d_83.z;
                float3  _S1346 = make_float3 (_S1343 * _S1344, _S1343 * _S1345, _S1344 * _S1345);
                s1_5 = d_83;
                sd_80 = _S1342;
                so_80 = _S1346;
                break;
            }
            break;
        }
        float3  _S1347;
        for(;;)
        {
            for(;;)
            {
                bool _S1348 = 1U >= k_17;
                _S1271 = _S1348;
                if(_S1348)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(1)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(1)])).x, (*(sorted_14 + (&h_8)->id_1[int(1)])).y, (*(sorted_14 + (&h_8)->id_1[int(1)])).z};
                }
                else
                {
                    float3  _S1349 = pca_load3_0(means_6, (&h_8)->id_1[int(1)]);
                    _S1347 = _S1349;
                }
                float3  d_84 = _S1347 - q_12;
                float3  sd_81 = sd_80 + d_84 * d_84;
                float _S1350 = d_84.x;
                float _S1351 = d_84.y;
                float _S1352 = d_84.z;
                float3  so_81 = so_80 + make_float3 (_S1350 * _S1351, _S1350 * _S1352, _S1351 * _S1352);
                s1_5 = s1_5 + d_84;
                sd_80 = sd_81;
                so_80 = so_81;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1353 = 2U >= k_17;
                _S1272 = _S1353;
                if(_S1353)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(2)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(2)])).x, (*(sorted_14 + (&h_8)->id_1[int(2)])).y, (*(sorted_14 + (&h_8)->id_1[int(2)])).z};
                }
                else
                {
                    float3  _S1354 = pca_load3_0(means_6, (&h_8)->id_1[int(2)]);
                    _S1347 = _S1354;
                }
                float3  d_85 = _S1347 - q_12;
                float3  sd_82 = sd_80 + d_85 * d_85;
                float _S1355 = d_85.x;
                float _S1356 = d_85.y;
                float _S1357 = d_85.z;
                float3  so_82 = so_80 + make_float3 (_S1355 * _S1356, _S1355 * _S1357, _S1356 * _S1357);
                s1_5 = s1_5 + d_85;
                sd_80 = sd_82;
                so_80 = so_82;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1358 = 3U >= k_17;
                _S1273 = _S1358;
                if(_S1358)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(3)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(3)])).x, (*(sorted_14 + (&h_8)->id_1[int(3)])).y, (*(sorted_14 + (&h_8)->id_1[int(3)])).z};
                }
                else
                {
                    float3  _S1359 = pca_load3_0(means_6, (&h_8)->id_1[int(3)]);
                    _S1347 = _S1359;
                }
                float3  d_86 = _S1347 - q_12;
                float3  sd_83 = sd_80 + d_86 * d_86;
                float _S1360 = d_86.x;
                float _S1361 = d_86.y;
                float _S1362 = d_86.z;
                float3  so_83 = so_80 + make_float3 (_S1360 * _S1361, _S1360 * _S1362, _S1361 * _S1362);
                s1_5 = s1_5 + d_86;
                sd_80 = sd_83;
                so_80 = so_83;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1363 = 4U >= k_17;
                _S1274 = _S1363;
                if(_S1363)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(4)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(4)])).x, (*(sorted_14 + (&h_8)->id_1[int(4)])).y, (*(sorted_14 + (&h_8)->id_1[int(4)])).z};
                }
                else
                {
                    float3  _S1364 = pca_load3_0(means_6, (&h_8)->id_1[int(4)]);
                    _S1347 = _S1364;
                }
                float3  d_87 = _S1347 - q_12;
                float3  sd_84 = sd_80 + d_87 * d_87;
                float _S1365 = d_87.x;
                float _S1366 = d_87.y;
                float _S1367 = d_87.z;
                float3  so_84 = so_80 + make_float3 (_S1365 * _S1366, _S1365 * _S1367, _S1366 * _S1367);
                s1_5 = s1_5 + d_87;
                sd_80 = sd_84;
                so_80 = so_84;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1368 = 5U >= k_17;
                _S1275 = _S1368;
                if(_S1368)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(5)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(5)])).x, (*(sorted_14 + (&h_8)->id_1[int(5)])).y, (*(sorted_14 + (&h_8)->id_1[int(5)])).z};
                }
                else
                {
                    float3  _S1369 = pca_load3_0(means_6, (&h_8)->id_1[int(5)]);
                    _S1347 = _S1369;
                }
                float3  d_88 = _S1347 - q_12;
                float3  sd_85 = sd_80 + d_88 * d_88;
                float _S1370 = d_88.x;
                float _S1371 = d_88.y;
                float _S1372 = d_88.z;
                float3  so_85 = so_80 + make_float3 (_S1370 * _S1371, _S1370 * _S1372, _S1371 * _S1372);
                s1_5 = s1_5 + d_88;
                sd_80 = sd_85;
                so_80 = so_85;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1373 = 6U >= k_17;
                _S1276 = _S1373;
                if(_S1373)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(6)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(6)])).x, (*(sorted_14 + (&h_8)->id_1[int(6)])).y, (*(sorted_14 + (&h_8)->id_1[int(6)])).z};
                }
                else
                {
                    float3  _S1374 = pca_load3_0(means_6, (&h_8)->id_1[int(6)]);
                    _S1347 = _S1374;
                }
                float3  d_89 = _S1347 - q_12;
                float3  sd_86 = sd_80 + d_89 * d_89;
                float _S1375 = d_89.x;
                float _S1376 = d_89.y;
                float _S1377 = d_89.z;
                float3  so_86 = so_80 + make_float3 (_S1375 * _S1376, _S1375 * _S1377, _S1376 * _S1377);
                s1_5 = s1_5 + d_89;
                sd_80 = sd_86;
                so_80 = so_86;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1378 = 7U >= k_17;
                _S1277 = _S1378;
                if(_S1378)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(7)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(7)])).x, (*(sorted_14 + (&h_8)->id_1[int(7)])).y, (*(sorted_14 + (&h_8)->id_1[int(7)])).z};
                }
                else
                {
                    float3  _S1379 = pca_load3_0(means_6, (&h_8)->id_1[int(7)]);
                    _S1347 = _S1379;
                }
                float3  d_90 = _S1347 - q_12;
                float3  sd_87 = sd_80 + d_90 * d_90;
                float _S1380 = d_90.x;
                float _S1381 = d_90.y;
                float _S1382 = d_90.z;
                float3  so_87 = so_80 + make_float3 (_S1380 * _S1381, _S1380 * _S1382, _S1381 * _S1382);
                s1_5 = s1_5 + d_90;
                sd_80 = sd_87;
                so_80 = so_87;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1383 = 8U >= k_17;
                _S1278 = _S1383;
                if(_S1383)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(8)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(8)])).x, (*(sorted_14 + (&h_8)->id_1[int(8)])).y, (*(sorted_14 + (&h_8)->id_1[int(8)])).z};
                }
                else
                {
                    float3  _S1384 = pca_load3_0(means_6, (&h_8)->id_1[int(8)]);
                    _S1347 = _S1384;
                }
                float3  d_91 = _S1347 - q_12;
                float3  sd_88 = sd_80 + d_91 * d_91;
                float _S1385 = d_91.x;
                float _S1386 = d_91.y;
                float _S1387 = d_91.z;
                float3  so_88 = so_80 + make_float3 (_S1385 * _S1386, _S1385 * _S1387, _S1386 * _S1387);
                s1_5 = s1_5 + d_91;
                sd_80 = sd_88;
                so_80 = so_88;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1388 = 9U >= k_17;
                _S1279 = _S1388;
                if(_S1388)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(9)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(9)])).x, (*(sorted_14 + (&h_8)->id_1[int(9)])).y, (*(sorted_14 + (&h_8)->id_1[int(9)])).z};
                }
                else
                {
                    float3  _S1389 = pca_load3_0(means_6, (&h_8)->id_1[int(9)]);
                    _S1347 = _S1389;
                }
                float3  d_92 = _S1347 - q_12;
                float3  sd_89 = sd_80 + d_92 * d_92;
                float _S1390 = d_92.x;
                float _S1391 = d_92.y;
                float _S1392 = d_92.z;
                float3  so_89 = so_80 + make_float3 (_S1390 * _S1391, _S1390 * _S1392, _S1391 * _S1392);
                s1_5 = s1_5 + d_92;
                sd_80 = sd_89;
                so_80 = so_89;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1393 = 10U >= k_17;
                _S1280 = _S1393;
                if(_S1393)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(10)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(10)])).x, (*(sorted_14 + (&h_8)->id_1[int(10)])).y, (*(sorted_14 + (&h_8)->id_1[int(10)])).z};
                }
                else
                {
                    float3  _S1394 = pca_load3_0(means_6, (&h_8)->id_1[int(10)]);
                    _S1347 = _S1394;
                }
                float3  d_93 = _S1347 - q_12;
                float3  sd_90 = sd_80 + d_93 * d_93;
                float _S1395 = d_93.x;
                float _S1396 = d_93.y;
                float _S1397 = d_93.z;
                float3  so_90 = so_80 + make_float3 (_S1395 * _S1396, _S1395 * _S1397, _S1396 * _S1397);
                s1_5 = s1_5 + d_93;
                sd_80 = sd_90;
                so_80 = so_90;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1398 = 11U >= k_17;
                _S1281 = _S1398;
                if(_S1398)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(11)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(11)])).x, (*(sorted_14 + (&h_8)->id_1[int(11)])).y, (*(sorted_14 + (&h_8)->id_1[int(11)])).z};
                }
                else
                {
                    float3  _S1399 = pca_load3_0(means_6, (&h_8)->id_1[int(11)]);
                    _S1347 = _S1399;
                }
                float3  d_94 = _S1347 - q_12;
                float3  sd_91 = sd_80 + d_94 * d_94;
                float _S1400 = d_94.x;
                float _S1401 = d_94.y;
                float _S1402 = d_94.z;
                float3  so_91 = so_80 + make_float3 (_S1400 * _S1401, _S1400 * _S1402, _S1401 * _S1402);
                s1_5 = s1_5 + d_94;
                sd_80 = sd_91;
                so_80 = so_91;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1403 = 12U >= k_17;
                _S1282 = _S1403;
                if(_S1403)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(12)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(12)])).x, (*(sorted_14 + (&h_8)->id_1[int(12)])).y, (*(sorted_14 + (&h_8)->id_1[int(12)])).z};
                }
                else
                {
                    float3  _S1404 = pca_load3_0(means_6, (&h_8)->id_1[int(12)]);
                    _S1347 = _S1404;
                }
                float3  d_95 = _S1347 - q_12;
                float3  sd_92 = sd_80 + d_95 * d_95;
                float _S1405 = d_95.x;
                float _S1406 = d_95.y;
                float _S1407 = d_95.z;
                float3  so_92 = so_80 + make_float3 (_S1405 * _S1406, _S1405 * _S1407, _S1406 * _S1407);
                s1_5 = s1_5 + d_95;
                sd_80 = sd_92;
                so_80 = so_92;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1408 = 13U >= k_17;
                _S1283 = _S1408;
                if(_S1408)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(13)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(13)])).x, (*(sorted_14 + (&h_8)->id_1[int(13)])).y, (*(sorted_14 + (&h_8)->id_1[int(13)])).z};
                }
                else
                {
                    float3  _S1409 = pca_load3_0(means_6, (&h_8)->id_1[int(13)]);
                    _S1347 = _S1409;
                }
                float3  d_96 = _S1347 - q_12;
                float3  sd_93 = sd_80 + d_96 * d_96;
                float _S1410 = d_96.x;
                float _S1411 = d_96.y;
                float _S1412 = d_96.z;
                float3  so_93 = so_80 + make_float3 (_S1410 * _S1411, _S1410 * _S1412, _S1411 * _S1412);
                s1_5 = s1_5 + d_96;
                sd_80 = sd_93;
                so_80 = so_93;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1413 = 14U >= k_17;
                _S1284 = _S1413;
                if(_S1413)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(14)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(14)])).x, (*(sorted_14 + (&h_8)->id_1[int(14)])).y, (*(sorted_14 + (&h_8)->id_1[int(14)])).z};
                }
                else
                {
                    float3  _S1414 = pca_load3_0(means_6, (&h_8)->id_1[int(14)]);
                    _S1347 = _S1414;
                }
                float3  d_97 = _S1347 - q_12;
                float3  sd_94 = sd_80 + d_97 * d_97;
                float _S1415 = d_97.x;
                float _S1416 = d_97.y;
                float _S1417 = d_97.z;
                float3  so_94 = so_80 + make_float3 (_S1415 * _S1416, _S1415 * _S1417, _S1416 * _S1417);
                s1_5 = s1_5 + d_97;
                sd_80 = sd_94;
                so_80 = so_94;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1418 = 15U >= k_17;
                _S1285 = _S1418;
                if(_S1418)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(15)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(15)])).x, (*(sorted_14 + (&h_8)->id_1[int(15)])).y, (*(sorted_14 + (&h_8)->id_1[int(15)])).z};
                }
                else
                {
                    float3  _S1419 = pca_load3_0(means_6, (&h_8)->id_1[int(15)]);
                    _S1347 = _S1419;
                }
                float3  d_98 = _S1347 - q_12;
                float3  sd_95 = sd_80 + d_98 * d_98;
                float _S1420 = d_98.x;
                float _S1421 = d_98.y;
                float _S1422 = d_98.z;
                float3  so_95 = so_80 + make_float3 (_S1420 * _S1421, _S1420 * _S1422, _S1421 * _S1422);
                s1_5 = s1_5 + d_98;
                sd_80 = sd_95;
                so_80 = so_95;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1423 = 16U >= k_17;
                _S1286 = _S1423;
                if(_S1423)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(16)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(16)])).x, (*(sorted_14 + (&h_8)->id_1[int(16)])).y, (*(sorted_14 + (&h_8)->id_1[int(16)])).z};
                }
                else
                {
                    float3  _S1424 = pca_load3_0(means_6, (&h_8)->id_1[int(16)]);
                    _S1347 = _S1424;
                }
                float3  d_99 = _S1347 - q_12;
                float3  sd_96 = sd_80 + d_99 * d_99;
                float _S1425 = d_99.x;
                float _S1426 = d_99.y;
                float _S1427 = d_99.z;
                float3  so_96 = so_80 + make_float3 (_S1425 * _S1426, _S1425 * _S1427, _S1426 * _S1427);
                s1_5 = s1_5 + d_99;
                sd_80 = sd_96;
                so_80 = so_96;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1428 = 17U >= k_17;
                _S1287 = _S1428;
                if(_S1428)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(17)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(17)])).x, (*(sorted_14 + (&h_8)->id_1[int(17)])).y, (*(sorted_14 + (&h_8)->id_1[int(17)])).z};
                }
                else
                {
                    float3  _S1429 = pca_load3_0(means_6, (&h_8)->id_1[int(17)]);
                    _S1347 = _S1429;
                }
                float3  d_100 = _S1347 - q_12;
                float3  sd_97 = sd_80 + d_100 * d_100;
                float _S1430 = d_100.x;
                float _S1431 = d_100.y;
                float _S1432 = d_100.z;
                float3  so_97 = so_80 + make_float3 (_S1430 * _S1431, _S1430 * _S1432, _S1431 * _S1432);
                s1_5 = s1_5 + d_100;
                sd_80 = sd_97;
                so_80 = so_97;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1433 = 18U >= k_17;
                _S1288 = _S1433;
                if(_S1433)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(18)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(18)])).x, (*(sorted_14 + (&h_8)->id_1[int(18)])).y, (*(sorted_14 + (&h_8)->id_1[int(18)])).z};
                }
                else
                {
                    float3  _S1434 = pca_load3_0(means_6, (&h_8)->id_1[int(18)]);
                    _S1347 = _S1434;
                }
                float3  d_101 = _S1347 - q_12;
                float3  sd_98 = sd_80 + d_101 * d_101;
                float _S1435 = d_101.x;
                float _S1436 = d_101.y;
                float _S1437 = d_101.z;
                float3  so_98 = so_80 + make_float3 (_S1435 * _S1436, _S1435 * _S1437, _S1436 * _S1437);
                s1_5 = s1_5 + d_101;
                sd_80 = sd_98;
                so_80 = so_98;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1438 = 19U >= k_17;
                _S1289 = _S1438;
                if(_S1438)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(19)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(19)])).x, (*(sorted_14 + (&h_8)->id_1[int(19)])).y, (*(sorted_14 + (&h_8)->id_1[int(19)])).z};
                }
                else
                {
                    float3  _S1439 = pca_load3_0(means_6, (&h_8)->id_1[int(19)]);
                    _S1347 = _S1439;
                }
                float3  d_102 = _S1347 - q_12;
                float3  sd_99 = sd_80 + d_102 * d_102;
                float _S1440 = d_102.x;
                float _S1441 = d_102.y;
                float _S1442 = d_102.z;
                float3  so_99 = so_80 + make_float3 (_S1440 * _S1441, _S1440 * _S1442, _S1441 * _S1442);
                s1_5 = s1_5 + d_102;
                sd_80 = sd_99;
                so_80 = so_99;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1443 = 20U >= k_17;
                _S1290 = _S1443;
                if(_S1443)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(20)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(20)])).x, (*(sorted_14 + (&h_8)->id_1[int(20)])).y, (*(sorted_14 + (&h_8)->id_1[int(20)])).z};
                }
                else
                {
                    float3  _S1444 = pca_load3_0(means_6, (&h_8)->id_1[int(20)]);
                    _S1347 = _S1444;
                }
                float3  d_103 = _S1347 - q_12;
                float3  sd_100 = sd_80 + d_103 * d_103;
                float _S1445 = d_103.x;
                float _S1446 = d_103.y;
                float _S1447 = d_103.z;
                float3  so_100 = so_80 + make_float3 (_S1445 * _S1446, _S1445 * _S1447, _S1446 * _S1447);
                s1_5 = s1_5 + d_103;
                sd_80 = sd_100;
                so_80 = so_100;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1448 = 21U >= k_17;
                _S1291 = _S1448;
                if(_S1448)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(21)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(21)])).x, (*(sorted_14 + (&h_8)->id_1[int(21)])).y, (*(sorted_14 + (&h_8)->id_1[int(21)])).z};
                }
                else
                {
                    float3  _S1449 = pca_load3_0(means_6, (&h_8)->id_1[int(21)]);
                    _S1347 = _S1449;
                }
                float3  d_104 = _S1347 - q_12;
                float3  sd_101 = sd_80 + d_104 * d_104;
                float _S1450 = d_104.x;
                float _S1451 = d_104.y;
                float _S1452 = d_104.z;
                float3  so_101 = so_80 + make_float3 (_S1450 * _S1451, _S1450 * _S1452, _S1451 * _S1452);
                s1_5 = s1_5 + d_104;
                sd_80 = sd_101;
                so_80 = so_101;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1453 = 22U >= k_17;
                _S1292 = _S1453;
                if(_S1453)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(22)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(22)])).x, (*(sorted_14 + (&h_8)->id_1[int(22)])).y, (*(sorted_14 + (&h_8)->id_1[int(22)])).z};
                }
                else
                {
                    float3  _S1454 = pca_load3_0(means_6, (&h_8)->id_1[int(22)]);
                    _S1347 = _S1454;
                }
                float3  d_105 = _S1347 - q_12;
                float3  sd_102 = sd_80 + d_105 * d_105;
                float _S1455 = d_105.x;
                float _S1456 = d_105.y;
                float _S1457 = d_105.z;
                float3  so_102 = so_80 + make_float3 (_S1455 * _S1456, _S1455 * _S1457, _S1456 * _S1457);
                s1_5 = s1_5 + d_105;
                sd_80 = sd_102;
                so_80 = so_102;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1458 = 23U >= k_17;
                _S1293 = _S1458;
                if(_S1458)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(23)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(23)])).x, (*(sorted_14 + (&h_8)->id_1[int(23)])).y, (*(sorted_14 + (&h_8)->id_1[int(23)])).z};
                }
                else
                {
                    float3  _S1459 = pca_load3_0(means_6, (&h_8)->id_1[int(23)]);
                    _S1347 = _S1459;
                }
                float3  d_106 = _S1347 - q_12;
                float3  sd_103 = sd_80 + d_106 * d_106;
                float _S1460 = d_106.x;
                float _S1461 = d_106.y;
                float _S1462 = d_106.z;
                float3  so_103 = so_80 + make_float3 (_S1460 * _S1461, _S1460 * _S1462, _S1461 * _S1462);
                s1_5 = s1_5 + d_106;
                sd_80 = sd_103;
                so_80 = so_103;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1463 = 24U >= k_17;
                _S1294 = _S1463;
                if(_S1463)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(24)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(24)])).x, (*(sorted_14 + (&h_8)->id_1[int(24)])).y, (*(sorted_14 + (&h_8)->id_1[int(24)])).z};
                }
                else
                {
                    float3  _S1464 = pca_load3_0(means_6, (&h_8)->id_1[int(24)]);
                    _S1347 = _S1464;
                }
                float3  d_107 = _S1347 - q_12;
                float3  sd_104 = sd_80 + d_107 * d_107;
                float _S1465 = d_107.x;
                float _S1466 = d_107.y;
                float _S1467 = d_107.z;
                float3  so_104 = so_80 + make_float3 (_S1465 * _S1466, _S1465 * _S1467, _S1466 * _S1467);
                s1_5 = s1_5 + d_107;
                sd_80 = sd_104;
                so_80 = so_104;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1468 = 25U >= k_17;
                _S1295 = _S1468;
                if(_S1468)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(25)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(25)])).x, (*(sorted_14 + (&h_8)->id_1[int(25)])).y, (*(sorted_14 + (&h_8)->id_1[int(25)])).z};
                }
                else
                {
                    float3  _S1469 = pca_load3_0(means_6, (&h_8)->id_1[int(25)]);
                    _S1347 = _S1469;
                }
                float3  d_108 = _S1347 - q_12;
                float3  sd_105 = sd_80 + d_108 * d_108;
                float _S1470 = d_108.x;
                float _S1471 = d_108.y;
                float _S1472 = d_108.z;
                float3  so_105 = so_80 + make_float3 (_S1470 * _S1471, _S1470 * _S1472, _S1471 * _S1472);
                s1_5 = s1_5 + d_108;
                sd_80 = sd_105;
                so_80 = so_105;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1473 = 26U >= k_17;
                _S1296 = _S1473;
                if(_S1473)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(26)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(26)])).x, (*(sorted_14 + (&h_8)->id_1[int(26)])).y, (*(sorted_14 + (&h_8)->id_1[int(26)])).z};
                }
                else
                {
                    float3  _S1474 = pca_load3_0(means_6, (&h_8)->id_1[int(26)]);
                    _S1347 = _S1474;
                }
                float3  d_109 = _S1347 - q_12;
                float3  sd_106 = sd_80 + d_109 * d_109;
                float _S1475 = d_109.x;
                float _S1476 = d_109.y;
                float _S1477 = d_109.z;
                float3  so_106 = so_80 + make_float3 (_S1475 * _S1476, _S1475 * _S1477, _S1476 * _S1477);
                s1_5 = s1_5 + d_109;
                sd_80 = sd_106;
                so_80 = so_106;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1478 = 27U >= k_17;
                _S1297 = _S1478;
                if(_S1478)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(27)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(27)])).x, (*(sorted_14 + (&h_8)->id_1[int(27)])).y, (*(sorted_14 + (&h_8)->id_1[int(27)])).z};
                }
                else
                {
                    float3  _S1479 = pca_load3_0(means_6, (&h_8)->id_1[int(27)]);
                    _S1347 = _S1479;
                }
                float3  d_110 = _S1347 - q_12;
                float3  sd_107 = sd_80 + d_110 * d_110;
                float _S1480 = d_110.x;
                float _S1481 = d_110.y;
                float _S1482 = d_110.z;
                float3  so_107 = so_80 + make_float3 (_S1480 * _S1481, _S1480 * _S1482, _S1481 * _S1482);
                s1_5 = s1_5 + d_110;
                sd_80 = sd_107;
                so_80 = so_107;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1483 = 28U >= k_17;
                _S1298 = _S1483;
                if(_S1483)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(28)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(28)])).x, (*(sorted_14 + (&h_8)->id_1[int(28)])).y, (*(sorted_14 + (&h_8)->id_1[int(28)])).z};
                }
                else
                {
                    float3  _S1484 = pca_load3_0(means_6, (&h_8)->id_1[int(28)]);
                    _S1347 = _S1484;
                }
                float3  d_111 = _S1347 - q_12;
                float3  sd_108 = sd_80 + d_111 * d_111;
                float _S1485 = d_111.x;
                float _S1486 = d_111.y;
                float _S1487 = d_111.z;
                float3  so_108 = so_80 + make_float3 (_S1485 * _S1486, _S1485 * _S1487, _S1486 * _S1487);
                s1_5 = s1_5 + d_111;
                sd_80 = sd_108;
                so_80 = so_108;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1488 = 29U >= k_17;
                _S1299 = _S1488;
                if(_S1488)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(29)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(29)])).x, (*(sorted_14 + (&h_8)->id_1[int(29)])).y, (*(sorted_14 + (&h_8)->id_1[int(29)])).z};
                }
                else
                {
                    float3  _S1489 = pca_load3_0(means_6, (&h_8)->id_1[int(29)]);
                    _S1347 = _S1489;
                }
                float3  d_112 = _S1347 - q_12;
                float3  sd_109 = sd_80 + d_112 * d_112;
                float _S1490 = d_112.x;
                float _S1491 = d_112.y;
                float _S1492 = d_112.z;
                float3  so_109 = so_80 + make_float3 (_S1490 * _S1491, _S1490 * _S1492, _S1491 * _S1492);
                s1_5 = s1_5 + d_112;
                sd_80 = sd_109;
                so_80 = so_109;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1493 = 30U >= k_17;
                _S1300 = _S1493;
                if(_S1493)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(30)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(30)])).x, (*(sorted_14 + (&h_8)->id_1[int(30)])).y, (*(sorted_14 + (&h_8)->id_1[int(30)])).z};
                }
                else
                {
                    float3  _S1494 = pca_load3_0(means_6, (&h_8)->id_1[int(30)]);
                    _S1347 = _S1494;
                }
                float3  d_113 = _S1347 - q_12;
                float3  sd_110 = sd_80 + d_113 * d_113;
                float _S1495 = d_113.x;
                float _S1496 = d_113.y;
                float _S1497 = d_113.z;
                float3  so_110 = so_80 + make_float3 (_S1495 * _S1496, _S1495 * _S1497, _S1496 * _S1497);
                s1_5 = s1_5 + d_113;
                sd_80 = sd_110;
                so_80 = so_110;
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                bool _S1498 = 31U >= k_17;
                _S1301 = _S1498;
                if(_S1498)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(31)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    _S1347 = float3 {(*(sorted_14 + (&h_8)->id_1[int(31)])).x, (*(sorted_14 + (&h_8)->id_1[int(31)])).y, (*(sorted_14 + (&h_8)->id_1[int(31)])).z};
                }
                else
                {
                    float3  _S1499 = pca_load3_0(means_6, (&h_8)->id_1[int(31)]);
                    _S1347 = _S1499;
                }
                float3  d_114 = _S1347 - q_12;
                float3  sd_111 = sd_80 + d_114 * d_114;
                float _S1500 = d_114.x;
                float _S1501 = d_114.y;
                float _S1502 = d_114.z;
                float3  so_111 = so_80 + make_float3 (_S1500 * _S1501, _S1500 * _S1502, _S1501 * _S1502);
                s1_5 = s1_5 + d_114;
                sd_80 = sd_111;
                so_80 = so_111;
                break;
            }
            break;
        }
        break;
    }
    float inv_k_5 = 1.0f / float(_S1306 + 1U);
    float3  mean_5 = s1_5 * make_float3 (inv_k_5);
    float _S1503 = mean_5.x;
    float _S1504 = mean_5.y;
    float _S1505 = mean_5.z;
    float3  ev_6;
    float3  e0_6;
    float3  e1_6;
    float3  e2_6;
    pca_sym3_eigen_0(sd_80 * make_float3 (inv_k_5) - mean_5 * mean_5, so_80 * make_float3 (inv_k_5) - make_float3 (_S1503 * _S1504, _S1503 * _S1505, _S1504 * _S1505), &ev_6, &e0_6, &e1_6, &e2_6);
    if(!all_0(isfinite_0(ev_6)))
    {
        scatter_5 = true;
    }
    else
    {
        scatter_5 = !all_0(isfinite_0(e0_6));
    }
    if(scatter_5)
    {
        return _S1339;
    }
    float t_6 = ev_6.x + ev_6.y + ev_6.z;
    float _S1506;
    if(t_6 > (9.999999960041972e-13f * dot_0(q_12, q_12) + 1.00000000317107685e-30f))
    {
        _S1506 = 1.0f / t_6;
    }
    else
    {
        _S1506 = 0.0f;
    }
    float g_2 = 2.0f * inv_k_5 * _S1506;
    Matrix<float, 3, 3>  _S1507 = makeMatrix<float, 3, 3> (g_2 * w_plane_5) * makeMatrix<float, 3, 3> (make_float3 (e0_6.x) * e0_6, make_float3 (e0_6.y) * e0_6, make_float3 (e0_6.z) * e0_6) + makeMatrix<float, 3, 3> (g_2 * w_tangent_5) * (makeMatrix<float, 3, 3> (make_float3 (e2_6.x) * e2_6, make_float3 (e2_6.y) * e2_6, make_float3 (e2_6.z) * e2_6) - makeMatrix<float, 3, 3> (make_float3 (e1_6.x) * e1_6, make_float3 (e1_6.y) * e1_6, make_float3 (e1_6.z) * e1_6));
    float3  v_88 = mul_0(_S1507, - mean_5);
    if(w_plane_5 != 0.0f)
    {
        scatter_5 = true;
    }
    else
    {
        scatter_5 = w_tangent_5 != 0.0f;
    }
    bool _S1508;
    if(scatter_5)
    {
        if(gate_means_5 == 0U)
        {
            _S1508 = true;
        }
        else
        {
            _S1508 = (*(radii_5 + self_8)) > 0.0f;
        }
    }
    else
    {
        _S1508 = false;
    }
    if(_S1508)
    {
        float * _S1509 = v_means_5 + 3U * self_8;
        float _S1510 = atomicAdd(_S1509 + int(0), v_88.x);
        float _S1511 = atomicAdd(_S1509 + int(1) + int(0), v_88.y);
        float _S1512 = atomicAdd(_S1509 + int(2) + int(0), v_88.z);
    }
    for(;;)
    {
        for(;;)
        {
            for(;;)
            {
                bool _S1513 = !scatter_5;
                _S1302 = _S1513;
                if(_S1513)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1270;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(0)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(0)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(0)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(0)])).x, (*(sorted_14 + (&h_8)->id_1[int(0)])).y, (*(sorted_14 + (&h_8)->id_1[int(0)])).z};
                }
                else
                {
                    float3  _S1514 = pca_load3_0(means_6, (&h_8)->id_1[int(0)]);
                    s1_5 = _S1514;
                }
                float3  v_89 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1515 = v_means_5 + 3U * axis_7;
                float _S1516 = atomicAdd(_S1515 + int(0), v_89.x);
                float _S1517 = atomicAdd(_S1515 + int(1) + int(0), v_89.y);
                float _S1518 = atomicAdd(_S1515 + int(2) + int(0), v_89.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1271;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(1)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(1)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(1)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(1)])).x, (*(sorted_14 + (&h_8)->id_1[int(1)])).y, (*(sorted_14 + (&h_8)->id_1[int(1)])).z};
                }
                else
                {
                    float3  _S1519 = pca_load3_0(means_6, (&h_8)->id_1[int(1)]);
                    s1_5 = _S1519;
                }
                float3  v_90 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1520 = v_means_5 + 3U * axis_7;
                float _S1521 = atomicAdd(_S1520 + int(0), v_90.x);
                float _S1522 = atomicAdd(_S1520 + int(1) + int(0), v_90.y);
                float _S1523 = atomicAdd(_S1520 + int(2) + int(0), v_90.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1272;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(2)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(2)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(2)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(2)])).x, (*(sorted_14 + (&h_8)->id_1[int(2)])).y, (*(sorted_14 + (&h_8)->id_1[int(2)])).z};
                }
                else
                {
                    float3  _S1524 = pca_load3_0(means_6, (&h_8)->id_1[int(2)]);
                    s1_5 = _S1524;
                }
                float3  v_91 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1525 = v_means_5 + 3U * axis_7;
                float _S1526 = atomicAdd(_S1525 + int(0), v_91.x);
                float _S1527 = atomicAdd(_S1525 + int(1) + int(0), v_91.y);
                float _S1528 = atomicAdd(_S1525 + int(2) + int(0), v_91.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1273;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(3)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(3)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(3)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(3)])).x, (*(sorted_14 + (&h_8)->id_1[int(3)])).y, (*(sorted_14 + (&h_8)->id_1[int(3)])).z};
                }
                else
                {
                    float3  _S1529 = pca_load3_0(means_6, (&h_8)->id_1[int(3)]);
                    s1_5 = _S1529;
                }
                float3  v_92 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1530 = v_means_5 + 3U * axis_7;
                float _S1531 = atomicAdd(_S1530 + int(0), v_92.x);
                float _S1532 = atomicAdd(_S1530 + int(1) + int(0), v_92.y);
                float _S1533 = atomicAdd(_S1530 + int(2) + int(0), v_92.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1274;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(4)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(4)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(4)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(4)])).x, (*(sorted_14 + (&h_8)->id_1[int(4)])).y, (*(sorted_14 + (&h_8)->id_1[int(4)])).z};
                }
                else
                {
                    float3  _S1534 = pca_load3_0(means_6, (&h_8)->id_1[int(4)]);
                    s1_5 = _S1534;
                }
                float3  v_93 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1535 = v_means_5 + 3U * axis_7;
                float _S1536 = atomicAdd(_S1535 + int(0), v_93.x);
                float _S1537 = atomicAdd(_S1535 + int(1) + int(0), v_93.y);
                float _S1538 = atomicAdd(_S1535 + int(2) + int(0), v_93.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1275;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(5)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(5)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(5)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(5)])).x, (*(sorted_14 + (&h_8)->id_1[int(5)])).y, (*(sorted_14 + (&h_8)->id_1[int(5)])).z};
                }
                else
                {
                    float3  _S1539 = pca_load3_0(means_6, (&h_8)->id_1[int(5)]);
                    s1_5 = _S1539;
                }
                float3  v_94 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1540 = v_means_5 + 3U * axis_7;
                float _S1541 = atomicAdd(_S1540 + int(0), v_94.x);
                float _S1542 = atomicAdd(_S1540 + int(1) + int(0), v_94.y);
                float _S1543 = atomicAdd(_S1540 + int(2) + int(0), v_94.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1276;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(6)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(6)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(6)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(6)])).x, (*(sorted_14 + (&h_8)->id_1[int(6)])).y, (*(sorted_14 + (&h_8)->id_1[int(6)])).z};
                }
                else
                {
                    float3  _S1544 = pca_load3_0(means_6, (&h_8)->id_1[int(6)]);
                    s1_5 = _S1544;
                }
                float3  v_95 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1545 = v_means_5 + 3U * axis_7;
                float _S1546 = atomicAdd(_S1545 + int(0), v_95.x);
                float _S1547 = atomicAdd(_S1545 + int(1) + int(0), v_95.y);
                float _S1548 = atomicAdd(_S1545 + int(2) + int(0), v_95.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1277;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(7)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(7)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(7)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(7)])).x, (*(sorted_14 + (&h_8)->id_1[int(7)])).y, (*(sorted_14 + (&h_8)->id_1[int(7)])).z};
                }
                else
                {
                    float3  _S1549 = pca_load3_0(means_6, (&h_8)->id_1[int(7)]);
                    s1_5 = _S1549;
                }
                float3  v_96 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1550 = v_means_5 + 3U * axis_7;
                float _S1551 = atomicAdd(_S1550 + int(0), v_96.x);
                float _S1552 = atomicAdd(_S1550 + int(1) + int(0), v_96.y);
                float _S1553 = atomicAdd(_S1550 + int(2) + int(0), v_96.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1278;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(8)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(8)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(8)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(8)])).x, (*(sorted_14 + (&h_8)->id_1[int(8)])).y, (*(sorted_14 + (&h_8)->id_1[int(8)])).z};
                }
                else
                {
                    float3  _S1554 = pca_load3_0(means_6, (&h_8)->id_1[int(8)]);
                    s1_5 = _S1554;
                }
                float3  v_97 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1555 = v_means_5 + 3U * axis_7;
                float _S1556 = atomicAdd(_S1555 + int(0), v_97.x);
                float _S1557 = atomicAdd(_S1555 + int(1) + int(0), v_97.y);
                float _S1558 = atomicAdd(_S1555 + int(2) + int(0), v_97.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1279;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(9)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(9)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(9)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(9)])).x, (*(sorted_14 + (&h_8)->id_1[int(9)])).y, (*(sorted_14 + (&h_8)->id_1[int(9)])).z};
                }
                else
                {
                    float3  _S1559 = pca_load3_0(means_6, (&h_8)->id_1[int(9)]);
                    s1_5 = _S1559;
                }
                float3  v_98 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1560 = v_means_5 + 3U * axis_7;
                float _S1561 = atomicAdd(_S1560 + int(0), v_98.x);
                float _S1562 = atomicAdd(_S1560 + int(1) + int(0), v_98.y);
                float _S1563 = atomicAdd(_S1560 + int(2) + int(0), v_98.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1280;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(10)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(10)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(10)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(10)])).x, (*(sorted_14 + (&h_8)->id_1[int(10)])).y, (*(sorted_14 + (&h_8)->id_1[int(10)])).z};
                }
                else
                {
                    float3  _S1564 = pca_load3_0(means_6, (&h_8)->id_1[int(10)]);
                    s1_5 = _S1564;
                }
                float3  v_99 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1565 = v_means_5 + 3U * axis_7;
                float _S1566 = atomicAdd(_S1565 + int(0), v_99.x);
                float _S1567 = atomicAdd(_S1565 + int(1) + int(0), v_99.y);
                float _S1568 = atomicAdd(_S1565 + int(2) + int(0), v_99.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1281;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(11)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(11)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(11)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(11)])).x, (*(sorted_14 + (&h_8)->id_1[int(11)])).y, (*(sorted_14 + (&h_8)->id_1[int(11)])).z};
                }
                else
                {
                    float3  _S1569 = pca_load3_0(means_6, (&h_8)->id_1[int(11)]);
                    s1_5 = _S1569;
                }
                float3  v_100 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1570 = v_means_5 + 3U * axis_7;
                float _S1571 = atomicAdd(_S1570 + int(0), v_100.x);
                float _S1572 = atomicAdd(_S1570 + int(1) + int(0), v_100.y);
                float _S1573 = atomicAdd(_S1570 + int(2) + int(0), v_100.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1282;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(12)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(12)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(12)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(12)])).x, (*(sorted_14 + (&h_8)->id_1[int(12)])).y, (*(sorted_14 + (&h_8)->id_1[int(12)])).z};
                }
                else
                {
                    float3  _S1574 = pca_load3_0(means_6, (&h_8)->id_1[int(12)]);
                    s1_5 = _S1574;
                }
                float3  v_101 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1575 = v_means_5 + 3U * axis_7;
                float _S1576 = atomicAdd(_S1575 + int(0), v_101.x);
                float _S1577 = atomicAdd(_S1575 + int(1) + int(0), v_101.y);
                float _S1578 = atomicAdd(_S1575 + int(2) + int(0), v_101.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1283;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(13)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(13)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(13)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(13)])).x, (*(sorted_14 + (&h_8)->id_1[int(13)])).y, (*(sorted_14 + (&h_8)->id_1[int(13)])).z};
                }
                else
                {
                    float3  _S1579 = pca_load3_0(means_6, (&h_8)->id_1[int(13)]);
                    s1_5 = _S1579;
                }
                float3  v_102 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1580 = v_means_5 + 3U * axis_7;
                float _S1581 = atomicAdd(_S1580 + int(0), v_102.x);
                float _S1582 = atomicAdd(_S1580 + int(1) + int(0), v_102.y);
                float _S1583 = atomicAdd(_S1580 + int(2) + int(0), v_102.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1284;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(14)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(14)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(14)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(14)])).x, (*(sorted_14 + (&h_8)->id_1[int(14)])).y, (*(sorted_14 + (&h_8)->id_1[int(14)])).z};
                }
                else
                {
                    float3  _S1584 = pca_load3_0(means_6, (&h_8)->id_1[int(14)]);
                    s1_5 = _S1584;
                }
                float3  v_103 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1585 = v_means_5 + 3U * axis_7;
                float _S1586 = atomicAdd(_S1585 + int(0), v_103.x);
                float _S1587 = atomicAdd(_S1585 + int(1) + int(0), v_103.y);
                float _S1588 = atomicAdd(_S1585 + int(2) + int(0), v_103.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1285;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(15)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(15)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(15)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(15)])).x, (*(sorted_14 + (&h_8)->id_1[int(15)])).y, (*(sorted_14 + (&h_8)->id_1[int(15)])).z};
                }
                else
                {
                    float3  _S1589 = pca_load3_0(means_6, (&h_8)->id_1[int(15)]);
                    s1_5 = _S1589;
                }
                float3  v_104 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1590 = v_means_5 + 3U * axis_7;
                float _S1591 = atomicAdd(_S1590 + int(0), v_104.x);
                float _S1592 = atomicAdd(_S1590 + int(1) + int(0), v_104.y);
                float _S1593 = atomicAdd(_S1590 + int(2) + int(0), v_104.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1286;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(16)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(16)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(16)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(16)])).x, (*(sorted_14 + (&h_8)->id_1[int(16)])).y, (*(sorted_14 + (&h_8)->id_1[int(16)])).z};
                }
                else
                {
                    float3  _S1594 = pca_load3_0(means_6, (&h_8)->id_1[int(16)]);
                    s1_5 = _S1594;
                }
                float3  v_105 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1595 = v_means_5 + 3U * axis_7;
                float _S1596 = atomicAdd(_S1595 + int(0), v_105.x);
                float _S1597 = atomicAdd(_S1595 + int(1) + int(0), v_105.y);
                float _S1598 = atomicAdd(_S1595 + int(2) + int(0), v_105.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1287;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(17)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(17)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(17)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(17)])).x, (*(sorted_14 + (&h_8)->id_1[int(17)])).y, (*(sorted_14 + (&h_8)->id_1[int(17)])).z};
                }
                else
                {
                    float3  _S1599 = pca_load3_0(means_6, (&h_8)->id_1[int(17)]);
                    s1_5 = _S1599;
                }
                float3  v_106 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1600 = v_means_5 + 3U * axis_7;
                float _S1601 = atomicAdd(_S1600 + int(0), v_106.x);
                float _S1602 = atomicAdd(_S1600 + int(1) + int(0), v_106.y);
                float _S1603 = atomicAdd(_S1600 + int(2) + int(0), v_106.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1288;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(18)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(18)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(18)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(18)])).x, (*(sorted_14 + (&h_8)->id_1[int(18)])).y, (*(sorted_14 + (&h_8)->id_1[int(18)])).z};
                }
                else
                {
                    float3  _S1604 = pca_load3_0(means_6, (&h_8)->id_1[int(18)]);
                    s1_5 = _S1604;
                }
                float3  v_107 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1605 = v_means_5 + 3U * axis_7;
                float _S1606 = atomicAdd(_S1605 + int(0), v_107.x);
                float _S1607 = atomicAdd(_S1605 + int(1) + int(0), v_107.y);
                float _S1608 = atomicAdd(_S1605 + int(2) + int(0), v_107.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1289;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(19)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(19)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(19)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(19)])).x, (*(sorted_14 + (&h_8)->id_1[int(19)])).y, (*(sorted_14 + (&h_8)->id_1[int(19)])).z};
                }
                else
                {
                    float3  _S1609 = pca_load3_0(means_6, (&h_8)->id_1[int(19)]);
                    s1_5 = _S1609;
                }
                float3  v_108 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1610 = v_means_5 + 3U * axis_7;
                float _S1611 = atomicAdd(_S1610 + int(0), v_108.x);
                float _S1612 = atomicAdd(_S1610 + int(1) + int(0), v_108.y);
                float _S1613 = atomicAdd(_S1610 + int(2) + int(0), v_108.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1290;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(20)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(20)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(20)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(20)])).x, (*(sorted_14 + (&h_8)->id_1[int(20)])).y, (*(sorted_14 + (&h_8)->id_1[int(20)])).z};
                }
                else
                {
                    float3  _S1614 = pca_load3_0(means_6, (&h_8)->id_1[int(20)]);
                    s1_5 = _S1614;
                }
                float3  v_109 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1615 = v_means_5 + 3U * axis_7;
                float _S1616 = atomicAdd(_S1615 + int(0), v_109.x);
                float _S1617 = atomicAdd(_S1615 + int(1) + int(0), v_109.y);
                float _S1618 = atomicAdd(_S1615 + int(2) + int(0), v_109.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1291;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(21)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(21)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(21)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(21)])).x, (*(sorted_14 + (&h_8)->id_1[int(21)])).y, (*(sorted_14 + (&h_8)->id_1[int(21)])).z};
                }
                else
                {
                    float3  _S1619 = pca_load3_0(means_6, (&h_8)->id_1[int(21)]);
                    s1_5 = _S1619;
                }
                float3  v_110 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1620 = v_means_5 + 3U * axis_7;
                float _S1621 = atomicAdd(_S1620 + int(0), v_110.x);
                float _S1622 = atomicAdd(_S1620 + int(1) + int(0), v_110.y);
                float _S1623 = atomicAdd(_S1620 + int(2) + int(0), v_110.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1292;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(22)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(22)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(22)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(22)])).x, (*(sorted_14 + (&h_8)->id_1[int(22)])).y, (*(sorted_14 + (&h_8)->id_1[int(22)])).z};
                }
                else
                {
                    float3  _S1624 = pca_load3_0(means_6, (&h_8)->id_1[int(22)]);
                    s1_5 = _S1624;
                }
                float3  v_111 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1625 = v_means_5 + 3U * axis_7;
                float _S1626 = atomicAdd(_S1625 + int(0), v_111.x);
                float _S1627 = atomicAdd(_S1625 + int(1) + int(0), v_111.y);
                float _S1628 = atomicAdd(_S1625 + int(2) + int(0), v_111.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1293;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(23)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(23)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(23)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(23)])).x, (*(sorted_14 + (&h_8)->id_1[int(23)])).y, (*(sorted_14 + (&h_8)->id_1[int(23)])).z};
                }
                else
                {
                    float3  _S1629 = pca_load3_0(means_6, (&h_8)->id_1[int(23)]);
                    s1_5 = _S1629;
                }
                float3  v_112 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1630 = v_means_5 + 3U * axis_7;
                float _S1631 = atomicAdd(_S1630 + int(0), v_112.x);
                float _S1632 = atomicAdd(_S1630 + int(1) + int(0), v_112.y);
                float _S1633 = atomicAdd(_S1630 + int(2) + int(0), v_112.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1294;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(24)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(24)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(24)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(24)])).x, (*(sorted_14 + (&h_8)->id_1[int(24)])).y, (*(sorted_14 + (&h_8)->id_1[int(24)])).z};
                }
                else
                {
                    float3  _S1634 = pca_load3_0(means_6, (&h_8)->id_1[int(24)]);
                    s1_5 = _S1634;
                }
                float3  v_113 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1635 = v_means_5 + 3U * axis_7;
                float _S1636 = atomicAdd(_S1635 + int(0), v_113.x);
                float _S1637 = atomicAdd(_S1635 + int(1) + int(0), v_113.y);
                float _S1638 = atomicAdd(_S1635 + int(2) + int(0), v_113.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1295;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(25)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(25)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(25)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(25)])).x, (*(sorted_14 + (&h_8)->id_1[int(25)])).y, (*(sorted_14 + (&h_8)->id_1[int(25)])).z};
                }
                else
                {
                    float3  _S1639 = pca_load3_0(means_6, (&h_8)->id_1[int(25)]);
                    s1_5 = _S1639;
                }
                float3  v_114 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1640 = v_means_5 + 3U * axis_7;
                float _S1641 = atomicAdd(_S1640 + int(0), v_114.x);
                float _S1642 = atomicAdd(_S1640 + int(1) + int(0), v_114.y);
                float _S1643 = atomicAdd(_S1640 + int(2) + int(0), v_114.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1296;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(26)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(26)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(26)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(26)])).x, (*(sorted_14 + (&h_8)->id_1[int(26)])).y, (*(sorted_14 + (&h_8)->id_1[int(26)])).z};
                }
                else
                {
                    float3  _S1644 = pca_load3_0(means_6, (&h_8)->id_1[int(26)]);
                    s1_5 = _S1644;
                }
                float3  v_115 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1645 = v_means_5 + 3U * axis_7;
                float _S1646 = atomicAdd(_S1645 + int(0), v_115.x);
                float _S1647 = atomicAdd(_S1645 + int(1) + int(0), v_115.y);
                float _S1648 = atomicAdd(_S1645 + int(2) + int(0), v_115.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1297;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(27)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(27)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(27)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(27)])).x, (*(sorted_14 + (&h_8)->id_1[int(27)])).y, (*(sorted_14 + (&h_8)->id_1[int(27)])).z};
                }
                else
                {
                    float3  _S1649 = pca_load3_0(means_6, (&h_8)->id_1[int(27)]);
                    s1_5 = _S1649;
                }
                float3  v_116 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1650 = v_means_5 + 3U * axis_7;
                float _S1651 = atomicAdd(_S1650 + int(0), v_116.x);
                float _S1652 = atomicAdd(_S1650 + int(1) + int(0), v_116.y);
                float _S1653 = atomicAdd(_S1650 + int(2) + int(0), v_116.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1298;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(28)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(28)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(28)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(28)])).x, (*(sorted_14 + (&h_8)->id_1[int(28)])).y, (*(sorted_14 + (&h_8)->id_1[int(28)])).z};
                }
                else
                {
                    float3  _S1654 = pca_load3_0(means_6, (&h_8)->id_1[int(28)]);
                    s1_5 = _S1654;
                }
                float3  v_117 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1655 = v_means_5 + 3U * axis_7;
                float _S1656 = atomicAdd(_S1655 + int(0), v_117.x);
                float _S1657 = atomicAdd(_S1655 + int(1) + int(0), v_117.y);
                float _S1658 = atomicAdd(_S1655 + int(2) + int(0), v_117.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1299;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(29)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(29)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(29)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(29)])).x, (*(sorted_14 + (&h_8)->id_1[int(29)])).y, (*(sorted_14 + (&h_8)->id_1[int(29)])).z};
                }
                else
                {
                    float3  _S1659 = pca_load3_0(means_6, (&h_8)->id_1[int(29)]);
                    s1_5 = _S1659;
                }
                float3  v_118 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1660 = v_means_5 + 3U * axis_7;
                float _S1661 = atomicAdd(_S1660 + int(0), v_118.x);
                float _S1662 = atomicAdd(_S1660 + int(1) + int(0), v_118.y);
                float _S1663 = atomicAdd(_S1660 + int(2) + int(0), v_118.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1300;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(30)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(30)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(30)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(30)])).x, (*(sorted_14 + (&h_8)->id_1[int(30)])).y, (*(sorted_14 + (&h_8)->id_1[int(30)])).z};
                }
                else
                {
                    float3  _S1664 = pca_load3_0(means_6, (&h_8)->id_1[int(30)]);
                    s1_5 = _S1664;
                }
                float3  v_119 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1665 = v_means_5 + 3U * axis_7;
                float _S1666 = atomicAdd(_S1665 + int(0), v_119.x);
                float _S1667 = atomicAdd(_S1665 + int(1) + int(0), v_119.y);
                float _S1668 = atomicAdd(_S1665 + int(2) + int(0), v_119.z);
                break;
            }
            break;
        }
        for(;;)
        {
            for(;;)
            {
                if(_S1302)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = _S1301;
                }
                if(scatter_5)
                {
                    scatter_5 = true;
                }
                else
                {
                    scatter_5 = !(((&h_8)->d_0[int(31)]) < 3.00000000549775576e+38f);
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    axis_7 = uint(*(perm_6 + (&h_8)->id_1[int(31)]));
                }
                else
                {
                    axis_7 = (&h_8)->id_1[int(31)];
                }
                if(gate_means_5 != 0U)
                {
                    scatter_5 = !((*(radii_5 + axis_7)) > 0.0f);
                }
                else
                {
                    scatter_5 = false;
                }
                if(scatter_5)
                {
                    break;
                }
                if(by_rank_5)
                {
                    s1_5 = float3 {(*(sorted_14 + (&h_8)->id_1[int(31)])).x, (*(sorted_14 + (&h_8)->id_1[int(31)])).y, (*(sorted_14 + (&h_8)->id_1[int(31)])).z};
                }
                else
                {
                    float3  _S1669 = pca_load3_0(means_6, (&h_8)->id_1[int(31)]);
                    s1_5 = _S1669;
                }
                float3  v_120 = mul_0(_S1507, s1_5 - q_12 - mean_5);
                float * _S1670 = v_means_5 + 3U * axis_7;
                float _S1671 = atomicAdd(_S1670 + int(0), v_120.x);
                float _S1672 = atomicAdd(_S1670 + int(1) + int(0), v_120.y);
                float _S1673 = atomicAdd(_S1670 + int(2) + int(0), v_120.z);
                break;
            }
            break;
        }
        break;
    }
    float3  ls_5 = pca_load3_0(scales_5, self_8);
    float _S1674 = ls_5.x;
    float _S1675 = ls_5.y;
    if(_S1674 <= _S1675)
    {
        if(_S1674 <= (ls_5.z))
        {
            axis_7 = 0U;
        }
        else
        {
            axis_7 = 2U;
        }
    }
    else
    {
        if(_S1675 <= (ls_5.z))
        {
            axis_7 = 1U;
        }
        else
        {
            axis_7 = 2U;
        }
    }
    uint _S1676 = 4U * self_8;
    float4  quat_6 = make_float4 (*(quats_5 + _S1676), *(quats_5 + (_S1676 + 1U)), *(quats_5 + (_S1676 + 2U)), *(quats_5 + (_S1676 + 3U)));
    float align_5 = pca_axis_align_0(quat_6, e0_6, axis_7);
    if(w_normal_5 != 0.0f)
    {
        float4  _S1677 = make_float4 (0.0f);
        DiffPair_vectorx3Cfloatx2C4x3E_0 dq_5;
        (&dq_5)->primal_0 = quat_6;
        (&dq_5)->differential_0 = _S1677;
        s_bwd_pca_axis_align_0(&dq_5, e0_6, axis_7, - w_normal_5);
        uint c_11 = 0U;
        for(;;)
        {
            if(c_11 < 4U)
            {
            }
            else
            {
                break;
            }
            float * _S1678 = v_quats_5 + (_S1676 + c_11);
            *_S1678 = *_S1678 + _slang_vector_get_element(dq_5.differential_0, c_11);
            c_11 = c_11 + 1U;
        }
    }
    if(w_surfel_5 != 0.0f)
    {
        float smin_5;
        if(axis_7 == 0U)
        {
            smin_5 = _S1674;
        }
        else
        {
            if(axis_7 == 1U)
            {
                smin_5 = _S1675;
            }
            else
            {
                smin_5 = ls_5.z;
            }
        }
        float * _S1679 = v_scales_5 + (3U * self_8 + axis_7);
        *_S1679 = *_S1679 + w_surfel_5 * (F32_exp((smin_5 - (F32_max((_S1674), ((F32_max((_S1675), (ls_5.z)))))))));
    }
    return make_float3 (ev_6.x * _S1506, (ev_6.z - ev_6.y) * _S1506, 1.0f - align_5);
}

inline __device__ float3  pca_reg_owner_k8(uint r_10, uint mode_6, uint n_11, uint k_18, uint window_6, float * means_7, float * quats_6, float * scales_6, int * perm_7, float4  * sorted_15, float4  * nodes_10, int4  * meta_10, int * ranges_10, int * nbrs_6, uint store_list_6, float * radii_6, uint gate_means_6, float * v_means_6, float * v_quats_6, float * v_scales_6, float w_plane_6, float w_tangent_6, float w_normal_6, float w_surfel_6)
{
    float3  _S1680 = pca_reg_owner_0(r_10, mode_6, n_11, k_18, window_6, means_7, quats_6, scales_6, perm_7, sorted_15, nodes_10, meta_10, ranges_10, nbrs_6, store_list_6, radii_6, gate_means_6, v_means_6, v_quats_6, v_scales_6, w_plane_6, w_tangent_6, w_normal_6, w_surfel_6);
    return _S1680;
}

inline __device__ float3  pca_reg_owner_k16(uint r_11, uint mode_7, uint n_12, uint k_19, uint window_7, float * means_8, float * quats_7, float * scales_7, int * perm_8, float4  * sorted_16, float4  * nodes_11, int4  * meta_11, int * ranges_11, int * nbrs_7, uint store_list_7, float * radii_7, uint gate_means_7, float * v_means_7, float * v_quats_7, float * v_scales_7, float w_plane_7, float w_tangent_7, float w_normal_7, float w_surfel_7)
{
    float3  _S1681 = pca_reg_owner_1(r_11, mode_7, n_12, k_19, window_7, means_8, quats_7, scales_7, perm_8, sorted_16, nodes_11, meta_11, ranges_11, nbrs_7, store_list_7, radii_7, gate_means_7, v_means_7, v_quats_7, v_scales_7, w_plane_7, w_tangent_7, w_normal_7, w_surfel_7);
    return _S1681;
}

inline __device__ float3  pca_reg_owner_k32(uint r_12, uint mode_8, uint n_13, uint k_20, uint window_8, float * means_9, float * quats_8, float * scales_8, int * perm_9, float4  * sorted_17, float4  * nodes_12, int4  * meta_12, int * ranges_12, int * nbrs_8, uint store_list_8, float * radii_8, uint gate_means_8, float * v_means_8, float * v_quats_8, float * v_scales_8, float w_plane_8, float w_tangent_8, float w_normal_8, float w_surfel_8)
{
    float3  _S1682 = pca_reg_owner_2(r_12, mode_8, n_13, k_20, window_8, means_9, quats_8, scales_8, perm_9, sorted_17, nodes_12, meta_12, ranges_12, nbrs_8, store_list_8, radii_8, gate_means_8, v_means_8, v_quats_8, v_scales_8, w_plane_8, w_tangent_8, w_normal_8, w_surfel_8);
    return _S1682;
}

inline __device__ float3  pca_reg_owner_k8_sa(uint r_13, uint mode_9, uint n_14, uint k_21, uint window_9, float * means_10, float * quats_9, float * scales_9, int * perm_10, float4  * sorted_18, float4  * nodes_13, int4  * meta_13, int * ranges_13, int * nbrs_9, uint store_list_9, float * radii_9, uint gate_means_9, float * v_means_9, float * v_quats_9, float * v_scales_9, float w_plane_9, float w_tangent_9, float w_normal_9, float w_surfel_9)
{
    float3  _S1683 = pca_reg_owner_3(r_13, mode_9, n_14, k_21, window_9, means_10, quats_9, scales_9, perm_10, sorted_18, nodes_13, meta_13, ranges_13, nbrs_9, store_list_9, radii_9, gate_means_9, v_means_9, v_quats_9, v_scales_9, w_plane_9, w_tangent_9, w_normal_9, w_surfel_9);
    return _S1683;
}

inline __device__ float3  pca_reg_owner_k16_sa(uint r_14, uint mode_10, uint n_15, uint k_22, uint window_10, float * means_11, float * quats_10, float * scales_10, int * perm_11, float4  * sorted_19, float4  * nodes_14, int4  * meta_14, int * ranges_14, int * nbrs_10, uint store_list_10, float * radii_10, uint gate_means_10, float * v_means_10, float * v_quats_10, float * v_scales_10, float w_plane_10, float w_tangent_10, float w_normal_10, float w_surfel_10)
{
    float3  _S1684 = pca_reg_owner_4(r_14, mode_10, n_15, k_22, window_10, means_11, quats_10, scales_10, perm_11, sorted_19, nodes_14, meta_14, ranges_14, nbrs_10, store_list_10, radii_10, gate_means_10, v_means_10, v_quats_10, v_scales_10, w_plane_10, w_tangent_10, w_normal_10, w_surfel_10);
    return _S1684;
}

inline __device__ float3  pca_reg_owner_k32_sa(uint r_15, uint mode_11, uint n_16, uint k_23, uint window_11, float * means_12, float * quats_11, float * scales_11, int * perm_12, float4  * sorted_20, float4  * nodes_15, int4  * meta_15, int * ranges_15, int * nbrs_11, uint store_list_11, float * radii_11, uint gate_means_11, float * v_means_11, float * v_quats_11, float * v_scales_11, float w_plane_11, float w_tangent_11, float w_normal_11, float w_surfel_11)
{
    float3  _S1685 = pca_reg_owner_5(r_15, mode_11, n_16, k_23, window_11, means_12, quats_11, scales_11, perm_12, sorted_20, nodes_15, meta_15, ranges_15, nbrs_11, store_list_11, radii_11, gate_means_11, v_means_11, v_quats_11, v_scales_11, w_plane_11, w_tangent_11, w_normal_11, w_surfel_11);
    return _S1685;
}

