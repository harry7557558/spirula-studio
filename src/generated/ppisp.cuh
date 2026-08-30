#pragma once

#include "generated/slang.cuh"

struct VignettingChannelParams_0
{
    float cx_0;
    float cy_0;
    float alpha0_0;
    float alpha1_0;
    float alpha2_0;
};

inline __device__ VignettingChannelParams_0 VignettingChannelParams_x24_syn_dzero_0()
{
    VignettingChannelParams_0 result_0;
    (&result_0)->cx_0 = 0.0f;
    (&result_0)->cy_0 = 0.0f;
    (&result_0)->alpha0_0 = 0.0f;
    (&result_0)->alpha1_0 = 0.0f;
    (&result_0)->alpha2_0 = 0.0f;
    return result_0;
}

struct ColorPPISPParams_0
{
    float2  b_0;
    float2  r_0;
    float2  g_0;
    float2  n_0;
};

inline __device__ ColorPPISPParams_0 ColorPPISPParams_x24_syn_dzero_0()
{
    ColorPPISPParams_0 result_1;
    float2  _S1 = make_float2 (0.0f);
    (&result_1)->b_0 = _S1;
    (&result_1)->r_0 = _S1;
    (&result_1)->g_0 = _S1;
    (&result_1)->n_0 = _S1;
    return result_1;
}

struct PPISPParamsNoCRF_0
{
    float exposure_0;
    FixedArray<VignettingChannelParams_0, 3>  vignette_params_0;
    ColorPPISPParams_0 color_params_0;
};

inline __device__ PPISPParamsNoCRF_0 PPISPParamsNoCRF_x24_syn_dzero_0()
{
    PPISPParamsNoCRF_0 result_2;
    (&result_2)->exposure_0 = 0.0f;
    VignettingChannelParams_0 _S2 = VignettingChannelParams_x24_syn_dzero_0();
    (&result_2)->vignette_params_0[int(0)] = _S2;
    (&result_2)->vignette_params_0[int(1)] = _S2;
    (&result_2)->vignette_params_0[int(2)] = _S2;
    (&result_2)->color_params_0 = ColorPPISPParams_x24_syn_dzero_0();
    return result_2;
}

struct RQSCRFPPISPChannelParams_0
{
    float g0_0;
    float g1_0;
    float x0_0;
    float y0_0;
    float gc_0;
};

inline __device__ RQSCRFPPISPChannelParams_0 RQSCRFPPISPChannelParams_x24_syn_dzero_0()
{
    RQSCRFPPISPChannelParams_0 result_3;
    (&result_3)->g0_0 = 0.0f;
    (&result_3)->g1_0 = 0.0f;
    (&result_3)->x0_0 = 0.0f;
    (&result_3)->y0_0 = 0.0f;
    (&result_3)->gc_0 = 0.0f;
    return result_3;
}

struct PPISPParamsRQS_0
{
    float exposure_1;
    FixedArray<VignettingChannelParams_0, 3>  vignette_params_1;
    ColorPPISPParams_0 color_params_1;
    FixedArray<RQSCRFPPISPChannelParams_0, 3>  crf_params_0;
};

inline __device__ PPISPParamsRQS_0 PPISPParamsRQS_x24_syn_dzero_0()
{
    PPISPParamsRQS_0 result_4;
    (&result_4)->exposure_1 = 0.0f;
    VignettingChannelParams_0 _S3 = VignettingChannelParams_x24_syn_dzero_0();
    (&result_4)->vignette_params_1[int(0)] = _S3;
    (&result_4)->vignette_params_1[int(1)] = _S3;
    (&result_4)->vignette_params_1[int(2)] = _S3;
    (&result_4)->color_params_1 = ColorPPISPParams_x24_syn_dzero_0();
    RQSCRFPPISPChannelParams_0 _S4 = RQSCRFPPISPChannelParams_x24_syn_dzero_0();
    (&result_4)->crf_params_0[int(0)] = _S4;
    (&result_4)->crf_params_0[int(1)] = _S4;
    (&result_4)->crf_params_0[int(2)] = _S4;
    return result_4;
}

struct CRFPPISPChannelParams_0
{
    float toe_0;
    float shoulder_0;
    float gamma_0;
    float center_0;
};

inline __device__ CRFPPISPChannelParams_0 CRFPPISPChannelParams_x24_syn_dzero_0()
{
    CRFPPISPChannelParams_0 result_5;
    (&result_5)->toe_0 = 0.0f;
    (&result_5)->shoulder_0 = 0.0f;
    (&result_5)->gamma_0 = 0.0f;
    (&result_5)->center_0 = 0.0f;
    return result_5;
}

struct PPISPParams_0
{
    float exposure_2;
    FixedArray<VignettingChannelParams_0, 3>  vignette_params_2;
    ColorPPISPParams_0 color_params_2;
    FixedArray<CRFPPISPChannelParams_0, 3>  crf_params_1;
};

inline __device__ PPISPParams_0 PPISPParams_x24_syn_dzero_0()
{
    PPISPParams_0 result_6;
    (&result_6)->exposure_2 = 0.0f;
    VignettingChannelParams_0 _S5 = VignettingChannelParams_x24_syn_dzero_0();
    (&result_6)->vignette_params_2[int(0)] = _S5;
    (&result_6)->vignette_params_2[int(1)] = _S5;
    (&result_6)->vignette_params_2[int(2)] = _S5;
    (&result_6)->color_params_2 = ColorPPISPParams_x24_syn_dzero_0();
    CRFPPISPChannelParams_0 _S6 = CRFPPISPChannelParams_x24_syn_dzero_0();
    (&result_6)->crf_params_1[int(0)] = _S6;
    (&result_6)->crf_params_1[int(1)] = _S6;
    (&result_6)->crf_params_1[int(2)] = _S6;
    return result_6;
}

struct DiffPair_float_0
{
    float primal_0;
    float differential_0;
};

inline __device__ void _d_exp2_0(DiffPair_float_0 * dpx_0, float dOut_0)
{
    float _S7 = (F32_exp2(((*dpx_0).primal_0))) * 0.69314718246459961f * dOut_0;
    dpx_0->primal_0 = (*dpx_0).primal_0;
    dpx_0->differential_0 = _S7;
    return;
}

inline __device__ void _d_max_0(DiffPair_float_0 * dpx_1, DiffPair_float_0 * dpy_0, float dOut_1)
{
    DiffPair_float_0 _S8 = *dpx_1;
    float _S9;
    if(((*dpx_1).primal_0) > ((*dpy_0).primal_0))
    {
        _S9 = dOut_1;
    }
    else
    {
        if(((*dpx_1).primal_0) < ((*dpy_0).primal_0))
        {
            _S9 = 0.0f;
        }
        else
        {
            _S9 = 0.5f * dOut_1;
        }
    }
    dpx_1->primal_0 = _S8.primal_0;
    dpx_1->differential_0 = _S9;
    DiffPair_float_0 _S10 = *dpy_0;
    if(((*dpy_0).primal_0) > (_S8.primal_0))
    {
        _S9 = dOut_1;
    }
    else
    {
        if(((*dpy_0).primal_0) < ((*dpx_1).primal_0))
        {
            _S9 = 0.0f;
        }
        else
        {
            _S9 = 0.5f * dOut_1;
        }
    }
    dpy_0->primal_0 = _S10.primal_0;
    dpy_0->differential_0 = _S9;
    return;
}

inline __device__ void _d_clamp_0(DiffPair_float_0 * dpx_2, DiffPair_float_0 * dpMin_0, DiffPair_float_0 * dpMax_0, float dOut_2)
{
    DiffPair_float_0 _S11 = *dpx_2;
    bool _S12;
    if(((*dpx_2).primal_0) >= ((*dpMin_0).primal_0))
    {
        _S12 = ((*dpx_2).primal_0) <= ((*dpMax_0).primal_0);
    }
    else
    {
        _S12 = false;
    }
    float _S13;
    if(_S12)
    {
        _S13 = dOut_2;
    }
    else
    {
        _S13 = 0.0f;
    }
    dpx_2->primal_0 = _S11.primal_0;
    dpx_2->differential_0 = _S13;
    DiffPair_float_0 _S14 = *dpMin_0;
    if((_S11.primal_0) < ((*dpMin_0).primal_0))
    {
        _S13 = dOut_2;
    }
    else
    {
        _S13 = 0.0f;
    }
    dpMin_0->primal_0 = _S14.primal_0;
    dpMin_0->differential_0 = _S13;
    DiffPair_float_0 _S15 = *dpMax_0;
    if(((*dpx_2).primal_0) > ((*dpMax_0).primal_0))
    {
        _S13 = dOut_2;
    }
    else
    {
        _S13 = 0.0f;
    }
    dpMax_0->primal_0 = _S15.primal_0;
    dpMax_0->differential_0 = _S13;
    return;
}

inline __device__ float clamp_0(float x_0, float minBound_0, float maxBound_0)
{
    return (F32_min(((F32_max((x_0), (minBound_0)))), (maxBound_0)));
}

struct DiffPair_matrixx3Cfloatx2C2x2C2x3E_0
{
    Matrix<float, 2, 2>  primal_0;
    Matrix<float, 2, 2>  differential_0;
};

struct DiffPair_vectorx3Cfloatx2C2x3E_0
{
    float2  primal_0;
    float2  differential_0;
};

inline __device__ void _d_mul_0(DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 * left_0, DiffPair_vectorx3Cfloatx2C2x3E_0 * right_0, float2  dOut_3)
{
    float _S16 = (*left_0).primal_0.rows[int(0)].x * dOut_3.x;
    Matrix<float, 2, 2>  left_d_result_0;
    *&(((&left_d_result_0)->rows + (int(0)))->x) = (*right_0).primal_0.x * dOut_3.x;
    float sum_0 = _S16 + (*left_0).primal_0.rows[int(1)].x * dOut_3.y;
    *&(((&left_d_result_0)->rows + (int(1)))->x) = (*right_0).primal_0.x * dOut_3.y;
    float2  right_d_result_0;
    *&((&right_d_result_0)->x) = sum_0;
    float _S17 = (*left_0).primal_0.rows[int(0)].y * dOut_3.x;
    *&(((&left_d_result_0)->rows + (int(0)))->y) = (*right_0).primal_0.y * dOut_3.x;
    float sum_1 = _S17 + (*left_0).primal_0.rows[int(1)].y * dOut_3.y;
    *&(((&left_d_result_0)->rows + (int(1)))->y) = (*right_0).primal_0.y * dOut_3.y;
    *&((&right_d_result_0)->y) = sum_1;
    left_0->primal_0 = (*left_0).primal_0;
    left_0->differential_0 = left_d_result_0;
    right_0->primal_0 = (*right_0).primal_0;
    right_0->differential_0 = right_d_result_0;
    return;
}

struct DiffPair_matrixx3Cfloatx2C3x2C3x3E_0
{
    Matrix<float, 3, 3>  primal_0;
    Matrix<float, 3, 3>  differential_0;
};

struct DiffPair_vectorx3Cfloatx2C3x3E_0
{
    float3  primal_0;
    float3  differential_0;
};

inline __device__ void _d_mul_1(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * left_1, DiffPair_vectorx3Cfloatx2C3x3E_0 * right_1, float3  dOut_4)
{
    float _S18 = (*left_1).primal_0.rows[int(0)].x * dOut_4.x;
    Matrix<float, 3, 3>  left_d_result_1;
    *&(((&left_d_result_1)->rows + (int(0)))->x) = (*right_1).primal_0.x * dOut_4.x;
    float sum_2 = _S18 + (*left_1).primal_0.rows[int(1)].x * dOut_4.y;
    *&(((&left_d_result_1)->rows + (int(1)))->x) = (*right_1).primal_0.x * dOut_4.y;
    float sum_3 = sum_2 + (*left_1).primal_0.rows[int(2)].x * dOut_4.z;
    *&(((&left_d_result_1)->rows + (int(2)))->x) = (*right_1).primal_0.x * dOut_4.z;
    float3  right_d_result_1;
    *&((&right_d_result_1)->x) = sum_3;
    float _S19 = (*left_1).primal_0.rows[int(0)].y * dOut_4.x;
    *&(((&left_d_result_1)->rows + (int(0)))->y) = (*right_1).primal_0.y * dOut_4.x;
    float sum_4 = _S19 + (*left_1).primal_0.rows[int(1)].y * dOut_4.y;
    *&(((&left_d_result_1)->rows + (int(1)))->y) = (*right_1).primal_0.y * dOut_4.y;
    float sum_5 = sum_4 + (*left_1).primal_0.rows[int(2)].y * dOut_4.z;
    *&(((&left_d_result_1)->rows + (int(2)))->y) = (*right_1).primal_0.y * dOut_4.z;
    *&((&right_d_result_1)->y) = sum_5;
    float _S20 = (*left_1).primal_0.rows[int(0)].z * dOut_4.x;
    *&(((&left_d_result_1)->rows + (int(0)))->z) = (*right_1).primal_0.z * dOut_4.x;
    float sum_6 = _S20 + (*left_1).primal_0.rows[int(1)].z * dOut_4.y;
    *&(((&left_d_result_1)->rows + (int(1)))->z) = (*right_1).primal_0.z * dOut_4.y;
    float sum_7 = sum_6 + (*left_1).primal_0.rows[int(2)].z * dOut_4.z;
    *&(((&left_d_result_1)->rows + (int(2)))->z) = (*right_1).primal_0.z * dOut_4.z;
    *&((&right_d_result_1)->z) = sum_7;
    left_1->primal_0 = (*left_1).primal_0;
    left_1->differential_0 = left_d_result_1;
    right_1->primal_0 = (*right_1).primal_0;
    right_1->differential_0 = right_d_result_1;
    return;
}

inline __device__ float2  mul_0(Matrix<float, 2, 2>  left_2, float2  right_2)
{
    float2  result_7;
    int i_0 = int(0);
    for(;;)
    {
        if(i_0 < int(2))
        {
        }
        else
        {
            break;
        }
        int j_0 = int(0);
        float sum_8 = 0.0f;
        for(;;)
        {
            if(j_0 < int(2))
            {
            }
            else
            {
                break;
            }
            float sum_9 = sum_8 + _slang_vector_get_element(left_2.rows[i_0], j_0) * _slang_vector_get_element(right_2, j_0);
            j_0 = j_0 + int(1);
            sum_8 = sum_9;
        }
        *_slang_vector_get_element_ptr(&result_7, i_0) = sum_8;
        i_0 = i_0 + int(1);
    }
    return result_7;
}

inline __device__ float3  mul_1(Matrix<float, 3, 3>  left_3, float3  right_3)
{
    float3  result_8;
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
        int j_1 = int(0);
        float sum_10 = 0.0f;
        for(;;)
        {
            if(j_1 < int(3))
            {
            }
            else
            {
                break;
            }
            float sum_11 = sum_10 + _slang_vector_get_element(left_3.rows[i_1], j_1) * _slang_vector_get_element(right_3, j_1);
            j_1 = j_1 + int(1);
            sum_10 = sum_11;
        }
        *_slang_vector_get_element_ptr(&result_8, i_1) = sum_10;
        i_1 = i_1 + int(1);
    }
    return result_8;
}

inline __device__ void mul_2(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * left_4, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * right_4, Matrix<float, 3, 3>  dOut_5)
{
    Matrix<float, 3, 3>  left_d_result_2;
    *&(((&left_d_result_2)->rows + (int(0)))->x) = 0.0f;
    *&(((&left_d_result_2)->rows + (int(0)))->y) = 0.0f;
    *&(((&left_d_result_2)->rows + (int(0)))->z) = 0.0f;
    *&(((&left_d_result_2)->rows + (int(1)))->x) = 0.0f;
    *&(((&left_d_result_2)->rows + (int(1)))->y) = 0.0f;
    *&(((&left_d_result_2)->rows + (int(1)))->z) = 0.0f;
    *&(((&left_d_result_2)->rows + (int(2)))->x) = 0.0f;
    *&(((&left_d_result_2)->rows + (int(2)))->y) = 0.0f;
    *&(((&left_d_result_2)->rows + (int(2)))->z) = 0.0f;
    Matrix<float, 3, 3>  right_d_result_2;
    *&(((&right_d_result_2)->rows + (int(0)))->x) = 0.0f;
    *&(((&right_d_result_2)->rows + (int(0)))->y) = 0.0f;
    *&(((&right_d_result_2)->rows + (int(0)))->z) = 0.0f;
    *&(((&right_d_result_2)->rows + (int(1)))->x) = 0.0f;
    *&(((&right_d_result_2)->rows + (int(1)))->y) = 0.0f;
    *&(((&right_d_result_2)->rows + (int(1)))->z) = 0.0f;
    *&(((&right_d_result_2)->rows + (int(2)))->x) = 0.0f;
    *&(((&right_d_result_2)->rows + (int(2)))->y) = 0.0f;
    *&(((&right_d_result_2)->rows + (int(2)))->z) = 0.0f;
    *&(((&left_d_result_2)->rows + (int(0)))->x) = *&(((&left_d_result_2)->rows + (int(0)))->x) + (*right_4).primal_0.rows[int(0)].x * dOut_5.rows[int(0)].x;
    *&(((&right_d_result_2)->rows + (int(0)))->x) = *&(((&right_d_result_2)->rows + (int(0)))->x) + (*left_4).primal_0.rows[int(0)].x * dOut_5.rows[int(0)].x;
    *&(((&left_d_result_2)->rows + (int(0)))->y) = *&(((&left_d_result_2)->rows + (int(0)))->y) + (*right_4).primal_0.rows[int(1)].x * dOut_5.rows[int(0)].x;
    *&(((&right_d_result_2)->rows + (int(1)))->x) = *&(((&right_d_result_2)->rows + (int(1)))->x) + (*left_4).primal_0.rows[int(0)].y * dOut_5.rows[int(0)].x;
    *&(((&left_d_result_2)->rows + (int(0)))->z) = *&(((&left_d_result_2)->rows + (int(0)))->z) + (*right_4).primal_0.rows[int(2)].x * dOut_5.rows[int(0)].x;
    *&(((&right_d_result_2)->rows + (int(2)))->x) = *&(((&right_d_result_2)->rows + (int(2)))->x) + (*left_4).primal_0.rows[int(0)].z * dOut_5.rows[int(0)].x;
    *&(((&left_d_result_2)->rows + (int(0)))->x) = *&(((&left_d_result_2)->rows + (int(0)))->x) + (*right_4).primal_0.rows[int(0)].y * dOut_5.rows[int(0)].y;
    *&(((&right_d_result_2)->rows + (int(0)))->y) = *&(((&right_d_result_2)->rows + (int(0)))->y) + (*left_4).primal_0.rows[int(0)].x * dOut_5.rows[int(0)].y;
    *&(((&left_d_result_2)->rows + (int(0)))->y) = *&(((&left_d_result_2)->rows + (int(0)))->y) + (*right_4).primal_0.rows[int(1)].y * dOut_5.rows[int(0)].y;
    *&(((&right_d_result_2)->rows + (int(1)))->y) = *&(((&right_d_result_2)->rows + (int(1)))->y) + (*left_4).primal_0.rows[int(0)].y * dOut_5.rows[int(0)].y;
    *&(((&left_d_result_2)->rows + (int(0)))->z) = *&(((&left_d_result_2)->rows + (int(0)))->z) + (*right_4).primal_0.rows[int(2)].y * dOut_5.rows[int(0)].y;
    *&(((&right_d_result_2)->rows + (int(2)))->y) = *&(((&right_d_result_2)->rows + (int(2)))->y) + (*left_4).primal_0.rows[int(0)].z * dOut_5.rows[int(0)].y;
    *&(((&left_d_result_2)->rows + (int(0)))->x) = *&(((&left_d_result_2)->rows + (int(0)))->x) + (*right_4).primal_0.rows[int(0)].z * dOut_5.rows[int(0)].z;
    *&(((&right_d_result_2)->rows + (int(0)))->z) = *&(((&right_d_result_2)->rows + (int(0)))->z) + (*left_4).primal_0.rows[int(0)].x * dOut_5.rows[int(0)].z;
    *&(((&left_d_result_2)->rows + (int(0)))->y) = *&(((&left_d_result_2)->rows + (int(0)))->y) + (*right_4).primal_0.rows[int(1)].z * dOut_5.rows[int(0)].z;
    *&(((&right_d_result_2)->rows + (int(1)))->z) = *&(((&right_d_result_2)->rows + (int(1)))->z) + (*left_4).primal_0.rows[int(0)].y * dOut_5.rows[int(0)].z;
    *&(((&left_d_result_2)->rows + (int(0)))->z) = *&(((&left_d_result_2)->rows + (int(0)))->z) + (*right_4).primal_0.rows[int(2)].z * dOut_5.rows[int(0)].z;
    *&(((&right_d_result_2)->rows + (int(2)))->z) = *&(((&right_d_result_2)->rows + (int(2)))->z) + (*left_4).primal_0.rows[int(0)].z * dOut_5.rows[int(0)].z;
    *&(((&left_d_result_2)->rows + (int(1)))->x) = *&(((&left_d_result_2)->rows + (int(1)))->x) + (*right_4).primal_0.rows[int(0)].x * dOut_5.rows[int(1)].x;
    *&(((&right_d_result_2)->rows + (int(0)))->x) = *&(((&right_d_result_2)->rows + (int(0)))->x) + (*left_4).primal_0.rows[int(1)].x * dOut_5.rows[int(1)].x;
    *&(((&left_d_result_2)->rows + (int(1)))->y) = *&(((&left_d_result_2)->rows + (int(1)))->y) + (*right_4).primal_0.rows[int(1)].x * dOut_5.rows[int(1)].x;
    *&(((&right_d_result_2)->rows + (int(1)))->x) = *&(((&right_d_result_2)->rows + (int(1)))->x) + (*left_4).primal_0.rows[int(1)].y * dOut_5.rows[int(1)].x;
    *&(((&left_d_result_2)->rows + (int(1)))->z) = *&(((&left_d_result_2)->rows + (int(1)))->z) + (*right_4).primal_0.rows[int(2)].x * dOut_5.rows[int(1)].x;
    *&(((&right_d_result_2)->rows + (int(2)))->x) = *&(((&right_d_result_2)->rows + (int(2)))->x) + (*left_4).primal_0.rows[int(1)].z * dOut_5.rows[int(1)].x;
    *&(((&left_d_result_2)->rows + (int(1)))->x) = *&(((&left_d_result_2)->rows + (int(1)))->x) + (*right_4).primal_0.rows[int(0)].y * dOut_5.rows[int(1)].y;
    *&(((&right_d_result_2)->rows + (int(0)))->y) = *&(((&right_d_result_2)->rows + (int(0)))->y) + (*left_4).primal_0.rows[int(1)].x * dOut_5.rows[int(1)].y;
    *&(((&left_d_result_2)->rows + (int(1)))->y) = *&(((&left_d_result_2)->rows + (int(1)))->y) + (*right_4).primal_0.rows[int(1)].y * dOut_5.rows[int(1)].y;
    *&(((&right_d_result_2)->rows + (int(1)))->y) = *&(((&right_d_result_2)->rows + (int(1)))->y) + (*left_4).primal_0.rows[int(1)].y * dOut_5.rows[int(1)].y;
    *&(((&left_d_result_2)->rows + (int(1)))->z) = *&(((&left_d_result_2)->rows + (int(1)))->z) + (*right_4).primal_0.rows[int(2)].y * dOut_5.rows[int(1)].y;
    *&(((&right_d_result_2)->rows + (int(2)))->y) = *&(((&right_d_result_2)->rows + (int(2)))->y) + (*left_4).primal_0.rows[int(1)].z * dOut_5.rows[int(1)].y;
    *&(((&left_d_result_2)->rows + (int(1)))->x) = *&(((&left_d_result_2)->rows + (int(1)))->x) + (*right_4).primal_0.rows[int(0)].z * dOut_5.rows[int(1)].z;
    *&(((&right_d_result_2)->rows + (int(0)))->z) = *&(((&right_d_result_2)->rows + (int(0)))->z) + (*left_4).primal_0.rows[int(1)].x * dOut_5.rows[int(1)].z;
    *&(((&left_d_result_2)->rows + (int(1)))->y) = *&(((&left_d_result_2)->rows + (int(1)))->y) + (*right_4).primal_0.rows[int(1)].z * dOut_5.rows[int(1)].z;
    *&(((&right_d_result_2)->rows + (int(1)))->z) = *&(((&right_d_result_2)->rows + (int(1)))->z) + (*left_4).primal_0.rows[int(1)].y * dOut_5.rows[int(1)].z;
    *&(((&left_d_result_2)->rows + (int(1)))->z) = *&(((&left_d_result_2)->rows + (int(1)))->z) + (*right_4).primal_0.rows[int(2)].z * dOut_5.rows[int(1)].z;
    *&(((&right_d_result_2)->rows + (int(2)))->z) = *&(((&right_d_result_2)->rows + (int(2)))->z) + (*left_4).primal_0.rows[int(1)].z * dOut_5.rows[int(1)].z;
    *&(((&left_d_result_2)->rows + (int(2)))->x) = *&(((&left_d_result_2)->rows + (int(2)))->x) + (*right_4).primal_0.rows[int(0)].x * dOut_5.rows[int(2)].x;
    *&(((&right_d_result_2)->rows + (int(0)))->x) = *&(((&right_d_result_2)->rows + (int(0)))->x) + (*left_4).primal_0.rows[int(2)].x * dOut_5.rows[int(2)].x;
    *&(((&left_d_result_2)->rows + (int(2)))->y) = *&(((&left_d_result_2)->rows + (int(2)))->y) + (*right_4).primal_0.rows[int(1)].x * dOut_5.rows[int(2)].x;
    *&(((&right_d_result_2)->rows + (int(1)))->x) = *&(((&right_d_result_2)->rows + (int(1)))->x) + (*left_4).primal_0.rows[int(2)].y * dOut_5.rows[int(2)].x;
    *&(((&left_d_result_2)->rows + (int(2)))->z) = *&(((&left_d_result_2)->rows + (int(2)))->z) + (*right_4).primal_0.rows[int(2)].x * dOut_5.rows[int(2)].x;
    *&(((&right_d_result_2)->rows + (int(2)))->x) = *&(((&right_d_result_2)->rows + (int(2)))->x) + (*left_4).primal_0.rows[int(2)].z * dOut_5.rows[int(2)].x;
    *&(((&left_d_result_2)->rows + (int(2)))->x) = *&(((&left_d_result_2)->rows + (int(2)))->x) + (*right_4).primal_0.rows[int(0)].y * dOut_5.rows[int(2)].y;
    *&(((&right_d_result_2)->rows + (int(0)))->y) = *&(((&right_d_result_2)->rows + (int(0)))->y) + (*left_4).primal_0.rows[int(2)].x * dOut_5.rows[int(2)].y;
    *&(((&left_d_result_2)->rows + (int(2)))->y) = *&(((&left_d_result_2)->rows + (int(2)))->y) + (*right_4).primal_0.rows[int(1)].y * dOut_5.rows[int(2)].y;
    *&(((&right_d_result_2)->rows + (int(1)))->y) = *&(((&right_d_result_2)->rows + (int(1)))->y) + (*left_4).primal_0.rows[int(2)].y * dOut_5.rows[int(2)].y;
    *&(((&left_d_result_2)->rows + (int(2)))->z) = *&(((&left_d_result_2)->rows + (int(2)))->z) + (*right_4).primal_0.rows[int(2)].y * dOut_5.rows[int(2)].y;
    *&(((&right_d_result_2)->rows + (int(2)))->y) = *&(((&right_d_result_2)->rows + (int(2)))->y) + (*left_4).primal_0.rows[int(2)].z * dOut_5.rows[int(2)].y;
    *&(((&left_d_result_2)->rows + (int(2)))->x) = *&(((&left_d_result_2)->rows + (int(2)))->x) + (*right_4).primal_0.rows[int(0)].z * dOut_5.rows[int(2)].z;
    *&(((&right_d_result_2)->rows + (int(0)))->z) = *&(((&right_d_result_2)->rows + (int(0)))->z) + (*left_4).primal_0.rows[int(2)].x * dOut_5.rows[int(2)].z;
    *&(((&left_d_result_2)->rows + (int(2)))->y) = *&(((&left_d_result_2)->rows + (int(2)))->y) + (*right_4).primal_0.rows[int(1)].z * dOut_5.rows[int(2)].z;
    *&(((&right_d_result_2)->rows + (int(1)))->z) = *&(((&right_d_result_2)->rows + (int(1)))->z) + (*left_4).primal_0.rows[int(2)].y * dOut_5.rows[int(2)].z;
    *&(((&left_d_result_2)->rows + (int(2)))->z) = *&(((&left_d_result_2)->rows + (int(2)))->z) + (*right_4).primal_0.rows[int(2)].z * dOut_5.rows[int(2)].z;
    *&(((&right_d_result_2)->rows + (int(2)))->z) = *&(((&right_d_result_2)->rows + (int(2)))->z) + (*left_4).primal_0.rows[int(2)].z * dOut_5.rows[int(2)].z;
    left_4->primal_0 = (*left_4).primal_0;
    left_4->differential_0 = left_d_result_2;
    right_4->primal_0 = (*right_4).primal_0;
    right_4->differential_0 = right_d_result_2;
    return;
}

inline __device__ Matrix<float, 3, 3>  mul_3(Matrix<float, 3, 3>  left_5, Matrix<float, 3, 3>  right_5)
{
    Matrix<float, 3, 3>  result_9;
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
            int i_2 = int(0);
            float sum_12 = 0.0f;
            for(;;)
            {
                if(i_2 < int(3))
                {
                }
                else
                {
                    break;
                }
                float sum_13 = sum_12 + _slang_vector_get_element(left_5.rows[r_1], i_2) * _slang_vector_get_element(right_5.rows[i_2], c_0);
                i_2 = i_2 + int(1);
                sum_12 = sum_13;
            }
            *_slang_vector_get_element_ptr(((&result_9)->rows + (r_1)), c_0) = sum_12;
            c_0 = c_0 + int(1);
        }
        r_1 = r_1 + int(1);
    }
    return result_9;
}

inline __device__ void _d_cross_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * a_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * b_1, float3  dOut_6)
{
    float _S21 = dOut_6.y;
    float _S22 = dOut_6.z;
    float _S23 = dOut_6.x;
    float _S24 = (*a_0).primal_0.z * _S21 + - (*a_0).primal_0.y * _S22;
    float _S25 = - (*a_0).primal_0.z * _S23 + (*a_0).primal_0.x * _S22;
    float _S26 = (*a_0).primal_0.y * _S23 + - (*a_0).primal_0.x * _S21;
    float3  _S27 = make_float3 (- (*b_1).primal_0.z * _S21 + (*b_1).primal_0.y * _S22, (*b_1).primal_0.z * _S23 + - (*b_1).primal_0.x * _S22, - (*b_1).primal_0.y * _S23 + (*b_1).primal_0.x * _S21);
    a_0->primal_0 = (*a_0).primal_0;
    a_0->differential_0 = _S27;
    float3  _S28 = make_float3 (_S24, _S25, _S26);
    b_1->primal_0 = (*b_1).primal_0;
    b_1->differential_0 = _S28;
    return;
}

inline __device__ float3  cross_0(float3  left_6, float3  right_6)
{
    float _S29 = left_6.y;
    float _S30 = right_6.z;
    float _S31 = left_6.z;
    float _S32 = right_6.y;
    float _S33 = right_6.x;
    float _S34 = left_6.x;
    return make_float3 (_S29 * _S30 - _S31 * _S32, _S31 * _S33 - _S34 * _S30, _S34 * _S32 - _S29 * _S33);
}

inline __device__ void _d_dot_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_3, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpy_1, float dOut_7)
{
    float3  x_d_result_0;
    *&((&x_d_result_0)->x) = (*dpy_1).primal_0.x * dOut_7;
    float3  y_d_result_0;
    *&((&y_d_result_0)->x) = (*dpx_3).primal_0.x * dOut_7;
    *&((&x_d_result_0)->y) = (*dpy_1).primal_0.y * dOut_7;
    *&((&y_d_result_0)->y) = (*dpx_3).primal_0.y * dOut_7;
    *&((&x_d_result_0)->z) = (*dpy_1).primal_0.z * dOut_7;
    *&((&y_d_result_0)->z) = (*dpx_3).primal_0.z * dOut_7;
    dpx_3->primal_0 = (*dpx_3).primal_0;
    dpx_3->differential_0 = x_d_result_0;
    dpy_1->primal_0 = (*dpy_1).primal_0;
    dpy_1->differential_0 = y_d_result_0;
    return;
}

inline __device__ float dot_0(float3  x_1, float3  y_0)
{
    int i_3 = int(0);
    float result_10 = 0.0f;
    for(;;)
    {
        if(i_3 < int(3))
        {
        }
        else
        {
            break;
        }
        float result_11 = result_10 + _slang_vector_get_element(x_1, i_3) * _slang_vector_get_element(y_0, i_3);
        i_3 = i_3 + int(1);
        result_10 = result_11;
    }
    return result_10;
}

inline __device__ void _d_abs_0(DiffPair_float_0 * dpx_4, float dOut_8)
{
    float _S35 = _slang_select(((*dpx_4).primal_0) > 0.0f, 1.0f,_slang_select(((*dpx_4).primal_0) == 0.0f, 0.0f,-1.0f)) * dOut_8;
    dpx_4->primal_0 = (*dpx_4).primal_0;
    dpx_4->differential_0 = _S35;
    return;
}

inline __device__ float3  min_0(float3  x_2, float3  y_1)
{
    float3  result_12;
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
        *_slang_vector_get_element_ptr(&result_12, i_4) = (F32_min((_slang_vector_get_element(x_2, i_4)), (_slang_vector_get_element(y_1, i_4))));
        i_4 = i_4 + int(1);
    }
    return result_12;
}

inline __device__ float3  max_0(float3  x_3, float3  y_2)
{
    float3  result_13;
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
        *_slang_vector_get_element_ptr(&result_13, i_5) = (F32_max((_slang_vector_get_element(x_3, i_5)), (_slang_vector_get_element(y_2, i_5))));
        i_5 = i_5 + int(1);
    }
    return result_13;
}

inline __device__ void _d_clamp_vector_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_5, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpy_2, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpz_0, float3  dOut_9)
{
    DiffPair_float_0 left_dp_0;
    (&left_dp_0)->primal_0 = (*dpx_5).primal_0.x;
    (&left_dp_0)->differential_0 = 0.0f;
    DiffPair_float_0 middle_dp_0;
    (&middle_dp_0)->primal_0 = (*dpy_2).primal_0.x;
    (&middle_dp_0)->differential_0 = 0.0f;
    DiffPair_float_0 right_dp_0;
    (&right_dp_0)->primal_0 = (*dpz_0).primal_0.x;
    (&right_dp_0)->differential_0 = 0.0f;
    _d_clamp_0(&left_dp_0, &middle_dp_0, &right_dp_0, dOut_9.x);
    float3  left_d_result_3;
    *&((&left_d_result_3)->x) = left_dp_0.differential_0;
    float3  middle_d_result_0;
    *&((&middle_d_result_0)->x) = middle_dp_0.differential_0;
    float3  right_d_result_3;
    *&((&right_d_result_3)->x) = right_dp_0.differential_0;
    DiffPair_float_0 left_dp_1;
    (&left_dp_1)->primal_0 = (*dpx_5).primal_0.y;
    (&left_dp_1)->differential_0 = 0.0f;
    DiffPair_float_0 middle_dp_1;
    (&middle_dp_1)->primal_0 = (*dpy_2).primal_0.y;
    (&middle_dp_1)->differential_0 = 0.0f;
    DiffPair_float_0 right_dp_1;
    (&right_dp_1)->primal_0 = (*dpz_0).primal_0.y;
    (&right_dp_1)->differential_0 = 0.0f;
    _d_clamp_0(&left_dp_1, &middle_dp_1, &right_dp_1, dOut_9.y);
    *&((&left_d_result_3)->y) = left_dp_1.differential_0;
    *&((&middle_d_result_0)->y) = middle_dp_1.differential_0;
    *&((&right_d_result_3)->y) = right_dp_1.differential_0;
    DiffPair_float_0 left_dp_2;
    (&left_dp_2)->primal_0 = (*dpx_5).primal_0.z;
    (&left_dp_2)->differential_0 = 0.0f;
    DiffPair_float_0 middle_dp_2;
    (&middle_dp_2)->primal_0 = (*dpy_2).primal_0.z;
    (&middle_dp_2)->differential_0 = 0.0f;
    DiffPair_float_0 right_dp_2;
    (&right_dp_2)->primal_0 = (*dpz_0).primal_0.z;
    (&right_dp_2)->differential_0 = 0.0f;
    _d_clamp_0(&left_dp_2, &middle_dp_2, &right_dp_2, dOut_9.z);
    *&((&left_d_result_3)->z) = left_dp_2.differential_0;
    *&((&middle_d_result_0)->z) = middle_dp_2.differential_0;
    *&((&right_d_result_3)->z) = right_dp_2.differential_0;
    dpx_5->primal_0 = (*dpx_5).primal_0;
    dpx_5->differential_0 = left_d_result_3;
    dpy_2->primal_0 = (*dpy_2).primal_0;
    dpy_2->differential_0 = middle_d_result_0;
    dpz_0->primal_0 = (*dpz_0).primal_0;
    dpz_0->differential_0 = right_d_result_3;
    return;
}

inline __device__ float3  clamp_1(float3  x_4, float3  minBound_1, float3  maxBound_1)
{
    return min_0(max_0(x_4, minBound_1), maxBound_1);
}

inline __device__ void _d_exp_0(DiffPair_float_0 * dpx_6, float dOut_10)
{
    float _S36 = (F32_exp(((*dpx_6).primal_0))) * dOut_10;
    dpx_6->primal_0 = (*dpx_6).primal_0;
    dpx_6->differential_0 = _S36;
    return;
}

inline __device__ void _d_log_0(DiffPair_float_0 * dpx_7, float dOut_11)
{
    float _S37 = 1.0f / (*dpx_7).primal_0 * dOut_11;
    dpx_7->primal_0 = (*dpx_7).primal_0;
    dpx_7->differential_0 = _S37;
    return;
}

inline __device__ void _d_lerp_0(DiffPair_float_0 * dpx_8, DiffPair_float_0 * dpy_3, DiffPair_float_0 * dps_0, float dOut_12)
{
    float _S38 = (1.0f - (*dps_0).primal_0) * dOut_12;
    dpx_8->primal_0 = (*dpx_8).primal_0;
    dpx_8->differential_0 = _S38;
    DiffPair_float_0 _S39 = *dpy_3;
    float _S40 = (*dps_0).primal_0 * dOut_12;
    dpy_3->primal_0 = (*dpy_3).primal_0;
    dpy_3->differential_0 = _S40;
    float _S41 = (_S39.primal_0 - (*dpx_8).primal_0) * dOut_12;
    dps_0->primal_0 = _S39.primal_0;
    dps_0->differential_0 = _S41;
    return;
}

inline __device__ float lerp_0(float x_5, float y_3, float s_0)
{
    return x_5 + (y_3 - x_5) * s_0;
}

inline __device__ void _d_pow_0(DiffPair_float_0 * dpx_9, DiffPair_float_0 * dpy_4, float dOut_13)
{
    if(((*dpx_9).primal_0) < 9.99999997475242708e-07f)
    {
        dpx_9->primal_0 = (*dpx_9).primal_0;
        dpx_9->differential_0 = 0.0f;
        dpy_4->primal_0 = (*dpy_4).primal_0;
        dpy_4->differential_0 = 0.0f;
    }
    else
    {
        float val_0 = (F32_pow(((*dpx_9).primal_0), ((*dpy_4).primal_0)));
        DiffPair_float_0 _S42 = *dpx_9;
        float _S43 = val_0 * (*dpy_4).primal_0 / (*dpx_9).primal_0 * dOut_13;
        dpx_9->primal_0 = (*dpx_9).primal_0;
        dpx_9->differential_0 = _S43;
        float _S44 = val_0 * (F32_log((_S42.primal_0))) * dOut_13;
        dpy_4->primal_0 = (*dpy_4).primal_0;
        dpy_4->differential_0 = _S44;
    }
    return;
}

inline __device__ float3  apply_ppisp(float3  rgb_in_0, float2  pix_coord_0, float2  image_center_0, float2  img_size_0, FixedArray<float, 36>  params_0)
{
    PPISPParams_0 p_0;
    (&p_0)->exposure_2 = params_0[int(0)];
    (&(&p_0)->vignette_params_2[int(0)])->cx_0 = params_0[int(1)];
    (&(&p_0)->vignette_params_2[int(0)])->cy_0 = params_0[int(2)];
    (&(&p_0)->vignette_params_2[int(0)])->alpha0_0 = params_0[int(3)];
    (&(&p_0)->vignette_params_2[int(0)])->alpha1_0 = params_0[int(4)];
    (&(&p_0)->vignette_params_2[int(0)])->alpha2_0 = params_0[int(5)];
    (&(&p_0)->vignette_params_2[int(1)])->cx_0 = params_0[int(6)];
    (&(&p_0)->vignette_params_2[int(1)])->cy_0 = params_0[int(7)];
    (&(&p_0)->vignette_params_2[int(1)])->alpha0_0 = params_0[int(8)];
    (&(&p_0)->vignette_params_2[int(1)])->alpha1_0 = params_0[int(9)];
    (&(&p_0)->vignette_params_2[int(1)])->alpha2_0 = params_0[int(10)];
    (&(&p_0)->vignette_params_2[int(2)])->cx_0 = params_0[int(11)];
    (&(&p_0)->vignette_params_2[int(2)])->cy_0 = params_0[int(12)];
    (&(&p_0)->vignette_params_2[int(2)])->alpha0_0 = params_0[int(13)];
    (&(&p_0)->vignette_params_2[int(2)])->alpha1_0 = params_0[int(14)];
    (&(&p_0)->vignette_params_2[int(2)])->alpha2_0 = params_0[int(15)];
    *&((&(&(&p_0)->color_params_2)->b_0)->x) = params_0[int(16)];
    *&((&(&(&p_0)->color_params_2)->b_0)->y) = params_0[int(17)];
    *&((&(&(&p_0)->color_params_2)->r_0)->x) = params_0[int(18)];
    *&((&(&(&p_0)->color_params_2)->r_0)->y) = params_0[int(19)];
    *&((&(&(&p_0)->color_params_2)->g_0)->x) = params_0[int(20)];
    *&((&(&(&p_0)->color_params_2)->g_0)->y) = params_0[int(21)];
    *&((&(&(&p_0)->color_params_2)->n_0)->x) = params_0[int(22)];
    *&((&(&(&p_0)->color_params_2)->n_0)->y) = params_0[int(23)];
    (&(&p_0)->crf_params_1[int(0)])->toe_0 = params_0[int(24)];
    (&(&p_0)->crf_params_1[int(0)])->shoulder_0 = params_0[int(25)];
    (&(&p_0)->crf_params_1[int(0)])->gamma_0 = params_0[int(26)];
    (&(&p_0)->crf_params_1[int(0)])->center_0 = params_0[int(27)];
    (&(&p_0)->crf_params_1[int(1)])->toe_0 = params_0[int(28)];
    (&(&p_0)->crf_params_1[int(1)])->shoulder_0 = params_0[int(29)];
    (&(&p_0)->crf_params_1[int(1)])->gamma_0 = params_0[int(30)];
    (&(&p_0)->crf_params_1[int(1)])->center_0 = params_0[int(31)];
    (&(&p_0)->crf_params_1[int(2)])->toe_0 = params_0[int(32)];
    (&(&p_0)->crf_params_1[int(2)])->shoulder_0 = params_0[int(33)];
    (&(&p_0)->crf_params_1[int(2)])->gamma_0 = params_0[int(34)];
    (&(&p_0)->crf_params_1[int(2)])->center_0 = params_0[int(35)];
    PPISPParams_0 _S45 = p_0;
    float _S46 = (F32_max((img_size_0.x), (img_size_0.y)));
    float _S47 = (pix_coord_0.x - image_center_0.x) / _S46;
    float _S48 = (pix_coord_0.y - image_center_0.y) / _S46;
    float3  rgb_out_0 = rgb_in_0 * make_float3 ((F32_exp2((p_0.exposure_2))));
    float dx_0 = _S47 - p_0.vignette_params_2[int(0)].cx_0;
    float dy_0 = _S48 - p_0.vignette_params_2[int(0)].cy_0;
    float r2_0 = dx_0 * dx_0 + dy_0 * dy_0;
    float r4_0 = r2_0 * r2_0;
    *&((&rgb_out_0)->x) = *&((&rgb_out_0)->x) * clamp_0(p_0.vignette_params_2[int(0)].alpha2_0 * (r4_0 * r2_0) + p_0.vignette_params_2[int(0)].alpha1_0 * r4_0 + p_0.vignette_params_2[int(0)].alpha0_0 * r2_0 + 1.0f, 0.0f, 1.0f);
    float dx_1 = _S47 - p_0.vignette_params_2[int(1)].cx_0;
    float dy_1 = _S48 - p_0.vignette_params_2[int(1)].cy_0;
    float r2_1 = dx_1 * dx_1 + dy_1 * dy_1;
    float r4_1 = r2_1 * r2_1;
    *&((&rgb_out_0)->y) = *&((&rgb_out_0)->y) * clamp_0(p_0.vignette_params_2[int(1)].alpha2_0 * (r4_1 * r2_1) + p_0.vignette_params_2[int(1)].alpha1_0 * r4_1 + p_0.vignette_params_2[int(1)].alpha0_0 * r2_1 + 1.0f, 0.0f, 1.0f);
    float dx_2 = _S47 - p_0.vignette_params_2[int(2)].cx_0;
    float dy_2 = _S48 - p_0.vignette_params_2[int(2)].cy_0;
    float r2_2 = dx_2 * dx_2 + dy_2 * dy_2;
    float r4_2 = r2_2 * r2_2;
    *&((&rgb_out_0)->z) = *&((&rgb_out_0)->z) * clamp_0(p_0.vignette_params_2[int(2)].alpha2_0 * (r4_2 * r2_2) + p_0.vignette_params_2[int(2)].alpha1_0 * r4_2 + p_0.vignette_params_2[int(2)].alpha0_0 * r2_2 + 1.0f, 0.0f, 1.0f);
    float3  _S49 = rgb_out_0;
    float2  bd_0 = mul_0(makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f), p_0.color_params_2.b_0);
    float2  rd_0 = mul_0(makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f), p_0.color_params_2.r_0);
    float2  gd_0 = mul_0(makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f), p_0.color_params_2.g_0);
    float2  nd_0 = mul_0(makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f), p_0.color_params_2.n_0);
    float _S50 = 0.3333333432674408f + nd_0.x;
    float _S51 = 0.3333333432674408f + nd_0.y;
    Matrix<float, 3, 3>  T_0 = makeMatrix<float, 3, 3> (bd_0.x, 1.0f + rd_0.x, gd_0.x, bd_0.y, rd_0.y, 1.0f + gd_0.y, 1.0f, 1.0f, 1.0f);
    Matrix<float, 3, 3>  M_0 = mul_3(makeMatrix<float, 3, 3> (0.0f, -1.0f, _S51, 1.0f, 0.0f, - _S50, - _S51, _S50, 0.0f), T_0);
    float3  r0_0 = make_float3 (M_0.rows[int(0)].x, M_0.rows[int(0)].y, M_0.rows[int(0)].z);
    float3  r1_0 = make_float3 (M_0.rows[int(1)].x, M_0.rows[int(1)].y, M_0.rows[int(1)].z);
    float3  r2_3 = make_float3 (M_0.rows[int(2)].x, M_0.rows[int(2)].y, M_0.rows[int(2)].z);
    float3  lambda_v_0 = cross_0(r0_0, r1_0);
    float3  lambda_v_1;
    if((dot_0(lambda_v_0, lambda_v_0)) < 9.99999968265522539e-21f)
    {
        float3  lambda_v_2 = cross_0(r0_0, r2_3);
        if((dot_0(lambda_v_2, lambda_v_2)) < 9.99999968265522539e-21f)
        {
            lambda_v_1 = cross_0(r1_0, r2_3);
        }
        else
        {
            lambda_v_1 = lambda_v_2;
        }
    }
    else
    {
        lambda_v_1 = lambda_v_0;
    }
    Matrix<float, 3, 3>  H_0 = mul_3(mul_3(T_0, makeMatrix<float, 3, 3> (lambda_v_1.x, 0.0f, 0.0f, 0.0f, lambda_v_1.y, 0.0f, 0.0f, 0.0f, lambda_v_1.z)), makeMatrix<float, 3, 3> (-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
    Matrix<float, 3, 3>  H_1;
    if((F32_abs((H_0.rows[int(2)].z))) > 9.99999968265522539e-21f)
    {
        H_1 = H_0 * makeMatrix<float, 3, 3> (1.0f / H_0.rows[int(2)].z);
    }
    else
    {
        H_1 = H_0;
    }
    float _S52 = _S49.x;
    float _S53 = _S49.y;
    float intensity_0 = _S52 + _S53 + _S49.z;
    float3  rgi_out_0 = mul_1(H_1, make_float3 (_S52, _S53, intensity_0));
    float norm_factor_0 = intensity_0 / (F32_max((rgi_out_0.z), (0.05000000074505806f * (F32_abs((intensity_0))) + 9.99999993922529029e-09f)));
    float out_r_0 = rgi_out_0.x * norm_factor_0;
    float out_g_0 = rgi_out_0.y * norm_factor_0;
    float3  _S54 = clamp_1(make_float3 (out_r_0, out_g_0, intensity_0 - out_r_0 - out_g_0), make_float3 (0.0f), make_float3 (1.0f));
    float3  rgb_out_1;
    float _S55 = _S54.x;
    float _S56 = 0.30000001192092896f + (F32_log((1.0f + (F32_exp((_S45.crf_params_1[int(0)].toe_0))))));
    float _S57 = 0.30000001192092896f + (F32_log((1.0f + (F32_exp((_S45.crf_params_1[int(0)].shoulder_0))))));
    float _S58 = 0.10000000149011612f + (F32_log((1.0f + (F32_exp((_S45.crf_params_1[int(0)].gamma_0))))));
    float _S59 = 1.0f / (1.0f + (F32_exp((- _S45.crf_params_1[int(0)].center_0))));
    float a_1 = _S57 * _S59 / lerp_0(_S56, _S57, _S59);
    float b_2 = 1.0f - a_1;
    float y_4;
    if(_S55 <= _S59)
    {
        y_4 = a_1 * (F32_pow((_S55 / _S59), (_S56)));
    }
    else
    {
        y_4 = 1.0f - b_2 * (F32_pow(((1.0f - _S55) / (1.0f - _S59)), (_S57)));
    }
    *&((&rgb_out_1)->x) = (F32_pow(((F32_max((0.0f), (y_4)))), (_S58)));
    float _S60 = _S54.y;
    float _S61 = 0.30000001192092896f + (F32_log((1.0f + (F32_exp((_S45.crf_params_1[int(1)].toe_0))))));
    float _S62 = 0.30000001192092896f + (F32_log((1.0f + (F32_exp((_S45.crf_params_1[int(1)].shoulder_0))))));
    float _S63 = 0.10000000149011612f + (F32_log((1.0f + (F32_exp((_S45.crf_params_1[int(1)].gamma_0))))));
    float _S64 = 1.0f / (1.0f + (F32_exp((- _S45.crf_params_1[int(1)].center_0))));
    float a_2 = _S62 * _S64 / lerp_0(_S61, _S62, _S64);
    float b_3 = 1.0f - a_2;
    if(_S60 <= _S64)
    {
        y_4 = a_2 * (F32_pow((_S60 / _S64), (_S61)));
    }
    else
    {
        y_4 = 1.0f - b_3 * (F32_pow(((1.0f - _S60) / (1.0f - _S64)), (_S62)));
    }
    *&((&rgb_out_1)->y) = (F32_pow(((F32_max((0.0f), (y_4)))), (_S63)));
    float _S65 = _S54.z;
    float _S66 = 0.30000001192092896f + (F32_log((1.0f + (F32_exp((_S45.crf_params_1[int(2)].toe_0))))));
    float _S67 = 0.30000001192092896f + (F32_log((1.0f + (F32_exp((_S45.crf_params_1[int(2)].shoulder_0))))));
    float _S68 = 0.10000000149011612f + (F32_log((1.0f + (F32_exp((_S45.crf_params_1[int(2)].gamma_0))))));
    float _S69 = 1.0f / (1.0f + (F32_exp((- _S45.crf_params_1[int(2)].center_0))));
    float a_3 = _S67 * _S69 / lerp_0(_S66, _S67, _S69);
    float b_4 = 1.0f - a_3;
    if(_S65 <= _S69)
    {
        y_4 = a_3 * (F32_pow((_S65 / _S69), (_S66)));
    }
    else
    {
        y_4 = 1.0f - b_4 * (F32_pow(((1.0f - _S65) / (1.0f - _S69)), (_S67)));
    }
    *&((&rgb_out_1)->z) = (F32_pow(((F32_max((0.0f), (y_4)))), (_S68)));
    return rgb_out_1;
}

inline __device__ float3  apply_ppisp_rqs(float3  rgb_in_1, float2  pix_coord_1, float2  image_center_1, float2  img_size_1, FixedArray<float, 39>  params_1)
{
    PPISPParamsRQS_0 p_1;
    (&p_1)->exposure_1 = params_1[int(0)];
    (&(&p_1)->vignette_params_1[int(0)])->cx_0 = params_1[int(1)];
    (&(&p_1)->vignette_params_1[int(0)])->cy_0 = params_1[int(2)];
    (&(&p_1)->vignette_params_1[int(0)])->alpha0_0 = params_1[int(3)];
    (&(&p_1)->vignette_params_1[int(0)])->alpha1_0 = params_1[int(4)];
    (&(&p_1)->vignette_params_1[int(0)])->alpha2_0 = params_1[int(5)];
    (&(&p_1)->vignette_params_1[int(1)])->cx_0 = params_1[int(6)];
    (&(&p_1)->vignette_params_1[int(1)])->cy_0 = params_1[int(7)];
    (&(&p_1)->vignette_params_1[int(1)])->alpha0_0 = params_1[int(8)];
    (&(&p_1)->vignette_params_1[int(1)])->alpha1_0 = params_1[int(9)];
    (&(&p_1)->vignette_params_1[int(1)])->alpha2_0 = params_1[int(10)];
    (&(&p_1)->vignette_params_1[int(2)])->cx_0 = params_1[int(11)];
    (&(&p_1)->vignette_params_1[int(2)])->cy_0 = params_1[int(12)];
    (&(&p_1)->vignette_params_1[int(2)])->alpha0_0 = params_1[int(13)];
    (&(&p_1)->vignette_params_1[int(2)])->alpha1_0 = params_1[int(14)];
    (&(&p_1)->vignette_params_1[int(2)])->alpha2_0 = params_1[int(15)];
    *&((&(&(&p_1)->color_params_1)->b_0)->x) = params_1[int(16)];
    *&((&(&(&p_1)->color_params_1)->b_0)->y) = params_1[int(17)];
    *&((&(&(&p_1)->color_params_1)->r_0)->x) = params_1[int(18)];
    *&((&(&(&p_1)->color_params_1)->r_0)->y) = params_1[int(19)];
    *&((&(&(&p_1)->color_params_1)->g_0)->x) = params_1[int(20)];
    *&((&(&(&p_1)->color_params_1)->g_0)->y) = params_1[int(21)];
    *&((&(&(&p_1)->color_params_1)->n_0)->x) = params_1[int(22)];
    *&((&(&(&p_1)->color_params_1)->n_0)->y) = params_1[int(23)];
    (&(&p_1)->crf_params_0[int(0)])->g0_0 = params_1[int(24)];
    (&(&p_1)->crf_params_0[int(0)])->g1_0 = params_1[int(25)];
    (&(&p_1)->crf_params_0[int(0)])->x0_0 = params_1[int(26)];
    (&(&p_1)->crf_params_0[int(0)])->y0_0 = params_1[int(27)];
    (&(&p_1)->crf_params_0[int(0)])->gc_0 = params_1[int(28)];
    (&(&p_1)->crf_params_0[int(1)])->g0_0 = params_1[int(29)];
    (&(&p_1)->crf_params_0[int(1)])->g1_0 = params_1[int(30)];
    (&(&p_1)->crf_params_0[int(1)])->x0_0 = params_1[int(31)];
    (&(&p_1)->crf_params_0[int(1)])->y0_0 = params_1[int(32)];
    (&(&p_1)->crf_params_0[int(1)])->gc_0 = params_1[int(33)];
    (&(&p_1)->crf_params_0[int(2)])->g0_0 = params_1[int(34)];
    (&(&p_1)->crf_params_0[int(2)])->g1_0 = params_1[int(35)];
    (&(&p_1)->crf_params_0[int(2)])->x0_0 = params_1[int(36)];
    (&(&p_1)->crf_params_0[int(2)])->y0_0 = params_1[int(37)];
    (&(&p_1)->crf_params_0[int(2)])->gc_0 = params_1[int(38)];
    PPISPParamsRQS_0 _S70 = p_1;
    float _S71 = (F32_max((img_size_1.x), (img_size_1.y)));
    float _S72 = (pix_coord_1.x - image_center_1.x) / _S71;
    float _S73 = (pix_coord_1.y - image_center_1.y) / _S71;
    float3  rgb_out_2 = rgb_in_1 * make_float3 ((F32_exp2((p_1.exposure_1))));
    float dx_3 = _S72 - p_1.vignette_params_1[int(0)].cx_0;
    float dy_3 = _S73 - p_1.vignette_params_1[int(0)].cy_0;
    float r2_4 = dx_3 * dx_3 + dy_3 * dy_3;
    float r4_3 = r2_4 * r2_4;
    *&((&rgb_out_2)->x) = *&((&rgb_out_2)->x) * clamp_0(p_1.vignette_params_1[int(0)].alpha2_0 * (r4_3 * r2_4) + p_1.vignette_params_1[int(0)].alpha1_0 * r4_3 + p_1.vignette_params_1[int(0)].alpha0_0 * r2_4 + 1.0f, 0.0f, 1.0f);
    float dx_4 = _S72 - p_1.vignette_params_1[int(1)].cx_0;
    float dy_4 = _S73 - p_1.vignette_params_1[int(1)].cy_0;
    float r2_5 = dx_4 * dx_4 + dy_4 * dy_4;
    float r4_4 = r2_5 * r2_5;
    *&((&rgb_out_2)->y) = *&((&rgb_out_2)->y) * clamp_0(p_1.vignette_params_1[int(1)].alpha2_0 * (r4_4 * r2_5) + p_1.vignette_params_1[int(1)].alpha1_0 * r4_4 + p_1.vignette_params_1[int(1)].alpha0_0 * r2_5 + 1.0f, 0.0f, 1.0f);
    float dx_5 = _S72 - p_1.vignette_params_1[int(2)].cx_0;
    float dy_5 = _S73 - p_1.vignette_params_1[int(2)].cy_0;
    float r2_6 = dx_5 * dx_5 + dy_5 * dy_5;
    float r4_5 = r2_6 * r2_6;
    *&((&rgb_out_2)->z) = *&((&rgb_out_2)->z) * clamp_0(p_1.vignette_params_1[int(2)].alpha2_0 * (r4_5 * r2_6) + p_1.vignette_params_1[int(2)].alpha1_0 * r4_5 + p_1.vignette_params_1[int(2)].alpha0_0 * r2_6 + 1.0f, 0.0f, 1.0f);
    float3  _S74 = rgb_out_2;
    float2  bd_1 = mul_0(makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f), p_1.color_params_1.b_0);
    float2  rd_1 = mul_0(makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f), p_1.color_params_1.r_0);
    float2  gd_1 = mul_0(makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f), p_1.color_params_1.g_0);
    float2  nd_1 = mul_0(makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f), p_1.color_params_1.n_0);
    float _S75 = 0.3333333432674408f + nd_1.x;
    float _S76 = 0.3333333432674408f + nd_1.y;
    Matrix<float, 3, 3>  T_1 = makeMatrix<float, 3, 3> (bd_1.x, 1.0f + rd_1.x, gd_1.x, bd_1.y, rd_1.y, 1.0f + gd_1.y, 1.0f, 1.0f, 1.0f);
    Matrix<float, 3, 3>  M_1 = mul_3(makeMatrix<float, 3, 3> (0.0f, -1.0f, _S76, 1.0f, 0.0f, - _S75, - _S76, _S75, 0.0f), T_1);
    float3  r0_1 = make_float3 (M_1.rows[int(0)].x, M_1.rows[int(0)].y, M_1.rows[int(0)].z);
    float3  r1_1 = make_float3 (M_1.rows[int(1)].x, M_1.rows[int(1)].y, M_1.rows[int(1)].z);
    float3  r2_7 = make_float3 (M_1.rows[int(2)].x, M_1.rows[int(2)].y, M_1.rows[int(2)].z);
    float3  lambda_v_3 = cross_0(r0_1, r1_1);
    float3  lambda_v_4;
    if((dot_0(lambda_v_3, lambda_v_3)) < 9.99999968265522539e-21f)
    {
        float3  lambda_v_5 = cross_0(r0_1, r2_7);
        if((dot_0(lambda_v_5, lambda_v_5)) < 9.99999968265522539e-21f)
        {
            lambda_v_4 = cross_0(r1_1, r2_7);
        }
        else
        {
            lambda_v_4 = lambda_v_5;
        }
    }
    else
    {
        lambda_v_4 = lambda_v_3;
    }
    Matrix<float, 3, 3>  H_2 = mul_3(mul_3(T_1, makeMatrix<float, 3, 3> (lambda_v_4.x, 0.0f, 0.0f, 0.0f, lambda_v_4.y, 0.0f, 0.0f, 0.0f, lambda_v_4.z)), makeMatrix<float, 3, 3> (-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
    Matrix<float, 3, 3>  H_3;
    if((F32_abs((H_2.rows[int(2)].z))) > 9.99999968265522539e-21f)
    {
        H_3 = H_2 * makeMatrix<float, 3, 3> (1.0f / H_2.rows[int(2)].z);
    }
    else
    {
        H_3 = H_2;
    }
    float _S77 = _S74.x;
    float _S78 = _S74.y;
    float intensity_1 = _S77 + _S78 + _S74.z;
    float3  rgi_out_1 = mul_1(H_3, make_float3 (_S77, _S78, intensity_1));
    float norm_factor_1 = intensity_1 / (F32_max((rgi_out_1.z), (0.05000000074505806f * (F32_abs((intensity_1))) + 9.99999993922529029e-09f)));
    float out_r_1 = rgi_out_1.x * norm_factor_1;
    float out_g_1 = rgi_out_1.y * norm_factor_1;
    float3  _S79 = clamp_1(make_float3 (out_r_1, out_g_1, intensity_1 - out_r_1 - out_g_1), make_float3 (0.0f), make_float3 (1.0f));
    float3  rgb_out_3;
    float _S80 = _S79.x;
    float g0_1 = (F32_exp((_S70.crf_params_0[int(0)].g0_0)));
    float g1_1 = (F32_exp((_S70.crf_params_0[int(0)].g1_0)));
    float x0_1 = 1.0f / (1.0f + (F32_exp((- _S70.crf_params_0[int(0)].x0_0))));
    float y0_1 = 1.0f / (1.0f + (F32_exp((- _S70.crf_params_0[int(0)].y0_0))));
    float gc_1 = (F32_exp((_S70.crf_params_0[int(0)].gc_0)));
    float y_5;
    if(_S80 < x0_1)
    {
        float s0_0 = y0_1 / x0_1;
        float t0_0 = _S80 / x0_1;
        float _S81 = 1.0f - t0_0;
        y_5 = y0_1 * (s0_0 * t0_0 * t0_0 + g0_1 * t0_0 * _S81) / (s0_0 + (g0_1 + gc_1 - 2.0f * s0_0) * t0_0 * _S81);
    }
    else
    {
        float _S82 = 1.0f - y0_1;
        float _S83 = 1.0f - x0_1;
        float s1_0 = _S82 / _S83;
        float t1_0 = (_S80 - x0_1) / _S83;
        float _S84 = 1.0f - t1_0;
        y_5 = y0_1 + _S82 * (s1_0 * t1_0 * t1_0 + gc_1 * t1_0 * _S84) / (s1_0 + (gc_1 + g1_1 - 2.0f * s1_0) * t1_0 * _S84);
    }
    *&((&rgb_out_3)->x) = y_5;
    float _S85 = _S79.y;
    float g0_2 = (F32_exp((_S70.crf_params_0[int(1)].g0_0)));
    float g1_2 = (F32_exp((_S70.crf_params_0[int(1)].g1_0)));
    float x0_2 = 1.0f / (1.0f + (F32_exp((- _S70.crf_params_0[int(1)].x0_0))));
    float y0_2 = 1.0f / (1.0f + (F32_exp((- _S70.crf_params_0[int(1)].y0_0))));
    float gc_2 = (F32_exp((_S70.crf_params_0[int(1)].gc_0)));
    if(_S85 < x0_2)
    {
        float s0_1 = y0_2 / x0_2;
        float t0_1 = _S85 / x0_2;
        float _S86 = 1.0f - t0_1;
        y_5 = y0_2 * (s0_1 * t0_1 * t0_1 + g0_2 * t0_1 * _S86) / (s0_1 + (g0_2 + gc_2 - 2.0f * s0_1) * t0_1 * _S86);
    }
    else
    {
        float _S87 = 1.0f - y0_2;
        float _S88 = 1.0f - x0_2;
        float s1_1 = _S87 / _S88;
        float t1_1 = (_S85 - x0_2) / _S88;
        float _S89 = 1.0f - t1_1;
        y_5 = y0_2 + _S87 * (s1_1 * t1_1 * t1_1 + gc_2 * t1_1 * _S89) / (s1_1 + (gc_2 + g1_2 - 2.0f * s1_1) * t1_1 * _S89);
    }
    *&((&rgb_out_3)->y) = y_5;
    float _S90 = _S79.z;
    float g0_3 = (F32_exp((_S70.crf_params_0[int(2)].g0_0)));
    float g1_3 = (F32_exp((_S70.crf_params_0[int(2)].g1_0)));
    float x0_3 = 1.0f / (1.0f + (F32_exp((- _S70.crf_params_0[int(2)].x0_0))));
    float y0_3 = 1.0f / (1.0f + (F32_exp((- _S70.crf_params_0[int(2)].y0_0))));
    float gc_3 = (F32_exp((_S70.crf_params_0[int(2)].gc_0)));
    if(_S90 < x0_3)
    {
        float s0_2 = y0_3 / x0_3;
        float t0_2 = _S90 / x0_3;
        float _S91 = 1.0f - t0_2;
        y_5 = y0_3 * (s0_2 * t0_2 * t0_2 + g0_3 * t0_2 * _S91) / (s0_2 + (g0_3 + gc_3 - 2.0f * s0_2) * t0_2 * _S91);
    }
    else
    {
        float _S92 = 1.0f - y0_3;
        float _S93 = 1.0f - x0_3;
        float s1_2 = _S92 / _S93;
        float t1_2 = (_S90 - x0_3) / _S93;
        float _S94 = 1.0f - t1_2;
        y_5 = y0_3 + _S92 * (s1_2 * t1_2 * t1_2 + gc_3 * t1_2 * _S94) / (s1_2 + (gc_3 + g1_3 - 2.0f * s1_2) * t1_2 * _S94);
    }
    *&((&rgb_out_3)->z) = y_5;
    return rgb_out_3;
}

inline __device__ float3  apply_ppisp_no_crf(float3  rgb_in_2, float2  pix_coord_2, float2  image_center_2, float2  img_size_2, FixedArray<float, 24>  params_2)
{
    PPISPParamsNoCRF_0 p_2;
    (&p_2)->exposure_0 = params_2[int(0)];
    (&(&p_2)->vignette_params_0[int(0)])->cx_0 = params_2[int(1)];
    (&(&p_2)->vignette_params_0[int(0)])->cy_0 = params_2[int(2)];
    (&(&p_2)->vignette_params_0[int(0)])->alpha0_0 = params_2[int(3)];
    (&(&p_2)->vignette_params_0[int(0)])->alpha1_0 = params_2[int(4)];
    (&(&p_2)->vignette_params_0[int(0)])->alpha2_0 = params_2[int(5)];
    (&(&p_2)->vignette_params_0[int(1)])->cx_0 = params_2[int(6)];
    (&(&p_2)->vignette_params_0[int(1)])->cy_0 = params_2[int(7)];
    (&(&p_2)->vignette_params_0[int(1)])->alpha0_0 = params_2[int(8)];
    (&(&p_2)->vignette_params_0[int(1)])->alpha1_0 = params_2[int(9)];
    (&(&p_2)->vignette_params_0[int(1)])->alpha2_0 = params_2[int(10)];
    (&(&p_2)->vignette_params_0[int(2)])->cx_0 = params_2[int(11)];
    (&(&p_2)->vignette_params_0[int(2)])->cy_0 = params_2[int(12)];
    (&(&p_2)->vignette_params_0[int(2)])->alpha0_0 = params_2[int(13)];
    (&(&p_2)->vignette_params_0[int(2)])->alpha1_0 = params_2[int(14)];
    (&(&p_2)->vignette_params_0[int(2)])->alpha2_0 = params_2[int(15)];
    *&((&(&(&p_2)->color_params_0)->b_0)->x) = params_2[int(16)];
    *&((&(&(&p_2)->color_params_0)->b_0)->y) = params_2[int(17)];
    *&((&(&(&p_2)->color_params_0)->r_0)->x) = params_2[int(18)];
    *&((&(&(&p_2)->color_params_0)->r_0)->y) = params_2[int(19)];
    *&((&(&(&p_2)->color_params_0)->g_0)->x) = params_2[int(20)];
    *&((&(&(&p_2)->color_params_0)->g_0)->y) = params_2[int(21)];
    *&((&(&(&p_2)->color_params_0)->n_0)->x) = params_2[int(22)];
    *&((&(&(&p_2)->color_params_0)->n_0)->y) = params_2[int(23)];
    float _S95 = (F32_max((img_size_2.x), (img_size_2.y)));
    float _S96 = (pix_coord_2.x - image_center_2.x) / _S95;
    float _S97 = (pix_coord_2.y - image_center_2.y) / _S95;
    float3  rgb_out_4 = rgb_in_2 * make_float3 ((F32_exp2((p_2.exposure_0))));
    float dx_6 = _S96 - p_2.vignette_params_0[int(0)].cx_0;
    float dy_6 = _S97 - p_2.vignette_params_0[int(0)].cy_0;
    float r2_8 = dx_6 * dx_6 + dy_6 * dy_6;
    float r4_6 = r2_8 * r2_8;
    *&((&rgb_out_4)->x) = *&((&rgb_out_4)->x) * clamp_0(p_2.vignette_params_0[int(0)].alpha2_0 * (r4_6 * r2_8) + p_2.vignette_params_0[int(0)].alpha1_0 * r4_6 + p_2.vignette_params_0[int(0)].alpha0_0 * r2_8 + 1.0f, 0.0f, 1.0f);
    float dx_7 = _S96 - p_2.vignette_params_0[int(1)].cx_0;
    float dy_7 = _S97 - p_2.vignette_params_0[int(1)].cy_0;
    float r2_9 = dx_7 * dx_7 + dy_7 * dy_7;
    float r4_7 = r2_9 * r2_9;
    *&((&rgb_out_4)->y) = *&((&rgb_out_4)->y) * clamp_0(p_2.vignette_params_0[int(1)].alpha2_0 * (r4_7 * r2_9) + p_2.vignette_params_0[int(1)].alpha1_0 * r4_7 + p_2.vignette_params_0[int(1)].alpha0_0 * r2_9 + 1.0f, 0.0f, 1.0f);
    float dx_8 = _S96 - p_2.vignette_params_0[int(2)].cx_0;
    float dy_8 = _S97 - p_2.vignette_params_0[int(2)].cy_0;
    float r2_10 = dx_8 * dx_8 + dy_8 * dy_8;
    float r4_8 = r2_10 * r2_10;
    *&((&rgb_out_4)->z) = *&((&rgb_out_4)->z) * clamp_0(p_2.vignette_params_0[int(2)].alpha2_0 * (r4_8 * r2_10) + p_2.vignette_params_0[int(2)].alpha1_0 * r4_8 + p_2.vignette_params_0[int(2)].alpha0_0 * r2_10 + 1.0f, 0.0f, 1.0f);
    float3  _S98 = rgb_out_4;
    float2  bd_2 = mul_0(makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f), p_2.color_params_0.b_0);
    float2  rd_2 = mul_0(makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f), p_2.color_params_0.r_0);
    float2  gd_2 = mul_0(makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f), p_2.color_params_0.g_0);
    float2  nd_2 = mul_0(makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f), p_2.color_params_0.n_0);
    float _S99 = 0.3333333432674408f + nd_2.x;
    float _S100 = 0.3333333432674408f + nd_2.y;
    Matrix<float, 3, 3>  T_2 = makeMatrix<float, 3, 3> (bd_2.x, 1.0f + rd_2.x, gd_2.x, bd_2.y, rd_2.y, 1.0f + gd_2.y, 1.0f, 1.0f, 1.0f);
    Matrix<float, 3, 3>  M_2 = mul_3(makeMatrix<float, 3, 3> (0.0f, -1.0f, _S100, 1.0f, 0.0f, - _S99, - _S100, _S99, 0.0f), T_2);
    float3  r0_2 = make_float3 (M_2.rows[int(0)].x, M_2.rows[int(0)].y, M_2.rows[int(0)].z);
    float3  r1_2 = make_float3 (M_2.rows[int(1)].x, M_2.rows[int(1)].y, M_2.rows[int(1)].z);
    float3  r2_11 = make_float3 (M_2.rows[int(2)].x, M_2.rows[int(2)].y, M_2.rows[int(2)].z);
    float3  lambda_v_6 = cross_0(r0_2, r1_2);
    float3  lambda_v_7;
    if((dot_0(lambda_v_6, lambda_v_6)) < 9.99999968265522539e-21f)
    {
        float3  lambda_v_8 = cross_0(r0_2, r2_11);
        if((dot_0(lambda_v_8, lambda_v_8)) < 9.99999968265522539e-21f)
        {
            lambda_v_7 = cross_0(r1_2, r2_11);
        }
        else
        {
            lambda_v_7 = lambda_v_8;
        }
    }
    else
    {
        lambda_v_7 = lambda_v_6;
    }
    Matrix<float, 3, 3>  H_4 = mul_3(mul_3(T_2, makeMatrix<float, 3, 3> (lambda_v_7.x, 0.0f, 0.0f, 0.0f, lambda_v_7.y, 0.0f, 0.0f, 0.0f, lambda_v_7.z)), makeMatrix<float, 3, 3> (-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
    Matrix<float, 3, 3>  H_5;
    if((F32_abs((H_4.rows[int(2)].z))) > 9.99999968265522539e-21f)
    {
        H_5 = H_4 * makeMatrix<float, 3, 3> (1.0f / H_4.rows[int(2)].z);
    }
    else
    {
        H_5 = H_4;
    }
    float _S101 = _S98.x;
    float _S102 = _S98.y;
    float intensity_2 = _S101 + _S102 + _S98.z;
    float3  rgi_out_2 = mul_1(H_5, make_float3 (_S101, _S102, intensity_2));
    float norm_factor_2 = intensity_2 / (F32_max((rgi_out_2.z), (0.05000000074505806f * (F32_abs((intensity_2))) + 9.99999993922529029e-09f)));
    float out_r_2 = rgi_out_2.x * norm_factor_2;
    float out_g_2 = rgi_out_2.y * norm_factor_2;
    return make_float3 (out_r_2, out_g_2, intensity_2 - out_r_2 - out_g_2);
}

struct DiffPair_arrayx3Cfloatx2C36x3E_0
{
    FixedArray<float, 36>  primal_0;
    FixedArray<float, 36>  differential_0;
};

inline __device__ float s_primal_ctx_exp2_0(float _S103)
{
    return (F32_exp2((_S103)));
}

inline __device__ float s_primal_ctx_clamp_0(float _S104, float _S105, float _S106)
{
    return clamp_0(_S104, _S105, _S106);
}

inline __device__ float2  s_primal_ctx_mul_0(Matrix<float, 2, 2>  _S107, float2  _S108)
{
    return mul_0(_S107, _S108);
}

inline __device__ Matrix<float, 3, 3>  s_primal_ctx_mul_1(Matrix<float, 3, 3>  _S109, Matrix<float, 3, 3>  _S110)
{
    return mul_3(_S109, _S110);
}

inline __device__ float3  s_primal_ctx_cross_0(float3  _S111, float3  _S112)
{
    return cross_0(_S111, _S112);
}

inline __device__ float s_primal_ctx_dot_0(float3  _S113, float3  _S114)
{
    return dot_0(_S113, _S114);
}

inline __device__ float s_primal_ctx_abs_0(float _S115)
{
    return (F32_abs((_S115)));
}

inline __device__ float3  s_primal_ctx_mul_2(Matrix<float, 3, 3>  _S116, float3  _S117)
{
    return mul_1(_S116, _S117);
}

inline __device__ float3  s_primal_ctx_clamp_1(float3  _S118, float3  _S119, float3  _S120)
{
    return clamp_1(_S118, _S119, _S120);
}

inline __device__ float s_primal_ctx_exp_0(float _S121)
{
    return (F32_exp((_S121)));
}

inline __device__ float s_primal_ctx_log_0(float _S122)
{
    return (F32_log((_S122)));
}

inline __device__ float s_primal_ctx_lerp_0(float _S123, float _S124, float _S125)
{
    return lerp_0(_S123, _S124, _S125);
}

inline __device__ float s_primal_ctx_pow_0(float _S126, float _S127)
{
    return (F32_pow((_S126), (_S127)));
}

inline __device__ void s_bwd_prop_pow_0(DiffPair_float_0 * _S128, DiffPair_float_0 * _S129, float _S130)
{
    _d_pow_0(_S128, _S129, _S130);
    return;
}

inline __device__ void s_bwd_prop_lerp_0(DiffPair_float_0 * _S131, DiffPair_float_0 * _S132, DiffPair_float_0 * _S133, float _S134)
{
    _d_lerp_0(_S131, _S132, _S133, _S134);
    return;
}

inline __device__ void s_bwd_prop_exp_0(DiffPair_float_0 * _S135, float _S136)
{
    _d_exp_0(_S135, _S136);
    return;
}

inline __device__ void s_bwd_prop_log_0(DiffPair_float_0 * _S137, float _S138)
{
    _d_log_0(_S137, _S138);
    return;
}

inline __device__ void s_bwd_prop_clamp_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S139, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S140, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S141, float3  _S142)
{
    _d_clamp_vector_0(_S139, _S140, _S141, _S142);
    return;
}

inline __device__ void s_bwd_prop_abs_0(DiffPair_float_0 * _S143, float _S144)
{
    _d_abs_0(_S143, _S144);
    return;
}

inline __device__ void s_bwd_prop_mul_0(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S145, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S146, float3  _S147)
{
    _d_mul_1(_S145, _S146, _S147);
    return;
}

inline __device__ void s_bwd_prop_mul_1(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S148, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S149, Matrix<float, 3, 3>  _S150)
{
    mul_2(_S148, _S149, _S150);
    return;
}

inline __device__ void s_bwd_prop_cross_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S151, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S152, float3  _S153)
{
    _d_cross_0(_S151, _S152, _S153);
    return;
}

inline __device__ void s_bwd_prop_dot_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S154, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S155, float _S156)
{
    _d_dot_0(_S154, _S155, _S156);
    return;
}

inline __device__ void s_bwd_prop_mul_2(DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 * _S157, DiffPair_vectorx3Cfloatx2C2x3E_0 * _S158, float2  _S159)
{
    _d_mul_0(_S157, _S158, _S159);
    return;
}

inline __device__ void s_bwd_prop_clamp_1(DiffPair_float_0 * _S160, DiffPair_float_0 * _S161, DiffPair_float_0 * _S162, float _S163)
{
    _d_clamp_0(_S160, _S161, _S162, _S163);
    return;
}

inline __device__ void s_bwd_prop_exp2_0(DiffPair_float_0 * _S164, float _S165)
{
    _d_exp2_0(_S164, _S165);
    return;
}

inline __device__ void s_bwd_prop_apply_ppisp_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dprgb_in_0, float2  pix_coord_3, float2  image_center_3, float2  img_size_3, DiffPair_arrayx3Cfloatx2C36x3E_0 * dpparams_0, float3  _s_dOut_0)
{
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S166 = *dprgb_in_0;
    float3  _S167 = make_float3 (0.0f);
    Matrix<float, 3, 3>  _S168 = makeMatrix<float, 3, 3> (0.0f);
    VignettingChannelParams_0 _S169 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    FixedArray<VignettingChannelParams_0, 3>  _S170 = {
        _S169, _S169, _S169
    };
    float2  _S171 = make_float2 (0.0f);
    ColorPPISPParams_0 _S172 = { _S171, _S171, _S171, _S171 };
    CRFPPISPChannelParams_0 _S173 = { 0.0f, 0.0f, 0.0f, 0.0f };
    FixedArray<CRFPPISPChannelParams_0, 3>  _S174 = {
        _S173, _S173, _S173
    };
    PPISPParams_0 _S175;
    (&_S175)->exposure_2 = dpparams_0->primal_0[int(0)];
    (&_S175)->vignette_params_2 = _S170;
    (&_S175)->color_params_2 = _S172;
    (&_S175)->crf_params_1 = _S174;
    (&(&_S175)->vignette_params_2[int(0)])->cx_0 = dpparams_0->primal_0[int(1)];
    (&(&_S175)->vignette_params_2[int(0)])->cy_0 = dpparams_0->primal_0[int(2)];
    float _S176 = dpparams_0->primal_0[int(3)];
    (&(&_S175)->vignette_params_2[int(0)])->alpha0_0 = dpparams_0->primal_0[int(3)];
    float _S177 = dpparams_0->primal_0[int(4)];
    (&(&_S175)->vignette_params_2[int(0)])->alpha1_0 = dpparams_0->primal_0[int(4)];
    float _S178 = dpparams_0->primal_0[int(5)];
    (&(&_S175)->vignette_params_2[int(0)])->alpha2_0 = dpparams_0->primal_0[int(5)];
    (&(&_S175)->vignette_params_2[int(1)])->cx_0 = dpparams_0->primal_0[int(6)];
    (&(&_S175)->vignette_params_2[int(1)])->cy_0 = dpparams_0->primal_0[int(7)];
    float _S179 = dpparams_0->primal_0[int(8)];
    (&(&_S175)->vignette_params_2[int(1)])->alpha0_0 = dpparams_0->primal_0[int(8)];
    float _S180 = dpparams_0->primal_0[int(9)];
    (&(&_S175)->vignette_params_2[int(1)])->alpha1_0 = dpparams_0->primal_0[int(9)];
    float _S181 = dpparams_0->primal_0[int(10)];
    (&(&_S175)->vignette_params_2[int(1)])->alpha2_0 = dpparams_0->primal_0[int(10)];
    (&(&_S175)->vignette_params_2[int(2)])->cx_0 = dpparams_0->primal_0[int(11)];
    (&(&_S175)->vignette_params_2[int(2)])->cy_0 = dpparams_0->primal_0[int(12)];
    float _S182 = dpparams_0->primal_0[int(13)];
    (&(&_S175)->vignette_params_2[int(2)])->alpha0_0 = dpparams_0->primal_0[int(13)];
    float _S183 = dpparams_0->primal_0[int(14)];
    (&(&_S175)->vignette_params_2[int(2)])->alpha1_0 = dpparams_0->primal_0[int(14)];
    float _S184 = dpparams_0->primal_0[int(15)];
    (&(&_S175)->vignette_params_2[int(2)])->alpha2_0 = dpparams_0->primal_0[int(15)];
    *&((&(&(&_S175)->color_params_2)->b_0)->x) = dpparams_0->primal_0[int(16)];
    *&((&(&(&_S175)->color_params_2)->b_0)->y) = dpparams_0->primal_0[int(17)];
    *&((&(&(&_S175)->color_params_2)->r_0)->x) = dpparams_0->primal_0[int(18)];
    *&((&(&(&_S175)->color_params_2)->r_0)->y) = dpparams_0->primal_0[int(19)];
    *&((&(&(&_S175)->color_params_2)->g_0)->x) = dpparams_0->primal_0[int(20)];
    *&((&(&(&_S175)->color_params_2)->g_0)->y) = dpparams_0->primal_0[int(21)];
    *&((&(&(&_S175)->color_params_2)->n_0)->x) = dpparams_0->primal_0[int(22)];
    *&((&(&(&_S175)->color_params_2)->n_0)->y) = dpparams_0->primal_0[int(23)];
    float _S185 = dpparams_0->primal_0[int(24)];
    (&(&_S175)->crf_params_1[int(0)])->toe_0 = dpparams_0->primal_0[int(24)];
    float _S186 = dpparams_0->primal_0[int(25)];
    (&(&_S175)->crf_params_1[int(0)])->shoulder_0 = dpparams_0->primal_0[int(25)];
    float _S187 = dpparams_0->primal_0[int(26)];
    (&(&_S175)->crf_params_1[int(0)])->gamma_0 = dpparams_0->primal_0[int(26)];
    float _S188 = dpparams_0->primal_0[int(27)];
    (&(&_S175)->crf_params_1[int(0)])->center_0 = dpparams_0->primal_0[int(27)];
    float _S189 = dpparams_0->primal_0[int(28)];
    (&(&_S175)->crf_params_1[int(1)])->toe_0 = dpparams_0->primal_0[int(28)];
    float _S190 = dpparams_0->primal_0[int(29)];
    (&(&_S175)->crf_params_1[int(1)])->shoulder_0 = dpparams_0->primal_0[int(29)];
    float _S191 = dpparams_0->primal_0[int(30)];
    (&(&_S175)->crf_params_1[int(1)])->gamma_0 = dpparams_0->primal_0[int(30)];
    float _S192 = dpparams_0->primal_0[int(31)];
    (&(&_S175)->crf_params_1[int(1)])->center_0 = dpparams_0->primal_0[int(31)];
    float _S193 = dpparams_0->primal_0[int(32)];
    (&(&_S175)->crf_params_1[int(2)])->toe_0 = dpparams_0->primal_0[int(32)];
    float _S194 = dpparams_0->primal_0[int(33)];
    (&(&_S175)->crf_params_1[int(2)])->shoulder_0 = dpparams_0->primal_0[int(33)];
    float _S195 = dpparams_0->primal_0[int(34)];
    (&(&_S175)->crf_params_1[int(2)])->gamma_0 = dpparams_0->primal_0[int(34)];
    float _S196 = dpparams_0->primal_0[int(35)];
    (&(&_S175)->crf_params_1[int(2)])->center_0 = dpparams_0->primal_0[int(35)];
    PPISPParams_0 _S197 = _S175;
    float _S198 = s_primal_ctx_exp2_0(_S175.exposure_2);
    float3  _S199 = make_float3 (_S198);
    float3  rgb_out_5 = (*dprgb_in_0).primal_0 * make_float3 (_S198);
    float _S200 = (F32_max((img_size_3.x), (img_size_3.y)));
    float _S201 = (pix_coord_3.x - image_center_3.x) / _S200;
    float _S202 = (pix_coord_3.y - image_center_3.y) / _S200;
    float dx_9 = _S201 - dpparams_0->primal_0[int(1)];
    float dy_9 = _S202 - dpparams_0->primal_0[int(2)];
    float r2_12 = dx_9 * dx_9 + dy_9 * dy_9;
    float r4_9 = r2_12 * r2_12;
    float r6_0 = r4_9 * r2_12;
    float falloff_0 = dpparams_0->primal_0[int(5)] * r6_0 + dpparams_0->primal_0[int(4)] * r4_9 + dpparams_0->primal_0[int(3)] * r2_12 + 1.0f;
    float _S203 = s_primal_ctx_clamp_0(falloff_0, 0.0f, 1.0f);
    float _S204 = rgb_out_5.x * _S203;
    float3  _S205 = rgb_out_5;
    *&((&_S205)->x) = _S204;
    float dx_10 = _S201 - dpparams_0->primal_0[int(6)];
    float dy_10 = _S202 - dpparams_0->primal_0[int(7)];
    float r2_13 = dx_10 * dx_10 + dy_10 * dy_10;
    float r4_10 = r2_13 * r2_13;
    float r6_1 = r4_10 * r2_13;
    float falloff_1 = dpparams_0->primal_0[int(10)] * r6_1 + dpparams_0->primal_0[int(9)] * r4_10 + dpparams_0->primal_0[int(8)] * r2_13 + 1.0f;
    float _S206 = s_primal_ctx_clamp_0(falloff_1, 0.0f, 1.0f);
    *&((&_S205)->y) = rgb_out_5.y * _S206;
    float dx_11 = _S201 - dpparams_0->primal_0[int(11)];
    float dy_11 = _S202 - dpparams_0->primal_0[int(12)];
    float r2_14 = dx_11 * dx_11 + dy_11 * dy_11;
    float r4_11 = r2_14 * r2_14;
    float r6_2 = r4_11 * r2_14;
    float falloff_2 = dpparams_0->primal_0[int(15)] * r6_2 + dpparams_0->primal_0[int(14)] * r4_11 + dpparams_0->primal_0[int(13)] * r2_14 + 1.0f;
    float _S207 = s_primal_ctx_clamp_0(falloff_2, 0.0f, 1.0f);
    *&((&_S205)->z) = rgb_out_5.z * _S207;
    PPISPParams_0 _S208 = _S175;
    float2  _S209 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f), _S175.color_params_2.b_0);
    float2  _S210 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f), _S175.color_params_2.r_0);
    float2  _S211 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f), _S175.color_params_2.g_0);
    float2  _S212 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f), _S175.color_params_2.n_0);
    float _S213 = 0.3333333432674408f + _S212.x;
    float _S214 = 0.3333333432674408f + _S212.y;
    Matrix<float, 3, 3>  T_3 = makeMatrix<float, 3, 3> (_S209.x, 1.0f + _S210.x, _S211.x, _S209.y, _S210.y, 1.0f + _S211.y, 1.0f, 1.0f, 1.0f);
    Matrix<float, 3, 3>  skew_0 = makeMatrix<float, 3, 3> (0.0f, -1.0f, _S214, 1.0f, 0.0f, - _S213, - _S214, _S213, 0.0f);
    Matrix<float, 3, 3>  _S215 = s_primal_ctx_mul_1(skew_0, T_3);
    float3  r0_3 = make_float3 (_S215.rows[int(0)].x, _S215.rows[int(0)].y, _S215.rows[int(0)].z);
    float3  r1_3 = make_float3 (_S215.rows[int(1)].x, _S215.rows[int(1)].y, _S215.rows[int(1)].z);
    float3  r2_15 = make_float3 (_S215.rows[int(2)].x, _S215.rows[int(2)].y, _S215.rows[int(2)].z);
    float3  _S216 = s_primal_ctx_cross_0(r0_3, r1_3);
    bool _S217 = (s_primal_ctx_dot_0(_S216, _S216)) < 9.99999968265522539e-21f;
    float3  lambda_v_9;
    float3  _S218;
    bool _S219;
    if(_S217)
    {
        float3  _S220 = s_primal_ctx_cross_0(r0_3, r2_15);
        bool _S221 = (s_primal_ctx_dot_0(_S220, _S220)) < 9.99999968265522539e-21f;
        if(_S221)
        {
            lambda_v_9 = s_primal_ctx_cross_0(r1_3, r2_15);
        }
        else
        {
            lambda_v_9 = _S220;
        }
        _S219 = _S221;
        _S218 = _S220;
    }
    else
    {
        lambda_v_9 = _S216;
        _S219 = false;
        _S218 = _S167;
    }
    Matrix<float, 3, 3>  S_inv_0 = makeMatrix<float, 3, 3> (-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    Matrix<float, 3, 3>  D_0 = makeMatrix<float, 3, 3> (lambda_v_9.x, 0.0f, 0.0f, 0.0f, lambda_v_9.y, 0.0f, 0.0f, 0.0f, lambda_v_9.z);
    Matrix<float, 3, 3>  _S222 = s_primal_ctx_mul_1(T_3, D_0);
    Matrix<float, 3, 3>  _S223 = s_primal_ctx_mul_1(_S222, S_inv_0);
    bool _S224 = (s_primal_ctx_abs_0(_S223.rows[int(2)].z)) > 9.99999968265522539e-21f;
    Matrix<float, 3, 3>  H_6;
    Matrix<float, 3, 3>  _S225;
    float _S226;
    if(_S224)
    {
        float inv_s_0 = 1.0f / _S223.rows[int(2)].z;
        Matrix<float, 3, 3>  _S227 = makeMatrix<float, 3, 3> (inv_s_0);
        float _S228 = _S223.rows[int(2)].z * _S223.rows[int(2)].z;
        H_6 = _S223 * makeMatrix<float, 3, 3> (inv_s_0);
        _S225 = _S227;
        _S226 = _S228;
    }
    else
    {
        H_6 = _S223;
        _S225 = _S168;
        _S226 = 0.0f;
    }
    float _S229 = _S205.x;
    float _S230 = _S205.y;
    float intensity_3 = _S229 + _S230 + _S205.z;
    float3  rgi_in_0 = make_float3 (_S229, _S230, intensity_3);
    float3  _S231 = s_primal_ctx_mul_2(H_6, rgi_in_0);
    float _S232 = _S231.z;
    float _S233 = 0.05000000074505806f * s_primal_ctx_abs_0(intensity_3) + 9.99999993922529029e-09f;
    float _S234 = (F32_max((_S232), (_S233)));
    float norm_factor_3 = intensity_3 / _S234;
    float _S235 = _S234 * _S234;
    float _S236 = _S231.x;
    float out_r_3 = _S236 * norm_factor_3;
    float _S237 = _S231.y;
    float out_g_3 = _S237 * norm_factor_3;
    float3  _S238 = make_float3 (out_r_3, out_g_3, intensity_3 - out_r_3 - out_g_3);
    float3  _S239 = make_float3 (0.0f);
    float3  _S240 = make_float3 (1.0f);
    float3  _S241 = s_primal_ctx_clamp_1(_S238, _S239, _S240);
    float _S242 = _S241.x;
    float _S243 = 1.0f + s_primal_ctx_exp_0(_S185);
    float _S244 = 0.30000001192092896f + s_primal_ctx_log_0(_S243);
    float _S245 = 1.0f + s_primal_ctx_exp_0(_S186);
    float _S246 = 0.30000001192092896f + s_primal_ctx_log_0(_S245);
    float _S247 = 1.0f + s_primal_ctx_exp_0(_S187);
    float _S248 = 0.10000000149011612f + s_primal_ctx_log_0(_S247);
    float _S249 = - _S188;
    float _S250 = 1.0f + s_primal_ctx_exp_0(_S249);
    float _S251 = 1.0f / _S250;
    float _S252 = _S250 * _S250;
    float _S253 = s_primal_ctx_lerp_0(_S244, _S246, _S251);
    float _S254 = _S246 * _S251;
    float a_4 = _S254 / _S253;
    float _S255 = _S253 * _S253;
    float b_5 = 1.0f - a_4;
    bool _S256 = _S242 <= _S251;
    float y_6;
    float _S257;
    float _S258;
    float _S259;
    float _S260;
    float _S261;
    float _S262;
    float _S263;
    float _S264;
    if(_S256)
    {
        float _S265 = _S242 / _S251;
        float _S266 = _S251 * _S251;
        float _S267 = s_primal_ctx_pow_0(_S265, _S244);
        y_6 = a_4 * _S267;
        _S257 = _S267;
        _S258 = _S265;
        _S259 = _S266;
        _S260 = 0.0f;
        _S261 = 0.0f;
        _S262 = 0.0f;
        _S263 = 0.0f;
        _S264 = 0.0f;
    }
    else
    {
        float _S268 = 1.0f - _S242;
        float _S269 = 1.0f - _S251;
        float _S270 = _S268 / _S269;
        float _S271 = _S269 * _S269;
        float _S272 = s_primal_ctx_pow_0(_S270, _S246);
        y_6 = 1.0f - b_5 * _S272;
        _S257 = 0.0f;
        _S258 = 0.0f;
        _S259 = 0.0f;
        _S260 = _S272;
        _S261 = _S270;
        _S262 = _S271;
        _S263 = _S268;
        _S264 = _S269;
    }
    float _S273 = (F32_max((0.0f), (y_6)));
    float _S274 = _S241.y;
    float _S275 = 1.0f + s_primal_ctx_exp_0(_S189);
    float _S276 = 0.30000001192092896f + s_primal_ctx_log_0(_S275);
    float _S277 = 1.0f + s_primal_ctx_exp_0(_S190);
    float _S278 = 0.30000001192092896f + s_primal_ctx_log_0(_S277);
    float _S279 = 1.0f + s_primal_ctx_exp_0(_S191);
    float _S280 = 0.10000000149011612f + s_primal_ctx_log_0(_S279);
    float _S281 = - _S192;
    float _S282 = 1.0f + s_primal_ctx_exp_0(_S281);
    float _S283 = 1.0f / _S282;
    float _S284 = _S282 * _S282;
    float _S285 = s_primal_ctx_lerp_0(_S276, _S278, _S283);
    float _S286 = _S278 * _S283;
    float a_5 = _S286 / _S285;
    float _S287 = _S285 * _S285;
    float b_6 = 1.0f - a_5;
    bool _S288 = _S274 <= _S283;
    float y_7;
    float _S289;
    float _S290;
    float _S291;
    float _S292;
    float _S293;
    float _S294;
    float _S295;
    float _S296;
    if(_S288)
    {
        float _S297 = _S274 / _S283;
        float _S298 = _S283 * _S283;
        float _S299 = s_primal_ctx_pow_0(_S297, _S276);
        y_7 = a_5 * _S299;
        _S289 = _S299;
        _S290 = _S297;
        _S291 = _S298;
        _S292 = 0.0f;
        _S293 = 0.0f;
        _S294 = 0.0f;
        _S295 = 0.0f;
        _S296 = 0.0f;
    }
    else
    {
        float _S300 = 1.0f - _S274;
        float _S301 = 1.0f - _S283;
        float _S302 = _S300 / _S301;
        float _S303 = _S301 * _S301;
        float _S304 = s_primal_ctx_pow_0(_S302, _S278);
        y_7 = 1.0f - b_6 * _S304;
        _S289 = 0.0f;
        _S290 = 0.0f;
        _S291 = 0.0f;
        _S292 = _S304;
        _S293 = _S302;
        _S294 = _S303;
        _S295 = _S300;
        _S296 = _S301;
    }
    float _S305 = (F32_max((0.0f), (y_7)));
    float _S306 = _S241.z;
    float _S307 = 1.0f + s_primal_ctx_exp_0(_S193);
    float _S308 = 0.30000001192092896f + s_primal_ctx_log_0(_S307);
    float _S309 = 1.0f + s_primal_ctx_exp_0(_S194);
    float _S310 = 0.30000001192092896f + s_primal_ctx_log_0(_S309);
    float _S311 = 1.0f + s_primal_ctx_exp_0(_S195);
    float _S312 = 0.10000000149011612f + s_primal_ctx_log_0(_S311);
    float _S313 = - _S196;
    float _S314 = 1.0f + s_primal_ctx_exp_0(_S313);
    float _S315 = 1.0f / _S314;
    float _S316 = _S314 * _S314;
    float _S317 = s_primal_ctx_lerp_0(_S308, _S310, _S315);
    float _S318 = _S310 * _S315;
    float a_6 = _S318 / _S317;
    float _S319 = _S317 * _S317;
    float b_7 = 1.0f - a_6;
    bool _S320 = _S306 <= _S315;
    float y_8;
    float _S321;
    float _S322;
    float _S323;
    float _S324;
    float _S325;
    float _S326;
    float _S327;
    float _S328;
    if(_S320)
    {
        float _S329 = _S306 / _S315;
        float _S330 = _S315 * _S315;
        float _S331 = s_primal_ctx_pow_0(_S329, _S308);
        y_8 = a_6 * _S331;
        _S321 = _S331;
        _S322 = _S329;
        _S323 = _S330;
        _S324 = 0.0f;
        _S325 = 0.0f;
        _S326 = 0.0f;
        _S327 = 0.0f;
        _S328 = 0.0f;
    }
    else
    {
        float _S332 = 1.0f - _S306;
        float _S333 = 1.0f - _S315;
        float _S334 = _S332 / _S333;
        float _S335 = _S333 * _S333;
        float _S336 = s_primal_ctx_pow_0(_S334, _S310);
        y_8 = 1.0f - b_7 * _S336;
        _S321 = 0.0f;
        _S322 = 0.0f;
        _S323 = 0.0f;
        _S324 = _S336;
        _S325 = _S334;
        _S326 = _S335;
        _S327 = _S332;
        _S328 = _S333;
    }
    float _S337 = (F32_max((0.0f), (y_8)));
    DiffPair_float_0 _S338;
    (&_S338)->primal_0 = _S337;
    (&_S338)->differential_0 = 0.0f;
    DiffPair_float_0 _S339;
    (&_S339)->primal_0 = _S312;
    (&_S339)->differential_0 = 0.0f;
    s_bwd_prop_pow_0(&_S338, &_S339, _s_dOut_0.z);
    DiffPair_float_0 _S340 = _S339;
    DiffPair_float_0 _S341;
    (&_S341)->primal_0 = 0.0f;
    (&_S341)->differential_0 = 0.0f;
    DiffPair_float_0 _S342;
    (&_S342)->primal_0 = y_8;
    (&_S342)->differential_0 = 0.0f;
    _d_max_0(&_S341, &_S342, _S338.differential_0);
    DiffPair_float_0 _S343 = _S342;
    if(_S320)
    {
        float _S344 = a_6 * _S343.differential_0;
        float _S345 = _S321 * _S343.differential_0;
        DiffPair_float_0 _S346;
        (&_S346)->primal_0 = _S322;
        (&_S346)->differential_0 = 0.0f;
        DiffPair_float_0 _S347;
        (&_S347)->primal_0 = _S308;
        (&_S347)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S346, &_S347, _S344);
        float _S348 = _S346.differential_0 / _S323;
        float _S349 = _S306 * - _S348;
        float _S350 = _S315 * _S348;
        y_8 = 0.0f;
        _S321 = _S345;
        _S322 = _S349;
        _S323 = 0.0f;
        _S324 = _S347.differential_0;
        _S325 = _S350;
    }
    else
    {
        float _S351 = - _S343.differential_0;
        float _S352 = b_7 * _S351;
        float _S353 = _S324 * _S351;
        DiffPair_float_0 _S354;
        (&_S354)->primal_0 = _S325;
        (&_S354)->differential_0 = 0.0f;
        DiffPair_float_0 _S355;
        (&_S355)->primal_0 = _S310;
        (&_S355)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S354, &_S355, _S352);
        float _S356 = _S354.differential_0 / _S326;
        float _S357 = - (_S327 * - _S356);
        float _S358 = - (_S328 * _S356);
        y_8 = _S353;
        _S321 = 0.0f;
        _S322 = _S357;
        _S323 = _S355.differential_0;
        _S324 = 0.0f;
        _S325 = _S358;
    }
    float _S359 = (- y_8 + _S321) / _S319;
    float _S360 = _S318 * - _S359;
    float _S361 = _S317 * _S359;
    float _S362 = _S310 * _S361;
    float _S363 = _S315 * _S361;
    DiffPair_float_0 _S364;
    (&_S364)->primal_0 = _S308;
    (&_S364)->differential_0 = 0.0f;
    DiffPair_float_0 _S365;
    (&_S365)->primal_0 = _S310;
    (&_S365)->differential_0 = 0.0f;
    DiffPair_float_0 _S366;
    (&_S366)->primal_0 = _S315;
    (&_S366)->differential_0 = 0.0f;
    s_bwd_prop_lerp_0(&_S364, &_S365, &_S366, _S360);
    float _S367 = - ((_S362 + _S366.differential_0 + _S322) / _S316);
    DiffPair_float_0 _S368;
    (&_S368)->primal_0 = _S313;
    (&_S368)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S368, _S367);
    float _S369 = - _S368.differential_0;
    DiffPair_float_0 _S370;
    (&_S370)->primal_0 = _S311;
    (&_S370)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S370, _S340.differential_0);
    DiffPair_float_0 _S371;
    (&_S371)->primal_0 = _S195;
    (&_S371)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S371, _S370.differential_0);
    DiffPair_float_0 _S372 = _S371;
    float _S373 = _S363 + _S365.differential_0 + _S323;
    DiffPair_float_0 _S374;
    (&_S374)->primal_0 = _S309;
    (&_S374)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S374, _S373);
    DiffPair_float_0 _S375;
    (&_S375)->primal_0 = _S194;
    (&_S375)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S375, _S374.differential_0);
    DiffPair_float_0 _S376 = _S375;
    float _S377 = _S364.differential_0 + _S324;
    DiffPair_float_0 _S378;
    (&_S378)->primal_0 = _S307;
    (&_S378)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S378, _S377);
    DiffPair_float_0 _S379;
    (&_S379)->primal_0 = _S193;
    (&_S379)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S379, _S378.differential_0);
    DiffPair_float_0 _S380 = _S379;
    float3  _S381 = make_float3 (0.0f, 0.0f, _S325);
    DiffPair_float_0 _S382;
    (&_S382)->primal_0 = _S305;
    (&_S382)->differential_0 = 0.0f;
    DiffPair_float_0 _S383;
    (&_S383)->primal_0 = _S280;
    (&_S383)->differential_0 = 0.0f;
    s_bwd_prop_pow_0(&_S382, &_S383, _s_dOut_0.y);
    DiffPair_float_0 _S384 = _S383;
    DiffPair_float_0 _S385;
    (&_S385)->primal_0 = 0.0f;
    (&_S385)->differential_0 = 0.0f;
    DiffPair_float_0 _S386;
    (&_S386)->primal_0 = y_7;
    (&_S386)->differential_0 = 0.0f;
    _d_max_0(&_S385, &_S386, _S382.differential_0);
    DiffPair_float_0 _S387 = _S386;
    if(_S288)
    {
        float _S388 = a_5 * _S387.differential_0;
        float _S389 = _S289 * _S387.differential_0;
        DiffPair_float_0 _S390;
        (&_S390)->primal_0 = _S290;
        (&_S390)->differential_0 = 0.0f;
        DiffPair_float_0 _S391;
        (&_S391)->primal_0 = _S276;
        (&_S391)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S390, &_S391, _S388);
        float _S392 = _S390.differential_0 / _S291;
        float _S393 = _S274 * - _S392;
        float _S394 = _S283 * _S392;
        y_7 = 0.0f;
        _S289 = _S389;
        _S290 = _S393;
        _S291 = 0.0f;
        _S292 = _S391.differential_0;
        _S293 = _S394;
    }
    else
    {
        float _S395 = - _S387.differential_0;
        float _S396 = b_6 * _S395;
        float _S397 = _S292 * _S395;
        DiffPair_float_0 _S398;
        (&_S398)->primal_0 = _S293;
        (&_S398)->differential_0 = 0.0f;
        DiffPair_float_0 _S399;
        (&_S399)->primal_0 = _S278;
        (&_S399)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S398, &_S399, _S396);
        float _S400 = _S398.differential_0 / _S294;
        float _S401 = - (_S295 * - _S400);
        float _S402 = - (_S296 * _S400);
        y_7 = _S397;
        _S289 = 0.0f;
        _S290 = _S401;
        _S291 = _S399.differential_0;
        _S292 = 0.0f;
        _S293 = _S402;
    }
    float _S403 = (- y_7 + _S289) / _S287;
    float _S404 = _S286 * - _S403;
    float _S405 = _S285 * _S403;
    float _S406 = _S278 * _S405;
    float _S407 = _S283 * _S405;
    DiffPair_float_0 _S408;
    (&_S408)->primal_0 = _S276;
    (&_S408)->differential_0 = 0.0f;
    DiffPair_float_0 _S409;
    (&_S409)->primal_0 = _S278;
    (&_S409)->differential_0 = 0.0f;
    DiffPair_float_0 _S410;
    (&_S410)->primal_0 = _S283;
    (&_S410)->differential_0 = 0.0f;
    s_bwd_prop_lerp_0(&_S408, &_S409, &_S410, _S404);
    float _S411 = - ((_S406 + _S410.differential_0 + _S290) / _S284);
    DiffPair_float_0 _S412;
    (&_S412)->primal_0 = _S281;
    (&_S412)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S412, _S411);
    float _S413 = - _S412.differential_0;
    DiffPair_float_0 _S414;
    (&_S414)->primal_0 = _S279;
    (&_S414)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S414, _S384.differential_0);
    DiffPair_float_0 _S415;
    (&_S415)->primal_0 = _S191;
    (&_S415)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S415, _S414.differential_0);
    DiffPair_float_0 _S416 = _S415;
    float _S417 = _S407 + _S409.differential_0 + _S291;
    DiffPair_float_0 _S418;
    (&_S418)->primal_0 = _S277;
    (&_S418)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S418, _S417);
    DiffPair_float_0 _S419;
    (&_S419)->primal_0 = _S190;
    (&_S419)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S419, _S418.differential_0);
    DiffPair_float_0 _S420 = _S419;
    float _S421 = _S408.differential_0 + _S292;
    DiffPair_float_0 _S422;
    (&_S422)->primal_0 = _S275;
    (&_S422)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S422, _S421);
    DiffPair_float_0 _S423;
    (&_S423)->primal_0 = _S189;
    (&_S423)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S423, _S422.differential_0);
    DiffPair_float_0 _S424 = _S423;
    float3  _S425 = _S381 + make_float3 (0.0f, _S293, 0.0f);
    DiffPair_float_0 _S426;
    (&_S426)->primal_0 = _S273;
    (&_S426)->differential_0 = 0.0f;
    DiffPair_float_0 _S427;
    (&_S427)->primal_0 = _S248;
    (&_S427)->differential_0 = 0.0f;
    s_bwd_prop_pow_0(&_S426, &_S427, _s_dOut_0.x);
    DiffPair_float_0 _S428 = _S427;
    DiffPair_float_0 _S429;
    (&_S429)->primal_0 = 0.0f;
    (&_S429)->differential_0 = 0.0f;
    DiffPair_float_0 _S430;
    (&_S430)->primal_0 = y_6;
    (&_S430)->differential_0 = 0.0f;
    _d_max_0(&_S429, &_S430, _S426.differential_0);
    DiffPair_float_0 _S431 = _S430;
    if(_S256)
    {
        float _S432 = a_4 * _S431.differential_0;
        float _S433 = _S257 * _S431.differential_0;
        DiffPair_float_0 _S434;
        (&_S434)->primal_0 = _S258;
        (&_S434)->differential_0 = 0.0f;
        DiffPair_float_0 _S435;
        (&_S435)->primal_0 = _S244;
        (&_S435)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S434, &_S435, _S432);
        float _S436 = _S434.differential_0 / _S259;
        float _S437 = _S242 * - _S436;
        float _S438 = _S251 * _S436;
        y_6 = 0.0f;
        _S257 = _S433;
        _S258 = _S437;
        _S259 = 0.0f;
        _S260 = _S435.differential_0;
        _S261 = _S438;
    }
    else
    {
        float _S439 = - _S431.differential_0;
        float _S440 = b_5 * _S439;
        float _S441 = _S260 * _S439;
        DiffPair_float_0 _S442;
        (&_S442)->primal_0 = _S261;
        (&_S442)->differential_0 = 0.0f;
        DiffPair_float_0 _S443;
        (&_S443)->primal_0 = _S246;
        (&_S443)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S442, &_S443, _S440);
        float _S444 = _S442.differential_0 / _S262;
        float _S445 = - (_S263 * - _S444);
        float _S446 = - (_S264 * _S444);
        y_6 = _S441;
        _S257 = 0.0f;
        _S258 = _S445;
        _S259 = _S443.differential_0;
        _S260 = 0.0f;
        _S261 = _S446;
    }
    float _S447 = (- y_6 + _S257) / _S255;
    float _S448 = _S254 * - _S447;
    float _S449 = _S253 * _S447;
    float _S450 = _S246 * _S449;
    float _S451 = _S251 * _S449;
    DiffPair_float_0 _S452;
    (&_S452)->primal_0 = _S244;
    (&_S452)->differential_0 = 0.0f;
    DiffPair_float_0 _S453;
    (&_S453)->primal_0 = _S246;
    (&_S453)->differential_0 = 0.0f;
    DiffPair_float_0 _S454;
    (&_S454)->primal_0 = _S251;
    (&_S454)->differential_0 = 0.0f;
    s_bwd_prop_lerp_0(&_S452, &_S453, &_S454, _S448);
    float _S455 = - ((_S450 + _S454.differential_0 + _S258) / _S252);
    DiffPair_float_0 _S456;
    (&_S456)->primal_0 = _S249;
    (&_S456)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S456, _S455);
    float _S457 = - _S456.differential_0;
    DiffPair_float_0 _S458;
    (&_S458)->primal_0 = _S247;
    (&_S458)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S458, _S428.differential_0);
    DiffPair_float_0 _S459;
    (&_S459)->primal_0 = _S187;
    (&_S459)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S459, _S458.differential_0);
    DiffPair_float_0 _S460 = _S459;
    float _S461 = _S451 + _S453.differential_0 + _S259;
    DiffPair_float_0 _S462;
    (&_S462)->primal_0 = _S245;
    (&_S462)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S462, _S461);
    DiffPair_float_0 _S463;
    (&_S463)->primal_0 = _S186;
    (&_S463)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S463, _S462.differential_0);
    DiffPair_float_0 _S464 = _S463;
    float _S465 = _S452.differential_0 + _S260;
    DiffPair_float_0 _S466;
    (&_S466)->primal_0 = _S243;
    (&_S466)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S466, _S465);
    DiffPair_float_0 _S467;
    (&_S467)->primal_0 = _S185;
    (&_S467)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S467, _S466.differential_0);
    DiffPair_float_0 _S468 = _S467;
    float3  _S469 = _S425 + make_float3 (_S261, 0.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S470;
    (&_S470)->primal_0 = _S238;
    (&_S470)->differential_0 = _S167;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S471;
    (&_S471)->primal_0 = _S239;
    (&_S471)->differential_0 = _S167;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S472;
    (&_S472)->primal_0 = _S240;
    (&_S472)->differential_0 = _S167;
    s_bwd_prop_clamp_0(&_S470, &_S471, &_S472, _S469);
    float _S473 = - _S470.differential_0.z;
    float _S474 = _S470.differential_0.y + _S473;
    float _S475 = norm_factor_3 * _S474;
    float _S476 = _S470.differential_0.x + _S473;
    float _S477 = norm_factor_3 * _S476;
    float _S478 = (_S237 * _S474 + _S236 * _S476) / _S235;
    float _S479 = intensity_3 * - _S478;
    float _S480 = _S234 * _S478;
    DiffPair_float_0 _S481;
    (&_S481)->primal_0 = _S232;
    (&_S481)->differential_0 = 0.0f;
    DiffPair_float_0 _S482;
    (&_S482)->primal_0 = _S233;
    (&_S482)->differential_0 = 0.0f;
    _d_max_0(&_S481, &_S482, _S479);
    float _S483 = 0.05000000074505806f * _S482.differential_0;
    DiffPair_float_0 _S484;
    (&_S484)->primal_0 = intensity_3;
    (&_S484)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S484, _S483);
    float3  _S485 = make_float3 (_S477, _S475, _S481.differential_0);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S486;
    (&_S486)->primal_0 = H_6;
    (&_S486)->differential_0 = _S168;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S487;
    (&_S487)->primal_0 = rgi_in_0;
    (&_S487)->differential_0 = _S167;
    s_bwd_prop_mul_0(&_S486, &_S487, _S485);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S488 = _S486;
    float _S489 = _S470.differential_0.z + _S480 + _S484.differential_0 + _S487.differential_0.z;
    float _S490 = _S487.differential_0.y + _S489;
    float _S491 = _S487.differential_0.x + _S489;
    float3  _S492 = make_float3 (_S491, _S490, _S489);
    if(_S224)
    {
        Matrix<float, 3, 3>  _S493 = _S223 * _S488.differential_0;
        Matrix<float, 3, 3>  _S494 = _S225 * _S488.differential_0;
        _S226 = - ((_S493.rows[int(0)].x + _S493.rows[int(0)].y + _S493.rows[int(0)].z + _S493.rows[int(1)].x + _S493.rows[int(1)].y + _S493.rows[int(1)].z + _S493.rows[int(2)].x + _S493.rows[int(2)].y + _S493.rows[int(2)].z) / _S226);
        H_6 = _S494;
    }
    else
    {
        _S226 = 0.0f;
        H_6 = _S488.differential_0;
    }
    DiffPair_float_0 _S495;
    (&_S495)->primal_0 = _S223.rows[int(2)].z;
    (&_S495)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S495, 0.0f);
    float _S496 = _S495.differential_0 + _S226;
    float3  _S497 = _S167;
    *&((&_S497)->z) = _S496;
    Matrix<float, 3, 3>  _S498 = _S168;
    _S498[int(2)] = _S497;
    Matrix<float, 3, 3>  _S499 = H_6 + _S498;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S500;
    (&_S500)->primal_0 = _S222;
    (&_S500)->differential_0 = _S168;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S501;
    (&_S501)->primal_0 = S_inv_0;
    (&_S501)->differential_0 = _S168;
    s_bwd_prop_mul_1(&_S500, &_S501, _S499);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S502;
    (&_S502)->primal_0 = T_3;
    (&_S502)->differential_0 = _S168;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S503;
    (&_S503)->primal_0 = D_0;
    (&_S503)->differential_0 = _S168;
    s_bwd_prop_mul_1(&_S502, &_S503, _S500.differential_0);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S504 = _S502;
    float3  _S505 = make_float3 (_S503.differential_0.rows[int(0)].x, _S503.differential_0.rows[int(1)].y, _S503.differential_0.rows[int(2)].z);
    float3  _S506;
    if(_S217)
    {
        if(_S219)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S507;
            (&_S507)->primal_0 = r1_3;
            (&_S507)->differential_0 = _S167;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S508;
            (&_S508)->primal_0 = r2_15;
            (&_S508)->differential_0 = _S167;
            s_bwd_prop_cross_0(&_S507, &_S508, _S505);
            _S205 = _S167;
            lambda_v_9 = _S508.differential_0;
            _S506 = _S507.differential_0;
        }
        else
        {
            _S205 = _S505;
            lambda_v_9 = _S167;
            _S506 = _S167;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S509;
        (&_S509)->primal_0 = _S218;
        (&_S509)->differential_0 = _S167;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S510;
        (&_S510)->primal_0 = _S218;
        (&_S510)->differential_0 = _S167;
        s_bwd_prop_dot_0(&_S509, &_S510, 0.0f);
        float3  _S511 = _S510.differential_0 + _S509.differential_0 + _S205;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S512;
        (&_S512)->primal_0 = r0_3;
        (&_S512)->differential_0 = _S167;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S513;
        (&_S513)->primal_0 = r2_15;
        (&_S513)->differential_0 = _S167;
        s_bwd_prop_cross_0(&_S512, &_S513, _S511);
        float3  _S514 = _S513.differential_0 + lambda_v_9;
        _S205 = _S167;
        lambda_v_9 = _S514;
        _S218 = _S506;
        _S506 = _S512.differential_0;
    }
    else
    {
        _S205 = _S505;
        lambda_v_9 = _S167;
        _S218 = _S167;
        _S506 = _S167;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S515;
    (&_S515)->primal_0 = _S216;
    (&_S515)->differential_0 = _S167;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S516;
    (&_S516)->primal_0 = _S216;
    (&_S516)->differential_0 = _S167;
    s_bwd_prop_dot_0(&_S515, &_S516, 0.0f);
    float3  _S517 = _S516.differential_0 + _S515.differential_0 + _S205;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S518;
    (&_S518)->primal_0 = r0_3;
    (&_S518)->differential_0 = _S167;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S519;
    (&_S519)->primal_0 = r1_3;
    (&_S519)->differential_0 = _S167;
    s_bwd_prop_cross_0(&_S518, &_S519, _S517);
    float3  _S520 = _S167;
    *&((&_S520)->z) = lambda_v_9.z;
    *&((&_S520)->y) = lambda_v_9.y;
    *&((&_S520)->x) = lambda_v_9.x;
    float3  _S521 = _S519.differential_0 + _S218;
    float3  _S522 = _S167;
    *&((&_S522)->z) = _S521.z;
    *&((&_S522)->y) = _S521.y;
    *&((&_S522)->x) = _S521.x;
    float3  _S523 = _S518.differential_0 + _S506;
    float3  _S524 = _S167;
    *&((&_S524)->z) = _S523.z;
    *&((&_S524)->y) = _S523.y;
    *&((&_S524)->x) = _S523.x;
    Matrix<float, 3, 3>  _S525 = _S168;
    _S525[int(2)] = _S520;
    _S525[int(1)] = _S522;
    _S525[int(0)] = _S524;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S526;
    (&_S526)->primal_0 = skew_0;
    (&_S526)->differential_0 = _S168;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S527;
    (&_S527)->primal_0 = T_3;
    (&_S527)->differential_0 = _S168;
    s_bwd_prop_mul_1(&_S526, &_S527, _S525);
    Matrix<float, 3, 3>  _S528 = _S527.differential_0 + _S504.differential_0;
    float2  _S529 = make_float2 (_S526.differential_0.rows[int(2)].y + - _S526.differential_0.rows[int(1)].z, _S526.differential_0.rows[int(0)].z + - _S526.differential_0.rows[int(2)].x);
    Matrix<float, 2, 2>  _S530 = makeMatrix<float, 2, 2> (0.0f);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S531;
    (&_S531)->primal_0 = makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f);
    (&_S531)->differential_0 = _S530;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S532;
    (&_S532)->primal_0 = _S208.color_params_2.n_0;
    (&_S532)->differential_0 = _S171;
    s_bwd_prop_mul_2(&_S531, &_S532, _S529);
    float2  _S533 = make_float2 (_S528.rows[int(0)].z, _S528.rows[int(1)].z);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S534;
    (&_S534)->primal_0 = makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f);
    (&_S534)->differential_0 = _S530;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S535;
    (&_S535)->primal_0 = _S208.color_params_2.g_0;
    (&_S535)->differential_0 = _S171;
    s_bwd_prop_mul_2(&_S534, &_S535, _S533);
    float2  _S536 = make_float2 (_S528.rows[int(0)].y, _S528.rows[int(1)].y);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S537;
    (&_S537)->primal_0 = makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f);
    (&_S537)->differential_0 = _S530;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S538;
    (&_S538)->primal_0 = _S208.color_params_2.r_0;
    (&_S538)->differential_0 = _S171;
    s_bwd_prop_mul_2(&_S537, &_S538, _S536);
    float2  _S539 = make_float2 (_S528.rows[int(0)].x, _S528.rows[int(1)].x);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S540;
    (&_S540)->primal_0 = makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f);
    (&_S540)->differential_0 = _S530;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S541;
    (&_S541)->primal_0 = _S208.color_params_2.b_0;
    (&_S541)->differential_0 = _S171;
    s_bwd_prop_mul_2(&_S540, &_S541, _S539);
    ColorPPISPParams_0 _S542 = ColorPPISPParams_x24_syn_dzero_0();
    (&_S542)->n_0 = _S532.differential_0;
    (&_S542)->g_0 = _S535.differential_0;
    (&_S542)->r_0 = _S538.differential_0;
    (&_S542)->b_0 = _S541.differential_0;
    _S205 = _S492;
    *&((&_S205)->z) = 0.0f;
    float _S543 = rgb_out_5.z * _S489;
    float _S544 = _S207 * _S489;
    DiffPair_float_0 _S545;
    (&_S545)->primal_0 = falloff_2;
    (&_S545)->differential_0 = 0.0f;
    DiffPair_float_0 _S546;
    (&_S546)->primal_0 = 0.0f;
    (&_S546)->differential_0 = 0.0f;
    DiffPair_float_0 _S547;
    (&_S547)->primal_0 = 1.0f;
    (&_S547)->differential_0 = 0.0f;
    s_bwd_prop_clamp_1(&_S545, &_S546, &_S547, _S543);
    float _S548 = r2_14 * _S545.differential_0;
    float _S549 = r4_11 * _S545.differential_0;
    float s_diff_r6_T_0 = _S184 * _S545.differential_0;
    float _S550 = r6_2 * _S545.differential_0;
    float _S551 = r2_14 * (_S183 * _S545.differential_0 + r2_14 * s_diff_r6_T_0);
    float _S552 = _S182 * _S545.differential_0 + r4_11 * s_diff_r6_T_0 + _S551 + _S551;
    float _S553 = dy_11 * _S552;
    float _S554 = dx_11 * _S552;
    float _S555 = - (_S553 + _S553);
    float _S556 = - (_S554 + _S554);
    *&((&_S205)->y) = 0.0f;
    float _S557 = rgb_out_5.y * _S490;
    float _S558 = _S206 * _S490;
    DiffPair_float_0 _S559;
    (&_S559)->primal_0 = falloff_1;
    (&_S559)->differential_0 = 0.0f;
    DiffPair_float_0 _S560;
    (&_S560)->primal_0 = 0.0f;
    (&_S560)->differential_0 = 0.0f;
    DiffPair_float_0 _S561;
    (&_S561)->primal_0 = 1.0f;
    (&_S561)->differential_0 = 0.0f;
    s_bwd_prop_clamp_1(&_S559, &_S560, &_S561, _S557);
    float _S562 = r2_13 * _S559.differential_0;
    float _S563 = r4_10 * _S559.differential_0;
    float s_diff_r6_T_1 = _S181 * _S559.differential_0;
    float _S564 = r6_1 * _S559.differential_0;
    float _S565 = r2_13 * (_S180 * _S559.differential_0 + r2_13 * s_diff_r6_T_1);
    float _S566 = _S179 * _S559.differential_0 + r4_10 * s_diff_r6_T_1 + _S565 + _S565;
    float _S567 = dy_10 * _S566;
    float _S568 = dx_10 * _S566;
    float _S569 = - (_S567 + _S567);
    float _S570 = - (_S568 + _S568);
    *&((&_S205)->x) = 0.0f;
    float _S571 = rgb_out_5.x * _S491;
    float _S572 = _S203 * _S491;
    DiffPair_float_0 _S573;
    (&_S573)->primal_0 = falloff_0;
    (&_S573)->differential_0 = 0.0f;
    DiffPair_float_0 _S574;
    (&_S574)->primal_0 = 0.0f;
    (&_S574)->differential_0 = 0.0f;
    DiffPair_float_0 _S575;
    (&_S575)->primal_0 = 1.0f;
    (&_S575)->differential_0 = 0.0f;
    s_bwd_prop_clamp_1(&_S573, &_S574, &_S575, _S571);
    float _S576 = r2_12 * _S573.differential_0;
    float _S577 = r4_9 * _S573.differential_0;
    float s_diff_r6_T_2 = _S178 * _S573.differential_0;
    float _S578 = r6_0 * _S573.differential_0;
    float _S579 = r2_12 * (_S177 * _S573.differential_0 + r2_12 * s_diff_r6_T_2);
    float _S580 = _S176 * _S573.differential_0 + r4_9 * s_diff_r6_T_2 + _S579 + _S579;
    float _S581 = dy_9 * _S580;
    float _S582 = dx_9 * _S580;
    float _S583 = - (_S581 + _S581);
    float _S584 = - (_S582 + _S582);
    float3  _S585 = _S167;
    *&((&_S585)->z) = _S544;
    *&((&_S585)->y) = _S558;
    *&((&_S585)->x) = _S572;
    float3  _S586 = _S205 + _S585;
    float3  _S587 = _S166.primal_0 * _S586;
    float3  _S588 = _S199 * _S586;
    float _S589 = _S587.x + _S587.y + _S587.z;
    DiffPair_float_0 _S590;
    (&_S590)->primal_0 = _S197.exposure_2;
    (&_S590)->differential_0 = 0.0f;
    s_bwd_prop_exp2_0(&_S590, _S589);
    PPISPParams_0 _S591 = PPISPParams_x24_syn_dzero_0();
    (&_S591)->color_params_2 = _S542;
    (&_S591)->exposure_2 = _S590.differential_0;
    _S175 = _S591;
    (&(&_S175)->crf_params_1[int(2)])->center_0 = 0.0f;
    float _S592 = _S591.crf_params_1[int(2)].center_0 + _S369;
    (&(&_S175)->crf_params_1[int(2)])->gamma_0 = 0.0f;
    float _S593 = _S591.crf_params_1[int(2)].gamma_0 + _S372.differential_0;
    (&(&_S175)->crf_params_1[int(2)])->shoulder_0 = 0.0f;
    float _S594 = _S591.crf_params_1[int(2)].shoulder_0 + _S376.differential_0;
    (&(&_S175)->crf_params_1[int(2)])->toe_0 = 0.0f;
    float _S595 = _S591.crf_params_1[int(2)].toe_0 + _S380.differential_0;
    (&(&_S175)->crf_params_1[int(1)])->center_0 = 0.0f;
    float _S596 = _S591.crf_params_1[int(1)].center_0 + _S413;
    (&(&_S175)->crf_params_1[int(1)])->gamma_0 = 0.0f;
    float _S597 = _S591.crf_params_1[int(1)].gamma_0 + _S416.differential_0;
    (&(&_S175)->crf_params_1[int(1)])->shoulder_0 = 0.0f;
    float _S598 = _S591.crf_params_1[int(1)].shoulder_0 + _S420.differential_0;
    (&(&_S175)->crf_params_1[int(1)])->toe_0 = 0.0f;
    float _S599 = _S591.crf_params_1[int(1)].toe_0 + _S424.differential_0;
    (&(&_S175)->crf_params_1[int(0)])->center_0 = 0.0f;
    float _S600 = _S591.crf_params_1[int(0)].center_0 + _S457;
    (&(&_S175)->crf_params_1[int(0)])->gamma_0 = 0.0f;
    float _S601 = _S591.crf_params_1[int(0)].gamma_0 + _S460.differential_0;
    (&(&_S175)->crf_params_1[int(0)])->shoulder_0 = 0.0f;
    float _S602 = _S591.crf_params_1[int(0)].shoulder_0 + _S464.differential_0;
    (&(&_S175)->crf_params_1[int(0)])->toe_0 = 0.0f;
    float _S603 = _S591.crf_params_1[int(0)].toe_0 + _S468.differential_0;
    *&((&(&(&_S175)->color_params_2)->n_0)->y) = 0.0f;
    *&((&(&(&_S175)->color_params_2)->n_0)->x) = 0.0f;
    *&((&(&(&_S175)->color_params_2)->g_0)->y) = 0.0f;
    *&((&(&(&_S175)->color_params_2)->g_0)->x) = 0.0f;
    *&((&(&(&_S175)->color_params_2)->r_0)->y) = 0.0f;
    *&((&(&(&_S175)->color_params_2)->r_0)->x) = 0.0f;
    *&((&(&(&_S175)->color_params_2)->b_0)->y) = 0.0f;
    *&((&(&(&_S175)->color_params_2)->b_0)->x) = 0.0f;
    (&(&_S175)->vignette_params_2[int(2)])->alpha2_0 = 0.0f;
    float _S604 = _S550 + _S591.vignette_params_2[int(2)].alpha2_0;
    (&(&_S175)->vignette_params_2[int(2)])->alpha1_0 = 0.0f;
    float _S605 = _S549 + _S591.vignette_params_2[int(2)].alpha1_0;
    (&(&_S175)->vignette_params_2[int(2)])->alpha0_0 = 0.0f;
    float _S606 = _S548 + _S591.vignette_params_2[int(2)].alpha0_0;
    (&(&_S175)->vignette_params_2[int(2)])->cy_0 = 0.0f;
    float _S607 = _S555 + _S591.vignette_params_2[int(2)].cy_0;
    (&(&_S175)->vignette_params_2[int(2)])->cx_0 = 0.0f;
    float _S608 = _S556 + _S591.vignette_params_2[int(2)].cx_0;
    (&(&_S175)->vignette_params_2[int(1)])->alpha2_0 = 0.0f;
    float _S609 = _S564 + _S591.vignette_params_2[int(1)].alpha2_0;
    (&(&_S175)->vignette_params_2[int(1)])->alpha1_0 = 0.0f;
    float _S610 = _S563 + _S591.vignette_params_2[int(1)].alpha1_0;
    (&(&_S175)->vignette_params_2[int(1)])->alpha0_0 = 0.0f;
    float _S611 = _S562 + _S591.vignette_params_2[int(1)].alpha0_0;
    (&(&_S175)->vignette_params_2[int(1)])->cy_0 = 0.0f;
    float _S612 = _S569 + _S591.vignette_params_2[int(1)].cy_0;
    (&(&_S175)->vignette_params_2[int(1)])->cx_0 = 0.0f;
    float _S613 = _S570 + _S591.vignette_params_2[int(1)].cx_0;
    (&(&_S175)->vignette_params_2[int(0)])->alpha2_0 = 0.0f;
    float _S614 = _S578 + _S591.vignette_params_2[int(0)].alpha2_0;
    (&(&_S175)->vignette_params_2[int(0)])->alpha1_0 = 0.0f;
    float _S615 = _S577 + _S591.vignette_params_2[int(0)].alpha1_0;
    (&(&_S175)->vignette_params_2[int(0)])->alpha0_0 = 0.0f;
    float _S616 = _S576 + _S591.vignette_params_2[int(0)].alpha0_0;
    (&(&_S175)->vignette_params_2[int(0)])->cy_0 = 0.0f;
    float _S617 = _S583 + _S591.vignette_params_2[int(0)].cy_0;
    (&(&_S175)->vignette_params_2[int(0)])->cx_0 = 0.0f;
    float _S618 = _S584 + _S591.vignette_params_2[int(0)].cx_0;
    FixedArray<float, 36>  _S619;
    _S619[int(0)] = 0.0f;
    _S619[int(1)] = 0.0f;
    _S619[int(2)] = 0.0f;
    _S619[int(3)] = 0.0f;
    _S619[int(4)] = 0.0f;
    _S619[int(5)] = 0.0f;
    _S619[int(6)] = 0.0f;
    _S619[int(7)] = 0.0f;
    _S619[int(8)] = 0.0f;
    _S619[int(9)] = 0.0f;
    _S619[int(10)] = 0.0f;
    _S619[int(11)] = 0.0f;
    _S619[int(12)] = 0.0f;
    _S619[int(13)] = 0.0f;
    _S619[int(14)] = 0.0f;
    _S619[int(15)] = 0.0f;
    _S619[int(16)] = 0.0f;
    _S619[int(17)] = 0.0f;
    _S619[int(18)] = 0.0f;
    _S619[int(19)] = 0.0f;
    _S619[int(20)] = 0.0f;
    _S619[int(21)] = 0.0f;
    _S619[int(22)] = 0.0f;
    _S619[int(23)] = 0.0f;
    _S619[int(24)] = 0.0f;
    _S619[int(25)] = 0.0f;
    _S619[int(26)] = 0.0f;
    _S619[int(27)] = 0.0f;
    _S619[int(28)] = 0.0f;
    _S619[int(29)] = 0.0f;
    _S619[int(30)] = 0.0f;
    _S619[int(31)] = 0.0f;
    _S619[int(32)] = 0.0f;
    _S619[int(33)] = 0.0f;
    _S619[int(34)] = 0.0f;
    _S619[int(35)] = 0.0f;
    _S619[int(8)] = _S611;
    _S619[int(16)] = _S591.color_params_2.b_0.x;
    _S619[int(15)] = _S604;
    _S619[int(14)] = _S605;
    _S619[int(13)] = _S606;
    _S619[int(12)] = _S607;
    _S619[int(11)] = _S608;
    _S619[int(10)] = _S609;
    _S619[int(9)] = _S610;
    _S619[int(17)] = _S591.color_params_2.b_0.y;
    _S619[int(7)] = _S612;
    _S619[int(6)] = _S613;
    _S619[int(5)] = _S614;
    _S619[int(4)] = _S615;
    _S619[int(3)] = _S616;
    _S619[int(2)] = _S617;
    _S619[int(1)] = _S618;
    _S619[int(0)] = _S175.exposure_2;
    _S619[int(26)] = _S601;
    _S619[int(34)] = _S593;
    _S619[int(33)] = _S594;
    _S619[int(32)] = _S595;
    _S619[int(31)] = _S596;
    _S619[int(30)] = _S597;
    _S619[int(29)] = _S598;
    _S619[int(28)] = _S599;
    _S619[int(27)] = _S600;
    _S619[int(35)] = _S592;
    _S619[int(25)] = _S602;
    _S619[int(24)] = _S603;
    _S619[int(23)] = _S591.color_params_2.n_0.y;
    _S619[int(22)] = _S591.color_params_2.n_0.x;
    _S619[int(21)] = _S591.color_params_2.g_0.y;
    _S619[int(20)] = _S591.color_params_2.g_0.x;
    _S619[int(19)] = _S591.color_params_2.r_0.y;
    _S619[int(18)] = _S591.color_params_2.r_0.x;
    dpparams_0->primal_0 = dpparams_0->primal_0;
    dpparams_0->differential_0 = _S619;
    dprgb_in_0->primal_0 = (*dprgb_in_0).primal_0;
    dprgb_in_0->differential_0 = _S588;
    return;
}

inline __device__ void s_bwd_apply_ppisp_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S620, float2  _S621, float2  _S622, float2  _S623, DiffPair_arrayx3Cfloatx2C36x3E_0 * _S624, float3  _S625)
{
    s_bwd_prop_apply_ppisp_0(_S620, _S621, _S622, _S623, _S624, _S625);
    return;
}

inline __device__ void apply_ppisp_vjp(float3  rgb_in_3, float2  pix_coord_4, float2  image_center_4, float2  img_size_4, FixedArray<float, 36>  params_3, float3  grad_out_0, float3  * grad_rgb_in_0, FixedArray<float, 36>  * grad_params_0)
{
    float3  _S626 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_rgb_in_0;
    (&dp_rgb_in_0)->primal_0 = rgb_in_3;
    (&dp_rgb_in_0)->differential_0 = _S626;
    FixedArray<float, 36>  _S627 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    DiffPair_arrayx3Cfloatx2C36x3E_0 dp_params_0;
    (&dp_params_0)->primal_0 = params_3;
    (&dp_params_0)->differential_0 = _S627;
    s_bwd_apply_ppisp_0(&dp_rgb_in_0, pix_coord_4, image_center_4, img_size_4, &dp_params_0, grad_out_0);
    *grad_rgb_in_0 = dp_rgb_in_0.differential_0;
    *grad_params_0 = (&dp_params_0)->differential_0;
    return;
}

struct DiffPair_arrayx3Cfloatx2C39x3E_0
{
    FixedArray<float, 39>  primal_0;
    FixedArray<float, 39>  differential_0;
};

inline __device__ void s_bwd_prop_apply_ppisp_rqs_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dprgb_in_1, float2  pix_coord_5, float2  image_center_5, float2  img_size_5, DiffPair_arrayx3Cfloatx2C39x3E_0 * dpparams_1, float3  _s_dOut_1)
{
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S628 = *dprgb_in_1;
    float3  _S629 = make_float3 (0.0f);
    Matrix<float, 3, 3>  _S630 = makeMatrix<float, 3, 3> (0.0f);
    VignettingChannelParams_0 _S631 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    FixedArray<VignettingChannelParams_0, 3>  _S632 = {
        _S631, _S631, _S631
    };
    float2  _S633 = make_float2 (0.0f);
    ColorPPISPParams_0 _S634 = { _S633, _S633, _S633, _S633 };
    RQSCRFPPISPChannelParams_0 _S635 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    FixedArray<RQSCRFPPISPChannelParams_0, 3>  _S636 = {
        _S635, _S635, _S635
    };
    PPISPParamsRQS_0 _S637;
    (&_S637)->exposure_1 = dpparams_1->primal_0[int(0)];
    (&_S637)->vignette_params_1 = _S632;
    (&_S637)->color_params_1 = _S634;
    (&_S637)->crf_params_0 = _S636;
    (&(&_S637)->vignette_params_1[int(0)])->cx_0 = dpparams_1->primal_0[int(1)];
    (&(&_S637)->vignette_params_1[int(0)])->cy_0 = dpparams_1->primal_0[int(2)];
    float _S638 = dpparams_1->primal_0[int(3)];
    (&(&_S637)->vignette_params_1[int(0)])->alpha0_0 = dpparams_1->primal_0[int(3)];
    float _S639 = dpparams_1->primal_0[int(4)];
    (&(&_S637)->vignette_params_1[int(0)])->alpha1_0 = dpparams_1->primal_0[int(4)];
    float _S640 = dpparams_1->primal_0[int(5)];
    (&(&_S637)->vignette_params_1[int(0)])->alpha2_0 = dpparams_1->primal_0[int(5)];
    (&(&_S637)->vignette_params_1[int(1)])->cx_0 = dpparams_1->primal_0[int(6)];
    (&(&_S637)->vignette_params_1[int(1)])->cy_0 = dpparams_1->primal_0[int(7)];
    float _S641 = dpparams_1->primal_0[int(8)];
    (&(&_S637)->vignette_params_1[int(1)])->alpha0_0 = dpparams_1->primal_0[int(8)];
    float _S642 = dpparams_1->primal_0[int(9)];
    (&(&_S637)->vignette_params_1[int(1)])->alpha1_0 = dpparams_1->primal_0[int(9)];
    float _S643 = dpparams_1->primal_0[int(10)];
    (&(&_S637)->vignette_params_1[int(1)])->alpha2_0 = dpparams_1->primal_0[int(10)];
    (&(&_S637)->vignette_params_1[int(2)])->cx_0 = dpparams_1->primal_0[int(11)];
    (&(&_S637)->vignette_params_1[int(2)])->cy_0 = dpparams_1->primal_0[int(12)];
    float _S644 = dpparams_1->primal_0[int(13)];
    (&(&_S637)->vignette_params_1[int(2)])->alpha0_0 = dpparams_1->primal_0[int(13)];
    float _S645 = dpparams_1->primal_0[int(14)];
    (&(&_S637)->vignette_params_1[int(2)])->alpha1_0 = dpparams_1->primal_0[int(14)];
    float _S646 = dpparams_1->primal_0[int(15)];
    (&(&_S637)->vignette_params_1[int(2)])->alpha2_0 = dpparams_1->primal_0[int(15)];
    *&((&(&(&_S637)->color_params_1)->b_0)->x) = dpparams_1->primal_0[int(16)];
    *&((&(&(&_S637)->color_params_1)->b_0)->y) = dpparams_1->primal_0[int(17)];
    *&((&(&(&_S637)->color_params_1)->r_0)->x) = dpparams_1->primal_0[int(18)];
    *&((&(&(&_S637)->color_params_1)->r_0)->y) = dpparams_1->primal_0[int(19)];
    *&((&(&(&_S637)->color_params_1)->g_0)->x) = dpparams_1->primal_0[int(20)];
    *&((&(&(&_S637)->color_params_1)->g_0)->y) = dpparams_1->primal_0[int(21)];
    *&((&(&(&_S637)->color_params_1)->n_0)->x) = dpparams_1->primal_0[int(22)];
    *&((&(&(&_S637)->color_params_1)->n_0)->y) = dpparams_1->primal_0[int(23)];
    float _S647 = dpparams_1->primal_0[int(24)];
    (&(&_S637)->crf_params_0[int(0)])->g0_0 = dpparams_1->primal_0[int(24)];
    float _S648 = dpparams_1->primal_0[int(25)];
    (&(&_S637)->crf_params_0[int(0)])->g1_0 = dpparams_1->primal_0[int(25)];
    float _S649 = dpparams_1->primal_0[int(26)];
    (&(&_S637)->crf_params_0[int(0)])->x0_0 = dpparams_1->primal_0[int(26)];
    float _S650 = dpparams_1->primal_0[int(27)];
    (&(&_S637)->crf_params_0[int(0)])->y0_0 = dpparams_1->primal_0[int(27)];
    float _S651 = dpparams_1->primal_0[int(28)];
    (&(&_S637)->crf_params_0[int(0)])->gc_0 = dpparams_1->primal_0[int(28)];
    float _S652 = dpparams_1->primal_0[int(29)];
    (&(&_S637)->crf_params_0[int(1)])->g0_0 = dpparams_1->primal_0[int(29)];
    float _S653 = dpparams_1->primal_0[int(30)];
    (&(&_S637)->crf_params_0[int(1)])->g1_0 = dpparams_1->primal_0[int(30)];
    float _S654 = dpparams_1->primal_0[int(31)];
    (&(&_S637)->crf_params_0[int(1)])->x0_0 = dpparams_1->primal_0[int(31)];
    float _S655 = dpparams_1->primal_0[int(32)];
    (&(&_S637)->crf_params_0[int(1)])->y0_0 = dpparams_1->primal_0[int(32)];
    float _S656 = dpparams_1->primal_0[int(33)];
    (&(&_S637)->crf_params_0[int(1)])->gc_0 = dpparams_1->primal_0[int(33)];
    float _S657 = dpparams_1->primal_0[int(34)];
    (&(&_S637)->crf_params_0[int(2)])->g0_0 = dpparams_1->primal_0[int(34)];
    float _S658 = dpparams_1->primal_0[int(35)];
    (&(&_S637)->crf_params_0[int(2)])->g1_0 = dpparams_1->primal_0[int(35)];
    float _S659 = dpparams_1->primal_0[int(36)];
    (&(&_S637)->crf_params_0[int(2)])->x0_0 = dpparams_1->primal_0[int(36)];
    float _S660 = dpparams_1->primal_0[int(37)];
    (&(&_S637)->crf_params_0[int(2)])->y0_0 = dpparams_1->primal_0[int(37)];
    float _S661 = dpparams_1->primal_0[int(38)];
    (&(&_S637)->crf_params_0[int(2)])->gc_0 = dpparams_1->primal_0[int(38)];
    PPISPParamsRQS_0 _S662 = _S637;
    float _S663 = s_primal_ctx_exp2_0(_S637.exposure_1);
    float3  _S664 = make_float3 (_S663);
    float3  rgb_out_6 = (*dprgb_in_1).primal_0 * make_float3 (_S663);
    float _S665 = (F32_max((img_size_5.x), (img_size_5.y)));
    float _S666 = (pix_coord_5.x - image_center_5.x) / _S665;
    float _S667 = (pix_coord_5.y - image_center_5.y) / _S665;
    float dx_12 = _S666 - dpparams_1->primal_0[int(1)];
    float dy_12 = _S667 - dpparams_1->primal_0[int(2)];
    float r2_16 = dx_12 * dx_12 + dy_12 * dy_12;
    float r4_12 = r2_16 * r2_16;
    float r6_3 = r4_12 * r2_16;
    float falloff_3 = dpparams_1->primal_0[int(5)] * r6_3 + dpparams_1->primal_0[int(4)] * r4_12 + dpparams_1->primal_0[int(3)] * r2_16 + 1.0f;
    float _S668 = s_primal_ctx_clamp_0(falloff_3, 0.0f, 1.0f);
    float _S669 = rgb_out_6.x * _S668;
    float3  _S670 = rgb_out_6;
    *&((&_S670)->x) = _S669;
    float dx_13 = _S666 - dpparams_1->primal_0[int(6)];
    float dy_13 = _S667 - dpparams_1->primal_0[int(7)];
    float r2_17 = dx_13 * dx_13 + dy_13 * dy_13;
    float r4_13 = r2_17 * r2_17;
    float r6_4 = r4_13 * r2_17;
    float falloff_4 = dpparams_1->primal_0[int(10)] * r6_4 + dpparams_1->primal_0[int(9)] * r4_13 + dpparams_1->primal_0[int(8)] * r2_17 + 1.0f;
    float _S671 = s_primal_ctx_clamp_0(falloff_4, 0.0f, 1.0f);
    *&((&_S670)->y) = rgb_out_6.y * _S671;
    float dx_14 = _S666 - dpparams_1->primal_0[int(11)];
    float dy_14 = _S667 - dpparams_1->primal_0[int(12)];
    float r2_18 = dx_14 * dx_14 + dy_14 * dy_14;
    float r4_14 = r2_18 * r2_18;
    float r6_5 = r4_14 * r2_18;
    float falloff_5 = dpparams_1->primal_0[int(15)] * r6_5 + dpparams_1->primal_0[int(14)] * r4_14 + dpparams_1->primal_0[int(13)] * r2_18 + 1.0f;
    float _S672 = s_primal_ctx_clamp_0(falloff_5, 0.0f, 1.0f);
    *&((&_S670)->z) = rgb_out_6.z * _S672;
    PPISPParamsRQS_0 _S673 = _S637;
    float2  _S674 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f), _S637.color_params_1.b_0);
    float2  _S675 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f), _S637.color_params_1.r_0);
    float2  _S676 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f), _S637.color_params_1.g_0);
    float2  _S677 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f), _S637.color_params_1.n_0);
    float _S678 = 0.3333333432674408f + _S677.x;
    float _S679 = 0.3333333432674408f + _S677.y;
    Matrix<float, 3, 3>  T_4 = makeMatrix<float, 3, 3> (_S674.x, 1.0f + _S675.x, _S676.x, _S674.y, _S675.y, 1.0f + _S676.y, 1.0f, 1.0f, 1.0f);
    Matrix<float, 3, 3>  skew_1 = makeMatrix<float, 3, 3> (0.0f, -1.0f, _S679, 1.0f, 0.0f, - _S678, - _S679, _S678, 0.0f);
    Matrix<float, 3, 3>  _S680 = s_primal_ctx_mul_1(skew_1, T_4);
    float3  r0_4 = make_float3 (_S680.rows[int(0)].x, _S680.rows[int(0)].y, _S680.rows[int(0)].z);
    float3  r1_4 = make_float3 (_S680.rows[int(1)].x, _S680.rows[int(1)].y, _S680.rows[int(1)].z);
    float3  r2_19 = make_float3 (_S680.rows[int(2)].x, _S680.rows[int(2)].y, _S680.rows[int(2)].z);
    float3  _S681 = s_primal_ctx_cross_0(r0_4, r1_4);
    bool _S682 = (s_primal_ctx_dot_0(_S681, _S681)) < 9.99999968265522539e-21f;
    float3  lambda_v_10;
    float3  _S683;
    bool _S684;
    if(_S682)
    {
        float3  _S685 = s_primal_ctx_cross_0(r0_4, r2_19);
        bool _S686 = (s_primal_ctx_dot_0(_S685, _S685)) < 9.99999968265522539e-21f;
        if(_S686)
        {
            lambda_v_10 = s_primal_ctx_cross_0(r1_4, r2_19);
        }
        else
        {
            lambda_v_10 = _S685;
        }
        _S684 = _S686;
        _S683 = _S685;
    }
    else
    {
        lambda_v_10 = _S681;
        _S684 = false;
        _S683 = _S629;
    }
    Matrix<float, 3, 3>  S_inv_1 = makeMatrix<float, 3, 3> (-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    Matrix<float, 3, 3>  D_1 = makeMatrix<float, 3, 3> (lambda_v_10.x, 0.0f, 0.0f, 0.0f, lambda_v_10.y, 0.0f, 0.0f, 0.0f, lambda_v_10.z);
    Matrix<float, 3, 3>  _S687 = s_primal_ctx_mul_1(T_4, D_1);
    Matrix<float, 3, 3>  _S688 = s_primal_ctx_mul_1(_S687, S_inv_1);
    bool _S689 = (s_primal_ctx_abs_0(_S688.rows[int(2)].z)) > 9.99999968265522539e-21f;
    Matrix<float, 3, 3>  H_7;
    Matrix<float, 3, 3>  _S690;
    float _S691;
    if(_S689)
    {
        float inv_s_1 = 1.0f / _S688.rows[int(2)].z;
        Matrix<float, 3, 3>  _S692 = makeMatrix<float, 3, 3> (inv_s_1);
        float _S693 = _S688.rows[int(2)].z * _S688.rows[int(2)].z;
        H_7 = _S688 * makeMatrix<float, 3, 3> (inv_s_1);
        _S690 = _S692;
        _S691 = _S693;
    }
    else
    {
        H_7 = _S688;
        _S690 = _S630;
        _S691 = 0.0f;
    }
    float _S694 = _S670.x;
    float _S695 = _S670.y;
    float intensity_4 = _S694 + _S695 + _S670.z;
    float3  rgi_in_1 = make_float3 (_S694, _S695, intensity_4);
    float3  _S696 = s_primal_ctx_mul_2(H_7, rgi_in_1);
    float _S697 = _S696.z;
    float _S698 = 0.05000000074505806f * s_primal_ctx_abs_0(intensity_4) + 9.99999993922529029e-09f;
    float _S699 = (F32_max((_S697), (_S698)));
    float norm_factor_4 = intensity_4 / _S699;
    float _S700 = _S699 * _S699;
    float _S701 = _S696.x;
    float out_r_4 = _S701 * norm_factor_4;
    float _S702 = _S696.y;
    float out_g_4 = _S702 * norm_factor_4;
    float3  _S703 = make_float3 (out_r_4, out_g_4, intensity_4 - out_r_4 - out_g_4);
    float3  _S704 = make_float3 (0.0f);
    float3  _S705 = make_float3 (1.0f);
    float3  _S706 = s_primal_ctx_clamp_1(_S703, _S704, _S705);
    float _S707 = _S706.x;
    float _S708 = s_primal_ctx_exp_0(_S647);
    float _S709 = s_primal_ctx_exp_0(_S648);
    float _S710 = - _S649;
    float _S711 = 1.0f + s_primal_ctx_exp_0(_S710);
    float x0_4 = 1.0f / _S711;
    float _S712 = _S711 * _S711;
    float _S713 = - _S650;
    float _S714 = 1.0f + s_primal_ctx_exp_0(_S713);
    float y0_4 = 1.0f / _S714;
    float _S715 = _S714 * _S714;
    float _S716 = s_primal_ctx_exp_0(_S651);
    bool _S717 = _S707 < x0_4;
    float _S718;
    float _S719;
    float _S720;
    float _S721;
    float _S722;
    float _S723;
    float _S724;
    float _S725;
    float _S726;
    float _S727;
    float _S728;
    float _S729;
    float _S730;
    float _S731;
    float _S732;
    float _S733;
    float _S734;
    float _S735;
    float _S736;
    float _S737;
    float _S738;
    float _S739;
    float _S740;
    float _S741;
    float _S742;
    float _S743;
    float _S744;
    if(_S717)
    {
        float s0_3 = y0_4 / x0_4;
        float _S745 = x0_4 * x0_4;
        float t0_3 = _S707 / x0_4;
        float _S746 = s0_3 * t0_3;
        float _S747 = _S708 * t0_3;
        float _S748 = 1.0f - t0_3;
        float _S749 = _S746 * t0_3 + _S747 * _S748;
        float _S750 = y0_4 * _S749;
        float _S751 = _S708 + _S716 - 2.0f * s0_3;
        float _S752 = _S751 * t0_3;
        float _S753 = s0_3 + _S752 * _S748;
        _S718 = _S753 * _S753;
        _S719 = _S750;
        _S720 = _S753;
        _S721 = _S752;
        _S722 = _S748;
        _S723 = _S751;
        _S724 = t0_3;
        _S725 = _S749;
        _S726 = _S747;
        _S727 = _S746;
        _S728 = s0_3;
        _S729 = _S745;
        _S730 = 0.0f;
        _S731 = 0.0f;
        _S732 = 0.0f;
        _S733 = 0.0f;
        _S734 = 0.0f;
        _S735 = 0.0f;
        _S736 = 0.0f;
        _S737 = 0.0f;
        _S738 = 0.0f;
        _S739 = 0.0f;
        _S740 = 0.0f;
        _S741 = 0.0f;
        _S742 = 0.0f;
        _S743 = 0.0f;
        _S744 = 0.0f;
    }
    else
    {
        float _S754 = 1.0f - y0_4;
        float _S755 = 1.0f - x0_4;
        float s1_3 = _S754 / _S755;
        float _S756 = _S755 * _S755;
        float _S757 = _S707 - x0_4;
        float t1_3 = _S757 / _S755;
        float _S758 = s1_3 * t1_3;
        float _S759 = _S716 * t1_3;
        float _S760 = 1.0f - t1_3;
        float _S761 = _S758 * t1_3 + _S759 * _S760;
        float _S762 = _S754 * _S761;
        float _S763 = _S716 + _S709 - 2.0f * s1_3;
        float _S764 = _S763 * t1_3;
        float _S765 = s1_3 + _S764 * _S760;
        float _S766 = _S765 * _S765;
        _S718 = 0.0f;
        _S719 = 0.0f;
        _S720 = 0.0f;
        _S721 = 0.0f;
        _S722 = 0.0f;
        _S723 = 0.0f;
        _S724 = 0.0f;
        _S725 = 0.0f;
        _S726 = 0.0f;
        _S727 = 0.0f;
        _S728 = 0.0f;
        _S729 = 0.0f;
        _S730 = _S766;
        _S731 = _S762;
        _S732 = _S765;
        _S733 = _S764;
        _S734 = _S760;
        _S735 = _S763;
        _S736 = t1_3;
        _S737 = _S754;
        _S738 = _S761;
        _S739 = _S759;
        _S740 = _S758;
        _S741 = s1_3;
        _S742 = _S756;
        _S743 = _S757;
        _S744 = _S755;
    }
    float _S767 = _S706.y;
    float _S768 = s_primal_ctx_exp_0(_S652);
    float _S769 = s_primal_ctx_exp_0(_S653);
    float _S770 = - _S654;
    float _S771 = 1.0f + s_primal_ctx_exp_0(_S770);
    float x0_5 = 1.0f / _S771;
    float _S772 = _S771 * _S771;
    float _S773 = - _S655;
    float _S774 = 1.0f + s_primal_ctx_exp_0(_S773);
    float y0_5 = 1.0f / _S774;
    float _S775 = _S774 * _S774;
    float _S776 = s_primal_ctx_exp_0(_S656);
    bool _S777 = _S767 < x0_5;
    float _S778;
    float _S779;
    float _S780;
    float _S781;
    float _S782;
    float _S783;
    float _S784;
    float _S785;
    float _S786;
    float _S787;
    float _S788;
    float _S789;
    float _S790;
    float _S791;
    float _S792;
    float _S793;
    float _S794;
    float _S795;
    float _S796;
    float _S797;
    float _S798;
    float _S799;
    float _S800;
    float _S801;
    float _S802;
    float _S803;
    float _S804;
    if(_S777)
    {
        float s0_4 = y0_5 / x0_5;
        float _S805 = x0_5 * x0_5;
        float t0_4 = _S767 / x0_5;
        float _S806 = s0_4 * t0_4;
        float _S807 = _S768 * t0_4;
        float _S808 = 1.0f - t0_4;
        float _S809 = _S806 * t0_4 + _S807 * _S808;
        float _S810 = y0_5 * _S809;
        float _S811 = _S768 + _S776 - 2.0f * s0_4;
        float _S812 = _S811 * t0_4;
        float _S813 = s0_4 + _S812 * _S808;
        _S778 = _S813 * _S813;
        _S779 = _S810;
        _S780 = _S813;
        _S781 = _S812;
        _S782 = _S808;
        _S783 = _S811;
        _S784 = t0_4;
        _S785 = _S809;
        _S786 = _S807;
        _S787 = _S806;
        _S788 = s0_4;
        _S789 = _S805;
        _S790 = 0.0f;
        _S791 = 0.0f;
        _S792 = 0.0f;
        _S793 = 0.0f;
        _S794 = 0.0f;
        _S795 = 0.0f;
        _S796 = 0.0f;
        _S797 = 0.0f;
        _S798 = 0.0f;
        _S799 = 0.0f;
        _S800 = 0.0f;
        _S801 = 0.0f;
        _S802 = 0.0f;
        _S803 = 0.0f;
        _S804 = 0.0f;
    }
    else
    {
        float _S814 = 1.0f - y0_5;
        float _S815 = 1.0f - x0_5;
        float s1_4 = _S814 / _S815;
        float _S816 = _S815 * _S815;
        float _S817 = _S767 - x0_5;
        float t1_4 = _S817 / _S815;
        float _S818 = s1_4 * t1_4;
        float _S819 = _S776 * t1_4;
        float _S820 = 1.0f - t1_4;
        float _S821 = _S818 * t1_4 + _S819 * _S820;
        float _S822 = _S814 * _S821;
        float _S823 = _S776 + _S769 - 2.0f * s1_4;
        float _S824 = _S823 * t1_4;
        float _S825 = s1_4 + _S824 * _S820;
        float _S826 = _S825 * _S825;
        _S778 = 0.0f;
        _S779 = 0.0f;
        _S780 = 0.0f;
        _S781 = 0.0f;
        _S782 = 0.0f;
        _S783 = 0.0f;
        _S784 = 0.0f;
        _S785 = 0.0f;
        _S786 = 0.0f;
        _S787 = 0.0f;
        _S788 = 0.0f;
        _S789 = 0.0f;
        _S790 = _S826;
        _S791 = _S822;
        _S792 = _S825;
        _S793 = _S824;
        _S794 = _S820;
        _S795 = _S823;
        _S796 = t1_4;
        _S797 = _S814;
        _S798 = _S821;
        _S799 = _S819;
        _S800 = _S818;
        _S801 = s1_4;
        _S802 = _S816;
        _S803 = _S817;
        _S804 = _S815;
    }
    float _S827 = _S706.z;
    float _S828 = s_primal_ctx_exp_0(_S657);
    float _S829 = s_primal_ctx_exp_0(_S658);
    float _S830 = - _S659;
    float _S831 = 1.0f + s_primal_ctx_exp_0(_S830);
    float x0_6 = 1.0f / _S831;
    float _S832 = _S831 * _S831;
    float _S833 = - _S660;
    float _S834 = 1.0f + s_primal_ctx_exp_0(_S833);
    float y0_6 = 1.0f / _S834;
    float _S835 = _S834 * _S834;
    float _S836 = s_primal_ctx_exp_0(_S661);
    bool _S837 = _S827 < x0_6;
    float _S838;
    float _S839;
    float _S840;
    float _S841;
    float _S842;
    float _S843;
    float _S844;
    float _S845;
    float _S846;
    float _S847;
    float _S848;
    float _S849;
    float _S850;
    float _S851;
    float _S852;
    float _S853;
    float _S854;
    float _S855;
    float _S856;
    float _S857;
    float _S858;
    float _S859;
    float _S860;
    float _S861;
    float _S862;
    float _S863;
    float _S864;
    if(_S837)
    {
        float s0_5 = y0_6 / x0_6;
        float _S865 = x0_6 * x0_6;
        float t0_5 = _S827 / x0_6;
        float _S866 = s0_5 * t0_5;
        float _S867 = _S828 * t0_5;
        float _S868 = 1.0f - t0_5;
        float _S869 = _S866 * t0_5 + _S867 * _S868;
        float _S870 = y0_6 * _S869;
        float _S871 = _S828 + _S836 - 2.0f * s0_5;
        float _S872 = _S871 * t0_5;
        float _S873 = s0_5 + _S872 * _S868;
        _S838 = _S873 * _S873;
        _S839 = _S870;
        _S840 = _S873;
        _S841 = _S872;
        _S842 = _S868;
        _S843 = _S871;
        _S844 = t0_5;
        _S845 = _S869;
        _S846 = _S867;
        _S847 = _S866;
        _S848 = s0_5;
        _S849 = _S865;
        _S850 = 0.0f;
        _S851 = 0.0f;
        _S852 = 0.0f;
        _S853 = 0.0f;
        _S854 = 0.0f;
        _S855 = 0.0f;
        _S856 = 0.0f;
        _S857 = 0.0f;
        _S858 = 0.0f;
        _S859 = 0.0f;
        _S860 = 0.0f;
        _S861 = 0.0f;
        _S862 = 0.0f;
        _S863 = 0.0f;
        _S864 = 0.0f;
    }
    else
    {
        float _S874 = 1.0f - y0_6;
        float _S875 = 1.0f - x0_6;
        float s1_5 = _S874 / _S875;
        float _S876 = _S875 * _S875;
        float _S877 = _S827 - x0_6;
        float t1_5 = _S877 / _S875;
        float _S878 = s1_5 * t1_5;
        float _S879 = _S836 * t1_5;
        float _S880 = 1.0f - t1_5;
        float _S881 = _S878 * t1_5 + _S879 * _S880;
        float _S882 = _S874 * _S881;
        float _S883 = _S836 + _S829 - 2.0f * s1_5;
        float _S884 = _S883 * t1_5;
        float _S885 = s1_5 + _S884 * _S880;
        float _S886 = _S885 * _S885;
        _S838 = 0.0f;
        _S839 = 0.0f;
        _S840 = 0.0f;
        _S841 = 0.0f;
        _S842 = 0.0f;
        _S843 = 0.0f;
        _S844 = 0.0f;
        _S845 = 0.0f;
        _S846 = 0.0f;
        _S847 = 0.0f;
        _S848 = 0.0f;
        _S849 = 0.0f;
        _S850 = _S886;
        _S851 = _S882;
        _S852 = _S885;
        _S853 = _S884;
        _S854 = _S880;
        _S855 = _S883;
        _S856 = t1_5;
        _S857 = _S874;
        _S858 = _S881;
        _S859 = _S879;
        _S860 = _S878;
        _S861 = s1_5;
        _S862 = _S876;
        _S863 = _S877;
        _S864 = _S875;
    }
    if(_S837)
    {
        float _S887 = _s_dOut_1.z / _S838;
        float _S888 = _S839 * - _S887;
        float _S889 = _S840 * _S887;
        float _S890 = _S842 * _S888;
        float _S891 = _S844 * _S890;
        float _S892 = y0_6 * _S889;
        float _S893 = _S842 * _S892;
        float _S894 = _S844 * _S892;
        float _S895 = (_S843 * _S890 + - (_S841 * _S888 + _S846 * _S892) + _S828 * _S893 + _S847 * _S892 + _S848 * _S894) / _S849;
        float _S896 = x0_6 * _S895;
        float _S897 = (_S888 + 2.0f * - _S891 + _S844 * _S894) / _S849;
        float _S898 = _S845 * _S889 + x0_6 * _S897;
        float _S899 = _S891 + _S844 * _S893;
        float _S900 = _S827 * - _S895 + y0_6 * - _S897;
        _S838 = _S891;
        _S839 = _S898;
        _S840 = _S900;
        _S841 = 0.0f;
        _S842 = _S899;
        _S843 = _S896;
    }
    else
    {
        float _S901 = _s_dOut_1.z / _S850;
        float _S902 = _S851 * - _S901;
        float _S903 = _S852 * _S901;
        float _S904 = _S854 * _S902;
        float _S905 = _S856 * _S904;
        float _S906 = _S857 * _S903;
        float _S907 = _S854 * _S906;
        float _S908 = _S856 * _S906;
        float _S909 = (_S855 * _S904 + - (_S853 * _S902 + _S859 * _S906) + _S836 * _S907 + _S860 * _S906 + _S861 * _S908) / _S862;
        float _S910 = _S864 * _S909;
        float _S911 = (_S902 + 2.0f * - _S905 + _S856 * _S908) / _S862;
        float _S912 = _s_dOut_1.z + - (_S858 * _S903 + _S864 * _S911);
        float _S913 = - _S910 + - (_S863 * - _S909 + _S857 * - _S911);
        _S838 = _S905 + _S856 * _S907;
        _S839 = _S912;
        _S840 = _S913;
        _S841 = _S905;
        _S842 = 0.0f;
        _S843 = _S910;
    }
    DiffPair_float_0 _S914;
    (&_S914)->primal_0 = _S661;
    (&_S914)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S914, _S838);
    DiffPair_float_0 _S915 = _S914;
    float _S916 = - (_S839 / _S835);
    DiffPair_float_0 _S917;
    (&_S917)->primal_0 = _S833;
    (&_S917)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S917, _S916);
    float _S918 = - _S917.differential_0;
    float _S919 = - (_S840 / _S832);
    DiffPair_float_0 _S920;
    (&_S920)->primal_0 = _S830;
    (&_S920)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S920, _S919);
    float _S921 = - _S920.differential_0;
    DiffPair_float_0 _S922;
    (&_S922)->primal_0 = _S658;
    (&_S922)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S922, _S841);
    DiffPair_float_0 _S923 = _S922;
    DiffPair_float_0 _S924;
    (&_S924)->primal_0 = _S657;
    (&_S924)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S924, _S842);
    DiffPair_float_0 _S925 = _S924;
    float3  _S926 = make_float3 (0.0f, 0.0f, _S843);
    if(_S777)
    {
        float _S927 = _s_dOut_1.y / _S778;
        float _S928 = _S779 * - _S927;
        float _S929 = _S780 * _S927;
        float _S930 = _S782 * _S928;
        float _S931 = _S784 * _S930;
        float _S932 = y0_5 * _S929;
        float _S933 = _S782 * _S932;
        float _S934 = _S784 * _S932;
        float _S935 = (_S783 * _S930 + - (_S781 * _S928 + _S786 * _S932) + _S768 * _S933 + _S787 * _S932 + _S788 * _S934) / _S789;
        float _S936 = x0_5 * _S935;
        float _S937 = (_S928 + 2.0f * - _S931 + _S784 * _S934) / _S789;
        float _S938 = _S785 * _S929 + x0_5 * _S937;
        float _S939 = _S931 + _S784 * _S933;
        float _S940 = _S767 * - _S935 + y0_5 * - _S937;
        _S778 = _S931;
        _S779 = _S938;
        _S780 = _S940;
        _S781 = 0.0f;
        _S782 = _S939;
        _S783 = _S936;
    }
    else
    {
        float _S941 = _s_dOut_1.y / _S790;
        float _S942 = _S791 * - _S941;
        float _S943 = _S792 * _S941;
        float _S944 = _S794 * _S942;
        float _S945 = _S796 * _S944;
        float _S946 = _S797 * _S943;
        float _S947 = _S794 * _S946;
        float _S948 = _S796 * _S946;
        float _S949 = (_S795 * _S944 + - (_S793 * _S942 + _S799 * _S946) + _S776 * _S947 + _S800 * _S946 + _S801 * _S948) / _S802;
        float _S950 = _S804 * _S949;
        float _S951 = (_S942 + 2.0f * - _S945 + _S796 * _S948) / _S802;
        float _S952 = _s_dOut_1.y + - (_S798 * _S943 + _S804 * _S951);
        float _S953 = - _S950 + - (_S803 * - _S949 + _S797 * - _S951);
        _S778 = _S945 + _S796 * _S947;
        _S779 = _S952;
        _S780 = _S953;
        _S781 = _S945;
        _S782 = 0.0f;
        _S783 = _S950;
    }
    DiffPair_float_0 _S954;
    (&_S954)->primal_0 = _S656;
    (&_S954)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S954, _S778);
    DiffPair_float_0 _S955 = _S954;
    float _S956 = - (_S779 / _S775);
    DiffPair_float_0 _S957;
    (&_S957)->primal_0 = _S773;
    (&_S957)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S957, _S956);
    float _S958 = - _S957.differential_0;
    float _S959 = - (_S780 / _S772);
    DiffPair_float_0 _S960;
    (&_S960)->primal_0 = _S770;
    (&_S960)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S960, _S959);
    float _S961 = - _S960.differential_0;
    DiffPair_float_0 _S962;
    (&_S962)->primal_0 = _S653;
    (&_S962)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S962, _S781);
    DiffPair_float_0 _S963 = _S962;
    DiffPair_float_0 _S964;
    (&_S964)->primal_0 = _S652;
    (&_S964)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S964, _S782);
    DiffPair_float_0 _S965 = _S964;
    float3  _S966 = _S926 + make_float3 (0.0f, _S783, 0.0f);
    if(_S717)
    {
        float _S967 = _s_dOut_1.x / _S718;
        float _S968 = _S719 * - _S967;
        float _S969 = _S720 * _S967;
        float _S970 = _S722 * _S968;
        float _S971 = _S724 * _S970;
        float _S972 = y0_4 * _S969;
        float _S973 = _S722 * _S972;
        float _S974 = _S724 * _S972;
        float _S975 = (_S723 * _S970 + - (_S721 * _S968 + _S726 * _S972) + _S708 * _S973 + _S727 * _S972 + _S728 * _S974) / _S729;
        float _S976 = x0_4 * _S975;
        float _S977 = (_S968 + 2.0f * - _S971 + _S724 * _S974) / _S729;
        float _S978 = _S725 * _S969 + x0_4 * _S977;
        float _S979 = _S971 + _S724 * _S973;
        float _S980 = _S707 * - _S975 + y0_4 * - _S977;
        _S718 = _S971;
        _S719 = _S978;
        _S720 = _S980;
        _S721 = 0.0f;
        _S722 = _S979;
        _S723 = _S976;
    }
    else
    {
        float _S981 = _s_dOut_1.x / _S730;
        float _S982 = _S731 * - _S981;
        float _S983 = _S732 * _S981;
        float _S984 = _S734 * _S982;
        float _S985 = _S736 * _S984;
        float _S986 = _S737 * _S983;
        float _S987 = _S734 * _S986;
        float _S988 = _S736 * _S986;
        float _S989 = (_S735 * _S984 + - (_S733 * _S982 + _S739 * _S986) + _S716 * _S987 + _S740 * _S986 + _S741 * _S988) / _S742;
        float _S990 = _S744 * _S989;
        float _S991 = (_S982 + 2.0f * - _S985 + _S736 * _S988) / _S742;
        float _S992 = _s_dOut_1.x + - (_S738 * _S983 + _S744 * _S991);
        float _S993 = - _S990 + - (_S743 * - _S989 + _S737 * - _S991);
        _S718 = _S985 + _S736 * _S987;
        _S719 = _S992;
        _S720 = _S993;
        _S721 = _S985;
        _S722 = 0.0f;
        _S723 = _S990;
    }
    DiffPair_float_0 _S994;
    (&_S994)->primal_0 = _S651;
    (&_S994)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S994, _S718);
    DiffPair_float_0 _S995 = _S994;
    float _S996 = - (_S719 / _S715);
    DiffPair_float_0 _S997;
    (&_S997)->primal_0 = _S713;
    (&_S997)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S997, _S996);
    float _S998 = - _S997.differential_0;
    float _S999 = - (_S720 / _S712);
    DiffPair_float_0 _S1000;
    (&_S1000)->primal_0 = _S710;
    (&_S1000)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S1000, _S999);
    float _S1001 = - _S1000.differential_0;
    DiffPair_float_0 _S1002;
    (&_S1002)->primal_0 = _S648;
    (&_S1002)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S1002, _S721);
    DiffPair_float_0 _S1003 = _S1002;
    DiffPair_float_0 _S1004;
    (&_S1004)->primal_0 = _S647;
    (&_S1004)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S1004, _S722);
    DiffPair_float_0 _S1005 = _S1004;
    float3  _S1006 = _S966 + make_float3 (_S723, 0.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1007;
    (&_S1007)->primal_0 = _S703;
    (&_S1007)->differential_0 = _S629;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1008;
    (&_S1008)->primal_0 = _S704;
    (&_S1008)->differential_0 = _S629;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1009;
    (&_S1009)->primal_0 = _S705;
    (&_S1009)->differential_0 = _S629;
    s_bwd_prop_clamp_0(&_S1007, &_S1008, &_S1009, _S1006);
    float _S1010 = - _S1007.differential_0.z;
    float _S1011 = _S1007.differential_0.y + _S1010;
    float _S1012 = norm_factor_4 * _S1011;
    float _S1013 = _S1007.differential_0.x + _S1010;
    float _S1014 = norm_factor_4 * _S1013;
    float _S1015 = (_S702 * _S1011 + _S701 * _S1013) / _S700;
    float _S1016 = intensity_4 * - _S1015;
    float _S1017 = _S699 * _S1015;
    DiffPair_float_0 _S1018;
    (&_S1018)->primal_0 = _S697;
    (&_S1018)->differential_0 = 0.0f;
    DiffPair_float_0 _S1019;
    (&_S1019)->primal_0 = _S698;
    (&_S1019)->differential_0 = 0.0f;
    _d_max_0(&_S1018, &_S1019, _S1016);
    float _S1020 = 0.05000000074505806f * _S1019.differential_0;
    DiffPair_float_0 _S1021;
    (&_S1021)->primal_0 = intensity_4;
    (&_S1021)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S1021, _S1020);
    float3  _S1022 = make_float3 (_S1014, _S1012, _S1018.differential_0);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1023;
    (&_S1023)->primal_0 = H_7;
    (&_S1023)->differential_0 = _S630;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1024;
    (&_S1024)->primal_0 = rgi_in_1;
    (&_S1024)->differential_0 = _S629;
    s_bwd_prop_mul_0(&_S1023, &_S1024, _S1022);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1025 = _S1023;
    float _S1026 = _S1007.differential_0.z + _S1017 + _S1021.differential_0 + _S1024.differential_0.z;
    float _S1027 = _S1024.differential_0.y + _S1026;
    float _S1028 = _S1024.differential_0.x + _S1026;
    float3  _S1029 = make_float3 (_S1028, _S1027, _S1026);
    if(_S689)
    {
        Matrix<float, 3, 3>  _S1030 = _S688 * _S1025.differential_0;
        Matrix<float, 3, 3>  _S1031 = _S690 * _S1025.differential_0;
        _S691 = - ((_S1030.rows[int(0)].x + _S1030.rows[int(0)].y + _S1030.rows[int(0)].z + _S1030.rows[int(1)].x + _S1030.rows[int(1)].y + _S1030.rows[int(1)].z + _S1030.rows[int(2)].x + _S1030.rows[int(2)].y + _S1030.rows[int(2)].z) / _S691);
        H_7 = _S1031;
    }
    else
    {
        _S691 = 0.0f;
        H_7 = _S1025.differential_0;
    }
    DiffPair_float_0 _S1032;
    (&_S1032)->primal_0 = _S688.rows[int(2)].z;
    (&_S1032)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S1032, 0.0f);
    float _S1033 = _S1032.differential_0 + _S691;
    float3  _S1034 = _S629;
    *&((&_S1034)->z) = _S1033;
    Matrix<float, 3, 3>  _S1035 = _S630;
    _S1035[int(2)] = _S1034;
    Matrix<float, 3, 3>  _S1036 = H_7 + _S1035;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1037;
    (&_S1037)->primal_0 = _S687;
    (&_S1037)->differential_0 = _S630;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1038;
    (&_S1038)->primal_0 = S_inv_1;
    (&_S1038)->differential_0 = _S630;
    s_bwd_prop_mul_1(&_S1037, &_S1038, _S1036);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1039;
    (&_S1039)->primal_0 = T_4;
    (&_S1039)->differential_0 = _S630;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1040;
    (&_S1040)->primal_0 = D_1;
    (&_S1040)->differential_0 = _S630;
    s_bwd_prop_mul_1(&_S1039, &_S1040, _S1037.differential_0);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1041 = _S1039;
    float3  _S1042 = make_float3 (_S1040.differential_0.rows[int(0)].x, _S1040.differential_0.rows[int(1)].y, _S1040.differential_0.rows[int(2)].z);
    float3  _S1043;
    if(_S682)
    {
        if(_S684)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1044;
            (&_S1044)->primal_0 = r1_4;
            (&_S1044)->differential_0 = _S629;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1045;
            (&_S1045)->primal_0 = r2_19;
            (&_S1045)->differential_0 = _S629;
            s_bwd_prop_cross_0(&_S1044, &_S1045, _S1042);
            _S670 = _S629;
            lambda_v_10 = _S1045.differential_0;
            _S1043 = _S1044.differential_0;
        }
        else
        {
            _S670 = _S1042;
            lambda_v_10 = _S629;
            _S1043 = _S629;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1046;
        (&_S1046)->primal_0 = _S683;
        (&_S1046)->differential_0 = _S629;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1047;
        (&_S1047)->primal_0 = _S683;
        (&_S1047)->differential_0 = _S629;
        s_bwd_prop_dot_0(&_S1046, &_S1047, 0.0f);
        float3  _S1048 = _S1047.differential_0 + _S1046.differential_0 + _S670;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1049;
        (&_S1049)->primal_0 = r0_4;
        (&_S1049)->differential_0 = _S629;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1050;
        (&_S1050)->primal_0 = r2_19;
        (&_S1050)->differential_0 = _S629;
        s_bwd_prop_cross_0(&_S1049, &_S1050, _S1048);
        float3  _S1051 = _S1050.differential_0 + lambda_v_10;
        _S670 = _S629;
        lambda_v_10 = _S1051;
        _S683 = _S1043;
        _S1043 = _S1049.differential_0;
    }
    else
    {
        _S670 = _S1042;
        lambda_v_10 = _S629;
        _S683 = _S629;
        _S1043 = _S629;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1052;
    (&_S1052)->primal_0 = _S681;
    (&_S1052)->differential_0 = _S629;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1053;
    (&_S1053)->primal_0 = _S681;
    (&_S1053)->differential_0 = _S629;
    s_bwd_prop_dot_0(&_S1052, &_S1053, 0.0f);
    float3  _S1054 = _S1053.differential_0 + _S1052.differential_0 + _S670;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1055;
    (&_S1055)->primal_0 = r0_4;
    (&_S1055)->differential_0 = _S629;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1056;
    (&_S1056)->primal_0 = r1_4;
    (&_S1056)->differential_0 = _S629;
    s_bwd_prop_cross_0(&_S1055, &_S1056, _S1054);
    float3  _S1057 = _S629;
    *&((&_S1057)->z) = lambda_v_10.z;
    *&((&_S1057)->y) = lambda_v_10.y;
    *&((&_S1057)->x) = lambda_v_10.x;
    float3  _S1058 = _S1056.differential_0 + _S683;
    float3  _S1059 = _S629;
    *&((&_S1059)->z) = _S1058.z;
    *&((&_S1059)->y) = _S1058.y;
    *&((&_S1059)->x) = _S1058.x;
    float3  _S1060 = _S1055.differential_0 + _S1043;
    float3  _S1061 = _S629;
    *&((&_S1061)->z) = _S1060.z;
    *&((&_S1061)->y) = _S1060.y;
    *&((&_S1061)->x) = _S1060.x;
    Matrix<float, 3, 3>  _S1062 = _S630;
    _S1062[int(2)] = _S1057;
    _S1062[int(1)] = _S1059;
    _S1062[int(0)] = _S1061;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1063;
    (&_S1063)->primal_0 = skew_1;
    (&_S1063)->differential_0 = _S630;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1064;
    (&_S1064)->primal_0 = T_4;
    (&_S1064)->differential_0 = _S630;
    s_bwd_prop_mul_1(&_S1063, &_S1064, _S1062);
    Matrix<float, 3, 3>  _S1065 = _S1064.differential_0 + _S1041.differential_0;
    float2  _S1066 = make_float2 (_S1063.differential_0.rows[int(2)].y + - _S1063.differential_0.rows[int(1)].z, _S1063.differential_0.rows[int(0)].z + - _S1063.differential_0.rows[int(2)].x);
    Matrix<float, 2, 2>  _S1067 = makeMatrix<float, 2, 2> (0.0f);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1068;
    (&_S1068)->primal_0 = makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f);
    (&_S1068)->differential_0 = _S1067;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1069;
    (&_S1069)->primal_0 = _S673.color_params_1.n_0;
    (&_S1069)->differential_0 = _S633;
    s_bwd_prop_mul_2(&_S1068, &_S1069, _S1066);
    float2  _S1070 = make_float2 (_S1065.rows[int(0)].z, _S1065.rows[int(1)].z);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1071;
    (&_S1071)->primal_0 = makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f);
    (&_S1071)->differential_0 = _S1067;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1072;
    (&_S1072)->primal_0 = _S673.color_params_1.g_0;
    (&_S1072)->differential_0 = _S633;
    s_bwd_prop_mul_2(&_S1071, &_S1072, _S1070);
    float2  _S1073 = make_float2 (_S1065.rows[int(0)].y, _S1065.rows[int(1)].y);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1074;
    (&_S1074)->primal_0 = makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f);
    (&_S1074)->differential_0 = _S1067;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1075;
    (&_S1075)->primal_0 = _S673.color_params_1.r_0;
    (&_S1075)->differential_0 = _S633;
    s_bwd_prop_mul_2(&_S1074, &_S1075, _S1073);
    float2  _S1076 = make_float2 (_S1065.rows[int(0)].x, _S1065.rows[int(1)].x);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1077;
    (&_S1077)->primal_0 = makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f);
    (&_S1077)->differential_0 = _S1067;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1078;
    (&_S1078)->primal_0 = _S673.color_params_1.b_0;
    (&_S1078)->differential_0 = _S633;
    s_bwd_prop_mul_2(&_S1077, &_S1078, _S1076);
    ColorPPISPParams_0 _S1079 = ColorPPISPParams_x24_syn_dzero_0();
    (&_S1079)->n_0 = _S1069.differential_0;
    (&_S1079)->g_0 = _S1072.differential_0;
    (&_S1079)->r_0 = _S1075.differential_0;
    (&_S1079)->b_0 = _S1078.differential_0;
    _S670 = _S1029;
    *&((&_S670)->z) = 0.0f;
    float _S1080 = rgb_out_6.z * _S1026;
    float _S1081 = _S672 * _S1026;
    DiffPair_float_0 _S1082;
    (&_S1082)->primal_0 = falloff_5;
    (&_S1082)->differential_0 = 0.0f;
    DiffPair_float_0 _S1083;
    (&_S1083)->primal_0 = 0.0f;
    (&_S1083)->differential_0 = 0.0f;
    DiffPair_float_0 _S1084;
    (&_S1084)->primal_0 = 1.0f;
    (&_S1084)->differential_0 = 0.0f;
    s_bwd_prop_clamp_1(&_S1082, &_S1083, &_S1084, _S1080);
    float _S1085 = r2_18 * _S1082.differential_0;
    float _S1086 = r4_14 * _S1082.differential_0;
    float s_diff_r6_T_3 = _S646 * _S1082.differential_0;
    float _S1087 = r6_5 * _S1082.differential_0;
    float _S1088 = r2_18 * (_S645 * _S1082.differential_0 + r2_18 * s_diff_r6_T_3);
    float _S1089 = _S644 * _S1082.differential_0 + r4_14 * s_diff_r6_T_3 + _S1088 + _S1088;
    float _S1090 = dy_14 * _S1089;
    float _S1091 = dx_14 * _S1089;
    float _S1092 = - (_S1090 + _S1090);
    float _S1093 = - (_S1091 + _S1091);
    *&((&_S670)->y) = 0.0f;
    float _S1094 = rgb_out_6.y * _S1027;
    float _S1095 = _S671 * _S1027;
    DiffPair_float_0 _S1096;
    (&_S1096)->primal_0 = falloff_4;
    (&_S1096)->differential_0 = 0.0f;
    DiffPair_float_0 _S1097;
    (&_S1097)->primal_0 = 0.0f;
    (&_S1097)->differential_0 = 0.0f;
    DiffPair_float_0 _S1098;
    (&_S1098)->primal_0 = 1.0f;
    (&_S1098)->differential_0 = 0.0f;
    s_bwd_prop_clamp_1(&_S1096, &_S1097, &_S1098, _S1094);
    float _S1099 = r2_17 * _S1096.differential_0;
    float _S1100 = r4_13 * _S1096.differential_0;
    float s_diff_r6_T_4 = _S643 * _S1096.differential_0;
    float _S1101 = r6_4 * _S1096.differential_0;
    float _S1102 = r2_17 * (_S642 * _S1096.differential_0 + r2_17 * s_diff_r6_T_4);
    float _S1103 = _S641 * _S1096.differential_0 + r4_13 * s_diff_r6_T_4 + _S1102 + _S1102;
    float _S1104 = dy_13 * _S1103;
    float _S1105 = dx_13 * _S1103;
    float _S1106 = - (_S1104 + _S1104);
    float _S1107 = - (_S1105 + _S1105);
    *&((&_S670)->x) = 0.0f;
    float _S1108 = rgb_out_6.x * _S1028;
    float _S1109 = _S668 * _S1028;
    DiffPair_float_0 _S1110;
    (&_S1110)->primal_0 = falloff_3;
    (&_S1110)->differential_0 = 0.0f;
    DiffPair_float_0 _S1111;
    (&_S1111)->primal_0 = 0.0f;
    (&_S1111)->differential_0 = 0.0f;
    DiffPair_float_0 _S1112;
    (&_S1112)->primal_0 = 1.0f;
    (&_S1112)->differential_0 = 0.0f;
    s_bwd_prop_clamp_1(&_S1110, &_S1111, &_S1112, _S1108);
    float _S1113 = r2_16 * _S1110.differential_0;
    float _S1114 = r4_12 * _S1110.differential_0;
    float s_diff_r6_T_5 = _S640 * _S1110.differential_0;
    float _S1115 = r6_3 * _S1110.differential_0;
    float _S1116 = r2_16 * (_S639 * _S1110.differential_0 + r2_16 * s_diff_r6_T_5);
    float _S1117 = _S638 * _S1110.differential_0 + r4_12 * s_diff_r6_T_5 + _S1116 + _S1116;
    float _S1118 = dy_12 * _S1117;
    float _S1119 = dx_12 * _S1117;
    float _S1120 = - (_S1118 + _S1118);
    float _S1121 = - (_S1119 + _S1119);
    float3  _S1122 = _S629;
    *&((&_S1122)->z) = _S1081;
    *&((&_S1122)->y) = _S1095;
    *&((&_S1122)->x) = _S1109;
    float3  _S1123 = _S670 + _S1122;
    float3  _S1124 = _S628.primal_0 * _S1123;
    float3  _S1125 = _S664 * _S1123;
    float _S1126 = _S1124.x + _S1124.y + _S1124.z;
    DiffPair_float_0 _S1127;
    (&_S1127)->primal_0 = _S662.exposure_1;
    (&_S1127)->differential_0 = 0.0f;
    s_bwd_prop_exp2_0(&_S1127, _S1126);
    PPISPParamsRQS_0 _S1128 = PPISPParamsRQS_x24_syn_dzero_0();
    (&_S1128)->color_params_1 = _S1079;
    (&_S1128)->exposure_1 = _S1127.differential_0;
    _S637 = _S1128;
    (&(&_S637)->crf_params_0[int(2)])->gc_0 = 0.0f;
    float _S1129 = _S1128.crf_params_0[int(2)].gc_0 + _S915.differential_0;
    (&(&_S637)->crf_params_0[int(2)])->y0_0 = 0.0f;
    float _S1130 = _S1128.crf_params_0[int(2)].y0_0 + _S918;
    (&(&_S637)->crf_params_0[int(2)])->x0_0 = 0.0f;
    float _S1131 = _S1128.crf_params_0[int(2)].x0_0 + _S921;
    (&(&_S637)->crf_params_0[int(2)])->g1_0 = 0.0f;
    float _S1132 = _S1128.crf_params_0[int(2)].g1_0 + _S923.differential_0;
    (&(&_S637)->crf_params_0[int(2)])->g0_0 = 0.0f;
    float _S1133 = _S1128.crf_params_0[int(2)].g0_0 + _S925.differential_0;
    (&(&_S637)->crf_params_0[int(1)])->gc_0 = 0.0f;
    float _S1134 = _S1128.crf_params_0[int(1)].gc_0 + _S955.differential_0;
    (&(&_S637)->crf_params_0[int(1)])->y0_0 = 0.0f;
    float _S1135 = _S1128.crf_params_0[int(1)].y0_0 + _S958;
    (&(&_S637)->crf_params_0[int(1)])->x0_0 = 0.0f;
    float _S1136 = _S1128.crf_params_0[int(1)].x0_0 + _S961;
    (&(&_S637)->crf_params_0[int(1)])->g1_0 = 0.0f;
    float _S1137 = _S1128.crf_params_0[int(1)].g1_0 + _S963.differential_0;
    (&(&_S637)->crf_params_0[int(1)])->g0_0 = 0.0f;
    float _S1138 = _S1128.crf_params_0[int(1)].g0_0 + _S965.differential_0;
    (&(&_S637)->crf_params_0[int(0)])->gc_0 = 0.0f;
    float _S1139 = _S1128.crf_params_0[int(0)].gc_0 + _S995.differential_0;
    (&(&_S637)->crf_params_0[int(0)])->y0_0 = 0.0f;
    float _S1140 = _S1128.crf_params_0[int(0)].y0_0 + _S998;
    (&(&_S637)->crf_params_0[int(0)])->x0_0 = 0.0f;
    float _S1141 = _S1128.crf_params_0[int(0)].x0_0 + _S1001;
    (&(&_S637)->crf_params_0[int(0)])->g1_0 = 0.0f;
    float _S1142 = _S1128.crf_params_0[int(0)].g1_0 + _S1003.differential_0;
    (&(&_S637)->crf_params_0[int(0)])->g0_0 = 0.0f;
    float _S1143 = _S1128.crf_params_0[int(0)].g0_0 + _S1005.differential_0;
    *&((&(&(&_S637)->color_params_1)->n_0)->y) = 0.0f;
    *&((&(&(&_S637)->color_params_1)->n_0)->x) = 0.0f;
    *&((&(&(&_S637)->color_params_1)->g_0)->y) = 0.0f;
    *&((&(&(&_S637)->color_params_1)->g_0)->x) = 0.0f;
    *&((&(&(&_S637)->color_params_1)->r_0)->y) = 0.0f;
    *&((&(&(&_S637)->color_params_1)->r_0)->x) = 0.0f;
    *&((&(&(&_S637)->color_params_1)->b_0)->y) = 0.0f;
    *&((&(&(&_S637)->color_params_1)->b_0)->x) = 0.0f;
    (&(&_S637)->vignette_params_1[int(2)])->alpha2_0 = 0.0f;
    float _S1144 = _S1087 + _S1128.vignette_params_1[int(2)].alpha2_0;
    (&(&_S637)->vignette_params_1[int(2)])->alpha1_0 = 0.0f;
    float _S1145 = _S1086 + _S1128.vignette_params_1[int(2)].alpha1_0;
    (&(&_S637)->vignette_params_1[int(2)])->alpha0_0 = 0.0f;
    float _S1146 = _S1085 + _S1128.vignette_params_1[int(2)].alpha0_0;
    (&(&_S637)->vignette_params_1[int(2)])->cy_0 = 0.0f;
    float _S1147 = _S1092 + _S1128.vignette_params_1[int(2)].cy_0;
    (&(&_S637)->vignette_params_1[int(2)])->cx_0 = 0.0f;
    float _S1148 = _S1093 + _S1128.vignette_params_1[int(2)].cx_0;
    (&(&_S637)->vignette_params_1[int(1)])->alpha2_0 = 0.0f;
    float _S1149 = _S1101 + _S1128.vignette_params_1[int(1)].alpha2_0;
    (&(&_S637)->vignette_params_1[int(1)])->alpha1_0 = 0.0f;
    float _S1150 = _S1100 + _S1128.vignette_params_1[int(1)].alpha1_0;
    (&(&_S637)->vignette_params_1[int(1)])->alpha0_0 = 0.0f;
    float _S1151 = _S1099 + _S1128.vignette_params_1[int(1)].alpha0_0;
    (&(&_S637)->vignette_params_1[int(1)])->cy_0 = 0.0f;
    float _S1152 = _S1106 + _S1128.vignette_params_1[int(1)].cy_0;
    (&(&_S637)->vignette_params_1[int(1)])->cx_0 = 0.0f;
    float _S1153 = _S1107 + _S1128.vignette_params_1[int(1)].cx_0;
    (&(&_S637)->vignette_params_1[int(0)])->alpha2_0 = 0.0f;
    float _S1154 = _S1115 + _S1128.vignette_params_1[int(0)].alpha2_0;
    (&(&_S637)->vignette_params_1[int(0)])->alpha1_0 = 0.0f;
    float _S1155 = _S1114 + _S1128.vignette_params_1[int(0)].alpha1_0;
    (&(&_S637)->vignette_params_1[int(0)])->alpha0_0 = 0.0f;
    float _S1156 = _S1113 + _S1128.vignette_params_1[int(0)].alpha0_0;
    (&(&_S637)->vignette_params_1[int(0)])->cy_0 = 0.0f;
    float _S1157 = _S1120 + _S1128.vignette_params_1[int(0)].cy_0;
    (&(&_S637)->vignette_params_1[int(0)])->cx_0 = 0.0f;
    float _S1158 = _S1121 + _S1128.vignette_params_1[int(0)].cx_0;
    FixedArray<float, 39>  _S1159;
    _S1159[int(0)] = 0.0f;
    _S1159[int(1)] = 0.0f;
    _S1159[int(2)] = 0.0f;
    _S1159[int(3)] = 0.0f;
    _S1159[int(4)] = 0.0f;
    _S1159[int(5)] = 0.0f;
    _S1159[int(6)] = 0.0f;
    _S1159[int(7)] = 0.0f;
    _S1159[int(8)] = 0.0f;
    _S1159[int(9)] = 0.0f;
    _S1159[int(10)] = 0.0f;
    _S1159[int(11)] = 0.0f;
    _S1159[int(12)] = 0.0f;
    _S1159[int(13)] = 0.0f;
    _S1159[int(14)] = 0.0f;
    _S1159[int(15)] = 0.0f;
    _S1159[int(16)] = 0.0f;
    _S1159[int(17)] = 0.0f;
    _S1159[int(18)] = 0.0f;
    _S1159[int(19)] = 0.0f;
    _S1159[int(20)] = 0.0f;
    _S1159[int(21)] = 0.0f;
    _S1159[int(22)] = 0.0f;
    _S1159[int(23)] = 0.0f;
    _S1159[int(24)] = 0.0f;
    _S1159[int(25)] = 0.0f;
    _S1159[int(26)] = 0.0f;
    _S1159[int(27)] = 0.0f;
    _S1159[int(28)] = 0.0f;
    _S1159[int(29)] = 0.0f;
    _S1159[int(30)] = 0.0f;
    _S1159[int(31)] = 0.0f;
    _S1159[int(32)] = 0.0f;
    _S1159[int(33)] = 0.0f;
    _S1159[int(34)] = 0.0f;
    _S1159[int(35)] = 0.0f;
    _S1159[int(36)] = 0.0f;
    _S1159[int(37)] = 0.0f;
    _S1159[int(38)] = 0.0f;
    _S1159[int(9)] = _S1150;
    _S1159[int(18)] = _S1128.color_params_1.r_0.x;
    _S1159[int(17)] = _S1128.color_params_1.b_0.y;
    _S1159[int(16)] = _S1128.color_params_1.b_0.x;
    _S1159[int(15)] = _S1144;
    _S1159[int(14)] = _S1145;
    _S1159[int(13)] = _S1146;
    _S1159[int(12)] = _S1147;
    _S1159[int(11)] = _S1148;
    _S1159[int(10)] = _S1149;
    _S1159[int(19)] = _S1128.color_params_1.r_0.y;
    _S1159[int(8)] = _S1151;
    _S1159[int(7)] = _S1152;
    _S1159[int(6)] = _S1153;
    _S1159[int(5)] = _S1154;
    _S1159[int(4)] = _S1155;
    _S1159[int(3)] = _S1156;
    _S1159[int(2)] = _S1157;
    _S1159[int(1)] = _S1158;
    _S1159[int(0)] = _S637.exposure_1;
    _S1159[int(28)] = _S1139;
    _S1159[int(37)] = _S1130;
    _S1159[int(36)] = _S1131;
    _S1159[int(35)] = _S1132;
    _S1159[int(34)] = _S1133;
    _S1159[int(33)] = _S1134;
    _S1159[int(32)] = _S1135;
    _S1159[int(31)] = _S1136;
    _S1159[int(30)] = _S1137;
    _S1159[int(29)] = _S1138;
    _S1159[int(38)] = _S1129;
    _S1159[int(27)] = _S1140;
    _S1159[int(26)] = _S1141;
    _S1159[int(25)] = _S1142;
    _S1159[int(24)] = _S1143;
    _S1159[int(23)] = _S1128.color_params_1.n_0.y;
    _S1159[int(22)] = _S1128.color_params_1.n_0.x;
    _S1159[int(21)] = _S1128.color_params_1.g_0.y;
    _S1159[int(20)] = _S1128.color_params_1.g_0.x;
    dpparams_1->primal_0 = dpparams_1->primal_0;
    dpparams_1->differential_0 = _S1159;
    dprgb_in_1->primal_0 = (*dprgb_in_1).primal_0;
    dprgb_in_1->differential_0 = _S1125;
    return;
}

inline __device__ void s_bwd_apply_ppisp_rqs_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1160, float2  _S1161, float2  _S1162, float2  _S1163, DiffPair_arrayx3Cfloatx2C39x3E_0 * _S1164, float3  _S1165)
{
    s_bwd_prop_apply_ppisp_rqs_0(_S1160, _S1161, _S1162, _S1163, _S1164, _S1165);
    return;
}

inline __device__ void apply_ppisp_rqs_vjp(float3  rgb_in_4, float2  pix_coord_6, float2  image_center_6, float2  img_size_6, FixedArray<float, 39>  params_4, float3  grad_out_1, float3  * grad_rgb_in_1, FixedArray<float, 39>  * grad_params_1)
{
    float3  _S1166 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_rgb_in_1;
    (&dp_rgb_in_1)->primal_0 = rgb_in_4;
    (&dp_rgb_in_1)->differential_0 = _S1166;
    FixedArray<float, 39>  _S1167 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    DiffPair_arrayx3Cfloatx2C39x3E_0 dp_params_1;
    (&dp_params_1)->primal_0 = params_4;
    (&dp_params_1)->differential_0 = _S1167;
    s_bwd_apply_ppisp_rqs_0(&dp_rgb_in_1, pix_coord_6, image_center_6, img_size_6, &dp_params_1, grad_out_1);
    *grad_rgb_in_1 = dp_rgb_in_1.differential_0;
    *grad_params_1 = (&dp_params_1)->differential_0;
    return;
}

struct DiffPair_arrayx3Cfloatx2C24x3E_0
{
    FixedArray<float, 24>  primal_0;
    FixedArray<float, 24>  differential_0;
};

inline __device__ void s_bwd_prop_apply_ppisp_no_crf_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dprgb_in_2, float2  pix_coord_7, float2  image_center_7, float2  img_size_7, DiffPair_arrayx3Cfloatx2C24x3E_0 * dpparams_2, float3  _s_dOut_2)
{
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1168 = *dprgb_in_2;
    float3  _S1169 = make_float3 (0.0f);
    Matrix<float, 3, 3>  _S1170 = makeMatrix<float, 3, 3> (0.0f);
    VignettingChannelParams_0 _S1171 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    FixedArray<VignettingChannelParams_0, 3>  _S1172 = {
        _S1171, _S1171, _S1171
    };
    float2  _S1173 = make_float2 (0.0f);
    ColorPPISPParams_0 _S1174 = { _S1173, _S1173, _S1173, _S1173 };
    PPISPParamsNoCRF_0 _S1175;
    (&_S1175)->exposure_0 = dpparams_2->primal_0[int(0)];
    (&_S1175)->vignette_params_0 = _S1172;
    (&_S1175)->color_params_0 = _S1174;
    (&(&_S1175)->vignette_params_0[int(0)])->cx_0 = dpparams_2->primal_0[int(1)];
    (&(&_S1175)->vignette_params_0[int(0)])->cy_0 = dpparams_2->primal_0[int(2)];
    float _S1176 = dpparams_2->primal_0[int(3)];
    (&(&_S1175)->vignette_params_0[int(0)])->alpha0_0 = dpparams_2->primal_0[int(3)];
    float _S1177 = dpparams_2->primal_0[int(4)];
    (&(&_S1175)->vignette_params_0[int(0)])->alpha1_0 = dpparams_2->primal_0[int(4)];
    float _S1178 = dpparams_2->primal_0[int(5)];
    (&(&_S1175)->vignette_params_0[int(0)])->alpha2_0 = dpparams_2->primal_0[int(5)];
    (&(&_S1175)->vignette_params_0[int(1)])->cx_0 = dpparams_2->primal_0[int(6)];
    (&(&_S1175)->vignette_params_0[int(1)])->cy_0 = dpparams_2->primal_0[int(7)];
    float _S1179 = dpparams_2->primal_0[int(8)];
    (&(&_S1175)->vignette_params_0[int(1)])->alpha0_0 = dpparams_2->primal_0[int(8)];
    float _S1180 = dpparams_2->primal_0[int(9)];
    (&(&_S1175)->vignette_params_0[int(1)])->alpha1_0 = dpparams_2->primal_0[int(9)];
    float _S1181 = dpparams_2->primal_0[int(10)];
    (&(&_S1175)->vignette_params_0[int(1)])->alpha2_0 = dpparams_2->primal_0[int(10)];
    (&(&_S1175)->vignette_params_0[int(2)])->cx_0 = dpparams_2->primal_0[int(11)];
    (&(&_S1175)->vignette_params_0[int(2)])->cy_0 = dpparams_2->primal_0[int(12)];
    float _S1182 = dpparams_2->primal_0[int(13)];
    (&(&_S1175)->vignette_params_0[int(2)])->alpha0_0 = dpparams_2->primal_0[int(13)];
    float _S1183 = dpparams_2->primal_0[int(14)];
    (&(&_S1175)->vignette_params_0[int(2)])->alpha1_0 = dpparams_2->primal_0[int(14)];
    float _S1184 = dpparams_2->primal_0[int(15)];
    (&(&_S1175)->vignette_params_0[int(2)])->alpha2_0 = dpparams_2->primal_0[int(15)];
    *&((&(&(&_S1175)->color_params_0)->b_0)->x) = dpparams_2->primal_0[int(16)];
    *&((&(&(&_S1175)->color_params_0)->b_0)->y) = dpparams_2->primal_0[int(17)];
    *&((&(&(&_S1175)->color_params_0)->r_0)->x) = dpparams_2->primal_0[int(18)];
    *&((&(&(&_S1175)->color_params_0)->r_0)->y) = dpparams_2->primal_0[int(19)];
    *&((&(&(&_S1175)->color_params_0)->g_0)->x) = dpparams_2->primal_0[int(20)];
    *&((&(&(&_S1175)->color_params_0)->g_0)->y) = dpparams_2->primal_0[int(21)];
    *&((&(&(&_S1175)->color_params_0)->n_0)->x) = dpparams_2->primal_0[int(22)];
    *&((&(&(&_S1175)->color_params_0)->n_0)->y) = dpparams_2->primal_0[int(23)];
    PPISPParamsNoCRF_0 _S1185 = _S1175;
    float _S1186 = s_primal_ctx_exp2_0(_S1175.exposure_0);
    float3  _S1187 = make_float3 (_S1186);
    float3  rgb_out_7 = (*dprgb_in_2).primal_0 * make_float3 (_S1186);
    float _S1188 = (F32_max((img_size_7.x), (img_size_7.y)));
    float _S1189 = (pix_coord_7.x - image_center_7.x) / _S1188;
    float _S1190 = (pix_coord_7.y - image_center_7.y) / _S1188;
    float dx_15 = _S1189 - dpparams_2->primal_0[int(1)];
    float dy_15 = _S1190 - dpparams_2->primal_0[int(2)];
    float r2_20 = dx_15 * dx_15 + dy_15 * dy_15;
    float r4_15 = r2_20 * r2_20;
    float r6_6 = r4_15 * r2_20;
    float falloff_6 = dpparams_2->primal_0[int(5)] * r6_6 + dpparams_2->primal_0[int(4)] * r4_15 + dpparams_2->primal_0[int(3)] * r2_20 + 1.0f;
    float _S1191 = s_primal_ctx_clamp_0(falloff_6, 0.0f, 1.0f);
    float _S1192 = rgb_out_7.x * _S1191;
    float3  _S1193 = rgb_out_7;
    *&((&_S1193)->x) = _S1192;
    float dx_16 = _S1189 - dpparams_2->primal_0[int(6)];
    float dy_16 = _S1190 - dpparams_2->primal_0[int(7)];
    float r2_21 = dx_16 * dx_16 + dy_16 * dy_16;
    float r4_16 = r2_21 * r2_21;
    float r6_7 = r4_16 * r2_21;
    float falloff_7 = dpparams_2->primal_0[int(10)] * r6_7 + dpparams_2->primal_0[int(9)] * r4_16 + dpparams_2->primal_0[int(8)] * r2_21 + 1.0f;
    float _S1194 = s_primal_ctx_clamp_0(falloff_7, 0.0f, 1.0f);
    *&((&_S1193)->y) = rgb_out_7.y * _S1194;
    float dx_17 = _S1189 - dpparams_2->primal_0[int(11)];
    float dy_17 = _S1190 - dpparams_2->primal_0[int(12)];
    float r2_22 = dx_17 * dx_17 + dy_17 * dy_17;
    float r4_17 = r2_22 * r2_22;
    float r6_8 = r4_17 * r2_22;
    float falloff_8 = dpparams_2->primal_0[int(15)] * r6_8 + dpparams_2->primal_0[int(14)] * r4_17 + dpparams_2->primal_0[int(13)] * r2_22 + 1.0f;
    float _S1195 = s_primal_ctx_clamp_0(falloff_8, 0.0f, 1.0f);
    *&((&_S1193)->z) = rgb_out_7.z * _S1195;
    PPISPParamsNoCRF_0 _S1196 = _S1175;
    float2  _S1197 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f), _S1175.color_params_0.b_0);
    float2  _S1198 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f), _S1175.color_params_0.r_0);
    float2  _S1199 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f), _S1175.color_params_0.g_0);
    float2  _S1200 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f), _S1175.color_params_0.n_0);
    float _S1201 = 0.3333333432674408f + _S1200.x;
    float _S1202 = 0.3333333432674408f + _S1200.y;
    Matrix<float, 3, 3>  T_5 = makeMatrix<float, 3, 3> (_S1197.x, 1.0f + _S1198.x, _S1199.x, _S1197.y, _S1198.y, 1.0f + _S1199.y, 1.0f, 1.0f, 1.0f);
    Matrix<float, 3, 3>  skew_2 = makeMatrix<float, 3, 3> (0.0f, -1.0f, _S1202, 1.0f, 0.0f, - _S1201, - _S1202, _S1201, 0.0f);
    Matrix<float, 3, 3>  _S1203 = s_primal_ctx_mul_1(skew_2, T_5);
    float3  r0_5 = make_float3 (_S1203.rows[int(0)].x, _S1203.rows[int(0)].y, _S1203.rows[int(0)].z);
    float3  r1_5 = make_float3 (_S1203.rows[int(1)].x, _S1203.rows[int(1)].y, _S1203.rows[int(1)].z);
    float3  r2_23 = make_float3 (_S1203.rows[int(2)].x, _S1203.rows[int(2)].y, _S1203.rows[int(2)].z);
    float3  _S1204 = s_primal_ctx_cross_0(r0_5, r1_5);
    bool _S1205 = (s_primal_ctx_dot_0(_S1204, _S1204)) < 9.99999968265522539e-21f;
    float3  lambda_v_11;
    float3  _S1206;
    bool _S1207;
    if(_S1205)
    {
        float3  _S1208 = s_primal_ctx_cross_0(r0_5, r2_23);
        bool _S1209 = (s_primal_ctx_dot_0(_S1208, _S1208)) < 9.99999968265522539e-21f;
        if(_S1209)
        {
            lambda_v_11 = s_primal_ctx_cross_0(r1_5, r2_23);
        }
        else
        {
            lambda_v_11 = _S1208;
        }
        _S1207 = _S1209;
        _S1206 = _S1208;
    }
    else
    {
        lambda_v_11 = _S1204;
        _S1207 = false;
        _S1206 = _S1169;
    }
    Matrix<float, 3, 3>  S_inv_2 = makeMatrix<float, 3, 3> (-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    Matrix<float, 3, 3>  D_2 = makeMatrix<float, 3, 3> (lambda_v_11.x, 0.0f, 0.0f, 0.0f, lambda_v_11.y, 0.0f, 0.0f, 0.0f, lambda_v_11.z);
    Matrix<float, 3, 3>  _S1210 = s_primal_ctx_mul_1(T_5, D_2);
    Matrix<float, 3, 3>  _S1211 = s_primal_ctx_mul_1(_S1210, S_inv_2);
    bool _S1212 = (s_primal_ctx_abs_0(_S1211.rows[int(2)].z)) > 9.99999968265522539e-21f;
    Matrix<float, 3, 3>  H_8;
    Matrix<float, 3, 3>  _S1213;
    float _S1214;
    if(_S1212)
    {
        float inv_s_2 = 1.0f / _S1211.rows[int(2)].z;
        Matrix<float, 3, 3>  _S1215 = makeMatrix<float, 3, 3> (inv_s_2);
        float _S1216 = _S1211.rows[int(2)].z * _S1211.rows[int(2)].z;
        H_8 = _S1211 * makeMatrix<float, 3, 3> (inv_s_2);
        _S1213 = _S1215;
        _S1214 = _S1216;
    }
    else
    {
        H_8 = _S1211;
        _S1213 = _S1170;
        _S1214 = 0.0f;
    }
    float _S1217 = _S1193.x;
    float _S1218 = _S1193.y;
    float intensity_5 = _S1217 + _S1218 + _S1193.z;
    float3  rgi_in_2 = make_float3 (_S1217, _S1218, intensity_5);
    float3  _S1219 = s_primal_ctx_mul_2(H_8, rgi_in_2);
    float _S1220 = _S1219.z;
    float _S1221 = 0.05000000074505806f * s_primal_ctx_abs_0(intensity_5) + 9.99999993922529029e-09f;
    float _S1222 = (F32_max((_S1220), (_S1221)));
    float norm_factor_5 = intensity_5 / _S1222;
    float _S1223 = - _s_dOut_2.z;
    float _S1224 = _s_dOut_2.y + _S1223;
    float _S1225 = norm_factor_5 * _S1224;
    float _S1226 = _s_dOut_2.x + _S1223;
    float _S1227 = norm_factor_5 * _S1226;
    float _S1228 = (_S1219.y * _S1224 + _S1219.x * _S1226) / (_S1222 * _S1222);
    float _S1229 = intensity_5 * - _S1228;
    float _S1230 = _S1222 * _S1228;
    DiffPair_float_0 _S1231;
    (&_S1231)->primal_0 = _S1220;
    (&_S1231)->differential_0 = 0.0f;
    DiffPair_float_0 _S1232;
    (&_S1232)->primal_0 = _S1221;
    (&_S1232)->differential_0 = 0.0f;
    _d_max_0(&_S1231, &_S1232, _S1229);
    float _S1233 = 0.05000000074505806f * _S1232.differential_0;
    DiffPair_float_0 _S1234;
    (&_S1234)->primal_0 = intensity_5;
    (&_S1234)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S1234, _S1233);
    float3  _S1235 = make_float3 (_S1227, _S1225, _S1231.differential_0);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1236;
    (&_S1236)->primal_0 = H_8;
    (&_S1236)->differential_0 = _S1170;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1237;
    (&_S1237)->primal_0 = rgi_in_2;
    (&_S1237)->differential_0 = _S1169;
    s_bwd_prop_mul_0(&_S1236, &_S1237, _S1235);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1238 = _S1236;
    float _S1239 = _s_dOut_2.z + _S1230 + _S1234.differential_0 + _S1237.differential_0.z;
    float _S1240 = _S1237.differential_0.y + _S1239;
    float _S1241 = _S1237.differential_0.x + _S1239;
    float3  _S1242 = make_float3 (_S1241, _S1240, _S1239);
    if(_S1212)
    {
        Matrix<float, 3, 3>  _S1243 = _S1211 * _S1238.differential_0;
        Matrix<float, 3, 3>  _S1244 = _S1213 * _S1238.differential_0;
        _S1214 = - ((_S1243.rows[int(0)].x + _S1243.rows[int(0)].y + _S1243.rows[int(0)].z + _S1243.rows[int(1)].x + _S1243.rows[int(1)].y + _S1243.rows[int(1)].z + _S1243.rows[int(2)].x + _S1243.rows[int(2)].y + _S1243.rows[int(2)].z) / _S1214);
        H_8 = _S1244;
    }
    else
    {
        _S1214 = 0.0f;
        H_8 = _S1238.differential_0;
    }
    DiffPair_float_0 _S1245;
    (&_S1245)->primal_0 = _S1211.rows[int(2)].z;
    (&_S1245)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S1245, 0.0f);
    float _S1246 = _S1245.differential_0 + _S1214;
    float3  _S1247 = _S1169;
    *&((&_S1247)->z) = _S1246;
    Matrix<float, 3, 3>  _S1248 = _S1170;
    _S1248[int(2)] = _S1247;
    Matrix<float, 3, 3>  _S1249 = H_8 + _S1248;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1250;
    (&_S1250)->primal_0 = _S1210;
    (&_S1250)->differential_0 = _S1170;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1251;
    (&_S1251)->primal_0 = S_inv_2;
    (&_S1251)->differential_0 = _S1170;
    s_bwd_prop_mul_1(&_S1250, &_S1251, _S1249);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1252;
    (&_S1252)->primal_0 = T_5;
    (&_S1252)->differential_0 = _S1170;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1253;
    (&_S1253)->primal_0 = D_2;
    (&_S1253)->differential_0 = _S1170;
    s_bwd_prop_mul_1(&_S1252, &_S1253, _S1250.differential_0);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1254 = _S1252;
    float3  _S1255 = make_float3 (_S1253.differential_0.rows[int(0)].x, _S1253.differential_0.rows[int(1)].y, _S1253.differential_0.rows[int(2)].z);
    float3  _S1256;
    if(_S1205)
    {
        if(_S1207)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1257;
            (&_S1257)->primal_0 = r1_5;
            (&_S1257)->differential_0 = _S1169;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1258;
            (&_S1258)->primal_0 = r2_23;
            (&_S1258)->differential_0 = _S1169;
            s_bwd_prop_cross_0(&_S1257, &_S1258, _S1255);
            _S1193 = _S1169;
            lambda_v_11 = _S1258.differential_0;
            _S1256 = _S1257.differential_0;
        }
        else
        {
            _S1193 = _S1255;
            lambda_v_11 = _S1169;
            _S1256 = _S1169;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1259;
        (&_S1259)->primal_0 = _S1206;
        (&_S1259)->differential_0 = _S1169;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1260;
        (&_S1260)->primal_0 = _S1206;
        (&_S1260)->differential_0 = _S1169;
        s_bwd_prop_dot_0(&_S1259, &_S1260, 0.0f);
        float3  _S1261 = _S1260.differential_0 + _S1259.differential_0 + _S1193;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1262;
        (&_S1262)->primal_0 = r0_5;
        (&_S1262)->differential_0 = _S1169;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1263;
        (&_S1263)->primal_0 = r2_23;
        (&_S1263)->differential_0 = _S1169;
        s_bwd_prop_cross_0(&_S1262, &_S1263, _S1261);
        float3  _S1264 = _S1263.differential_0 + lambda_v_11;
        _S1193 = _S1169;
        lambda_v_11 = _S1264;
        _S1206 = _S1256;
        _S1256 = _S1262.differential_0;
    }
    else
    {
        _S1193 = _S1255;
        lambda_v_11 = _S1169;
        _S1206 = _S1169;
        _S1256 = _S1169;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1265;
    (&_S1265)->primal_0 = _S1204;
    (&_S1265)->differential_0 = _S1169;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1266;
    (&_S1266)->primal_0 = _S1204;
    (&_S1266)->differential_0 = _S1169;
    s_bwd_prop_dot_0(&_S1265, &_S1266, 0.0f);
    float3  _S1267 = _S1266.differential_0 + _S1265.differential_0 + _S1193;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1268;
    (&_S1268)->primal_0 = r0_5;
    (&_S1268)->differential_0 = _S1169;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1269;
    (&_S1269)->primal_0 = r1_5;
    (&_S1269)->differential_0 = _S1169;
    s_bwd_prop_cross_0(&_S1268, &_S1269, _S1267);
    float3  _S1270 = _S1169;
    *&((&_S1270)->z) = lambda_v_11.z;
    *&((&_S1270)->y) = lambda_v_11.y;
    *&((&_S1270)->x) = lambda_v_11.x;
    float3  _S1271 = _S1269.differential_0 + _S1206;
    float3  _S1272 = _S1169;
    *&((&_S1272)->z) = _S1271.z;
    *&((&_S1272)->y) = _S1271.y;
    *&((&_S1272)->x) = _S1271.x;
    float3  _S1273 = _S1268.differential_0 + _S1256;
    float3  _S1274 = _S1169;
    *&((&_S1274)->z) = _S1273.z;
    *&((&_S1274)->y) = _S1273.y;
    *&((&_S1274)->x) = _S1273.x;
    Matrix<float, 3, 3>  _S1275 = _S1170;
    _S1275[int(2)] = _S1270;
    _S1275[int(1)] = _S1272;
    _S1275[int(0)] = _S1274;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1276;
    (&_S1276)->primal_0 = skew_2;
    (&_S1276)->differential_0 = _S1170;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1277;
    (&_S1277)->primal_0 = T_5;
    (&_S1277)->differential_0 = _S1170;
    s_bwd_prop_mul_1(&_S1276, &_S1277, _S1275);
    Matrix<float, 3, 3>  _S1278 = _S1277.differential_0 + _S1254.differential_0;
    float2  _S1279 = make_float2 (_S1276.differential_0.rows[int(2)].y + - _S1276.differential_0.rows[int(1)].z, _S1276.differential_0.rows[int(0)].z + - _S1276.differential_0.rows[int(2)].x);
    Matrix<float, 2, 2>  _S1280 = makeMatrix<float, 2, 2> (0.0f);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1281;
    (&_S1281)->primal_0 = makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f);
    (&_S1281)->differential_0 = _S1280;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1282;
    (&_S1282)->primal_0 = _S1196.color_params_0.n_0;
    (&_S1282)->differential_0 = _S1173;
    s_bwd_prop_mul_2(&_S1281, &_S1282, _S1279);
    float2  _S1283 = make_float2 (_S1278.rows[int(0)].z, _S1278.rows[int(1)].z);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1284;
    (&_S1284)->primal_0 = makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f);
    (&_S1284)->differential_0 = _S1280;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1285;
    (&_S1285)->primal_0 = _S1196.color_params_0.g_0;
    (&_S1285)->differential_0 = _S1173;
    s_bwd_prop_mul_2(&_S1284, &_S1285, _S1283);
    float2  _S1286 = make_float2 (_S1278.rows[int(0)].y, _S1278.rows[int(1)].y);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1287;
    (&_S1287)->primal_0 = makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f);
    (&_S1287)->differential_0 = _S1280;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1288;
    (&_S1288)->primal_0 = _S1196.color_params_0.r_0;
    (&_S1288)->differential_0 = _S1173;
    s_bwd_prop_mul_2(&_S1287, &_S1288, _S1286);
    float2  _S1289 = make_float2 (_S1278.rows[int(0)].x, _S1278.rows[int(1)].x);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1290;
    (&_S1290)->primal_0 = makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f);
    (&_S1290)->differential_0 = _S1280;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1291;
    (&_S1291)->primal_0 = _S1196.color_params_0.b_0;
    (&_S1291)->differential_0 = _S1173;
    s_bwd_prop_mul_2(&_S1290, &_S1291, _S1289);
    ColorPPISPParams_0 _S1292 = ColorPPISPParams_x24_syn_dzero_0();
    (&_S1292)->n_0 = _S1282.differential_0;
    (&_S1292)->g_0 = _S1285.differential_0;
    (&_S1292)->r_0 = _S1288.differential_0;
    (&_S1292)->b_0 = _S1291.differential_0;
    _S1193 = _S1242;
    *&((&_S1193)->z) = 0.0f;
    float _S1293 = rgb_out_7.z * _S1239;
    float _S1294 = _S1195 * _S1239;
    DiffPair_float_0 _S1295;
    (&_S1295)->primal_0 = falloff_8;
    (&_S1295)->differential_0 = 0.0f;
    DiffPair_float_0 _S1296;
    (&_S1296)->primal_0 = 0.0f;
    (&_S1296)->differential_0 = 0.0f;
    DiffPair_float_0 _S1297;
    (&_S1297)->primal_0 = 1.0f;
    (&_S1297)->differential_0 = 0.0f;
    s_bwd_prop_clamp_1(&_S1295, &_S1296, &_S1297, _S1293);
    float _S1298 = r2_22 * _S1295.differential_0;
    float _S1299 = r4_17 * _S1295.differential_0;
    float s_diff_r6_T_6 = _S1184 * _S1295.differential_0;
    float _S1300 = r6_8 * _S1295.differential_0;
    float _S1301 = r2_22 * (_S1183 * _S1295.differential_0 + r2_22 * s_diff_r6_T_6);
    float _S1302 = _S1182 * _S1295.differential_0 + r4_17 * s_diff_r6_T_6 + _S1301 + _S1301;
    float _S1303 = dy_17 * _S1302;
    float _S1304 = dx_17 * _S1302;
    float _S1305 = - (_S1303 + _S1303);
    float _S1306 = - (_S1304 + _S1304);
    *&((&_S1193)->y) = 0.0f;
    float _S1307 = rgb_out_7.y * _S1240;
    float _S1308 = _S1194 * _S1240;
    DiffPair_float_0 _S1309;
    (&_S1309)->primal_0 = falloff_7;
    (&_S1309)->differential_0 = 0.0f;
    DiffPair_float_0 _S1310;
    (&_S1310)->primal_0 = 0.0f;
    (&_S1310)->differential_0 = 0.0f;
    DiffPair_float_0 _S1311;
    (&_S1311)->primal_0 = 1.0f;
    (&_S1311)->differential_0 = 0.0f;
    s_bwd_prop_clamp_1(&_S1309, &_S1310, &_S1311, _S1307);
    float _S1312 = r2_21 * _S1309.differential_0;
    float _S1313 = r4_16 * _S1309.differential_0;
    float s_diff_r6_T_7 = _S1181 * _S1309.differential_0;
    float _S1314 = r6_7 * _S1309.differential_0;
    float _S1315 = r2_21 * (_S1180 * _S1309.differential_0 + r2_21 * s_diff_r6_T_7);
    float _S1316 = _S1179 * _S1309.differential_0 + r4_16 * s_diff_r6_T_7 + _S1315 + _S1315;
    float _S1317 = dy_16 * _S1316;
    float _S1318 = dx_16 * _S1316;
    float _S1319 = - (_S1317 + _S1317);
    float _S1320 = - (_S1318 + _S1318);
    *&((&_S1193)->x) = 0.0f;
    float _S1321 = rgb_out_7.x * _S1241;
    float _S1322 = _S1191 * _S1241;
    DiffPair_float_0 _S1323;
    (&_S1323)->primal_0 = falloff_6;
    (&_S1323)->differential_0 = 0.0f;
    DiffPair_float_0 _S1324;
    (&_S1324)->primal_0 = 0.0f;
    (&_S1324)->differential_0 = 0.0f;
    DiffPair_float_0 _S1325;
    (&_S1325)->primal_0 = 1.0f;
    (&_S1325)->differential_0 = 0.0f;
    s_bwd_prop_clamp_1(&_S1323, &_S1324, &_S1325, _S1321);
    float _S1326 = r2_20 * _S1323.differential_0;
    float _S1327 = r4_15 * _S1323.differential_0;
    float s_diff_r6_T_8 = _S1178 * _S1323.differential_0;
    float _S1328 = r6_6 * _S1323.differential_0;
    float _S1329 = r2_20 * (_S1177 * _S1323.differential_0 + r2_20 * s_diff_r6_T_8);
    float _S1330 = _S1176 * _S1323.differential_0 + r4_15 * s_diff_r6_T_8 + _S1329 + _S1329;
    float _S1331 = dy_15 * _S1330;
    float _S1332 = dx_15 * _S1330;
    float _S1333 = - (_S1331 + _S1331);
    float _S1334 = - (_S1332 + _S1332);
    float3  _S1335 = _S1169;
    *&((&_S1335)->z) = _S1294;
    *&((&_S1335)->y) = _S1308;
    *&((&_S1335)->x) = _S1322;
    float3  _S1336 = _S1193 + _S1335;
    float3  _S1337 = _S1168.primal_0 * _S1336;
    float3  _S1338 = _S1187 * _S1336;
    float _S1339 = _S1337.x + _S1337.y + _S1337.z;
    DiffPair_float_0 _S1340;
    (&_S1340)->primal_0 = _S1185.exposure_0;
    (&_S1340)->differential_0 = 0.0f;
    s_bwd_prop_exp2_0(&_S1340, _S1339);
    PPISPParamsNoCRF_0 _S1341 = PPISPParamsNoCRF_x24_syn_dzero_0();
    (&_S1341)->color_params_0 = _S1292;
    (&_S1341)->exposure_0 = _S1340.differential_0;
    _S1175 = _S1341;
    *&((&(&(&_S1175)->color_params_0)->n_0)->y) = 0.0f;
    *&((&(&(&_S1175)->color_params_0)->n_0)->x) = 0.0f;
    *&((&(&(&_S1175)->color_params_0)->g_0)->y) = 0.0f;
    *&((&(&(&_S1175)->color_params_0)->g_0)->x) = 0.0f;
    *&((&(&(&_S1175)->color_params_0)->r_0)->y) = 0.0f;
    *&((&(&(&_S1175)->color_params_0)->r_0)->x) = 0.0f;
    *&((&(&(&_S1175)->color_params_0)->b_0)->y) = 0.0f;
    *&((&(&(&_S1175)->color_params_0)->b_0)->x) = 0.0f;
    (&(&_S1175)->vignette_params_0[int(2)])->alpha2_0 = 0.0f;
    float _S1342 = _S1300 + _S1341.vignette_params_0[int(2)].alpha2_0;
    (&(&_S1175)->vignette_params_0[int(2)])->alpha1_0 = 0.0f;
    float _S1343 = _S1299 + _S1341.vignette_params_0[int(2)].alpha1_0;
    (&(&_S1175)->vignette_params_0[int(2)])->alpha0_0 = 0.0f;
    float _S1344 = _S1298 + _S1341.vignette_params_0[int(2)].alpha0_0;
    (&(&_S1175)->vignette_params_0[int(2)])->cy_0 = 0.0f;
    float _S1345 = _S1305 + _S1341.vignette_params_0[int(2)].cy_0;
    (&(&_S1175)->vignette_params_0[int(2)])->cx_0 = 0.0f;
    float _S1346 = _S1306 + _S1341.vignette_params_0[int(2)].cx_0;
    (&(&_S1175)->vignette_params_0[int(1)])->alpha2_0 = 0.0f;
    float _S1347 = _S1314 + _S1341.vignette_params_0[int(1)].alpha2_0;
    (&(&_S1175)->vignette_params_0[int(1)])->alpha1_0 = 0.0f;
    float _S1348 = _S1313 + _S1341.vignette_params_0[int(1)].alpha1_0;
    (&(&_S1175)->vignette_params_0[int(1)])->alpha0_0 = 0.0f;
    float _S1349 = _S1312 + _S1341.vignette_params_0[int(1)].alpha0_0;
    (&(&_S1175)->vignette_params_0[int(1)])->cy_0 = 0.0f;
    float _S1350 = _S1319 + _S1341.vignette_params_0[int(1)].cy_0;
    (&(&_S1175)->vignette_params_0[int(1)])->cx_0 = 0.0f;
    float _S1351 = _S1320 + _S1341.vignette_params_0[int(1)].cx_0;
    (&(&_S1175)->vignette_params_0[int(0)])->alpha2_0 = 0.0f;
    float _S1352 = _S1328 + _S1341.vignette_params_0[int(0)].alpha2_0;
    (&(&_S1175)->vignette_params_0[int(0)])->alpha1_0 = 0.0f;
    float _S1353 = _S1327 + _S1341.vignette_params_0[int(0)].alpha1_0;
    (&(&_S1175)->vignette_params_0[int(0)])->alpha0_0 = 0.0f;
    float _S1354 = _S1326 + _S1341.vignette_params_0[int(0)].alpha0_0;
    (&(&_S1175)->vignette_params_0[int(0)])->cy_0 = 0.0f;
    float _S1355 = _S1333 + _S1341.vignette_params_0[int(0)].cy_0;
    (&(&_S1175)->vignette_params_0[int(0)])->cx_0 = 0.0f;
    float _S1356 = _S1334 + _S1341.vignette_params_0[int(0)].cx_0;
    FixedArray<float, 24>  _S1357;
    _S1357[int(0)] = 0.0f;
    _S1357[int(1)] = 0.0f;
    _S1357[int(2)] = 0.0f;
    _S1357[int(3)] = 0.0f;
    _S1357[int(4)] = 0.0f;
    _S1357[int(5)] = 0.0f;
    _S1357[int(6)] = 0.0f;
    _S1357[int(7)] = 0.0f;
    _S1357[int(8)] = 0.0f;
    _S1357[int(9)] = 0.0f;
    _S1357[int(10)] = 0.0f;
    _S1357[int(11)] = 0.0f;
    _S1357[int(12)] = 0.0f;
    _S1357[int(13)] = 0.0f;
    _S1357[int(14)] = 0.0f;
    _S1357[int(15)] = 0.0f;
    _S1357[int(16)] = 0.0f;
    _S1357[int(17)] = 0.0f;
    _S1357[int(18)] = 0.0f;
    _S1357[int(19)] = 0.0f;
    _S1357[int(20)] = 0.0f;
    _S1357[int(21)] = 0.0f;
    _S1357[int(22)] = 0.0f;
    _S1357[int(23)] = 0.0f;
    _S1357[int(11)] = _S1346;
    _S1357[int(0)] = _S1175.exposure_0;
    _S1357[int(1)] = _S1356;
    _S1357[int(2)] = _S1355;
    _S1357[int(3)] = _S1354;
    _S1357[int(4)] = _S1353;
    _S1357[int(5)] = _S1352;
    _S1357[int(6)] = _S1351;
    _S1357[int(7)] = _S1350;
    _S1357[int(8)] = _S1349;
    _S1357[int(9)] = _S1348;
    _S1357[int(10)] = _S1347;
    _S1357[int(23)] = _S1341.color_params_0.n_0.y;
    _S1357[int(12)] = _S1345;
    _S1357[int(13)] = _S1344;
    _S1357[int(14)] = _S1343;
    _S1357[int(15)] = _S1342;
    _S1357[int(16)] = _S1341.color_params_0.b_0.x;
    _S1357[int(17)] = _S1341.color_params_0.b_0.y;
    _S1357[int(18)] = _S1341.color_params_0.r_0.x;
    _S1357[int(19)] = _S1341.color_params_0.r_0.y;
    _S1357[int(20)] = _S1341.color_params_0.g_0.x;
    _S1357[int(21)] = _S1341.color_params_0.g_0.y;
    _S1357[int(22)] = _S1341.color_params_0.n_0.x;
    dpparams_2->primal_0 = dpparams_2->primal_0;
    dpparams_2->differential_0 = _S1357;
    dprgb_in_2->primal_0 = (*dprgb_in_2).primal_0;
    dprgb_in_2->differential_0 = _S1338;
    return;
}

inline __device__ void s_bwd_apply_ppisp_no_crf_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1358, float2  _S1359, float2  _S1360, float2  _S1361, DiffPair_arrayx3Cfloatx2C24x3E_0 * _S1362, float3  _S1363)
{
    s_bwd_prop_apply_ppisp_no_crf_0(_S1358, _S1359, _S1360, _S1361, _S1362, _S1363);
    return;
}

inline __device__ void apply_ppisp_no_crf_vjp(float3  rgb_in_5, float2  pix_coord_8, float2  image_center_8, float2  img_size_8, FixedArray<float, 24>  params_5, float3  grad_out_2, float3  * grad_rgb_in_2, FixedArray<float, 24>  * grad_params_2)
{
    float3  _S1364 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_rgb_in_2;
    (&dp_rgb_in_2)->primal_0 = rgb_in_5;
    (&dp_rgb_in_2)->differential_0 = _S1364;
    FixedArray<float, 24>  _S1365 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    DiffPair_arrayx3Cfloatx2C24x3E_0 dp_params_2;
    (&dp_params_2)->primal_0 = params_5;
    (&dp_params_2)->differential_0 = _S1365;
    s_bwd_apply_ppisp_no_crf_0(&dp_rgb_in_2, pix_coord_8, image_center_8, img_size_8, &dp_params_2, grad_out_2);
    *grad_rgb_in_2 = dp_rgb_in_2.differential_0;
    *grad_params_2 = (&dp_params_2)->differential_0;
    return;
}

inline __device__ void compute_raw_ppisp_regularization_loss(FixedArray<float, 36>  params_6, FixedArray<float, 22>  * _S1366)
{
    PPISPParams_0 p_3;
    (&p_3)->exposure_2 = params_6[int(0)];
    (&(&p_3)->vignette_params_2[int(0)])->cx_0 = params_6[int(1)];
    (&(&p_3)->vignette_params_2[int(0)])->cy_0 = params_6[int(2)];
    (&(&p_3)->vignette_params_2[int(0)])->alpha0_0 = params_6[int(3)];
    (&(&p_3)->vignette_params_2[int(0)])->alpha1_0 = params_6[int(4)];
    (&(&p_3)->vignette_params_2[int(0)])->alpha2_0 = params_6[int(5)];
    (&(&p_3)->vignette_params_2[int(1)])->cx_0 = params_6[int(6)];
    (&(&p_3)->vignette_params_2[int(1)])->cy_0 = params_6[int(7)];
    (&(&p_3)->vignette_params_2[int(1)])->alpha0_0 = params_6[int(8)];
    (&(&p_3)->vignette_params_2[int(1)])->alpha1_0 = params_6[int(9)];
    (&(&p_3)->vignette_params_2[int(1)])->alpha2_0 = params_6[int(10)];
    (&(&p_3)->vignette_params_2[int(2)])->cx_0 = params_6[int(11)];
    (&(&p_3)->vignette_params_2[int(2)])->cy_0 = params_6[int(12)];
    (&(&p_3)->vignette_params_2[int(2)])->alpha0_0 = params_6[int(13)];
    (&(&p_3)->vignette_params_2[int(2)])->alpha1_0 = params_6[int(14)];
    (&(&p_3)->vignette_params_2[int(2)])->alpha2_0 = params_6[int(15)];
    *&((&(&(&p_3)->color_params_2)->b_0)->x) = params_6[int(16)];
    *&((&(&(&p_3)->color_params_2)->b_0)->y) = params_6[int(17)];
    *&((&(&(&p_3)->color_params_2)->r_0)->x) = params_6[int(18)];
    *&((&(&(&p_3)->color_params_2)->r_0)->y) = params_6[int(19)];
    *&((&(&(&p_3)->color_params_2)->g_0)->x) = params_6[int(20)];
    *&((&(&(&p_3)->color_params_2)->g_0)->y) = params_6[int(21)];
    *&((&(&(&p_3)->color_params_2)->n_0)->x) = params_6[int(22)];
    *&((&(&(&p_3)->color_params_2)->n_0)->y) = params_6[int(23)];
    (&(&p_3)->crf_params_1[int(0)])->toe_0 = params_6[int(24)];
    (&(&p_3)->crf_params_1[int(0)])->shoulder_0 = params_6[int(25)];
    (&(&p_3)->crf_params_1[int(0)])->gamma_0 = params_6[int(26)];
    (&(&p_3)->crf_params_1[int(0)])->center_0 = params_6[int(27)];
    (&(&p_3)->crf_params_1[int(1)])->toe_0 = params_6[int(28)];
    (&(&p_3)->crf_params_1[int(1)])->shoulder_0 = params_6[int(29)];
    (&(&p_3)->crf_params_1[int(1)])->gamma_0 = params_6[int(30)];
    (&(&p_3)->crf_params_1[int(1)])->center_0 = params_6[int(31)];
    (&(&p_3)->crf_params_1[int(2)])->toe_0 = params_6[int(32)];
    (&(&p_3)->crf_params_1[int(2)])->shoulder_0 = params_6[int(33)];
    (&(&p_3)->crf_params_1[int(2)])->gamma_0 = params_6[int(34)];
    (&(&p_3)->crf_params_1[int(2)])->center_0 = params_6[int(35)];
    FixedArray<float, 22>  losses_0;
    losses_0[int(0)] = 0.0f;
    losses_0[int(1)] = 0.0f;
    losses_0[int(2)] = 0.0f;
    losses_0[int(3)] = 0.0f;
    losses_0[int(4)] = 0.0f;
    losses_0[int(5)] = 0.0f;
    losses_0[int(6)] = 0.0f;
    losses_0[int(7)] = 0.0f;
    losses_0[int(8)] = 0.0f;
    losses_0[int(9)] = 0.0f;
    losses_0[int(10)] = 0.0f;
    losses_0[int(11)] = 0.0f;
    losses_0[int(12)] = 0.0f;
    losses_0[int(13)] = 0.0f;
    losses_0[int(14)] = 0.0f;
    losses_0[int(15)] = 0.0f;
    losses_0[int(16)] = 0.0f;
    losses_0[int(17)] = 0.0f;
    losses_0[int(18)] = 0.0f;
    losses_0[int(19)] = 0.0f;
    losses_0[int(20)] = 0.0f;
    losses_0[int(21)] = 0.0f;
    losses_0[int(0)] = p_3.exposure_2;
    float _S1367 = p_3.vignette_params_2[int(0)].cx_0;
    float _S1368 = p_3.vignette_params_2[int(0)].cy_0;
    float _S1369 = p_3.vignette_params_2[int(1)].cx_0;
    float _S1370 = p_3.vignette_params_2[int(1)].cy_0;
    float _S1371 = p_3.vignette_params_2[int(2)].cx_0;
    float _S1372 = p_3.vignette_params_2[int(2)].cy_0;
    losses_0[int(1)] = _S1367 * _S1367 + _S1368 * _S1368 + _S1369 * _S1369 + _S1370 * _S1370 + _S1371 * _S1371 + _S1372 * _S1372;
    losses_0[int(2)] = (F32_max((0.0f), (p_3.vignette_params_2[int(0)].alpha0_0))) + (F32_max((0.0f), (p_3.vignette_params_2[int(1)].alpha0_0))) + (F32_max((0.0f), (p_3.vignette_params_2[int(2)].alpha0_0)));
    losses_0[int(3)] = (F32_max((0.0f), (p_3.vignette_params_2[int(0)].alpha1_0))) + (F32_max((0.0f), (p_3.vignette_params_2[int(1)].alpha1_0))) + (F32_max((0.0f), (p_3.vignette_params_2[int(2)].alpha1_0)));
    losses_0[int(4)] = (F32_max((0.0f), (p_3.vignette_params_2[int(0)].alpha2_0))) + (F32_max((0.0f), (p_3.vignette_params_2[int(1)].alpha2_0))) + (F32_max((0.0f), (p_3.vignette_params_2[int(2)].alpha2_0)));
    float mean_0 = (p_3.vignette_params_2[int(0)].cx_0 + p_3.vignette_params_2[int(1)].cx_0 + p_3.vignette_params_2[int(2)].cx_0) / 3.0f;
    float _S1373 = p_3.vignette_params_2[int(0)].cx_0 - mean_0;
    float _S1374 = p_3.vignette_params_2[int(1)].cx_0 - mean_0;
    float _S1375 = p_3.vignette_params_2[int(2)].cx_0 - mean_0;
    losses_0[int(5)] = (_S1373 * _S1373 + _S1374 * _S1374 + _S1375 * _S1375) / 3.0f;
    float mean_1 = (p_3.vignette_params_2[int(0)].cy_0 + p_3.vignette_params_2[int(1)].cy_0 + p_3.vignette_params_2[int(2)].cy_0) / 3.0f;
    float _S1376 = p_3.vignette_params_2[int(0)].cy_0 - mean_1;
    float _S1377 = p_3.vignette_params_2[int(1)].cy_0 - mean_1;
    float _S1378 = p_3.vignette_params_2[int(2)].cy_0 - mean_1;
    losses_0[int(6)] = (_S1376 * _S1376 + _S1377 * _S1377 + _S1378 * _S1378) / 3.0f;
    float mean_2 = (p_3.vignette_params_2[int(0)].alpha0_0 + p_3.vignette_params_2[int(1)].alpha0_0 + p_3.vignette_params_2[int(2)].alpha0_0) / 3.0f;
    float _S1379 = p_3.vignette_params_2[int(0)].alpha0_0 - mean_2;
    float _S1380 = p_3.vignette_params_2[int(1)].alpha0_0 - mean_2;
    float _S1381 = p_3.vignette_params_2[int(2)].alpha0_0 - mean_2;
    losses_0[int(7)] = (_S1379 * _S1379 + _S1380 * _S1380 + _S1381 * _S1381) / 3.0f;
    float mean_3 = (p_3.vignette_params_2[int(0)].alpha1_0 + p_3.vignette_params_2[int(1)].alpha1_0 + p_3.vignette_params_2[int(2)].alpha1_0) / 3.0f;
    float _S1382 = p_3.vignette_params_2[int(0)].alpha1_0 - mean_3;
    float _S1383 = p_3.vignette_params_2[int(1)].alpha1_0 - mean_3;
    float _S1384 = p_3.vignette_params_2[int(2)].alpha1_0 - mean_3;
    losses_0[int(8)] = (_S1382 * _S1382 + _S1383 * _S1383 + _S1384 * _S1384) / 3.0f;
    float mean_4 = (p_3.vignette_params_2[int(0)].alpha2_0 + p_3.vignette_params_2[int(1)].alpha2_0 + p_3.vignette_params_2[int(2)].alpha2_0) / 3.0f;
    float _S1385 = p_3.vignette_params_2[int(0)].alpha2_0 - mean_4;
    float _S1386 = p_3.vignette_params_2[int(1)].alpha2_0 - mean_4;
    float _S1387 = p_3.vignette_params_2[int(2)].alpha2_0 - mean_4;
    losses_0[int(9)] = (_S1385 * _S1385 + _S1386 * _S1386 + _S1387 * _S1387) / 3.0f;
    float2  bd_3 = mul_0(makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f), p_3.color_params_2.b_0);
    float2  rd_3 = mul_0(makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f), p_3.color_params_2.r_0);
    float2  gd_3 = mul_0(makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f), p_3.color_params_2.g_0);
    float2  nd_3 = mul_0(makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f), p_3.color_params_2.n_0);
    losses_0[int(10)] = bd_3.x;
    losses_0[int(11)] = bd_3.y;
    losses_0[int(12)] = rd_3.x;
    losses_0[int(13)] = rd_3.y;
    losses_0[int(14)] = gd_3.x;
    losses_0[int(15)] = gd_3.y;
    losses_0[int(16)] = nd_3.x;
    losses_0[int(17)] = nd_3.y;
    float mean_5 = (p_3.crf_params_1[int(0)].toe_0 + p_3.crf_params_1[int(1)].toe_0 + p_3.crf_params_1[int(2)].toe_0) / 3.0f;
    float _S1388 = p_3.crf_params_1[int(0)].toe_0 - mean_5;
    float _S1389 = p_3.crf_params_1[int(1)].toe_0 - mean_5;
    float _S1390 = p_3.crf_params_1[int(2)].toe_0 - mean_5;
    losses_0[int(18)] = (_S1388 * _S1388 + _S1389 * _S1389 + _S1390 * _S1390) / 3.0f;
    float mean_6 = (p_3.crf_params_1[int(0)].shoulder_0 + p_3.crf_params_1[int(1)].shoulder_0 + p_3.crf_params_1[int(2)].shoulder_0) / 3.0f;
    float _S1391 = p_3.crf_params_1[int(0)].shoulder_0 - mean_6;
    float _S1392 = p_3.crf_params_1[int(1)].shoulder_0 - mean_6;
    float _S1393 = p_3.crf_params_1[int(2)].shoulder_0 - mean_6;
    losses_0[int(19)] = (_S1391 * _S1391 + _S1392 * _S1392 + _S1393 * _S1393) / 3.0f;
    float mean_7 = (p_3.crf_params_1[int(0)].gamma_0 + p_3.crf_params_1[int(1)].gamma_0 + p_3.crf_params_1[int(2)].gamma_0) / 3.0f;
    float _S1394 = p_3.crf_params_1[int(0)].gamma_0 - mean_7;
    float _S1395 = p_3.crf_params_1[int(1)].gamma_0 - mean_7;
    float _S1396 = p_3.crf_params_1[int(2)].gamma_0 - mean_7;
    losses_0[int(20)] = (_S1394 * _S1394 + _S1395 * _S1395 + _S1396 * _S1396) / 3.0f;
    float mean_8 = (p_3.crf_params_1[int(0)].center_0 + p_3.crf_params_1[int(1)].center_0 + p_3.crf_params_1[int(2)].center_0) / 3.0f;
    float _S1397 = p_3.crf_params_1[int(0)].center_0 - mean_8;
    float _S1398 = p_3.crf_params_1[int(1)].center_0 - mean_8;
    float _S1399 = p_3.crf_params_1[int(2)].center_0 - mean_8;
    losses_0[int(21)] = (_S1397 * _S1397 + _S1398 * _S1398 + _S1399 * _S1399) / 3.0f;
    *_S1366 = losses_0;
    return;
}

inline __device__ void compute_raw_ppisp_rqs_regularization_loss(FixedArray<float, 39>  params_7, FixedArray<float, 23>  * _S1400)
{
    PPISPParamsRQS_0 p_4;
    (&p_4)->exposure_1 = params_7[int(0)];
    (&(&p_4)->vignette_params_1[int(0)])->cx_0 = params_7[int(1)];
    (&(&p_4)->vignette_params_1[int(0)])->cy_0 = params_7[int(2)];
    (&(&p_4)->vignette_params_1[int(0)])->alpha0_0 = params_7[int(3)];
    (&(&p_4)->vignette_params_1[int(0)])->alpha1_0 = params_7[int(4)];
    (&(&p_4)->vignette_params_1[int(0)])->alpha2_0 = params_7[int(5)];
    (&(&p_4)->vignette_params_1[int(1)])->cx_0 = params_7[int(6)];
    (&(&p_4)->vignette_params_1[int(1)])->cy_0 = params_7[int(7)];
    (&(&p_4)->vignette_params_1[int(1)])->alpha0_0 = params_7[int(8)];
    (&(&p_4)->vignette_params_1[int(1)])->alpha1_0 = params_7[int(9)];
    (&(&p_4)->vignette_params_1[int(1)])->alpha2_0 = params_7[int(10)];
    (&(&p_4)->vignette_params_1[int(2)])->cx_0 = params_7[int(11)];
    (&(&p_4)->vignette_params_1[int(2)])->cy_0 = params_7[int(12)];
    (&(&p_4)->vignette_params_1[int(2)])->alpha0_0 = params_7[int(13)];
    (&(&p_4)->vignette_params_1[int(2)])->alpha1_0 = params_7[int(14)];
    (&(&p_4)->vignette_params_1[int(2)])->alpha2_0 = params_7[int(15)];
    *&((&(&(&p_4)->color_params_1)->b_0)->x) = params_7[int(16)];
    *&((&(&(&p_4)->color_params_1)->b_0)->y) = params_7[int(17)];
    *&((&(&(&p_4)->color_params_1)->r_0)->x) = params_7[int(18)];
    *&((&(&(&p_4)->color_params_1)->r_0)->y) = params_7[int(19)];
    *&((&(&(&p_4)->color_params_1)->g_0)->x) = params_7[int(20)];
    *&((&(&(&p_4)->color_params_1)->g_0)->y) = params_7[int(21)];
    *&((&(&(&p_4)->color_params_1)->n_0)->x) = params_7[int(22)];
    *&((&(&(&p_4)->color_params_1)->n_0)->y) = params_7[int(23)];
    (&(&p_4)->crf_params_0[int(0)])->g0_0 = params_7[int(24)];
    (&(&p_4)->crf_params_0[int(0)])->g1_0 = params_7[int(25)];
    (&(&p_4)->crf_params_0[int(0)])->x0_0 = params_7[int(26)];
    (&(&p_4)->crf_params_0[int(0)])->y0_0 = params_7[int(27)];
    (&(&p_4)->crf_params_0[int(0)])->gc_0 = params_7[int(28)];
    (&(&p_4)->crf_params_0[int(1)])->g0_0 = params_7[int(29)];
    (&(&p_4)->crf_params_0[int(1)])->g1_0 = params_7[int(30)];
    (&(&p_4)->crf_params_0[int(1)])->x0_0 = params_7[int(31)];
    (&(&p_4)->crf_params_0[int(1)])->y0_0 = params_7[int(32)];
    (&(&p_4)->crf_params_0[int(1)])->gc_0 = params_7[int(33)];
    (&(&p_4)->crf_params_0[int(2)])->g0_0 = params_7[int(34)];
    (&(&p_4)->crf_params_0[int(2)])->g1_0 = params_7[int(35)];
    (&(&p_4)->crf_params_0[int(2)])->x0_0 = params_7[int(36)];
    (&(&p_4)->crf_params_0[int(2)])->y0_0 = params_7[int(37)];
    (&(&p_4)->crf_params_0[int(2)])->gc_0 = params_7[int(38)];
    FixedArray<float, 23>  losses_1;
    losses_1[int(0)] = 0.0f;
    losses_1[int(1)] = 0.0f;
    losses_1[int(2)] = 0.0f;
    losses_1[int(3)] = 0.0f;
    losses_1[int(4)] = 0.0f;
    losses_1[int(5)] = 0.0f;
    losses_1[int(6)] = 0.0f;
    losses_1[int(7)] = 0.0f;
    losses_1[int(8)] = 0.0f;
    losses_1[int(9)] = 0.0f;
    losses_1[int(10)] = 0.0f;
    losses_1[int(11)] = 0.0f;
    losses_1[int(12)] = 0.0f;
    losses_1[int(13)] = 0.0f;
    losses_1[int(14)] = 0.0f;
    losses_1[int(15)] = 0.0f;
    losses_1[int(16)] = 0.0f;
    losses_1[int(17)] = 0.0f;
    losses_1[int(18)] = 0.0f;
    losses_1[int(19)] = 0.0f;
    losses_1[int(20)] = 0.0f;
    losses_1[int(21)] = 0.0f;
    losses_1[int(22)] = 0.0f;
    losses_1[int(0)] = p_4.exposure_1;
    float _S1401 = p_4.vignette_params_1[int(0)].cx_0;
    float _S1402 = p_4.vignette_params_1[int(0)].cy_0;
    float _S1403 = p_4.vignette_params_1[int(1)].cx_0;
    float _S1404 = p_4.vignette_params_1[int(1)].cy_0;
    float _S1405 = p_4.vignette_params_1[int(2)].cx_0;
    float _S1406 = p_4.vignette_params_1[int(2)].cy_0;
    losses_1[int(1)] = _S1401 * _S1401 + _S1402 * _S1402 + _S1403 * _S1403 + _S1404 * _S1404 + _S1405 * _S1405 + _S1406 * _S1406;
    losses_1[int(2)] = (F32_max((0.0f), (p_4.vignette_params_1[int(0)].alpha0_0))) + (F32_max((0.0f), (p_4.vignette_params_1[int(1)].alpha0_0))) + (F32_max((0.0f), (p_4.vignette_params_1[int(2)].alpha0_0)));
    losses_1[int(3)] = (F32_max((0.0f), (p_4.vignette_params_1[int(0)].alpha1_0))) + (F32_max((0.0f), (p_4.vignette_params_1[int(1)].alpha1_0))) + (F32_max((0.0f), (p_4.vignette_params_1[int(2)].alpha1_0)));
    losses_1[int(4)] = (F32_max((0.0f), (p_4.vignette_params_1[int(0)].alpha2_0))) + (F32_max((0.0f), (p_4.vignette_params_1[int(1)].alpha2_0))) + (F32_max((0.0f), (p_4.vignette_params_1[int(2)].alpha2_0)));
    float mean_9 = (p_4.vignette_params_1[int(0)].cx_0 + p_4.vignette_params_1[int(1)].cx_0 + p_4.vignette_params_1[int(2)].cx_0) / 3.0f;
    float _S1407 = p_4.vignette_params_1[int(0)].cx_0 - mean_9;
    float _S1408 = p_4.vignette_params_1[int(1)].cx_0 - mean_9;
    float _S1409 = p_4.vignette_params_1[int(2)].cx_0 - mean_9;
    losses_1[int(5)] = (_S1407 * _S1407 + _S1408 * _S1408 + _S1409 * _S1409) / 3.0f;
    float mean_10 = (p_4.vignette_params_1[int(0)].cy_0 + p_4.vignette_params_1[int(1)].cy_0 + p_4.vignette_params_1[int(2)].cy_0) / 3.0f;
    float _S1410 = p_4.vignette_params_1[int(0)].cy_0 - mean_10;
    float _S1411 = p_4.vignette_params_1[int(1)].cy_0 - mean_10;
    float _S1412 = p_4.vignette_params_1[int(2)].cy_0 - mean_10;
    losses_1[int(6)] = (_S1410 * _S1410 + _S1411 * _S1411 + _S1412 * _S1412) / 3.0f;
    float mean_11 = (p_4.vignette_params_1[int(0)].alpha0_0 + p_4.vignette_params_1[int(1)].alpha0_0 + p_4.vignette_params_1[int(2)].alpha0_0) / 3.0f;
    float _S1413 = p_4.vignette_params_1[int(0)].alpha0_0 - mean_11;
    float _S1414 = p_4.vignette_params_1[int(1)].alpha0_0 - mean_11;
    float _S1415 = p_4.vignette_params_1[int(2)].alpha0_0 - mean_11;
    losses_1[int(7)] = (_S1413 * _S1413 + _S1414 * _S1414 + _S1415 * _S1415) / 3.0f;
    float mean_12 = (p_4.vignette_params_1[int(0)].alpha1_0 + p_4.vignette_params_1[int(1)].alpha1_0 + p_4.vignette_params_1[int(2)].alpha1_0) / 3.0f;
    float _S1416 = p_4.vignette_params_1[int(0)].alpha1_0 - mean_12;
    float _S1417 = p_4.vignette_params_1[int(1)].alpha1_0 - mean_12;
    float _S1418 = p_4.vignette_params_1[int(2)].alpha1_0 - mean_12;
    losses_1[int(8)] = (_S1416 * _S1416 + _S1417 * _S1417 + _S1418 * _S1418) / 3.0f;
    float mean_13 = (p_4.vignette_params_1[int(0)].alpha2_0 + p_4.vignette_params_1[int(1)].alpha2_0 + p_4.vignette_params_1[int(2)].alpha2_0) / 3.0f;
    float _S1419 = p_4.vignette_params_1[int(0)].alpha2_0 - mean_13;
    float _S1420 = p_4.vignette_params_1[int(1)].alpha2_0 - mean_13;
    float _S1421 = p_4.vignette_params_1[int(2)].alpha2_0 - mean_13;
    losses_1[int(9)] = (_S1419 * _S1419 + _S1420 * _S1420 + _S1421 * _S1421) / 3.0f;
    float2  bd_4 = mul_0(makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f), p_4.color_params_1.b_0);
    float2  rd_4 = mul_0(makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f), p_4.color_params_1.r_0);
    float2  gd_4 = mul_0(makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f), p_4.color_params_1.g_0);
    float2  nd_4 = mul_0(makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f), p_4.color_params_1.n_0);
    losses_1[int(10)] = bd_4.x;
    losses_1[int(11)] = bd_4.y;
    losses_1[int(12)] = rd_4.x;
    losses_1[int(13)] = rd_4.y;
    losses_1[int(14)] = gd_4.x;
    losses_1[int(15)] = gd_4.y;
    losses_1[int(16)] = nd_4.x;
    losses_1[int(17)] = nd_4.y;
    float mean_14 = (p_4.crf_params_0[int(0)].g0_0 + p_4.crf_params_0[int(1)].g0_0 + p_4.crf_params_0[int(2)].g0_0) / 3.0f;
    float _S1422 = p_4.crf_params_0[int(0)].g0_0 - mean_14;
    float _S1423 = p_4.crf_params_0[int(1)].g0_0 - mean_14;
    float _S1424 = p_4.crf_params_0[int(2)].g0_0 - mean_14;
    losses_1[int(18)] = (_S1422 * _S1422 + _S1423 * _S1423 + _S1424 * _S1424) / 3.0f;
    float mean_15 = (p_4.crf_params_0[int(0)].g1_0 + p_4.crf_params_0[int(1)].g1_0 + p_4.crf_params_0[int(2)].g1_0) / 3.0f;
    float _S1425 = p_4.crf_params_0[int(0)].g1_0 - mean_15;
    float _S1426 = p_4.crf_params_0[int(1)].g1_0 - mean_15;
    float _S1427 = p_4.crf_params_0[int(2)].g1_0 - mean_15;
    losses_1[int(19)] = (_S1425 * _S1425 + _S1426 * _S1426 + _S1427 * _S1427) / 3.0f;
    float mean_16 = (p_4.crf_params_0[int(0)].x0_0 + p_4.crf_params_0[int(1)].x0_0 + p_4.crf_params_0[int(2)].x0_0) / 3.0f;
    float _S1428 = p_4.crf_params_0[int(0)].x0_0 - mean_16;
    float _S1429 = p_4.crf_params_0[int(1)].x0_0 - mean_16;
    float _S1430 = p_4.crf_params_0[int(2)].x0_0 - mean_16;
    losses_1[int(20)] = (_S1428 * _S1428 + _S1429 * _S1429 + _S1430 * _S1430) / 3.0f;
    float mean_17 = (p_4.crf_params_0[int(0)].y0_0 + p_4.crf_params_0[int(1)].y0_0 + p_4.crf_params_0[int(2)].y0_0) / 3.0f;
    float _S1431 = p_4.crf_params_0[int(0)].y0_0 - mean_17;
    float _S1432 = p_4.crf_params_0[int(1)].y0_0 - mean_17;
    float _S1433 = p_4.crf_params_0[int(2)].y0_0 - mean_17;
    losses_1[int(21)] = (_S1431 * _S1431 + _S1432 * _S1432 + _S1433 * _S1433) / 3.0f;
    float mean_18 = (p_4.crf_params_0[int(0)].gc_0 + p_4.crf_params_0[int(1)].gc_0 + p_4.crf_params_0[int(2)].gc_0) / 3.0f;
    float _S1434 = p_4.crf_params_0[int(0)].gc_0 - mean_18;
    float _S1435 = p_4.crf_params_0[int(1)].gc_0 - mean_18;
    float _S1436 = p_4.crf_params_0[int(2)].gc_0 - mean_18;
    losses_1[int(22)] = (_S1434 * _S1434 + _S1435 * _S1435 + _S1436 * _S1436) / 3.0f;
    *_S1400 = losses_1;
    return;
}

inline __device__ void s_bwd_prop_compute_raw_ppisp_regularization_loss_0(DiffPair_arrayx3Cfloatx2C36x3E_0 * dpparams_3, FixedArray<float, 22>  * _s_dOut_3)
{
    VignettingChannelParams_0 _S1437 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    FixedArray<VignettingChannelParams_0, 3>  _S1438 = {
        _S1437, _S1437, _S1437
    };
    float2  _S1439 = make_float2 (0.0f);
    ColorPPISPParams_0 _S1440 = { _S1439, _S1439, _S1439, _S1439 };
    CRFPPISPChannelParams_0 _S1441 = { 0.0f, 0.0f, 0.0f, 0.0f };
    FixedArray<CRFPPISPChannelParams_0, 3>  _S1442 = {
        _S1441, _S1441, _S1441
    };
    PPISPParams_0 _S1443;
    (&_S1443)->exposure_2 = dpparams_3->primal_0[int(0)];
    (&_S1443)->vignette_params_2 = _S1438;
    (&_S1443)->color_params_2 = _S1440;
    (&_S1443)->crf_params_1 = _S1442;
    (&(&_S1443)->vignette_params_2[int(0)])->cx_0 = dpparams_3->primal_0[int(1)];
    (&(&_S1443)->vignette_params_2[int(0)])->cy_0 = dpparams_3->primal_0[int(2)];
    (&(&_S1443)->vignette_params_2[int(0)])->alpha0_0 = dpparams_3->primal_0[int(3)];
    (&(&_S1443)->vignette_params_2[int(0)])->alpha1_0 = dpparams_3->primal_0[int(4)];
    (&(&_S1443)->vignette_params_2[int(0)])->alpha2_0 = dpparams_3->primal_0[int(5)];
    (&(&_S1443)->vignette_params_2[int(1)])->cx_0 = dpparams_3->primal_0[int(6)];
    (&(&_S1443)->vignette_params_2[int(1)])->cy_0 = dpparams_3->primal_0[int(7)];
    (&(&_S1443)->vignette_params_2[int(1)])->alpha0_0 = dpparams_3->primal_0[int(8)];
    (&(&_S1443)->vignette_params_2[int(1)])->alpha1_0 = dpparams_3->primal_0[int(9)];
    (&(&_S1443)->vignette_params_2[int(1)])->alpha2_0 = dpparams_3->primal_0[int(10)];
    (&(&_S1443)->vignette_params_2[int(2)])->cx_0 = dpparams_3->primal_0[int(11)];
    (&(&_S1443)->vignette_params_2[int(2)])->cy_0 = dpparams_3->primal_0[int(12)];
    (&(&_S1443)->vignette_params_2[int(2)])->alpha0_0 = dpparams_3->primal_0[int(13)];
    (&(&_S1443)->vignette_params_2[int(2)])->alpha1_0 = dpparams_3->primal_0[int(14)];
    (&(&_S1443)->vignette_params_2[int(2)])->alpha2_0 = dpparams_3->primal_0[int(15)];
    *&((&(&(&_S1443)->color_params_2)->b_0)->x) = dpparams_3->primal_0[int(16)];
    *&((&(&(&_S1443)->color_params_2)->b_0)->y) = dpparams_3->primal_0[int(17)];
    *&((&(&(&_S1443)->color_params_2)->r_0)->x) = dpparams_3->primal_0[int(18)];
    *&((&(&(&_S1443)->color_params_2)->r_0)->y) = dpparams_3->primal_0[int(19)];
    *&((&(&(&_S1443)->color_params_2)->g_0)->x) = dpparams_3->primal_0[int(20)];
    *&((&(&(&_S1443)->color_params_2)->g_0)->y) = dpparams_3->primal_0[int(21)];
    *&((&(&(&_S1443)->color_params_2)->n_0)->x) = dpparams_3->primal_0[int(22)];
    *&((&(&(&_S1443)->color_params_2)->n_0)->y) = dpparams_3->primal_0[int(23)];
    (&(&_S1443)->crf_params_1[int(0)])->toe_0 = dpparams_3->primal_0[int(24)];
    (&(&_S1443)->crf_params_1[int(0)])->shoulder_0 = dpparams_3->primal_0[int(25)];
    (&(&_S1443)->crf_params_1[int(0)])->gamma_0 = dpparams_3->primal_0[int(26)];
    (&(&_S1443)->crf_params_1[int(0)])->center_0 = dpparams_3->primal_0[int(27)];
    (&(&_S1443)->crf_params_1[int(1)])->toe_0 = dpparams_3->primal_0[int(28)];
    (&(&_S1443)->crf_params_1[int(1)])->shoulder_0 = dpparams_3->primal_0[int(29)];
    (&(&_S1443)->crf_params_1[int(1)])->gamma_0 = dpparams_3->primal_0[int(30)];
    (&(&_S1443)->crf_params_1[int(1)])->center_0 = dpparams_3->primal_0[int(31)];
    (&(&_S1443)->crf_params_1[int(2)])->toe_0 = dpparams_3->primal_0[int(32)];
    (&(&_S1443)->crf_params_1[int(2)])->shoulder_0 = dpparams_3->primal_0[int(33)];
    (&(&_S1443)->crf_params_1[int(2)])->gamma_0 = dpparams_3->primal_0[int(34)];
    (&(&_S1443)->crf_params_1[int(2)])->center_0 = dpparams_3->primal_0[int(35)];
    float mean_19 = (dpparams_3->primal_0[int(1)] + dpparams_3->primal_0[int(6)] + dpparams_3->primal_0[int(11)]) / 3.0f;
    float _S1444 = dpparams_3->primal_0[int(1)] - mean_19;
    float _S1445 = dpparams_3->primal_0[int(6)] - mean_19;
    float _S1446 = dpparams_3->primal_0[int(11)] - mean_19;
    float mean_20 = (dpparams_3->primal_0[int(2)] + dpparams_3->primal_0[int(7)] + dpparams_3->primal_0[int(12)]) / 3.0f;
    float _S1447 = dpparams_3->primal_0[int(2)] - mean_20;
    float _S1448 = dpparams_3->primal_0[int(7)] - mean_20;
    float _S1449 = dpparams_3->primal_0[int(12)] - mean_20;
    float mean_21 = (dpparams_3->primal_0[int(3)] + dpparams_3->primal_0[int(8)] + dpparams_3->primal_0[int(13)]) / 3.0f;
    float _S1450 = dpparams_3->primal_0[int(3)] - mean_21;
    float _S1451 = dpparams_3->primal_0[int(8)] - mean_21;
    float _S1452 = dpparams_3->primal_0[int(13)] - mean_21;
    float mean_22 = (dpparams_3->primal_0[int(4)] + dpparams_3->primal_0[int(9)] + dpparams_3->primal_0[int(14)]) / 3.0f;
    float _S1453 = dpparams_3->primal_0[int(4)] - mean_22;
    float _S1454 = dpparams_3->primal_0[int(9)] - mean_22;
    float _S1455 = dpparams_3->primal_0[int(14)] - mean_22;
    float mean_23 = (dpparams_3->primal_0[int(5)] + dpparams_3->primal_0[int(10)] + dpparams_3->primal_0[int(15)]) / 3.0f;
    float _S1456 = dpparams_3->primal_0[int(5)] - mean_23;
    float _S1457 = dpparams_3->primal_0[int(10)] - mean_23;
    float _S1458 = dpparams_3->primal_0[int(15)] - mean_23;
    float mean_24 = (dpparams_3->primal_0[int(24)] + dpparams_3->primal_0[int(28)] + dpparams_3->primal_0[int(32)]) / 3.0f;
    float mean_25 = (dpparams_3->primal_0[int(25)] + dpparams_3->primal_0[int(29)] + dpparams_3->primal_0[int(33)]) / 3.0f;
    float mean_26 = (dpparams_3->primal_0[int(26)] + dpparams_3->primal_0[int(30)] + dpparams_3->primal_0[int(34)]) / 3.0f;
    float mean_27 = (dpparams_3->primal_0[int(27)] + dpparams_3->primal_0[int(31)] + dpparams_3->primal_0[int(35)]) / 3.0f;
    float _S1459 = 0.3333333432674408f * (*_s_dOut_3)[int(21)];
    float _S1460 = (dpparams_3->primal_0[int(35)] - mean_27) * _S1459;
    float _S1461 = _S1460 + _S1460;
    float _S1462 = (dpparams_3->primal_0[int(31)] - mean_27) * _S1459;
    float _S1463 = _S1462 + _S1462;
    float _S1464 = (dpparams_3->primal_0[int(27)] - mean_27) * _S1459;
    float _S1465 = _S1464 + _S1464;
    float _S1466 = 0.3333333432674408f * (- _S1461 + - _S1463 + - _S1465);
    float _S1467 = 0.3333333432674408f * (*_s_dOut_3)[int(20)];
    float _S1468 = (dpparams_3->primal_0[int(34)] - mean_26) * _S1467;
    float _S1469 = _S1468 + _S1468;
    float _S1470 = (dpparams_3->primal_0[int(30)] - mean_26) * _S1467;
    float _S1471 = _S1470 + _S1470;
    float _S1472 = (dpparams_3->primal_0[int(26)] - mean_26) * _S1467;
    float _S1473 = _S1472 + _S1472;
    float _S1474 = 0.3333333432674408f * (- _S1469 + - _S1471 + - _S1473);
    float _S1475 = 0.3333333432674408f * (*_s_dOut_3)[int(19)];
    float _S1476 = (dpparams_3->primal_0[int(33)] - mean_25) * _S1475;
    float _S1477 = _S1476 + _S1476;
    float _S1478 = (dpparams_3->primal_0[int(29)] - mean_25) * _S1475;
    float _S1479 = _S1478 + _S1478;
    float _S1480 = (dpparams_3->primal_0[int(25)] - mean_25) * _S1475;
    float _S1481 = _S1480 + _S1480;
    float _S1482 = 0.3333333432674408f * (- _S1477 + - _S1479 + - _S1481);
    float _S1483 = 0.3333333432674408f * (*_s_dOut_3)[int(18)];
    float _S1484 = (dpparams_3->primal_0[int(32)] - mean_24) * _S1483;
    float _S1485 = _S1484 + _S1484;
    float _S1486 = (dpparams_3->primal_0[int(28)] - mean_24) * _S1483;
    float _S1487 = _S1486 + _S1486;
    float _S1488 = (dpparams_3->primal_0[int(24)] - mean_24) * _S1483;
    float _S1489 = _S1488 + _S1488;
    float _S1490 = 0.3333333432674408f * (- _S1485 + - _S1487 + - _S1489);
    float2  _S1491 = make_float2 ((*_s_dOut_3)[int(16)], (*_s_dOut_3)[int(17)]);
    Matrix<float, 2, 2>  _S1492 = makeMatrix<float, 2, 2> (0.0f);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1493;
    (&_S1493)->primal_0 = makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f);
    (&_S1493)->differential_0 = _S1492;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1494;
    (&_S1494)->primal_0 = _S1443.color_params_2.n_0;
    (&_S1494)->differential_0 = _S1439;
    s_bwd_prop_mul_2(&_S1493, &_S1494, _S1491);
    float2  _S1495 = make_float2 ((*_s_dOut_3)[int(14)], (*_s_dOut_3)[int(15)]);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1496;
    (&_S1496)->primal_0 = makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f);
    (&_S1496)->differential_0 = _S1492;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1497;
    (&_S1497)->primal_0 = _S1443.color_params_2.g_0;
    (&_S1497)->differential_0 = _S1439;
    s_bwd_prop_mul_2(&_S1496, &_S1497, _S1495);
    float2  _S1498 = make_float2 ((*_s_dOut_3)[int(12)], (*_s_dOut_3)[int(13)]);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1499;
    (&_S1499)->primal_0 = makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f);
    (&_S1499)->differential_0 = _S1492;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1500;
    (&_S1500)->primal_0 = _S1443.color_params_2.r_0;
    (&_S1500)->differential_0 = _S1439;
    s_bwd_prop_mul_2(&_S1499, &_S1500, _S1498);
    float2  _S1501 = make_float2 ((*_s_dOut_3)[int(10)], (*_s_dOut_3)[int(11)]);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1502;
    (&_S1502)->primal_0 = makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f);
    (&_S1502)->differential_0 = _S1492;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1503;
    (&_S1503)->primal_0 = _S1443.color_params_2.b_0;
    (&_S1503)->differential_0 = _S1439;
    s_bwd_prop_mul_2(&_S1502, &_S1503, _S1501);
    ColorPPISPParams_0 _S1504 = ColorPPISPParams_x24_syn_dzero_0();
    (&_S1504)->n_0 = _S1494.differential_0;
    (&_S1504)->g_0 = _S1497.differential_0;
    (&_S1504)->r_0 = _S1500.differential_0;
    (&_S1504)->b_0 = _S1503.differential_0;
    float _S1505 = 0.3333333432674408f * (*_s_dOut_3)[int(9)];
    float _S1506 = _S1458 * _S1505;
    float _S1507 = _S1506 + _S1506;
    float _S1508 = _S1457 * _S1505;
    float _S1509 = _S1508 + _S1508;
    float _S1510 = _S1456 * _S1505;
    float _S1511 = _S1510 + _S1510;
    float _S1512 = 0.3333333432674408f * (- _S1507 + - _S1509 + - _S1511);
    float _S1513 = 0.3333333432674408f * (*_s_dOut_3)[int(8)];
    float _S1514 = _S1455 * _S1513;
    float _S1515 = _S1514 + _S1514;
    float _S1516 = _S1454 * _S1513;
    float _S1517 = _S1516 + _S1516;
    float _S1518 = _S1453 * _S1513;
    float _S1519 = _S1518 + _S1518;
    float _S1520 = 0.3333333432674408f * (- _S1515 + - _S1517 + - _S1519);
    float _S1521 = 0.3333333432674408f * (*_s_dOut_3)[int(7)];
    float _S1522 = _S1452 * _S1521;
    float _S1523 = _S1522 + _S1522;
    float _S1524 = _S1451 * _S1521;
    float _S1525 = _S1524 + _S1524;
    float _S1526 = _S1450 * _S1521;
    float _S1527 = _S1526 + _S1526;
    float _S1528 = 0.3333333432674408f * (- _S1523 + - _S1525 + - _S1527);
    float _S1529 = 0.3333333432674408f * (*_s_dOut_3)[int(6)];
    float _S1530 = _S1449 * _S1529;
    float _S1531 = _S1530 + _S1530;
    float _S1532 = _S1448 * _S1529;
    float _S1533 = _S1532 + _S1532;
    float _S1534 = _S1447 * _S1529;
    float _S1535 = _S1534 + _S1534;
    float _S1536 = 0.3333333432674408f * (- _S1531 + - _S1533 + - _S1535);
    float _S1537 = 0.3333333432674408f * (*_s_dOut_3)[int(5)];
    float _S1538 = _S1446 * _S1537;
    float _S1539 = _S1538 + _S1538;
    float _S1540 = _S1445 * _S1537;
    float _S1541 = _S1540 + _S1540;
    float _S1542 = _S1444 * _S1537;
    float _S1543 = _S1542 + _S1542;
    float _S1544 = 0.3333333432674408f * (- _S1539 + - _S1541 + - _S1543);
    DiffPair_float_0 _S1545;
    (&_S1545)->primal_0 = 0.0f;
    (&_S1545)->differential_0 = 0.0f;
    DiffPair_float_0 _S1546;
    (&_S1546)->primal_0 = dpparams_3->primal_0[int(15)];
    (&_S1546)->differential_0 = 0.0f;
    _d_max_0(&_S1545, &_S1546, (*_s_dOut_3)[int(4)]);
    DiffPair_float_0 _S1547;
    (&_S1547)->primal_0 = 0.0f;
    (&_S1547)->differential_0 = 0.0f;
    DiffPair_float_0 _S1548;
    (&_S1548)->primal_0 = dpparams_3->primal_0[int(10)];
    (&_S1548)->differential_0 = 0.0f;
    _d_max_0(&_S1547, &_S1548, (*_s_dOut_3)[int(4)]);
    DiffPair_float_0 _S1549;
    (&_S1549)->primal_0 = 0.0f;
    (&_S1549)->differential_0 = 0.0f;
    DiffPair_float_0 _S1550;
    (&_S1550)->primal_0 = dpparams_3->primal_0[int(5)];
    (&_S1550)->differential_0 = 0.0f;
    _d_max_0(&_S1549, &_S1550, (*_s_dOut_3)[int(4)]);
    DiffPair_float_0 _S1551;
    (&_S1551)->primal_0 = 0.0f;
    (&_S1551)->differential_0 = 0.0f;
    DiffPair_float_0 _S1552;
    (&_S1552)->primal_0 = dpparams_3->primal_0[int(14)];
    (&_S1552)->differential_0 = 0.0f;
    _d_max_0(&_S1551, &_S1552, (*_s_dOut_3)[int(3)]);
    DiffPair_float_0 _S1553;
    (&_S1553)->primal_0 = 0.0f;
    (&_S1553)->differential_0 = 0.0f;
    DiffPair_float_0 _S1554;
    (&_S1554)->primal_0 = dpparams_3->primal_0[int(9)];
    (&_S1554)->differential_0 = 0.0f;
    _d_max_0(&_S1553, &_S1554, (*_s_dOut_3)[int(3)]);
    DiffPair_float_0 _S1555;
    (&_S1555)->primal_0 = 0.0f;
    (&_S1555)->differential_0 = 0.0f;
    DiffPair_float_0 _S1556;
    (&_S1556)->primal_0 = dpparams_3->primal_0[int(4)];
    (&_S1556)->differential_0 = 0.0f;
    _d_max_0(&_S1555, &_S1556, (*_s_dOut_3)[int(3)]);
    DiffPair_float_0 _S1557;
    (&_S1557)->primal_0 = 0.0f;
    (&_S1557)->differential_0 = 0.0f;
    DiffPair_float_0 _S1558;
    (&_S1558)->primal_0 = dpparams_3->primal_0[int(13)];
    (&_S1558)->differential_0 = 0.0f;
    _d_max_0(&_S1557, &_S1558, (*_s_dOut_3)[int(2)]);
    DiffPair_float_0 _S1559;
    (&_S1559)->primal_0 = 0.0f;
    (&_S1559)->differential_0 = 0.0f;
    DiffPair_float_0 _S1560;
    (&_S1560)->primal_0 = dpparams_3->primal_0[int(8)];
    (&_S1560)->differential_0 = 0.0f;
    _d_max_0(&_S1559, &_S1560, (*_s_dOut_3)[int(2)]);
    DiffPair_float_0 _S1561;
    (&_S1561)->primal_0 = 0.0f;
    (&_S1561)->differential_0 = 0.0f;
    DiffPair_float_0 _S1562;
    (&_S1562)->primal_0 = dpparams_3->primal_0[int(3)];
    (&_S1562)->differential_0 = 0.0f;
    _d_max_0(&_S1561, &_S1562, (*_s_dOut_3)[int(2)]);
    float _S1563 = dpparams_3->primal_0[int(12)] * (*_s_dOut_3)[int(1)];
    float _S1564 = dpparams_3->primal_0[int(11)] * (*_s_dOut_3)[int(1)];
    float _S1565 = dpparams_3->primal_0[int(7)] * (*_s_dOut_3)[int(1)];
    float _S1566 = dpparams_3->primal_0[int(6)] * (*_s_dOut_3)[int(1)];
    float _S1567 = dpparams_3->primal_0[int(2)] * (*_s_dOut_3)[int(1)];
    float _S1568 = dpparams_3->primal_0[int(1)] * (*_s_dOut_3)[int(1)];
    PPISPParams_0 _S1569 = PPISPParams_x24_syn_dzero_0();
    (&_S1569)->color_params_2 = _S1504;
    (&_S1569)->exposure_2 = (*_s_dOut_3)[int(0)];
    _S1443 = _S1569;
    (&(&_S1443)->crf_params_1[int(2)])->center_0 = 0.0f;
    float _S1570 = _S1461 + _S1466 + _S1569.crf_params_1[int(2)].center_0;
    (&(&_S1443)->crf_params_1[int(2)])->gamma_0 = 0.0f;
    float _S1571 = _S1469 + _S1474 + _S1569.crf_params_1[int(2)].gamma_0;
    (&(&_S1443)->crf_params_1[int(2)])->shoulder_0 = 0.0f;
    float _S1572 = _S1477 + _S1482 + _S1569.crf_params_1[int(2)].shoulder_0;
    (&(&_S1443)->crf_params_1[int(2)])->toe_0 = 0.0f;
    float _S1573 = _S1485 + _S1490 + _S1569.crf_params_1[int(2)].toe_0;
    (&(&_S1443)->crf_params_1[int(1)])->center_0 = 0.0f;
    float _S1574 = _S1463 + _S1466 + _S1569.crf_params_1[int(1)].center_0;
    (&(&_S1443)->crf_params_1[int(1)])->gamma_0 = 0.0f;
    float _S1575 = _S1471 + _S1474 + _S1569.crf_params_1[int(1)].gamma_0;
    (&(&_S1443)->crf_params_1[int(1)])->shoulder_0 = 0.0f;
    float _S1576 = _S1479 + _S1482 + _S1569.crf_params_1[int(1)].shoulder_0;
    (&(&_S1443)->crf_params_1[int(1)])->toe_0 = 0.0f;
    float _S1577 = _S1487 + _S1490 + _S1569.crf_params_1[int(1)].toe_0;
    (&(&_S1443)->crf_params_1[int(0)])->center_0 = 0.0f;
    float _S1578 = _S1465 + _S1466 + _S1569.crf_params_1[int(0)].center_0;
    (&(&_S1443)->crf_params_1[int(0)])->gamma_0 = 0.0f;
    float _S1579 = _S1473 + _S1474 + _S1569.crf_params_1[int(0)].gamma_0;
    (&(&_S1443)->crf_params_1[int(0)])->shoulder_0 = 0.0f;
    float _S1580 = _S1481 + _S1482 + _S1569.crf_params_1[int(0)].shoulder_0;
    (&(&_S1443)->crf_params_1[int(0)])->toe_0 = 0.0f;
    float _S1581 = _S1489 + _S1490 + _S1569.crf_params_1[int(0)].toe_0;
    *&((&(&(&_S1443)->color_params_2)->n_0)->y) = 0.0f;
    *&((&(&(&_S1443)->color_params_2)->n_0)->x) = 0.0f;
    *&((&(&(&_S1443)->color_params_2)->g_0)->y) = 0.0f;
    *&((&(&(&_S1443)->color_params_2)->g_0)->x) = 0.0f;
    *&((&(&(&_S1443)->color_params_2)->r_0)->y) = 0.0f;
    *&((&(&(&_S1443)->color_params_2)->r_0)->x) = 0.0f;
    *&((&(&(&_S1443)->color_params_2)->b_0)->y) = 0.0f;
    *&((&(&(&_S1443)->color_params_2)->b_0)->x) = 0.0f;
    (&(&_S1443)->vignette_params_2[int(2)])->alpha2_0 = 0.0f;
    float _S1582 = _S1507 + _S1512 + _S1546.differential_0 + _S1569.vignette_params_2[int(2)].alpha2_0;
    (&(&_S1443)->vignette_params_2[int(2)])->alpha1_0 = 0.0f;
    float _S1583 = _S1515 + _S1520 + _S1552.differential_0 + _S1569.vignette_params_2[int(2)].alpha1_0;
    (&(&_S1443)->vignette_params_2[int(2)])->alpha0_0 = 0.0f;
    float _S1584 = _S1523 + _S1528 + _S1558.differential_0 + _S1569.vignette_params_2[int(2)].alpha0_0;
    (&(&_S1443)->vignette_params_2[int(2)])->cy_0 = 0.0f;
    float _S1585 = _S1531 + _S1536 + _S1563 + _S1563 + _S1569.vignette_params_2[int(2)].cy_0;
    (&(&_S1443)->vignette_params_2[int(2)])->cx_0 = 0.0f;
    float _S1586 = _S1539 + _S1544 + _S1564 + _S1564 + _S1569.vignette_params_2[int(2)].cx_0;
    (&(&_S1443)->vignette_params_2[int(1)])->alpha2_0 = 0.0f;
    float _S1587 = _S1509 + _S1512 + _S1548.differential_0 + _S1569.vignette_params_2[int(1)].alpha2_0;
    (&(&_S1443)->vignette_params_2[int(1)])->alpha1_0 = 0.0f;
    float _S1588 = _S1517 + _S1520 + _S1554.differential_0 + _S1569.vignette_params_2[int(1)].alpha1_0;
    (&(&_S1443)->vignette_params_2[int(1)])->alpha0_0 = 0.0f;
    float _S1589 = _S1525 + _S1528 + _S1560.differential_0 + _S1569.vignette_params_2[int(1)].alpha0_0;
    (&(&_S1443)->vignette_params_2[int(1)])->cy_0 = 0.0f;
    float _S1590 = _S1533 + _S1536 + _S1565 + _S1565 + _S1569.vignette_params_2[int(1)].cy_0;
    (&(&_S1443)->vignette_params_2[int(1)])->cx_0 = 0.0f;
    float _S1591 = _S1541 + _S1544 + _S1566 + _S1566 + _S1569.vignette_params_2[int(1)].cx_0;
    (&(&_S1443)->vignette_params_2[int(0)])->alpha2_0 = 0.0f;
    float _S1592 = _S1511 + _S1512 + _S1550.differential_0 + _S1569.vignette_params_2[int(0)].alpha2_0;
    (&(&_S1443)->vignette_params_2[int(0)])->alpha1_0 = 0.0f;
    float _S1593 = _S1519 + _S1520 + _S1556.differential_0 + _S1569.vignette_params_2[int(0)].alpha1_0;
    (&(&_S1443)->vignette_params_2[int(0)])->alpha0_0 = 0.0f;
    float _S1594 = _S1527 + _S1528 + _S1562.differential_0 + _S1569.vignette_params_2[int(0)].alpha0_0;
    (&(&_S1443)->vignette_params_2[int(0)])->cy_0 = 0.0f;
    float _S1595 = _S1535 + _S1536 + _S1567 + _S1567 + _S1569.vignette_params_2[int(0)].cy_0;
    (&(&_S1443)->vignette_params_2[int(0)])->cx_0 = 0.0f;
    float _S1596 = _S1543 + _S1544 + _S1568 + _S1568 + _S1569.vignette_params_2[int(0)].cx_0;
    FixedArray<float, 36>  _S1597;
    _S1597[int(0)] = 0.0f;
    _S1597[int(1)] = 0.0f;
    _S1597[int(2)] = 0.0f;
    _S1597[int(3)] = 0.0f;
    _S1597[int(4)] = 0.0f;
    _S1597[int(5)] = 0.0f;
    _S1597[int(6)] = 0.0f;
    _S1597[int(7)] = 0.0f;
    _S1597[int(8)] = 0.0f;
    _S1597[int(9)] = 0.0f;
    _S1597[int(10)] = 0.0f;
    _S1597[int(11)] = 0.0f;
    _S1597[int(12)] = 0.0f;
    _S1597[int(13)] = 0.0f;
    _S1597[int(14)] = 0.0f;
    _S1597[int(15)] = 0.0f;
    _S1597[int(16)] = 0.0f;
    _S1597[int(17)] = 0.0f;
    _S1597[int(18)] = 0.0f;
    _S1597[int(19)] = 0.0f;
    _S1597[int(20)] = 0.0f;
    _S1597[int(21)] = 0.0f;
    _S1597[int(22)] = 0.0f;
    _S1597[int(23)] = 0.0f;
    _S1597[int(24)] = 0.0f;
    _S1597[int(25)] = 0.0f;
    _S1597[int(26)] = 0.0f;
    _S1597[int(27)] = 0.0f;
    _S1597[int(28)] = 0.0f;
    _S1597[int(29)] = 0.0f;
    _S1597[int(30)] = 0.0f;
    _S1597[int(31)] = 0.0f;
    _S1597[int(32)] = 0.0f;
    _S1597[int(33)] = 0.0f;
    _S1597[int(34)] = 0.0f;
    _S1597[int(35)] = 0.0f;
    _S1597[int(8)] = _S1589;
    _S1597[int(16)] = _S1569.color_params_2.b_0.x;
    _S1597[int(15)] = _S1582;
    _S1597[int(14)] = _S1583;
    _S1597[int(13)] = _S1584;
    _S1597[int(12)] = _S1585;
    _S1597[int(11)] = _S1586;
    _S1597[int(10)] = _S1587;
    _S1597[int(9)] = _S1588;
    _S1597[int(17)] = _S1569.color_params_2.b_0.y;
    _S1597[int(7)] = _S1590;
    _S1597[int(6)] = _S1591;
    _S1597[int(5)] = _S1592;
    _S1597[int(4)] = _S1593;
    _S1597[int(3)] = _S1594;
    _S1597[int(2)] = _S1595;
    _S1597[int(1)] = _S1596;
    _S1597[int(0)] = _S1443.exposure_2;
    _S1597[int(26)] = _S1579;
    _S1597[int(34)] = _S1571;
    _S1597[int(33)] = _S1572;
    _S1597[int(32)] = _S1573;
    _S1597[int(31)] = _S1574;
    _S1597[int(30)] = _S1575;
    _S1597[int(29)] = _S1576;
    _S1597[int(28)] = _S1577;
    _S1597[int(27)] = _S1578;
    _S1597[int(35)] = _S1570;
    _S1597[int(25)] = _S1580;
    _S1597[int(24)] = _S1581;
    _S1597[int(23)] = _S1569.color_params_2.n_0.y;
    _S1597[int(22)] = _S1569.color_params_2.n_0.x;
    _S1597[int(21)] = _S1569.color_params_2.g_0.y;
    _S1597[int(20)] = _S1569.color_params_2.g_0.x;
    _S1597[int(19)] = _S1569.color_params_2.r_0.y;
    _S1597[int(18)] = _S1569.color_params_2.r_0.x;
    dpparams_3->primal_0 = dpparams_3->primal_0;
    dpparams_3->differential_0 = _S1597;
    return;
}

inline __device__ void s_bwd_compute_raw_ppisp_regularization_loss_0(DiffPair_arrayx3Cfloatx2C36x3E_0 * _S1598, FixedArray<float, 22>  * _S1599)
{
    s_bwd_prop_compute_raw_ppisp_regularization_loss_0(_S1598, _S1599);
    return;
}

inline __device__ void compute_raw_ppisp_regularization_loss_vjp(FixedArray<float, 36>  params_8, FixedArray<float, 22>  grad_out_3, FixedArray<float, 36>  * _S1600)
{
    FixedArray<float, 36>  _S1601 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    DiffPair_arrayx3Cfloatx2C36x3E_0 dp_params_3;
    (&dp_params_3)->primal_0 = params_8;
    (&dp_params_3)->differential_0 = _S1601;
    FixedArray<float, 22>  _S1602 = grad_out_3;
    s_bwd_compute_raw_ppisp_regularization_loss_0(&dp_params_3, &_S1602);
    *_S1600 = (&dp_params_3)->differential_0;
    return;
}

inline __device__ void s_bwd_prop_compute_raw_ppisp_rqs_regularization_loss_0(DiffPair_arrayx3Cfloatx2C39x3E_0 * dpparams_4, FixedArray<float, 23>  * _s_dOut_4)
{
    VignettingChannelParams_0 _S1603 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    FixedArray<VignettingChannelParams_0, 3>  _S1604 = {
        _S1603, _S1603, _S1603
    };
    float2  _S1605 = make_float2 (0.0f);
    ColorPPISPParams_0 _S1606 = { _S1605, _S1605, _S1605, _S1605 };
    RQSCRFPPISPChannelParams_0 _S1607 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    FixedArray<RQSCRFPPISPChannelParams_0, 3>  _S1608 = {
        _S1607, _S1607, _S1607
    };
    PPISPParamsRQS_0 _S1609;
    (&_S1609)->exposure_1 = dpparams_4->primal_0[int(0)];
    (&_S1609)->vignette_params_1 = _S1604;
    (&_S1609)->color_params_1 = _S1606;
    (&_S1609)->crf_params_0 = _S1608;
    (&(&_S1609)->vignette_params_1[int(0)])->cx_0 = dpparams_4->primal_0[int(1)];
    (&(&_S1609)->vignette_params_1[int(0)])->cy_0 = dpparams_4->primal_0[int(2)];
    (&(&_S1609)->vignette_params_1[int(0)])->alpha0_0 = dpparams_4->primal_0[int(3)];
    (&(&_S1609)->vignette_params_1[int(0)])->alpha1_0 = dpparams_4->primal_0[int(4)];
    (&(&_S1609)->vignette_params_1[int(0)])->alpha2_0 = dpparams_4->primal_0[int(5)];
    (&(&_S1609)->vignette_params_1[int(1)])->cx_0 = dpparams_4->primal_0[int(6)];
    (&(&_S1609)->vignette_params_1[int(1)])->cy_0 = dpparams_4->primal_0[int(7)];
    (&(&_S1609)->vignette_params_1[int(1)])->alpha0_0 = dpparams_4->primal_0[int(8)];
    (&(&_S1609)->vignette_params_1[int(1)])->alpha1_0 = dpparams_4->primal_0[int(9)];
    (&(&_S1609)->vignette_params_1[int(1)])->alpha2_0 = dpparams_4->primal_0[int(10)];
    (&(&_S1609)->vignette_params_1[int(2)])->cx_0 = dpparams_4->primal_0[int(11)];
    (&(&_S1609)->vignette_params_1[int(2)])->cy_0 = dpparams_4->primal_0[int(12)];
    (&(&_S1609)->vignette_params_1[int(2)])->alpha0_0 = dpparams_4->primal_0[int(13)];
    (&(&_S1609)->vignette_params_1[int(2)])->alpha1_0 = dpparams_4->primal_0[int(14)];
    (&(&_S1609)->vignette_params_1[int(2)])->alpha2_0 = dpparams_4->primal_0[int(15)];
    *&((&(&(&_S1609)->color_params_1)->b_0)->x) = dpparams_4->primal_0[int(16)];
    *&((&(&(&_S1609)->color_params_1)->b_0)->y) = dpparams_4->primal_0[int(17)];
    *&((&(&(&_S1609)->color_params_1)->r_0)->x) = dpparams_4->primal_0[int(18)];
    *&((&(&(&_S1609)->color_params_1)->r_0)->y) = dpparams_4->primal_0[int(19)];
    *&((&(&(&_S1609)->color_params_1)->g_0)->x) = dpparams_4->primal_0[int(20)];
    *&((&(&(&_S1609)->color_params_1)->g_0)->y) = dpparams_4->primal_0[int(21)];
    *&((&(&(&_S1609)->color_params_1)->n_0)->x) = dpparams_4->primal_0[int(22)];
    *&((&(&(&_S1609)->color_params_1)->n_0)->y) = dpparams_4->primal_0[int(23)];
    (&(&_S1609)->crf_params_0[int(0)])->g0_0 = dpparams_4->primal_0[int(24)];
    (&(&_S1609)->crf_params_0[int(0)])->g1_0 = dpparams_4->primal_0[int(25)];
    (&(&_S1609)->crf_params_0[int(0)])->x0_0 = dpparams_4->primal_0[int(26)];
    (&(&_S1609)->crf_params_0[int(0)])->y0_0 = dpparams_4->primal_0[int(27)];
    (&(&_S1609)->crf_params_0[int(0)])->gc_0 = dpparams_4->primal_0[int(28)];
    (&(&_S1609)->crf_params_0[int(1)])->g0_0 = dpparams_4->primal_0[int(29)];
    (&(&_S1609)->crf_params_0[int(1)])->g1_0 = dpparams_4->primal_0[int(30)];
    (&(&_S1609)->crf_params_0[int(1)])->x0_0 = dpparams_4->primal_0[int(31)];
    (&(&_S1609)->crf_params_0[int(1)])->y0_0 = dpparams_4->primal_0[int(32)];
    (&(&_S1609)->crf_params_0[int(1)])->gc_0 = dpparams_4->primal_0[int(33)];
    (&(&_S1609)->crf_params_0[int(2)])->g0_0 = dpparams_4->primal_0[int(34)];
    (&(&_S1609)->crf_params_0[int(2)])->g1_0 = dpparams_4->primal_0[int(35)];
    (&(&_S1609)->crf_params_0[int(2)])->x0_0 = dpparams_4->primal_0[int(36)];
    (&(&_S1609)->crf_params_0[int(2)])->y0_0 = dpparams_4->primal_0[int(37)];
    (&(&_S1609)->crf_params_0[int(2)])->gc_0 = dpparams_4->primal_0[int(38)];
    float mean_28 = (dpparams_4->primal_0[int(1)] + dpparams_4->primal_0[int(6)] + dpparams_4->primal_0[int(11)]) / 3.0f;
    float _S1610 = dpparams_4->primal_0[int(1)] - mean_28;
    float _S1611 = dpparams_4->primal_0[int(6)] - mean_28;
    float _S1612 = dpparams_4->primal_0[int(11)] - mean_28;
    float mean_29 = (dpparams_4->primal_0[int(2)] + dpparams_4->primal_0[int(7)] + dpparams_4->primal_0[int(12)]) / 3.0f;
    float _S1613 = dpparams_4->primal_0[int(2)] - mean_29;
    float _S1614 = dpparams_4->primal_0[int(7)] - mean_29;
    float _S1615 = dpparams_4->primal_0[int(12)] - mean_29;
    float mean_30 = (dpparams_4->primal_0[int(3)] + dpparams_4->primal_0[int(8)] + dpparams_4->primal_0[int(13)]) / 3.0f;
    float _S1616 = dpparams_4->primal_0[int(3)] - mean_30;
    float _S1617 = dpparams_4->primal_0[int(8)] - mean_30;
    float _S1618 = dpparams_4->primal_0[int(13)] - mean_30;
    float mean_31 = (dpparams_4->primal_0[int(4)] + dpparams_4->primal_0[int(9)] + dpparams_4->primal_0[int(14)]) / 3.0f;
    float _S1619 = dpparams_4->primal_0[int(4)] - mean_31;
    float _S1620 = dpparams_4->primal_0[int(9)] - mean_31;
    float _S1621 = dpparams_4->primal_0[int(14)] - mean_31;
    float mean_32 = (dpparams_4->primal_0[int(5)] + dpparams_4->primal_0[int(10)] + dpparams_4->primal_0[int(15)]) / 3.0f;
    float _S1622 = dpparams_4->primal_0[int(5)] - mean_32;
    float _S1623 = dpparams_4->primal_0[int(10)] - mean_32;
    float _S1624 = dpparams_4->primal_0[int(15)] - mean_32;
    float mean_33 = (dpparams_4->primal_0[int(24)] + dpparams_4->primal_0[int(29)] + dpparams_4->primal_0[int(34)]) / 3.0f;
    float mean_34 = (dpparams_4->primal_0[int(25)] + dpparams_4->primal_0[int(30)] + dpparams_4->primal_0[int(35)]) / 3.0f;
    float mean_35 = (dpparams_4->primal_0[int(26)] + dpparams_4->primal_0[int(31)] + dpparams_4->primal_0[int(36)]) / 3.0f;
    float mean_36 = (dpparams_4->primal_0[int(27)] + dpparams_4->primal_0[int(32)] + dpparams_4->primal_0[int(37)]) / 3.0f;
    float mean_37 = (dpparams_4->primal_0[int(28)] + dpparams_4->primal_0[int(33)] + dpparams_4->primal_0[int(38)]) / 3.0f;
    float _S1625 = 0.3333333432674408f * (*_s_dOut_4)[int(22)];
    float _S1626 = (dpparams_4->primal_0[int(38)] - mean_37) * _S1625;
    float _S1627 = _S1626 + _S1626;
    float _S1628 = (dpparams_4->primal_0[int(33)] - mean_37) * _S1625;
    float _S1629 = _S1628 + _S1628;
    float _S1630 = (dpparams_4->primal_0[int(28)] - mean_37) * _S1625;
    float _S1631 = _S1630 + _S1630;
    float _S1632 = 0.3333333432674408f * (- _S1627 + - _S1629 + - _S1631);
    float _S1633 = 0.3333333432674408f * (*_s_dOut_4)[int(21)];
    float _S1634 = (dpparams_4->primal_0[int(37)] - mean_36) * _S1633;
    float _S1635 = _S1634 + _S1634;
    float _S1636 = (dpparams_4->primal_0[int(32)] - mean_36) * _S1633;
    float _S1637 = _S1636 + _S1636;
    float _S1638 = (dpparams_4->primal_0[int(27)] - mean_36) * _S1633;
    float _S1639 = _S1638 + _S1638;
    float _S1640 = 0.3333333432674408f * (- _S1635 + - _S1637 + - _S1639);
    float _S1641 = 0.3333333432674408f * (*_s_dOut_4)[int(20)];
    float _S1642 = (dpparams_4->primal_0[int(36)] - mean_35) * _S1641;
    float _S1643 = _S1642 + _S1642;
    float _S1644 = (dpparams_4->primal_0[int(31)] - mean_35) * _S1641;
    float _S1645 = _S1644 + _S1644;
    float _S1646 = (dpparams_4->primal_0[int(26)] - mean_35) * _S1641;
    float _S1647 = _S1646 + _S1646;
    float _S1648 = 0.3333333432674408f * (- _S1643 + - _S1645 + - _S1647);
    float _S1649 = 0.3333333432674408f * (*_s_dOut_4)[int(19)];
    float _S1650 = (dpparams_4->primal_0[int(35)] - mean_34) * _S1649;
    float _S1651 = _S1650 + _S1650;
    float _S1652 = (dpparams_4->primal_0[int(30)] - mean_34) * _S1649;
    float _S1653 = _S1652 + _S1652;
    float _S1654 = (dpparams_4->primal_0[int(25)] - mean_34) * _S1649;
    float _S1655 = _S1654 + _S1654;
    float _S1656 = 0.3333333432674408f * (- _S1651 + - _S1653 + - _S1655);
    float _S1657 = 0.3333333432674408f * (*_s_dOut_4)[int(18)];
    float _S1658 = (dpparams_4->primal_0[int(34)] - mean_33) * _S1657;
    float _S1659 = _S1658 + _S1658;
    float _S1660 = (dpparams_4->primal_0[int(29)] - mean_33) * _S1657;
    float _S1661 = _S1660 + _S1660;
    float _S1662 = (dpparams_4->primal_0[int(24)] - mean_33) * _S1657;
    float _S1663 = _S1662 + _S1662;
    float _S1664 = 0.3333333432674408f * (- _S1659 + - _S1661 + - _S1663);
    float2  _S1665 = make_float2 ((*_s_dOut_4)[int(16)], (*_s_dOut_4)[int(17)]);
    Matrix<float, 2, 2>  _S1666 = makeMatrix<float, 2, 2> (0.0f);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1667;
    (&_S1667)->primal_0 = makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f);
    (&_S1667)->differential_0 = _S1666;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1668;
    (&_S1668)->primal_0 = _S1609.color_params_1.n_0;
    (&_S1668)->differential_0 = _S1605;
    s_bwd_prop_mul_2(&_S1667, &_S1668, _S1665);
    float2  _S1669 = make_float2 ((*_s_dOut_4)[int(14)], (*_s_dOut_4)[int(15)]);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1670;
    (&_S1670)->primal_0 = makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f);
    (&_S1670)->differential_0 = _S1666;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1671;
    (&_S1671)->primal_0 = _S1609.color_params_1.g_0;
    (&_S1671)->differential_0 = _S1605;
    s_bwd_prop_mul_2(&_S1670, &_S1671, _S1669);
    float2  _S1672 = make_float2 ((*_s_dOut_4)[int(12)], (*_s_dOut_4)[int(13)]);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1673;
    (&_S1673)->primal_0 = makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f);
    (&_S1673)->differential_0 = _S1666;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1674;
    (&_S1674)->primal_0 = _S1609.color_params_1.r_0;
    (&_S1674)->differential_0 = _S1605;
    s_bwd_prop_mul_2(&_S1673, &_S1674, _S1672);
    float2  _S1675 = make_float2 ((*_s_dOut_4)[int(10)], (*_s_dOut_4)[int(11)]);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1676;
    (&_S1676)->primal_0 = makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f);
    (&_S1676)->differential_0 = _S1666;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1677;
    (&_S1677)->primal_0 = _S1609.color_params_1.b_0;
    (&_S1677)->differential_0 = _S1605;
    s_bwd_prop_mul_2(&_S1676, &_S1677, _S1675);
    ColorPPISPParams_0 _S1678 = ColorPPISPParams_x24_syn_dzero_0();
    (&_S1678)->n_0 = _S1668.differential_0;
    (&_S1678)->g_0 = _S1671.differential_0;
    (&_S1678)->r_0 = _S1674.differential_0;
    (&_S1678)->b_0 = _S1677.differential_0;
    float _S1679 = 0.3333333432674408f * (*_s_dOut_4)[int(9)];
    float _S1680 = _S1624 * _S1679;
    float _S1681 = _S1680 + _S1680;
    float _S1682 = _S1623 * _S1679;
    float _S1683 = _S1682 + _S1682;
    float _S1684 = _S1622 * _S1679;
    float _S1685 = _S1684 + _S1684;
    float _S1686 = 0.3333333432674408f * (- _S1681 + - _S1683 + - _S1685);
    float _S1687 = 0.3333333432674408f * (*_s_dOut_4)[int(8)];
    float _S1688 = _S1621 * _S1687;
    float _S1689 = _S1688 + _S1688;
    float _S1690 = _S1620 * _S1687;
    float _S1691 = _S1690 + _S1690;
    float _S1692 = _S1619 * _S1687;
    float _S1693 = _S1692 + _S1692;
    float _S1694 = 0.3333333432674408f * (- _S1689 + - _S1691 + - _S1693);
    float _S1695 = 0.3333333432674408f * (*_s_dOut_4)[int(7)];
    float _S1696 = _S1618 * _S1695;
    float _S1697 = _S1696 + _S1696;
    float _S1698 = _S1617 * _S1695;
    float _S1699 = _S1698 + _S1698;
    float _S1700 = _S1616 * _S1695;
    float _S1701 = _S1700 + _S1700;
    float _S1702 = 0.3333333432674408f * (- _S1697 + - _S1699 + - _S1701);
    float _S1703 = 0.3333333432674408f * (*_s_dOut_4)[int(6)];
    float _S1704 = _S1615 * _S1703;
    float _S1705 = _S1704 + _S1704;
    float _S1706 = _S1614 * _S1703;
    float _S1707 = _S1706 + _S1706;
    float _S1708 = _S1613 * _S1703;
    float _S1709 = _S1708 + _S1708;
    float _S1710 = 0.3333333432674408f * (- _S1705 + - _S1707 + - _S1709);
    float _S1711 = 0.3333333432674408f * (*_s_dOut_4)[int(5)];
    float _S1712 = _S1612 * _S1711;
    float _S1713 = _S1712 + _S1712;
    float _S1714 = _S1611 * _S1711;
    float _S1715 = _S1714 + _S1714;
    float _S1716 = _S1610 * _S1711;
    float _S1717 = _S1716 + _S1716;
    float _S1718 = 0.3333333432674408f * (- _S1713 + - _S1715 + - _S1717);
    DiffPair_float_0 _S1719;
    (&_S1719)->primal_0 = 0.0f;
    (&_S1719)->differential_0 = 0.0f;
    DiffPair_float_0 _S1720;
    (&_S1720)->primal_0 = dpparams_4->primal_0[int(15)];
    (&_S1720)->differential_0 = 0.0f;
    _d_max_0(&_S1719, &_S1720, (*_s_dOut_4)[int(4)]);
    DiffPair_float_0 _S1721;
    (&_S1721)->primal_0 = 0.0f;
    (&_S1721)->differential_0 = 0.0f;
    DiffPair_float_0 _S1722;
    (&_S1722)->primal_0 = dpparams_4->primal_0[int(10)];
    (&_S1722)->differential_0 = 0.0f;
    _d_max_0(&_S1721, &_S1722, (*_s_dOut_4)[int(4)]);
    DiffPair_float_0 _S1723;
    (&_S1723)->primal_0 = 0.0f;
    (&_S1723)->differential_0 = 0.0f;
    DiffPair_float_0 _S1724;
    (&_S1724)->primal_0 = dpparams_4->primal_0[int(5)];
    (&_S1724)->differential_0 = 0.0f;
    _d_max_0(&_S1723, &_S1724, (*_s_dOut_4)[int(4)]);
    DiffPair_float_0 _S1725;
    (&_S1725)->primal_0 = 0.0f;
    (&_S1725)->differential_0 = 0.0f;
    DiffPair_float_0 _S1726;
    (&_S1726)->primal_0 = dpparams_4->primal_0[int(14)];
    (&_S1726)->differential_0 = 0.0f;
    _d_max_0(&_S1725, &_S1726, (*_s_dOut_4)[int(3)]);
    DiffPair_float_0 _S1727;
    (&_S1727)->primal_0 = 0.0f;
    (&_S1727)->differential_0 = 0.0f;
    DiffPair_float_0 _S1728;
    (&_S1728)->primal_0 = dpparams_4->primal_0[int(9)];
    (&_S1728)->differential_0 = 0.0f;
    _d_max_0(&_S1727, &_S1728, (*_s_dOut_4)[int(3)]);
    DiffPair_float_0 _S1729;
    (&_S1729)->primal_0 = 0.0f;
    (&_S1729)->differential_0 = 0.0f;
    DiffPair_float_0 _S1730;
    (&_S1730)->primal_0 = dpparams_4->primal_0[int(4)];
    (&_S1730)->differential_0 = 0.0f;
    _d_max_0(&_S1729, &_S1730, (*_s_dOut_4)[int(3)]);
    DiffPair_float_0 _S1731;
    (&_S1731)->primal_0 = 0.0f;
    (&_S1731)->differential_0 = 0.0f;
    DiffPair_float_0 _S1732;
    (&_S1732)->primal_0 = dpparams_4->primal_0[int(13)];
    (&_S1732)->differential_0 = 0.0f;
    _d_max_0(&_S1731, &_S1732, (*_s_dOut_4)[int(2)]);
    DiffPair_float_0 _S1733;
    (&_S1733)->primal_0 = 0.0f;
    (&_S1733)->differential_0 = 0.0f;
    DiffPair_float_0 _S1734;
    (&_S1734)->primal_0 = dpparams_4->primal_0[int(8)];
    (&_S1734)->differential_0 = 0.0f;
    _d_max_0(&_S1733, &_S1734, (*_s_dOut_4)[int(2)]);
    DiffPair_float_0 _S1735;
    (&_S1735)->primal_0 = 0.0f;
    (&_S1735)->differential_0 = 0.0f;
    DiffPair_float_0 _S1736;
    (&_S1736)->primal_0 = dpparams_4->primal_0[int(3)];
    (&_S1736)->differential_0 = 0.0f;
    _d_max_0(&_S1735, &_S1736, (*_s_dOut_4)[int(2)]);
    float _S1737 = dpparams_4->primal_0[int(12)] * (*_s_dOut_4)[int(1)];
    float _S1738 = dpparams_4->primal_0[int(11)] * (*_s_dOut_4)[int(1)];
    float _S1739 = dpparams_4->primal_0[int(7)] * (*_s_dOut_4)[int(1)];
    float _S1740 = dpparams_4->primal_0[int(6)] * (*_s_dOut_4)[int(1)];
    float _S1741 = dpparams_4->primal_0[int(2)] * (*_s_dOut_4)[int(1)];
    float _S1742 = dpparams_4->primal_0[int(1)] * (*_s_dOut_4)[int(1)];
    PPISPParamsRQS_0 _S1743 = PPISPParamsRQS_x24_syn_dzero_0();
    (&_S1743)->color_params_1 = _S1678;
    (&_S1743)->exposure_1 = (*_s_dOut_4)[int(0)];
    _S1609 = _S1743;
    (&(&_S1609)->crf_params_0[int(2)])->gc_0 = 0.0f;
    float _S1744 = _S1627 + _S1632 + _S1743.crf_params_0[int(2)].gc_0;
    (&(&_S1609)->crf_params_0[int(2)])->y0_0 = 0.0f;
    float _S1745 = _S1635 + _S1640 + _S1743.crf_params_0[int(2)].y0_0;
    (&(&_S1609)->crf_params_0[int(2)])->x0_0 = 0.0f;
    float _S1746 = _S1643 + _S1648 + _S1743.crf_params_0[int(2)].x0_0;
    (&(&_S1609)->crf_params_0[int(2)])->g1_0 = 0.0f;
    float _S1747 = _S1651 + _S1656 + _S1743.crf_params_0[int(2)].g1_0;
    (&(&_S1609)->crf_params_0[int(2)])->g0_0 = 0.0f;
    float _S1748 = _S1659 + _S1664 + _S1743.crf_params_0[int(2)].g0_0;
    (&(&_S1609)->crf_params_0[int(1)])->gc_0 = 0.0f;
    float _S1749 = _S1629 + _S1632 + _S1743.crf_params_0[int(1)].gc_0;
    (&(&_S1609)->crf_params_0[int(1)])->y0_0 = 0.0f;
    float _S1750 = _S1637 + _S1640 + _S1743.crf_params_0[int(1)].y0_0;
    (&(&_S1609)->crf_params_0[int(1)])->x0_0 = 0.0f;
    float _S1751 = _S1645 + _S1648 + _S1743.crf_params_0[int(1)].x0_0;
    (&(&_S1609)->crf_params_0[int(1)])->g1_0 = 0.0f;
    float _S1752 = _S1653 + _S1656 + _S1743.crf_params_0[int(1)].g1_0;
    (&(&_S1609)->crf_params_0[int(1)])->g0_0 = 0.0f;
    float _S1753 = _S1661 + _S1664 + _S1743.crf_params_0[int(1)].g0_0;
    (&(&_S1609)->crf_params_0[int(0)])->gc_0 = 0.0f;
    float _S1754 = _S1631 + _S1632 + _S1743.crf_params_0[int(0)].gc_0;
    (&(&_S1609)->crf_params_0[int(0)])->y0_0 = 0.0f;
    float _S1755 = _S1639 + _S1640 + _S1743.crf_params_0[int(0)].y0_0;
    (&(&_S1609)->crf_params_0[int(0)])->x0_0 = 0.0f;
    float _S1756 = _S1647 + _S1648 + _S1743.crf_params_0[int(0)].x0_0;
    (&(&_S1609)->crf_params_0[int(0)])->g1_0 = 0.0f;
    float _S1757 = _S1655 + _S1656 + _S1743.crf_params_0[int(0)].g1_0;
    (&(&_S1609)->crf_params_0[int(0)])->g0_0 = 0.0f;
    float _S1758 = _S1663 + _S1664 + _S1743.crf_params_0[int(0)].g0_0;
    *&((&(&(&_S1609)->color_params_1)->n_0)->y) = 0.0f;
    *&((&(&(&_S1609)->color_params_1)->n_0)->x) = 0.0f;
    *&((&(&(&_S1609)->color_params_1)->g_0)->y) = 0.0f;
    *&((&(&(&_S1609)->color_params_1)->g_0)->x) = 0.0f;
    *&((&(&(&_S1609)->color_params_1)->r_0)->y) = 0.0f;
    *&((&(&(&_S1609)->color_params_1)->r_0)->x) = 0.0f;
    *&((&(&(&_S1609)->color_params_1)->b_0)->y) = 0.0f;
    *&((&(&(&_S1609)->color_params_1)->b_0)->x) = 0.0f;
    (&(&_S1609)->vignette_params_1[int(2)])->alpha2_0 = 0.0f;
    float _S1759 = _S1681 + _S1686 + _S1720.differential_0 + _S1743.vignette_params_1[int(2)].alpha2_0;
    (&(&_S1609)->vignette_params_1[int(2)])->alpha1_0 = 0.0f;
    float _S1760 = _S1689 + _S1694 + _S1726.differential_0 + _S1743.vignette_params_1[int(2)].alpha1_0;
    (&(&_S1609)->vignette_params_1[int(2)])->alpha0_0 = 0.0f;
    float _S1761 = _S1697 + _S1702 + _S1732.differential_0 + _S1743.vignette_params_1[int(2)].alpha0_0;
    (&(&_S1609)->vignette_params_1[int(2)])->cy_0 = 0.0f;
    float _S1762 = _S1705 + _S1710 + _S1737 + _S1737 + _S1743.vignette_params_1[int(2)].cy_0;
    (&(&_S1609)->vignette_params_1[int(2)])->cx_0 = 0.0f;
    float _S1763 = _S1713 + _S1718 + _S1738 + _S1738 + _S1743.vignette_params_1[int(2)].cx_0;
    (&(&_S1609)->vignette_params_1[int(1)])->alpha2_0 = 0.0f;
    float _S1764 = _S1683 + _S1686 + _S1722.differential_0 + _S1743.vignette_params_1[int(1)].alpha2_0;
    (&(&_S1609)->vignette_params_1[int(1)])->alpha1_0 = 0.0f;
    float _S1765 = _S1691 + _S1694 + _S1728.differential_0 + _S1743.vignette_params_1[int(1)].alpha1_0;
    (&(&_S1609)->vignette_params_1[int(1)])->alpha0_0 = 0.0f;
    float _S1766 = _S1699 + _S1702 + _S1734.differential_0 + _S1743.vignette_params_1[int(1)].alpha0_0;
    (&(&_S1609)->vignette_params_1[int(1)])->cy_0 = 0.0f;
    float _S1767 = _S1707 + _S1710 + _S1739 + _S1739 + _S1743.vignette_params_1[int(1)].cy_0;
    (&(&_S1609)->vignette_params_1[int(1)])->cx_0 = 0.0f;
    float _S1768 = _S1715 + _S1718 + _S1740 + _S1740 + _S1743.vignette_params_1[int(1)].cx_0;
    (&(&_S1609)->vignette_params_1[int(0)])->alpha2_0 = 0.0f;
    float _S1769 = _S1685 + _S1686 + _S1724.differential_0 + _S1743.vignette_params_1[int(0)].alpha2_0;
    (&(&_S1609)->vignette_params_1[int(0)])->alpha1_0 = 0.0f;
    float _S1770 = _S1693 + _S1694 + _S1730.differential_0 + _S1743.vignette_params_1[int(0)].alpha1_0;
    (&(&_S1609)->vignette_params_1[int(0)])->alpha0_0 = 0.0f;
    float _S1771 = _S1701 + _S1702 + _S1736.differential_0 + _S1743.vignette_params_1[int(0)].alpha0_0;
    (&(&_S1609)->vignette_params_1[int(0)])->cy_0 = 0.0f;
    float _S1772 = _S1709 + _S1710 + _S1741 + _S1741 + _S1743.vignette_params_1[int(0)].cy_0;
    (&(&_S1609)->vignette_params_1[int(0)])->cx_0 = 0.0f;
    float _S1773 = _S1717 + _S1718 + _S1742 + _S1742 + _S1743.vignette_params_1[int(0)].cx_0;
    FixedArray<float, 39>  _S1774;
    _S1774[int(0)] = 0.0f;
    _S1774[int(1)] = 0.0f;
    _S1774[int(2)] = 0.0f;
    _S1774[int(3)] = 0.0f;
    _S1774[int(4)] = 0.0f;
    _S1774[int(5)] = 0.0f;
    _S1774[int(6)] = 0.0f;
    _S1774[int(7)] = 0.0f;
    _S1774[int(8)] = 0.0f;
    _S1774[int(9)] = 0.0f;
    _S1774[int(10)] = 0.0f;
    _S1774[int(11)] = 0.0f;
    _S1774[int(12)] = 0.0f;
    _S1774[int(13)] = 0.0f;
    _S1774[int(14)] = 0.0f;
    _S1774[int(15)] = 0.0f;
    _S1774[int(16)] = 0.0f;
    _S1774[int(17)] = 0.0f;
    _S1774[int(18)] = 0.0f;
    _S1774[int(19)] = 0.0f;
    _S1774[int(20)] = 0.0f;
    _S1774[int(21)] = 0.0f;
    _S1774[int(22)] = 0.0f;
    _S1774[int(23)] = 0.0f;
    _S1774[int(24)] = 0.0f;
    _S1774[int(25)] = 0.0f;
    _S1774[int(26)] = 0.0f;
    _S1774[int(27)] = 0.0f;
    _S1774[int(28)] = 0.0f;
    _S1774[int(29)] = 0.0f;
    _S1774[int(30)] = 0.0f;
    _S1774[int(31)] = 0.0f;
    _S1774[int(32)] = 0.0f;
    _S1774[int(33)] = 0.0f;
    _S1774[int(34)] = 0.0f;
    _S1774[int(35)] = 0.0f;
    _S1774[int(36)] = 0.0f;
    _S1774[int(37)] = 0.0f;
    _S1774[int(38)] = 0.0f;
    _S1774[int(9)] = _S1765;
    _S1774[int(18)] = _S1743.color_params_1.r_0.x;
    _S1774[int(17)] = _S1743.color_params_1.b_0.y;
    _S1774[int(16)] = _S1743.color_params_1.b_0.x;
    _S1774[int(15)] = _S1759;
    _S1774[int(14)] = _S1760;
    _S1774[int(13)] = _S1761;
    _S1774[int(12)] = _S1762;
    _S1774[int(11)] = _S1763;
    _S1774[int(10)] = _S1764;
    _S1774[int(19)] = _S1743.color_params_1.r_0.y;
    _S1774[int(8)] = _S1766;
    _S1774[int(7)] = _S1767;
    _S1774[int(6)] = _S1768;
    _S1774[int(5)] = _S1769;
    _S1774[int(4)] = _S1770;
    _S1774[int(3)] = _S1771;
    _S1774[int(2)] = _S1772;
    _S1774[int(1)] = _S1773;
    _S1774[int(0)] = _S1609.exposure_1;
    _S1774[int(28)] = _S1754;
    _S1774[int(37)] = _S1745;
    _S1774[int(36)] = _S1746;
    _S1774[int(35)] = _S1747;
    _S1774[int(34)] = _S1748;
    _S1774[int(33)] = _S1749;
    _S1774[int(32)] = _S1750;
    _S1774[int(31)] = _S1751;
    _S1774[int(30)] = _S1752;
    _S1774[int(29)] = _S1753;
    _S1774[int(38)] = _S1744;
    _S1774[int(27)] = _S1755;
    _S1774[int(26)] = _S1756;
    _S1774[int(25)] = _S1757;
    _S1774[int(24)] = _S1758;
    _S1774[int(23)] = _S1743.color_params_1.n_0.y;
    _S1774[int(22)] = _S1743.color_params_1.n_0.x;
    _S1774[int(21)] = _S1743.color_params_1.g_0.y;
    _S1774[int(20)] = _S1743.color_params_1.g_0.x;
    dpparams_4->primal_0 = dpparams_4->primal_0;
    dpparams_4->differential_0 = _S1774;
    return;
}

inline __device__ void s_bwd_compute_raw_ppisp_rqs_regularization_loss_0(DiffPair_arrayx3Cfloatx2C39x3E_0 * _S1775, FixedArray<float, 23>  * _S1776)
{
    s_bwd_prop_compute_raw_ppisp_rqs_regularization_loss_0(_S1775, _S1776);
    return;
}

inline __device__ void compute_raw_ppisp_rqs_regularization_loss_vjp(FixedArray<float, 39>  params_9, FixedArray<float, 23>  grad_out_4, FixedArray<float, 39>  * _S1777)
{
    FixedArray<float, 39>  _S1778 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    DiffPair_arrayx3Cfloatx2C39x3E_0 dp_params_4;
    (&dp_params_4)->primal_0 = params_9;
    (&dp_params_4)->differential_0 = _S1778;
    FixedArray<float, 23>  _S1779 = grad_out_4;
    s_bwd_compute_raw_ppisp_rqs_regularization_loss_0(&dp_params_4, &_S1779);
    *_S1777 = (&dp_params_4)->differential_0;
    return;
}

inline __device__ void compute_raw_ppisp_no_crf_regularization_loss(FixedArray<float, 24>  params_10, FixedArray<float, 18>  * _S1780)
{
    PPISPParamsNoCRF_0 p_5;
    (&p_5)->exposure_0 = params_10[int(0)];
    (&(&p_5)->vignette_params_0[int(0)])->cx_0 = params_10[int(1)];
    (&(&p_5)->vignette_params_0[int(0)])->cy_0 = params_10[int(2)];
    (&(&p_5)->vignette_params_0[int(0)])->alpha0_0 = params_10[int(3)];
    (&(&p_5)->vignette_params_0[int(0)])->alpha1_0 = params_10[int(4)];
    (&(&p_5)->vignette_params_0[int(0)])->alpha2_0 = params_10[int(5)];
    (&(&p_5)->vignette_params_0[int(1)])->cx_0 = params_10[int(6)];
    (&(&p_5)->vignette_params_0[int(1)])->cy_0 = params_10[int(7)];
    (&(&p_5)->vignette_params_0[int(1)])->alpha0_0 = params_10[int(8)];
    (&(&p_5)->vignette_params_0[int(1)])->alpha1_0 = params_10[int(9)];
    (&(&p_5)->vignette_params_0[int(1)])->alpha2_0 = params_10[int(10)];
    (&(&p_5)->vignette_params_0[int(2)])->cx_0 = params_10[int(11)];
    (&(&p_5)->vignette_params_0[int(2)])->cy_0 = params_10[int(12)];
    (&(&p_5)->vignette_params_0[int(2)])->alpha0_0 = params_10[int(13)];
    (&(&p_5)->vignette_params_0[int(2)])->alpha1_0 = params_10[int(14)];
    (&(&p_5)->vignette_params_0[int(2)])->alpha2_0 = params_10[int(15)];
    *&((&(&(&p_5)->color_params_0)->b_0)->x) = params_10[int(16)];
    *&((&(&(&p_5)->color_params_0)->b_0)->y) = params_10[int(17)];
    *&((&(&(&p_5)->color_params_0)->r_0)->x) = params_10[int(18)];
    *&((&(&(&p_5)->color_params_0)->r_0)->y) = params_10[int(19)];
    *&((&(&(&p_5)->color_params_0)->g_0)->x) = params_10[int(20)];
    *&((&(&(&p_5)->color_params_0)->g_0)->y) = params_10[int(21)];
    *&((&(&(&p_5)->color_params_0)->n_0)->x) = params_10[int(22)];
    *&((&(&(&p_5)->color_params_0)->n_0)->y) = params_10[int(23)];
    FixedArray<float, 18>  losses_2;
    losses_2[int(0)] = 0.0f;
    losses_2[int(1)] = 0.0f;
    losses_2[int(2)] = 0.0f;
    losses_2[int(3)] = 0.0f;
    losses_2[int(4)] = 0.0f;
    losses_2[int(5)] = 0.0f;
    losses_2[int(6)] = 0.0f;
    losses_2[int(7)] = 0.0f;
    losses_2[int(8)] = 0.0f;
    losses_2[int(9)] = 0.0f;
    losses_2[int(10)] = 0.0f;
    losses_2[int(11)] = 0.0f;
    losses_2[int(12)] = 0.0f;
    losses_2[int(13)] = 0.0f;
    losses_2[int(14)] = 0.0f;
    losses_2[int(15)] = 0.0f;
    losses_2[int(16)] = 0.0f;
    losses_2[int(17)] = 0.0f;
    losses_2[int(0)] = p_5.exposure_0;
    float _S1781 = p_5.vignette_params_0[int(0)].cx_0;
    float _S1782 = p_5.vignette_params_0[int(0)].cy_0;
    float _S1783 = p_5.vignette_params_0[int(1)].cx_0;
    float _S1784 = p_5.vignette_params_0[int(1)].cy_0;
    float _S1785 = p_5.vignette_params_0[int(2)].cx_0;
    float _S1786 = p_5.vignette_params_0[int(2)].cy_0;
    losses_2[int(1)] = _S1781 * _S1781 + _S1782 * _S1782 + _S1783 * _S1783 + _S1784 * _S1784 + _S1785 * _S1785 + _S1786 * _S1786;
    losses_2[int(2)] = (F32_max((0.0f), (p_5.vignette_params_0[int(0)].alpha0_0))) + (F32_max((0.0f), (p_5.vignette_params_0[int(1)].alpha0_0))) + (F32_max((0.0f), (p_5.vignette_params_0[int(2)].alpha0_0)));
    losses_2[int(3)] = (F32_max((0.0f), (p_5.vignette_params_0[int(0)].alpha1_0))) + (F32_max((0.0f), (p_5.vignette_params_0[int(1)].alpha1_0))) + (F32_max((0.0f), (p_5.vignette_params_0[int(2)].alpha1_0)));
    losses_2[int(4)] = (F32_max((0.0f), (p_5.vignette_params_0[int(0)].alpha2_0))) + (F32_max((0.0f), (p_5.vignette_params_0[int(1)].alpha2_0))) + (F32_max((0.0f), (p_5.vignette_params_0[int(2)].alpha2_0)));
    float mean_38 = (p_5.vignette_params_0[int(0)].cx_0 + p_5.vignette_params_0[int(1)].cx_0 + p_5.vignette_params_0[int(2)].cx_0) / 3.0f;
    float _S1787 = p_5.vignette_params_0[int(0)].cx_0 - mean_38;
    float _S1788 = p_5.vignette_params_0[int(1)].cx_0 - mean_38;
    float _S1789 = p_5.vignette_params_0[int(2)].cx_0 - mean_38;
    losses_2[int(5)] = (_S1787 * _S1787 + _S1788 * _S1788 + _S1789 * _S1789) / 3.0f;
    float mean_39 = (p_5.vignette_params_0[int(0)].cy_0 + p_5.vignette_params_0[int(1)].cy_0 + p_5.vignette_params_0[int(2)].cy_0) / 3.0f;
    float _S1790 = p_5.vignette_params_0[int(0)].cy_0 - mean_39;
    float _S1791 = p_5.vignette_params_0[int(1)].cy_0 - mean_39;
    float _S1792 = p_5.vignette_params_0[int(2)].cy_0 - mean_39;
    losses_2[int(6)] = (_S1790 * _S1790 + _S1791 * _S1791 + _S1792 * _S1792) / 3.0f;
    float mean_40 = (p_5.vignette_params_0[int(0)].alpha0_0 + p_5.vignette_params_0[int(1)].alpha0_0 + p_5.vignette_params_0[int(2)].alpha0_0) / 3.0f;
    float _S1793 = p_5.vignette_params_0[int(0)].alpha0_0 - mean_40;
    float _S1794 = p_5.vignette_params_0[int(1)].alpha0_0 - mean_40;
    float _S1795 = p_5.vignette_params_0[int(2)].alpha0_0 - mean_40;
    losses_2[int(7)] = (_S1793 * _S1793 + _S1794 * _S1794 + _S1795 * _S1795) / 3.0f;
    float mean_41 = (p_5.vignette_params_0[int(0)].alpha1_0 + p_5.vignette_params_0[int(1)].alpha1_0 + p_5.vignette_params_0[int(2)].alpha1_0) / 3.0f;
    float _S1796 = p_5.vignette_params_0[int(0)].alpha1_0 - mean_41;
    float _S1797 = p_5.vignette_params_0[int(1)].alpha1_0 - mean_41;
    float _S1798 = p_5.vignette_params_0[int(2)].alpha1_0 - mean_41;
    losses_2[int(8)] = (_S1796 * _S1796 + _S1797 * _S1797 + _S1798 * _S1798) / 3.0f;
    float mean_42 = (p_5.vignette_params_0[int(0)].alpha2_0 + p_5.vignette_params_0[int(1)].alpha2_0 + p_5.vignette_params_0[int(2)].alpha2_0) / 3.0f;
    float _S1799 = p_5.vignette_params_0[int(0)].alpha2_0 - mean_42;
    float _S1800 = p_5.vignette_params_0[int(1)].alpha2_0 - mean_42;
    float _S1801 = p_5.vignette_params_0[int(2)].alpha2_0 - mean_42;
    losses_2[int(9)] = (_S1799 * _S1799 + _S1800 * _S1800 + _S1801 * _S1801) / 3.0f;
    float2  bd_5 = mul_0(makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f), p_5.color_params_0.b_0);
    float2  rd_5 = mul_0(makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f), p_5.color_params_0.r_0);
    float2  gd_5 = mul_0(makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f), p_5.color_params_0.g_0);
    float2  nd_5 = mul_0(makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f), p_5.color_params_0.n_0);
    losses_2[int(10)] = bd_5.x;
    losses_2[int(11)] = bd_5.y;
    losses_2[int(12)] = rd_5.x;
    losses_2[int(13)] = rd_5.y;
    losses_2[int(14)] = gd_5.x;
    losses_2[int(15)] = gd_5.y;
    losses_2[int(16)] = nd_5.x;
    losses_2[int(17)] = nd_5.y;
    *_S1780 = losses_2;
    return;
}

inline __device__ void s_bwd_prop_compute_raw_ppisp_no_crf_regularization_loss_0(DiffPair_arrayx3Cfloatx2C24x3E_0 * dpparams_5, FixedArray<float, 18>  * _s_dOut_5)
{
    VignettingChannelParams_0 _S1802 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    FixedArray<VignettingChannelParams_0, 3>  _S1803 = {
        _S1802, _S1802, _S1802
    };
    float2  _S1804 = make_float2 (0.0f);
    ColorPPISPParams_0 _S1805 = { _S1804, _S1804, _S1804, _S1804 };
    PPISPParamsNoCRF_0 _S1806;
    (&_S1806)->exposure_0 = dpparams_5->primal_0[int(0)];
    (&_S1806)->vignette_params_0 = _S1803;
    (&_S1806)->color_params_0 = _S1805;
    (&(&_S1806)->vignette_params_0[int(0)])->cx_0 = dpparams_5->primal_0[int(1)];
    (&(&_S1806)->vignette_params_0[int(0)])->cy_0 = dpparams_5->primal_0[int(2)];
    (&(&_S1806)->vignette_params_0[int(0)])->alpha0_0 = dpparams_5->primal_0[int(3)];
    (&(&_S1806)->vignette_params_0[int(0)])->alpha1_0 = dpparams_5->primal_0[int(4)];
    (&(&_S1806)->vignette_params_0[int(0)])->alpha2_0 = dpparams_5->primal_0[int(5)];
    (&(&_S1806)->vignette_params_0[int(1)])->cx_0 = dpparams_5->primal_0[int(6)];
    (&(&_S1806)->vignette_params_0[int(1)])->cy_0 = dpparams_5->primal_0[int(7)];
    (&(&_S1806)->vignette_params_0[int(1)])->alpha0_0 = dpparams_5->primal_0[int(8)];
    (&(&_S1806)->vignette_params_0[int(1)])->alpha1_0 = dpparams_5->primal_0[int(9)];
    (&(&_S1806)->vignette_params_0[int(1)])->alpha2_0 = dpparams_5->primal_0[int(10)];
    (&(&_S1806)->vignette_params_0[int(2)])->cx_0 = dpparams_5->primal_0[int(11)];
    (&(&_S1806)->vignette_params_0[int(2)])->cy_0 = dpparams_5->primal_0[int(12)];
    (&(&_S1806)->vignette_params_0[int(2)])->alpha0_0 = dpparams_5->primal_0[int(13)];
    (&(&_S1806)->vignette_params_0[int(2)])->alpha1_0 = dpparams_5->primal_0[int(14)];
    (&(&_S1806)->vignette_params_0[int(2)])->alpha2_0 = dpparams_5->primal_0[int(15)];
    *&((&(&(&_S1806)->color_params_0)->b_0)->x) = dpparams_5->primal_0[int(16)];
    *&((&(&(&_S1806)->color_params_0)->b_0)->y) = dpparams_5->primal_0[int(17)];
    *&((&(&(&_S1806)->color_params_0)->r_0)->x) = dpparams_5->primal_0[int(18)];
    *&((&(&(&_S1806)->color_params_0)->r_0)->y) = dpparams_5->primal_0[int(19)];
    *&((&(&(&_S1806)->color_params_0)->g_0)->x) = dpparams_5->primal_0[int(20)];
    *&((&(&(&_S1806)->color_params_0)->g_0)->y) = dpparams_5->primal_0[int(21)];
    *&((&(&(&_S1806)->color_params_0)->n_0)->x) = dpparams_5->primal_0[int(22)];
    *&((&(&(&_S1806)->color_params_0)->n_0)->y) = dpparams_5->primal_0[int(23)];
    float mean_43 = (dpparams_5->primal_0[int(1)] + dpparams_5->primal_0[int(6)] + dpparams_5->primal_0[int(11)]) / 3.0f;
    float _S1807 = dpparams_5->primal_0[int(1)] - mean_43;
    float _S1808 = dpparams_5->primal_0[int(6)] - mean_43;
    float _S1809 = dpparams_5->primal_0[int(11)] - mean_43;
    float mean_44 = (dpparams_5->primal_0[int(2)] + dpparams_5->primal_0[int(7)] + dpparams_5->primal_0[int(12)]) / 3.0f;
    float _S1810 = dpparams_5->primal_0[int(2)] - mean_44;
    float _S1811 = dpparams_5->primal_0[int(7)] - mean_44;
    float _S1812 = dpparams_5->primal_0[int(12)] - mean_44;
    float mean_45 = (dpparams_5->primal_0[int(3)] + dpparams_5->primal_0[int(8)] + dpparams_5->primal_0[int(13)]) / 3.0f;
    float _S1813 = dpparams_5->primal_0[int(3)] - mean_45;
    float _S1814 = dpparams_5->primal_0[int(8)] - mean_45;
    float _S1815 = dpparams_5->primal_0[int(13)] - mean_45;
    float mean_46 = (dpparams_5->primal_0[int(4)] + dpparams_5->primal_0[int(9)] + dpparams_5->primal_0[int(14)]) / 3.0f;
    float _S1816 = dpparams_5->primal_0[int(4)] - mean_46;
    float _S1817 = dpparams_5->primal_0[int(9)] - mean_46;
    float _S1818 = dpparams_5->primal_0[int(14)] - mean_46;
    float mean_47 = (dpparams_5->primal_0[int(5)] + dpparams_5->primal_0[int(10)] + dpparams_5->primal_0[int(15)]) / 3.0f;
    float _S1819 = dpparams_5->primal_0[int(5)] - mean_47;
    float _S1820 = dpparams_5->primal_0[int(10)] - mean_47;
    float _S1821 = dpparams_5->primal_0[int(15)] - mean_47;
    float2  _S1822 = make_float2 ((*_s_dOut_5)[int(16)], (*_s_dOut_5)[int(17)]);
    Matrix<float, 2, 2>  _S1823 = makeMatrix<float, 2, 2> (0.0f);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1824;
    (&_S1824)->primal_0 = makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f);
    (&_S1824)->differential_0 = _S1823;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1825;
    (&_S1825)->primal_0 = _S1806.color_params_0.n_0;
    (&_S1825)->differential_0 = _S1804;
    s_bwd_prop_mul_2(&_S1824, &_S1825, _S1822);
    float2  _S1826 = make_float2 ((*_s_dOut_5)[int(14)], (*_s_dOut_5)[int(15)]);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1827;
    (&_S1827)->primal_0 = makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f);
    (&_S1827)->differential_0 = _S1823;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1828;
    (&_S1828)->primal_0 = _S1806.color_params_0.g_0;
    (&_S1828)->differential_0 = _S1804;
    s_bwd_prop_mul_2(&_S1827, &_S1828, _S1826);
    float2  _S1829 = make_float2 ((*_s_dOut_5)[int(12)], (*_s_dOut_5)[int(13)]);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1830;
    (&_S1830)->primal_0 = makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f);
    (&_S1830)->differential_0 = _S1823;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1831;
    (&_S1831)->primal_0 = _S1806.color_params_0.r_0;
    (&_S1831)->differential_0 = _S1804;
    s_bwd_prop_mul_2(&_S1830, &_S1831, _S1829);
    float2  _S1832 = make_float2 ((*_s_dOut_5)[int(10)], (*_s_dOut_5)[int(11)]);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1833;
    (&_S1833)->primal_0 = makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f);
    (&_S1833)->differential_0 = _S1823;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1834;
    (&_S1834)->primal_0 = _S1806.color_params_0.b_0;
    (&_S1834)->differential_0 = _S1804;
    s_bwd_prop_mul_2(&_S1833, &_S1834, _S1832);
    ColorPPISPParams_0 _S1835 = ColorPPISPParams_x24_syn_dzero_0();
    (&_S1835)->n_0 = _S1825.differential_0;
    (&_S1835)->g_0 = _S1828.differential_0;
    (&_S1835)->r_0 = _S1831.differential_0;
    (&_S1835)->b_0 = _S1834.differential_0;
    float _S1836 = 0.3333333432674408f * (*_s_dOut_5)[int(9)];
    float _S1837 = _S1821 * _S1836;
    float _S1838 = _S1837 + _S1837;
    float _S1839 = _S1820 * _S1836;
    float _S1840 = _S1839 + _S1839;
    float _S1841 = _S1819 * _S1836;
    float _S1842 = _S1841 + _S1841;
    float _S1843 = 0.3333333432674408f * (- _S1838 + - _S1840 + - _S1842);
    float _S1844 = 0.3333333432674408f * (*_s_dOut_5)[int(8)];
    float _S1845 = _S1818 * _S1844;
    float _S1846 = _S1845 + _S1845;
    float _S1847 = _S1817 * _S1844;
    float _S1848 = _S1847 + _S1847;
    float _S1849 = _S1816 * _S1844;
    float _S1850 = _S1849 + _S1849;
    float _S1851 = 0.3333333432674408f * (- _S1846 + - _S1848 + - _S1850);
    float _S1852 = 0.3333333432674408f * (*_s_dOut_5)[int(7)];
    float _S1853 = _S1815 * _S1852;
    float _S1854 = _S1853 + _S1853;
    float _S1855 = _S1814 * _S1852;
    float _S1856 = _S1855 + _S1855;
    float _S1857 = _S1813 * _S1852;
    float _S1858 = _S1857 + _S1857;
    float _S1859 = 0.3333333432674408f * (- _S1854 + - _S1856 + - _S1858);
    float _S1860 = 0.3333333432674408f * (*_s_dOut_5)[int(6)];
    float _S1861 = _S1812 * _S1860;
    float _S1862 = _S1861 + _S1861;
    float _S1863 = _S1811 * _S1860;
    float _S1864 = _S1863 + _S1863;
    float _S1865 = _S1810 * _S1860;
    float _S1866 = _S1865 + _S1865;
    float _S1867 = 0.3333333432674408f * (- _S1862 + - _S1864 + - _S1866);
    float _S1868 = 0.3333333432674408f * (*_s_dOut_5)[int(5)];
    float _S1869 = _S1809 * _S1868;
    float _S1870 = _S1869 + _S1869;
    float _S1871 = _S1808 * _S1868;
    float _S1872 = _S1871 + _S1871;
    float _S1873 = _S1807 * _S1868;
    float _S1874 = _S1873 + _S1873;
    float _S1875 = 0.3333333432674408f * (- _S1870 + - _S1872 + - _S1874);
    DiffPair_float_0 _S1876;
    (&_S1876)->primal_0 = 0.0f;
    (&_S1876)->differential_0 = 0.0f;
    DiffPair_float_0 _S1877;
    (&_S1877)->primal_0 = dpparams_5->primal_0[int(15)];
    (&_S1877)->differential_0 = 0.0f;
    _d_max_0(&_S1876, &_S1877, (*_s_dOut_5)[int(4)]);
    DiffPair_float_0 _S1878;
    (&_S1878)->primal_0 = 0.0f;
    (&_S1878)->differential_0 = 0.0f;
    DiffPair_float_0 _S1879;
    (&_S1879)->primal_0 = dpparams_5->primal_0[int(10)];
    (&_S1879)->differential_0 = 0.0f;
    _d_max_0(&_S1878, &_S1879, (*_s_dOut_5)[int(4)]);
    DiffPair_float_0 _S1880;
    (&_S1880)->primal_0 = 0.0f;
    (&_S1880)->differential_0 = 0.0f;
    DiffPair_float_0 _S1881;
    (&_S1881)->primal_0 = dpparams_5->primal_0[int(5)];
    (&_S1881)->differential_0 = 0.0f;
    _d_max_0(&_S1880, &_S1881, (*_s_dOut_5)[int(4)]);
    DiffPair_float_0 _S1882;
    (&_S1882)->primal_0 = 0.0f;
    (&_S1882)->differential_0 = 0.0f;
    DiffPair_float_0 _S1883;
    (&_S1883)->primal_0 = dpparams_5->primal_0[int(14)];
    (&_S1883)->differential_0 = 0.0f;
    _d_max_0(&_S1882, &_S1883, (*_s_dOut_5)[int(3)]);
    DiffPair_float_0 _S1884;
    (&_S1884)->primal_0 = 0.0f;
    (&_S1884)->differential_0 = 0.0f;
    DiffPair_float_0 _S1885;
    (&_S1885)->primal_0 = dpparams_5->primal_0[int(9)];
    (&_S1885)->differential_0 = 0.0f;
    _d_max_0(&_S1884, &_S1885, (*_s_dOut_5)[int(3)]);
    DiffPair_float_0 _S1886;
    (&_S1886)->primal_0 = 0.0f;
    (&_S1886)->differential_0 = 0.0f;
    DiffPair_float_0 _S1887;
    (&_S1887)->primal_0 = dpparams_5->primal_0[int(4)];
    (&_S1887)->differential_0 = 0.0f;
    _d_max_0(&_S1886, &_S1887, (*_s_dOut_5)[int(3)]);
    DiffPair_float_0 _S1888;
    (&_S1888)->primal_0 = 0.0f;
    (&_S1888)->differential_0 = 0.0f;
    DiffPair_float_0 _S1889;
    (&_S1889)->primal_0 = dpparams_5->primal_0[int(13)];
    (&_S1889)->differential_0 = 0.0f;
    _d_max_0(&_S1888, &_S1889, (*_s_dOut_5)[int(2)]);
    DiffPair_float_0 _S1890;
    (&_S1890)->primal_0 = 0.0f;
    (&_S1890)->differential_0 = 0.0f;
    DiffPair_float_0 _S1891;
    (&_S1891)->primal_0 = dpparams_5->primal_0[int(8)];
    (&_S1891)->differential_0 = 0.0f;
    _d_max_0(&_S1890, &_S1891, (*_s_dOut_5)[int(2)]);
    DiffPair_float_0 _S1892;
    (&_S1892)->primal_0 = 0.0f;
    (&_S1892)->differential_0 = 0.0f;
    DiffPair_float_0 _S1893;
    (&_S1893)->primal_0 = dpparams_5->primal_0[int(3)];
    (&_S1893)->differential_0 = 0.0f;
    _d_max_0(&_S1892, &_S1893, (*_s_dOut_5)[int(2)]);
    float _S1894 = dpparams_5->primal_0[int(12)] * (*_s_dOut_5)[int(1)];
    float _S1895 = dpparams_5->primal_0[int(11)] * (*_s_dOut_5)[int(1)];
    float _S1896 = dpparams_5->primal_0[int(7)] * (*_s_dOut_5)[int(1)];
    float _S1897 = dpparams_5->primal_0[int(6)] * (*_s_dOut_5)[int(1)];
    float _S1898 = dpparams_5->primal_0[int(2)] * (*_s_dOut_5)[int(1)];
    float _S1899 = dpparams_5->primal_0[int(1)] * (*_s_dOut_5)[int(1)];
    PPISPParamsNoCRF_0 _S1900 = PPISPParamsNoCRF_x24_syn_dzero_0();
    (&_S1900)->color_params_0 = _S1835;
    (&_S1900)->exposure_0 = (*_s_dOut_5)[int(0)];
    _S1806 = _S1900;
    *&((&(&(&_S1806)->color_params_0)->n_0)->y) = 0.0f;
    *&((&(&(&_S1806)->color_params_0)->n_0)->x) = 0.0f;
    *&((&(&(&_S1806)->color_params_0)->g_0)->y) = 0.0f;
    *&((&(&(&_S1806)->color_params_0)->g_0)->x) = 0.0f;
    *&((&(&(&_S1806)->color_params_0)->r_0)->y) = 0.0f;
    *&((&(&(&_S1806)->color_params_0)->r_0)->x) = 0.0f;
    *&((&(&(&_S1806)->color_params_0)->b_0)->y) = 0.0f;
    *&((&(&(&_S1806)->color_params_0)->b_0)->x) = 0.0f;
    (&(&_S1806)->vignette_params_0[int(2)])->alpha2_0 = 0.0f;
    float _S1901 = _S1838 + _S1843 + _S1877.differential_0 + _S1900.vignette_params_0[int(2)].alpha2_0;
    (&(&_S1806)->vignette_params_0[int(2)])->alpha1_0 = 0.0f;
    float _S1902 = _S1846 + _S1851 + _S1883.differential_0 + _S1900.vignette_params_0[int(2)].alpha1_0;
    (&(&_S1806)->vignette_params_0[int(2)])->alpha0_0 = 0.0f;
    float _S1903 = _S1854 + _S1859 + _S1889.differential_0 + _S1900.vignette_params_0[int(2)].alpha0_0;
    (&(&_S1806)->vignette_params_0[int(2)])->cy_0 = 0.0f;
    float _S1904 = _S1862 + _S1867 + _S1894 + _S1894 + _S1900.vignette_params_0[int(2)].cy_0;
    (&(&_S1806)->vignette_params_0[int(2)])->cx_0 = 0.0f;
    float _S1905 = _S1870 + _S1875 + _S1895 + _S1895 + _S1900.vignette_params_0[int(2)].cx_0;
    (&(&_S1806)->vignette_params_0[int(1)])->alpha2_0 = 0.0f;
    float _S1906 = _S1840 + _S1843 + _S1879.differential_0 + _S1900.vignette_params_0[int(1)].alpha2_0;
    (&(&_S1806)->vignette_params_0[int(1)])->alpha1_0 = 0.0f;
    float _S1907 = _S1848 + _S1851 + _S1885.differential_0 + _S1900.vignette_params_0[int(1)].alpha1_0;
    (&(&_S1806)->vignette_params_0[int(1)])->alpha0_0 = 0.0f;
    float _S1908 = _S1856 + _S1859 + _S1891.differential_0 + _S1900.vignette_params_0[int(1)].alpha0_0;
    (&(&_S1806)->vignette_params_0[int(1)])->cy_0 = 0.0f;
    float _S1909 = _S1864 + _S1867 + _S1896 + _S1896 + _S1900.vignette_params_0[int(1)].cy_0;
    (&(&_S1806)->vignette_params_0[int(1)])->cx_0 = 0.0f;
    float _S1910 = _S1872 + _S1875 + _S1897 + _S1897 + _S1900.vignette_params_0[int(1)].cx_0;
    (&(&_S1806)->vignette_params_0[int(0)])->alpha2_0 = 0.0f;
    float _S1911 = _S1842 + _S1843 + _S1881.differential_0 + _S1900.vignette_params_0[int(0)].alpha2_0;
    (&(&_S1806)->vignette_params_0[int(0)])->alpha1_0 = 0.0f;
    float _S1912 = _S1850 + _S1851 + _S1887.differential_0 + _S1900.vignette_params_0[int(0)].alpha1_0;
    (&(&_S1806)->vignette_params_0[int(0)])->alpha0_0 = 0.0f;
    float _S1913 = _S1858 + _S1859 + _S1893.differential_0 + _S1900.vignette_params_0[int(0)].alpha0_0;
    (&(&_S1806)->vignette_params_0[int(0)])->cy_0 = 0.0f;
    float _S1914 = _S1866 + _S1867 + _S1898 + _S1898 + _S1900.vignette_params_0[int(0)].cy_0;
    (&(&_S1806)->vignette_params_0[int(0)])->cx_0 = 0.0f;
    float _S1915 = _S1874 + _S1875 + _S1899 + _S1899 + _S1900.vignette_params_0[int(0)].cx_0;
    FixedArray<float, 24>  _S1916;
    _S1916[int(0)] = 0.0f;
    _S1916[int(1)] = 0.0f;
    _S1916[int(2)] = 0.0f;
    _S1916[int(3)] = 0.0f;
    _S1916[int(4)] = 0.0f;
    _S1916[int(5)] = 0.0f;
    _S1916[int(6)] = 0.0f;
    _S1916[int(7)] = 0.0f;
    _S1916[int(8)] = 0.0f;
    _S1916[int(9)] = 0.0f;
    _S1916[int(10)] = 0.0f;
    _S1916[int(11)] = 0.0f;
    _S1916[int(12)] = 0.0f;
    _S1916[int(13)] = 0.0f;
    _S1916[int(14)] = 0.0f;
    _S1916[int(15)] = 0.0f;
    _S1916[int(16)] = 0.0f;
    _S1916[int(17)] = 0.0f;
    _S1916[int(18)] = 0.0f;
    _S1916[int(19)] = 0.0f;
    _S1916[int(20)] = 0.0f;
    _S1916[int(21)] = 0.0f;
    _S1916[int(22)] = 0.0f;
    _S1916[int(23)] = 0.0f;
    _S1916[int(11)] = _S1905;
    _S1916[int(0)] = _S1806.exposure_0;
    _S1916[int(1)] = _S1915;
    _S1916[int(2)] = _S1914;
    _S1916[int(3)] = _S1913;
    _S1916[int(4)] = _S1912;
    _S1916[int(5)] = _S1911;
    _S1916[int(6)] = _S1910;
    _S1916[int(7)] = _S1909;
    _S1916[int(8)] = _S1908;
    _S1916[int(9)] = _S1907;
    _S1916[int(10)] = _S1906;
    _S1916[int(23)] = _S1900.color_params_0.n_0.y;
    _S1916[int(12)] = _S1904;
    _S1916[int(13)] = _S1903;
    _S1916[int(14)] = _S1902;
    _S1916[int(15)] = _S1901;
    _S1916[int(16)] = _S1900.color_params_0.b_0.x;
    _S1916[int(17)] = _S1900.color_params_0.b_0.y;
    _S1916[int(18)] = _S1900.color_params_0.r_0.x;
    _S1916[int(19)] = _S1900.color_params_0.r_0.y;
    _S1916[int(20)] = _S1900.color_params_0.g_0.x;
    _S1916[int(21)] = _S1900.color_params_0.g_0.y;
    _S1916[int(22)] = _S1900.color_params_0.n_0.x;
    dpparams_5->primal_0 = dpparams_5->primal_0;
    dpparams_5->differential_0 = _S1916;
    return;
}

inline __device__ void s_bwd_compute_raw_ppisp_no_crf_regularization_loss_0(DiffPair_arrayx3Cfloatx2C24x3E_0 * _S1917, FixedArray<float, 18>  * _S1918)
{
    s_bwd_prop_compute_raw_ppisp_no_crf_regularization_loss_0(_S1917, _S1918);
    return;
}

inline __device__ void compute_raw_ppisp_no_crf_regularization_loss_vjp(FixedArray<float, 24>  params_11, FixedArray<float, 18>  grad_out_5, FixedArray<float, 24>  * _S1919)
{
    FixedArray<float, 24>  _S1920 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    DiffPair_arrayx3Cfloatx2C24x3E_0 dp_params_5;
    (&dp_params_5)->primal_0 = params_11;
    (&dp_params_5)->differential_0 = _S1920;
    FixedArray<float, 18>  _S1921 = grad_out_5;
    s_bwd_compute_raw_ppisp_no_crf_regularization_loss_0(&dp_params_5, &_S1921);
    *_S1919 = (&dp_params_5)->differential_0;
    return;
}

inline __device__ void compute_ppisp_regularization_loss(FixedArray<float, 22>  raw_losses_0, int num_cameras_0, FixedArray<float, 6>  loss_weights_0, FixedArray<float, 6>  * _S1922)
{
    float _S1923;
    FixedArray<float, 6>  losses_3;
    float _S1924 = float(num_cameras_0);
    float _S1925 = raw_losses_0[int(0)] / _S1924;
    for(;;)
    {
        float _S1926 = (F32_abs((_S1925)));
        if(_S1926 < 0.10000000149011612f)
        {
            _S1923 = 0.5f * _S1925 * _S1925 / 0.10000000149011612f;
            break;
        }
        else
        {
            _S1923 = _S1926 - 0.05000000074505806f;
            break;
        }
    }
    losses_3[int(0)] = _S1923;
    losses_3[int(1)] = raw_losses_0[int(1)] / (3.0f * _S1924);
    losses_3[int(2)] = (raw_losses_0[int(2)] + raw_losses_0[int(3)] + raw_losses_0[int(4)]) / (9.0f * _S1924);
    losses_3[int(3)] = (raw_losses_0[int(5)] + raw_losses_0[int(6)] + raw_losses_0[int(7)] + raw_losses_0[int(8)] + raw_losses_0[int(9)]) / (5.0f * _S1924);
    float _S1927 = raw_losses_0[int(10)] / _S1924;
    for(;;)
    {
        float _S1928 = (F32_abs((_S1927)));
        if(_S1928 < 0.00499999988824129f)
        {
            _S1923 = 0.5f * _S1927 * _S1927 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S1923 = _S1928 - 0.00249999994412065f;
            break;
        }
    }
    float _S1929;
    float _S1930 = raw_losses_0[int(11)] / _S1924;
    for(;;)
    {
        float _S1931 = (F32_abs((_S1930)));
        if(_S1931 < 0.00499999988824129f)
        {
            _S1929 = 0.5f * _S1930 * _S1930 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S1929 = _S1931 - 0.00249999994412065f;
            break;
        }
    }
    float _S1932 = _S1923 + _S1929;
    float _S1933 = raw_losses_0[int(12)] / _S1924;
    for(;;)
    {
        float _S1934 = (F32_abs((_S1933)));
        if(_S1934 < 0.00499999988824129f)
        {
            _S1923 = 0.5f * _S1933 * _S1933 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S1923 = _S1934 - 0.00249999994412065f;
            break;
        }
    }
    float _S1935 = _S1932 + _S1923;
    float _S1936 = raw_losses_0[int(13)] / _S1924;
    for(;;)
    {
        float _S1937 = (F32_abs((_S1936)));
        if(_S1937 < 0.00499999988824129f)
        {
            _S1923 = 0.5f * _S1936 * _S1936 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S1923 = _S1937 - 0.00249999994412065f;
            break;
        }
    }
    float _S1938 = _S1935 + _S1923;
    float _S1939 = raw_losses_0[int(14)] / _S1924;
    for(;;)
    {
        float _S1940 = (F32_abs((_S1939)));
        if(_S1940 < 0.00499999988824129f)
        {
            _S1923 = 0.5f * _S1939 * _S1939 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S1923 = _S1940 - 0.00249999994412065f;
            break;
        }
    }
    float _S1941 = _S1938 + _S1923;
    float _S1942 = raw_losses_0[int(15)] / _S1924;
    for(;;)
    {
        float _S1943 = (F32_abs((_S1942)));
        if(_S1943 < 0.00499999988824129f)
        {
            _S1923 = 0.5f * _S1942 * _S1942 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S1923 = _S1943 - 0.00249999994412065f;
            break;
        }
    }
    float _S1944 = _S1941 + _S1923;
    float _S1945 = raw_losses_0[int(16)] / _S1924;
    for(;;)
    {
        float _S1946 = (F32_abs((_S1945)));
        if(_S1946 < 0.00499999988824129f)
        {
            _S1923 = 0.5f * _S1945 * _S1945 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S1923 = _S1946 - 0.00249999994412065f;
            break;
        }
    }
    float _S1947 = _S1944 + _S1923;
    float _S1948 = raw_losses_0[int(17)] / _S1924;
    for(;;)
    {
        float _S1949 = (F32_abs((_S1948)));
        if(_S1949 < 0.00499999988824129f)
        {
            _S1923 = 0.5f * _S1948 * _S1948 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S1923 = _S1949 - 0.00249999994412065f;
            break;
        }
    }
    float _S1950 = (_S1947 + _S1923) / 8.0f;
    float _S1951 = (raw_losses_0[int(18)] + raw_losses_0[int(19)] + raw_losses_0[int(20)] + raw_losses_0[int(21)]) / (4.0f * _S1924);
    losses_3[int(0)] = losses_3[int(0)] * loss_weights_0[int(0)];
    losses_3[int(1)] = losses_3[int(1)] * loss_weights_0[int(1)];
    losses_3[int(2)] = losses_3[int(2)] * loss_weights_0[int(2)];
    losses_3[int(3)] = losses_3[int(3)] * loss_weights_0[int(3)];
    losses_3[int(4)] = _S1950 * loss_weights_0[int(4)];
    losses_3[int(5)] = _S1951 * loss_weights_0[int(5)];
    *_S1922 = losses_3;
    return;
}

inline __device__ void compute_ppisp_rqs_regularization_loss(FixedArray<float, 23>  raw_losses_1, int num_cameras_1, FixedArray<float, 6>  loss_weights_1, FixedArray<float, 6>  * _S1952)
{
    float _S1953;
    FixedArray<float, 6>  losses_4;
    float _S1954 = float(num_cameras_1);
    float _S1955 = raw_losses_1[int(0)] / _S1954;
    for(;;)
    {
        float _S1956 = (F32_abs((_S1955)));
        if(_S1956 < 0.10000000149011612f)
        {
            _S1953 = 0.5f * _S1955 * _S1955 / 0.10000000149011612f;
            break;
        }
        else
        {
            _S1953 = _S1956 - 0.05000000074505806f;
            break;
        }
    }
    losses_4[int(0)] = _S1953;
    losses_4[int(1)] = raw_losses_1[int(1)] / (3.0f * _S1954);
    losses_4[int(2)] = (raw_losses_1[int(2)] + raw_losses_1[int(3)] + raw_losses_1[int(4)]) / (9.0f * _S1954);
    float _S1957 = 5.0f * _S1954;
    losses_4[int(3)] = (raw_losses_1[int(5)] + raw_losses_1[int(6)] + raw_losses_1[int(7)] + raw_losses_1[int(8)] + raw_losses_1[int(9)]) / _S1957;
    float _S1958 = raw_losses_1[int(10)] / _S1954;
    for(;;)
    {
        float _S1959 = (F32_abs((_S1958)));
        if(_S1959 < 0.00499999988824129f)
        {
            _S1953 = 0.5f * _S1958 * _S1958 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S1953 = _S1959 - 0.00249999994412065f;
            break;
        }
    }
    float _S1960;
    float _S1961 = raw_losses_1[int(11)] / _S1954;
    for(;;)
    {
        float _S1962 = (F32_abs((_S1961)));
        if(_S1962 < 0.00499999988824129f)
        {
            _S1960 = 0.5f * _S1961 * _S1961 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S1960 = _S1962 - 0.00249999994412065f;
            break;
        }
    }
    float _S1963 = _S1953 + _S1960;
    float _S1964 = raw_losses_1[int(12)] / _S1954;
    for(;;)
    {
        float _S1965 = (F32_abs((_S1964)));
        if(_S1965 < 0.00499999988824129f)
        {
            _S1953 = 0.5f * _S1964 * _S1964 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S1953 = _S1965 - 0.00249999994412065f;
            break;
        }
    }
    float _S1966 = _S1963 + _S1953;
    float _S1967 = raw_losses_1[int(13)] / _S1954;
    for(;;)
    {
        float _S1968 = (F32_abs((_S1967)));
        if(_S1968 < 0.00499999988824129f)
        {
            _S1953 = 0.5f * _S1967 * _S1967 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S1953 = _S1968 - 0.00249999994412065f;
            break;
        }
    }
    float _S1969 = _S1966 + _S1953;
    float _S1970 = raw_losses_1[int(14)] / _S1954;
    for(;;)
    {
        float _S1971 = (F32_abs((_S1970)));
        if(_S1971 < 0.00499999988824129f)
        {
            _S1953 = 0.5f * _S1970 * _S1970 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S1953 = _S1971 - 0.00249999994412065f;
            break;
        }
    }
    float _S1972 = _S1969 + _S1953;
    float _S1973 = raw_losses_1[int(15)] / _S1954;
    for(;;)
    {
        float _S1974 = (F32_abs((_S1973)));
        if(_S1974 < 0.00499999988824129f)
        {
            _S1953 = 0.5f * _S1973 * _S1973 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S1953 = _S1974 - 0.00249999994412065f;
            break;
        }
    }
    float _S1975 = _S1972 + _S1953;
    float _S1976 = raw_losses_1[int(16)] / _S1954;
    for(;;)
    {
        float _S1977 = (F32_abs((_S1976)));
        if(_S1977 < 0.00499999988824129f)
        {
            _S1953 = 0.5f * _S1976 * _S1976 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S1953 = _S1977 - 0.00249999994412065f;
            break;
        }
    }
    float _S1978 = _S1975 + _S1953;
    float _S1979 = raw_losses_1[int(17)] / _S1954;
    for(;;)
    {
        float _S1980 = (F32_abs((_S1979)));
        if(_S1980 < 0.00499999988824129f)
        {
            _S1953 = 0.5f * _S1979 * _S1979 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S1953 = _S1980 - 0.00249999994412065f;
            break;
        }
    }
    float _S1981 = (_S1978 + _S1953) / 8.0f;
    float _S1982 = (raw_losses_1[int(18)] + raw_losses_1[int(19)] + raw_losses_1[int(20)] + raw_losses_1[int(21)] + raw_losses_1[int(22)]) / _S1957;
    losses_4[int(0)] = losses_4[int(0)] * loss_weights_1[int(0)];
    losses_4[int(1)] = losses_4[int(1)] * loss_weights_1[int(1)];
    losses_4[int(2)] = losses_4[int(2)] * loss_weights_1[int(2)];
    losses_4[int(3)] = losses_4[int(3)] * loss_weights_1[int(3)];
    losses_4[int(4)] = _S1981 * loss_weights_1[int(4)];
    losses_4[int(5)] = _S1982 * loss_weights_1[int(5)];
    *_S1952 = losses_4;
    return;
}

struct DiffPair_arrayx3Cfloatx2C22x3E_0
{
    FixedArray<float, 22>  primal_0;
    FixedArray<float, 22>  differential_0;
};

inline __device__ void s_bwd_prop_compute_ppisp_regularization_loss_0(DiffPair_arrayx3Cfloatx2C22x3E_0 * dpraw_losses_0, int num_cameras_2, FixedArray<float, 6>  * loss_weights_2, FixedArray<float, 6>  * _s_dOut_6)
{
    FixedArray<float, 22>  _S1983 = dpraw_losses_0->primal_0;
    float _S1984 = float(num_cameras_2);
    float _S1985 = dpraw_losses_0->primal_0[int(0)] / _S1984;
    bool _S1986 = (s_primal_ctx_abs_0(_S1985)) < 0.10000000149011612f;
    float _S1987;
    if(_S1986)
    {
        _S1987 = 0.5f * _S1985;
    }
    else
    {
        _S1987 = 0.0f;
    }
    float _S1988 = 3.0f * _S1984;
    float _S1989 = 9.0f * _S1984;
    float _S1990 = 5.0f * _S1984;
    float _S1991 = _S1983[int(10)] / _S1984;
    bool _S1992 = (s_primal_ctx_abs_0(_S1991)) < 0.00499999988824129f;
    float _S1993;
    if(_S1992)
    {
        _S1993 = 0.5f * _S1991;
    }
    else
    {
        _S1993 = 0.0f;
    }
    float _S1994 = _S1983[int(11)] / _S1984;
    bool _S1995 = (s_primal_ctx_abs_0(_S1994)) < 0.00499999988824129f;
    float _S1996;
    if(_S1995)
    {
        _S1996 = 0.5f * _S1994;
    }
    else
    {
        _S1996 = 0.0f;
    }
    float _S1997 = _S1983[int(12)] / _S1984;
    bool _S1998 = (s_primal_ctx_abs_0(_S1997)) < 0.00499999988824129f;
    float _S1999;
    if(_S1998)
    {
        _S1999 = 0.5f * _S1997;
    }
    else
    {
        _S1999 = 0.0f;
    }
    float _S2000 = _S1983[int(13)] / _S1984;
    bool _S2001 = (s_primal_ctx_abs_0(_S2000)) < 0.00499999988824129f;
    float _S2002;
    if(_S2001)
    {
        _S2002 = 0.5f * _S2000;
    }
    else
    {
        _S2002 = 0.0f;
    }
    float _S2003 = _S1983[int(14)] / _S1984;
    bool _S2004 = (s_primal_ctx_abs_0(_S2003)) < 0.00499999988824129f;
    float _S2005;
    if(_S2004)
    {
        _S2005 = 0.5f * _S2003;
    }
    else
    {
        _S2005 = 0.0f;
    }
    float _S2006 = _S1983[int(15)] / _S1984;
    bool _S2007 = (s_primal_ctx_abs_0(_S2006)) < 0.00499999988824129f;
    float _S2008;
    if(_S2007)
    {
        _S2008 = 0.5f * _S2006;
    }
    else
    {
        _S2008 = 0.0f;
    }
    float _S2009 = _S1983[int(16)] / _S1984;
    bool _S2010 = (s_primal_ctx_abs_0(_S2009)) < 0.00499999988824129f;
    float _S2011;
    if(_S2010)
    {
        _S2011 = 0.5f * _S2009;
    }
    else
    {
        _S2011 = 0.0f;
    }
    float _S2012 = _S1983[int(17)] / _S1984;
    bool _S2013 = (s_primal_ctx_abs_0(_S2012)) < 0.00499999988824129f;
    float _S2014;
    if(_S2013)
    {
        _S2014 = 0.5f * _S2012;
    }
    else
    {
        _S2014 = 0.0f;
    }
    float _S2015 = (*loss_weights_2)[int(3)] * (*_s_dOut_6)[int(3)];
    float _S2016 = (*loss_weights_2)[int(2)] * (*_s_dOut_6)[int(2)];
    float _S2017 = (*loss_weights_2)[int(1)] * (*_s_dOut_6)[int(1)];
    float _S2018 = (*loss_weights_2)[int(0)] * (*_s_dOut_6)[int(0)];
    float _S2019 = (*loss_weights_2)[int(5)] * (*_s_dOut_6)[int(5)] / (4.0f * _S1984);
    float _S2020 = 0.125f * ((*loss_weights_2)[int(4)] * (*_s_dOut_6)[int(4)]);
    FixedArray<float, 22>  _S2021;
    _S2021[int(0)] = 0.0f;
    _S2021[int(1)] = 0.0f;
    _S2021[int(2)] = 0.0f;
    _S2021[int(3)] = 0.0f;
    _S2021[int(4)] = 0.0f;
    _S2021[int(5)] = 0.0f;
    _S2021[int(6)] = 0.0f;
    _S2021[int(7)] = 0.0f;
    _S2021[int(8)] = 0.0f;
    _S2021[int(9)] = 0.0f;
    _S2021[int(10)] = 0.0f;
    _S2021[int(11)] = 0.0f;
    _S2021[int(12)] = 0.0f;
    _S2021[int(13)] = 0.0f;
    _S2021[int(14)] = 0.0f;
    _S2021[int(15)] = 0.0f;
    _S2021[int(16)] = 0.0f;
    _S2021[int(17)] = 0.0f;
    _S2021[int(18)] = 0.0f;
    _S2021[int(19)] = 0.0f;
    _S2021[int(20)] = 0.0f;
    _S2021[int(21)] = 0.0f;
    _S2021[int(21)] = _S2019;
    _S2021[int(20)] = _S2019;
    _S2021[int(19)] = _S2019;
    _S2021[int(18)] = _S2019;
    float _S2022 = _S2021[int(0)];
    float _S2023 = _S2021[int(1)];
    float _S2024 = _S2021[int(2)];
    float _S2025 = _S2021[int(3)];
    float _S2026 = _S2021[int(4)];
    float _S2027 = _S2021[int(5)];
    float _S2028 = _S2021[int(6)];
    float _S2029 = _S2021[int(7)];
    float _S2030 = _S2021[int(8)];
    float _S2031 = _S2021[int(9)];
    float _S2032 = _S2021[int(10)];
    float _S2033 = _S2021[int(11)];
    float _S2034 = _S2021[int(12)];
    float _S2035 = _S2021[int(13)];
    float _S2036 = _S2021[int(14)];
    float _S2037 = _S2021[int(15)];
    float _S2038 = _S2021[int(16)];
    float _S2039 = _S2021[int(17)];
    float _S2040 = _S2021[int(18)];
    float _S2041 = _S2021[int(19)];
    float _S2042 = _S2021[int(20)];
    float _S2043 = _S2021[int(21)];
    float _S2044;
    if(_S2013)
    {
        float _S2045 = 200.0f * _S2020;
        float _S2046 = _S2014 * _S2045 + 0.5f * (_S2012 * _S2045);
        _S2014 = 0.0f;
        _S2044 = _S2046;
    }
    else
    {
        _S2014 = _S2020;
        _S2044 = 0.0f;
    }
    DiffPair_float_0 _S2047;
    (&_S2047)->primal_0 = _S2012;
    (&_S2047)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2047, _S2014);
    float _S2048 = (_S2047.differential_0 + _S2044) / _S1984;
    FixedArray<float, 22>  _S2049;
    _S2049[int(0)] = 0.0f;
    _S2049[int(1)] = 0.0f;
    _S2049[int(2)] = 0.0f;
    _S2049[int(3)] = 0.0f;
    _S2049[int(4)] = 0.0f;
    _S2049[int(5)] = 0.0f;
    _S2049[int(6)] = 0.0f;
    _S2049[int(7)] = 0.0f;
    _S2049[int(8)] = 0.0f;
    _S2049[int(9)] = 0.0f;
    _S2049[int(10)] = 0.0f;
    _S2049[int(11)] = 0.0f;
    _S2049[int(12)] = 0.0f;
    _S2049[int(13)] = 0.0f;
    _S2049[int(14)] = 0.0f;
    _S2049[int(15)] = 0.0f;
    _S2049[int(16)] = 0.0f;
    _S2049[int(17)] = 0.0f;
    _S2049[int(18)] = 0.0f;
    _S2049[int(19)] = 0.0f;
    _S2049[int(20)] = 0.0f;
    _S2049[int(21)] = 0.0f;
    _S2049[int(17)] = _S2048;
    float _S2050 = _S2022 + _S2049[int(0)];
    float _S2051 = _S2023 + _S2049[int(1)];
    float _S2052 = _S2024 + _S2049[int(2)];
    float _S2053 = _S2025 + _S2049[int(3)];
    float _S2054 = _S2026 + _S2049[int(4)];
    float _S2055 = _S2027 + _S2049[int(5)];
    float _S2056 = _S2028 + _S2049[int(6)];
    float _S2057 = _S2029 + _S2049[int(7)];
    float _S2058 = _S2030 + _S2049[int(8)];
    float _S2059 = _S2031 + _S2049[int(9)];
    float _S2060 = _S2032 + _S2049[int(10)];
    float _S2061 = _S2033 + _S2049[int(11)];
    float _S2062 = _S2034 + _S2049[int(12)];
    float _S2063 = _S2035 + _S2049[int(13)];
    float _S2064 = _S2036 + _S2049[int(14)];
    float _S2065 = _S2037 + _S2049[int(15)];
    float _S2066 = _S2038 + _S2049[int(16)];
    float _S2067 = _S2039 + _S2049[int(17)];
    float _S2068 = _S2040 + _S2049[int(18)];
    float _S2069 = _S2041 + _S2049[int(19)];
    float _S2070 = _S2042 + _S2049[int(20)];
    float _S2071 = _S2043 + _S2049[int(21)];
    if(_S2010)
    {
        float _S2072 = 200.0f * _S2020;
        float _S2073 = _S2011 * _S2072 + 0.5f * (_S2009 * _S2072);
        _S2011 = 0.0f;
        _S2014 = _S2073;
    }
    else
    {
        _S2011 = _S2020;
        _S2014 = 0.0f;
    }
    DiffPair_float_0 _S2074;
    (&_S2074)->primal_0 = _S2009;
    (&_S2074)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2074, _S2011);
    float _S2075 = (_S2074.differential_0 + _S2014) / _S1984;
    FixedArray<float, 22>  _S2076;
    _S2076[int(0)] = 0.0f;
    _S2076[int(1)] = 0.0f;
    _S2076[int(2)] = 0.0f;
    _S2076[int(3)] = 0.0f;
    _S2076[int(4)] = 0.0f;
    _S2076[int(5)] = 0.0f;
    _S2076[int(6)] = 0.0f;
    _S2076[int(7)] = 0.0f;
    _S2076[int(8)] = 0.0f;
    _S2076[int(9)] = 0.0f;
    _S2076[int(10)] = 0.0f;
    _S2076[int(11)] = 0.0f;
    _S2076[int(12)] = 0.0f;
    _S2076[int(13)] = 0.0f;
    _S2076[int(14)] = 0.0f;
    _S2076[int(15)] = 0.0f;
    _S2076[int(16)] = 0.0f;
    _S2076[int(17)] = 0.0f;
    _S2076[int(18)] = 0.0f;
    _S2076[int(19)] = 0.0f;
    _S2076[int(20)] = 0.0f;
    _S2076[int(21)] = 0.0f;
    _S2076[int(16)] = _S2075;
    float _S2077 = _S2050 + _S2076[int(0)];
    float _S2078 = _S2051 + _S2076[int(1)];
    float _S2079 = _S2052 + _S2076[int(2)];
    float _S2080 = _S2053 + _S2076[int(3)];
    float _S2081 = _S2054 + _S2076[int(4)];
    float _S2082 = _S2055 + _S2076[int(5)];
    float _S2083 = _S2056 + _S2076[int(6)];
    float _S2084 = _S2057 + _S2076[int(7)];
    float _S2085 = _S2058 + _S2076[int(8)];
    float _S2086 = _S2059 + _S2076[int(9)];
    float _S2087 = _S2060 + _S2076[int(10)];
    float _S2088 = _S2061 + _S2076[int(11)];
    float _S2089 = _S2062 + _S2076[int(12)];
    float _S2090 = _S2063 + _S2076[int(13)];
    float _S2091 = _S2064 + _S2076[int(14)];
    float _S2092 = _S2065 + _S2076[int(15)];
    float _S2093 = _S2066 + _S2076[int(16)];
    float _S2094 = _S2067 + _S2076[int(17)];
    float _S2095 = _S2068 + _S2076[int(18)];
    float _S2096 = _S2069 + _S2076[int(19)];
    float _S2097 = _S2070 + _S2076[int(20)];
    float _S2098 = _S2071 + _S2076[int(21)];
    if(_S2007)
    {
        float _S2099 = 200.0f * _S2020;
        float _S2100 = _S2008 * _S2099 + 0.5f * (_S2006 * _S2099);
        _S2008 = 0.0f;
        _S2011 = _S2100;
    }
    else
    {
        _S2008 = _S2020;
        _S2011 = 0.0f;
    }
    DiffPair_float_0 _S2101;
    (&_S2101)->primal_0 = _S2006;
    (&_S2101)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2101, _S2008);
    float _S2102 = (_S2101.differential_0 + _S2011) / _S1984;
    FixedArray<float, 22>  _S2103;
    _S2103[int(0)] = 0.0f;
    _S2103[int(1)] = 0.0f;
    _S2103[int(2)] = 0.0f;
    _S2103[int(3)] = 0.0f;
    _S2103[int(4)] = 0.0f;
    _S2103[int(5)] = 0.0f;
    _S2103[int(6)] = 0.0f;
    _S2103[int(7)] = 0.0f;
    _S2103[int(8)] = 0.0f;
    _S2103[int(9)] = 0.0f;
    _S2103[int(10)] = 0.0f;
    _S2103[int(11)] = 0.0f;
    _S2103[int(12)] = 0.0f;
    _S2103[int(13)] = 0.0f;
    _S2103[int(14)] = 0.0f;
    _S2103[int(15)] = 0.0f;
    _S2103[int(16)] = 0.0f;
    _S2103[int(17)] = 0.0f;
    _S2103[int(18)] = 0.0f;
    _S2103[int(19)] = 0.0f;
    _S2103[int(20)] = 0.0f;
    _S2103[int(21)] = 0.0f;
    _S2103[int(15)] = _S2102;
    float _S2104 = _S2077 + _S2103[int(0)];
    float _S2105 = _S2078 + _S2103[int(1)];
    float _S2106 = _S2079 + _S2103[int(2)];
    float _S2107 = _S2080 + _S2103[int(3)];
    float _S2108 = _S2081 + _S2103[int(4)];
    float _S2109 = _S2082 + _S2103[int(5)];
    float _S2110 = _S2083 + _S2103[int(6)];
    float _S2111 = _S2084 + _S2103[int(7)];
    float _S2112 = _S2085 + _S2103[int(8)];
    float _S2113 = _S2086 + _S2103[int(9)];
    float _S2114 = _S2087 + _S2103[int(10)];
    float _S2115 = _S2088 + _S2103[int(11)];
    float _S2116 = _S2089 + _S2103[int(12)];
    float _S2117 = _S2090 + _S2103[int(13)];
    float _S2118 = _S2091 + _S2103[int(14)];
    float _S2119 = _S2092 + _S2103[int(15)];
    float _S2120 = _S2093 + _S2103[int(16)];
    float _S2121 = _S2094 + _S2103[int(17)];
    float _S2122 = _S2095 + _S2103[int(18)];
    float _S2123 = _S2096 + _S2103[int(19)];
    float _S2124 = _S2097 + _S2103[int(20)];
    float _S2125 = _S2098 + _S2103[int(21)];
    if(_S2004)
    {
        float _S2126 = 200.0f * _S2020;
        float _S2127 = _S2005 * _S2126 + 0.5f * (_S2003 * _S2126);
        _S2005 = 0.0f;
        _S2008 = _S2127;
    }
    else
    {
        _S2005 = _S2020;
        _S2008 = 0.0f;
    }
    DiffPair_float_0 _S2128;
    (&_S2128)->primal_0 = _S2003;
    (&_S2128)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2128, _S2005);
    float _S2129 = (_S2128.differential_0 + _S2008) / _S1984;
    FixedArray<float, 22>  _S2130;
    _S2130[int(0)] = 0.0f;
    _S2130[int(1)] = 0.0f;
    _S2130[int(2)] = 0.0f;
    _S2130[int(3)] = 0.0f;
    _S2130[int(4)] = 0.0f;
    _S2130[int(5)] = 0.0f;
    _S2130[int(6)] = 0.0f;
    _S2130[int(7)] = 0.0f;
    _S2130[int(8)] = 0.0f;
    _S2130[int(9)] = 0.0f;
    _S2130[int(10)] = 0.0f;
    _S2130[int(11)] = 0.0f;
    _S2130[int(12)] = 0.0f;
    _S2130[int(13)] = 0.0f;
    _S2130[int(14)] = 0.0f;
    _S2130[int(15)] = 0.0f;
    _S2130[int(16)] = 0.0f;
    _S2130[int(17)] = 0.0f;
    _S2130[int(18)] = 0.0f;
    _S2130[int(19)] = 0.0f;
    _S2130[int(20)] = 0.0f;
    _S2130[int(21)] = 0.0f;
    _S2130[int(14)] = _S2129;
    float _S2131 = _S2104 + _S2130[int(0)];
    float _S2132 = _S2105 + _S2130[int(1)];
    float _S2133 = _S2106 + _S2130[int(2)];
    float _S2134 = _S2107 + _S2130[int(3)];
    float _S2135 = _S2108 + _S2130[int(4)];
    float _S2136 = _S2109 + _S2130[int(5)];
    float _S2137 = _S2110 + _S2130[int(6)];
    float _S2138 = _S2111 + _S2130[int(7)];
    float _S2139 = _S2112 + _S2130[int(8)];
    float _S2140 = _S2113 + _S2130[int(9)];
    float _S2141 = _S2114 + _S2130[int(10)];
    float _S2142 = _S2115 + _S2130[int(11)];
    float _S2143 = _S2116 + _S2130[int(12)];
    float _S2144 = _S2117 + _S2130[int(13)];
    float _S2145 = _S2118 + _S2130[int(14)];
    float _S2146 = _S2119 + _S2130[int(15)];
    float _S2147 = _S2120 + _S2130[int(16)];
    float _S2148 = _S2121 + _S2130[int(17)];
    float _S2149 = _S2122 + _S2130[int(18)];
    float _S2150 = _S2123 + _S2130[int(19)];
    float _S2151 = _S2124 + _S2130[int(20)];
    float _S2152 = _S2125 + _S2130[int(21)];
    if(_S2001)
    {
        float _S2153 = 200.0f * _S2020;
        float _S2154 = _S2002 * _S2153 + 0.5f * (_S2000 * _S2153);
        _S2002 = 0.0f;
        _S2005 = _S2154;
    }
    else
    {
        _S2002 = _S2020;
        _S2005 = 0.0f;
    }
    DiffPair_float_0 _S2155;
    (&_S2155)->primal_0 = _S2000;
    (&_S2155)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2155, _S2002);
    float _S2156 = (_S2155.differential_0 + _S2005) / _S1984;
    FixedArray<float, 22>  _S2157;
    _S2157[int(0)] = 0.0f;
    _S2157[int(1)] = 0.0f;
    _S2157[int(2)] = 0.0f;
    _S2157[int(3)] = 0.0f;
    _S2157[int(4)] = 0.0f;
    _S2157[int(5)] = 0.0f;
    _S2157[int(6)] = 0.0f;
    _S2157[int(7)] = 0.0f;
    _S2157[int(8)] = 0.0f;
    _S2157[int(9)] = 0.0f;
    _S2157[int(10)] = 0.0f;
    _S2157[int(11)] = 0.0f;
    _S2157[int(12)] = 0.0f;
    _S2157[int(13)] = 0.0f;
    _S2157[int(14)] = 0.0f;
    _S2157[int(15)] = 0.0f;
    _S2157[int(16)] = 0.0f;
    _S2157[int(17)] = 0.0f;
    _S2157[int(18)] = 0.0f;
    _S2157[int(19)] = 0.0f;
    _S2157[int(20)] = 0.0f;
    _S2157[int(21)] = 0.0f;
    _S2157[int(13)] = _S2156;
    float _S2158 = _S2131 + _S2157[int(0)];
    float _S2159 = _S2132 + _S2157[int(1)];
    float _S2160 = _S2133 + _S2157[int(2)];
    float _S2161 = _S2134 + _S2157[int(3)];
    float _S2162 = _S2135 + _S2157[int(4)];
    float _S2163 = _S2136 + _S2157[int(5)];
    float _S2164 = _S2137 + _S2157[int(6)];
    float _S2165 = _S2138 + _S2157[int(7)];
    float _S2166 = _S2139 + _S2157[int(8)];
    float _S2167 = _S2140 + _S2157[int(9)];
    float _S2168 = _S2141 + _S2157[int(10)];
    float _S2169 = _S2142 + _S2157[int(11)];
    float _S2170 = _S2143 + _S2157[int(12)];
    float _S2171 = _S2144 + _S2157[int(13)];
    float _S2172 = _S2145 + _S2157[int(14)];
    float _S2173 = _S2146 + _S2157[int(15)];
    float _S2174 = _S2147 + _S2157[int(16)];
    float _S2175 = _S2148 + _S2157[int(17)];
    float _S2176 = _S2149 + _S2157[int(18)];
    float _S2177 = _S2150 + _S2157[int(19)];
    float _S2178 = _S2151 + _S2157[int(20)];
    float _S2179 = _S2152 + _S2157[int(21)];
    if(_S1998)
    {
        float _S2180 = 200.0f * _S2020;
        float _S2181 = _S1999 * _S2180 + 0.5f * (_S1997 * _S2180);
        _S1999 = 0.0f;
        _S2002 = _S2181;
    }
    else
    {
        _S1999 = _S2020;
        _S2002 = 0.0f;
    }
    DiffPair_float_0 _S2182;
    (&_S2182)->primal_0 = _S1997;
    (&_S2182)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2182, _S1999);
    float _S2183 = (_S2182.differential_0 + _S2002) / _S1984;
    FixedArray<float, 22>  _S2184;
    _S2184[int(0)] = 0.0f;
    _S2184[int(1)] = 0.0f;
    _S2184[int(2)] = 0.0f;
    _S2184[int(3)] = 0.0f;
    _S2184[int(4)] = 0.0f;
    _S2184[int(5)] = 0.0f;
    _S2184[int(6)] = 0.0f;
    _S2184[int(7)] = 0.0f;
    _S2184[int(8)] = 0.0f;
    _S2184[int(9)] = 0.0f;
    _S2184[int(10)] = 0.0f;
    _S2184[int(11)] = 0.0f;
    _S2184[int(12)] = 0.0f;
    _S2184[int(13)] = 0.0f;
    _S2184[int(14)] = 0.0f;
    _S2184[int(15)] = 0.0f;
    _S2184[int(16)] = 0.0f;
    _S2184[int(17)] = 0.0f;
    _S2184[int(18)] = 0.0f;
    _S2184[int(19)] = 0.0f;
    _S2184[int(20)] = 0.0f;
    _S2184[int(21)] = 0.0f;
    _S2184[int(12)] = _S2183;
    float _S2185 = _S2158 + _S2184[int(0)];
    float _S2186 = _S2159 + _S2184[int(1)];
    float _S2187 = _S2160 + _S2184[int(2)];
    float _S2188 = _S2161 + _S2184[int(3)];
    float _S2189 = _S2162 + _S2184[int(4)];
    float _S2190 = _S2163 + _S2184[int(5)];
    float _S2191 = _S2164 + _S2184[int(6)];
    float _S2192 = _S2165 + _S2184[int(7)];
    float _S2193 = _S2166 + _S2184[int(8)];
    float _S2194 = _S2167 + _S2184[int(9)];
    float _S2195 = _S2168 + _S2184[int(10)];
    float _S2196 = _S2169 + _S2184[int(11)];
    float _S2197 = _S2170 + _S2184[int(12)];
    float _S2198 = _S2171 + _S2184[int(13)];
    float _S2199 = _S2172 + _S2184[int(14)];
    float _S2200 = _S2173 + _S2184[int(15)];
    float _S2201 = _S2174 + _S2184[int(16)];
    float _S2202 = _S2175 + _S2184[int(17)];
    float _S2203 = _S2176 + _S2184[int(18)];
    float _S2204 = _S2177 + _S2184[int(19)];
    float _S2205 = _S2178 + _S2184[int(20)];
    float _S2206 = _S2179 + _S2184[int(21)];
    if(_S1995)
    {
        float _S2207 = 200.0f * _S2020;
        float _S2208 = _S1996 * _S2207 + 0.5f * (_S1994 * _S2207);
        _S1996 = 0.0f;
        _S1999 = _S2208;
    }
    else
    {
        _S1996 = _S2020;
        _S1999 = 0.0f;
    }
    DiffPair_float_0 _S2209;
    (&_S2209)->primal_0 = _S1994;
    (&_S2209)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2209, _S1996);
    float _S2210 = (_S2209.differential_0 + _S1999) / _S1984;
    FixedArray<float, 22>  _S2211;
    _S2211[int(0)] = 0.0f;
    _S2211[int(1)] = 0.0f;
    _S2211[int(2)] = 0.0f;
    _S2211[int(3)] = 0.0f;
    _S2211[int(4)] = 0.0f;
    _S2211[int(5)] = 0.0f;
    _S2211[int(6)] = 0.0f;
    _S2211[int(7)] = 0.0f;
    _S2211[int(8)] = 0.0f;
    _S2211[int(9)] = 0.0f;
    _S2211[int(10)] = 0.0f;
    _S2211[int(11)] = 0.0f;
    _S2211[int(12)] = 0.0f;
    _S2211[int(13)] = 0.0f;
    _S2211[int(14)] = 0.0f;
    _S2211[int(15)] = 0.0f;
    _S2211[int(16)] = 0.0f;
    _S2211[int(17)] = 0.0f;
    _S2211[int(18)] = 0.0f;
    _S2211[int(19)] = 0.0f;
    _S2211[int(20)] = 0.0f;
    _S2211[int(21)] = 0.0f;
    _S2211[int(11)] = _S2210;
    float _S2212 = _S2185 + _S2211[int(0)];
    float _S2213 = _S2186 + _S2211[int(1)];
    float _S2214 = _S2187 + _S2211[int(2)];
    float _S2215 = _S2188 + _S2211[int(3)];
    float _S2216 = _S2189 + _S2211[int(4)];
    float _S2217 = _S2190 + _S2211[int(5)];
    float _S2218 = _S2191 + _S2211[int(6)];
    float _S2219 = _S2192 + _S2211[int(7)];
    float _S2220 = _S2193 + _S2211[int(8)];
    float _S2221 = _S2194 + _S2211[int(9)];
    float _S2222 = _S2195 + _S2211[int(10)];
    float _S2223 = _S2196 + _S2211[int(11)];
    float _S2224 = _S2197 + _S2211[int(12)];
    float _S2225 = _S2198 + _S2211[int(13)];
    float _S2226 = _S2199 + _S2211[int(14)];
    float _S2227 = _S2200 + _S2211[int(15)];
    float _S2228 = _S2201 + _S2211[int(16)];
    float _S2229 = _S2202 + _S2211[int(17)];
    float _S2230 = _S2203 + _S2211[int(18)];
    float _S2231 = _S2204 + _S2211[int(19)];
    float _S2232 = _S2205 + _S2211[int(20)];
    float _S2233 = _S2206 + _S2211[int(21)];
    if(_S1992)
    {
        float _S2234 = 200.0f * _S2020;
        float _S2235 = _S1993 * _S2234 + 0.5f * (_S1991 * _S2234);
        _S1993 = 0.0f;
        _S1996 = _S2235;
    }
    else
    {
        _S1993 = _S2020;
        _S1996 = 0.0f;
    }
    DiffPair_float_0 _S2236;
    (&_S2236)->primal_0 = _S1991;
    (&_S2236)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2236, _S1993);
    float _S2237 = (_S2236.differential_0 + _S1996) / _S1984;
    float _S2238 = _S2015 / _S1990;
    float _S2239 = _S2016 / _S1989;
    float _S2240 = _S2017 / _S1988;
    FixedArray<float, 22>  _S2241;
    _S2241[int(0)] = 0.0f;
    _S2241[int(1)] = 0.0f;
    _S2241[int(2)] = 0.0f;
    _S2241[int(3)] = 0.0f;
    _S2241[int(4)] = 0.0f;
    _S2241[int(5)] = 0.0f;
    _S2241[int(6)] = 0.0f;
    _S2241[int(7)] = 0.0f;
    _S2241[int(8)] = 0.0f;
    _S2241[int(9)] = 0.0f;
    _S2241[int(10)] = 0.0f;
    _S2241[int(11)] = 0.0f;
    _S2241[int(12)] = 0.0f;
    _S2241[int(13)] = 0.0f;
    _S2241[int(14)] = 0.0f;
    _S2241[int(15)] = 0.0f;
    _S2241[int(16)] = 0.0f;
    _S2241[int(17)] = 0.0f;
    _S2241[int(18)] = 0.0f;
    _S2241[int(19)] = 0.0f;
    _S2241[int(20)] = 0.0f;
    _S2241[int(21)] = 0.0f;
    _S2241[int(10)] = _S2237;
    _S2241[int(9)] = _S2238;
    _S2241[int(8)] = _S2238;
    _S2241[int(7)] = _S2238;
    _S2241[int(6)] = _S2238;
    _S2241[int(5)] = _S2238;
    _S2241[int(4)] = _S2239;
    _S2241[int(3)] = _S2239;
    _S2241[int(2)] = _S2239;
    _S2241[int(1)] = _S2240;
    float _S2242 = _S2212 + _S2241[int(0)];
    float _S2243 = _S2213 + _S2241[int(1)];
    float _S2244 = _S2214 + _S2241[int(2)];
    float _S2245 = _S2215 + _S2241[int(3)];
    float _S2246 = _S2216 + _S2241[int(4)];
    float _S2247 = _S2217 + _S2241[int(5)];
    float _S2248 = _S2218 + _S2241[int(6)];
    float _S2249 = _S2219 + _S2241[int(7)];
    float _S2250 = _S2220 + _S2241[int(8)];
    float _S2251 = _S2221 + _S2241[int(9)];
    float _S2252 = _S2222 + _S2241[int(10)];
    float _S2253 = _S2223 + _S2241[int(11)];
    float _S2254 = _S2224 + _S2241[int(12)];
    float _S2255 = _S2225 + _S2241[int(13)];
    float _S2256 = _S2226 + _S2241[int(14)];
    float _S2257 = _S2227 + _S2241[int(15)];
    float _S2258 = _S2228 + _S2241[int(16)];
    float _S2259 = _S2229 + _S2241[int(17)];
    float _S2260 = _S2230 + _S2241[int(18)];
    float _S2261 = _S2231 + _S2241[int(19)];
    float _S2262 = _S2232 + _S2241[int(20)];
    float _S2263 = _S2233 + _S2241[int(21)];
    if(_S1986)
    {
        float _S2264 = 10.0f * _S2018;
        float _S2265 = _S1987 * _S2264 + 0.5f * (_S1985 * _S2264);
        _S1987 = 0.0f;
        _S1993 = _S2265;
    }
    else
    {
        _S1987 = _S2018;
        _S1993 = 0.0f;
    }
    DiffPair_float_0 _S2266;
    (&_S2266)->primal_0 = _S1985;
    (&_S2266)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2266, _S1987);
    float _S2267 = (_S2266.differential_0 + _S1993) / _S1984;
    FixedArray<float, 22>  _S2268;
    _S2268[int(0)] = 0.0f;
    _S2268[int(1)] = 0.0f;
    _S2268[int(2)] = 0.0f;
    _S2268[int(3)] = 0.0f;
    _S2268[int(4)] = 0.0f;
    _S2268[int(5)] = 0.0f;
    _S2268[int(6)] = 0.0f;
    _S2268[int(7)] = 0.0f;
    _S2268[int(8)] = 0.0f;
    _S2268[int(9)] = 0.0f;
    _S2268[int(10)] = 0.0f;
    _S2268[int(11)] = 0.0f;
    _S2268[int(12)] = 0.0f;
    _S2268[int(13)] = 0.0f;
    _S2268[int(14)] = 0.0f;
    _S2268[int(15)] = 0.0f;
    _S2268[int(16)] = 0.0f;
    _S2268[int(17)] = 0.0f;
    _S2268[int(18)] = 0.0f;
    _S2268[int(19)] = 0.0f;
    _S2268[int(20)] = 0.0f;
    _S2268[int(21)] = 0.0f;
    _S2268[int(0)] = _S2267;
    FixedArray<float, 22>  _S2269 = {
        _S2242 + _S2268[int(0)], _S2243 + _S2268[int(1)], _S2244 + _S2268[int(2)], _S2245 + _S2268[int(3)], _S2246 + _S2268[int(4)], _S2247 + _S2268[int(5)], _S2248 + _S2268[int(6)], _S2249 + _S2268[int(7)], _S2250 + _S2268[int(8)], _S2251 + _S2268[int(9)], _S2252 + _S2268[int(10)], _S2253 + _S2268[int(11)], _S2254 + _S2268[int(12)], _S2255 + _S2268[int(13)], _S2256 + _S2268[int(14)], _S2257 + _S2268[int(15)], _S2258 + _S2268[int(16)], _S2259 + _S2268[int(17)], _S2260 + _S2268[int(18)], _S2261 + _S2268[int(19)], _S2262 + _S2268[int(20)], _S2263 + _S2268[int(21)]
    };
    dpraw_losses_0->primal_0 = dpraw_losses_0->primal_0;
    dpraw_losses_0->differential_0 = _S2269;
    return;
}

inline __device__ void s_bwd_compute_ppisp_regularization_loss_0(DiffPair_arrayx3Cfloatx2C22x3E_0 * _S2270, int _S2271, FixedArray<float, 6>  * _S2272, FixedArray<float, 6>  * _S2273)
{
    s_bwd_prop_compute_ppisp_regularization_loss_0(_S2270, _S2271, _S2272, _S2273);
    return;
}

inline __device__ void compute_ppisp_regularization_loss_vjp(FixedArray<float, 22>  raw_losses_2, int num_cameras_3, FixedArray<float, 6>  loss_weights_3, FixedArray<float, 6>  grad_out_6, FixedArray<float, 22>  * _S2274)
{
    FixedArray<float, 22>  _S2275 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    DiffPair_arrayx3Cfloatx2C22x3E_0 dp_raw_losses_0;
    (&dp_raw_losses_0)->primal_0 = raw_losses_2;
    (&dp_raw_losses_0)->differential_0 = _S2275;
    FixedArray<float, 6>  _S2276 = loss_weights_3;
    FixedArray<float, 6>  _S2277 = grad_out_6;
    s_bwd_compute_ppisp_regularization_loss_0(&dp_raw_losses_0, num_cameras_3, &_S2276, &_S2277);
    *_S2274 = (&dp_raw_losses_0)->differential_0;
    return;
}

struct DiffPair_arrayx3Cfloatx2C23x3E_0
{
    FixedArray<float, 23>  primal_0;
    FixedArray<float, 23>  differential_0;
};

inline __device__ void s_bwd_prop_compute_ppisp_rqs_regularization_loss_0(DiffPair_arrayx3Cfloatx2C23x3E_0 * dpraw_losses_1, int num_cameras_4, FixedArray<float, 6>  * loss_weights_4, FixedArray<float, 6>  * _s_dOut_7)
{
    FixedArray<float, 23>  _S2278 = dpraw_losses_1->primal_0;
    float _S2279 = float(num_cameras_4);
    float _S2280 = dpraw_losses_1->primal_0[int(0)] / _S2279;
    bool _S2281 = (s_primal_ctx_abs_0(_S2280)) < 0.10000000149011612f;
    float _S2282;
    if(_S2281)
    {
        _S2282 = 0.5f * _S2280;
    }
    else
    {
        _S2282 = 0.0f;
    }
    float _S2283 = 3.0f * _S2279;
    float _S2284 = 9.0f * _S2279;
    float _S2285 = 5.0f * _S2279;
    float _S2286 = _S2278[int(10)] / _S2279;
    bool _S2287 = (s_primal_ctx_abs_0(_S2286)) < 0.00499999988824129f;
    float _S2288;
    if(_S2287)
    {
        _S2288 = 0.5f * _S2286;
    }
    else
    {
        _S2288 = 0.0f;
    }
    float _S2289 = _S2278[int(11)] / _S2279;
    bool _S2290 = (s_primal_ctx_abs_0(_S2289)) < 0.00499999988824129f;
    float _S2291;
    if(_S2290)
    {
        _S2291 = 0.5f * _S2289;
    }
    else
    {
        _S2291 = 0.0f;
    }
    float _S2292 = _S2278[int(12)] / _S2279;
    bool _S2293 = (s_primal_ctx_abs_0(_S2292)) < 0.00499999988824129f;
    float _S2294;
    if(_S2293)
    {
        _S2294 = 0.5f * _S2292;
    }
    else
    {
        _S2294 = 0.0f;
    }
    float _S2295 = _S2278[int(13)] / _S2279;
    bool _S2296 = (s_primal_ctx_abs_0(_S2295)) < 0.00499999988824129f;
    float _S2297;
    if(_S2296)
    {
        _S2297 = 0.5f * _S2295;
    }
    else
    {
        _S2297 = 0.0f;
    }
    float _S2298 = _S2278[int(14)] / _S2279;
    bool _S2299 = (s_primal_ctx_abs_0(_S2298)) < 0.00499999988824129f;
    float _S2300;
    if(_S2299)
    {
        _S2300 = 0.5f * _S2298;
    }
    else
    {
        _S2300 = 0.0f;
    }
    float _S2301 = _S2278[int(15)] / _S2279;
    bool _S2302 = (s_primal_ctx_abs_0(_S2301)) < 0.00499999988824129f;
    float _S2303;
    if(_S2302)
    {
        _S2303 = 0.5f * _S2301;
    }
    else
    {
        _S2303 = 0.0f;
    }
    float _S2304 = _S2278[int(16)] / _S2279;
    bool _S2305 = (s_primal_ctx_abs_0(_S2304)) < 0.00499999988824129f;
    float _S2306;
    if(_S2305)
    {
        _S2306 = 0.5f * _S2304;
    }
    else
    {
        _S2306 = 0.0f;
    }
    float _S2307 = _S2278[int(17)] / _S2279;
    bool _S2308 = (s_primal_ctx_abs_0(_S2307)) < 0.00499999988824129f;
    float _S2309;
    if(_S2308)
    {
        _S2309 = 0.5f * _S2307;
    }
    else
    {
        _S2309 = 0.0f;
    }
    float _S2310 = (*loss_weights_4)[int(3)] * (*_s_dOut_7)[int(3)];
    float _S2311 = (*loss_weights_4)[int(2)] * (*_s_dOut_7)[int(2)];
    float _S2312 = (*loss_weights_4)[int(1)] * (*_s_dOut_7)[int(1)];
    float _S2313 = (*loss_weights_4)[int(0)] * (*_s_dOut_7)[int(0)];
    float _S2314 = (*loss_weights_4)[int(5)] * (*_s_dOut_7)[int(5)] / _S2285;
    float _S2315 = 0.125f * ((*loss_weights_4)[int(4)] * (*_s_dOut_7)[int(4)]);
    FixedArray<float, 23>  _S2316;
    _S2316[int(0)] = 0.0f;
    _S2316[int(1)] = 0.0f;
    _S2316[int(2)] = 0.0f;
    _S2316[int(3)] = 0.0f;
    _S2316[int(4)] = 0.0f;
    _S2316[int(5)] = 0.0f;
    _S2316[int(6)] = 0.0f;
    _S2316[int(7)] = 0.0f;
    _S2316[int(8)] = 0.0f;
    _S2316[int(9)] = 0.0f;
    _S2316[int(10)] = 0.0f;
    _S2316[int(11)] = 0.0f;
    _S2316[int(12)] = 0.0f;
    _S2316[int(13)] = 0.0f;
    _S2316[int(14)] = 0.0f;
    _S2316[int(15)] = 0.0f;
    _S2316[int(16)] = 0.0f;
    _S2316[int(17)] = 0.0f;
    _S2316[int(18)] = 0.0f;
    _S2316[int(19)] = 0.0f;
    _S2316[int(20)] = 0.0f;
    _S2316[int(21)] = 0.0f;
    _S2316[int(22)] = 0.0f;
    _S2316[int(22)] = _S2314;
    _S2316[int(21)] = _S2314;
    _S2316[int(20)] = _S2314;
    _S2316[int(19)] = _S2314;
    _S2316[int(18)] = _S2314;
    float _S2317 = _S2316[int(0)];
    float _S2318 = _S2316[int(1)];
    float _S2319 = _S2316[int(2)];
    float _S2320 = _S2316[int(3)];
    float _S2321 = _S2316[int(4)];
    float _S2322 = _S2316[int(5)];
    float _S2323 = _S2316[int(6)];
    float _S2324 = _S2316[int(7)];
    float _S2325 = _S2316[int(8)];
    float _S2326 = _S2316[int(9)];
    float _S2327 = _S2316[int(10)];
    float _S2328 = _S2316[int(11)];
    float _S2329 = _S2316[int(12)];
    float _S2330 = _S2316[int(13)];
    float _S2331 = _S2316[int(14)];
    float _S2332 = _S2316[int(15)];
    float _S2333 = _S2316[int(16)];
    float _S2334 = _S2316[int(17)];
    float _S2335 = _S2316[int(18)];
    float _S2336 = _S2316[int(19)];
    float _S2337 = _S2316[int(20)];
    float _S2338 = _S2316[int(21)];
    float _S2339 = _S2316[int(22)];
    float _S2340;
    if(_S2308)
    {
        float _S2341 = 200.0f * _S2315;
        float _S2342 = _S2309 * _S2341 + 0.5f * (_S2307 * _S2341);
        _S2309 = 0.0f;
        _S2340 = _S2342;
    }
    else
    {
        _S2309 = _S2315;
        _S2340 = 0.0f;
    }
    DiffPair_float_0 _S2343;
    (&_S2343)->primal_0 = _S2307;
    (&_S2343)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2343, _S2309);
    float _S2344 = (_S2343.differential_0 + _S2340) / _S2279;
    FixedArray<float, 23>  _S2345;
    _S2345[int(0)] = 0.0f;
    _S2345[int(1)] = 0.0f;
    _S2345[int(2)] = 0.0f;
    _S2345[int(3)] = 0.0f;
    _S2345[int(4)] = 0.0f;
    _S2345[int(5)] = 0.0f;
    _S2345[int(6)] = 0.0f;
    _S2345[int(7)] = 0.0f;
    _S2345[int(8)] = 0.0f;
    _S2345[int(9)] = 0.0f;
    _S2345[int(10)] = 0.0f;
    _S2345[int(11)] = 0.0f;
    _S2345[int(12)] = 0.0f;
    _S2345[int(13)] = 0.0f;
    _S2345[int(14)] = 0.0f;
    _S2345[int(15)] = 0.0f;
    _S2345[int(16)] = 0.0f;
    _S2345[int(17)] = 0.0f;
    _S2345[int(18)] = 0.0f;
    _S2345[int(19)] = 0.0f;
    _S2345[int(20)] = 0.0f;
    _S2345[int(21)] = 0.0f;
    _S2345[int(22)] = 0.0f;
    _S2345[int(17)] = _S2344;
    float _S2346 = _S2317 + _S2345[int(0)];
    float _S2347 = _S2318 + _S2345[int(1)];
    float _S2348 = _S2319 + _S2345[int(2)];
    float _S2349 = _S2320 + _S2345[int(3)];
    float _S2350 = _S2321 + _S2345[int(4)];
    float _S2351 = _S2322 + _S2345[int(5)];
    float _S2352 = _S2323 + _S2345[int(6)];
    float _S2353 = _S2324 + _S2345[int(7)];
    float _S2354 = _S2325 + _S2345[int(8)];
    float _S2355 = _S2326 + _S2345[int(9)];
    float _S2356 = _S2327 + _S2345[int(10)];
    float _S2357 = _S2328 + _S2345[int(11)];
    float _S2358 = _S2329 + _S2345[int(12)];
    float _S2359 = _S2330 + _S2345[int(13)];
    float _S2360 = _S2331 + _S2345[int(14)];
    float _S2361 = _S2332 + _S2345[int(15)];
    float _S2362 = _S2333 + _S2345[int(16)];
    float _S2363 = _S2334 + _S2345[int(17)];
    float _S2364 = _S2335 + _S2345[int(18)];
    float _S2365 = _S2336 + _S2345[int(19)];
    float _S2366 = _S2337 + _S2345[int(20)];
    float _S2367 = _S2338 + _S2345[int(21)];
    float _S2368 = _S2339 + _S2345[int(22)];
    if(_S2305)
    {
        float _S2369 = 200.0f * _S2315;
        float _S2370 = _S2306 * _S2369 + 0.5f * (_S2304 * _S2369);
        _S2306 = 0.0f;
        _S2309 = _S2370;
    }
    else
    {
        _S2306 = _S2315;
        _S2309 = 0.0f;
    }
    DiffPair_float_0 _S2371;
    (&_S2371)->primal_0 = _S2304;
    (&_S2371)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2371, _S2306);
    float _S2372 = (_S2371.differential_0 + _S2309) / _S2279;
    FixedArray<float, 23>  _S2373;
    _S2373[int(0)] = 0.0f;
    _S2373[int(1)] = 0.0f;
    _S2373[int(2)] = 0.0f;
    _S2373[int(3)] = 0.0f;
    _S2373[int(4)] = 0.0f;
    _S2373[int(5)] = 0.0f;
    _S2373[int(6)] = 0.0f;
    _S2373[int(7)] = 0.0f;
    _S2373[int(8)] = 0.0f;
    _S2373[int(9)] = 0.0f;
    _S2373[int(10)] = 0.0f;
    _S2373[int(11)] = 0.0f;
    _S2373[int(12)] = 0.0f;
    _S2373[int(13)] = 0.0f;
    _S2373[int(14)] = 0.0f;
    _S2373[int(15)] = 0.0f;
    _S2373[int(16)] = 0.0f;
    _S2373[int(17)] = 0.0f;
    _S2373[int(18)] = 0.0f;
    _S2373[int(19)] = 0.0f;
    _S2373[int(20)] = 0.0f;
    _S2373[int(21)] = 0.0f;
    _S2373[int(22)] = 0.0f;
    _S2373[int(16)] = _S2372;
    float _S2374 = _S2346 + _S2373[int(0)];
    float _S2375 = _S2347 + _S2373[int(1)];
    float _S2376 = _S2348 + _S2373[int(2)];
    float _S2377 = _S2349 + _S2373[int(3)];
    float _S2378 = _S2350 + _S2373[int(4)];
    float _S2379 = _S2351 + _S2373[int(5)];
    float _S2380 = _S2352 + _S2373[int(6)];
    float _S2381 = _S2353 + _S2373[int(7)];
    float _S2382 = _S2354 + _S2373[int(8)];
    float _S2383 = _S2355 + _S2373[int(9)];
    float _S2384 = _S2356 + _S2373[int(10)];
    float _S2385 = _S2357 + _S2373[int(11)];
    float _S2386 = _S2358 + _S2373[int(12)];
    float _S2387 = _S2359 + _S2373[int(13)];
    float _S2388 = _S2360 + _S2373[int(14)];
    float _S2389 = _S2361 + _S2373[int(15)];
    float _S2390 = _S2362 + _S2373[int(16)];
    float _S2391 = _S2363 + _S2373[int(17)];
    float _S2392 = _S2364 + _S2373[int(18)];
    float _S2393 = _S2365 + _S2373[int(19)];
    float _S2394 = _S2366 + _S2373[int(20)];
    float _S2395 = _S2367 + _S2373[int(21)];
    float _S2396 = _S2368 + _S2373[int(22)];
    if(_S2302)
    {
        float _S2397 = 200.0f * _S2315;
        float _S2398 = _S2303 * _S2397 + 0.5f * (_S2301 * _S2397);
        _S2303 = 0.0f;
        _S2306 = _S2398;
    }
    else
    {
        _S2303 = _S2315;
        _S2306 = 0.0f;
    }
    DiffPair_float_0 _S2399;
    (&_S2399)->primal_0 = _S2301;
    (&_S2399)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2399, _S2303);
    float _S2400 = (_S2399.differential_0 + _S2306) / _S2279;
    FixedArray<float, 23>  _S2401;
    _S2401[int(0)] = 0.0f;
    _S2401[int(1)] = 0.0f;
    _S2401[int(2)] = 0.0f;
    _S2401[int(3)] = 0.0f;
    _S2401[int(4)] = 0.0f;
    _S2401[int(5)] = 0.0f;
    _S2401[int(6)] = 0.0f;
    _S2401[int(7)] = 0.0f;
    _S2401[int(8)] = 0.0f;
    _S2401[int(9)] = 0.0f;
    _S2401[int(10)] = 0.0f;
    _S2401[int(11)] = 0.0f;
    _S2401[int(12)] = 0.0f;
    _S2401[int(13)] = 0.0f;
    _S2401[int(14)] = 0.0f;
    _S2401[int(15)] = 0.0f;
    _S2401[int(16)] = 0.0f;
    _S2401[int(17)] = 0.0f;
    _S2401[int(18)] = 0.0f;
    _S2401[int(19)] = 0.0f;
    _S2401[int(20)] = 0.0f;
    _S2401[int(21)] = 0.0f;
    _S2401[int(22)] = 0.0f;
    _S2401[int(15)] = _S2400;
    float _S2402 = _S2374 + _S2401[int(0)];
    float _S2403 = _S2375 + _S2401[int(1)];
    float _S2404 = _S2376 + _S2401[int(2)];
    float _S2405 = _S2377 + _S2401[int(3)];
    float _S2406 = _S2378 + _S2401[int(4)];
    float _S2407 = _S2379 + _S2401[int(5)];
    float _S2408 = _S2380 + _S2401[int(6)];
    float _S2409 = _S2381 + _S2401[int(7)];
    float _S2410 = _S2382 + _S2401[int(8)];
    float _S2411 = _S2383 + _S2401[int(9)];
    float _S2412 = _S2384 + _S2401[int(10)];
    float _S2413 = _S2385 + _S2401[int(11)];
    float _S2414 = _S2386 + _S2401[int(12)];
    float _S2415 = _S2387 + _S2401[int(13)];
    float _S2416 = _S2388 + _S2401[int(14)];
    float _S2417 = _S2389 + _S2401[int(15)];
    float _S2418 = _S2390 + _S2401[int(16)];
    float _S2419 = _S2391 + _S2401[int(17)];
    float _S2420 = _S2392 + _S2401[int(18)];
    float _S2421 = _S2393 + _S2401[int(19)];
    float _S2422 = _S2394 + _S2401[int(20)];
    float _S2423 = _S2395 + _S2401[int(21)];
    float _S2424 = _S2396 + _S2401[int(22)];
    if(_S2299)
    {
        float _S2425 = 200.0f * _S2315;
        float _S2426 = _S2300 * _S2425 + 0.5f * (_S2298 * _S2425);
        _S2300 = 0.0f;
        _S2303 = _S2426;
    }
    else
    {
        _S2300 = _S2315;
        _S2303 = 0.0f;
    }
    DiffPair_float_0 _S2427;
    (&_S2427)->primal_0 = _S2298;
    (&_S2427)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2427, _S2300);
    float _S2428 = (_S2427.differential_0 + _S2303) / _S2279;
    FixedArray<float, 23>  _S2429;
    _S2429[int(0)] = 0.0f;
    _S2429[int(1)] = 0.0f;
    _S2429[int(2)] = 0.0f;
    _S2429[int(3)] = 0.0f;
    _S2429[int(4)] = 0.0f;
    _S2429[int(5)] = 0.0f;
    _S2429[int(6)] = 0.0f;
    _S2429[int(7)] = 0.0f;
    _S2429[int(8)] = 0.0f;
    _S2429[int(9)] = 0.0f;
    _S2429[int(10)] = 0.0f;
    _S2429[int(11)] = 0.0f;
    _S2429[int(12)] = 0.0f;
    _S2429[int(13)] = 0.0f;
    _S2429[int(14)] = 0.0f;
    _S2429[int(15)] = 0.0f;
    _S2429[int(16)] = 0.0f;
    _S2429[int(17)] = 0.0f;
    _S2429[int(18)] = 0.0f;
    _S2429[int(19)] = 0.0f;
    _S2429[int(20)] = 0.0f;
    _S2429[int(21)] = 0.0f;
    _S2429[int(22)] = 0.0f;
    _S2429[int(14)] = _S2428;
    float _S2430 = _S2402 + _S2429[int(0)];
    float _S2431 = _S2403 + _S2429[int(1)];
    float _S2432 = _S2404 + _S2429[int(2)];
    float _S2433 = _S2405 + _S2429[int(3)];
    float _S2434 = _S2406 + _S2429[int(4)];
    float _S2435 = _S2407 + _S2429[int(5)];
    float _S2436 = _S2408 + _S2429[int(6)];
    float _S2437 = _S2409 + _S2429[int(7)];
    float _S2438 = _S2410 + _S2429[int(8)];
    float _S2439 = _S2411 + _S2429[int(9)];
    float _S2440 = _S2412 + _S2429[int(10)];
    float _S2441 = _S2413 + _S2429[int(11)];
    float _S2442 = _S2414 + _S2429[int(12)];
    float _S2443 = _S2415 + _S2429[int(13)];
    float _S2444 = _S2416 + _S2429[int(14)];
    float _S2445 = _S2417 + _S2429[int(15)];
    float _S2446 = _S2418 + _S2429[int(16)];
    float _S2447 = _S2419 + _S2429[int(17)];
    float _S2448 = _S2420 + _S2429[int(18)];
    float _S2449 = _S2421 + _S2429[int(19)];
    float _S2450 = _S2422 + _S2429[int(20)];
    float _S2451 = _S2423 + _S2429[int(21)];
    float _S2452 = _S2424 + _S2429[int(22)];
    if(_S2296)
    {
        float _S2453 = 200.0f * _S2315;
        float _S2454 = _S2297 * _S2453 + 0.5f * (_S2295 * _S2453);
        _S2297 = 0.0f;
        _S2300 = _S2454;
    }
    else
    {
        _S2297 = _S2315;
        _S2300 = 0.0f;
    }
    DiffPair_float_0 _S2455;
    (&_S2455)->primal_0 = _S2295;
    (&_S2455)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2455, _S2297);
    float _S2456 = (_S2455.differential_0 + _S2300) / _S2279;
    FixedArray<float, 23>  _S2457;
    _S2457[int(0)] = 0.0f;
    _S2457[int(1)] = 0.0f;
    _S2457[int(2)] = 0.0f;
    _S2457[int(3)] = 0.0f;
    _S2457[int(4)] = 0.0f;
    _S2457[int(5)] = 0.0f;
    _S2457[int(6)] = 0.0f;
    _S2457[int(7)] = 0.0f;
    _S2457[int(8)] = 0.0f;
    _S2457[int(9)] = 0.0f;
    _S2457[int(10)] = 0.0f;
    _S2457[int(11)] = 0.0f;
    _S2457[int(12)] = 0.0f;
    _S2457[int(13)] = 0.0f;
    _S2457[int(14)] = 0.0f;
    _S2457[int(15)] = 0.0f;
    _S2457[int(16)] = 0.0f;
    _S2457[int(17)] = 0.0f;
    _S2457[int(18)] = 0.0f;
    _S2457[int(19)] = 0.0f;
    _S2457[int(20)] = 0.0f;
    _S2457[int(21)] = 0.0f;
    _S2457[int(22)] = 0.0f;
    _S2457[int(13)] = _S2456;
    float _S2458 = _S2430 + _S2457[int(0)];
    float _S2459 = _S2431 + _S2457[int(1)];
    float _S2460 = _S2432 + _S2457[int(2)];
    float _S2461 = _S2433 + _S2457[int(3)];
    float _S2462 = _S2434 + _S2457[int(4)];
    float _S2463 = _S2435 + _S2457[int(5)];
    float _S2464 = _S2436 + _S2457[int(6)];
    float _S2465 = _S2437 + _S2457[int(7)];
    float _S2466 = _S2438 + _S2457[int(8)];
    float _S2467 = _S2439 + _S2457[int(9)];
    float _S2468 = _S2440 + _S2457[int(10)];
    float _S2469 = _S2441 + _S2457[int(11)];
    float _S2470 = _S2442 + _S2457[int(12)];
    float _S2471 = _S2443 + _S2457[int(13)];
    float _S2472 = _S2444 + _S2457[int(14)];
    float _S2473 = _S2445 + _S2457[int(15)];
    float _S2474 = _S2446 + _S2457[int(16)];
    float _S2475 = _S2447 + _S2457[int(17)];
    float _S2476 = _S2448 + _S2457[int(18)];
    float _S2477 = _S2449 + _S2457[int(19)];
    float _S2478 = _S2450 + _S2457[int(20)];
    float _S2479 = _S2451 + _S2457[int(21)];
    float _S2480 = _S2452 + _S2457[int(22)];
    if(_S2293)
    {
        float _S2481 = 200.0f * _S2315;
        float _S2482 = _S2294 * _S2481 + 0.5f * (_S2292 * _S2481);
        _S2294 = 0.0f;
        _S2297 = _S2482;
    }
    else
    {
        _S2294 = _S2315;
        _S2297 = 0.0f;
    }
    DiffPair_float_0 _S2483;
    (&_S2483)->primal_0 = _S2292;
    (&_S2483)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2483, _S2294);
    float _S2484 = (_S2483.differential_0 + _S2297) / _S2279;
    FixedArray<float, 23>  _S2485;
    _S2485[int(0)] = 0.0f;
    _S2485[int(1)] = 0.0f;
    _S2485[int(2)] = 0.0f;
    _S2485[int(3)] = 0.0f;
    _S2485[int(4)] = 0.0f;
    _S2485[int(5)] = 0.0f;
    _S2485[int(6)] = 0.0f;
    _S2485[int(7)] = 0.0f;
    _S2485[int(8)] = 0.0f;
    _S2485[int(9)] = 0.0f;
    _S2485[int(10)] = 0.0f;
    _S2485[int(11)] = 0.0f;
    _S2485[int(12)] = 0.0f;
    _S2485[int(13)] = 0.0f;
    _S2485[int(14)] = 0.0f;
    _S2485[int(15)] = 0.0f;
    _S2485[int(16)] = 0.0f;
    _S2485[int(17)] = 0.0f;
    _S2485[int(18)] = 0.0f;
    _S2485[int(19)] = 0.0f;
    _S2485[int(20)] = 0.0f;
    _S2485[int(21)] = 0.0f;
    _S2485[int(22)] = 0.0f;
    _S2485[int(12)] = _S2484;
    float _S2486 = _S2458 + _S2485[int(0)];
    float _S2487 = _S2459 + _S2485[int(1)];
    float _S2488 = _S2460 + _S2485[int(2)];
    float _S2489 = _S2461 + _S2485[int(3)];
    float _S2490 = _S2462 + _S2485[int(4)];
    float _S2491 = _S2463 + _S2485[int(5)];
    float _S2492 = _S2464 + _S2485[int(6)];
    float _S2493 = _S2465 + _S2485[int(7)];
    float _S2494 = _S2466 + _S2485[int(8)];
    float _S2495 = _S2467 + _S2485[int(9)];
    float _S2496 = _S2468 + _S2485[int(10)];
    float _S2497 = _S2469 + _S2485[int(11)];
    float _S2498 = _S2470 + _S2485[int(12)];
    float _S2499 = _S2471 + _S2485[int(13)];
    float _S2500 = _S2472 + _S2485[int(14)];
    float _S2501 = _S2473 + _S2485[int(15)];
    float _S2502 = _S2474 + _S2485[int(16)];
    float _S2503 = _S2475 + _S2485[int(17)];
    float _S2504 = _S2476 + _S2485[int(18)];
    float _S2505 = _S2477 + _S2485[int(19)];
    float _S2506 = _S2478 + _S2485[int(20)];
    float _S2507 = _S2479 + _S2485[int(21)];
    float _S2508 = _S2480 + _S2485[int(22)];
    if(_S2290)
    {
        float _S2509 = 200.0f * _S2315;
        float _S2510 = _S2291 * _S2509 + 0.5f * (_S2289 * _S2509);
        _S2291 = 0.0f;
        _S2294 = _S2510;
    }
    else
    {
        _S2291 = _S2315;
        _S2294 = 0.0f;
    }
    DiffPair_float_0 _S2511;
    (&_S2511)->primal_0 = _S2289;
    (&_S2511)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2511, _S2291);
    float _S2512 = (_S2511.differential_0 + _S2294) / _S2279;
    FixedArray<float, 23>  _S2513;
    _S2513[int(0)] = 0.0f;
    _S2513[int(1)] = 0.0f;
    _S2513[int(2)] = 0.0f;
    _S2513[int(3)] = 0.0f;
    _S2513[int(4)] = 0.0f;
    _S2513[int(5)] = 0.0f;
    _S2513[int(6)] = 0.0f;
    _S2513[int(7)] = 0.0f;
    _S2513[int(8)] = 0.0f;
    _S2513[int(9)] = 0.0f;
    _S2513[int(10)] = 0.0f;
    _S2513[int(11)] = 0.0f;
    _S2513[int(12)] = 0.0f;
    _S2513[int(13)] = 0.0f;
    _S2513[int(14)] = 0.0f;
    _S2513[int(15)] = 0.0f;
    _S2513[int(16)] = 0.0f;
    _S2513[int(17)] = 0.0f;
    _S2513[int(18)] = 0.0f;
    _S2513[int(19)] = 0.0f;
    _S2513[int(20)] = 0.0f;
    _S2513[int(21)] = 0.0f;
    _S2513[int(22)] = 0.0f;
    _S2513[int(11)] = _S2512;
    float _S2514 = _S2486 + _S2513[int(0)];
    float _S2515 = _S2487 + _S2513[int(1)];
    float _S2516 = _S2488 + _S2513[int(2)];
    float _S2517 = _S2489 + _S2513[int(3)];
    float _S2518 = _S2490 + _S2513[int(4)];
    float _S2519 = _S2491 + _S2513[int(5)];
    float _S2520 = _S2492 + _S2513[int(6)];
    float _S2521 = _S2493 + _S2513[int(7)];
    float _S2522 = _S2494 + _S2513[int(8)];
    float _S2523 = _S2495 + _S2513[int(9)];
    float _S2524 = _S2496 + _S2513[int(10)];
    float _S2525 = _S2497 + _S2513[int(11)];
    float _S2526 = _S2498 + _S2513[int(12)];
    float _S2527 = _S2499 + _S2513[int(13)];
    float _S2528 = _S2500 + _S2513[int(14)];
    float _S2529 = _S2501 + _S2513[int(15)];
    float _S2530 = _S2502 + _S2513[int(16)];
    float _S2531 = _S2503 + _S2513[int(17)];
    float _S2532 = _S2504 + _S2513[int(18)];
    float _S2533 = _S2505 + _S2513[int(19)];
    float _S2534 = _S2506 + _S2513[int(20)];
    float _S2535 = _S2507 + _S2513[int(21)];
    float _S2536 = _S2508 + _S2513[int(22)];
    if(_S2287)
    {
        float _S2537 = 200.0f * _S2315;
        float _S2538 = _S2288 * _S2537 + 0.5f * (_S2286 * _S2537);
        _S2288 = 0.0f;
        _S2291 = _S2538;
    }
    else
    {
        _S2288 = _S2315;
        _S2291 = 0.0f;
    }
    DiffPair_float_0 _S2539;
    (&_S2539)->primal_0 = _S2286;
    (&_S2539)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2539, _S2288);
    float _S2540 = (_S2539.differential_0 + _S2291) / _S2279;
    float _S2541 = _S2310 / _S2285;
    float _S2542 = _S2311 / _S2284;
    float _S2543 = _S2312 / _S2283;
    FixedArray<float, 23>  _S2544;
    _S2544[int(0)] = 0.0f;
    _S2544[int(1)] = 0.0f;
    _S2544[int(2)] = 0.0f;
    _S2544[int(3)] = 0.0f;
    _S2544[int(4)] = 0.0f;
    _S2544[int(5)] = 0.0f;
    _S2544[int(6)] = 0.0f;
    _S2544[int(7)] = 0.0f;
    _S2544[int(8)] = 0.0f;
    _S2544[int(9)] = 0.0f;
    _S2544[int(10)] = 0.0f;
    _S2544[int(11)] = 0.0f;
    _S2544[int(12)] = 0.0f;
    _S2544[int(13)] = 0.0f;
    _S2544[int(14)] = 0.0f;
    _S2544[int(15)] = 0.0f;
    _S2544[int(16)] = 0.0f;
    _S2544[int(17)] = 0.0f;
    _S2544[int(18)] = 0.0f;
    _S2544[int(19)] = 0.0f;
    _S2544[int(20)] = 0.0f;
    _S2544[int(21)] = 0.0f;
    _S2544[int(22)] = 0.0f;
    _S2544[int(10)] = _S2540;
    _S2544[int(9)] = _S2541;
    _S2544[int(8)] = _S2541;
    _S2544[int(7)] = _S2541;
    _S2544[int(6)] = _S2541;
    _S2544[int(5)] = _S2541;
    _S2544[int(4)] = _S2542;
    _S2544[int(3)] = _S2542;
    _S2544[int(2)] = _S2542;
    _S2544[int(1)] = _S2543;
    float _S2545 = _S2514 + _S2544[int(0)];
    float _S2546 = _S2515 + _S2544[int(1)];
    float _S2547 = _S2516 + _S2544[int(2)];
    float _S2548 = _S2517 + _S2544[int(3)];
    float _S2549 = _S2518 + _S2544[int(4)];
    float _S2550 = _S2519 + _S2544[int(5)];
    float _S2551 = _S2520 + _S2544[int(6)];
    float _S2552 = _S2521 + _S2544[int(7)];
    float _S2553 = _S2522 + _S2544[int(8)];
    float _S2554 = _S2523 + _S2544[int(9)];
    float _S2555 = _S2524 + _S2544[int(10)];
    float _S2556 = _S2525 + _S2544[int(11)];
    float _S2557 = _S2526 + _S2544[int(12)];
    float _S2558 = _S2527 + _S2544[int(13)];
    float _S2559 = _S2528 + _S2544[int(14)];
    float _S2560 = _S2529 + _S2544[int(15)];
    float _S2561 = _S2530 + _S2544[int(16)];
    float _S2562 = _S2531 + _S2544[int(17)];
    float _S2563 = _S2532 + _S2544[int(18)];
    float _S2564 = _S2533 + _S2544[int(19)];
    float _S2565 = _S2534 + _S2544[int(20)];
    float _S2566 = _S2535 + _S2544[int(21)];
    float _S2567 = _S2536 + _S2544[int(22)];
    if(_S2281)
    {
        float _S2568 = 10.0f * _S2313;
        float _S2569 = _S2282 * _S2568 + 0.5f * (_S2280 * _S2568);
        _S2282 = 0.0f;
        _S2288 = _S2569;
    }
    else
    {
        _S2282 = _S2313;
        _S2288 = 0.0f;
    }
    DiffPair_float_0 _S2570;
    (&_S2570)->primal_0 = _S2280;
    (&_S2570)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2570, _S2282);
    float _S2571 = (_S2570.differential_0 + _S2288) / _S2279;
    FixedArray<float, 23>  _S2572;
    _S2572[int(0)] = 0.0f;
    _S2572[int(1)] = 0.0f;
    _S2572[int(2)] = 0.0f;
    _S2572[int(3)] = 0.0f;
    _S2572[int(4)] = 0.0f;
    _S2572[int(5)] = 0.0f;
    _S2572[int(6)] = 0.0f;
    _S2572[int(7)] = 0.0f;
    _S2572[int(8)] = 0.0f;
    _S2572[int(9)] = 0.0f;
    _S2572[int(10)] = 0.0f;
    _S2572[int(11)] = 0.0f;
    _S2572[int(12)] = 0.0f;
    _S2572[int(13)] = 0.0f;
    _S2572[int(14)] = 0.0f;
    _S2572[int(15)] = 0.0f;
    _S2572[int(16)] = 0.0f;
    _S2572[int(17)] = 0.0f;
    _S2572[int(18)] = 0.0f;
    _S2572[int(19)] = 0.0f;
    _S2572[int(20)] = 0.0f;
    _S2572[int(21)] = 0.0f;
    _S2572[int(22)] = 0.0f;
    _S2572[int(0)] = _S2571;
    FixedArray<float, 23>  _S2573 = {
        _S2545 + _S2572[int(0)], _S2546 + _S2572[int(1)], _S2547 + _S2572[int(2)], _S2548 + _S2572[int(3)], _S2549 + _S2572[int(4)], _S2550 + _S2572[int(5)], _S2551 + _S2572[int(6)], _S2552 + _S2572[int(7)], _S2553 + _S2572[int(8)], _S2554 + _S2572[int(9)], _S2555 + _S2572[int(10)], _S2556 + _S2572[int(11)], _S2557 + _S2572[int(12)], _S2558 + _S2572[int(13)], _S2559 + _S2572[int(14)], _S2560 + _S2572[int(15)], _S2561 + _S2572[int(16)], _S2562 + _S2572[int(17)], _S2563 + _S2572[int(18)], _S2564 + _S2572[int(19)], _S2565 + _S2572[int(20)], _S2566 + _S2572[int(21)], _S2567 + _S2572[int(22)]
    };
    dpraw_losses_1->primal_0 = dpraw_losses_1->primal_0;
    dpraw_losses_1->differential_0 = _S2573;
    return;
}

inline __device__ void s_bwd_compute_ppisp_rqs_regularization_loss_0(DiffPair_arrayx3Cfloatx2C23x3E_0 * _S2574, int _S2575, FixedArray<float, 6>  * _S2576, FixedArray<float, 6>  * _S2577)
{
    s_bwd_prop_compute_ppisp_rqs_regularization_loss_0(_S2574, _S2575, _S2576, _S2577);
    return;
}

inline __device__ void compute_ppisp_rqs_regularization_loss_vjp(FixedArray<float, 23>  raw_losses_3, int num_cameras_5, FixedArray<float, 6>  loss_weights_5, FixedArray<float, 6>  grad_out_7, FixedArray<float, 23>  * _S2578)
{
    FixedArray<float, 23>  _S2579 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    DiffPair_arrayx3Cfloatx2C23x3E_0 dp_raw_losses_1;
    (&dp_raw_losses_1)->primal_0 = raw_losses_3;
    (&dp_raw_losses_1)->differential_0 = _S2579;
    FixedArray<float, 6>  _S2580 = loss_weights_5;
    FixedArray<float, 6>  _S2581 = grad_out_7;
    s_bwd_compute_ppisp_rqs_regularization_loss_0(&dp_raw_losses_1, num_cameras_5, &_S2580, &_S2581);
    *_S2578 = (&dp_raw_losses_1)->differential_0;
    return;
}

inline __device__ void compute_ppisp_no_crf_regularization_loss(FixedArray<float, 18>  raw_losses_4, int num_cameras_6, FixedArray<float, 6>  loss_weights_6, FixedArray<float, 6>  * _S2582)
{
    float _S2583;
    FixedArray<float, 6>  losses_5;
    float _S2584 = float(num_cameras_6);
    float _S2585 = raw_losses_4[int(0)] / _S2584;
    for(;;)
    {
        float _S2586 = (F32_abs((_S2585)));
        if(_S2586 < 0.10000000149011612f)
        {
            _S2583 = 0.5f * _S2585 * _S2585 / 0.10000000149011612f;
            break;
        }
        else
        {
            _S2583 = _S2586 - 0.05000000074505806f;
            break;
        }
    }
    losses_5[int(0)] = _S2583;
    losses_5[int(1)] = raw_losses_4[int(1)] / (3.0f * _S2584);
    losses_5[int(2)] = (raw_losses_4[int(2)] + raw_losses_4[int(3)] + raw_losses_4[int(4)]) / (9.0f * _S2584);
    losses_5[int(3)] = (raw_losses_4[int(5)] + raw_losses_4[int(6)] + raw_losses_4[int(7)] + raw_losses_4[int(8)] + raw_losses_4[int(9)]) / (5.0f * _S2584);
    float _S2587 = raw_losses_4[int(10)] / _S2584;
    for(;;)
    {
        float _S2588 = (F32_abs((_S2587)));
        if(_S2588 < 0.00499999988824129f)
        {
            _S2583 = 0.5f * _S2587 * _S2587 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2583 = _S2588 - 0.00249999994412065f;
            break;
        }
    }
    float _S2589;
    float _S2590 = raw_losses_4[int(11)] / _S2584;
    for(;;)
    {
        float _S2591 = (F32_abs((_S2590)));
        if(_S2591 < 0.00499999988824129f)
        {
            _S2589 = 0.5f * _S2590 * _S2590 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2589 = _S2591 - 0.00249999994412065f;
            break;
        }
    }
    float _S2592 = _S2583 + _S2589;
    float _S2593 = raw_losses_4[int(12)] / _S2584;
    for(;;)
    {
        float _S2594 = (F32_abs((_S2593)));
        if(_S2594 < 0.00499999988824129f)
        {
            _S2583 = 0.5f * _S2593 * _S2593 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2583 = _S2594 - 0.00249999994412065f;
            break;
        }
    }
    float _S2595 = _S2592 + _S2583;
    float _S2596 = raw_losses_4[int(13)] / _S2584;
    for(;;)
    {
        float _S2597 = (F32_abs((_S2596)));
        if(_S2597 < 0.00499999988824129f)
        {
            _S2583 = 0.5f * _S2596 * _S2596 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2583 = _S2597 - 0.00249999994412065f;
            break;
        }
    }
    float _S2598 = _S2595 + _S2583;
    float _S2599 = raw_losses_4[int(14)] / _S2584;
    for(;;)
    {
        float _S2600 = (F32_abs((_S2599)));
        if(_S2600 < 0.00499999988824129f)
        {
            _S2583 = 0.5f * _S2599 * _S2599 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2583 = _S2600 - 0.00249999994412065f;
            break;
        }
    }
    float _S2601 = _S2598 + _S2583;
    float _S2602 = raw_losses_4[int(15)] / _S2584;
    for(;;)
    {
        float _S2603 = (F32_abs((_S2602)));
        if(_S2603 < 0.00499999988824129f)
        {
            _S2583 = 0.5f * _S2602 * _S2602 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2583 = _S2603 - 0.00249999994412065f;
            break;
        }
    }
    float _S2604 = _S2601 + _S2583;
    float _S2605 = raw_losses_4[int(16)] / _S2584;
    for(;;)
    {
        float _S2606 = (F32_abs((_S2605)));
        if(_S2606 < 0.00499999988824129f)
        {
            _S2583 = 0.5f * _S2605 * _S2605 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2583 = _S2606 - 0.00249999994412065f;
            break;
        }
    }
    float _S2607 = _S2604 + _S2583;
    float _S2608 = raw_losses_4[int(17)] / _S2584;
    for(;;)
    {
        float _S2609 = (F32_abs((_S2608)));
        if(_S2609 < 0.00499999988824129f)
        {
            _S2583 = 0.5f * _S2608 * _S2608 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2583 = _S2609 - 0.00249999994412065f;
            break;
        }
    }
    float _S2610 = (_S2607 + _S2583) / 8.0f;
    losses_5[int(5)] = 0.0f;
    losses_5[int(0)] = losses_5[int(0)] * loss_weights_6[int(0)];
    losses_5[int(1)] = losses_5[int(1)] * loss_weights_6[int(1)];
    losses_5[int(2)] = losses_5[int(2)] * loss_weights_6[int(2)];
    losses_5[int(3)] = losses_5[int(3)] * loss_weights_6[int(3)];
    losses_5[int(4)] = _S2610 * loss_weights_6[int(4)];
    *_S2582 = losses_5;
    return;
}

struct DiffPair_arrayx3Cfloatx2C18x3E_0
{
    FixedArray<float, 18>  primal_0;
    FixedArray<float, 18>  differential_0;
};

inline __device__ void s_bwd_prop_compute_ppisp_no_crf_regularization_loss_0(DiffPair_arrayx3Cfloatx2C18x3E_0 * dpraw_losses_2, int num_cameras_7, FixedArray<float, 6>  * loss_weights_7, FixedArray<float, 6>  * _s_dOut_8)
{
    FixedArray<float, 18>  _S2611 = dpraw_losses_2->primal_0;
    float _S2612 = float(num_cameras_7);
    float _S2613 = dpraw_losses_2->primal_0[int(0)] / _S2612;
    bool _S2614 = (s_primal_ctx_abs_0(_S2613)) < 0.10000000149011612f;
    float _S2615;
    if(_S2614)
    {
        _S2615 = 0.5f * _S2613;
    }
    else
    {
        _S2615 = 0.0f;
    }
    float _S2616 = 3.0f * _S2612;
    float _S2617 = 9.0f * _S2612;
    float _S2618 = 5.0f * _S2612;
    float _S2619 = _S2611[int(10)] / _S2612;
    bool _S2620 = (s_primal_ctx_abs_0(_S2619)) < 0.00499999988824129f;
    float _S2621;
    if(_S2620)
    {
        _S2621 = 0.5f * _S2619;
    }
    else
    {
        _S2621 = 0.0f;
    }
    float _S2622 = _S2611[int(11)] / _S2612;
    bool _S2623 = (s_primal_ctx_abs_0(_S2622)) < 0.00499999988824129f;
    float _S2624;
    if(_S2623)
    {
        _S2624 = 0.5f * _S2622;
    }
    else
    {
        _S2624 = 0.0f;
    }
    float _S2625 = _S2611[int(12)] / _S2612;
    bool _S2626 = (s_primal_ctx_abs_0(_S2625)) < 0.00499999988824129f;
    float _S2627;
    if(_S2626)
    {
        _S2627 = 0.5f * _S2625;
    }
    else
    {
        _S2627 = 0.0f;
    }
    float _S2628 = _S2611[int(13)] / _S2612;
    bool _S2629 = (s_primal_ctx_abs_0(_S2628)) < 0.00499999988824129f;
    float _S2630;
    if(_S2629)
    {
        _S2630 = 0.5f * _S2628;
    }
    else
    {
        _S2630 = 0.0f;
    }
    float _S2631 = _S2611[int(14)] / _S2612;
    bool _S2632 = (s_primal_ctx_abs_0(_S2631)) < 0.00499999988824129f;
    float _S2633;
    if(_S2632)
    {
        _S2633 = 0.5f * _S2631;
    }
    else
    {
        _S2633 = 0.0f;
    }
    float _S2634 = _S2611[int(15)] / _S2612;
    bool _S2635 = (s_primal_ctx_abs_0(_S2634)) < 0.00499999988824129f;
    float _S2636;
    if(_S2635)
    {
        _S2636 = 0.5f * _S2634;
    }
    else
    {
        _S2636 = 0.0f;
    }
    float _S2637 = _S2611[int(16)] / _S2612;
    bool _S2638 = (s_primal_ctx_abs_0(_S2637)) < 0.00499999988824129f;
    float _S2639;
    if(_S2638)
    {
        _S2639 = 0.5f * _S2637;
    }
    else
    {
        _S2639 = 0.0f;
    }
    float _S2640 = _S2611[int(17)] / _S2612;
    bool _S2641 = (s_primal_ctx_abs_0(_S2640)) < 0.00499999988824129f;
    float _S2642;
    if(_S2641)
    {
        _S2642 = 0.5f * _S2640;
    }
    else
    {
        _S2642 = 0.0f;
    }
    float _S2643 = (*loss_weights_7)[int(3)] * (*_s_dOut_8)[int(3)];
    float _S2644 = (*loss_weights_7)[int(2)] * (*_s_dOut_8)[int(2)];
    float _S2645 = (*loss_weights_7)[int(1)] * (*_s_dOut_8)[int(1)];
    float _S2646 = (*loss_weights_7)[int(0)] * (*_s_dOut_8)[int(0)];
    float _S2647 = 0.125f * ((*loss_weights_7)[int(4)] * (*_s_dOut_8)[int(4)]);
    float _S2648;
    if(_S2641)
    {
        float _S2649 = 200.0f * _S2647;
        float _S2650 = _S2642 * _S2649 + 0.5f * (_S2640 * _S2649);
        _S2642 = 0.0f;
        _S2648 = _S2650;
    }
    else
    {
        _S2642 = _S2647;
        _S2648 = 0.0f;
    }
    DiffPair_float_0 _S2651;
    (&_S2651)->primal_0 = _S2640;
    (&_S2651)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2651, _S2642);
    float _S2652 = (_S2651.differential_0 + _S2648) / _S2612;
    FixedArray<float, 18>  _S2653;
    _S2653[int(0)] = 0.0f;
    _S2653[int(1)] = 0.0f;
    _S2653[int(2)] = 0.0f;
    _S2653[int(3)] = 0.0f;
    _S2653[int(4)] = 0.0f;
    _S2653[int(5)] = 0.0f;
    _S2653[int(6)] = 0.0f;
    _S2653[int(7)] = 0.0f;
    _S2653[int(8)] = 0.0f;
    _S2653[int(9)] = 0.0f;
    _S2653[int(10)] = 0.0f;
    _S2653[int(11)] = 0.0f;
    _S2653[int(12)] = 0.0f;
    _S2653[int(13)] = 0.0f;
    _S2653[int(14)] = 0.0f;
    _S2653[int(15)] = 0.0f;
    _S2653[int(16)] = 0.0f;
    _S2653[int(17)] = 0.0f;
    _S2653[int(17)] = _S2652;
    float _S2654 = _S2653[int(0)];
    float _S2655 = _S2653[int(1)];
    float _S2656 = _S2653[int(2)];
    float _S2657 = _S2653[int(3)];
    float _S2658 = _S2653[int(4)];
    float _S2659 = _S2653[int(5)];
    float _S2660 = _S2653[int(6)];
    float _S2661 = _S2653[int(7)];
    float _S2662 = _S2653[int(8)];
    float _S2663 = _S2653[int(9)];
    float _S2664 = _S2653[int(10)];
    float _S2665 = _S2653[int(11)];
    float _S2666 = _S2653[int(12)];
    float _S2667 = _S2653[int(13)];
    float _S2668 = _S2653[int(14)];
    float _S2669 = _S2653[int(15)];
    float _S2670 = _S2653[int(16)];
    float _S2671 = _S2653[int(17)];
    if(_S2638)
    {
        float _S2672 = 200.0f * _S2647;
        float _S2673 = _S2639 * _S2672 + 0.5f * (_S2637 * _S2672);
        _S2639 = 0.0f;
        _S2642 = _S2673;
    }
    else
    {
        _S2639 = _S2647;
        _S2642 = 0.0f;
    }
    DiffPair_float_0 _S2674;
    (&_S2674)->primal_0 = _S2637;
    (&_S2674)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2674, _S2639);
    float _S2675 = (_S2674.differential_0 + _S2642) / _S2612;
    FixedArray<float, 18>  _S2676;
    _S2676[int(0)] = 0.0f;
    _S2676[int(1)] = 0.0f;
    _S2676[int(2)] = 0.0f;
    _S2676[int(3)] = 0.0f;
    _S2676[int(4)] = 0.0f;
    _S2676[int(5)] = 0.0f;
    _S2676[int(6)] = 0.0f;
    _S2676[int(7)] = 0.0f;
    _S2676[int(8)] = 0.0f;
    _S2676[int(9)] = 0.0f;
    _S2676[int(10)] = 0.0f;
    _S2676[int(11)] = 0.0f;
    _S2676[int(12)] = 0.0f;
    _S2676[int(13)] = 0.0f;
    _S2676[int(14)] = 0.0f;
    _S2676[int(15)] = 0.0f;
    _S2676[int(16)] = 0.0f;
    _S2676[int(17)] = 0.0f;
    _S2676[int(16)] = _S2675;
    float _S2677 = _S2654 + _S2676[int(0)];
    float _S2678 = _S2655 + _S2676[int(1)];
    float _S2679 = _S2656 + _S2676[int(2)];
    float _S2680 = _S2657 + _S2676[int(3)];
    float _S2681 = _S2658 + _S2676[int(4)];
    float _S2682 = _S2659 + _S2676[int(5)];
    float _S2683 = _S2660 + _S2676[int(6)];
    float _S2684 = _S2661 + _S2676[int(7)];
    float _S2685 = _S2662 + _S2676[int(8)];
    float _S2686 = _S2663 + _S2676[int(9)];
    float _S2687 = _S2664 + _S2676[int(10)];
    float _S2688 = _S2665 + _S2676[int(11)];
    float _S2689 = _S2666 + _S2676[int(12)];
    float _S2690 = _S2667 + _S2676[int(13)];
    float _S2691 = _S2668 + _S2676[int(14)];
    float _S2692 = _S2669 + _S2676[int(15)];
    float _S2693 = _S2670 + _S2676[int(16)];
    float _S2694 = _S2671 + _S2676[int(17)];
    if(_S2635)
    {
        float _S2695 = 200.0f * _S2647;
        float _S2696 = _S2636 * _S2695 + 0.5f * (_S2634 * _S2695);
        _S2636 = 0.0f;
        _S2639 = _S2696;
    }
    else
    {
        _S2636 = _S2647;
        _S2639 = 0.0f;
    }
    DiffPair_float_0 _S2697;
    (&_S2697)->primal_0 = _S2634;
    (&_S2697)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2697, _S2636);
    float _S2698 = (_S2697.differential_0 + _S2639) / _S2612;
    FixedArray<float, 18>  _S2699;
    _S2699[int(0)] = 0.0f;
    _S2699[int(1)] = 0.0f;
    _S2699[int(2)] = 0.0f;
    _S2699[int(3)] = 0.0f;
    _S2699[int(4)] = 0.0f;
    _S2699[int(5)] = 0.0f;
    _S2699[int(6)] = 0.0f;
    _S2699[int(7)] = 0.0f;
    _S2699[int(8)] = 0.0f;
    _S2699[int(9)] = 0.0f;
    _S2699[int(10)] = 0.0f;
    _S2699[int(11)] = 0.0f;
    _S2699[int(12)] = 0.0f;
    _S2699[int(13)] = 0.0f;
    _S2699[int(14)] = 0.0f;
    _S2699[int(15)] = 0.0f;
    _S2699[int(16)] = 0.0f;
    _S2699[int(17)] = 0.0f;
    _S2699[int(15)] = _S2698;
    float _S2700 = _S2677 + _S2699[int(0)];
    float _S2701 = _S2678 + _S2699[int(1)];
    float _S2702 = _S2679 + _S2699[int(2)];
    float _S2703 = _S2680 + _S2699[int(3)];
    float _S2704 = _S2681 + _S2699[int(4)];
    float _S2705 = _S2682 + _S2699[int(5)];
    float _S2706 = _S2683 + _S2699[int(6)];
    float _S2707 = _S2684 + _S2699[int(7)];
    float _S2708 = _S2685 + _S2699[int(8)];
    float _S2709 = _S2686 + _S2699[int(9)];
    float _S2710 = _S2687 + _S2699[int(10)];
    float _S2711 = _S2688 + _S2699[int(11)];
    float _S2712 = _S2689 + _S2699[int(12)];
    float _S2713 = _S2690 + _S2699[int(13)];
    float _S2714 = _S2691 + _S2699[int(14)];
    float _S2715 = _S2692 + _S2699[int(15)];
    float _S2716 = _S2693 + _S2699[int(16)];
    float _S2717 = _S2694 + _S2699[int(17)];
    if(_S2632)
    {
        float _S2718 = 200.0f * _S2647;
        float _S2719 = _S2633 * _S2718 + 0.5f * (_S2631 * _S2718);
        _S2633 = 0.0f;
        _S2636 = _S2719;
    }
    else
    {
        _S2633 = _S2647;
        _S2636 = 0.0f;
    }
    DiffPair_float_0 _S2720;
    (&_S2720)->primal_0 = _S2631;
    (&_S2720)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2720, _S2633);
    float _S2721 = (_S2720.differential_0 + _S2636) / _S2612;
    FixedArray<float, 18>  _S2722;
    _S2722[int(0)] = 0.0f;
    _S2722[int(1)] = 0.0f;
    _S2722[int(2)] = 0.0f;
    _S2722[int(3)] = 0.0f;
    _S2722[int(4)] = 0.0f;
    _S2722[int(5)] = 0.0f;
    _S2722[int(6)] = 0.0f;
    _S2722[int(7)] = 0.0f;
    _S2722[int(8)] = 0.0f;
    _S2722[int(9)] = 0.0f;
    _S2722[int(10)] = 0.0f;
    _S2722[int(11)] = 0.0f;
    _S2722[int(12)] = 0.0f;
    _S2722[int(13)] = 0.0f;
    _S2722[int(14)] = 0.0f;
    _S2722[int(15)] = 0.0f;
    _S2722[int(16)] = 0.0f;
    _S2722[int(17)] = 0.0f;
    _S2722[int(14)] = _S2721;
    float _S2723 = _S2700 + _S2722[int(0)];
    float _S2724 = _S2701 + _S2722[int(1)];
    float _S2725 = _S2702 + _S2722[int(2)];
    float _S2726 = _S2703 + _S2722[int(3)];
    float _S2727 = _S2704 + _S2722[int(4)];
    float _S2728 = _S2705 + _S2722[int(5)];
    float _S2729 = _S2706 + _S2722[int(6)];
    float _S2730 = _S2707 + _S2722[int(7)];
    float _S2731 = _S2708 + _S2722[int(8)];
    float _S2732 = _S2709 + _S2722[int(9)];
    float _S2733 = _S2710 + _S2722[int(10)];
    float _S2734 = _S2711 + _S2722[int(11)];
    float _S2735 = _S2712 + _S2722[int(12)];
    float _S2736 = _S2713 + _S2722[int(13)];
    float _S2737 = _S2714 + _S2722[int(14)];
    float _S2738 = _S2715 + _S2722[int(15)];
    float _S2739 = _S2716 + _S2722[int(16)];
    float _S2740 = _S2717 + _S2722[int(17)];
    if(_S2629)
    {
        float _S2741 = 200.0f * _S2647;
        float _S2742 = _S2630 * _S2741 + 0.5f * (_S2628 * _S2741);
        _S2630 = 0.0f;
        _S2633 = _S2742;
    }
    else
    {
        _S2630 = _S2647;
        _S2633 = 0.0f;
    }
    DiffPair_float_0 _S2743;
    (&_S2743)->primal_0 = _S2628;
    (&_S2743)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2743, _S2630);
    float _S2744 = (_S2743.differential_0 + _S2633) / _S2612;
    FixedArray<float, 18>  _S2745;
    _S2745[int(0)] = 0.0f;
    _S2745[int(1)] = 0.0f;
    _S2745[int(2)] = 0.0f;
    _S2745[int(3)] = 0.0f;
    _S2745[int(4)] = 0.0f;
    _S2745[int(5)] = 0.0f;
    _S2745[int(6)] = 0.0f;
    _S2745[int(7)] = 0.0f;
    _S2745[int(8)] = 0.0f;
    _S2745[int(9)] = 0.0f;
    _S2745[int(10)] = 0.0f;
    _S2745[int(11)] = 0.0f;
    _S2745[int(12)] = 0.0f;
    _S2745[int(13)] = 0.0f;
    _S2745[int(14)] = 0.0f;
    _S2745[int(15)] = 0.0f;
    _S2745[int(16)] = 0.0f;
    _S2745[int(17)] = 0.0f;
    _S2745[int(13)] = _S2744;
    float _S2746 = _S2723 + _S2745[int(0)];
    float _S2747 = _S2724 + _S2745[int(1)];
    float _S2748 = _S2725 + _S2745[int(2)];
    float _S2749 = _S2726 + _S2745[int(3)];
    float _S2750 = _S2727 + _S2745[int(4)];
    float _S2751 = _S2728 + _S2745[int(5)];
    float _S2752 = _S2729 + _S2745[int(6)];
    float _S2753 = _S2730 + _S2745[int(7)];
    float _S2754 = _S2731 + _S2745[int(8)];
    float _S2755 = _S2732 + _S2745[int(9)];
    float _S2756 = _S2733 + _S2745[int(10)];
    float _S2757 = _S2734 + _S2745[int(11)];
    float _S2758 = _S2735 + _S2745[int(12)];
    float _S2759 = _S2736 + _S2745[int(13)];
    float _S2760 = _S2737 + _S2745[int(14)];
    float _S2761 = _S2738 + _S2745[int(15)];
    float _S2762 = _S2739 + _S2745[int(16)];
    float _S2763 = _S2740 + _S2745[int(17)];
    if(_S2626)
    {
        float _S2764 = 200.0f * _S2647;
        float _S2765 = _S2627 * _S2764 + 0.5f * (_S2625 * _S2764);
        _S2627 = 0.0f;
        _S2630 = _S2765;
    }
    else
    {
        _S2627 = _S2647;
        _S2630 = 0.0f;
    }
    DiffPair_float_0 _S2766;
    (&_S2766)->primal_0 = _S2625;
    (&_S2766)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2766, _S2627);
    float _S2767 = (_S2766.differential_0 + _S2630) / _S2612;
    FixedArray<float, 18>  _S2768;
    _S2768[int(0)] = 0.0f;
    _S2768[int(1)] = 0.0f;
    _S2768[int(2)] = 0.0f;
    _S2768[int(3)] = 0.0f;
    _S2768[int(4)] = 0.0f;
    _S2768[int(5)] = 0.0f;
    _S2768[int(6)] = 0.0f;
    _S2768[int(7)] = 0.0f;
    _S2768[int(8)] = 0.0f;
    _S2768[int(9)] = 0.0f;
    _S2768[int(10)] = 0.0f;
    _S2768[int(11)] = 0.0f;
    _S2768[int(12)] = 0.0f;
    _S2768[int(13)] = 0.0f;
    _S2768[int(14)] = 0.0f;
    _S2768[int(15)] = 0.0f;
    _S2768[int(16)] = 0.0f;
    _S2768[int(17)] = 0.0f;
    _S2768[int(12)] = _S2767;
    float _S2769 = _S2746 + _S2768[int(0)];
    float _S2770 = _S2747 + _S2768[int(1)];
    float _S2771 = _S2748 + _S2768[int(2)];
    float _S2772 = _S2749 + _S2768[int(3)];
    float _S2773 = _S2750 + _S2768[int(4)];
    float _S2774 = _S2751 + _S2768[int(5)];
    float _S2775 = _S2752 + _S2768[int(6)];
    float _S2776 = _S2753 + _S2768[int(7)];
    float _S2777 = _S2754 + _S2768[int(8)];
    float _S2778 = _S2755 + _S2768[int(9)];
    float _S2779 = _S2756 + _S2768[int(10)];
    float _S2780 = _S2757 + _S2768[int(11)];
    float _S2781 = _S2758 + _S2768[int(12)];
    float _S2782 = _S2759 + _S2768[int(13)];
    float _S2783 = _S2760 + _S2768[int(14)];
    float _S2784 = _S2761 + _S2768[int(15)];
    float _S2785 = _S2762 + _S2768[int(16)];
    float _S2786 = _S2763 + _S2768[int(17)];
    if(_S2623)
    {
        float _S2787 = 200.0f * _S2647;
        float _S2788 = _S2624 * _S2787 + 0.5f * (_S2622 * _S2787);
        _S2624 = 0.0f;
        _S2627 = _S2788;
    }
    else
    {
        _S2624 = _S2647;
        _S2627 = 0.0f;
    }
    DiffPair_float_0 _S2789;
    (&_S2789)->primal_0 = _S2622;
    (&_S2789)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2789, _S2624);
    float _S2790 = (_S2789.differential_0 + _S2627) / _S2612;
    FixedArray<float, 18>  _S2791;
    _S2791[int(0)] = 0.0f;
    _S2791[int(1)] = 0.0f;
    _S2791[int(2)] = 0.0f;
    _S2791[int(3)] = 0.0f;
    _S2791[int(4)] = 0.0f;
    _S2791[int(5)] = 0.0f;
    _S2791[int(6)] = 0.0f;
    _S2791[int(7)] = 0.0f;
    _S2791[int(8)] = 0.0f;
    _S2791[int(9)] = 0.0f;
    _S2791[int(10)] = 0.0f;
    _S2791[int(11)] = 0.0f;
    _S2791[int(12)] = 0.0f;
    _S2791[int(13)] = 0.0f;
    _S2791[int(14)] = 0.0f;
    _S2791[int(15)] = 0.0f;
    _S2791[int(16)] = 0.0f;
    _S2791[int(17)] = 0.0f;
    _S2791[int(11)] = _S2790;
    float _S2792 = _S2769 + _S2791[int(0)];
    float _S2793 = _S2770 + _S2791[int(1)];
    float _S2794 = _S2771 + _S2791[int(2)];
    float _S2795 = _S2772 + _S2791[int(3)];
    float _S2796 = _S2773 + _S2791[int(4)];
    float _S2797 = _S2774 + _S2791[int(5)];
    float _S2798 = _S2775 + _S2791[int(6)];
    float _S2799 = _S2776 + _S2791[int(7)];
    float _S2800 = _S2777 + _S2791[int(8)];
    float _S2801 = _S2778 + _S2791[int(9)];
    float _S2802 = _S2779 + _S2791[int(10)];
    float _S2803 = _S2780 + _S2791[int(11)];
    float _S2804 = _S2781 + _S2791[int(12)];
    float _S2805 = _S2782 + _S2791[int(13)];
    float _S2806 = _S2783 + _S2791[int(14)];
    float _S2807 = _S2784 + _S2791[int(15)];
    float _S2808 = _S2785 + _S2791[int(16)];
    float _S2809 = _S2786 + _S2791[int(17)];
    if(_S2620)
    {
        float _S2810 = 200.0f * _S2647;
        float _S2811 = _S2621 * _S2810 + 0.5f * (_S2619 * _S2810);
        _S2621 = 0.0f;
        _S2624 = _S2811;
    }
    else
    {
        _S2621 = _S2647;
        _S2624 = 0.0f;
    }
    DiffPair_float_0 _S2812;
    (&_S2812)->primal_0 = _S2619;
    (&_S2812)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2812, _S2621);
    float _S2813 = (_S2812.differential_0 + _S2624) / _S2612;
    float _S2814 = _S2643 / _S2618;
    float _S2815 = _S2644 / _S2617;
    float _S2816 = _S2645 / _S2616;
    FixedArray<float, 18>  _S2817;
    _S2817[int(0)] = 0.0f;
    _S2817[int(1)] = 0.0f;
    _S2817[int(2)] = 0.0f;
    _S2817[int(3)] = 0.0f;
    _S2817[int(4)] = 0.0f;
    _S2817[int(5)] = 0.0f;
    _S2817[int(6)] = 0.0f;
    _S2817[int(7)] = 0.0f;
    _S2817[int(8)] = 0.0f;
    _S2817[int(9)] = 0.0f;
    _S2817[int(10)] = 0.0f;
    _S2817[int(11)] = 0.0f;
    _S2817[int(12)] = 0.0f;
    _S2817[int(13)] = 0.0f;
    _S2817[int(14)] = 0.0f;
    _S2817[int(15)] = 0.0f;
    _S2817[int(16)] = 0.0f;
    _S2817[int(17)] = 0.0f;
    _S2817[int(10)] = _S2813;
    _S2817[int(9)] = _S2814;
    _S2817[int(8)] = _S2814;
    _S2817[int(7)] = _S2814;
    _S2817[int(6)] = _S2814;
    _S2817[int(5)] = _S2814;
    _S2817[int(4)] = _S2815;
    _S2817[int(3)] = _S2815;
    _S2817[int(2)] = _S2815;
    _S2817[int(1)] = _S2816;
    float _S2818 = _S2792 + _S2817[int(0)];
    float _S2819 = _S2793 + _S2817[int(1)];
    float _S2820 = _S2794 + _S2817[int(2)];
    float _S2821 = _S2795 + _S2817[int(3)];
    float _S2822 = _S2796 + _S2817[int(4)];
    float _S2823 = _S2797 + _S2817[int(5)];
    float _S2824 = _S2798 + _S2817[int(6)];
    float _S2825 = _S2799 + _S2817[int(7)];
    float _S2826 = _S2800 + _S2817[int(8)];
    float _S2827 = _S2801 + _S2817[int(9)];
    float _S2828 = _S2802 + _S2817[int(10)];
    float _S2829 = _S2803 + _S2817[int(11)];
    float _S2830 = _S2804 + _S2817[int(12)];
    float _S2831 = _S2805 + _S2817[int(13)];
    float _S2832 = _S2806 + _S2817[int(14)];
    float _S2833 = _S2807 + _S2817[int(15)];
    float _S2834 = _S2808 + _S2817[int(16)];
    float _S2835 = _S2809 + _S2817[int(17)];
    if(_S2614)
    {
        float _S2836 = 10.0f * _S2646;
        float _S2837 = _S2615 * _S2836 + 0.5f * (_S2613 * _S2836);
        _S2615 = 0.0f;
        _S2621 = _S2837;
    }
    else
    {
        _S2615 = _S2646;
        _S2621 = 0.0f;
    }
    DiffPair_float_0 _S2838;
    (&_S2838)->primal_0 = _S2613;
    (&_S2838)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2838, _S2615);
    float _S2839 = (_S2838.differential_0 + _S2621) / _S2612;
    FixedArray<float, 18>  _S2840;
    _S2840[int(0)] = 0.0f;
    _S2840[int(1)] = 0.0f;
    _S2840[int(2)] = 0.0f;
    _S2840[int(3)] = 0.0f;
    _S2840[int(4)] = 0.0f;
    _S2840[int(5)] = 0.0f;
    _S2840[int(6)] = 0.0f;
    _S2840[int(7)] = 0.0f;
    _S2840[int(8)] = 0.0f;
    _S2840[int(9)] = 0.0f;
    _S2840[int(10)] = 0.0f;
    _S2840[int(11)] = 0.0f;
    _S2840[int(12)] = 0.0f;
    _S2840[int(13)] = 0.0f;
    _S2840[int(14)] = 0.0f;
    _S2840[int(15)] = 0.0f;
    _S2840[int(16)] = 0.0f;
    _S2840[int(17)] = 0.0f;
    _S2840[int(0)] = _S2839;
    FixedArray<float, 18>  _S2841 = {
        _S2818 + _S2840[int(0)], _S2819 + _S2840[int(1)], _S2820 + _S2840[int(2)], _S2821 + _S2840[int(3)], _S2822 + _S2840[int(4)], _S2823 + _S2840[int(5)], _S2824 + _S2840[int(6)], _S2825 + _S2840[int(7)], _S2826 + _S2840[int(8)], _S2827 + _S2840[int(9)], _S2828 + _S2840[int(10)], _S2829 + _S2840[int(11)], _S2830 + _S2840[int(12)], _S2831 + _S2840[int(13)], _S2832 + _S2840[int(14)], _S2833 + _S2840[int(15)], _S2834 + _S2840[int(16)], _S2835 + _S2840[int(17)]
    };
    dpraw_losses_2->primal_0 = dpraw_losses_2->primal_0;
    dpraw_losses_2->differential_0 = _S2841;
    return;
}

inline __device__ void s_bwd_compute_ppisp_no_crf_regularization_loss_0(DiffPair_arrayx3Cfloatx2C18x3E_0 * _S2842, int _S2843, FixedArray<float, 6>  * _S2844, FixedArray<float, 6>  * _S2845)
{
    s_bwd_prop_compute_ppisp_no_crf_regularization_loss_0(_S2842, _S2843, _S2844, _S2845);
    return;
}

inline __device__ void compute_ppisp_no_crf_regularization_loss_vjp(FixedArray<float, 18>  raw_losses_5, int num_cameras_8, FixedArray<float, 6>  loss_weights_8, FixedArray<float, 6>  grad_out_8, FixedArray<float, 18>  * _S2846)
{
    FixedArray<float, 18>  _S2847 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    DiffPair_arrayx3Cfloatx2C18x3E_0 dp_raw_losses_2;
    (&dp_raw_losses_2)->primal_0 = raw_losses_5;
    (&dp_raw_losses_2)->differential_0 = _S2847;
    FixedArray<float, 6>  _S2848 = loss_weights_8;
    FixedArray<float, 6>  _S2849 = grad_out_8;
    s_bwd_compute_ppisp_no_crf_regularization_loss_0(&dp_raw_losses_2, num_cameras_8, &_S2848, &_S2849);
    *_S2846 = (&dp_raw_losses_2)->differential_0;
    return;
}

