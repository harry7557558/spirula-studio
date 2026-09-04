#pragma once

#include "generated/slang.cuh"

struct ColorPPISPParams_0
{
    float2  b_0;
    float2  r_0;
    float2  g_0;
    float2  n_0;
};

inline __device__ ColorPPISPParams_0 ColorPPISPParams_x24_syn_dzero_0()
{
    ColorPPISPParams_0 result_0;
    float2  _S1 = make_float2 (0.0f);
    (&result_0)->b_0 = _S1;
    (&result_0)->r_0 = _S1;
    (&result_0)->g_0 = _S1;
    (&result_0)->n_0 = _S1;
    return result_0;
}

struct PPISPParamsNoCRFNoVig_0
{
    float exposure_0;
    ColorPPISPParams_0 color_params_0;
};

inline __device__ PPISPParamsNoCRFNoVig_0 PPISPParamsNoCRFNoVig_x24_syn_dzero_0()
{
    PPISPParamsNoCRFNoVig_0 result_1;
    (&result_1)->exposure_0 = 0.0f;
    (&result_1)->color_params_0 = ColorPPISPParams_x24_syn_dzero_0();
    return result_1;
}

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
    VignettingChannelParams_0 result_2;
    (&result_2)->cx_0 = 0.0f;
    (&result_2)->cy_0 = 0.0f;
    (&result_2)->alpha0_0 = 0.0f;
    (&result_2)->alpha1_0 = 0.0f;
    (&result_2)->alpha2_0 = 0.0f;
    return result_2;
}

struct PPISPParamsNoCRF_0
{
    float exposure_1;
    FixedArray<VignettingChannelParams_0, 3>  vignette_params_0;
    ColorPPISPParams_0 color_params_1;
};

inline __device__ PPISPParamsNoCRF_0 PPISPParamsNoCRF_x24_syn_dzero_0()
{
    PPISPParamsNoCRF_0 result_3;
    (&result_3)->exposure_1 = 0.0f;
    VignettingChannelParams_0 _S2 = VignettingChannelParams_x24_syn_dzero_0();
    (&result_3)->vignette_params_0[int(0)] = _S2;
    (&result_3)->vignette_params_0[int(1)] = _S2;
    (&result_3)->vignette_params_0[int(2)] = _S2;
    (&result_3)->color_params_1 = ColorPPISPParams_x24_syn_dzero_0();
    return result_3;
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
    RQSCRFPPISPChannelParams_0 result_4;
    (&result_4)->g0_0 = 0.0f;
    (&result_4)->g1_0 = 0.0f;
    (&result_4)->x0_0 = 0.0f;
    (&result_4)->y0_0 = 0.0f;
    (&result_4)->gc_0 = 0.0f;
    return result_4;
}

struct PPISPParamsRQS_0
{
    float exposure_2;
    FixedArray<VignettingChannelParams_0, 3>  vignette_params_1;
    ColorPPISPParams_0 color_params_2;
    FixedArray<RQSCRFPPISPChannelParams_0, 3>  crf_params_0;
};

inline __device__ PPISPParamsRQS_0 PPISPParamsRQS_x24_syn_dzero_0()
{
    PPISPParamsRQS_0 result_5;
    (&result_5)->exposure_2 = 0.0f;
    VignettingChannelParams_0 _S3 = VignettingChannelParams_x24_syn_dzero_0();
    (&result_5)->vignette_params_1[int(0)] = _S3;
    (&result_5)->vignette_params_1[int(1)] = _S3;
    (&result_5)->vignette_params_1[int(2)] = _S3;
    (&result_5)->color_params_2 = ColorPPISPParams_x24_syn_dzero_0();
    RQSCRFPPISPChannelParams_0 _S4 = RQSCRFPPISPChannelParams_x24_syn_dzero_0();
    (&result_5)->crf_params_0[int(0)] = _S4;
    (&result_5)->crf_params_0[int(1)] = _S4;
    (&result_5)->crf_params_0[int(2)] = _S4;
    return result_5;
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
    CRFPPISPChannelParams_0 result_6;
    (&result_6)->toe_0 = 0.0f;
    (&result_6)->shoulder_0 = 0.0f;
    (&result_6)->gamma_0 = 0.0f;
    (&result_6)->center_0 = 0.0f;
    return result_6;
}

struct PPISPParams_0
{
    float exposure_3;
    FixedArray<VignettingChannelParams_0, 3>  vignette_params_2;
    ColorPPISPParams_0 color_params_3;
    FixedArray<CRFPPISPChannelParams_0, 3>  crf_params_1;
};

inline __device__ PPISPParams_0 PPISPParams_x24_syn_dzero_0()
{
    PPISPParams_0 result_7;
    (&result_7)->exposure_3 = 0.0f;
    VignettingChannelParams_0 _S5 = VignettingChannelParams_x24_syn_dzero_0();
    (&result_7)->vignette_params_2[int(0)] = _S5;
    (&result_7)->vignette_params_2[int(1)] = _S5;
    (&result_7)->vignette_params_2[int(2)] = _S5;
    (&result_7)->color_params_3 = ColorPPISPParams_x24_syn_dzero_0();
    CRFPPISPChannelParams_0 _S6 = CRFPPISPChannelParams_x24_syn_dzero_0();
    (&result_7)->crf_params_1[int(0)] = _S6;
    (&result_7)->crf_params_1[int(1)] = _S6;
    (&result_7)->crf_params_1[int(2)] = _S6;
    return result_7;
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
    float2  result_8;
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
        *_slang_vector_get_element_ptr(&result_8, i_0) = sum_8;
        i_0 = i_0 + int(1);
    }
    return result_8;
}

inline __device__ float3  mul_1(Matrix<float, 3, 3>  left_3, float3  right_3)
{
    float3  result_9;
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
        *_slang_vector_get_element_ptr(&result_9, i_1) = sum_10;
        i_1 = i_1 + int(1);
    }
    return result_9;
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
    Matrix<float, 3, 3>  result_10;
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
            *_slang_vector_get_element_ptr(((&result_10)->rows + (r_1)), c_0) = sum_12;
            c_0 = c_0 + int(1);
        }
        r_1 = r_1 + int(1);
    }
    return result_10;
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
    float result_11 = 0.0f;
    for(;;)
    {
        if(i_3 < int(3))
        {
        }
        else
        {
            break;
        }
        float result_12 = result_11 + _slang_vector_get_element(x_1, i_3) * _slang_vector_get_element(y_0, i_3);
        i_3 = i_3 + int(1);
        result_11 = result_12;
    }
    return result_11;
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
    float3  result_13;
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
        *_slang_vector_get_element_ptr(&result_13, i_4) = (F32_min((_slang_vector_get_element(x_2, i_4)), (_slang_vector_get_element(y_1, i_4))));
        i_4 = i_4 + int(1);
    }
    return result_13;
}

inline __device__ float3  max_0(float3  x_3, float3  y_2)
{
    float3  result_14;
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
        *_slang_vector_get_element_ptr(&result_14, i_5) = (F32_max((_slang_vector_get_element(x_3, i_5)), (_slang_vector_get_element(y_2, i_5))));
        i_5 = i_5 + int(1);
    }
    return result_14;
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
    (&p_0)->exposure_3 = params_0[int(0)];
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
    *&((&(&(&p_0)->color_params_3)->b_0)->x) = params_0[int(16)];
    *&((&(&(&p_0)->color_params_3)->b_0)->y) = params_0[int(17)];
    *&((&(&(&p_0)->color_params_3)->r_0)->x) = params_0[int(18)];
    *&((&(&(&p_0)->color_params_3)->r_0)->y) = params_0[int(19)];
    *&((&(&(&p_0)->color_params_3)->g_0)->x) = params_0[int(20)];
    *&((&(&(&p_0)->color_params_3)->g_0)->y) = params_0[int(21)];
    *&((&(&(&p_0)->color_params_3)->n_0)->x) = params_0[int(22)];
    *&((&(&(&p_0)->color_params_3)->n_0)->y) = params_0[int(23)];
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
    float3  rgb_out_0 = rgb_in_0 * make_float3 ((F32_exp2((p_0.exposure_3))));
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
    float2  bd_0 = mul_0(makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f), p_0.color_params_3.b_0);
    float2  rd_0 = mul_0(makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f), p_0.color_params_3.r_0);
    float2  gd_0 = mul_0(makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f), p_0.color_params_3.g_0);
    float2  nd_0 = mul_0(makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f), p_0.color_params_3.n_0);
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
    float norm_factor_0 = intensity_0 / (F32_max((rgi_out_0.z), (0.00009999999747379f * (F32_abs((intensity_0))) + 9.99999993922529029e-09f)));
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
    (&p_1)->exposure_2 = params_1[int(0)];
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
    *&((&(&(&p_1)->color_params_2)->b_0)->x) = params_1[int(16)];
    *&((&(&(&p_1)->color_params_2)->b_0)->y) = params_1[int(17)];
    *&((&(&(&p_1)->color_params_2)->r_0)->x) = params_1[int(18)];
    *&((&(&(&p_1)->color_params_2)->r_0)->y) = params_1[int(19)];
    *&((&(&(&p_1)->color_params_2)->g_0)->x) = params_1[int(20)];
    *&((&(&(&p_1)->color_params_2)->g_0)->y) = params_1[int(21)];
    *&((&(&(&p_1)->color_params_2)->n_0)->x) = params_1[int(22)];
    *&((&(&(&p_1)->color_params_2)->n_0)->y) = params_1[int(23)];
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
    float3  rgb_out_2 = rgb_in_1 * make_float3 ((F32_exp2((p_1.exposure_2))));
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
    float2  bd_1 = mul_0(makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f), p_1.color_params_2.b_0);
    float2  rd_1 = mul_0(makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f), p_1.color_params_2.r_0);
    float2  gd_1 = mul_0(makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f), p_1.color_params_2.g_0);
    float2  nd_1 = mul_0(makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f), p_1.color_params_2.n_0);
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
    float norm_factor_1 = intensity_1 / (F32_max((rgi_out_1.z), (0.00009999999747379f * (F32_abs((intensity_1))) + 9.99999993922529029e-09f)));
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

inline __device__ float3  apply_ppisp_no_crf(float3  rgb_in_2, float2  pix_coord_2, float2  image_center_2, float2  img_size_2, FixedArray<float, 24>  params_2, bool clamp_output_0)
{
    PPISPParamsNoCRF_0 p_2;
    (&p_2)->exposure_1 = params_2[int(0)];
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
    *&((&(&(&p_2)->color_params_1)->b_0)->x) = params_2[int(16)];
    *&((&(&(&p_2)->color_params_1)->b_0)->y) = params_2[int(17)];
    *&((&(&(&p_2)->color_params_1)->r_0)->x) = params_2[int(18)];
    *&((&(&(&p_2)->color_params_1)->r_0)->y) = params_2[int(19)];
    *&((&(&(&p_2)->color_params_1)->g_0)->x) = params_2[int(20)];
    *&((&(&(&p_2)->color_params_1)->g_0)->y) = params_2[int(21)];
    *&((&(&(&p_2)->color_params_1)->n_0)->x) = params_2[int(22)];
    *&((&(&(&p_2)->color_params_1)->n_0)->y) = params_2[int(23)];
    float _S95 = (F32_max((img_size_2.x), (img_size_2.y)));
    float _S96 = (pix_coord_2.x - image_center_2.x) / _S95;
    float _S97 = (pix_coord_2.y - image_center_2.y) / _S95;
    float3  rgb_out_4 = rgb_in_2 * make_float3 ((F32_exp2((p_2.exposure_1))));
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
    float2  bd_2 = mul_0(makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f), p_2.color_params_1.b_0);
    float2  rd_2 = mul_0(makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f), p_2.color_params_1.r_0);
    float2  gd_2 = mul_0(makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f), p_2.color_params_1.g_0);
    float2  nd_2 = mul_0(makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f), p_2.color_params_1.n_0);
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
    float norm_factor_2 = intensity_2 / (F32_max((rgi_out_2.z), (0.00009999999747379f * (F32_abs((intensity_2))) + 9.99999993922529029e-09f)));
    float out_r_2 = rgi_out_2.x * norm_factor_2;
    float out_g_2 = rgi_out_2.y * norm_factor_2;
    float3  _S103 = make_float3 (out_r_2, out_g_2, intensity_2 - out_r_2 - out_g_2);
    float3  rgb_0;
    if(clamp_output_0)
    {
        rgb_0 = clamp_1(_S103, make_float3 (0.0f), make_float3 (1.0f));
    }
    else
    {
        rgb_0 = _S103;
    }
    return rgb_0;
}

inline __device__ float3  apply_ppisp_no_crf_no_vig(float3  rgb_in_3, float2  pix_coord_3, float2  image_center_3, float2  img_size_3, FixedArray<float, 9>  params_3, bool clamp_output_1)
{
    PPISPParamsNoCRFNoVig_0 p_3;
    (&p_3)->exposure_0 = params_3[int(0)];
    *&((&(&(&p_3)->color_params_0)->b_0)->x) = params_3[int(1)];
    *&((&(&(&p_3)->color_params_0)->b_0)->y) = params_3[int(2)];
    *&((&(&(&p_3)->color_params_0)->r_0)->x) = params_3[int(3)];
    *&((&(&(&p_3)->color_params_0)->r_0)->y) = params_3[int(4)];
    *&((&(&(&p_3)->color_params_0)->g_0)->x) = params_3[int(5)];
    *&((&(&(&p_3)->color_params_0)->g_0)->y) = params_3[int(6)];
    *&((&(&(&p_3)->color_params_0)->n_0)->x) = params_3[int(7)];
    *&((&(&(&p_3)->color_params_0)->n_0)->y) = params_3[int(8)];
    float3  _S104 = rgb_in_3 * make_float3 ((F32_exp2((p_3.exposure_0))));
    float2  bd_3 = mul_0(makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f), p_3.color_params_0.b_0);
    float2  rd_3 = mul_0(makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f), p_3.color_params_0.r_0);
    float2  gd_3 = mul_0(makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f), p_3.color_params_0.g_0);
    float2  nd_3 = mul_0(makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f), p_3.color_params_0.n_0);
    float _S105 = 0.3333333432674408f + nd_3.x;
    float _S106 = 0.3333333432674408f + nd_3.y;
    Matrix<float, 3, 3>  T_3 = makeMatrix<float, 3, 3> (bd_3.x, 1.0f + rd_3.x, gd_3.x, bd_3.y, rd_3.y, 1.0f + gd_3.y, 1.0f, 1.0f, 1.0f);
    Matrix<float, 3, 3>  M_3 = mul_3(makeMatrix<float, 3, 3> (0.0f, -1.0f, _S106, 1.0f, 0.0f, - _S105, - _S106, _S105, 0.0f), T_3);
    float3  r0_3 = make_float3 (M_3.rows[int(0)].x, M_3.rows[int(0)].y, M_3.rows[int(0)].z);
    float3  r1_3 = make_float3 (M_3.rows[int(1)].x, M_3.rows[int(1)].y, M_3.rows[int(1)].z);
    float3  r2_12 = make_float3 (M_3.rows[int(2)].x, M_3.rows[int(2)].y, M_3.rows[int(2)].z);
    float3  lambda_v_9 = cross_0(r0_3, r1_3);
    float3  lambda_v_10;
    if((dot_0(lambda_v_9, lambda_v_9)) < 9.99999968265522539e-21f)
    {
        float3  lambda_v_11 = cross_0(r0_3, r2_12);
        if((dot_0(lambda_v_11, lambda_v_11)) < 9.99999968265522539e-21f)
        {
            lambda_v_10 = cross_0(r1_3, r2_12);
        }
        else
        {
            lambda_v_10 = lambda_v_11;
        }
    }
    else
    {
        lambda_v_10 = lambda_v_9;
    }
    Matrix<float, 3, 3>  H_6 = mul_3(mul_3(T_3, makeMatrix<float, 3, 3> (lambda_v_10.x, 0.0f, 0.0f, 0.0f, lambda_v_10.y, 0.0f, 0.0f, 0.0f, lambda_v_10.z)), makeMatrix<float, 3, 3> (-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f));
    Matrix<float, 3, 3>  H_7;
    if((F32_abs((H_6.rows[int(2)].z))) > 9.99999968265522539e-21f)
    {
        H_7 = H_6 * makeMatrix<float, 3, 3> (1.0f / H_6.rows[int(2)].z);
    }
    else
    {
        H_7 = H_6;
    }
    float _S107 = _S104.x;
    float _S108 = _S104.y;
    float intensity_3 = _S107 + _S108 + _S104.z;
    float3  rgi_out_3 = mul_1(H_7, make_float3 (_S107, _S108, intensity_3));
    float norm_factor_3 = intensity_3 / (F32_max((rgi_out_3.z), (0.00009999999747379f * (F32_abs((intensity_3))) + 9.99999993922529029e-09f)));
    float out_r_3 = rgi_out_3.x * norm_factor_3;
    float out_g_3 = rgi_out_3.y * norm_factor_3;
    float3  _S109 = make_float3 (out_r_3, out_g_3, intensity_3 - out_r_3 - out_g_3);
    float3  rgb_1;
    if(clamp_output_1)
    {
        rgb_1 = clamp_1(_S109, make_float3 (0.0f), make_float3 (1.0f));
    }
    else
    {
        rgb_1 = _S109;
    }
    return rgb_1;
}

struct DiffPair_arrayx3Cfloatx2C36x3E_0
{
    FixedArray<float, 36>  primal_0;
    FixedArray<float, 36>  differential_0;
};

inline __device__ float s_primal_ctx_exp2_0(float _S110)
{
    return (F32_exp2((_S110)));
}

inline __device__ float s_primal_ctx_clamp_0(float _S111, float _S112, float _S113)
{
    return clamp_0(_S111, _S112, _S113);
}

inline __device__ float2  s_primal_ctx_mul_0(Matrix<float, 2, 2>  _S114, float2  _S115)
{
    return mul_0(_S114, _S115);
}

inline __device__ Matrix<float, 3, 3>  s_primal_ctx_mul_1(Matrix<float, 3, 3>  _S116, Matrix<float, 3, 3>  _S117)
{
    return mul_3(_S116, _S117);
}

inline __device__ float3  s_primal_ctx_cross_0(float3  _S118, float3  _S119)
{
    return cross_0(_S118, _S119);
}

inline __device__ float s_primal_ctx_dot_0(float3  _S120, float3  _S121)
{
    return dot_0(_S120, _S121);
}

inline __device__ float s_primal_ctx_abs_0(float _S122)
{
    return (F32_abs((_S122)));
}

inline __device__ float3  s_primal_ctx_mul_2(Matrix<float, 3, 3>  _S123, float3  _S124)
{
    return mul_1(_S123, _S124);
}

inline __device__ float3  s_primal_ctx_clamp_1(float3  _S125, float3  _S126, float3  _S127)
{
    return clamp_1(_S125, _S126, _S127);
}

inline __device__ float s_primal_ctx_exp_0(float _S128)
{
    return (F32_exp((_S128)));
}

inline __device__ float s_primal_ctx_log_0(float _S129)
{
    return (F32_log((_S129)));
}

inline __device__ float s_primal_ctx_lerp_0(float _S130, float _S131, float _S132)
{
    return lerp_0(_S130, _S131, _S132);
}

inline __device__ float s_primal_ctx_pow_0(float _S133, float _S134)
{
    return (F32_pow((_S133), (_S134)));
}

inline __device__ void s_bwd_prop_pow_0(DiffPair_float_0 * _S135, DiffPair_float_0 * _S136, float _S137)
{
    _d_pow_0(_S135, _S136, _S137);
    return;
}

inline __device__ void s_bwd_prop_lerp_0(DiffPair_float_0 * _S138, DiffPair_float_0 * _S139, DiffPair_float_0 * _S140, float _S141)
{
    _d_lerp_0(_S138, _S139, _S140, _S141);
    return;
}

inline __device__ void s_bwd_prop_exp_0(DiffPair_float_0 * _S142, float _S143)
{
    _d_exp_0(_S142, _S143);
    return;
}

inline __device__ void s_bwd_prop_log_0(DiffPair_float_0 * _S144, float _S145)
{
    _d_log_0(_S144, _S145);
    return;
}

inline __device__ void s_bwd_prop_clamp_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S146, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S147, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S148, float3  _S149)
{
    _d_clamp_vector_0(_S146, _S147, _S148, _S149);
    return;
}

inline __device__ void s_bwd_prop_abs_0(DiffPair_float_0 * _S150, float _S151)
{
    _d_abs_0(_S150, _S151);
    return;
}

inline __device__ void s_bwd_prop_mul_0(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S152, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S153, float3  _S154)
{
    _d_mul_1(_S152, _S153, _S154);
    return;
}

inline __device__ void s_bwd_prop_mul_1(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S155, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S156, Matrix<float, 3, 3>  _S157)
{
    mul_2(_S155, _S156, _S157);
    return;
}

inline __device__ void s_bwd_prop_cross_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S158, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S159, float3  _S160)
{
    _d_cross_0(_S158, _S159, _S160);
    return;
}

inline __device__ void s_bwd_prop_dot_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S161, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S162, float _S163)
{
    _d_dot_0(_S161, _S162, _S163);
    return;
}

inline __device__ void s_bwd_prop_mul_2(DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 * _S164, DiffPair_vectorx3Cfloatx2C2x3E_0 * _S165, float2  _S166)
{
    _d_mul_0(_S164, _S165, _S166);
    return;
}

inline __device__ void s_bwd_prop_clamp_1(DiffPair_float_0 * _S167, DiffPair_float_0 * _S168, DiffPair_float_0 * _S169, float _S170)
{
    _d_clamp_0(_S167, _S168, _S169, _S170);
    return;
}

inline __device__ void s_bwd_prop_exp2_0(DiffPair_float_0 * _S171, float _S172)
{
    _d_exp2_0(_S171, _S172);
    return;
}

inline __device__ void s_bwd_prop_apply_ppisp_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dprgb_in_0, float2  pix_coord_4, float2  image_center_4, float2  img_size_4, DiffPair_arrayx3Cfloatx2C36x3E_0 * dpparams_0, float3  _s_dOut_0)
{
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S173 = *dprgb_in_0;
    float3  _S174 = make_float3 (0.0f);
    Matrix<float, 3, 3>  _S175 = makeMatrix<float, 3, 3> (0.0f);
    VignettingChannelParams_0 _S176 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    FixedArray<VignettingChannelParams_0, 3>  _S177 = {
        _S176, _S176, _S176
    };
    float2  _S178 = make_float2 (0.0f);
    ColorPPISPParams_0 _S179 = { _S178, _S178, _S178, _S178 };
    CRFPPISPChannelParams_0 _S180 = { 0.0f, 0.0f, 0.0f, 0.0f };
    FixedArray<CRFPPISPChannelParams_0, 3>  _S181 = {
        _S180, _S180, _S180
    };
    PPISPParams_0 _S182;
    (&_S182)->exposure_3 = dpparams_0->primal_0[int(0)];
    (&_S182)->vignette_params_2 = _S177;
    (&_S182)->color_params_3 = _S179;
    (&_S182)->crf_params_1 = _S181;
    (&(&_S182)->vignette_params_2[int(0)])->cx_0 = dpparams_0->primal_0[int(1)];
    (&(&_S182)->vignette_params_2[int(0)])->cy_0 = dpparams_0->primal_0[int(2)];
    float _S183 = dpparams_0->primal_0[int(3)];
    (&(&_S182)->vignette_params_2[int(0)])->alpha0_0 = dpparams_0->primal_0[int(3)];
    float _S184 = dpparams_0->primal_0[int(4)];
    (&(&_S182)->vignette_params_2[int(0)])->alpha1_0 = dpparams_0->primal_0[int(4)];
    float _S185 = dpparams_0->primal_0[int(5)];
    (&(&_S182)->vignette_params_2[int(0)])->alpha2_0 = dpparams_0->primal_0[int(5)];
    (&(&_S182)->vignette_params_2[int(1)])->cx_0 = dpparams_0->primal_0[int(6)];
    (&(&_S182)->vignette_params_2[int(1)])->cy_0 = dpparams_0->primal_0[int(7)];
    float _S186 = dpparams_0->primal_0[int(8)];
    (&(&_S182)->vignette_params_2[int(1)])->alpha0_0 = dpparams_0->primal_0[int(8)];
    float _S187 = dpparams_0->primal_0[int(9)];
    (&(&_S182)->vignette_params_2[int(1)])->alpha1_0 = dpparams_0->primal_0[int(9)];
    float _S188 = dpparams_0->primal_0[int(10)];
    (&(&_S182)->vignette_params_2[int(1)])->alpha2_0 = dpparams_0->primal_0[int(10)];
    (&(&_S182)->vignette_params_2[int(2)])->cx_0 = dpparams_0->primal_0[int(11)];
    (&(&_S182)->vignette_params_2[int(2)])->cy_0 = dpparams_0->primal_0[int(12)];
    float _S189 = dpparams_0->primal_0[int(13)];
    (&(&_S182)->vignette_params_2[int(2)])->alpha0_0 = dpparams_0->primal_0[int(13)];
    float _S190 = dpparams_0->primal_0[int(14)];
    (&(&_S182)->vignette_params_2[int(2)])->alpha1_0 = dpparams_0->primal_0[int(14)];
    float _S191 = dpparams_0->primal_0[int(15)];
    (&(&_S182)->vignette_params_2[int(2)])->alpha2_0 = dpparams_0->primal_0[int(15)];
    *&((&(&(&_S182)->color_params_3)->b_0)->x) = dpparams_0->primal_0[int(16)];
    *&((&(&(&_S182)->color_params_3)->b_0)->y) = dpparams_0->primal_0[int(17)];
    *&((&(&(&_S182)->color_params_3)->r_0)->x) = dpparams_0->primal_0[int(18)];
    *&((&(&(&_S182)->color_params_3)->r_0)->y) = dpparams_0->primal_0[int(19)];
    *&((&(&(&_S182)->color_params_3)->g_0)->x) = dpparams_0->primal_0[int(20)];
    *&((&(&(&_S182)->color_params_3)->g_0)->y) = dpparams_0->primal_0[int(21)];
    *&((&(&(&_S182)->color_params_3)->n_0)->x) = dpparams_0->primal_0[int(22)];
    *&((&(&(&_S182)->color_params_3)->n_0)->y) = dpparams_0->primal_0[int(23)];
    float _S192 = dpparams_0->primal_0[int(24)];
    (&(&_S182)->crf_params_1[int(0)])->toe_0 = dpparams_0->primal_0[int(24)];
    float _S193 = dpparams_0->primal_0[int(25)];
    (&(&_S182)->crf_params_1[int(0)])->shoulder_0 = dpparams_0->primal_0[int(25)];
    float _S194 = dpparams_0->primal_0[int(26)];
    (&(&_S182)->crf_params_1[int(0)])->gamma_0 = dpparams_0->primal_0[int(26)];
    float _S195 = dpparams_0->primal_0[int(27)];
    (&(&_S182)->crf_params_1[int(0)])->center_0 = dpparams_0->primal_0[int(27)];
    float _S196 = dpparams_0->primal_0[int(28)];
    (&(&_S182)->crf_params_1[int(1)])->toe_0 = dpparams_0->primal_0[int(28)];
    float _S197 = dpparams_0->primal_0[int(29)];
    (&(&_S182)->crf_params_1[int(1)])->shoulder_0 = dpparams_0->primal_0[int(29)];
    float _S198 = dpparams_0->primal_0[int(30)];
    (&(&_S182)->crf_params_1[int(1)])->gamma_0 = dpparams_0->primal_0[int(30)];
    float _S199 = dpparams_0->primal_0[int(31)];
    (&(&_S182)->crf_params_1[int(1)])->center_0 = dpparams_0->primal_0[int(31)];
    float _S200 = dpparams_0->primal_0[int(32)];
    (&(&_S182)->crf_params_1[int(2)])->toe_0 = dpparams_0->primal_0[int(32)];
    float _S201 = dpparams_0->primal_0[int(33)];
    (&(&_S182)->crf_params_1[int(2)])->shoulder_0 = dpparams_0->primal_0[int(33)];
    float _S202 = dpparams_0->primal_0[int(34)];
    (&(&_S182)->crf_params_1[int(2)])->gamma_0 = dpparams_0->primal_0[int(34)];
    float _S203 = dpparams_0->primal_0[int(35)];
    (&(&_S182)->crf_params_1[int(2)])->center_0 = dpparams_0->primal_0[int(35)];
    PPISPParams_0 _S204 = _S182;
    float _S205 = s_primal_ctx_exp2_0(_S182.exposure_3);
    float3  _S206 = make_float3 (_S205);
    float3  rgb_out_5 = (*dprgb_in_0).primal_0 * make_float3 (_S205);
    float _S207 = (F32_max((img_size_4.x), (img_size_4.y)));
    float _S208 = (pix_coord_4.x - image_center_4.x) / _S207;
    float _S209 = (pix_coord_4.y - image_center_4.y) / _S207;
    float dx_9 = _S208 - dpparams_0->primal_0[int(1)];
    float dy_9 = _S209 - dpparams_0->primal_0[int(2)];
    float r2_13 = dx_9 * dx_9 + dy_9 * dy_9;
    float r4_9 = r2_13 * r2_13;
    float r6_0 = r4_9 * r2_13;
    float falloff_0 = dpparams_0->primal_0[int(5)] * r6_0 + dpparams_0->primal_0[int(4)] * r4_9 + dpparams_0->primal_0[int(3)] * r2_13 + 1.0f;
    float _S210 = s_primal_ctx_clamp_0(falloff_0, 0.0f, 1.0f);
    float _S211 = rgb_out_5.x * _S210;
    float3  _S212 = rgb_out_5;
    *&((&_S212)->x) = _S211;
    float dx_10 = _S208 - dpparams_0->primal_0[int(6)];
    float dy_10 = _S209 - dpparams_0->primal_0[int(7)];
    float r2_14 = dx_10 * dx_10 + dy_10 * dy_10;
    float r4_10 = r2_14 * r2_14;
    float r6_1 = r4_10 * r2_14;
    float falloff_1 = dpparams_0->primal_0[int(10)] * r6_1 + dpparams_0->primal_0[int(9)] * r4_10 + dpparams_0->primal_0[int(8)] * r2_14 + 1.0f;
    float _S213 = s_primal_ctx_clamp_0(falloff_1, 0.0f, 1.0f);
    *&((&_S212)->y) = rgb_out_5.y * _S213;
    float dx_11 = _S208 - dpparams_0->primal_0[int(11)];
    float dy_11 = _S209 - dpparams_0->primal_0[int(12)];
    float r2_15 = dx_11 * dx_11 + dy_11 * dy_11;
    float r4_11 = r2_15 * r2_15;
    float r6_2 = r4_11 * r2_15;
    float falloff_2 = dpparams_0->primal_0[int(15)] * r6_2 + dpparams_0->primal_0[int(14)] * r4_11 + dpparams_0->primal_0[int(13)] * r2_15 + 1.0f;
    float _S214 = s_primal_ctx_clamp_0(falloff_2, 0.0f, 1.0f);
    *&((&_S212)->z) = rgb_out_5.z * _S214;
    PPISPParams_0 _S215 = _S182;
    float2  _S216 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f), _S182.color_params_3.b_0);
    float2  _S217 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f), _S182.color_params_3.r_0);
    float2  _S218 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f), _S182.color_params_3.g_0);
    float2  _S219 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f), _S182.color_params_3.n_0);
    float _S220 = 0.3333333432674408f + _S219.x;
    float _S221 = 0.3333333432674408f + _S219.y;
    Matrix<float, 3, 3>  T_4 = makeMatrix<float, 3, 3> (_S216.x, 1.0f + _S217.x, _S218.x, _S216.y, _S217.y, 1.0f + _S218.y, 1.0f, 1.0f, 1.0f);
    Matrix<float, 3, 3>  skew_0 = makeMatrix<float, 3, 3> (0.0f, -1.0f, _S221, 1.0f, 0.0f, - _S220, - _S221, _S220, 0.0f);
    Matrix<float, 3, 3>  _S222 = s_primal_ctx_mul_1(skew_0, T_4);
    float3  r0_4 = make_float3 (_S222.rows[int(0)].x, _S222.rows[int(0)].y, _S222.rows[int(0)].z);
    float3  r1_4 = make_float3 (_S222.rows[int(1)].x, _S222.rows[int(1)].y, _S222.rows[int(1)].z);
    float3  r2_16 = make_float3 (_S222.rows[int(2)].x, _S222.rows[int(2)].y, _S222.rows[int(2)].z);
    float3  _S223 = s_primal_ctx_cross_0(r0_4, r1_4);
    bool _S224 = (s_primal_ctx_dot_0(_S223, _S223)) < 9.99999968265522539e-21f;
    float3  lambda_v_12;
    float3  _S225;
    bool _S226;
    if(_S224)
    {
        float3  _S227 = s_primal_ctx_cross_0(r0_4, r2_16);
        bool _S228 = (s_primal_ctx_dot_0(_S227, _S227)) < 9.99999968265522539e-21f;
        if(_S228)
        {
            lambda_v_12 = s_primal_ctx_cross_0(r1_4, r2_16);
        }
        else
        {
            lambda_v_12 = _S227;
        }
        _S226 = _S228;
        _S225 = _S227;
    }
    else
    {
        lambda_v_12 = _S223;
        _S226 = false;
        _S225 = _S174;
    }
    Matrix<float, 3, 3>  S_inv_0 = makeMatrix<float, 3, 3> (-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    Matrix<float, 3, 3>  D_0 = makeMatrix<float, 3, 3> (lambda_v_12.x, 0.0f, 0.0f, 0.0f, lambda_v_12.y, 0.0f, 0.0f, 0.0f, lambda_v_12.z);
    Matrix<float, 3, 3>  _S229 = s_primal_ctx_mul_1(T_4, D_0);
    Matrix<float, 3, 3>  _S230 = s_primal_ctx_mul_1(_S229, S_inv_0);
    bool _S231 = (s_primal_ctx_abs_0(_S230.rows[int(2)].z)) > 9.99999968265522539e-21f;
    Matrix<float, 3, 3>  H_8;
    Matrix<float, 3, 3>  _S232;
    float _S233;
    if(_S231)
    {
        float inv_s_0 = 1.0f / _S230.rows[int(2)].z;
        Matrix<float, 3, 3>  _S234 = makeMatrix<float, 3, 3> (inv_s_0);
        float _S235 = _S230.rows[int(2)].z * _S230.rows[int(2)].z;
        H_8 = _S230 * makeMatrix<float, 3, 3> (inv_s_0);
        _S232 = _S234;
        _S233 = _S235;
    }
    else
    {
        H_8 = _S230;
        _S232 = _S175;
        _S233 = 0.0f;
    }
    float _S236 = _S212.x;
    float _S237 = _S212.y;
    float intensity_4 = _S236 + _S237 + _S212.z;
    float3  rgi_in_0 = make_float3 (_S236, _S237, intensity_4);
    float3  _S238 = s_primal_ctx_mul_2(H_8, rgi_in_0);
    float _S239 = _S238.z;
    float _S240 = 0.00009999999747379f * s_primal_ctx_abs_0(intensity_4) + 9.99999993922529029e-09f;
    float _S241 = (F32_max((_S239), (_S240)));
    float norm_factor_4 = intensity_4 / _S241;
    float _S242 = _S241 * _S241;
    float _S243 = _S238.x;
    float out_r_4 = _S243 * norm_factor_4;
    float _S244 = _S238.y;
    float out_g_4 = _S244 * norm_factor_4;
    float3  _S245 = make_float3 (out_r_4, out_g_4, intensity_4 - out_r_4 - out_g_4);
    float3  _S246 = make_float3 (0.0f);
    float3  _S247 = make_float3 (1.0f);
    float3  _S248 = s_primal_ctx_clamp_1(_S245, _S246, _S247);
    float _S249 = _S248.x;
    float _S250 = 1.0f + s_primal_ctx_exp_0(_S192);
    float _S251 = 0.30000001192092896f + s_primal_ctx_log_0(_S250);
    float _S252 = 1.0f + s_primal_ctx_exp_0(_S193);
    float _S253 = 0.30000001192092896f + s_primal_ctx_log_0(_S252);
    float _S254 = 1.0f + s_primal_ctx_exp_0(_S194);
    float _S255 = 0.10000000149011612f + s_primal_ctx_log_0(_S254);
    float _S256 = - _S195;
    float _S257 = 1.0f + s_primal_ctx_exp_0(_S256);
    float _S258 = 1.0f / _S257;
    float _S259 = _S257 * _S257;
    float _S260 = s_primal_ctx_lerp_0(_S251, _S253, _S258);
    float _S261 = _S253 * _S258;
    float a_4 = _S261 / _S260;
    float _S262 = _S260 * _S260;
    float b_5 = 1.0f - a_4;
    bool _S263 = _S249 <= _S258;
    float y_6;
    float _S264;
    float _S265;
    float _S266;
    float _S267;
    float _S268;
    float _S269;
    float _S270;
    float _S271;
    if(_S263)
    {
        float _S272 = _S249 / _S258;
        float _S273 = _S258 * _S258;
        float _S274 = s_primal_ctx_pow_0(_S272, _S251);
        y_6 = a_4 * _S274;
        _S264 = _S274;
        _S265 = _S272;
        _S266 = _S273;
        _S267 = 0.0f;
        _S268 = 0.0f;
        _S269 = 0.0f;
        _S270 = 0.0f;
        _S271 = 0.0f;
    }
    else
    {
        float _S275 = 1.0f - _S249;
        float _S276 = 1.0f - _S258;
        float _S277 = _S275 / _S276;
        float _S278 = _S276 * _S276;
        float _S279 = s_primal_ctx_pow_0(_S277, _S253);
        y_6 = 1.0f - b_5 * _S279;
        _S264 = 0.0f;
        _S265 = 0.0f;
        _S266 = 0.0f;
        _S267 = _S279;
        _S268 = _S277;
        _S269 = _S278;
        _S270 = _S275;
        _S271 = _S276;
    }
    float _S280 = (F32_max((0.0f), (y_6)));
    float _S281 = _S248.y;
    float _S282 = 1.0f + s_primal_ctx_exp_0(_S196);
    float _S283 = 0.30000001192092896f + s_primal_ctx_log_0(_S282);
    float _S284 = 1.0f + s_primal_ctx_exp_0(_S197);
    float _S285 = 0.30000001192092896f + s_primal_ctx_log_0(_S284);
    float _S286 = 1.0f + s_primal_ctx_exp_0(_S198);
    float _S287 = 0.10000000149011612f + s_primal_ctx_log_0(_S286);
    float _S288 = - _S199;
    float _S289 = 1.0f + s_primal_ctx_exp_0(_S288);
    float _S290 = 1.0f / _S289;
    float _S291 = _S289 * _S289;
    float _S292 = s_primal_ctx_lerp_0(_S283, _S285, _S290);
    float _S293 = _S285 * _S290;
    float a_5 = _S293 / _S292;
    float _S294 = _S292 * _S292;
    float b_6 = 1.0f - a_5;
    bool _S295 = _S281 <= _S290;
    float y_7;
    float _S296;
    float _S297;
    float _S298;
    float _S299;
    float _S300;
    float _S301;
    float _S302;
    float _S303;
    if(_S295)
    {
        float _S304 = _S281 / _S290;
        float _S305 = _S290 * _S290;
        float _S306 = s_primal_ctx_pow_0(_S304, _S283);
        y_7 = a_5 * _S306;
        _S296 = _S306;
        _S297 = _S304;
        _S298 = _S305;
        _S299 = 0.0f;
        _S300 = 0.0f;
        _S301 = 0.0f;
        _S302 = 0.0f;
        _S303 = 0.0f;
    }
    else
    {
        float _S307 = 1.0f - _S281;
        float _S308 = 1.0f - _S290;
        float _S309 = _S307 / _S308;
        float _S310 = _S308 * _S308;
        float _S311 = s_primal_ctx_pow_0(_S309, _S285);
        y_7 = 1.0f - b_6 * _S311;
        _S296 = 0.0f;
        _S297 = 0.0f;
        _S298 = 0.0f;
        _S299 = _S311;
        _S300 = _S309;
        _S301 = _S310;
        _S302 = _S307;
        _S303 = _S308;
    }
    float _S312 = (F32_max((0.0f), (y_7)));
    float _S313 = _S248.z;
    float _S314 = 1.0f + s_primal_ctx_exp_0(_S200);
    float _S315 = 0.30000001192092896f + s_primal_ctx_log_0(_S314);
    float _S316 = 1.0f + s_primal_ctx_exp_0(_S201);
    float _S317 = 0.30000001192092896f + s_primal_ctx_log_0(_S316);
    float _S318 = 1.0f + s_primal_ctx_exp_0(_S202);
    float _S319 = 0.10000000149011612f + s_primal_ctx_log_0(_S318);
    float _S320 = - _S203;
    float _S321 = 1.0f + s_primal_ctx_exp_0(_S320);
    float _S322 = 1.0f / _S321;
    float _S323 = _S321 * _S321;
    float _S324 = s_primal_ctx_lerp_0(_S315, _S317, _S322);
    float _S325 = _S317 * _S322;
    float a_6 = _S325 / _S324;
    float _S326 = _S324 * _S324;
    float b_7 = 1.0f - a_6;
    bool _S327 = _S313 <= _S322;
    float y_8;
    float _S328;
    float _S329;
    float _S330;
    float _S331;
    float _S332;
    float _S333;
    float _S334;
    float _S335;
    if(_S327)
    {
        float _S336 = _S313 / _S322;
        float _S337 = _S322 * _S322;
        float _S338 = s_primal_ctx_pow_0(_S336, _S315);
        y_8 = a_6 * _S338;
        _S328 = _S338;
        _S329 = _S336;
        _S330 = _S337;
        _S331 = 0.0f;
        _S332 = 0.0f;
        _S333 = 0.0f;
        _S334 = 0.0f;
        _S335 = 0.0f;
    }
    else
    {
        float _S339 = 1.0f - _S313;
        float _S340 = 1.0f - _S322;
        float _S341 = _S339 / _S340;
        float _S342 = _S340 * _S340;
        float _S343 = s_primal_ctx_pow_0(_S341, _S317);
        y_8 = 1.0f - b_7 * _S343;
        _S328 = 0.0f;
        _S329 = 0.0f;
        _S330 = 0.0f;
        _S331 = _S343;
        _S332 = _S341;
        _S333 = _S342;
        _S334 = _S339;
        _S335 = _S340;
    }
    float _S344 = (F32_max((0.0f), (y_8)));
    DiffPair_float_0 _S345;
    (&_S345)->primal_0 = _S344;
    (&_S345)->differential_0 = 0.0f;
    DiffPair_float_0 _S346;
    (&_S346)->primal_0 = _S319;
    (&_S346)->differential_0 = 0.0f;
    s_bwd_prop_pow_0(&_S345, &_S346, _s_dOut_0.z);
    DiffPair_float_0 _S347 = _S346;
    DiffPair_float_0 _S348;
    (&_S348)->primal_0 = 0.0f;
    (&_S348)->differential_0 = 0.0f;
    DiffPair_float_0 _S349;
    (&_S349)->primal_0 = y_8;
    (&_S349)->differential_0 = 0.0f;
    _d_max_0(&_S348, &_S349, _S345.differential_0);
    DiffPair_float_0 _S350 = _S349;
    if(_S327)
    {
        float _S351 = a_6 * _S350.differential_0;
        float _S352 = _S328 * _S350.differential_0;
        DiffPair_float_0 _S353;
        (&_S353)->primal_0 = _S329;
        (&_S353)->differential_0 = 0.0f;
        DiffPair_float_0 _S354;
        (&_S354)->primal_0 = _S315;
        (&_S354)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S353, &_S354, _S351);
        float _S355 = _S353.differential_0 / _S330;
        float _S356 = _S313 * - _S355;
        float _S357 = _S322 * _S355;
        y_8 = 0.0f;
        _S328 = _S352;
        _S329 = _S356;
        _S330 = 0.0f;
        _S331 = _S354.differential_0;
        _S332 = _S357;
    }
    else
    {
        float _S358 = - _S350.differential_0;
        float _S359 = b_7 * _S358;
        float _S360 = _S331 * _S358;
        DiffPair_float_0 _S361;
        (&_S361)->primal_0 = _S332;
        (&_S361)->differential_0 = 0.0f;
        DiffPair_float_0 _S362;
        (&_S362)->primal_0 = _S317;
        (&_S362)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S361, &_S362, _S359);
        float _S363 = _S361.differential_0 / _S333;
        float _S364 = - (_S334 * - _S363);
        float _S365 = - (_S335 * _S363);
        y_8 = _S360;
        _S328 = 0.0f;
        _S329 = _S364;
        _S330 = _S362.differential_0;
        _S331 = 0.0f;
        _S332 = _S365;
    }
    float _S366 = (- y_8 + _S328) / _S326;
    float _S367 = _S325 * - _S366;
    float _S368 = _S324 * _S366;
    float _S369 = _S317 * _S368;
    float _S370 = _S322 * _S368;
    DiffPair_float_0 _S371;
    (&_S371)->primal_0 = _S315;
    (&_S371)->differential_0 = 0.0f;
    DiffPair_float_0 _S372;
    (&_S372)->primal_0 = _S317;
    (&_S372)->differential_0 = 0.0f;
    DiffPair_float_0 _S373;
    (&_S373)->primal_0 = _S322;
    (&_S373)->differential_0 = 0.0f;
    s_bwd_prop_lerp_0(&_S371, &_S372, &_S373, _S367);
    float _S374 = - ((_S369 + _S373.differential_0 + _S329) / _S323);
    DiffPair_float_0 _S375;
    (&_S375)->primal_0 = _S320;
    (&_S375)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S375, _S374);
    float _S376 = - _S375.differential_0;
    DiffPair_float_0 _S377;
    (&_S377)->primal_0 = _S318;
    (&_S377)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S377, _S347.differential_0);
    DiffPair_float_0 _S378;
    (&_S378)->primal_0 = _S202;
    (&_S378)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S378, _S377.differential_0);
    DiffPair_float_0 _S379 = _S378;
    float _S380 = _S370 + _S372.differential_0 + _S330;
    DiffPair_float_0 _S381;
    (&_S381)->primal_0 = _S316;
    (&_S381)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S381, _S380);
    DiffPair_float_0 _S382;
    (&_S382)->primal_0 = _S201;
    (&_S382)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S382, _S381.differential_0);
    DiffPair_float_0 _S383 = _S382;
    float _S384 = _S371.differential_0 + _S331;
    DiffPair_float_0 _S385;
    (&_S385)->primal_0 = _S314;
    (&_S385)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S385, _S384);
    DiffPair_float_0 _S386;
    (&_S386)->primal_0 = _S200;
    (&_S386)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S386, _S385.differential_0);
    DiffPair_float_0 _S387 = _S386;
    float3  _S388 = make_float3 (0.0f, 0.0f, _S332);
    DiffPair_float_0 _S389;
    (&_S389)->primal_0 = _S312;
    (&_S389)->differential_0 = 0.0f;
    DiffPair_float_0 _S390;
    (&_S390)->primal_0 = _S287;
    (&_S390)->differential_0 = 0.0f;
    s_bwd_prop_pow_0(&_S389, &_S390, _s_dOut_0.y);
    DiffPair_float_0 _S391 = _S390;
    DiffPair_float_0 _S392;
    (&_S392)->primal_0 = 0.0f;
    (&_S392)->differential_0 = 0.0f;
    DiffPair_float_0 _S393;
    (&_S393)->primal_0 = y_7;
    (&_S393)->differential_0 = 0.0f;
    _d_max_0(&_S392, &_S393, _S389.differential_0);
    DiffPair_float_0 _S394 = _S393;
    if(_S295)
    {
        float _S395 = a_5 * _S394.differential_0;
        float _S396 = _S296 * _S394.differential_0;
        DiffPair_float_0 _S397;
        (&_S397)->primal_0 = _S297;
        (&_S397)->differential_0 = 0.0f;
        DiffPair_float_0 _S398;
        (&_S398)->primal_0 = _S283;
        (&_S398)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S397, &_S398, _S395);
        float _S399 = _S397.differential_0 / _S298;
        float _S400 = _S281 * - _S399;
        float _S401 = _S290 * _S399;
        y_7 = 0.0f;
        _S296 = _S396;
        _S297 = _S400;
        _S298 = 0.0f;
        _S299 = _S398.differential_0;
        _S300 = _S401;
    }
    else
    {
        float _S402 = - _S394.differential_0;
        float _S403 = b_6 * _S402;
        float _S404 = _S299 * _S402;
        DiffPair_float_0 _S405;
        (&_S405)->primal_0 = _S300;
        (&_S405)->differential_0 = 0.0f;
        DiffPair_float_0 _S406;
        (&_S406)->primal_0 = _S285;
        (&_S406)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S405, &_S406, _S403);
        float _S407 = _S405.differential_0 / _S301;
        float _S408 = - (_S302 * - _S407);
        float _S409 = - (_S303 * _S407);
        y_7 = _S404;
        _S296 = 0.0f;
        _S297 = _S408;
        _S298 = _S406.differential_0;
        _S299 = 0.0f;
        _S300 = _S409;
    }
    float _S410 = (- y_7 + _S296) / _S294;
    float _S411 = _S293 * - _S410;
    float _S412 = _S292 * _S410;
    float _S413 = _S285 * _S412;
    float _S414 = _S290 * _S412;
    DiffPair_float_0 _S415;
    (&_S415)->primal_0 = _S283;
    (&_S415)->differential_0 = 0.0f;
    DiffPair_float_0 _S416;
    (&_S416)->primal_0 = _S285;
    (&_S416)->differential_0 = 0.0f;
    DiffPair_float_0 _S417;
    (&_S417)->primal_0 = _S290;
    (&_S417)->differential_0 = 0.0f;
    s_bwd_prop_lerp_0(&_S415, &_S416, &_S417, _S411);
    float _S418 = - ((_S413 + _S417.differential_0 + _S297) / _S291);
    DiffPair_float_0 _S419;
    (&_S419)->primal_0 = _S288;
    (&_S419)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S419, _S418);
    float _S420 = - _S419.differential_0;
    DiffPair_float_0 _S421;
    (&_S421)->primal_0 = _S286;
    (&_S421)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S421, _S391.differential_0);
    DiffPair_float_0 _S422;
    (&_S422)->primal_0 = _S198;
    (&_S422)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S422, _S421.differential_0);
    DiffPair_float_0 _S423 = _S422;
    float _S424 = _S414 + _S416.differential_0 + _S298;
    DiffPair_float_0 _S425;
    (&_S425)->primal_0 = _S284;
    (&_S425)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S425, _S424);
    DiffPair_float_0 _S426;
    (&_S426)->primal_0 = _S197;
    (&_S426)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S426, _S425.differential_0);
    DiffPair_float_0 _S427 = _S426;
    float _S428 = _S415.differential_0 + _S299;
    DiffPair_float_0 _S429;
    (&_S429)->primal_0 = _S282;
    (&_S429)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S429, _S428);
    DiffPair_float_0 _S430;
    (&_S430)->primal_0 = _S196;
    (&_S430)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S430, _S429.differential_0);
    DiffPair_float_0 _S431 = _S430;
    float3  _S432 = _S388 + make_float3 (0.0f, _S300, 0.0f);
    DiffPair_float_0 _S433;
    (&_S433)->primal_0 = _S280;
    (&_S433)->differential_0 = 0.0f;
    DiffPair_float_0 _S434;
    (&_S434)->primal_0 = _S255;
    (&_S434)->differential_0 = 0.0f;
    s_bwd_prop_pow_0(&_S433, &_S434, _s_dOut_0.x);
    DiffPair_float_0 _S435 = _S434;
    DiffPair_float_0 _S436;
    (&_S436)->primal_0 = 0.0f;
    (&_S436)->differential_0 = 0.0f;
    DiffPair_float_0 _S437;
    (&_S437)->primal_0 = y_6;
    (&_S437)->differential_0 = 0.0f;
    _d_max_0(&_S436, &_S437, _S433.differential_0);
    DiffPair_float_0 _S438 = _S437;
    if(_S263)
    {
        float _S439 = a_4 * _S438.differential_0;
        float _S440 = _S264 * _S438.differential_0;
        DiffPair_float_0 _S441;
        (&_S441)->primal_0 = _S265;
        (&_S441)->differential_0 = 0.0f;
        DiffPair_float_0 _S442;
        (&_S442)->primal_0 = _S251;
        (&_S442)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S441, &_S442, _S439);
        float _S443 = _S441.differential_0 / _S266;
        float _S444 = _S249 * - _S443;
        float _S445 = _S258 * _S443;
        y_6 = 0.0f;
        _S264 = _S440;
        _S265 = _S444;
        _S266 = 0.0f;
        _S267 = _S442.differential_0;
        _S268 = _S445;
    }
    else
    {
        float _S446 = - _S438.differential_0;
        float _S447 = b_5 * _S446;
        float _S448 = _S267 * _S446;
        DiffPair_float_0 _S449;
        (&_S449)->primal_0 = _S268;
        (&_S449)->differential_0 = 0.0f;
        DiffPair_float_0 _S450;
        (&_S450)->primal_0 = _S253;
        (&_S450)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S449, &_S450, _S447);
        float _S451 = _S449.differential_0 / _S269;
        float _S452 = - (_S270 * - _S451);
        float _S453 = - (_S271 * _S451);
        y_6 = _S448;
        _S264 = 0.0f;
        _S265 = _S452;
        _S266 = _S450.differential_0;
        _S267 = 0.0f;
        _S268 = _S453;
    }
    float _S454 = (- y_6 + _S264) / _S262;
    float _S455 = _S261 * - _S454;
    float _S456 = _S260 * _S454;
    float _S457 = _S253 * _S456;
    float _S458 = _S258 * _S456;
    DiffPair_float_0 _S459;
    (&_S459)->primal_0 = _S251;
    (&_S459)->differential_0 = 0.0f;
    DiffPair_float_0 _S460;
    (&_S460)->primal_0 = _S253;
    (&_S460)->differential_0 = 0.0f;
    DiffPair_float_0 _S461;
    (&_S461)->primal_0 = _S258;
    (&_S461)->differential_0 = 0.0f;
    s_bwd_prop_lerp_0(&_S459, &_S460, &_S461, _S455);
    float _S462 = - ((_S457 + _S461.differential_0 + _S265) / _S259);
    DiffPair_float_0 _S463;
    (&_S463)->primal_0 = _S256;
    (&_S463)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S463, _S462);
    float _S464 = - _S463.differential_0;
    DiffPair_float_0 _S465;
    (&_S465)->primal_0 = _S254;
    (&_S465)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S465, _S435.differential_0);
    DiffPair_float_0 _S466;
    (&_S466)->primal_0 = _S194;
    (&_S466)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S466, _S465.differential_0);
    DiffPair_float_0 _S467 = _S466;
    float _S468 = _S458 + _S460.differential_0 + _S266;
    DiffPair_float_0 _S469;
    (&_S469)->primal_0 = _S252;
    (&_S469)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S469, _S468);
    DiffPair_float_0 _S470;
    (&_S470)->primal_0 = _S193;
    (&_S470)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S470, _S469.differential_0);
    DiffPair_float_0 _S471 = _S470;
    float _S472 = _S459.differential_0 + _S267;
    DiffPair_float_0 _S473;
    (&_S473)->primal_0 = _S250;
    (&_S473)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S473, _S472);
    DiffPair_float_0 _S474;
    (&_S474)->primal_0 = _S192;
    (&_S474)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S474, _S473.differential_0);
    DiffPair_float_0 _S475 = _S474;
    float3  _S476 = _S432 + make_float3 (_S268, 0.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S477;
    (&_S477)->primal_0 = _S245;
    (&_S477)->differential_0 = _S174;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S478;
    (&_S478)->primal_0 = _S246;
    (&_S478)->differential_0 = _S174;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S479;
    (&_S479)->primal_0 = _S247;
    (&_S479)->differential_0 = _S174;
    s_bwd_prop_clamp_0(&_S477, &_S478, &_S479, _S476);
    float _S480 = - _S477.differential_0.z;
    float _S481 = _S477.differential_0.y + _S480;
    float _S482 = norm_factor_4 * _S481;
    float _S483 = _S477.differential_0.x + _S480;
    float _S484 = norm_factor_4 * _S483;
    float _S485 = (_S244 * _S481 + _S243 * _S483) / _S242;
    float _S486 = intensity_4 * - _S485;
    float _S487 = _S241 * _S485;
    DiffPair_float_0 _S488;
    (&_S488)->primal_0 = _S239;
    (&_S488)->differential_0 = 0.0f;
    DiffPair_float_0 _S489;
    (&_S489)->primal_0 = _S240;
    (&_S489)->differential_0 = 0.0f;
    _d_max_0(&_S488, &_S489, _S486);
    float _S490 = 0.00009999999747379f * _S489.differential_0;
    DiffPair_float_0 _S491;
    (&_S491)->primal_0 = intensity_4;
    (&_S491)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S491, _S490);
    float3  _S492 = make_float3 (_S484, _S482, _S488.differential_0);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S493;
    (&_S493)->primal_0 = H_8;
    (&_S493)->differential_0 = _S175;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S494;
    (&_S494)->primal_0 = rgi_in_0;
    (&_S494)->differential_0 = _S174;
    s_bwd_prop_mul_0(&_S493, &_S494, _S492);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S495 = _S493;
    float _S496 = _S477.differential_0.z + _S487 + _S491.differential_0 + _S494.differential_0.z;
    float _S497 = _S494.differential_0.y + _S496;
    float _S498 = _S494.differential_0.x + _S496;
    float3  _S499 = make_float3 (_S498, _S497, _S496);
    if(_S231)
    {
        Matrix<float, 3, 3>  _S500 = _S230 * _S495.differential_0;
        Matrix<float, 3, 3>  _S501 = _S232 * _S495.differential_0;
        _S233 = - ((_S500.rows[int(0)].x + _S500.rows[int(0)].y + _S500.rows[int(0)].z + _S500.rows[int(1)].x + _S500.rows[int(1)].y + _S500.rows[int(1)].z + _S500.rows[int(2)].x + _S500.rows[int(2)].y + _S500.rows[int(2)].z) / _S233);
        H_8 = _S501;
    }
    else
    {
        _S233 = 0.0f;
        H_8 = _S495.differential_0;
    }
    DiffPair_float_0 _S502;
    (&_S502)->primal_0 = _S230.rows[int(2)].z;
    (&_S502)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S502, 0.0f);
    float _S503 = _S502.differential_0 + _S233;
    float3  _S504 = _S174;
    *&((&_S504)->z) = _S503;
    Matrix<float, 3, 3>  _S505 = _S175;
    _S505[int(2)] = _S504;
    Matrix<float, 3, 3>  _S506 = H_8 + _S505;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S507;
    (&_S507)->primal_0 = _S229;
    (&_S507)->differential_0 = _S175;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S508;
    (&_S508)->primal_0 = S_inv_0;
    (&_S508)->differential_0 = _S175;
    s_bwd_prop_mul_1(&_S507, &_S508, _S506);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S509;
    (&_S509)->primal_0 = T_4;
    (&_S509)->differential_0 = _S175;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S510;
    (&_S510)->primal_0 = D_0;
    (&_S510)->differential_0 = _S175;
    s_bwd_prop_mul_1(&_S509, &_S510, _S507.differential_0);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S511 = _S509;
    float3  _S512 = make_float3 (_S510.differential_0.rows[int(0)].x, _S510.differential_0.rows[int(1)].y, _S510.differential_0.rows[int(2)].z);
    float3  _S513;
    if(_S224)
    {
        if(_S226)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S514;
            (&_S514)->primal_0 = r1_4;
            (&_S514)->differential_0 = _S174;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S515;
            (&_S515)->primal_0 = r2_16;
            (&_S515)->differential_0 = _S174;
            s_bwd_prop_cross_0(&_S514, &_S515, _S512);
            _S212 = _S174;
            lambda_v_12 = _S515.differential_0;
            _S513 = _S514.differential_0;
        }
        else
        {
            _S212 = _S512;
            lambda_v_12 = _S174;
            _S513 = _S174;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S516;
        (&_S516)->primal_0 = _S225;
        (&_S516)->differential_0 = _S174;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S517;
        (&_S517)->primal_0 = _S225;
        (&_S517)->differential_0 = _S174;
        s_bwd_prop_dot_0(&_S516, &_S517, 0.0f);
        float3  _S518 = _S517.differential_0 + _S516.differential_0 + _S212;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S519;
        (&_S519)->primal_0 = r0_4;
        (&_S519)->differential_0 = _S174;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S520;
        (&_S520)->primal_0 = r2_16;
        (&_S520)->differential_0 = _S174;
        s_bwd_prop_cross_0(&_S519, &_S520, _S518);
        float3  _S521 = _S520.differential_0 + lambda_v_12;
        _S212 = _S174;
        lambda_v_12 = _S521;
        _S225 = _S513;
        _S513 = _S519.differential_0;
    }
    else
    {
        _S212 = _S512;
        lambda_v_12 = _S174;
        _S225 = _S174;
        _S513 = _S174;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S522;
    (&_S522)->primal_0 = _S223;
    (&_S522)->differential_0 = _S174;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S523;
    (&_S523)->primal_0 = _S223;
    (&_S523)->differential_0 = _S174;
    s_bwd_prop_dot_0(&_S522, &_S523, 0.0f);
    float3  _S524 = _S523.differential_0 + _S522.differential_0 + _S212;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S525;
    (&_S525)->primal_0 = r0_4;
    (&_S525)->differential_0 = _S174;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S526;
    (&_S526)->primal_0 = r1_4;
    (&_S526)->differential_0 = _S174;
    s_bwd_prop_cross_0(&_S525, &_S526, _S524);
    float3  _S527 = _S174;
    *&((&_S527)->z) = lambda_v_12.z;
    *&((&_S527)->y) = lambda_v_12.y;
    *&((&_S527)->x) = lambda_v_12.x;
    float3  _S528 = _S526.differential_0 + _S225;
    float3  _S529 = _S174;
    *&((&_S529)->z) = _S528.z;
    *&((&_S529)->y) = _S528.y;
    *&((&_S529)->x) = _S528.x;
    float3  _S530 = _S525.differential_0 + _S513;
    float3  _S531 = _S174;
    *&((&_S531)->z) = _S530.z;
    *&((&_S531)->y) = _S530.y;
    *&((&_S531)->x) = _S530.x;
    Matrix<float, 3, 3>  _S532 = _S175;
    _S532[int(2)] = _S527;
    _S532[int(1)] = _S529;
    _S532[int(0)] = _S531;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S533;
    (&_S533)->primal_0 = skew_0;
    (&_S533)->differential_0 = _S175;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S534;
    (&_S534)->primal_0 = T_4;
    (&_S534)->differential_0 = _S175;
    s_bwd_prop_mul_1(&_S533, &_S534, _S532);
    Matrix<float, 3, 3>  _S535 = _S534.differential_0 + _S511.differential_0;
    float2  _S536 = make_float2 (_S533.differential_0.rows[int(2)].y + - _S533.differential_0.rows[int(1)].z, _S533.differential_0.rows[int(0)].z + - _S533.differential_0.rows[int(2)].x);
    Matrix<float, 2, 2>  _S537 = makeMatrix<float, 2, 2> (0.0f);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S538;
    (&_S538)->primal_0 = makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f);
    (&_S538)->differential_0 = _S537;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S539;
    (&_S539)->primal_0 = _S215.color_params_3.n_0;
    (&_S539)->differential_0 = _S178;
    s_bwd_prop_mul_2(&_S538, &_S539, _S536);
    float2  _S540 = make_float2 (_S535.rows[int(0)].z, _S535.rows[int(1)].z);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S541;
    (&_S541)->primal_0 = makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f);
    (&_S541)->differential_0 = _S537;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S542;
    (&_S542)->primal_0 = _S215.color_params_3.g_0;
    (&_S542)->differential_0 = _S178;
    s_bwd_prop_mul_2(&_S541, &_S542, _S540);
    float2  _S543 = make_float2 (_S535.rows[int(0)].y, _S535.rows[int(1)].y);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S544;
    (&_S544)->primal_0 = makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f);
    (&_S544)->differential_0 = _S537;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S545;
    (&_S545)->primal_0 = _S215.color_params_3.r_0;
    (&_S545)->differential_0 = _S178;
    s_bwd_prop_mul_2(&_S544, &_S545, _S543);
    float2  _S546 = make_float2 (_S535.rows[int(0)].x, _S535.rows[int(1)].x);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S547;
    (&_S547)->primal_0 = makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f);
    (&_S547)->differential_0 = _S537;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S548;
    (&_S548)->primal_0 = _S215.color_params_3.b_0;
    (&_S548)->differential_0 = _S178;
    s_bwd_prop_mul_2(&_S547, &_S548, _S546);
    ColorPPISPParams_0 _S549 = ColorPPISPParams_x24_syn_dzero_0();
    (&_S549)->n_0 = _S539.differential_0;
    (&_S549)->g_0 = _S542.differential_0;
    (&_S549)->r_0 = _S545.differential_0;
    (&_S549)->b_0 = _S548.differential_0;
    _S212 = _S499;
    *&((&_S212)->z) = 0.0f;
    float _S550 = rgb_out_5.z * _S496;
    float _S551 = _S214 * _S496;
    DiffPair_float_0 _S552;
    (&_S552)->primal_0 = falloff_2;
    (&_S552)->differential_0 = 0.0f;
    DiffPair_float_0 _S553;
    (&_S553)->primal_0 = 0.0f;
    (&_S553)->differential_0 = 0.0f;
    DiffPair_float_0 _S554;
    (&_S554)->primal_0 = 1.0f;
    (&_S554)->differential_0 = 0.0f;
    s_bwd_prop_clamp_1(&_S552, &_S553, &_S554, _S550);
    float _S555 = r2_15 * _S552.differential_0;
    float _S556 = r4_11 * _S552.differential_0;
    float s_diff_r6_T_0 = _S191 * _S552.differential_0;
    float _S557 = r6_2 * _S552.differential_0;
    float _S558 = r2_15 * (_S190 * _S552.differential_0 + r2_15 * s_diff_r6_T_0);
    float _S559 = _S189 * _S552.differential_0 + r4_11 * s_diff_r6_T_0 + _S558 + _S558;
    float _S560 = dy_11 * _S559;
    float _S561 = dx_11 * _S559;
    float _S562 = - (_S560 + _S560);
    float _S563 = - (_S561 + _S561);
    *&((&_S212)->y) = 0.0f;
    float _S564 = rgb_out_5.y * _S497;
    float _S565 = _S213 * _S497;
    DiffPair_float_0 _S566;
    (&_S566)->primal_0 = falloff_1;
    (&_S566)->differential_0 = 0.0f;
    DiffPair_float_0 _S567;
    (&_S567)->primal_0 = 0.0f;
    (&_S567)->differential_0 = 0.0f;
    DiffPair_float_0 _S568;
    (&_S568)->primal_0 = 1.0f;
    (&_S568)->differential_0 = 0.0f;
    s_bwd_prop_clamp_1(&_S566, &_S567, &_S568, _S564);
    float _S569 = r2_14 * _S566.differential_0;
    float _S570 = r4_10 * _S566.differential_0;
    float s_diff_r6_T_1 = _S188 * _S566.differential_0;
    float _S571 = r6_1 * _S566.differential_0;
    float _S572 = r2_14 * (_S187 * _S566.differential_0 + r2_14 * s_diff_r6_T_1);
    float _S573 = _S186 * _S566.differential_0 + r4_10 * s_diff_r6_T_1 + _S572 + _S572;
    float _S574 = dy_10 * _S573;
    float _S575 = dx_10 * _S573;
    float _S576 = - (_S574 + _S574);
    float _S577 = - (_S575 + _S575);
    *&((&_S212)->x) = 0.0f;
    float _S578 = rgb_out_5.x * _S498;
    float _S579 = _S210 * _S498;
    DiffPair_float_0 _S580;
    (&_S580)->primal_0 = falloff_0;
    (&_S580)->differential_0 = 0.0f;
    DiffPair_float_0 _S581;
    (&_S581)->primal_0 = 0.0f;
    (&_S581)->differential_0 = 0.0f;
    DiffPair_float_0 _S582;
    (&_S582)->primal_0 = 1.0f;
    (&_S582)->differential_0 = 0.0f;
    s_bwd_prop_clamp_1(&_S580, &_S581, &_S582, _S578);
    float _S583 = r2_13 * _S580.differential_0;
    float _S584 = r4_9 * _S580.differential_0;
    float s_diff_r6_T_2 = _S185 * _S580.differential_0;
    float _S585 = r6_0 * _S580.differential_0;
    float _S586 = r2_13 * (_S184 * _S580.differential_0 + r2_13 * s_diff_r6_T_2);
    float _S587 = _S183 * _S580.differential_0 + r4_9 * s_diff_r6_T_2 + _S586 + _S586;
    float _S588 = dy_9 * _S587;
    float _S589 = dx_9 * _S587;
    float _S590 = - (_S588 + _S588);
    float _S591 = - (_S589 + _S589);
    float3  _S592 = _S174;
    *&((&_S592)->z) = _S551;
    *&((&_S592)->y) = _S565;
    *&((&_S592)->x) = _S579;
    float3  _S593 = _S212 + _S592;
    float3  _S594 = _S173.primal_0 * _S593;
    float3  _S595 = _S206 * _S593;
    float _S596 = _S594.x + _S594.y + _S594.z;
    DiffPair_float_0 _S597;
    (&_S597)->primal_0 = _S204.exposure_3;
    (&_S597)->differential_0 = 0.0f;
    s_bwd_prop_exp2_0(&_S597, _S596);
    PPISPParams_0 _S598 = PPISPParams_x24_syn_dzero_0();
    (&_S598)->color_params_3 = _S549;
    (&_S598)->exposure_3 = _S597.differential_0;
    _S182 = _S598;
    (&(&_S182)->crf_params_1[int(2)])->center_0 = 0.0f;
    float _S599 = _S598.crf_params_1[int(2)].center_0 + _S376;
    (&(&_S182)->crf_params_1[int(2)])->gamma_0 = 0.0f;
    float _S600 = _S598.crf_params_1[int(2)].gamma_0 + _S379.differential_0;
    (&(&_S182)->crf_params_1[int(2)])->shoulder_0 = 0.0f;
    float _S601 = _S598.crf_params_1[int(2)].shoulder_0 + _S383.differential_0;
    (&(&_S182)->crf_params_1[int(2)])->toe_0 = 0.0f;
    float _S602 = _S598.crf_params_1[int(2)].toe_0 + _S387.differential_0;
    (&(&_S182)->crf_params_1[int(1)])->center_0 = 0.0f;
    float _S603 = _S598.crf_params_1[int(1)].center_0 + _S420;
    (&(&_S182)->crf_params_1[int(1)])->gamma_0 = 0.0f;
    float _S604 = _S598.crf_params_1[int(1)].gamma_0 + _S423.differential_0;
    (&(&_S182)->crf_params_1[int(1)])->shoulder_0 = 0.0f;
    float _S605 = _S598.crf_params_1[int(1)].shoulder_0 + _S427.differential_0;
    (&(&_S182)->crf_params_1[int(1)])->toe_0 = 0.0f;
    float _S606 = _S598.crf_params_1[int(1)].toe_0 + _S431.differential_0;
    (&(&_S182)->crf_params_1[int(0)])->center_0 = 0.0f;
    float _S607 = _S598.crf_params_1[int(0)].center_0 + _S464;
    (&(&_S182)->crf_params_1[int(0)])->gamma_0 = 0.0f;
    float _S608 = _S598.crf_params_1[int(0)].gamma_0 + _S467.differential_0;
    (&(&_S182)->crf_params_1[int(0)])->shoulder_0 = 0.0f;
    float _S609 = _S598.crf_params_1[int(0)].shoulder_0 + _S471.differential_0;
    (&(&_S182)->crf_params_1[int(0)])->toe_0 = 0.0f;
    float _S610 = _S598.crf_params_1[int(0)].toe_0 + _S475.differential_0;
    *&((&(&(&_S182)->color_params_3)->n_0)->y) = 0.0f;
    *&((&(&(&_S182)->color_params_3)->n_0)->x) = 0.0f;
    *&((&(&(&_S182)->color_params_3)->g_0)->y) = 0.0f;
    *&((&(&(&_S182)->color_params_3)->g_0)->x) = 0.0f;
    *&((&(&(&_S182)->color_params_3)->r_0)->y) = 0.0f;
    *&((&(&(&_S182)->color_params_3)->r_0)->x) = 0.0f;
    *&((&(&(&_S182)->color_params_3)->b_0)->y) = 0.0f;
    *&((&(&(&_S182)->color_params_3)->b_0)->x) = 0.0f;
    (&(&_S182)->vignette_params_2[int(2)])->alpha2_0 = 0.0f;
    float _S611 = _S557 + _S598.vignette_params_2[int(2)].alpha2_0;
    (&(&_S182)->vignette_params_2[int(2)])->alpha1_0 = 0.0f;
    float _S612 = _S556 + _S598.vignette_params_2[int(2)].alpha1_0;
    (&(&_S182)->vignette_params_2[int(2)])->alpha0_0 = 0.0f;
    float _S613 = _S555 + _S598.vignette_params_2[int(2)].alpha0_0;
    (&(&_S182)->vignette_params_2[int(2)])->cy_0 = 0.0f;
    float _S614 = _S562 + _S598.vignette_params_2[int(2)].cy_0;
    (&(&_S182)->vignette_params_2[int(2)])->cx_0 = 0.0f;
    float _S615 = _S563 + _S598.vignette_params_2[int(2)].cx_0;
    (&(&_S182)->vignette_params_2[int(1)])->alpha2_0 = 0.0f;
    float _S616 = _S571 + _S598.vignette_params_2[int(1)].alpha2_0;
    (&(&_S182)->vignette_params_2[int(1)])->alpha1_0 = 0.0f;
    float _S617 = _S570 + _S598.vignette_params_2[int(1)].alpha1_0;
    (&(&_S182)->vignette_params_2[int(1)])->alpha0_0 = 0.0f;
    float _S618 = _S569 + _S598.vignette_params_2[int(1)].alpha0_0;
    (&(&_S182)->vignette_params_2[int(1)])->cy_0 = 0.0f;
    float _S619 = _S576 + _S598.vignette_params_2[int(1)].cy_0;
    (&(&_S182)->vignette_params_2[int(1)])->cx_0 = 0.0f;
    float _S620 = _S577 + _S598.vignette_params_2[int(1)].cx_0;
    (&(&_S182)->vignette_params_2[int(0)])->alpha2_0 = 0.0f;
    float _S621 = _S585 + _S598.vignette_params_2[int(0)].alpha2_0;
    (&(&_S182)->vignette_params_2[int(0)])->alpha1_0 = 0.0f;
    float _S622 = _S584 + _S598.vignette_params_2[int(0)].alpha1_0;
    (&(&_S182)->vignette_params_2[int(0)])->alpha0_0 = 0.0f;
    float _S623 = _S583 + _S598.vignette_params_2[int(0)].alpha0_0;
    (&(&_S182)->vignette_params_2[int(0)])->cy_0 = 0.0f;
    float _S624 = _S590 + _S598.vignette_params_2[int(0)].cy_0;
    (&(&_S182)->vignette_params_2[int(0)])->cx_0 = 0.0f;
    float _S625 = _S591 + _S598.vignette_params_2[int(0)].cx_0;
    FixedArray<float, 36>  _S626;
    _S626[int(0)] = 0.0f;
    _S626[int(1)] = 0.0f;
    _S626[int(2)] = 0.0f;
    _S626[int(3)] = 0.0f;
    _S626[int(4)] = 0.0f;
    _S626[int(5)] = 0.0f;
    _S626[int(6)] = 0.0f;
    _S626[int(7)] = 0.0f;
    _S626[int(8)] = 0.0f;
    _S626[int(9)] = 0.0f;
    _S626[int(10)] = 0.0f;
    _S626[int(11)] = 0.0f;
    _S626[int(12)] = 0.0f;
    _S626[int(13)] = 0.0f;
    _S626[int(14)] = 0.0f;
    _S626[int(15)] = 0.0f;
    _S626[int(16)] = 0.0f;
    _S626[int(17)] = 0.0f;
    _S626[int(18)] = 0.0f;
    _S626[int(19)] = 0.0f;
    _S626[int(20)] = 0.0f;
    _S626[int(21)] = 0.0f;
    _S626[int(22)] = 0.0f;
    _S626[int(23)] = 0.0f;
    _S626[int(24)] = 0.0f;
    _S626[int(25)] = 0.0f;
    _S626[int(26)] = 0.0f;
    _S626[int(27)] = 0.0f;
    _S626[int(28)] = 0.0f;
    _S626[int(29)] = 0.0f;
    _S626[int(30)] = 0.0f;
    _S626[int(31)] = 0.0f;
    _S626[int(32)] = 0.0f;
    _S626[int(33)] = 0.0f;
    _S626[int(34)] = 0.0f;
    _S626[int(35)] = 0.0f;
    _S626[int(8)] = _S618;
    _S626[int(16)] = _S598.color_params_3.b_0.x;
    _S626[int(15)] = _S611;
    _S626[int(14)] = _S612;
    _S626[int(13)] = _S613;
    _S626[int(12)] = _S614;
    _S626[int(11)] = _S615;
    _S626[int(10)] = _S616;
    _S626[int(9)] = _S617;
    _S626[int(17)] = _S598.color_params_3.b_0.y;
    _S626[int(7)] = _S619;
    _S626[int(6)] = _S620;
    _S626[int(5)] = _S621;
    _S626[int(4)] = _S622;
    _S626[int(3)] = _S623;
    _S626[int(2)] = _S624;
    _S626[int(1)] = _S625;
    _S626[int(0)] = _S182.exposure_3;
    _S626[int(26)] = _S608;
    _S626[int(34)] = _S600;
    _S626[int(33)] = _S601;
    _S626[int(32)] = _S602;
    _S626[int(31)] = _S603;
    _S626[int(30)] = _S604;
    _S626[int(29)] = _S605;
    _S626[int(28)] = _S606;
    _S626[int(27)] = _S607;
    _S626[int(35)] = _S599;
    _S626[int(25)] = _S609;
    _S626[int(24)] = _S610;
    _S626[int(23)] = _S598.color_params_3.n_0.y;
    _S626[int(22)] = _S598.color_params_3.n_0.x;
    _S626[int(21)] = _S598.color_params_3.g_0.y;
    _S626[int(20)] = _S598.color_params_3.g_0.x;
    _S626[int(19)] = _S598.color_params_3.r_0.y;
    _S626[int(18)] = _S598.color_params_3.r_0.x;
    dpparams_0->primal_0 = dpparams_0->primal_0;
    dpparams_0->differential_0 = _S626;
    dprgb_in_0->primal_0 = (*dprgb_in_0).primal_0;
    dprgb_in_0->differential_0 = _S595;
    return;
}

inline __device__ void s_bwd_apply_ppisp_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S627, float2  _S628, float2  _S629, float2  _S630, DiffPair_arrayx3Cfloatx2C36x3E_0 * _S631, float3  _S632)
{
    s_bwd_prop_apply_ppisp_0(_S627, _S628, _S629, _S630, _S631, _S632);
    return;
}

inline __device__ void apply_ppisp_vjp(float3  rgb_in_4, float2  pix_coord_5, float2  image_center_5, float2  img_size_5, FixedArray<float, 36>  params_4, float3  grad_out_0, float3  * grad_rgb_in_0, FixedArray<float, 36>  * grad_params_0)
{
    float3  _S633 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_rgb_in_0;
    (&dp_rgb_in_0)->primal_0 = rgb_in_4;
    (&dp_rgb_in_0)->differential_0 = _S633;
    FixedArray<float, 36>  _S634 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    DiffPair_arrayx3Cfloatx2C36x3E_0 dp_params_0;
    (&dp_params_0)->primal_0 = params_4;
    (&dp_params_0)->differential_0 = _S634;
    s_bwd_apply_ppisp_0(&dp_rgb_in_0, pix_coord_5, image_center_5, img_size_5, &dp_params_0, grad_out_0);
    *grad_rgb_in_0 = dp_rgb_in_0.differential_0;
    *grad_params_0 = (&dp_params_0)->differential_0;
    return;
}

struct DiffPair_arrayx3Cfloatx2C39x3E_0
{
    FixedArray<float, 39>  primal_0;
    FixedArray<float, 39>  differential_0;
};

inline __device__ void s_bwd_prop_apply_ppisp_rqs_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dprgb_in_1, float2  pix_coord_6, float2  image_center_6, float2  img_size_6, DiffPair_arrayx3Cfloatx2C39x3E_0 * dpparams_1, float3  _s_dOut_1)
{
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S635 = *dprgb_in_1;
    float3  _S636 = make_float3 (0.0f);
    Matrix<float, 3, 3>  _S637 = makeMatrix<float, 3, 3> (0.0f);
    VignettingChannelParams_0 _S638 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    FixedArray<VignettingChannelParams_0, 3>  _S639 = {
        _S638, _S638, _S638
    };
    float2  _S640 = make_float2 (0.0f);
    ColorPPISPParams_0 _S641 = { _S640, _S640, _S640, _S640 };
    RQSCRFPPISPChannelParams_0 _S642 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    FixedArray<RQSCRFPPISPChannelParams_0, 3>  _S643 = {
        _S642, _S642, _S642
    };
    PPISPParamsRQS_0 _S644;
    (&_S644)->exposure_2 = dpparams_1->primal_0[int(0)];
    (&_S644)->vignette_params_1 = _S639;
    (&_S644)->color_params_2 = _S641;
    (&_S644)->crf_params_0 = _S643;
    (&(&_S644)->vignette_params_1[int(0)])->cx_0 = dpparams_1->primal_0[int(1)];
    (&(&_S644)->vignette_params_1[int(0)])->cy_0 = dpparams_1->primal_0[int(2)];
    float _S645 = dpparams_1->primal_0[int(3)];
    (&(&_S644)->vignette_params_1[int(0)])->alpha0_0 = dpparams_1->primal_0[int(3)];
    float _S646 = dpparams_1->primal_0[int(4)];
    (&(&_S644)->vignette_params_1[int(0)])->alpha1_0 = dpparams_1->primal_0[int(4)];
    float _S647 = dpparams_1->primal_0[int(5)];
    (&(&_S644)->vignette_params_1[int(0)])->alpha2_0 = dpparams_1->primal_0[int(5)];
    (&(&_S644)->vignette_params_1[int(1)])->cx_0 = dpparams_1->primal_0[int(6)];
    (&(&_S644)->vignette_params_1[int(1)])->cy_0 = dpparams_1->primal_0[int(7)];
    float _S648 = dpparams_1->primal_0[int(8)];
    (&(&_S644)->vignette_params_1[int(1)])->alpha0_0 = dpparams_1->primal_0[int(8)];
    float _S649 = dpparams_1->primal_0[int(9)];
    (&(&_S644)->vignette_params_1[int(1)])->alpha1_0 = dpparams_1->primal_0[int(9)];
    float _S650 = dpparams_1->primal_0[int(10)];
    (&(&_S644)->vignette_params_1[int(1)])->alpha2_0 = dpparams_1->primal_0[int(10)];
    (&(&_S644)->vignette_params_1[int(2)])->cx_0 = dpparams_1->primal_0[int(11)];
    (&(&_S644)->vignette_params_1[int(2)])->cy_0 = dpparams_1->primal_0[int(12)];
    float _S651 = dpparams_1->primal_0[int(13)];
    (&(&_S644)->vignette_params_1[int(2)])->alpha0_0 = dpparams_1->primal_0[int(13)];
    float _S652 = dpparams_1->primal_0[int(14)];
    (&(&_S644)->vignette_params_1[int(2)])->alpha1_0 = dpparams_1->primal_0[int(14)];
    float _S653 = dpparams_1->primal_0[int(15)];
    (&(&_S644)->vignette_params_1[int(2)])->alpha2_0 = dpparams_1->primal_0[int(15)];
    *&((&(&(&_S644)->color_params_2)->b_0)->x) = dpparams_1->primal_0[int(16)];
    *&((&(&(&_S644)->color_params_2)->b_0)->y) = dpparams_1->primal_0[int(17)];
    *&((&(&(&_S644)->color_params_2)->r_0)->x) = dpparams_1->primal_0[int(18)];
    *&((&(&(&_S644)->color_params_2)->r_0)->y) = dpparams_1->primal_0[int(19)];
    *&((&(&(&_S644)->color_params_2)->g_0)->x) = dpparams_1->primal_0[int(20)];
    *&((&(&(&_S644)->color_params_2)->g_0)->y) = dpparams_1->primal_0[int(21)];
    *&((&(&(&_S644)->color_params_2)->n_0)->x) = dpparams_1->primal_0[int(22)];
    *&((&(&(&_S644)->color_params_2)->n_0)->y) = dpparams_1->primal_0[int(23)];
    float _S654 = dpparams_1->primal_0[int(24)];
    (&(&_S644)->crf_params_0[int(0)])->g0_0 = dpparams_1->primal_0[int(24)];
    float _S655 = dpparams_1->primal_0[int(25)];
    (&(&_S644)->crf_params_0[int(0)])->g1_0 = dpparams_1->primal_0[int(25)];
    float _S656 = dpparams_1->primal_0[int(26)];
    (&(&_S644)->crf_params_0[int(0)])->x0_0 = dpparams_1->primal_0[int(26)];
    float _S657 = dpparams_1->primal_0[int(27)];
    (&(&_S644)->crf_params_0[int(0)])->y0_0 = dpparams_1->primal_0[int(27)];
    float _S658 = dpparams_1->primal_0[int(28)];
    (&(&_S644)->crf_params_0[int(0)])->gc_0 = dpparams_1->primal_0[int(28)];
    float _S659 = dpparams_1->primal_0[int(29)];
    (&(&_S644)->crf_params_0[int(1)])->g0_0 = dpparams_1->primal_0[int(29)];
    float _S660 = dpparams_1->primal_0[int(30)];
    (&(&_S644)->crf_params_0[int(1)])->g1_0 = dpparams_1->primal_0[int(30)];
    float _S661 = dpparams_1->primal_0[int(31)];
    (&(&_S644)->crf_params_0[int(1)])->x0_0 = dpparams_1->primal_0[int(31)];
    float _S662 = dpparams_1->primal_0[int(32)];
    (&(&_S644)->crf_params_0[int(1)])->y0_0 = dpparams_1->primal_0[int(32)];
    float _S663 = dpparams_1->primal_0[int(33)];
    (&(&_S644)->crf_params_0[int(1)])->gc_0 = dpparams_1->primal_0[int(33)];
    float _S664 = dpparams_1->primal_0[int(34)];
    (&(&_S644)->crf_params_0[int(2)])->g0_0 = dpparams_1->primal_0[int(34)];
    float _S665 = dpparams_1->primal_0[int(35)];
    (&(&_S644)->crf_params_0[int(2)])->g1_0 = dpparams_1->primal_0[int(35)];
    float _S666 = dpparams_1->primal_0[int(36)];
    (&(&_S644)->crf_params_0[int(2)])->x0_0 = dpparams_1->primal_0[int(36)];
    float _S667 = dpparams_1->primal_0[int(37)];
    (&(&_S644)->crf_params_0[int(2)])->y0_0 = dpparams_1->primal_0[int(37)];
    float _S668 = dpparams_1->primal_0[int(38)];
    (&(&_S644)->crf_params_0[int(2)])->gc_0 = dpparams_1->primal_0[int(38)];
    PPISPParamsRQS_0 _S669 = _S644;
    float _S670 = s_primal_ctx_exp2_0(_S644.exposure_2);
    float3  _S671 = make_float3 (_S670);
    float3  rgb_out_6 = (*dprgb_in_1).primal_0 * make_float3 (_S670);
    float _S672 = (F32_max((img_size_6.x), (img_size_6.y)));
    float _S673 = (pix_coord_6.x - image_center_6.x) / _S672;
    float _S674 = (pix_coord_6.y - image_center_6.y) / _S672;
    float dx_12 = _S673 - dpparams_1->primal_0[int(1)];
    float dy_12 = _S674 - dpparams_1->primal_0[int(2)];
    float r2_17 = dx_12 * dx_12 + dy_12 * dy_12;
    float r4_12 = r2_17 * r2_17;
    float r6_3 = r4_12 * r2_17;
    float falloff_3 = dpparams_1->primal_0[int(5)] * r6_3 + dpparams_1->primal_0[int(4)] * r4_12 + dpparams_1->primal_0[int(3)] * r2_17 + 1.0f;
    float _S675 = s_primal_ctx_clamp_0(falloff_3, 0.0f, 1.0f);
    float _S676 = rgb_out_6.x * _S675;
    float3  _S677 = rgb_out_6;
    *&((&_S677)->x) = _S676;
    float dx_13 = _S673 - dpparams_1->primal_0[int(6)];
    float dy_13 = _S674 - dpparams_1->primal_0[int(7)];
    float r2_18 = dx_13 * dx_13 + dy_13 * dy_13;
    float r4_13 = r2_18 * r2_18;
    float r6_4 = r4_13 * r2_18;
    float falloff_4 = dpparams_1->primal_0[int(10)] * r6_4 + dpparams_1->primal_0[int(9)] * r4_13 + dpparams_1->primal_0[int(8)] * r2_18 + 1.0f;
    float _S678 = s_primal_ctx_clamp_0(falloff_4, 0.0f, 1.0f);
    *&((&_S677)->y) = rgb_out_6.y * _S678;
    float dx_14 = _S673 - dpparams_1->primal_0[int(11)];
    float dy_14 = _S674 - dpparams_1->primal_0[int(12)];
    float r2_19 = dx_14 * dx_14 + dy_14 * dy_14;
    float r4_14 = r2_19 * r2_19;
    float r6_5 = r4_14 * r2_19;
    float falloff_5 = dpparams_1->primal_0[int(15)] * r6_5 + dpparams_1->primal_0[int(14)] * r4_14 + dpparams_1->primal_0[int(13)] * r2_19 + 1.0f;
    float _S679 = s_primal_ctx_clamp_0(falloff_5, 0.0f, 1.0f);
    *&((&_S677)->z) = rgb_out_6.z * _S679;
    PPISPParamsRQS_0 _S680 = _S644;
    float2  _S681 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f), _S644.color_params_2.b_0);
    float2  _S682 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f), _S644.color_params_2.r_0);
    float2  _S683 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f), _S644.color_params_2.g_0);
    float2  _S684 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f), _S644.color_params_2.n_0);
    float _S685 = 0.3333333432674408f + _S684.x;
    float _S686 = 0.3333333432674408f + _S684.y;
    Matrix<float, 3, 3>  T_5 = makeMatrix<float, 3, 3> (_S681.x, 1.0f + _S682.x, _S683.x, _S681.y, _S682.y, 1.0f + _S683.y, 1.0f, 1.0f, 1.0f);
    Matrix<float, 3, 3>  skew_1 = makeMatrix<float, 3, 3> (0.0f, -1.0f, _S686, 1.0f, 0.0f, - _S685, - _S686, _S685, 0.0f);
    Matrix<float, 3, 3>  _S687 = s_primal_ctx_mul_1(skew_1, T_5);
    float3  r0_5 = make_float3 (_S687.rows[int(0)].x, _S687.rows[int(0)].y, _S687.rows[int(0)].z);
    float3  r1_5 = make_float3 (_S687.rows[int(1)].x, _S687.rows[int(1)].y, _S687.rows[int(1)].z);
    float3  r2_20 = make_float3 (_S687.rows[int(2)].x, _S687.rows[int(2)].y, _S687.rows[int(2)].z);
    float3  _S688 = s_primal_ctx_cross_0(r0_5, r1_5);
    bool _S689 = (s_primal_ctx_dot_0(_S688, _S688)) < 9.99999968265522539e-21f;
    float3  lambda_v_13;
    float3  _S690;
    bool _S691;
    if(_S689)
    {
        float3  _S692 = s_primal_ctx_cross_0(r0_5, r2_20);
        bool _S693 = (s_primal_ctx_dot_0(_S692, _S692)) < 9.99999968265522539e-21f;
        if(_S693)
        {
            lambda_v_13 = s_primal_ctx_cross_0(r1_5, r2_20);
        }
        else
        {
            lambda_v_13 = _S692;
        }
        _S691 = _S693;
        _S690 = _S692;
    }
    else
    {
        lambda_v_13 = _S688;
        _S691 = false;
        _S690 = _S636;
    }
    Matrix<float, 3, 3>  S_inv_1 = makeMatrix<float, 3, 3> (-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    Matrix<float, 3, 3>  D_1 = makeMatrix<float, 3, 3> (lambda_v_13.x, 0.0f, 0.0f, 0.0f, lambda_v_13.y, 0.0f, 0.0f, 0.0f, lambda_v_13.z);
    Matrix<float, 3, 3>  _S694 = s_primal_ctx_mul_1(T_5, D_1);
    Matrix<float, 3, 3>  _S695 = s_primal_ctx_mul_1(_S694, S_inv_1);
    bool _S696 = (s_primal_ctx_abs_0(_S695.rows[int(2)].z)) > 9.99999968265522539e-21f;
    Matrix<float, 3, 3>  H_9;
    Matrix<float, 3, 3>  _S697;
    float _S698;
    if(_S696)
    {
        float inv_s_1 = 1.0f / _S695.rows[int(2)].z;
        Matrix<float, 3, 3>  _S699 = makeMatrix<float, 3, 3> (inv_s_1);
        float _S700 = _S695.rows[int(2)].z * _S695.rows[int(2)].z;
        H_9 = _S695 * makeMatrix<float, 3, 3> (inv_s_1);
        _S697 = _S699;
        _S698 = _S700;
    }
    else
    {
        H_9 = _S695;
        _S697 = _S637;
        _S698 = 0.0f;
    }
    float _S701 = _S677.x;
    float _S702 = _S677.y;
    float intensity_5 = _S701 + _S702 + _S677.z;
    float3  rgi_in_1 = make_float3 (_S701, _S702, intensity_5);
    float3  _S703 = s_primal_ctx_mul_2(H_9, rgi_in_1);
    float _S704 = _S703.z;
    float _S705 = 0.00009999999747379f * s_primal_ctx_abs_0(intensity_5) + 9.99999993922529029e-09f;
    float _S706 = (F32_max((_S704), (_S705)));
    float norm_factor_5 = intensity_5 / _S706;
    float _S707 = _S706 * _S706;
    float _S708 = _S703.x;
    float out_r_5 = _S708 * norm_factor_5;
    float _S709 = _S703.y;
    float out_g_5 = _S709 * norm_factor_5;
    float3  _S710 = make_float3 (out_r_5, out_g_5, intensity_5 - out_r_5 - out_g_5);
    float3  _S711 = make_float3 (0.0f);
    float3  _S712 = make_float3 (1.0f);
    float3  _S713 = s_primal_ctx_clamp_1(_S710, _S711, _S712);
    float _S714 = _S713.x;
    float _S715 = s_primal_ctx_exp_0(_S654);
    float _S716 = s_primal_ctx_exp_0(_S655);
    float _S717 = - _S656;
    float _S718 = 1.0f + s_primal_ctx_exp_0(_S717);
    float x0_4 = 1.0f / _S718;
    float _S719 = _S718 * _S718;
    float _S720 = - _S657;
    float _S721 = 1.0f + s_primal_ctx_exp_0(_S720);
    float y0_4 = 1.0f / _S721;
    float _S722 = _S721 * _S721;
    float _S723 = s_primal_ctx_exp_0(_S658);
    bool _S724 = _S714 < x0_4;
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
    float _S745;
    float _S746;
    float _S747;
    float _S748;
    float _S749;
    float _S750;
    float _S751;
    if(_S724)
    {
        float s0_3 = y0_4 / x0_4;
        float _S752 = x0_4 * x0_4;
        float t0_3 = _S714 / x0_4;
        float _S753 = s0_3 * t0_3;
        float _S754 = _S715 * t0_3;
        float _S755 = 1.0f - t0_3;
        float _S756 = _S753 * t0_3 + _S754 * _S755;
        float _S757 = y0_4 * _S756;
        float _S758 = _S715 + _S723 - 2.0f * s0_3;
        float _S759 = _S758 * t0_3;
        float _S760 = s0_3 + _S759 * _S755;
        _S725 = _S760 * _S760;
        _S726 = _S757;
        _S727 = _S760;
        _S728 = _S759;
        _S729 = _S755;
        _S730 = _S758;
        _S731 = t0_3;
        _S732 = _S756;
        _S733 = _S754;
        _S734 = _S753;
        _S735 = s0_3;
        _S736 = _S752;
        _S737 = 0.0f;
        _S738 = 0.0f;
        _S739 = 0.0f;
        _S740 = 0.0f;
        _S741 = 0.0f;
        _S742 = 0.0f;
        _S743 = 0.0f;
        _S744 = 0.0f;
        _S745 = 0.0f;
        _S746 = 0.0f;
        _S747 = 0.0f;
        _S748 = 0.0f;
        _S749 = 0.0f;
        _S750 = 0.0f;
        _S751 = 0.0f;
    }
    else
    {
        float _S761 = 1.0f - y0_4;
        float _S762 = 1.0f - x0_4;
        float s1_3 = _S761 / _S762;
        float _S763 = _S762 * _S762;
        float _S764 = _S714 - x0_4;
        float t1_3 = _S764 / _S762;
        float _S765 = s1_3 * t1_3;
        float _S766 = _S723 * t1_3;
        float _S767 = 1.0f - t1_3;
        float _S768 = _S765 * t1_3 + _S766 * _S767;
        float _S769 = _S761 * _S768;
        float _S770 = _S723 + _S716 - 2.0f * s1_3;
        float _S771 = _S770 * t1_3;
        float _S772 = s1_3 + _S771 * _S767;
        float _S773 = _S772 * _S772;
        _S725 = 0.0f;
        _S726 = 0.0f;
        _S727 = 0.0f;
        _S728 = 0.0f;
        _S729 = 0.0f;
        _S730 = 0.0f;
        _S731 = 0.0f;
        _S732 = 0.0f;
        _S733 = 0.0f;
        _S734 = 0.0f;
        _S735 = 0.0f;
        _S736 = 0.0f;
        _S737 = _S773;
        _S738 = _S769;
        _S739 = _S772;
        _S740 = _S771;
        _S741 = _S767;
        _S742 = _S770;
        _S743 = t1_3;
        _S744 = _S761;
        _S745 = _S768;
        _S746 = _S766;
        _S747 = _S765;
        _S748 = s1_3;
        _S749 = _S763;
        _S750 = _S764;
        _S751 = _S762;
    }
    float _S774 = _S713.y;
    float _S775 = s_primal_ctx_exp_0(_S659);
    float _S776 = s_primal_ctx_exp_0(_S660);
    float _S777 = - _S661;
    float _S778 = 1.0f + s_primal_ctx_exp_0(_S777);
    float x0_5 = 1.0f / _S778;
    float _S779 = _S778 * _S778;
    float _S780 = - _S662;
    float _S781 = 1.0f + s_primal_ctx_exp_0(_S780);
    float y0_5 = 1.0f / _S781;
    float _S782 = _S781 * _S781;
    float _S783 = s_primal_ctx_exp_0(_S663);
    bool _S784 = _S774 < x0_5;
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
    float _S805;
    float _S806;
    float _S807;
    float _S808;
    float _S809;
    float _S810;
    float _S811;
    if(_S784)
    {
        float s0_4 = y0_5 / x0_5;
        float _S812 = x0_5 * x0_5;
        float t0_4 = _S774 / x0_5;
        float _S813 = s0_4 * t0_4;
        float _S814 = _S775 * t0_4;
        float _S815 = 1.0f - t0_4;
        float _S816 = _S813 * t0_4 + _S814 * _S815;
        float _S817 = y0_5 * _S816;
        float _S818 = _S775 + _S783 - 2.0f * s0_4;
        float _S819 = _S818 * t0_4;
        float _S820 = s0_4 + _S819 * _S815;
        _S785 = _S820 * _S820;
        _S786 = _S817;
        _S787 = _S820;
        _S788 = _S819;
        _S789 = _S815;
        _S790 = _S818;
        _S791 = t0_4;
        _S792 = _S816;
        _S793 = _S814;
        _S794 = _S813;
        _S795 = s0_4;
        _S796 = _S812;
        _S797 = 0.0f;
        _S798 = 0.0f;
        _S799 = 0.0f;
        _S800 = 0.0f;
        _S801 = 0.0f;
        _S802 = 0.0f;
        _S803 = 0.0f;
        _S804 = 0.0f;
        _S805 = 0.0f;
        _S806 = 0.0f;
        _S807 = 0.0f;
        _S808 = 0.0f;
        _S809 = 0.0f;
        _S810 = 0.0f;
        _S811 = 0.0f;
    }
    else
    {
        float _S821 = 1.0f - y0_5;
        float _S822 = 1.0f - x0_5;
        float s1_4 = _S821 / _S822;
        float _S823 = _S822 * _S822;
        float _S824 = _S774 - x0_5;
        float t1_4 = _S824 / _S822;
        float _S825 = s1_4 * t1_4;
        float _S826 = _S783 * t1_4;
        float _S827 = 1.0f - t1_4;
        float _S828 = _S825 * t1_4 + _S826 * _S827;
        float _S829 = _S821 * _S828;
        float _S830 = _S783 + _S776 - 2.0f * s1_4;
        float _S831 = _S830 * t1_4;
        float _S832 = s1_4 + _S831 * _S827;
        float _S833 = _S832 * _S832;
        _S785 = 0.0f;
        _S786 = 0.0f;
        _S787 = 0.0f;
        _S788 = 0.0f;
        _S789 = 0.0f;
        _S790 = 0.0f;
        _S791 = 0.0f;
        _S792 = 0.0f;
        _S793 = 0.0f;
        _S794 = 0.0f;
        _S795 = 0.0f;
        _S796 = 0.0f;
        _S797 = _S833;
        _S798 = _S829;
        _S799 = _S832;
        _S800 = _S831;
        _S801 = _S827;
        _S802 = _S830;
        _S803 = t1_4;
        _S804 = _S821;
        _S805 = _S828;
        _S806 = _S826;
        _S807 = _S825;
        _S808 = s1_4;
        _S809 = _S823;
        _S810 = _S824;
        _S811 = _S822;
    }
    float _S834 = _S713.z;
    float _S835 = s_primal_ctx_exp_0(_S664);
    float _S836 = s_primal_ctx_exp_0(_S665);
    float _S837 = - _S666;
    float _S838 = 1.0f + s_primal_ctx_exp_0(_S837);
    float x0_6 = 1.0f / _S838;
    float _S839 = _S838 * _S838;
    float _S840 = - _S667;
    float _S841 = 1.0f + s_primal_ctx_exp_0(_S840);
    float y0_6 = 1.0f / _S841;
    float _S842 = _S841 * _S841;
    float _S843 = s_primal_ctx_exp_0(_S668);
    bool _S844 = _S834 < x0_6;
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
    float _S865;
    float _S866;
    float _S867;
    float _S868;
    float _S869;
    float _S870;
    float _S871;
    if(_S844)
    {
        float s0_5 = y0_6 / x0_6;
        float _S872 = x0_6 * x0_6;
        float t0_5 = _S834 / x0_6;
        float _S873 = s0_5 * t0_5;
        float _S874 = _S835 * t0_5;
        float _S875 = 1.0f - t0_5;
        float _S876 = _S873 * t0_5 + _S874 * _S875;
        float _S877 = y0_6 * _S876;
        float _S878 = _S835 + _S843 - 2.0f * s0_5;
        float _S879 = _S878 * t0_5;
        float _S880 = s0_5 + _S879 * _S875;
        _S845 = _S880 * _S880;
        _S846 = _S877;
        _S847 = _S880;
        _S848 = _S879;
        _S849 = _S875;
        _S850 = _S878;
        _S851 = t0_5;
        _S852 = _S876;
        _S853 = _S874;
        _S854 = _S873;
        _S855 = s0_5;
        _S856 = _S872;
        _S857 = 0.0f;
        _S858 = 0.0f;
        _S859 = 0.0f;
        _S860 = 0.0f;
        _S861 = 0.0f;
        _S862 = 0.0f;
        _S863 = 0.0f;
        _S864 = 0.0f;
        _S865 = 0.0f;
        _S866 = 0.0f;
        _S867 = 0.0f;
        _S868 = 0.0f;
        _S869 = 0.0f;
        _S870 = 0.0f;
        _S871 = 0.0f;
    }
    else
    {
        float _S881 = 1.0f - y0_6;
        float _S882 = 1.0f - x0_6;
        float s1_5 = _S881 / _S882;
        float _S883 = _S882 * _S882;
        float _S884 = _S834 - x0_6;
        float t1_5 = _S884 / _S882;
        float _S885 = s1_5 * t1_5;
        float _S886 = _S843 * t1_5;
        float _S887 = 1.0f - t1_5;
        float _S888 = _S885 * t1_5 + _S886 * _S887;
        float _S889 = _S881 * _S888;
        float _S890 = _S843 + _S836 - 2.0f * s1_5;
        float _S891 = _S890 * t1_5;
        float _S892 = s1_5 + _S891 * _S887;
        float _S893 = _S892 * _S892;
        _S845 = 0.0f;
        _S846 = 0.0f;
        _S847 = 0.0f;
        _S848 = 0.0f;
        _S849 = 0.0f;
        _S850 = 0.0f;
        _S851 = 0.0f;
        _S852 = 0.0f;
        _S853 = 0.0f;
        _S854 = 0.0f;
        _S855 = 0.0f;
        _S856 = 0.0f;
        _S857 = _S893;
        _S858 = _S889;
        _S859 = _S892;
        _S860 = _S891;
        _S861 = _S887;
        _S862 = _S890;
        _S863 = t1_5;
        _S864 = _S881;
        _S865 = _S888;
        _S866 = _S886;
        _S867 = _S885;
        _S868 = s1_5;
        _S869 = _S883;
        _S870 = _S884;
        _S871 = _S882;
    }
    if(_S844)
    {
        float _S894 = _s_dOut_1.z / _S845;
        float _S895 = _S846 * - _S894;
        float _S896 = _S847 * _S894;
        float _S897 = _S849 * _S895;
        float _S898 = _S851 * _S897;
        float _S899 = y0_6 * _S896;
        float _S900 = _S849 * _S899;
        float _S901 = _S851 * _S899;
        float _S902 = (_S850 * _S897 + - (_S848 * _S895 + _S853 * _S899) + _S835 * _S900 + _S854 * _S899 + _S855 * _S901) / _S856;
        float _S903 = x0_6 * _S902;
        float _S904 = (_S895 + 2.0f * - _S898 + _S851 * _S901) / _S856;
        float _S905 = _S852 * _S896 + x0_6 * _S904;
        float _S906 = _S898 + _S851 * _S900;
        float _S907 = _S834 * - _S902 + y0_6 * - _S904;
        _S845 = _S898;
        _S846 = _S905;
        _S847 = _S907;
        _S848 = 0.0f;
        _S849 = _S906;
        _S850 = _S903;
    }
    else
    {
        float _S908 = _s_dOut_1.z / _S857;
        float _S909 = _S858 * - _S908;
        float _S910 = _S859 * _S908;
        float _S911 = _S861 * _S909;
        float _S912 = _S863 * _S911;
        float _S913 = _S864 * _S910;
        float _S914 = _S861 * _S913;
        float _S915 = _S863 * _S913;
        float _S916 = (_S862 * _S911 + - (_S860 * _S909 + _S866 * _S913) + _S843 * _S914 + _S867 * _S913 + _S868 * _S915) / _S869;
        float _S917 = _S871 * _S916;
        float _S918 = (_S909 + 2.0f * - _S912 + _S863 * _S915) / _S869;
        float _S919 = _s_dOut_1.z + - (_S865 * _S910 + _S871 * _S918);
        float _S920 = - _S917 + - (_S870 * - _S916 + _S864 * - _S918);
        _S845 = _S912 + _S863 * _S914;
        _S846 = _S919;
        _S847 = _S920;
        _S848 = _S912;
        _S849 = 0.0f;
        _S850 = _S917;
    }
    DiffPair_float_0 _S921;
    (&_S921)->primal_0 = _S668;
    (&_S921)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S921, _S845);
    DiffPair_float_0 _S922 = _S921;
    float _S923 = - (_S846 / _S842);
    DiffPair_float_0 _S924;
    (&_S924)->primal_0 = _S840;
    (&_S924)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S924, _S923);
    float _S925 = - _S924.differential_0;
    float _S926 = - (_S847 / _S839);
    DiffPair_float_0 _S927;
    (&_S927)->primal_0 = _S837;
    (&_S927)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S927, _S926);
    float _S928 = - _S927.differential_0;
    DiffPair_float_0 _S929;
    (&_S929)->primal_0 = _S665;
    (&_S929)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S929, _S848);
    DiffPair_float_0 _S930 = _S929;
    DiffPair_float_0 _S931;
    (&_S931)->primal_0 = _S664;
    (&_S931)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S931, _S849);
    DiffPair_float_0 _S932 = _S931;
    float3  _S933 = make_float3 (0.0f, 0.0f, _S850);
    if(_S784)
    {
        float _S934 = _s_dOut_1.y / _S785;
        float _S935 = _S786 * - _S934;
        float _S936 = _S787 * _S934;
        float _S937 = _S789 * _S935;
        float _S938 = _S791 * _S937;
        float _S939 = y0_5 * _S936;
        float _S940 = _S789 * _S939;
        float _S941 = _S791 * _S939;
        float _S942 = (_S790 * _S937 + - (_S788 * _S935 + _S793 * _S939) + _S775 * _S940 + _S794 * _S939 + _S795 * _S941) / _S796;
        float _S943 = x0_5 * _S942;
        float _S944 = (_S935 + 2.0f * - _S938 + _S791 * _S941) / _S796;
        float _S945 = _S792 * _S936 + x0_5 * _S944;
        float _S946 = _S938 + _S791 * _S940;
        float _S947 = _S774 * - _S942 + y0_5 * - _S944;
        _S785 = _S938;
        _S786 = _S945;
        _S787 = _S947;
        _S788 = 0.0f;
        _S789 = _S946;
        _S790 = _S943;
    }
    else
    {
        float _S948 = _s_dOut_1.y / _S797;
        float _S949 = _S798 * - _S948;
        float _S950 = _S799 * _S948;
        float _S951 = _S801 * _S949;
        float _S952 = _S803 * _S951;
        float _S953 = _S804 * _S950;
        float _S954 = _S801 * _S953;
        float _S955 = _S803 * _S953;
        float _S956 = (_S802 * _S951 + - (_S800 * _S949 + _S806 * _S953) + _S783 * _S954 + _S807 * _S953 + _S808 * _S955) / _S809;
        float _S957 = _S811 * _S956;
        float _S958 = (_S949 + 2.0f * - _S952 + _S803 * _S955) / _S809;
        float _S959 = _s_dOut_1.y + - (_S805 * _S950 + _S811 * _S958);
        float _S960 = - _S957 + - (_S810 * - _S956 + _S804 * - _S958);
        _S785 = _S952 + _S803 * _S954;
        _S786 = _S959;
        _S787 = _S960;
        _S788 = _S952;
        _S789 = 0.0f;
        _S790 = _S957;
    }
    DiffPair_float_0 _S961;
    (&_S961)->primal_0 = _S663;
    (&_S961)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S961, _S785);
    DiffPair_float_0 _S962 = _S961;
    float _S963 = - (_S786 / _S782);
    DiffPair_float_0 _S964;
    (&_S964)->primal_0 = _S780;
    (&_S964)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S964, _S963);
    float _S965 = - _S964.differential_0;
    float _S966 = - (_S787 / _S779);
    DiffPair_float_0 _S967;
    (&_S967)->primal_0 = _S777;
    (&_S967)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S967, _S966);
    float _S968 = - _S967.differential_0;
    DiffPair_float_0 _S969;
    (&_S969)->primal_0 = _S660;
    (&_S969)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S969, _S788);
    DiffPair_float_0 _S970 = _S969;
    DiffPair_float_0 _S971;
    (&_S971)->primal_0 = _S659;
    (&_S971)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S971, _S789);
    DiffPair_float_0 _S972 = _S971;
    float3  _S973 = _S933 + make_float3 (0.0f, _S790, 0.0f);
    if(_S724)
    {
        float _S974 = _s_dOut_1.x / _S725;
        float _S975 = _S726 * - _S974;
        float _S976 = _S727 * _S974;
        float _S977 = _S729 * _S975;
        float _S978 = _S731 * _S977;
        float _S979 = y0_4 * _S976;
        float _S980 = _S729 * _S979;
        float _S981 = _S731 * _S979;
        float _S982 = (_S730 * _S977 + - (_S728 * _S975 + _S733 * _S979) + _S715 * _S980 + _S734 * _S979 + _S735 * _S981) / _S736;
        float _S983 = x0_4 * _S982;
        float _S984 = (_S975 + 2.0f * - _S978 + _S731 * _S981) / _S736;
        float _S985 = _S732 * _S976 + x0_4 * _S984;
        float _S986 = _S978 + _S731 * _S980;
        float _S987 = _S714 * - _S982 + y0_4 * - _S984;
        _S725 = _S978;
        _S726 = _S985;
        _S727 = _S987;
        _S728 = 0.0f;
        _S729 = _S986;
        _S730 = _S983;
    }
    else
    {
        float _S988 = _s_dOut_1.x / _S737;
        float _S989 = _S738 * - _S988;
        float _S990 = _S739 * _S988;
        float _S991 = _S741 * _S989;
        float _S992 = _S743 * _S991;
        float _S993 = _S744 * _S990;
        float _S994 = _S741 * _S993;
        float _S995 = _S743 * _S993;
        float _S996 = (_S742 * _S991 + - (_S740 * _S989 + _S746 * _S993) + _S723 * _S994 + _S747 * _S993 + _S748 * _S995) / _S749;
        float _S997 = _S751 * _S996;
        float _S998 = (_S989 + 2.0f * - _S992 + _S743 * _S995) / _S749;
        float _S999 = _s_dOut_1.x + - (_S745 * _S990 + _S751 * _S998);
        float _S1000 = - _S997 + - (_S750 * - _S996 + _S744 * - _S998);
        _S725 = _S992 + _S743 * _S994;
        _S726 = _S999;
        _S727 = _S1000;
        _S728 = _S992;
        _S729 = 0.0f;
        _S730 = _S997;
    }
    DiffPair_float_0 _S1001;
    (&_S1001)->primal_0 = _S658;
    (&_S1001)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S1001, _S725);
    DiffPair_float_0 _S1002 = _S1001;
    float _S1003 = - (_S726 / _S722);
    DiffPair_float_0 _S1004;
    (&_S1004)->primal_0 = _S720;
    (&_S1004)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S1004, _S1003);
    float _S1005 = - _S1004.differential_0;
    float _S1006 = - (_S727 / _S719);
    DiffPair_float_0 _S1007;
    (&_S1007)->primal_0 = _S717;
    (&_S1007)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S1007, _S1006);
    float _S1008 = - _S1007.differential_0;
    DiffPair_float_0 _S1009;
    (&_S1009)->primal_0 = _S655;
    (&_S1009)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S1009, _S728);
    DiffPair_float_0 _S1010 = _S1009;
    DiffPair_float_0 _S1011;
    (&_S1011)->primal_0 = _S654;
    (&_S1011)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S1011, _S729);
    DiffPair_float_0 _S1012 = _S1011;
    float3  _S1013 = _S973 + make_float3 (_S730, 0.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1014;
    (&_S1014)->primal_0 = _S710;
    (&_S1014)->differential_0 = _S636;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1015;
    (&_S1015)->primal_0 = _S711;
    (&_S1015)->differential_0 = _S636;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1016;
    (&_S1016)->primal_0 = _S712;
    (&_S1016)->differential_0 = _S636;
    s_bwd_prop_clamp_0(&_S1014, &_S1015, &_S1016, _S1013);
    float _S1017 = - _S1014.differential_0.z;
    float _S1018 = _S1014.differential_0.y + _S1017;
    float _S1019 = norm_factor_5 * _S1018;
    float _S1020 = _S1014.differential_0.x + _S1017;
    float _S1021 = norm_factor_5 * _S1020;
    float _S1022 = (_S709 * _S1018 + _S708 * _S1020) / _S707;
    float _S1023 = intensity_5 * - _S1022;
    float _S1024 = _S706 * _S1022;
    DiffPair_float_0 _S1025;
    (&_S1025)->primal_0 = _S704;
    (&_S1025)->differential_0 = 0.0f;
    DiffPair_float_0 _S1026;
    (&_S1026)->primal_0 = _S705;
    (&_S1026)->differential_0 = 0.0f;
    _d_max_0(&_S1025, &_S1026, _S1023);
    float _S1027 = 0.00009999999747379f * _S1026.differential_0;
    DiffPair_float_0 _S1028;
    (&_S1028)->primal_0 = intensity_5;
    (&_S1028)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S1028, _S1027);
    float3  _S1029 = make_float3 (_S1021, _S1019, _S1025.differential_0);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1030;
    (&_S1030)->primal_0 = H_9;
    (&_S1030)->differential_0 = _S637;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1031;
    (&_S1031)->primal_0 = rgi_in_1;
    (&_S1031)->differential_0 = _S636;
    s_bwd_prop_mul_0(&_S1030, &_S1031, _S1029);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1032 = _S1030;
    float _S1033 = _S1014.differential_0.z + _S1024 + _S1028.differential_0 + _S1031.differential_0.z;
    float _S1034 = _S1031.differential_0.y + _S1033;
    float _S1035 = _S1031.differential_0.x + _S1033;
    float3  _S1036 = make_float3 (_S1035, _S1034, _S1033);
    if(_S696)
    {
        Matrix<float, 3, 3>  _S1037 = _S695 * _S1032.differential_0;
        Matrix<float, 3, 3>  _S1038 = _S697 * _S1032.differential_0;
        _S698 = - ((_S1037.rows[int(0)].x + _S1037.rows[int(0)].y + _S1037.rows[int(0)].z + _S1037.rows[int(1)].x + _S1037.rows[int(1)].y + _S1037.rows[int(1)].z + _S1037.rows[int(2)].x + _S1037.rows[int(2)].y + _S1037.rows[int(2)].z) / _S698);
        H_9 = _S1038;
    }
    else
    {
        _S698 = 0.0f;
        H_9 = _S1032.differential_0;
    }
    DiffPair_float_0 _S1039;
    (&_S1039)->primal_0 = _S695.rows[int(2)].z;
    (&_S1039)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S1039, 0.0f);
    float _S1040 = _S1039.differential_0 + _S698;
    float3  _S1041 = _S636;
    *&((&_S1041)->z) = _S1040;
    Matrix<float, 3, 3>  _S1042 = _S637;
    _S1042[int(2)] = _S1041;
    Matrix<float, 3, 3>  _S1043 = H_9 + _S1042;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1044;
    (&_S1044)->primal_0 = _S694;
    (&_S1044)->differential_0 = _S637;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1045;
    (&_S1045)->primal_0 = S_inv_1;
    (&_S1045)->differential_0 = _S637;
    s_bwd_prop_mul_1(&_S1044, &_S1045, _S1043);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1046;
    (&_S1046)->primal_0 = T_5;
    (&_S1046)->differential_0 = _S637;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1047;
    (&_S1047)->primal_0 = D_1;
    (&_S1047)->differential_0 = _S637;
    s_bwd_prop_mul_1(&_S1046, &_S1047, _S1044.differential_0);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1048 = _S1046;
    float3  _S1049 = make_float3 (_S1047.differential_0.rows[int(0)].x, _S1047.differential_0.rows[int(1)].y, _S1047.differential_0.rows[int(2)].z);
    float3  _S1050;
    if(_S689)
    {
        if(_S691)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1051;
            (&_S1051)->primal_0 = r1_5;
            (&_S1051)->differential_0 = _S636;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1052;
            (&_S1052)->primal_0 = r2_20;
            (&_S1052)->differential_0 = _S636;
            s_bwd_prop_cross_0(&_S1051, &_S1052, _S1049);
            _S677 = _S636;
            lambda_v_13 = _S1052.differential_0;
            _S1050 = _S1051.differential_0;
        }
        else
        {
            _S677 = _S1049;
            lambda_v_13 = _S636;
            _S1050 = _S636;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1053;
        (&_S1053)->primal_0 = _S690;
        (&_S1053)->differential_0 = _S636;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1054;
        (&_S1054)->primal_0 = _S690;
        (&_S1054)->differential_0 = _S636;
        s_bwd_prop_dot_0(&_S1053, &_S1054, 0.0f);
        float3  _S1055 = _S1054.differential_0 + _S1053.differential_0 + _S677;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1056;
        (&_S1056)->primal_0 = r0_5;
        (&_S1056)->differential_0 = _S636;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1057;
        (&_S1057)->primal_0 = r2_20;
        (&_S1057)->differential_0 = _S636;
        s_bwd_prop_cross_0(&_S1056, &_S1057, _S1055);
        float3  _S1058 = _S1057.differential_0 + lambda_v_13;
        _S677 = _S636;
        lambda_v_13 = _S1058;
        _S690 = _S1050;
        _S1050 = _S1056.differential_0;
    }
    else
    {
        _S677 = _S1049;
        lambda_v_13 = _S636;
        _S690 = _S636;
        _S1050 = _S636;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1059;
    (&_S1059)->primal_0 = _S688;
    (&_S1059)->differential_0 = _S636;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1060;
    (&_S1060)->primal_0 = _S688;
    (&_S1060)->differential_0 = _S636;
    s_bwd_prop_dot_0(&_S1059, &_S1060, 0.0f);
    float3  _S1061 = _S1060.differential_0 + _S1059.differential_0 + _S677;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1062;
    (&_S1062)->primal_0 = r0_5;
    (&_S1062)->differential_0 = _S636;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1063;
    (&_S1063)->primal_0 = r1_5;
    (&_S1063)->differential_0 = _S636;
    s_bwd_prop_cross_0(&_S1062, &_S1063, _S1061);
    float3  _S1064 = _S636;
    *&((&_S1064)->z) = lambda_v_13.z;
    *&((&_S1064)->y) = lambda_v_13.y;
    *&((&_S1064)->x) = lambda_v_13.x;
    float3  _S1065 = _S1063.differential_0 + _S690;
    float3  _S1066 = _S636;
    *&((&_S1066)->z) = _S1065.z;
    *&((&_S1066)->y) = _S1065.y;
    *&((&_S1066)->x) = _S1065.x;
    float3  _S1067 = _S1062.differential_0 + _S1050;
    float3  _S1068 = _S636;
    *&((&_S1068)->z) = _S1067.z;
    *&((&_S1068)->y) = _S1067.y;
    *&((&_S1068)->x) = _S1067.x;
    Matrix<float, 3, 3>  _S1069 = _S637;
    _S1069[int(2)] = _S1064;
    _S1069[int(1)] = _S1066;
    _S1069[int(0)] = _S1068;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1070;
    (&_S1070)->primal_0 = skew_1;
    (&_S1070)->differential_0 = _S637;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1071;
    (&_S1071)->primal_0 = T_5;
    (&_S1071)->differential_0 = _S637;
    s_bwd_prop_mul_1(&_S1070, &_S1071, _S1069);
    Matrix<float, 3, 3>  _S1072 = _S1071.differential_0 + _S1048.differential_0;
    float2  _S1073 = make_float2 (_S1070.differential_0.rows[int(2)].y + - _S1070.differential_0.rows[int(1)].z, _S1070.differential_0.rows[int(0)].z + - _S1070.differential_0.rows[int(2)].x);
    Matrix<float, 2, 2>  _S1074 = makeMatrix<float, 2, 2> (0.0f);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1075;
    (&_S1075)->primal_0 = makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f);
    (&_S1075)->differential_0 = _S1074;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1076;
    (&_S1076)->primal_0 = _S680.color_params_2.n_0;
    (&_S1076)->differential_0 = _S640;
    s_bwd_prop_mul_2(&_S1075, &_S1076, _S1073);
    float2  _S1077 = make_float2 (_S1072.rows[int(0)].z, _S1072.rows[int(1)].z);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1078;
    (&_S1078)->primal_0 = makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f);
    (&_S1078)->differential_0 = _S1074;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1079;
    (&_S1079)->primal_0 = _S680.color_params_2.g_0;
    (&_S1079)->differential_0 = _S640;
    s_bwd_prop_mul_2(&_S1078, &_S1079, _S1077);
    float2  _S1080 = make_float2 (_S1072.rows[int(0)].y, _S1072.rows[int(1)].y);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1081;
    (&_S1081)->primal_0 = makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f);
    (&_S1081)->differential_0 = _S1074;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1082;
    (&_S1082)->primal_0 = _S680.color_params_2.r_0;
    (&_S1082)->differential_0 = _S640;
    s_bwd_prop_mul_2(&_S1081, &_S1082, _S1080);
    float2  _S1083 = make_float2 (_S1072.rows[int(0)].x, _S1072.rows[int(1)].x);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1084;
    (&_S1084)->primal_0 = makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f);
    (&_S1084)->differential_0 = _S1074;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1085;
    (&_S1085)->primal_0 = _S680.color_params_2.b_0;
    (&_S1085)->differential_0 = _S640;
    s_bwd_prop_mul_2(&_S1084, &_S1085, _S1083);
    ColorPPISPParams_0 _S1086 = ColorPPISPParams_x24_syn_dzero_0();
    (&_S1086)->n_0 = _S1076.differential_0;
    (&_S1086)->g_0 = _S1079.differential_0;
    (&_S1086)->r_0 = _S1082.differential_0;
    (&_S1086)->b_0 = _S1085.differential_0;
    _S677 = _S1036;
    *&((&_S677)->z) = 0.0f;
    float _S1087 = rgb_out_6.z * _S1033;
    float _S1088 = _S679 * _S1033;
    DiffPair_float_0 _S1089;
    (&_S1089)->primal_0 = falloff_5;
    (&_S1089)->differential_0 = 0.0f;
    DiffPair_float_0 _S1090;
    (&_S1090)->primal_0 = 0.0f;
    (&_S1090)->differential_0 = 0.0f;
    DiffPair_float_0 _S1091;
    (&_S1091)->primal_0 = 1.0f;
    (&_S1091)->differential_0 = 0.0f;
    s_bwd_prop_clamp_1(&_S1089, &_S1090, &_S1091, _S1087);
    float _S1092 = r2_19 * _S1089.differential_0;
    float _S1093 = r4_14 * _S1089.differential_0;
    float s_diff_r6_T_3 = _S653 * _S1089.differential_0;
    float _S1094 = r6_5 * _S1089.differential_0;
    float _S1095 = r2_19 * (_S652 * _S1089.differential_0 + r2_19 * s_diff_r6_T_3);
    float _S1096 = _S651 * _S1089.differential_0 + r4_14 * s_diff_r6_T_3 + _S1095 + _S1095;
    float _S1097 = dy_14 * _S1096;
    float _S1098 = dx_14 * _S1096;
    float _S1099 = - (_S1097 + _S1097);
    float _S1100 = - (_S1098 + _S1098);
    *&((&_S677)->y) = 0.0f;
    float _S1101 = rgb_out_6.y * _S1034;
    float _S1102 = _S678 * _S1034;
    DiffPair_float_0 _S1103;
    (&_S1103)->primal_0 = falloff_4;
    (&_S1103)->differential_0 = 0.0f;
    DiffPair_float_0 _S1104;
    (&_S1104)->primal_0 = 0.0f;
    (&_S1104)->differential_0 = 0.0f;
    DiffPair_float_0 _S1105;
    (&_S1105)->primal_0 = 1.0f;
    (&_S1105)->differential_0 = 0.0f;
    s_bwd_prop_clamp_1(&_S1103, &_S1104, &_S1105, _S1101);
    float _S1106 = r2_18 * _S1103.differential_0;
    float _S1107 = r4_13 * _S1103.differential_0;
    float s_diff_r6_T_4 = _S650 * _S1103.differential_0;
    float _S1108 = r6_4 * _S1103.differential_0;
    float _S1109 = r2_18 * (_S649 * _S1103.differential_0 + r2_18 * s_diff_r6_T_4);
    float _S1110 = _S648 * _S1103.differential_0 + r4_13 * s_diff_r6_T_4 + _S1109 + _S1109;
    float _S1111 = dy_13 * _S1110;
    float _S1112 = dx_13 * _S1110;
    float _S1113 = - (_S1111 + _S1111);
    float _S1114 = - (_S1112 + _S1112);
    *&((&_S677)->x) = 0.0f;
    float _S1115 = rgb_out_6.x * _S1035;
    float _S1116 = _S675 * _S1035;
    DiffPair_float_0 _S1117;
    (&_S1117)->primal_0 = falloff_3;
    (&_S1117)->differential_0 = 0.0f;
    DiffPair_float_0 _S1118;
    (&_S1118)->primal_0 = 0.0f;
    (&_S1118)->differential_0 = 0.0f;
    DiffPair_float_0 _S1119;
    (&_S1119)->primal_0 = 1.0f;
    (&_S1119)->differential_0 = 0.0f;
    s_bwd_prop_clamp_1(&_S1117, &_S1118, &_S1119, _S1115);
    float _S1120 = r2_17 * _S1117.differential_0;
    float _S1121 = r4_12 * _S1117.differential_0;
    float s_diff_r6_T_5 = _S647 * _S1117.differential_0;
    float _S1122 = r6_3 * _S1117.differential_0;
    float _S1123 = r2_17 * (_S646 * _S1117.differential_0 + r2_17 * s_diff_r6_T_5);
    float _S1124 = _S645 * _S1117.differential_0 + r4_12 * s_diff_r6_T_5 + _S1123 + _S1123;
    float _S1125 = dy_12 * _S1124;
    float _S1126 = dx_12 * _S1124;
    float _S1127 = - (_S1125 + _S1125);
    float _S1128 = - (_S1126 + _S1126);
    float3  _S1129 = _S636;
    *&((&_S1129)->z) = _S1088;
    *&((&_S1129)->y) = _S1102;
    *&((&_S1129)->x) = _S1116;
    float3  _S1130 = _S677 + _S1129;
    float3  _S1131 = _S635.primal_0 * _S1130;
    float3  _S1132 = _S671 * _S1130;
    float _S1133 = _S1131.x + _S1131.y + _S1131.z;
    DiffPair_float_0 _S1134;
    (&_S1134)->primal_0 = _S669.exposure_2;
    (&_S1134)->differential_0 = 0.0f;
    s_bwd_prop_exp2_0(&_S1134, _S1133);
    PPISPParamsRQS_0 _S1135 = PPISPParamsRQS_x24_syn_dzero_0();
    (&_S1135)->color_params_2 = _S1086;
    (&_S1135)->exposure_2 = _S1134.differential_0;
    _S644 = _S1135;
    (&(&_S644)->crf_params_0[int(2)])->gc_0 = 0.0f;
    float _S1136 = _S1135.crf_params_0[int(2)].gc_0 + _S922.differential_0;
    (&(&_S644)->crf_params_0[int(2)])->y0_0 = 0.0f;
    float _S1137 = _S1135.crf_params_0[int(2)].y0_0 + _S925;
    (&(&_S644)->crf_params_0[int(2)])->x0_0 = 0.0f;
    float _S1138 = _S1135.crf_params_0[int(2)].x0_0 + _S928;
    (&(&_S644)->crf_params_0[int(2)])->g1_0 = 0.0f;
    float _S1139 = _S1135.crf_params_0[int(2)].g1_0 + _S930.differential_0;
    (&(&_S644)->crf_params_0[int(2)])->g0_0 = 0.0f;
    float _S1140 = _S1135.crf_params_0[int(2)].g0_0 + _S932.differential_0;
    (&(&_S644)->crf_params_0[int(1)])->gc_0 = 0.0f;
    float _S1141 = _S1135.crf_params_0[int(1)].gc_0 + _S962.differential_0;
    (&(&_S644)->crf_params_0[int(1)])->y0_0 = 0.0f;
    float _S1142 = _S1135.crf_params_0[int(1)].y0_0 + _S965;
    (&(&_S644)->crf_params_0[int(1)])->x0_0 = 0.0f;
    float _S1143 = _S1135.crf_params_0[int(1)].x0_0 + _S968;
    (&(&_S644)->crf_params_0[int(1)])->g1_0 = 0.0f;
    float _S1144 = _S1135.crf_params_0[int(1)].g1_0 + _S970.differential_0;
    (&(&_S644)->crf_params_0[int(1)])->g0_0 = 0.0f;
    float _S1145 = _S1135.crf_params_0[int(1)].g0_0 + _S972.differential_0;
    (&(&_S644)->crf_params_0[int(0)])->gc_0 = 0.0f;
    float _S1146 = _S1135.crf_params_0[int(0)].gc_0 + _S1002.differential_0;
    (&(&_S644)->crf_params_0[int(0)])->y0_0 = 0.0f;
    float _S1147 = _S1135.crf_params_0[int(0)].y0_0 + _S1005;
    (&(&_S644)->crf_params_0[int(0)])->x0_0 = 0.0f;
    float _S1148 = _S1135.crf_params_0[int(0)].x0_0 + _S1008;
    (&(&_S644)->crf_params_0[int(0)])->g1_0 = 0.0f;
    float _S1149 = _S1135.crf_params_0[int(0)].g1_0 + _S1010.differential_0;
    (&(&_S644)->crf_params_0[int(0)])->g0_0 = 0.0f;
    float _S1150 = _S1135.crf_params_0[int(0)].g0_0 + _S1012.differential_0;
    *&((&(&(&_S644)->color_params_2)->n_0)->y) = 0.0f;
    *&((&(&(&_S644)->color_params_2)->n_0)->x) = 0.0f;
    *&((&(&(&_S644)->color_params_2)->g_0)->y) = 0.0f;
    *&((&(&(&_S644)->color_params_2)->g_0)->x) = 0.0f;
    *&((&(&(&_S644)->color_params_2)->r_0)->y) = 0.0f;
    *&((&(&(&_S644)->color_params_2)->r_0)->x) = 0.0f;
    *&((&(&(&_S644)->color_params_2)->b_0)->y) = 0.0f;
    *&((&(&(&_S644)->color_params_2)->b_0)->x) = 0.0f;
    (&(&_S644)->vignette_params_1[int(2)])->alpha2_0 = 0.0f;
    float _S1151 = _S1094 + _S1135.vignette_params_1[int(2)].alpha2_0;
    (&(&_S644)->vignette_params_1[int(2)])->alpha1_0 = 0.0f;
    float _S1152 = _S1093 + _S1135.vignette_params_1[int(2)].alpha1_0;
    (&(&_S644)->vignette_params_1[int(2)])->alpha0_0 = 0.0f;
    float _S1153 = _S1092 + _S1135.vignette_params_1[int(2)].alpha0_0;
    (&(&_S644)->vignette_params_1[int(2)])->cy_0 = 0.0f;
    float _S1154 = _S1099 + _S1135.vignette_params_1[int(2)].cy_0;
    (&(&_S644)->vignette_params_1[int(2)])->cx_0 = 0.0f;
    float _S1155 = _S1100 + _S1135.vignette_params_1[int(2)].cx_0;
    (&(&_S644)->vignette_params_1[int(1)])->alpha2_0 = 0.0f;
    float _S1156 = _S1108 + _S1135.vignette_params_1[int(1)].alpha2_0;
    (&(&_S644)->vignette_params_1[int(1)])->alpha1_0 = 0.0f;
    float _S1157 = _S1107 + _S1135.vignette_params_1[int(1)].alpha1_0;
    (&(&_S644)->vignette_params_1[int(1)])->alpha0_0 = 0.0f;
    float _S1158 = _S1106 + _S1135.vignette_params_1[int(1)].alpha0_0;
    (&(&_S644)->vignette_params_1[int(1)])->cy_0 = 0.0f;
    float _S1159 = _S1113 + _S1135.vignette_params_1[int(1)].cy_0;
    (&(&_S644)->vignette_params_1[int(1)])->cx_0 = 0.0f;
    float _S1160 = _S1114 + _S1135.vignette_params_1[int(1)].cx_0;
    (&(&_S644)->vignette_params_1[int(0)])->alpha2_0 = 0.0f;
    float _S1161 = _S1122 + _S1135.vignette_params_1[int(0)].alpha2_0;
    (&(&_S644)->vignette_params_1[int(0)])->alpha1_0 = 0.0f;
    float _S1162 = _S1121 + _S1135.vignette_params_1[int(0)].alpha1_0;
    (&(&_S644)->vignette_params_1[int(0)])->alpha0_0 = 0.0f;
    float _S1163 = _S1120 + _S1135.vignette_params_1[int(0)].alpha0_0;
    (&(&_S644)->vignette_params_1[int(0)])->cy_0 = 0.0f;
    float _S1164 = _S1127 + _S1135.vignette_params_1[int(0)].cy_0;
    (&(&_S644)->vignette_params_1[int(0)])->cx_0 = 0.0f;
    float _S1165 = _S1128 + _S1135.vignette_params_1[int(0)].cx_0;
    FixedArray<float, 39>  _S1166;
    _S1166[int(0)] = 0.0f;
    _S1166[int(1)] = 0.0f;
    _S1166[int(2)] = 0.0f;
    _S1166[int(3)] = 0.0f;
    _S1166[int(4)] = 0.0f;
    _S1166[int(5)] = 0.0f;
    _S1166[int(6)] = 0.0f;
    _S1166[int(7)] = 0.0f;
    _S1166[int(8)] = 0.0f;
    _S1166[int(9)] = 0.0f;
    _S1166[int(10)] = 0.0f;
    _S1166[int(11)] = 0.0f;
    _S1166[int(12)] = 0.0f;
    _S1166[int(13)] = 0.0f;
    _S1166[int(14)] = 0.0f;
    _S1166[int(15)] = 0.0f;
    _S1166[int(16)] = 0.0f;
    _S1166[int(17)] = 0.0f;
    _S1166[int(18)] = 0.0f;
    _S1166[int(19)] = 0.0f;
    _S1166[int(20)] = 0.0f;
    _S1166[int(21)] = 0.0f;
    _S1166[int(22)] = 0.0f;
    _S1166[int(23)] = 0.0f;
    _S1166[int(24)] = 0.0f;
    _S1166[int(25)] = 0.0f;
    _S1166[int(26)] = 0.0f;
    _S1166[int(27)] = 0.0f;
    _S1166[int(28)] = 0.0f;
    _S1166[int(29)] = 0.0f;
    _S1166[int(30)] = 0.0f;
    _S1166[int(31)] = 0.0f;
    _S1166[int(32)] = 0.0f;
    _S1166[int(33)] = 0.0f;
    _S1166[int(34)] = 0.0f;
    _S1166[int(35)] = 0.0f;
    _S1166[int(36)] = 0.0f;
    _S1166[int(37)] = 0.0f;
    _S1166[int(38)] = 0.0f;
    _S1166[int(9)] = _S1157;
    _S1166[int(18)] = _S1135.color_params_2.r_0.x;
    _S1166[int(17)] = _S1135.color_params_2.b_0.y;
    _S1166[int(16)] = _S1135.color_params_2.b_0.x;
    _S1166[int(15)] = _S1151;
    _S1166[int(14)] = _S1152;
    _S1166[int(13)] = _S1153;
    _S1166[int(12)] = _S1154;
    _S1166[int(11)] = _S1155;
    _S1166[int(10)] = _S1156;
    _S1166[int(19)] = _S1135.color_params_2.r_0.y;
    _S1166[int(8)] = _S1158;
    _S1166[int(7)] = _S1159;
    _S1166[int(6)] = _S1160;
    _S1166[int(5)] = _S1161;
    _S1166[int(4)] = _S1162;
    _S1166[int(3)] = _S1163;
    _S1166[int(2)] = _S1164;
    _S1166[int(1)] = _S1165;
    _S1166[int(0)] = _S644.exposure_2;
    _S1166[int(28)] = _S1146;
    _S1166[int(37)] = _S1137;
    _S1166[int(36)] = _S1138;
    _S1166[int(35)] = _S1139;
    _S1166[int(34)] = _S1140;
    _S1166[int(33)] = _S1141;
    _S1166[int(32)] = _S1142;
    _S1166[int(31)] = _S1143;
    _S1166[int(30)] = _S1144;
    _S1166[int(29)] = _S1145;
    _S1166[int(38)] = _S1136;
    _S1166[int(27)] = _S1147;
    _S1166[int(26)] = _S1148;
    _S1166[int(25)] = _S1149;
    _S1166[int(24)] = _S1150;
    _S1166[int(23)] = _S1135.color_params_2.n_0.y;
    _S1166[int(22)] = _S1135.color_params_2.n_0.x;
    _S1166[int(21)] = _S1135.color_params_2.g_0.y;
    _S1166[int(20)] = _S1135.color_params_2.g_0.x;
    dpparams_1->primal_0 = dpparams_1->primal_0;
    dpparams_1->differential_0 = _S1166;
    dprgb_in_1->primal_0 = (*dprgb_in_1).primal_0;
    dprgb_in_1->differential_0 = _S1132;
    return;
}

inline __device__ void s_bwd_apply_ppisp_rqs_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1167, float2  _S1168, float2  _S1169, float2  _S1170, DiffPair_arrayx3Cfloatx2C39x3E_0 * _S1171, float3  _S1172)
{
    s_bwd_prop_apply_ppisp_rqs_0(_S1167, _S1168, _S1169, _S1170, _S1171, _S1172);
    return;
}

inline __device__ void apply_ppisp_rqs_vjp(float3  rgb_in_5, float2  pix_coord_7, float2  image_center_7, float2  img_size_7, FixedArray<float, 39>  params_5, float3  grad_out_1, float3  * grad_rgb_in_1, FixedArray<float, 39>  * grad_params_1)
{
    float3  _S1173 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_rgb_in_1;
    (&dp_rgb_in_1)->primal_0 = rgb_in_5;
    (&dp_rgb_in_1)->differential_0 = _S1173;
    FixedArray<float, 39>  _S1174 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    DiffPair_arrayx3Cfloatx2C39x3E_0 dp_params_1;
    (&dp_params_1)->primal_0 = params_5;
    (&dp_params_1)->differential_0 = _S1174;
    s_bwd_apply_ppisp_rqs_0(&dp_rgb_in_1, pix_coord_7, image_center_7, img_size_7, &dp_params_1, grad_out_1);
    *grad_rgb_in_1 = dp_rgb_in_1.differential_0;
    *grad_params_1 = (&dp_params_1)->differential_0;
    return;
}

struct DiffPair_arrayx3Cfloatx2C24x3E_0
{
    FixedArray<float, 24>  primal_0;
    FixedArray<float, 24>  differential_0;
};

inline __device__ void s_bwd_prop_apply_ppisp_no_crf_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dprgb_in_2, float2  pix_coord_8, float2  image_center_8, float2  img_size_8, DiffPair_arrayx3Cfloatx2C24x3E_0 * dpparams_2, bool clamp_output_2, float3  _s_dOut_2)
{
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1175 = *dprgb_in_2;
    float3  _S1176 = make_float3 (0.0f);
    Matrix<float, 3, 3>  _S1177 = makeMatrix<float, 3, 3> (0.0f);
    VignettingChannelParams_0 _S1178 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    FixedArray<VignettingChannelParams_0, 3>  _S1179 = {
        _S1178, _S1178, _S1178
    };
    float2  _S1180 = make_float2 (0.0f);
    ColorPPISPParams_0 _S1181 = { _S1180, _S1180, _S1180, _S1180 };
    PPISPParamsNoCRF_0 _S1182;
    (&_S1182)->exposure_1 = dpparams_2->primal_0[int(0)];
    (&_S1182)->vignette_params_0 = _S1179;
    (&_S1182)->color_params_1 = _S1181;
    (&(&_S1182)->vignette_params_0[int(0)])->cx_0 = dpparams_2->primal_0[int(1)];
    (&(&_S1182)->vignette_params_0[int(0)])->cy_0 = dpparams_2->primal_0[int(2)];
    float _S1183 = dpparams_2->primal_0[int(3)];
    (&(&_S1182)->vignette_params_0[int(0)])->alpha0_0 = dpparams_2->primal_0[int(3)];
    float _S1184 = dpparams_2->primal_0[int(4)];
    (&(&_S1182)->vignette_params_0[int(0)])->alpha1_0 = dpparams_2->primal_0[int(4)];
    float _S1185 = dpparams_2->primal_0[int(5)];
    (&(&_S1182)->vignette_params_0[int(0)])->alpha2_0 = dpparams_2->primal_0[int(5)];
    (&(&_S1182)->vignette_params_0[int(1)])->cx_0 = dpparams_2->primal_0[int(6)];
    (&(&_S1182)->vignette_params_0[int(1)])->cy_0 = dpparams_2->primal_0[int(7)];
    float _S1186 = dpparams_2->primal_0[int(8)];
    (&(&_S1182)->vignette_params_0[int(1)])->alpha0_0 = dpparams_2->primal_0[int(8)];
    float _S1187 = dpparams_2->primal_0[int(9)];
    (&(&_S1182)->vignette_params_0[int(1)])->alpha1_0 = dpparams_2->primal_0[int(9)];
    float _S1188 = dpparams_2->primal_0[int(10)];
    (&(&_S1182)->vignette_params_0[int(1)])->alpha2_0 = dpparams_2->primal_0[int(10)];
    (&(&_S1182)->vignette_params_0[int(2)])->cx_0 = dpparams_2->primal_0[int(11)];
    (&(&_S1182)->vignette_params_0[int(2)])->cy_0 = dpparams_2->primal_0[int(12)];
    float _S1189 = dpparams_2->primal_0[int(13)];
    (&(&_S1182)->vignette_params_0[int(2)])->alpha0_0 = dpparams_2->primal_0[int(13)];
    float _S1190 = dpparams_2->primal_0[int(14)];
    (&(&_S1182)->vignette_params_0[int(2)])->alpha1_0 = dpparams_2->primal_0[int(14)];
    float _S1191 = dpparams_2->primal_0[int(15)];
    (&(&_S1182)->vignette_params_0[int(2)])->alpha2_0 = dpparams_2->primal_0[int(15)];
    *&((&(&(&_S1182)->color_params_1)->b_0)->x) = dpparams_2->primal_0[int(16)];
    *&((&(&(&_S1182)->color_params_1)->b_0)->y) = dpparams_2->primal_0[int(17)];
    *&((&(&(&_S1182)->color_params_1)->r_0)->x) = dpparams_2->primal_0[int(18)];
    *&((&(&(&_S1182)->color_params_1)->r_0)->y) = dpparams_2->primal_0[int(19)];
    *&((&(&(&_S1182)->color_params_1)->g_0)->x) = dpparams_2->primal_0[int(20)];
    *&((&(&(&_S1182)->color_params_1)->g_0)->y) = dpparams_2->primal_0[int(21)];
    *&((&(&(&_S1182)->color_params_1)->n_0)->x) = dpparams_2->primal_0[int(22)];
    *&((&(&(&_S1182)->color_params_1)->n_0)->y) = dpparams_2->primal_0[int(23)];
    PPISPParamsNoCRF_0 _S1192 = _S1182;
    float _S1193 = s_primal_ctx_exp2_0(_S1182.exposure_1);
    float3  _S1194 = make_float3 (_S1193);
    float3  rgb_out_7 = (*dprgb_in_2).primal_0 * make_float3 (_S1193);
    float _S1195 = (F32_max((img_size_8.x), (img_size_8.y)));
    float _S1196 = (pix_coord_8.x - image_center_8.x) / _S1195;
    float _S1197 = (pix_coord_8.y - image_center_8.y) / _S1195;
    float dx_15 = _S1196 - dpparams_2->primal_0[int(1)];
    float dy_15 = _S1197 - dpparams_2->primal_0[int(2)];
    float r2_21 = dx_15 * dx_15 + dy_15 * dy_15;
    float r4_15 = r2_21 * r2_21;
    float r6_6 = r4_15 * r2_21;
    float falloff_6 = dpparams_2->primal_0[int(5)] * r6_6 + dpparams_2->primal_0[int(4)] * r4_15 + dpparams_2->primal_0[int(3)] * r2_21 + 1.0f;
    float _S1198 = s_primal_ctx_clamp_0(falloff_6, 0.0f, 1.0f);
    float _S1199 = rgb_out_7.x * _S1198;
    float3  _S1200 = rgb_out_7;
    *&((&_S1200)->x) = _S1199;
    float dx_16 = _S1196 - dpparams_2->primal_0[int(6)];
    float dy_16 = _S1197 - dpparams_2->primal_0[int(7)];
    float r2_22 = dx_16 * dx_16 + dy_16 * dy_16;
    float r4_16 = r2_22 * r2_22;
    float r6_7 = r4_16 * r2_22;
    float falloff_7 = dpparams_2->primal_0[int(10)] * r6_7 + dpparams_2->primal_0[int(9)] * r4_16 + dpparams_2->primal_0[int(8)] * r2_22 + 1.0f;
    float _S1201 = s_primal_ctx_clamp_0(falloff_7, 0.0f, 1.0f);
    *&((&_S1200)->y) = rgb_out_7.y * _S1201;
    float dx_17 = _S1196 - dpparams_2->primal_0[int(11)];
    float dy_17 = _S1197 - dpparams_2->primal_0[int(12)];
    float r2_23 = dx_17 * dx_17 + dy_17 * dy_17;
    float r4_17 = r2_23 * r2_23;
    float r6_8 = r4_17 * r2_23;
    float falloff_8 = dpparams_2->primal_0[int(15)] * r6_8 + dpparams_2->primal_0[int(14)] * r4_17 + dpparams_2->primal_0[int(13)] * r2_23 + 1.0f;
    float _S1202 = s_primal_ctx_clamp_0(falloff_8, 0.0f, 1.0f);
    *&((&_S1200)->z) = rgb_out_7.z * _S1202;
    PPISPParamsNoCRF_0 _S1203 = _S1182;
    float2  _S1204 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f), _S1182.color_params_1.b_0);
    float2  _S1205 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f), _S1182.color_params_1.r_0);
    float2  _S1206 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f), _S1182.color_params_1.g_0);
    float2  _S1207 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f), _S1182.color_params_1.n_0);
    float _S1208 = 0.3333333432674408f + _S1207.x;
    float _S1209 = 0.3333333432674408f + _S1207.y;
    Matrix<float, 3, 3>  T_6 = makeMatrix<float, 3, 3> (_S1204.x, 1.0f + _S1205.x, _S1206.x, _S1204.y, _S1205.y, 1.0f + _S1206.y, 1.0f, 1.0f, 1.0f);
    Matrix<float, 3, 3>  skew_2 = makeMatrix<float, 3, 3> (0.0f, -1.0f, _S1209, 1.0f, 0.0f, - _S1208, - _S1209, _S1208, 0.0f);
    Matrix<float, 3, 3>  _S1210 = s_primal_ctx_mul_1(skew_2, T_6);
    float3  r0_6 = make_float3 (_S1210.rows[int(0)].x, _S1210.rows[int(0)].y, _S1210.rows[int(0)].z);
    float3  r1_6 = make_float3 (_S1210.rows[int(1)].x, _S1210.rows[int(1)].y, _S1210.rows[int(1)].z);
    float3  r2_24 = make_float3 (_S1210.rows[int(2)].x, _S1210.rows[int(2)].y, _S1210.rows[int(2)].z);
    float3  _S1211 = s_primal_ctx_cross_0(r0_6, r1_6);
    bool _S1212 = (s_primal_ctx_dot_0(_S1211, _S1211)) < 9.99999968265522539e-21f;
    float3  lambda_v_14;
    float3  _S1213;
    bool _S1214;
    if(_S1212)
    {
        float3  _S1215 = s_primal_ctx_cross_0(r0_6, r2_24);
        bool _S1216 = (s_primal_ctx_dot_0(_S1215, _S1215)) < 9.99999968265522539e-21f;
        if(_S1216)
        {
            lambda_v_14 = s_primal_ctx_cross_0(r1_6, r2_24);
        }
        else
        {
            lambda_v_14 = _S1215;
        }
        _S1214 = _S1216;
        _S1213 = _S1215;
    }
    else
    {
        lambda_v_14 = _S1211;
        _S1214 = false;
        _S1213 = _S1176;
    }
    Matrix<float, 3, 3>  S_inv_2 = makeMatrix<float, 3, 3> (-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    Matrix<float, 3, 3>  D_2 = makeMatrix<float, 3, 3> (lambda_v_14.x, 0.0f, 0.0f, 0.0f, lambda_v_14.y, 0.0f, 0.0f, 0.0f, lambda_v_14.z);
    Matrix<float, 3, 3>  _S1217 = s_primal_ctx_mul_1(T_6, D_2);
    Matrix<float, 3, 3>  _S1218 = s_primal_ctx_mul_1(_S1217, S_inv_2);
    bool _S1219 = (s_primal_ctx_abs_0(_S1218.rows[int(2)].z)) > 9.99999968265522539e-21f;
    Matrix<float, 3, 3>  H_10;
    Matrix<float, 3, 3>  _S1220;
    float _S1221;
    if(_S1219)
    {
        float inv_s_2 = 1.0f / _S1218.rows[int(2)].z;
        Matrix<float, 3, 3>  _S1222 = makeMatrix<float, 3, 3> (inv_s_2);
        float _S1223 = _S1218.rows[int(2)].z * _S1218.rows[int(2)].z;
        H_10 = _S1218 * makeMatrix<float, 3, 3> (inv_s_2);
        _S1220 = _S1222;
        _S1221 = _S1223;
    }
    else
    {
        H_10 = _S1218;
        _S1220 = _S1177;
        _S1221 = 0.0f;
    }
    float _S1224 = _S1200.x;
    float _S1225 = _S1200.y;
    float intensity_6 = _S1224 + _S1225 + _S1200.z;
    float3  rgi_in_2 = make_float3 (_S1224, _S1225, intensity_6);
    float3  _S1226 = s_primal_ctx_mul_2(H_10, rgi_in_2);
    float _S1227 = _S1226.z;
    float _S1228 = 0.00009999999747379f * s_primal_ctx_abs_0(intensity_6) + 9.99999993922529029e-09f;
    float _S1229 = (F32_max((_S1227), (_S1228)));
    float norm_factor_6 = intensity_6 / _S1229;
    float _S1230 = _S1229 * _S1229;
    float _S1231 = _S1226.x;
    float out_r_6 = _S1231 * norm_factor_6;
    float _S1232 = _S1226.y;
    float out_g_6 = _S1232 * norm_factor_6;
    float3  _S1233 = make_float3 (out_r_6, out_g_6, intensity_6 - out_r_6 - out_g_6);
    if(clamp_output_2)
    {
        float3  _S1234 = make_float3 (1.0f);
        _S1200 = make_float3 (0.0f);
        lambda_v_14 = _S1234;
    }
    else
    {
        _S1200 = _S1176;
        lambda_v_14 = _S1176;
    }
    if(clamp_output_2)
    {
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1235;
        (&_S1235)->primal_0 = _S1233;
        (&_S1235)->differential_0 = _S1176;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1236;
        (&_S1236)->primal_0 = _S1200;
        (&_S1236)->differential_0 = _S1176;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1237;
        (&_S1237)->primal_0 = lambda_v_14;
        (&_S1237)->differential_0 = _S1176;
        s_bwd_prop_clamp_0(&_S1235, &_S1236, &_S1237, _s_dOut_2);
        _S1200 = _S1235.differential_0;
    }
    else
    {
        _S1200 = _s_dOut_2;
    }
    float _S1238 = - _S1200.z;
    float _S1239 = _S1200.y + _S1238;
    float _S1240 = norm_factor_6 * _S1239;
    float _S1241 = _S1200.x + _S1238;
    float _S1242 = norm_factor_6 * _S1241;
    float _S1243 = (_S1232 * _S1239 + _S1231 * _S1241) / _S1230;
    float _S1244 = intensity_6 * - _S1243;
    float _S1245 = _S1229 * _S1243;
    DiffPair_float_0 _S1246;
    (&_S1246)->primal_0 = _S1227;
    (&_S1246)->differential_0 = 0.0f;
    DiffPair_float_0 _S1247;
    (&_S1247)->primal_0 = _S1228;
    (&_S1247)->differential_0 = 0.0f;
    _d_max_0(&_S1246, &_S1247, _S1244);
    float _S1248 = 0.00009999999747379f * _S1247.differential_0;
    DiffPair_float_0 _S1249;
    (&_S1249)->primal_0 = intensity_6;
    (&_S1249)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S1249, _S1248);
    float3  _S1250 = make_float3 (_S1242, _S1240, _S1246.differential_0);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1251;
    (&_S1251)->primal_0 = H_10;
    (&_S1251)->differential_0 = _S1177;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1252;
    (&_S1252)->primal_0 = rgi_in_2;
    (&_S1252)->differential_0 = _S1176;
    s_bwd_prop_mul_0(&_S1251, &_S1252, _S1250);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1253 = _S1251;
    float _S1254 = _S1200.z + _S1245 + _S1249.differential_0 + _S1252.differential_0.z;
    float _S1255 = _S1252.differential_0.y + _S1254;
    float _S1256 = _S1252.differential_0.x + _S1254;
    float3  _S1257 = make_float3 (_S1256, _S1255, _S1254);
    if(_S1219)
    {
        Matrix<float, 3, 3>  _S1258 = _S1218 * _S1253.differential_0;
        Matrix<float, 3, 3>  _S1259 = _S1220 * _S1253.differential_0;
        _S1221 = - ((_S1258.rows[int(0)].x + _S1258.rows[int(0)].y + _S1258.rows[int(0)].z + _S1258.rows[int(1)].x + _S1258.rows[int(1)].y + _S1258.rows[int(1)].z + _S1258.rows[int(2)].x + _S1258.rows[int(2)].y + _S1258.rows[int(2)].z) / _S1221);
        H_10 = _S1259;
    }
    else
    {
        _S1221 = 0.0f;
        H_10 = _S1253.differential_0;
    }
    DiffPair_float_0 _S1260;
    (&_S1260)->primal_0 = _S1218.rows[int(2)].z;
    (&_S1260)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S1260, 0.0f);
    float _S1261 = _S1260.differential_0 + _S1221;
    float3  _S1262 = _S1176;
    *&((&_S1262)->z) = _S1261;
    Matrix<float, 3, 3>  _S1263 = _S1177;
    _S1263[int(2)] = _S1262;
    Matrix<float, 3, 3>  _S1264 = H_10 + _S1263;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1265;
    (&_S1265)->primal_0 = _S1217;
    (&_S1265)->differential_0 = _S1177;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1266;
    (&_S1266)->primal_0 = S_inv_2;
    (&_S1266)->differential_0 = _S1177;
    s_bwd_prop_mul_1(&_S1265, &_S1266, _S1264);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1267;
    (&_S1267)->primal_0 = T_6;
    (&_S1267)->differential_0 = _S1177;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1268;
    (&_S1268)->primal_0 = D_2;
    (&_S1268)->differential_0 = _S1177;
    s_bwd_prop_mul_1(&_S1267, &_S1268, _S1265.differential_0);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1269 = _S1267;
    float3  _S1270 = make_float3 (_S1268.differential_0.rows[int(0)].x, _S1268.differential_0.rows[int(1)].y, _S1268.differential_0.rows[int(2)].z);
    float3  _S1271;
    if(_S1212)
    {
        if(_S1214)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1272;
            (&_S1272)->primal_0 = r1_6;
            (&_S1272)->differential_0 = _S1176;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1273;
            (&_S1273)->primal_0 = r2_24;
            (&_S1273)->differential_0 = _S1176;
            s_bwd_prop_cross_0(&_S1272, &_S1273, _S1270);
            _S1200 = _S1176;
            lambda_v_14 = _S1273.differential_0;
            _S1271 = _S1272.differential_0;
        }
        else
        {
            _S1200 = _S1270;
            lambda_v_14 = _S1176;
            _S1271 = _S1176;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1274;
        (&_S1274)->primal_0 = _S1213;
        (&_S1274)->differential_0 = _S1176;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1275;
        (&_S1275)->primal_0 = _S1213;
        (&_S1275)->differential_0 = _S1176;
        s_bwd_prop_dot_0(&_S1274, &_S1275, 0.0f);
        float3  _S1276 = _S1275.differential_0 + _S1274.differential_0 + _S1200;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1277;
        (&_S1277)->primal_0 = r0_6;
        (&_S1277)->differential_0 = _S1176;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1278;
        (&_S1278)->primal_0 = r2_24;
        (&_S1278)->differential_0 = _S1176;
        s_bwd_prop_cross_0(&_S1277, &_S1278, _S1276);
        float3  _S1279 = _S1278.differential_0 + lambda_v_14;
        _S1200 = _S1176;
        lambda_v_14 = _S1279;
        _S1213 = _S1271;
        _S1271 = _S1277.differential_0;
    }
    else
    {
        _S1200 = _S1270;
        lambda_v_14 = _S1176;
        _S1213 = _S1176;
        _S1271 = _S1176;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1280;
    (&_S1280)->primal_0 = _S1211;
    (&_S1280)->differential_0 = _S1176;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1281;
    (&_S1281)->primal_0 = _S1211;
    (&_S1281)->differential_0 = _S1176;
    s_bwd_prop_dot_0(&_S1280, &_S1281, 0.0f);
    float3  _S1282 = _S1281.differential_0 + _S1280.differential_0 + _S1200;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1283;
    (&_S1283)->primal_0 = r0_6;
    (&_S1283)->differential_0 = _S1176;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1284;
    (&_S1284)->primal_0 = r1_6;
    (&_S1284)->differential_0 = _S1176;
    s_bwd_prop_cross_0(&_S1283, &_S1284, _S1282);
    float3  _S1285 = _S1176;
    *&((&_S1285)->z) = lambda_v_14.z;
    *&((&_S1285)->y) = lambda_v_14.y;
    *&((&_S1285)->x) = lambda_v_14.x;
    float3  _S1286 = _S1284.differential_0 + _S1213;
    float3  _S1287 = _S1176;
    *&((&_S1287)->z) = _S1286.z;
    *&((&_S1287)->y) = _S1286.y;
    *&((&_S1287)->x) = _S1286.x;
    float3  _S1288 = _S1283.differential_0 + _S1271;
    float3  _S1289 = _S1176;
    *&((&_S1289)->z) = _S1288.z;
    *&((&_S1289)->y) = _S1288.y;
    *&((&_S1289)->x) = _S1288.x;
    Matrix<float, 3, 3>  _S1290 = _S1177;
    _S1290[int(2)] = _S1285;
    _S1290[int(1)] = _S1287;
    _S1290[int(0)] = _S1289;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1291;
    (&_S1291)->primal_0 = skew_2;
    (&_S1291)->differential_0 = _S1177;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1292;
    (&_S1292)->primal_0 = T_6;
    (&_S1292)->differential_0 = _S1177;
    s_bwd_prop_mul_1(&_S1291, &_S1292, _S1290);
    Matrix<float, 3, 3>  _S1293 = _S1292.differential_0 + _S1269.differential_0;
    float2  _S1294 = make_float2 (_S1291.differential_0.rows[int(2)].y + - _S1291.differential_0.rows[int(1)].z, _S1291.differential_0.rows[int(0)].z + - _S1291.differential_0.rows[int(2)].x);
    Matrix<float, 2, 2>  _S1295 = makeMatrix<float, 2, 2> (0.0f);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1296;
    (&_S1296)->primal_0 = makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f);
    (&_S1296)->differential_0 = _S1295;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1297;
    (&_S1297)->primal_0 = _S1203.color_params_1.n_0;
    (&_S1297)->differential_0 = _S1180;
    s_bwd_prop_mul_2(&_S1296, &_S1297, _S1294);
    float2  _S1298 = make_float2 (_S1293.rows[int(0)].z, _S1293.rows[int(1)].z);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1299;
    (&_S1299)->primal_0 = makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f);
    (&_S1299)->differential_0 = _S1295;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1300;
    (&_S1300)->primal_0 = _S1203.color_params_1.g_0;
    (&_S1300)->differential_0 = _S1180;
    s_bwd_prop_mul_2(&_S1299, &_S1300, _S1298);
    float2  _S1301 = make_float2 (_S1293.rows[int(0)].y, _S1293.rows[int(1)].y);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1302;
    (&_S1302)->primal_0 = makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f);
    (&_S1302)->differential_0 = _S1295;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1303;
    (&_S1303)->primal_0 = _S1203.color_params_1.r_0;
    (&_S1303)->differential_0 = _S1180;
    s_bwd_prop_mul_2(&_S1302, &_S1303, _S1301);
    float2  _S1304 = make_float2 (_S1293.rows[int(0)].x, _S1293.rows[int(1)].x);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1305;
    (&_S1305)->primal_0 = makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f);
    (&_S1305)->differential_0 = _S1295;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1306;
    (&_S1306)->primal_0 = _S1203.color_params_1.b_0;
    (&_S1306)->differential_0 = _S1180;
    s_bwd_prop_mul_2(&_S1305, &_S1306, _S1304);
    ColorPPISPParams_0 _S1307 = ColorPPISPParams_x24_syn_dzero_0();
    (&_S1307)->n_0 = _S1297.differential_0;
    (&_S1307)->g_0 = _S1300.differential_0;
    (&_S1307)->r_0 = _S1303.differential_0;
    (&_S1307)->b_0 = _S1306.differential_0;
    _S1200 = _S1257;
    *&((&_S1200)->z) = 0.0f;
    float _S1308 = rgb_out_7.z * _S1254;
    float _S1309 = _S1202 * _S1254;
    DiffPair_float_0 _S1310;
    (&_S1310)->primal_0 = falloff_8;
    (&_S1310)->differential_0 = 0.0f;
    DiffPair_float_0 _S1311;
    (&_S1311)->primal_0 = 0.0f;
    (&_S1311)->differential_0 = 0.0f;
    DiffPair_float_0 _S1312;
    (&_S1312)->primal_0 = 1.0f;
    (&_S1312)->differential_0 = 0.0f;
    s_bwd_prop_clamp_1(&_S1310, &_S1311, &_S1312, _S1308);
    float _S1313 = r2_23 * _S1310.differential_0;
    float _S1314 = r4_17 * _S1310.differential_0;
    float s_diff_r6_T_6 = _S1191 * _S1310.differential_0;
    float _S1315 = r6_8 * _S1310.differential_0;
    float _S1316 = r2_23 * (_S1190 * _S1310.differential_0 + r2_23 * s_diff_r6_T_6);
    float _S1317 = _S1189 * _S1310.differential_0 + r4_17 * s_diff_r6_T_6 + _S1316 + _S1316;
    float _S1318 = dy_17 * _S1317;
    float _S1319 = dx_17 * _S1317;
    float _S1320 = - (_S1318 + _S1318);
    float _S1321 = - (_S1319 + _S1319);
    *&((&_S1200)->y) = 0.0f;
    float _S1322 = rgb_out_7.y * _S1255;
    float _S1323 = _S1201 * _S1255;
    DiffPair_float_0 _S1324;
    (&_S1324)->primal_0 = falloff_7;
    (&_S1324)->differential_0 = 0.0f;
    DiffPair_float_0 _S1325;
    (&_S1325)->primal_0 = 0.0f;
    (&_S1325)->differential_0 = 0.0f;
    DiffPair_float_0 _S1326;
    (&_S1326)->primal_0 = 1.0f;
    (&_S1326)->differential_0 = 0.0f;
    s_bwd_prop_clamp_1(&_S1324, &_S1325, &_S1326, _S1322);
    float _S1327 = r2_22 * _S1324.differential_0;
    float _S1328 = r4_16 * _S1324.differential_0;
    float s_diff_r6_T_7 = _S1188 * _S1324.differential_0;
    float _S1329 = r6_7 * _S1324.differential_0;
    float _S1330 = r2_22 * (_S1187 * _S1324.differential_0 + r2_22 * s_diff_r6_T_7);
    float _S1331 = _S1186 * _S1324.differential_0 + r4_16 * s_diff_r6_T_7 + _S1330 + _S1330;
    float _S1332 = dy_16 * _S1331;
    float _S1333 = dx_16 * _S1331;
    float _S1334 = - (_S1332 + _S1332);
    float _S1335 = - (_S1333 + _S1333);
    *&((&_S1200)->x) = 0.0f;
    float _S1336 = rgb_out_7.x * _S1256;
    float _S1337 = _S1198 * _S1256;
    DiffPair_float_0 _S1338;
    (&_S1338)->primal_0 = falloff_6;
    (&_S1338)->differential_0 = 0.0f;
    DiffPair_float_0 _S1339;
    (&_S1339)->primal_0 = 0.0f;
    (&_S1339)->differential_0 = 0.0f;
    DiffPair_float_0 _S1340;
    (&_S1340)->primal_0 = 1.0f;
    (&_S1340)->differential_0 = 0.0f;
    s_bwd_prop_clamp_1(&_S1338, &_S1339, &_S1340, _S1336);
    float _S1341 = r2_21 * _S1338.differential_0;
    float _S1342 = r4_15 * _S1338.differential_0;
    float s_diff_r6_T_8 = _S1185 * _S1338.differential_0;
    float _S1343 = r6_6 * _S1338.differential_0;
    float _S1344 = r2_21 * (_S1184 * _S1338.differential_0 + r2_21 * s_diff_r6_T_8);
    float _S1345 = _S1183 * _S1338.differential_0 + r4_15 * s_diff_r6_T_8 + _S1344 + _S1344;
    float _S1346 = dy_15 * _S1345;
    float _S1347 = dx_15 * _S1345;
    float _S1348 = - (_S1346 + _S1346);
    float _S1349 = - (_S1347 + _S1347);
    float3  _S1350 = _S1176;
    *&((&_S1350)->z) = _S1309;
    *&((&_S1350)->y) = _S1323;
    *&((&_S1350)->x) = _S1337;
    float3  _S1351 = _S1200 + _S1350;
    float3  _S1352 = _S1175.primal_0 * _S1351;
    float3  _S1353 = _S1194 * _S1351;
    float _S1354 = _S1352.x + _S1352.y + _S1352.z;
    DiffPair_float_0 _S1355;
    (&_S1355)->primal_0 = _S1192.exposure_1;
    (&_S1355)->differential_0 = 0.0f;
    s_bwd_prop_exp2_0(&_S1355, _S1354);
    PPISPParamsNoCRF_0 _S1356 = PPISPParamsNoCRF_x24_syn_dzero_0();
    (&_S1356)->color_params_1 = _S1307;
    (&_S1356)->exposure_1 = _S1355.differential_0;
    _S1182 = _S1356;
    *&((&(&(&_S1182)->color_params_1)->n_0)->y) = 0.0f;
    *&((&(&(&_S1182)->color_params_1)->n_0)->x) = 0.0f;
    *&((&(&(&_S1182)->color_params_1)->g_0)->y) = 0.0f;
    *&((&(&(&_S1182)->color_params_1)->g_0)->x) = 0.0f;
    *&((&(&(&_S1182)->color_params_1)->r_0)->y) = 0.0f;
    *&((&(&(&_S1182)->color_params_1)->r_0)->x) = 0.0f;
    *&((&(&(&_S1182)->color_params_1)->b_0)->y) = 0.0f;
    *&((&(&(&_S1182)->color_params_1)->b_0)->x) = 0.0f;
    (&(&_S1182)->vignette_params_0[int(2)])->alpha2_0 = 0.0f;
    float _S1357 = _S1315 + _S1356.vignette_params_0[int(2)].alpha2_0;
    (&(&_S1182)->vignette_params_0[int(2)])->alpha1_0 = 0.0f;
    float _S1358 = _S1314 + _S1356.vignette_params_0[int(2)].alpha1_0;
    (&(&_S1182)->vignette_params_0[int(2)])->alpha0_0 = 0.0f;
    float _S1359 = _S1313 + _S1356.vignette_params_0[int(2)].alpha0_0;
    (&(&_S1182)->vignette_params_0[int(2)])->cy_0 = 0.0f;
    float _S1360 = _S1320 + _S1356.vignette_params_0[int(2)].cy_0;
    (&(&_S1182)->vignette_params_0[int(2)])->cx_0 = 0.0f;
    float _S1361 = _S1321 + _S1356.vignette_params_0[int(2)].cx_0;
    (&(&_S1182)->vignette_params_0[int(1)])->alpha2_0 = 0.0f;
    float _S1362 = _S1329 + _S1356.vignette_params_0[int(1)].alpha2_0;
    (&(&_S1182)->vignette_params_0[int(1)])->alpha1_0 = 0.0f;
    float _S1363 = _S1328 + _S1356.vignette_params_0[int(1)].alpha1_0;
    (&(&_S1182)->vignette_params_0[int(1)])->alpha0_0 = 0.0f;
    float _S1364 = _S1327 + _S1356.vignette_params_0[int(1)].alpha0_0;
    (&(&_S1182)->vignette_params_0[int(1)])->cy_0 = 0.0f;
    float _S1365 = _S1334 + _S1356.vignette_params_0[int(1)].cy_0;
    (&(&_S1182)->vignette_params_0[int(1)])->cx_0 = 0.0f;
    float _S1366 = _S1335 + _S1356.vignette_params_0[int(1)].cx_0;
    (&(&_S1182)->vignette_params_0[int(0)])->alpha2_0 = 0.0f;
    float _S1367 = _S1343 + _S1356.vignette_params_0[int(0)].alpha2_0;
    (&(&_S1182)->vignette_params_0[int(0)])->alpha1_0 = 0.0f;
    float _S1368 = _S1342 + _S1356.vignette_params_0[int(0)].alpha1_0;
    (&(&_S1182)->vignette_params_0[int(0)])->alpha0_0 = 0.0f;
    float _S1369 = _S1341 + _S1356.vignette_params_0[int(0)].alpha0_0;
    (&(&_S1182)->vignette_params_0[int(0)])->cy_0 = 0.0f;
    float _S1370 = _S1348 + _S1356.vignette_params_0[int(0)].cy_0;
    (&(&_S1182)->vignette_params_0[int(0)])->cx_0 = 0.0f;
    float _S1371 = _S1349 + _S1356.vignette_params_0[int(0)].cx_0;
    FixedArray<float, 24>  _S1372;
    _S1372[int(0)] = 0.0f;
    _S1372[int(1)] = 0.0f;
    _S1372[int(2)] = 0.0f;
    _S1372[int(3)] = 0.0f;
    _S1372[int(4)] = 0.0f;
    _S1372[int(5)] = 0.0f;
    _S1372[int(6)] = 0.0f;
    _S1372[int(7)] = 0.0f;
    _S1372[int(8)] = 0.0f;
    _S1372[int(9)] = 0.0f;
    _S1372[int(10)] = 0.0f;
    _S1372[int(11)] = 0.0f;
    _S1372[int(12)] = 0.0f;
    _S1372[int(13)] = 0.0f;
    _S1372[int(14)] = 0.0f;
    _S1372[int(15)] = 0.0f;
    _S1372[int(16)] = 0.0f;
    _S1372[int(17)] = 0.0f;
    _S1372[int(18)] = 0.0f;
    _S1372[int(19)] = 0.0f;
    _S1372[int(20)] = 0.0f;
    _S1372[int(21)] = 0.0f;
    _S1372[int(22)] = 0.0f;
    _S1372[int(23)] = 0.0f;
    _S1372[int(11)] = _S1361;
    _S1372[int(0)] = _S1182.exposure_1;
    _S1372[int(1)] = _S1371;
    _S1372[int(2)] = _S1370;
    _S1372[int(3)] = _S1369;
    _S1372[int(4)] = _S1368;
    _S1372[int(5)] = _S1367;
    _S1372[int(6)] = _S1366;
    _S1372[int(7)] = _S1365;
    _S1372[int(8)] = _S1364;
    _S1372[int(9)] = _S1363;
    _S1372[int(10)] = _S1362;
    _S1372[int(23)] = _S1356.color_params_1.n_0.y;
    _S1372[int(12)] = _S1360;
    _S1372[int(13)] = _S1359;
    _S1372[int(14)] = _S1358;
    _S1372[int(15)] = _S1357;
    _S1372[int(16)] = _S1356.color_params_1.b_0.x;
    _S1372[int(17)] = _S1356.color_params_1.b_0.y;
    _S1372[int(18)] = _S1356.color_params_1.r_0.x;
    _S1372[int(19)] = _S1356.color_params_1.r_0.y;
    _S1372[int(20)] = _S1356.color_params_1.g_0.x;
    _S1372[int(21)] = _S1356.color_params_1.g_0.y;
    _S1372[int(22)] = _S1356.color_params_1.n_0.x;
    dpparams_2->primal_0 = dpparams_2->primal_0;
    dpparams_2->differential_0 = _S1372;
    dprgb_in_2->primal_0 = (*dprgb_in_2).primal_0;
    dprgb_in_2->differential_0 = _S1353;
    return;
}

inline __device__ void s_bwd_apply_ppisp_no_crf_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1373, float2  _S1374, float2  _S1375, float2  _S1376, DiffPair_arrayx3Cfloatx2C24x3E_0 * _S1377, bool _S1378, float3  _S1379)
{
    s_bwd_prop_apply_ppisp_no_crf_0(_S1373, _S1374, _S1375, _S1376, _S1377, _S1378, _S1379);
    return;
}

inline __device__ void apply_ppisp_no_crf_vjp(float3  rgb_in_6, float2  pix_coord_9, float2  image_center_9, float2  img_size_9, FixedArray<float, 24>  params_6, bool clamp_output_3, float3  grad_out_2, float3  * grad_rgb_in_2, FixedArray<float, 24>  * grad_params_2)
{
    float3  _S1380 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_rgb_in_2;
    (&dp_rgb_in_2)->primal_0 = rgb_in_6;
    (&dp_rgb_in_2)->differential_0 = _S1380;
    FixedArray<float, 24>  _S1381 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    DiffPair_arrayx3Cfloatx2C24x3E_0 dp_params_2;
    (&dp_params_2)->primal_0 = params_6;
    (&dp_params_2)->differential_0 = _S1381;
    s_bwd_apply_ppisp_no_crf_0(&dp_rgb_in_2, pix_coord_9, image_center_9, img_size_9, &dp_params_2, clamp_output_3, grad_out_2);
    *grad_rgb_in_2 = dp_rgb_in_2.differential_0;
    *grad_params_2 = (&dp_params_2)->differential_0;
    return;
}

struct DiffPair_arrayx3Cfloatx2C9x3E_0
{
    FixedArray<float, 9>  primal_0;
    FixedArray<float, 9>  differential_0;
};

inline __device__ void s_bwd_prop_apply_ppisp_no_crf_no_vig_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dprgb_in_3, float2  pix_coord_10, float2  image_center_10, float2  img_size_10, DiffPair_arrayx3Cfloatx2C9x3E_0 * dpparams_3, bool clamp_output_4, float3  _s_dOut_3)
{
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1382 = *dprgb_in_3;
    float3  _S1383 = make_float3 (0.0f);
    Matrix<float, 3, 3>  _S1384 = makeMatrix<float, 3, 3> (0.0f);
    float2  _S1385 = make_float2 (0.0f);
    ColorPPISPParams_0 _S1386 = { _S1385, _S1385, _S1385, _S1385 };
    PPISPParamsNoCRFNoVig_0 _S1387;
    (&_S1387)->exposure_0 = dpparams_3->primal_0[int(0)];
    (&_S1387)->color_params_0 = _S1386;
    *&((&(&(&_S1387)->color_params_0)->b_0)->x) = dpparams_3->primal_0[int(1)];
    *&((&(&(&_S1387)->color_params_0)->b_0)->y) = dpparams_3->primal_0[int(2)];
    *&((&(&(&_S1387)->color_params_0)->r_0)->x) = dpparams_3->primal_0[int(3)];
    *&((&(&(&_S1387)->color_params_0)->r_0)->y) = dpparams_3->primal_0[int(4)];
    *&((&(&(&_S1387)->color_params_0)->g_0)->x) = dpparams_3->primal_0[int(5)];
    *&((&(&(&_S1387)->color_params_0)->g_0)->y) = dpparams_3->primal_0[int(6)];
    *&((&(&(&_S1387)->color_params_0)->n_0)->x) = dpparams_3->primal_0[int(7)];
    *&((&(&(&_S1387)->color_params_0)->n_0)->y) = dpparams_3->primal_0[int(8)];
    PPISPParamsNoCRFNoVig_0 _S1388 = _S1387;
    float _S1389 = s_primal_ctx_exp2_0(_S1387.exposure_0);
    float3  _S1390 = make_float3 (_S1389);
    float3  _S1391 = (*dprgb_in_3).primal_0 * make_float3 (_S1389);
    PPISPParamsNoCRFNoVig_0 _S1392 = _S1387;
    float2  _S1393 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f), _S1387.color_params_0.b_0);
    float2  _S1394 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f), _S1387.color_params_0.r_0);
    float2  _S1395 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f), _S1387.color_params_0.g_0);
    float2  _S1396 = s_primal_ctx_mul_0(makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f), _S1387.color_params_0.n_0);
    float _S1397 = 0.3333333432674408f + _S1396.x;
    float _S1398 = 0.3333333432674408f + _S1396.y;
    Matrix<float, 3, 3>  T_7 = makeMatrix<float, 3, 3> (_S1393.x, 1.0f + _S1394.x, _S1395.x, _S1393.y, _S1394.y, 1.0f + _S1395.y, 1.0f, 1.0f, 1.0f);
    Matrix<float, 3, 3>  skew_3 = makeMatrix<float, 3, 3> (0.0f, -1.0f, _S1398, 1.0f, 0.0f, - _S1397, - _S1398, _S1397, 0.0f);
    Matrix<float, 3, 3>  _S1399 = s_primal_ctx_mul_1(skew_3, T_7);
    float3  r0_7 = make_float3 (_S1399.rows[int(0)].x, _S1399.rows[int(0)].y, _S1399.rows[int(0)].z);
    float3  r1_7 = make_float3 (_S1399.rows[int(1)].x, _S1399.rows[int(1)].y, _S1399.rows[int(1)].z);
    float3  r2_25 = make_float3 (_S1399.rows[int(2)].x, _S1399.rows[int(2)].y, _S1399.rows[int(2)].z);
    float3  _S1400 = s_primal_ctx_cross_0(r0_7, r1_7);
    bool _S1401 = (s_primal_ctx_dot_0(_S1400, _S1400)) < 9.99999968265522539e-21f;
    float3  lambda_v_15;
    float3  _S1402;
    bool _S1403;
    if(_S1401)
    {
        float3  _S1404 = s_primal_ctx_cross_0(r0_7, r2_25);
        bool _S1405 = (s_primal_ctx_dot_0(_S1404, _S1404)) < 9.99999968265522539e-21f;
        if(_S1405)
        {
            lambda_v_15 = s_primal_ctx_cross_0(r1_7, r2_25);
        }
        else
        {
            lambda_v_15 = _S1404;
        }
        _S1403 = _S1405;
        _S1402 = _S1404;
    }
    else
    {
        lambda_v_15 = _S1400;
        _S1403 = false;
        _S1402 = _S1383;
    }
    Matrix<float, 3, 3>  S_inv_3 = makeMatrix<float, 3, 3> (-1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    Matrix<float, 3, 3>  D_3 = makeMatrix<float, 3, 3> (lambda_v_15.x, 0.0f, 0.0f, 0.0f, lambda_v_15.y, 0.0f, 0.0f, 0.0f, lambda_v_15.z);
    Matrix<float, 3, 3>  _S1406 = s_primal_ctx_mul_1(T_7, D_3);
    Matrix<float, 3, 3>  _S1407 = s_primal_ctx_mul_1(_S1406, S_inv_3);
    bool _S1408 = (s_primal_ctx_abs_0(_S1407.rows[int(2)].z)) > 9.99999968265522539e-21f;
    Matrix<float, 3, 3>  H_11;
    Matrix<float, 3, 3>  _S1409;
    float _S1410;
    if(_S1408)
    {
        float inv_s_3 = 1.0f / _S1407.rows[int(2)].z;
        Matrix<float, 3, 3>  _S1411 = makeMatrix<float, 3, 3> (inv_s_3);
        float _S1412 = _S1407.rows[int(2)].z * _S1407.rows[int(2)].z;
        H_11 = _S1407 * makeMatrix<float, 3, 3> (inv_s_3);
        _S1409 = _S1411;
        _S1410 = _S1412;
    }
    else
    {
        H_11 = _S1407;
        _S1409 = _S1384;
        _S1410 = 0.0f;
    }
    float _S1413 = _S1391.x;
    float _S1414 = _S1391.y;
    float intensity_7 = _S1413 + _S1414 + _S1391.z;
    float3  rgi_in_3 = make_float3 (_S1413, _S1414, intensity_7);
    float3  _S1415 = s_primal_ctx_mul_2(H_11, rgi_in_3);
    float _S1416 = _S1415.z;
    float _S1417 = 0.00009999999747379f * s_primal_ctx_abs_0(intensity_7) + 9.99999993922529029e-09f;
    float _S1418 = (F32_max((_S1416), (_S1417)));
    float norm_factor_7 = intensity_7 / _S1418;
    float _S1419 = _S1418 * _S1418;
    float _S1420 = _S1415.x;
    float out_r_7 = _S1420 * norm_factor_7;
    float _S1421 = _S1415.y;
    float out_g_7 = _S1421 * norm_factor_7;
    float3  _S1422 = make_float3 (out_r_7, out_g_7, intensity_7 - out_r_7 - out_g_7);
    float3  _S1423;
    if(clamp_output_4)
    {
        float3  _S1424 = make_float3 (1.0f);
        lambda_v_15 = make_float3 (0.0f);
        _S1423 = _S1424;
    }
    else
    {
        lambda_v_15 = _S1383;
        _S1423 = _S1383;
    }
    if(clamp_output_4)
    {
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1425;
        (&_S1425)->primal_0 = _S1422;
        (&_S1425)->differential_0 = _S1383;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1426;
        (&_S1426)->primal_0 = lambda_v_15;
        (&_S1426)->differential_0 = _S1383;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1427;
        (&_S1427)->primal_0 = _S1423;
        (&_S1427)->differential_0 = _S1383;
        s_bwd_prop_clamp_0(&_S1425, &_S1426, &_S1427, _s_dOut_3);
        lambda_v_15 = _S1425.differential_0;
    }
    else
    {
        lambda_v_15 = _s_dOut_3;
    }
    float _S1428 = - lambda_v_15.z;
    float _S1429 = lambda_v_15.y + _S1428;
    float _S1430 = norm_factor_7 * _S1429;
    float _S1431 = lambda_v_15.x + _S1428;
    float _S1432 = norm_factor_7 * _S1431;
    float _S1433 = (_S1421 * _S1429 + _S1420 * _S1431) / _S1419;
    float _S1434 = intensity_7 * - _S1433;
    float _S1435 = _S1418 * _S1433;
    DiffPair_float_0 _S1436;
    (&_S1436)->primal_0 = _S1416;
    (&_S1436)->differential_0 = 0.0f;
    DiffPair_float_0 _S1437;
    (&_S1437)->primal_0 = _S1417;
    (&_S1437)->differential_0 = 0.0f;
    _d_max_0(&_S1436, &_S1437, _S1434);
    float _S1438 = 0.00009999999747379f * _S1437.differential_0;
    DiffPair_float_0 _S1439;
    (&_S1439)->primal_0 = intensity_7;
    (&_S1439)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S1439, _S1438);
    float3  _S1440 = make_float3 (_S1432, _S1430, _S1436.differential_0);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1441;
    (&_S1441)->primal_0 = H_11;
    (&_S1441)->differential_0 = _S1384;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1442;
    (&_S1442)->primal_0 = rgi_in_3;
    (&_S1442)->differential_0 = _S1383;
    s_bwd_prop_mul_0(&_S1441, &_S1442, _S1440);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1443 = _S1441;
    float _S1444 = lambda_v_15.z + _S1435 + _S1439.differential_0 + _S1442.differential_0.z;
    float3  _S1445 = make_float3 (_S1442.differential_0.x + _S1444, _S1442.differential_0.y + _S1444, _S1444);
    if(_S1408)
    {
        Matrix<float, 3, 3>  _S1446 = _S1407 * _S1443.differential_0;
        Matrix<float, 3, 3>  _S1447 = _S1409 * _S1443.differential_0;
        _S1410 = - ((_S1446.rows[int(0)].x + _S1446.rows[int(0)].y + _S1446.rows[int(0)].z + _S1446.rows[int(1)].x + _S1446.rows[int(1)].y + _S1446.rows[int(1)].z + _S1446.rows[int(2)].x + _S1446.rows[int(2)].y + _S1446.rows[int(2)].z) / _S1410);
        H_11 = _S1447;
    }
    else
    {
        _S1410 = 0.0f;
        H_11 = _S1443.differential_0;
    }
    DiffPair_float_0 _S1448;
    (&_S1448)->primal_0 = _S1407.rows[int(2)].z;
    (&_S1448)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S1448, 0.0f);
    float _S1449 = _S1448.differential_0 + _S1410;
    float3  _S1450 = _S1383;
    *&((&_S1450)->z) = _S1449;
    Matrix<float, 3, 3>  _S1451 = _S1384;
    _S1451[int(2)] = _S1450;
    Matrix<float, 3, 3>  _S1452 = H_11 + _S1451;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1453;
    (&_S1453)->primal_0 = _S1406;
    (&_S1453)->differential_0 = _S1384;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1454;
    (&_S1454)->primal_0 = S_inv_3;
    (&_S1454)->differential_0 = _S1384;
    s_bwd_prop_mul_1(&_S1453, &_S1454, _S1452);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1455;
    (&_S1455)->primal_0 = T_7;
    (&_S1455)->differential_0 = _S1384;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1456;
    (&_S1456)->primal_0 = D_3;
    (&_S1456)->differential_0 = _S1384;
    s_bwd_prop_mul_1(&_S1455, &_S1456, _S1453.differential_0);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1457 = _S1455;
    float3  _S1458 = make_float3 (_S1456.differential_0.rows[int(0)].x, _S1456.differential_0.rows[int(1)].y, _S1456.differential_0.rows[int(2)].z);
    float3  _S1459;
    if(_S1401)
    {
        if(_S1403)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1460;
            (&_S1460)->primal_0 = r1_7;
            (&_S1460)->differential_0 = _S1383;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1461;
            (&_S1461)->primal_0 = r2_25;
            (&_S1461)->differential_0 = _S1383;
            s_bwd_prop_cross_0(&_S1460, &_S1461, _S1458);
            lambda_v_15 = _S1383;
            _S1423 = _S1461.differential_0;
            _S1459 = _S1460.differential_0;
        }
        else
        {
            lambda_v_15 = _S1458;
            _S1423 = _S1383;
            _S1459 = _S1383;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1462;
        (&_S1462)->primal_0 = _S1402;
        (&_S1462)->differential_0 = _S1383;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1463;
        (&_S1463)->primal_0 = _S1402;
        (&_S1463)->differential_0 = _S1383;
        s_bwd_prop_dot_0(&_S1462, &_S1463, 0.0f);
        float3  _S1464 = _S1463.differential_0 + _S1462.differential_0 + lambda_v_15;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1465;
        (&_S1465)->primal_0 = r0_7;
        (&_S1465)->differential_0 = _S1383;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1466;
        (&_S1466)->primal_0 = r2_25;
        (&_S1466)->differential_0 = _S1383;
        s_bwd_prop_cross_0(&_S1465, &_S1466, _S1464);
        float3  _S1467 = _S1466.differential_0 + _S1423;
        lambda_v_15 = _S1383;
        _S1402 = _S1467;
        _S1423 = _S1459;
        _S1459 = _S1465.differential_0;
    }
    else
    {
        lambda_v_15 = _S1458;
        _S1402 = _S1383;
        _S1423 = _S1383;
        _S1459 = _S1383;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1468;
    (&_S1468)->primal_0 = _S1400;
    (&_S1468)->differential_0 = _S1383;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1469;
    (&_S1469)->primal_0 = _S1400;
    (&_S1469)->differential_0 = _S1383;
    s_bwd_prop_dot_0(&_S1468, &_S1469, 0.0f);
    float3  _S1470 = _S1469.differential_0 + _S1468.differential_0 + lambda_v_15;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1471;
    (&_S1471)->primal_0 = r0_7;
    (&_S1471)->differential_0 = _S1383;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1472;
    (&_S1472)->primal_0 = r1_7;
    (&_S1472)->differential_0 = _S1383;
    s_bwd_prop_cross_0(&_S1471, &_S1472, _S1470);
    float3  _S1473 = _S1383;
    *&((&_S1473)->z) = _S1402.z;
    *&((&_S1473)->y) = _S1402.y;
    *&((&_S1473)->x) = _S1402.x;
    float3  _S1474 = _S1472.differential_0 + _S1423;
    float3  _S1475 = _S1383;
    *&((&_S1475)->z) = _S1474.z;
    *&((&_S1475)->y) = _S1474.y;
    *&((&_S1475)->x) = _S1474.x;
    float3  _S1476 = _S1471.differential_0 + _S1459;
    float3  _S1477 = _S1383;
    *&((&_S1477)->z) = _S1476.z;
    *&((&_S1477)->y) = _S1476.y;
    *&((&_S1477)->x) = _S1476.x;
    Matrix<float, 3, 3>  _S1478 = _S1384;
    _S1478[int(2)] = _S1473;
    _S1478[int(1)] = _S1475;
    _S1478[int(0)] = _S1477;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1479;
    (&_S1479)->primal_0 = skew_3;
    (&_S1479)->differential_0 = _S1384;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1480;
    (&_S1480)->primal_0 = T_7;
    (&_S1480)->differential_0 = _S1384;
    s_bwd_prop_mul_1(&_S1479, &_S1480, _S1478);
    Matrix<float, 3, 3>  _S1481 = _S1480.differential_0 + _S1457.differential_0;
    float2  _S1482 = make_float2 (_S1479.differential_0.rows[int(2)].y + - _S1479.differential_0.rows[int(1)].z, _S1479.differential_0.rows[int(0)].z + - _S1479.differential_0.rows[int(2)].x);
    Matrix<float, 2, 2>  _S1483 = makeMatrix<float, 2, 2> (0.0f);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1484;
    (&_S1484)->primal_0 = makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f);
    (&_S1484)->differential_0 = _S1483;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1485;
    (&_S1485)->primal_0 = _S1392.color_params_0.n_0;
    (&_S1485)->differential_0 = _S1385;
    s_bwd_prop_mul_2(&_S1484, &_S1485, _S1482);
    float2  _S1486 = make_float2 (_S1481.rows[int(0)].z, _S1481.rows[int(1)].z);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1487;
    (&_S1487)->primal_0 = makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f);
    (&_S1487)->differential_0 = _S1483;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1488;
    (&_S1488)->primal_0 = _S1392.color_params_0.g_0;
    (&_S1488)->differential_0 = _S1385;
    s_bwd_prop_mul_2(&_S1487, &_S1488, _S1486);
    float2  _S1489 = make_float2 (_S1481.rows[int(0)].y, _S1481.rows[int(1)].y);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1490;
    (&_S1490)->primal_0 = makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f);
    (&_S1490)->differential_0 = _S1483;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1491;
    (&_S1491)->primal_0 = _S1392.color_params_0.r_0;
    (&_S1491)->differential_0 = _S1385;
    s_bwd_prop_mul_2(&_S1490, &_S1491, _S1489);
    float2  _S1492 = make_float2 (_S1481.rows[int(0)].x, _S1481.rows[int(1)].x);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1493;
    (&_S1493)->primal_0 = makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f);
    (&_S1493)->differential_0 = _S1483;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1494;
    (&_S1494)->primal_0 = _S1392.color_params_0.b_0;
    (&_S1494)->differential_0 = _S1385;
    s_bwd_prop_mul_2(&_S1493, &_S1494, _S1492);
    ColorPPISPParams_0 _S1495 = ColorPPISPParams_x24_syn_dzero_0();
    (&_S1495)->n_0 = _S1485.differential_0;
    (&_S1495)->g_0 = _S1488.differential_0;
    (&_S1495)->r_0 = _S1491.differential_0;
    (&_S1495)->b_0 = _S1494.differential_0;
    float3  _S1496 = _S1382.primal_0 * _S1445;
    float3  _S1497 = _S1390 * _S1445;
    float _S1498 = _S1496.x + _S1496.y + _S1496.z;
    DiffPair_float_0 _S1499;
    (&_S1499)->primal_0 = _S1388.exposure_0;
    (&_S1499)->differential_0 = 0.0f;
    s_bwd_prop_exp2_0(&_S1499, _S1498);
    PPISPParamsNoCRFNoVig_0 _S1500 = PPISPParamsNoCRFNoVig_x24_syn_dzero_0();
    (&_S1500)->color_params_0 = _S1495;
    (&_S1500)->exposure_0 = _S1499.differential_0;
    _S1387 = _S1500;
    *&((&(&(&_S1387)->color_params_0)->n_0)->y) = 0.0f;
    *&((&(&(&_S1387)->color_params_0)->n_0)->x) = 0.0f;
    *&((&(&(&_S1387)->color_params_0)->g_0)->y) = 0.0f;
    *&((&(&(&_S1387)->color_params_0)->g_0)->x) = 0.0f;
    *&((&(&(&_S1387)->color_params_0)->r_0)->y) = 0.0f;
    *&((&(&(&_S1387)->color_params_0)->r_0)->x) = 0.0f;
    *&((&(&(&_S1387)->color_params_0)->b_0)->y) = 0.0f;
    *&((&(&(&_S1387)->color_params_0)->b_0)->x) = 0.0f;
    FixedArray<float, 9>  _S1501;
    _S1501[int(0)] = 0.0f;
    _S1501[int(1)] = 0.0f;
    _S1501[int(2)] = 0.0f;
    _S1501[int(3)] = 0.0f;
    _S1501[int(4)] = 0.0f;
    _S1501[int(5)] = 0.0f;
    _S1501[int(6)] = 0.0f;
    _S1501[int(7)] = 0.0f;
    _S1501[int(8)] = 0.0f;
    _S1501[int(8)] = _S1500.color_params_0.n_0.y;
    _S1501[int(7)] = _S1500.color_params_0.n_0.x;
    _S1501[int(6)] = _S1500.color_params_0.g_0.y;
    _S1501[int(5)] = _S1500.color_params_0.g_0.x;
    _S1501[int(4)] = _S1500.color_params_0.r_0.y;
    _S1501[int(3)] = _S1500.color_params_0.r_0.x;
    _S1501[int(2)] = _S1500.color_params_0.b_0.y;
    _S1501[int(1)] = _S1500.color_params_0.b_0.x;
    _S1501[int(0)] = _S1387.exposure_0;
    dpparams_3->primal_0 = dpparams_3->primal_0;
    dpparams_3->differential_0 = _S1501;
    dprgb_in_3->primal_0 = (*dprgb_in_3).primal_0;
    dprgb_in_3->differential_0 = _S1497;
    return;
}

inline __device__ void s_bwd_apply_ppisp_no_crf_no_vig_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1502, float2  _S1503, float2  _S1504, float2  _S1505, DiffPair_arrayx3Cfloatx2C9x3E_0 * _S1506, bool _S1507, float3  _S1508)
{
    s_bwd_prop_apply_ppisp_no_crf_no_vig_0(_S1502, _S1503, _S1504, _S1505, _S1506, _S1507, _S1508);
    return;
}

inline __device__ void apply_ppisp_no_crf_no_vig_vjp(float3  rgb_in_7, float2  pix_coord_11, float2  image_center_11, float2  img_size_11, FixedArray<float, 9>  params_7, bool clamp_output_5, float3  grad_out_3, float3  * grad_rgb_in_3, FixedArray<float, 9>  * grad_params_3)
{
    float3  _S1509 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_rgb_in_3;
    (&dp_rgb_in_3)->primal_0 = rgb_in_7;
    (&dp_rgb_in_3)->differential_0 = _S1509;
    FixedArray<float, 9>  _S1510 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    DiffPair_arrayx3Cfloatx2C9x3E_0 dp_params_3;
    (&dp_params_3)->primal_0 = params_7;
    (&dp_params_3)->differential_0 = _S1510;
    s_bwd_apply_ppisp_no_crf_no_vig_0(&dp_rgb_in_3, pix_coord_11, image_center_11, img_size_11, &dp_params_3, clamp_output_5, grad_out_3);
    *grad_rgb_in_3 = dp_rgb_in_3.differential_0;
    *grad_params_3 = (&dp_params_3)->differential_0;
    return;
}

inline __device__ void compute_raw_ppisp_regularization_loss(FixedArray<float, 36>  params_8, FixedArray<float, 22>  * _S1511)
{
    PPISPParams_0 p_4;
    (&p_4)->exposure_3 = params_8[int(0)];
    (&(&p_4)->vignette_params_2[int(0)])->cx_0 = params_8[int(1)];
    (&(&p_4)->vignette_params_2[int(0)])->cy_0 = params_8[int(2)];
    (&(&p_4)->vignette_params_2[int(0)])->alpha0_0 = params_8[int(3)];
    (&(&p_4)->vignette_params_2[int(0)])->alpha1_0 = params_8[int(4)];
    (&(&p_4)->vignette_params_2[int(0)])->alpha2_0 = params_8[int(5)];
    (&(&p_4)->vignette_params_2[int(1)])->cx_0 = params_8[int(6)];
    (&(&p_4)->vignette_params_2[int(1)])->cy_0 = params_8[int(7)];
    (&(&p_4)->vignette_params_2[int(1)])->alpha0_0 = params_8[int(8)];
    (&(&p_4)->vignette_params_2[int(1)])->alpha1_0 = params_8[int(9)];
    (&(&p_4)->vignette_params_2[int(1)])->alpha2_0 = params_8[int(10)];
    (&(&p_4)->vignette_params_2[int(2)])->cx_0 = params_8[int(11)];
    (&(&p_4)->vignette_params_2[int(2)])->cy_0 = params_8[int(12)];
    (&(&p_4)->vignette_params_2[int(2)])->alpha0_0 = params_8[int(13)];
    (&(&p_4)->vignette_params_2[int(2)])->alpha1_0 = params_8[int(14)];
    (&(&p_4)->vignette_params_2[int(2)])->alpha2_0 = params_8[int(15)];
    *&((&(&(&p_4)->color_params_3)->b_0)->x) = params_8[int(16)];
    *&((&(&(&p_4)->color_params_3)->b_0)->y) = params_8[int(17)];
    *&((&(&(&p_4)->color_params_3)->r_0)->x) = params_8[int(18)];
    *&((&(&(&p_4)->color_params_3)->r_0)->y) = params_8[int(19)];
    *&((&(&(&p_4)->color_params_3)->g_0)->x) = params_8[int(20)];
    *&((&(&(&p_4)->color_params_3)->g_0)->y) = params_8[int(21)];
    *&((&(&(&p_4)->color_params_3)->n_0)->x) = params_8[int(22)];
    *&((&(&(&p_4)->color_params_3)->n_0)->y) = params_8[int(23)];
    (&(&p_4)->crf_params_1[int(0)])->toe_0 = params_8[int(24)];
    (&(&p_4)->crf_params_1[int(0)])->shoulder_0 = params_8[int(25)];
    (&(&p_4)->crf_params_1[int(0)])->gamma_0 = params_8[int(26)];
    (&(&p_4)->crf_params_1[int(0)])->center_0 = params_8[int(27)];
    (&(&p_4)->crf_params_1[int(1)])->toe_0 = params_8[int(28)];
    (&(&p_4)->crf_params_1[int(1)])->shoulder_0 = params_8[int(29)];
    (&(&p_4)->crf_params_1[int(1)])->gamma_0 = params_8[int(30)];
    (&(&p_4)->crf_params_1[int(1)])->center_0 = params_8[int(31)];
    (&(&p_4)->crf_params_1[int(2)])->toe_0 = params_8[int(32)];
    (&(&p_4)->crf_params_1[int(2)])->shoulder_0 = params_8[int(33)];
    (&(&p_4)->crf_params_1[int(2)])->gamma_0 = params_8[int(34)];
    (&(&p_4)->crf_params_1[int(2)])->center_0 = params_8[int(35)];
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
    losses_0[int(0)] = p_4.exposure_3;
    float _S1512 = p_4.vignette_params_2[int(0)].cx_0;
    float _S1513 = p_4.vignette_params_2[int(0)].cy_0;
    float _S1514 = p_4.vignette_params_2[int(1)].cx_0;
    float _S1515 = p_4.vignette_params_2[int(1)].cy_0;
    float _S1516 = p_4.vignette_params_2[int(2)].cx_0;
    float _S1517 = p_4.vignette_params_2[int(2)].cy_0;
    losses_0[int(1)] = _S1512 * _S1512 + _S1513 * _S1513 + _S1514 * _S1514 + _S1515 * _S1515 + _S1516 * _S1516 + _S1517 * _S1517;
    losses_0[int(2)] = (F32_max((0.0f), (p_4.vignette_params_2[int(0)].alpha0_0))) + (F32_max((0.0f), (p_4.vignette_params_2[int(1)].alpha0_0))) + (F32_max((0.0f), (p_4.vignette_params_2[int(2)].alpha0_0)));
    losses_0[int(3)] = (F32_max((0.0f), (p_4.vignette_params_2[int(0)].alpha1_0))) + (F32_max((0.0f), (p_4.vignette_params_2[int(1)].alpha1_0))) + (F32_max((0.0f), (p_4.vignette_params_2[int(2)].alpha1_0)));
    losses_0[int(4)] = (F32_max((0.0f), (p_4.vignette_params_2[int(0)].alpha2_0))) + (F32_max((0.0f), (p_4.vignette_params_2[int(1)].alpha2_0))) + (F32_max((0.0f), (p_4.vignette_params_2[int(2)].alpha2_0)));
    float mean_0 = (p_4.vignette_params_2[int(0)].cx_0 + p_4.vignette_params_2[int(1)].cx_0 + p_4.vignette_params_2[int(2)].cx_0) / 3.0f;
    float _S1518 = p_4.vignette_params_2[int(0)].cx_0 - mean_0;
    float _S1519 = p_4.vignette_params_2[int(1)].cx_0 - mean_0;
    float _S1520 = p_4.vignette_params_2[int(2)].cx_0 - mean_0;
    losses_0[int(5)] = (_S1518 * _S1518 + _S1519 * _S1519 + _S1520 * _S1520) / 3.0f;
    float mean_1 = (p_4.vignette_params_2[int(0)].cy_0 + p_4.vignette_params_2[int(1)].cy_0 + p_4.vignette_params_2[int(2)].cy_0) / 3.0f;
    float _S1521 = p_4.vignette_params_2[int(0)].cy_0 - mean_1;
    float _S1522 = p_4.vignette_params_2[int(1)].cy_0 - mean_1;
    float _S1523 = p_4.vignette_params_2[int(2)].cy_0 - mean_1;
    losses_0[int(6)] = (_S1521 * _S1521 + _S1522 * _S1522 + _S1523 * _S1523) / 3.0f;
    float mean_2 = (p_4.vignette_params_2[int(0)].alpha0_0 + p_4.vignette_params_2[int(1)].alpha0_0 + p_4.vignette_params_2[int(2)].alpha0_0) / 3.0f;
    float _S1524 = p_4.vignette_params_2[int(0)].alpha0_0 - mean_2;
    float _S1525 = p_4.vignette_params_2[int(1)].alpha0_0 - mean_2;
    float _S1526 = p_4.vignette_params_2[int(2)].alpha0_0 - mean_2;
    losses_0[int(7)] = (_S1524 * _S1524 + _S1525 * _S1525 + _S1526 * _S1526) / 3.0f;
    float mean_3 = (p_4.vignette_params_2[int(0)].alpha1_0 + p_4.vignette_params_2[int(1)].alpha1_0 + p_4.vignette_params_2[int(2)].alpha1_0) / 3.0f;
    float _S1527 = p_4.vignette_params_2[int(0)].alpha1_0 - mean_3;
    float _S1528 = p_4.vignette_params_2[int(1)].alpha1_0 - mean_3;
    float _S1529 = p_4.vignette_params_2[int(2)].alpha1_0 - mean_3;
    losses_0[int(8)] = (_S1527 * _S1527 + _S1528 * _S1528 + _S1529 * _S1529) / 3.0f;
    float mean_4 = (p_4.vignette_params_2[int(0)].alpha2_0 + p_4.vignette_params_2[int(1)].alpha2_0 + p_4.vignette_params_2[int(2)].alpha2_0) / 3.0f;
    float _S1530 = p_4.vignette_params_2[int(0)].alpha2_0 - mean_4;
    float _S1531 = p_4.vignette_params_2[int(1)].alpha2_0 - mean_4;
    float _S1532 = p_4.vignette_params_2[int(2)].alpha2_0 - mean_4;
    losses_0[int(9)] = (_S1530 * _S1530 + _S1531 * _S1531 + _S1532 * _S1532) / 3.0f;
    float2  bd_4 = mul_0(makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f), p_4.color_params_3.b_0);
    float2  rd_4 = mul_0(makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f), p_4.color_params_3.r_0);
    float2  gd_4 = mul_0(makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f), p_4.color_params_3.g_0);
    float2  nd_4 = mul_0(makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f), p_4.color_params_3.n_0);
    losses_0[int(10)] = bd_4.x;
    losses_0[int(11)] = bd_4.y;
    losses_0[int(12)] = rd_4.x;
    losses_0[int(13)] = rd_4.y;
    losses_0[int(14)] = gd_4.x;
    losses_0[int(15)] = gd_4.y;
    losses_0[int(16)] = nd_4.x;
    losses_0[int(17)] = nd_4.y;
    float mean_5 = (p_4.crf_params_1[int(0)].toe_0 + p_4.crf_params_1[int(1)].toe_0 + p_4.crf_params_1[int(2)].toe_0) / 3.0f;
    float _S1533 = p_4.crf_params_1[int(0)].toe_0 - mean_5;
    float _S1534 = p_4.crf_params_1[int(1)].toe_0 - mean_5;
    float _S1535 = p_4.crf_params_1[int(2)].toe_0 - mean_5;
    losses_0[int(18)] = (_S1533 * _S1533 + _S1534 * _S1534 + _S1535 * _S1535) / 3.0f;
    float mean_6 = (p_4.crf_params_1[int(0)].shoulder_0 + p_4.crf_params_1[int(1)].shoulder_0 + p_4.crf_params_1[int(2)].shoulder_0) / 3.0f;
    float _S1536 = p_4.crf_params_1[int(0)].shoulder_0 - mean_6;
    float _S1537 = p_4.crf_params_1[int(1)].shoulder_0 - mean_6;
    float _S1538 = p_4.crf_params_1[int(2)].shoulder_0 - mean_6;
    losses_0[int(19)] = (_S1536 * _S1536 + _S1537 * _S1537 + _S1538 * _S1538) / 3.0f;
    float mean_7 = (p_4.crf_params_1[int(0)].gamma_0 + p_4.crf_params_1[int(1)].gamma_0 + p_4.crf_params_1[int(2)].gamma_0) / 3.0f;
    float _S1539 = p_4.crf_params_1[int(0)].gamma_0 - mean_7;
    float _S1540 = p_4.crf_params_1[int(1)].gamma_0 - mean_7;
    float _S1541 = p_4.crf_params_1[int(2)].gamma_0 - mean_7;
    losses_0[int(20)] = (_S1539 * _S1539 + _S1540 * _S1540 + _S1541 * _S1541) / 3.0f;
    float mean_8 = (p_4.crf_params_1[int(0)].center_0 + p_4.crf_params_1[int(1)].center_0 + p_4.crf_params_1[int(2)].center_0) / 3.0f;
    float _S1542 = p_4.crf_params_1[int(0)].center_0 - mean_8;
    float _S1543 = p_4.crf_params_1[int(1)].center_0 - mean_8;
    float _S1544 = p_4.crf_params_1[int(2)].center_0 - mean_8;
    losses_0[int(21)] = (_S1542 * _S1542 + _S1543 * _S1543 + _S1544 * _S1544) / 3.0f;
    *_S1511 = losses_0;
    return;
}

inline __device__ void compute_raw_ppisp_rqs_regularization_loss(FixedArray<float, 39>  params_9, FixedArray<float, 23>  * _S1545)
{
    PPISPParamsRQS_0 p_5;
    (&p_5)->exposure_2 = params_9[int(0)];
    (&(&p_5)->vignette_params_1[int(0)])->cx_0 = params_9[int(1)];
    (&(&p_5)->vignette_params_1[int(0)])->cy_0 = params_9[int(2)];
    (&(&p_5)->vignette_params_1[int(0)])->alpha0_0 = params_9[int(3)];
    (&(&p_5)->vignette_params_1[int(0)])->alpha1_0 = params_9[int(4)];
    (&(&p_5)->vignette_params_1[int(0)])->alpha2_0 = params_9[int(5)];
    (&(&p_5)->vignette_params_1[int(1)])->cx_0 = params_9[int(6)];
    (&(&p_5)->vignette_params_1[int(1)])->cy_0 = params_9[int(7)];
    (&(&p_5)->vignette_params_1[int(1)])->alpha0_0 = params_9[int(8)];
    (&(&p_5)->vignette_params_1[int(1)])->alpha1_0 = params_9[int(9)];
    (&(&p_5)->vignette_params_1[int(1)])->alpha2_0 = params_9[int(10)];
    (&(&p_5)->vignette_params_1[int(2)])->cx_0 = params_9[int(11)];
    (&(&p_5)->vignette_params_1[int(2)])->cy_0 = params_9[int(12)];
    (&(&p_5)->vignette_params_1[int(2)])->alpha0_0 = params_9[int(13)];
    (&(&p_5)->vignette_params_1[int(2)])->alpha1_0 = params_9[int(14)];
    (&(&p_5)->vignette_params_1[int(2)])->alpha2_0 = params_9[int(15)];
    *&((&(&(&p_5)->color_params_2)->b_0)->x) = params_9[int(16)];
    *&((&(&(&p_5)->color_params_2)->b_0)->y) = params_9[int(17)];
    *&((&(&(&p_5)->color_params_2)->r_0)->x) = params_9[int(18)];
    *&((&(&(&p_5)->color_params_2)->r_0)->y) = params_9[int(19)];
    *&((&(&(&p_5)->color_params_2)->g_0)->x) = params_9[int(20)];
    *&((&(&(&p_5)->color_params_2)->g_0)->y) = params_9[int(21)];
    *&((&(&(&p_5)->color_params_2)->n_0)->x) = params_9[int(22)];
    *&((&(&(&p_5)->color_params_2)->n_0)->y) = params_9[int(23)];
    (&(&p_5)->crf_params_0[int(0)])->g0_0 = params_9[int(24)];
    (&(&p_5)->crf_params_0[int(0)])->g1_0 = params_9[int(25)];
    (&(&p_5)->crf_params_0[int(0)])->x0_0 = params_9[int(26)];
    (&(&p_5)->crf_params_0[int(0)])->y0_0 = params_9[int(27)];
    (&(&p_5)->crf_params_0[int(0)])->gc_0 = params_9[int(28)];
    (&(&p_5)->crf_params_0[int(1)])->g0_0 = params_9[int(29)];
    (&(&p_5)->crf_params_0[int(1)])->g1_0 = params_9[int(30)];
    (&(&p_5)->crf_params_0[int(1)])->x0_0 = params_9[int(31)];
    (&(&p_5)->crf_params_0[int(1)])->y0_0 = params_9[int(32)];
    (&(&p_5)->crf_params_0[int(1)])->gc_0 = params_9[int(33)];
    (&(&p_5)->crf_params_0[int(2)])->g0_0 = params_9[int(34)];
    (&(&p_5)->crf_params_0[int(2)])->g1_0 = params_9[int(35)];
    (&(&p_5)->crf_params_0[int(2)])->x0_0 = params_9[int(36)];
    (&(&p_5)->crf_params_0[int(2)])->y0_0 = params_9[int(37)];
    (&(&p_5)->crf_params_0[int(2)])->gc_0 = params_9[int(38)];
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
    losses_1[int(0)] = p_5.exposure_2;
    float _S1546 = p_5.vignette_params_1[int(0)].cx_0;
    float _S1547 = p_5.vignette_params_1[int(0)].cy_0;
    float _S1548 = p_5.vignette_params_1[int(1)].cx_0;
    float _S1549 = p_5.vignette_params_1[int(1)].cy_0;
    float _S1550 = p_5.vignette_params_1[int(2)].cx_0;
    float _S1551 = p_5.vignette_params_1[int(2)].cy_0;
    losses_1[int(1)] = _S1546 * _S1546 + _S1547 * _S1547 + _S1548 * _S1548 + _S1549 * _S1549 + _S1550 * _S1550 + _S1551 * _S1551;
    losses_1[int(2)] = (F32_max((0.0f), (p_5.vignette_params_1[int(0)].alpha0_0))) + (F32_max((0.0f), (p_5.vignette_params_1[int(1)].alpha0_0))) + (F32_max((0.0f), (p_5.vignette_params_1[int(2)].alpha0_0)));
    losses_1[int(3)] = (F32_max((0.0f), (p_5.vignette_params_1[int(0)].alpha1_0))) + (F32_max((0.0f), (p_5.vignette_params_1[int(1)].alpha1_0))) + (F32_max((0.0f), (p_5.vignette_params_1[int(2)].alpha1_0)));
    losses_1[int(4)] = (F32_max((0.0f), (p_5.vignette_params_1[int(0)].alpha2_0))) + (F32_max((0.0f), (p_5.vignette_params_1[int(1)].alpha2_0))) + (F32_max((0.0f), (p_5.vignette_params_1[int(2)].alpha2_0)));
    float mean_9 = (p_5.vignette_params_1[int(0)].cx_0 + p_5.vignette_params_1[int(1)].cx_0 + p_5.vignette_params_1[int(2)].cx_0) / 3.0f;
    float _S1552 = p_5.vignette_params_1[int(0)].cx_0 - mean_9;
    float _S1553 = p_5.vignette_params_1[int(1)].cx_0 - mean_9;
    float _S1554 = p_5.vignette_params_1[int(2)].cx_0 - mean_9;
    losses_1[int(5)] = (_S1552 * _S1552 + _S1553 * _S1553 + _S1554 * _S1554) / 3.0f;
    float mean_10 = (p_5.vignette_params_1[int(0)].cy_0 + p_5.vignette_params_1[int(1)].cy_0 + p_5.vignette_params_1[int(2)].cy_0) / 3.0f;
    float _S1555 = p_5.vignette_params_1[int(0)].cy_0 - mean_10;
    float _S1556 = p_5.vignette_params_1[int(1)].cy_0 - mean_10;
    float _S1557 = p_5.vignette_params_1[int(2)].cy_0 - mean_10;
    losses_1[int(6)] = (_S1555 * _S1555 + _S1556 * _S1556 + _S1557 * _S1557) / 3.0f;
    float mean_11 = (p_5.vignette_params_1[int(0)].alpha0_0 + p_5.vignette_params_1[int(1)].alpha0_0 + p_5.vignette_params_1[int(2)].alpha0_0) / 3.0f;
    float _S1558 = p_5.vignette_params_1[int(0)].alpha0_0 - mean_11;
    float _S1559 = p_5.vignette_params_1[int(1)].alpha0_0 - mean_11;
    float _S1560 = p_5.vignette_params_1[int(2)].alpha0_0 - mean_11;
    losses_1[int(7)] = (_S1558 * _S1558 + _S1559 * _S1559 + _S1560 * _S1560) / 3.0f;
    float mean_12 = (p_5.vignette_params_1[int(0)].alpha1_0 + p_5.vignette_params_1[int(1)].alpha1_0 + p_5.vignette_params_1[int(2)].alpha1_0) / 3.0f;
    float _S1561 = p_5.vignette_params_1[int(0)].alpha1_0 - mean_12;
    float _S1562 = p_5.vignette_params_1[int(1)].alpha1_0 - mean_12;
    float _S1563 = p_5.vignette_params_1[int(2)].alpha1_0 - mean_12;
    losses_1[int(8)] = (_S1561 * _S1561 + _S1562 * _S1562 + _S1563 * _S1563) / 3.0f;
    float mean_13 = (p_5.vignette_params_1[int(0)].alpha2_0 + p_5.vignette_params_1[int(1)].alpha2_0 + p_5.vignette_params_1[int(2)].alpha2_0) / 3.0f;
    float _S1564 = p_5.vignette_params_1[int(0)].alpha2_0 - mean_13;
    float _S1565 = p_5.vignette_params_1[int(1)].alpha2_0 - mean_13;
    float _S1566 = p_5.vignette_params_1[int(2)].alpha2_0 - mean_13;
    losses_1[int(9)] = (_S1564 * _S1564 + _S1565 * _S1565 + _S1566 * _S1566) / 3.0f;
    float2  bd_5 = mul_0(makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f), p_5.color_params_2.b_0);
    float2  rd_5 = mul_0(makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f), p_5.color_params_2.r_0);
    float2  gd_5 = mul_0(makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f), p_5.color_params_2.g_0);
    float2  nd_5 = mul_0(makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f), p_5.color_params_2.n_0);
    losses_1[int(10)] = bd_5.x;
    losses_1[int(11)] = bd_5.y;
    losses_1[int(12)] = rd_5.x;
    losses_1[int(13)] = rd_5.y;
    losses_1[int(14)] = gd_5.x;
    losses_1[int(15)] = gd_5.y;
    losses_1[int(16)] = nd_5.x;
    losses_1[int(17)] = nd_5.y;
    float mean_14 = (p_5.crf_params_0[int(0)].g0_0 + p_5.crf_params_0[int(1)].g0_0 + p_5.crf_params_0[int(2)].g0_0) / 3.0f;
    float _S1567 = p_5.crf_params_0[int(0)].g0_0 - mean_14;
    float _S1568 = p_5.crf_params_0[int(1)].g0_0 - mean_14;
    float _S1569 = p_5.crf_params_0[int(2)].g0_0 - mean_14;
    losses_1[int(18)] = (_S1567 * _S1567 + _S1568 * _S1568 + _S1569 * _S1569) / 3.0f;
    float mean_15 = (p_5.crf_params_0[int(0)].g1_0 + p_5.crf_params_0[int(1)].g1_0 + p_5.crf_params_0[int(2)].g1_0) / 3.0f;
    float _S1570 = p_5.crf_params_0[int(0)].g1_0 - mean_15;
    float _S1571 = p_5.crf_params_0[int(1)].g1_0 - mean_15;
    float _S1572 = p_5.crf_params_0[int(2)].g1_0 - mean_15;
    losses_1[int(19)] = (_S1570 * _S1570 + _S1571 * _S1571 + _S1572 * _S1572) / 3.0f;
    float mean_16 = (p_5.crf_params_0[int(0)].x0_0 + p_5.crf_params_0[int(1)].x0_0 + p_5.crf_params_0[int(2)].x0_0) / 3.0f;
    float _S1573 = p_5.crf_params_0[int(0)].x0_0 - mean_16;
    float _S1574 = p_5.crf_params_0[int(1)].x0_0 - mean_16;
    float _S1575 = p_5.crf_params_0[int(2)].x0_0 - mean_16;
    losses_1[int(20)] = (_S1573 * _S1573 + _S1574 * _S1574 + _S1575 * _S1575) / 3.0f;
    float mean_17 = (p_5.crf_params_0[int(0)].y0_0 + p_5.crf_params_0[int(1)].y0_0 + p_5.crf_params_0[int(2)].y0_0) / 3.0f;
    float _S1576 = p_5.crf_params_0[int(0)].y0_0 - mean_17;
    float _S1577 = p_5.crf_params_0[int(1)].y0_0 - mean_17;
    float _S1578 = p_5.crf_params_0[int(2)].y0_0 - mean_17;
    losses_1[int(21)] = (_S1576 * _S1576 + _S1577 * _S1577 + _S1578 * _S1578) / 3.0f;
    float mean_18 = (p_5.crf_params_0[int(0)].gc_0 + p_5.crf_params_0[int(1)].gc_0 + p_5.crf_params_0[int(2)].gc_0) / 3.0f;
    float _S1579 = p_5.crf_params_0[int(0)].gc_0 - mean_18;
    float _S1580 = p_5.crf_params_0[int(1)].gc_0 - mean_18;
    float _S1581 = p_5.crf_params_0[int(2)].gc_0 - mean_18;
    losses_1[int(22)] = (_S1579 * _S1579 + _S1580 * _S1580 + _S1581 * _S1581) / 3.0f;
    *_S1545 = losses_1;
    return;
}

inline __device__ void s_bwd_prop_compute_raw_ppisp_regularization_loss_0(DiffPair_arrayx3Cfloatx2C36x3E_0 * dpparams_4, FixedArray<float, 22>  * _s_dOut_4)
{
    VignettingChannelParams_0 _S1582 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    FixedArray<VignettingChannelParams_0, 3>  _S1583 = {
        _S1582, _S1582, _S1582
    };
    float2  _S1584 = make_float2 (0.0f);
    ColorPPISPParams_0 _S1585 = { _S1584, _S1584, _S1584, _S1584 };
    CRFPPISPChannelParams_0 _S1586 = { 0.0f, 0.0f, 0.0f, 0.0f };
    FixedArray<CRFPPISPChannelParams_0, 3>  _S1587 = {
        _S1586, _S1586, _S1586
    };
    PPISPParams_0 _S1588;
    (&_S1588)->exposure_3 = dpparams_4->primal_0[int(0)];
    (&_S1588)->vignette_params_2 = _S1583;
    (&_S1588)->color_params_3 = _S1585;
    (&_S1588)->crf_params_1 = _S1587;
    (&(&_S1588)->vignette_params_2[int(0)])->cx_0 = dpparams_4->primal_0[int(1)];
    (&(&_S1588)->vignette_params_2[int(0)])->cy_0 = dpparams_4->primal_0[int(2)];
    (&(&_S1588)->vignette_params_2[int(0)])->alpha0_0 = dpparams_4->primal_0[int(3)];
    (&(&_S1588)->vignette_params_2[int(0)])->alpha1_0 = dpparams_4->primal_0[int(4)];
    (&(&_S1588)->vignette_params_2[int(0)])->alpha2_0 = dpparams_4->primal_0[int(5)];
    (&(&_S1588)->vignette_params_2[int(1)])->cx_0 = dpparams_4->primal_0[int(6)];
    (&(&_S1588)->vignette_params_2[int(1)])->cy_0 = dpparams_4->primal_0[int(7)];
    (&(&_S1588)->vignette_params_2[int(1)])->alpha0_0 = dpparams_4->primal_0[int(8)];
    (&(&_S1588)->vignette_params_2[int(1)])->alpha1_0 = dpparams_4->primal_0[int(9)];
    (&(&_S1588)->vignette_params_2[int(1)])->alpha2_0 = dpparams_4->primal_0[int(10)];
    (&(&_S1588)->vignette_params_2[int(2)])->cx_0 = dpparams_4->primal_0[int(11)];
    (&(&_S1588)->vignette_params_2[int(2)])->cy_0 = dpparams_4->primal_0[int(12)];
    (&(&_S1588)->vignette_params_2[int(2)])->alpha0_0 = dpparams_4->primal_0[int(13)];
    (&(&_S1588)->vignette_params_2[int(2)])->alpha1_0 = dpparams_4->primal_0[int(14)];
    (&(&_S1588)->vignette_params_2[int(2)])->alpha2_0 = dpparams_4->primal_0[int(15)];
    *&((&(&(&_S1588)->color_params_3)->b_0)->x) = dpparams_4->primal_0[int(16)];
    *&((&(&(&_S1588)->color_params_3)->b_0)->y) = dpparams_4->primal_0[int(17)];
    *&((&(&(&_S1588)->color_params_3)->r_0)->x) = dpparams_4->primal_0[int(18)];
    *&((&(&(&_S1588)->color_params_3)->r_0)->y) = dpparams_4->primal_0[int(19)];
    *&((&(&(&_S1588)->color_params_3)->g_0)->x) = dpparams_4->primal_0[int(20)];
    *&((&(&(&_S1588)->color_params_3)->g_0)->y) = dpparams_4->primal_0[int(21)];
    *&((&(&(&_S1588)->color_params_3)->n_0)->x) = dpparams_4->primal_0[int(22)];
    *&((&(&(&_S1588)->color_params_3)->n_0)->y) = dpparams_4->primal_0[int(23)];
    (&(&_S1588)->crf_params_1[int(0)])->toe_0 = dpparams_4->primal_0[int(24)];
    (&(&_S1588)->crf_params_1[int(0)])->shoulder_0 = dpparams_4->primal_0[int(25)];
    (&(&_S1588)->crf_params_1[int(0)])->gamma_0 = dpparams_4->primal_0[int(26)];
    (&(&_S1588)->crf_params_1[int(0)])->center_0 = dpparams_4->primal_0[int(27)];
    (&(&_S1588)->crf_params_1[int(1)])->toe_0 = dpparams_4->primal_0[int(28)];
    (&(&_S1588)->crf_params_1[int(1)])->shoulder_0 = dpparams_4->primal_0[int(29)];
    (&(&_S1588)->crf_params_1[int(1)])->gamma_0 = dpparams_4->primal_0[int(30)];
    (&(&_S1588)->crf_params_1[int(1)])->center_0 = dpparams_4->primal_0[int(31)];
    (&(&_S1588)->crf_params_1[int(2)])->toe_0 = dpparams_4->primal_0[int(32)];
    (&(&_S1588)->crf_params_1[int(2)])->shoulder_0 = dpparams_4->primal_0[int(33)];
    (&(&_S1588)->crf_params_1[int(2)])->gamma_0 = dpparams_4->primal_0[int(34)];
    (&(&_S1588)->crf_params_1[int(2)])->center_0 = dpparams_4->primal_0[int(35)];
    float mean_19 = (dpparams_4->primal_0[int(1)] + dpparams_4->primal_0[int(6)] + dpparams_4->primal_0[int(11)]) / 3.0f;
    float _S1589 = dpparams_4->primal_0[int(1)] - mean_19;
    float _S1590 = dpparams_4->primal_0[int(6)] - mean_19;
    float _S1591 = dpparams_4->primal_0[int(11)] - mean_19;
    float mean_20 = (dpparams_4->primal_0[int(2)] + dpparams_4->primal_0[int(7)] + dpparams_4->primal_0[int(12)]) / 3.0f;
    float _S1592 = dpparams_4->primal_0[int(2)] - mean_20;
    float _S1593 = dpparams_4->primal_0[int(7)] - mean_20;
    float _S1594 = dpparams_4->primal_0[int(12)] - mean_20;
    float mean_21 = (dpparams_4->primal_0[int(3)] + dpparams_4->primal_0[int(8)] + dpparams_4->primal_0[int(13)]) / 3.0f;
    float _S1595 = dpparams_4->primal_0[int(3)] - mean_21;
    float _S1596 = dpparams_4->primal_0[int(8)] - mean_21;
    float _S1597 = dpparams_4->primal_0[int(13)] - mean_21;
    float mean_22 = (dpparams_4->primal_0[int(4)] + dpparams_4->primal_0[int(9)] + dpparams_4->primal_0[int(14)]) / 3.0f;
    float _S1598 = dpparams_4->primal_0[int(4)] - mean_22;
    float _S1599 = dpparams_4->primal_0[int(9)] - mean_22;
    float _S1600 = dpparams_4->primal_0[int(14)] - mean_22;
    float mean_23 = (dpparams_4->primal_0[int(5)] + dpparams_4->primal_0[int(10)] + dpparams_4->primal_0[int(15)]) / 3.0f;
    float _S1601 = dpparams_4->primal_0[int(5)] - mean_23;
    float _S1602 = dpparams_4->primal_0[int(10)] - mean_23;
    float _S1603 = dpparams_4->primal_0[int(15)] - mean_23;
    float mean_24 = (dpparams_4->primal_0[int(24)] + dpparams_4->primal_0[int(28)] + dpparams_4->primal_0[int(32)]) / 3.0f;
    float mean_25 = (dpparams_4->primal_0[int(25)] + dpparams_4->primal_0[int(29)] + dpparams_4->primal_0[int(33)]) / 3.0f;
    float mean_26 = (dpparams_4->primal_0[int(26)] + dpparams_4->primal_0[int(30)] + dpparams_4->primal_0[int(34)]) / 3.0f;
    float mean_27 = (dpparams_4->primal_0[int(27)] + dpparams_4->primal_0[int(31)] + dpparams_4->primal_0[int(35)]) / 3.0f;
    float _S1604 = 0.3333333432674408f * (*_s_dOut_4)[int(21)];
    float _S1605 = (dpparams_4->primal_0[int(35)] - mean_27) * _S1604;
    float _S1606 = _S1605 + _S1605;
    float _S1607 = (dpparams_4->primal_0[int(31)] - mean_27) * _S1604;
    float _S1608 = _S1607 + _S1607;
    float _S1609 = (dpparams_4->primal_0[int(27)] - mean_27) * _S1604;
    float _S1610 = _S1609 + _S1609;
    float _S1611 = 0.3333333432674408f * (- _S1606 + - _S1608 + - _S1610);
    float _S1612 = 0.3333333432674408f * (*_s_dOut_4)[int(20)];
    float _S1613 = (dpparams_4->primal_0[int(34)] - mean_26) * _S1612;
    float _S1614 = _S1613 + _S1613;
    float _S1615 = (dpparams_4->primal_0[int(30)] - mean_26) * _S1612;
    float _S1616 = _S1615 + _S1615;
    float _S1617 = (dpparams_4->primal_0[int(26)] - mean_26) * _S1612;
    float _S1618 = _S1617 + _S1617;
    float _S1619 = 0.3333333432674408f * (- _S1614 + - _S1616 + - _S1618);
    float _S1620 = 0.3333333432674408f * (*_s_dOut_4)[int(19)];
    float _S1621 = (dpparams_4->primal_0[int(33)] - mean_25) * _S1620;
    float _S1622 = _S1621 + _S1621;
    float _S1623 = (dpparams_4->primal_0[int(29)] - mean_25) * _S1620;
    float _S1624 = _S1623 + _S1623;
    float _S1625 = (dpparams_4->primal_0[int(25)] - mean_25) * _S1620;
    float _S1626 = _S1625 + _S1625;
    float _S1627 = 0.3333333432674408f * (- _S1622 + - _S1624 + - _S1626);
    float _S1628 = 0.3333333432674408f * (*_s_dOut_4)[int(18)];
    float _S1629 = (dpparams_4->primal_0[int(32)] - mean_24) * _S1628;
    float _S1630 = _S1629 + _S1629;
    float _S1631 = (dpparams_4->primal_0[int(28)] - mean_24) * _S1628;
    float _S1632 = _S1631 + _S1631;
    float _S1633 = (dpparams_4->primal_0[int(24)] - mean_24) * _S1628;
    float _S1634 = _S1633 + _S1633;
    float _S1635 = 0.3333333432674408f * (- _S1630 + - _S1632 + - _S1634);
    float2  _S1636 = make_float2 ((*_s_dOut_4)[int(16)], (*_s_dOut_4)[int(17)]);
    Matrix<float, 2, 2>  _S1637 = makeMatrix<float, 2, 2> (0.0f);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1638;
    (&_S1638)->primal_0 = makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f);
    (&_S1638)->differential_0 = _S1637;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1639;
    (&_S1639)->primal_0 = _S1588.color_params_3.n_0;
    (&_S1639)->differential_0 = _S1584;
    s_bwd_prop_mul_2(&_S1638, &_S1639, _S1636);
    float2  _S1640 = make_float2 ((*_s_dOut_4)[int(14)], (*_s_dOut_4)[int(15)]);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1641;
    (&_S1641)->primal_0 = makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f);
    (&_S1641)->differential_0 = _S1637;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1642;
    (&_S1642)->primal_0 = _S1588.color_params_3.g_0;
    (&_S1642)->differential_0 = _S1584;
    s_bwd_prop_mul_2(&_S1641, &_S1642, _S1640);
    float2  _S1643 = make_float2 ((*_s_dOut_4)[int(12)], (*_s_dOut_4)[int(13)]);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1644;
    (&_S1644)->primal_0 = makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f);
    (&_S1644)->differential_0 = _S1637;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1645;
    (&_S1645)->primal_0 = _S1588.color_params_3.r_0;
    (&_S1645)->differential_0 = _S1584;
    s_bwd_prop_mul_2(&_S1644, &_S1645, _S1643);
    float2  _S1646 = make_float2 ((*_s_dOut_4)[int(10)], (*_s_dOut_4)[int(11)]);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1647;
    (&_S1647)->primal_0 = makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f);
    (&_S1647)->differential_0 = _S1637;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1648;
    (&_S1648)->primal_0 = _S1588.color_params_3.b_0;
    (&_S1648)->differential_0 = _S1584;
    s_bwd_prop_mul_2(&_S1647, &_S1648, _S1646);
    ColorPPISPParams_0 _S1649 = ColorPPISPParams_x24_syn_dzero_0();
    (&_S1649)->n_0 = _S1639.differential_0;
    (&_S1649)->g_0 = _S1642.differential_0;
    (&_S1649)->r_0 = _S1645.differential_0;
    (&_S1649)->b_0 = _S1648.differential_0;
    float _S1650 = 0.3333333432674408f * (*_s_dOut_4)[int(9)];
    float _S1651 = _S1603 * _S1650;
    float _S1652 = _S1651 + _S1651;
    float _S1653 = _S1602 * _S1650;
    float _S1654 = _S1653 + _S1653;
    float _S1655 = _S1601 * _S1650;
    float _S1656 = _S1655 + _S1655;
    float _S1657 = 0.3333333432674408f * (- _S1652 + - _S1654 + - _S1656);
    float _S1658 = 0.3333333432674408f * (*_s_dOut_4)[int(8)];
    float _S1659 = _S1600 * _S1658;
    float _S1660 = _S1659 + _S1659;
    float _S1661 = _S1599 * _S1658;
    float _S1662 = _S1661 + _S1661;
    float _S1663 = _S1598 * _S1658;
    float _S1664 = _S1663 + _S1663;
    float _S1665 = 0.3333333432674408f * (- _S1660 + - _S1662 + - _S1664);
    float _S1666 = 0.3333333432674408f * (*_s_dOut_4)[int(7)];
    float _S1667 = _S1597 * _S1666;
    float _S1668 = _S1667 + _S1667;
    float _S1669 = _S1596 * _S1666;
    float _S1670 = _S1669 + _S1669;
    float _S1671 = _S1595 * _S1666;
    float _S1672 = _S1671 + _S1671;
    float _S1673 = 0.3333333432674408f * (- _S1668 + - _S1670 + - _S1672);
    float _S1674 = 0.3333333432674408f * (*_s_dOut_4)[int(6)];
    float _S1675 = _S1594 * _S1674;
    float _S1676 = _S1675 + _S1675;
    float _S1677 = _S1593 * _S1674;
    float _S1678 = _S1677 + _S1677;
    float _S1679 = _S1592 * _S1674;
    float _S1680 = _S1679 + _S1679;
    float _S1681 = 0.3333333432674408f * (- _S1676 + - _S1678 + - _S1680);
    float _S1682 = 0.3333333432674408f * (*_s_dOut_4)[int(5)];
    float _S1683 = _S1591 * _S1682;
    float _S1684 = _S1683 + _S1683;
    float _S1685 = _S1590 * _S1682;
    float _S1686 = _S1685 + _S1685;
    float _S1687 = _S1589 * _S1682;
    float _S1688 = _S1687 + _S1687;
    float _S1689 = 0.3333333432674408f * (- _S1684 + - _S1686 + - _S1688);
    DiffPair_float_0 _S1690;
    (&_S1690)->primal_0 = 0.0f;
    (&_S1690)->differential_0 = 0.0f;
    DiffPair_float_0 _S1691;
    (&_S1691)->primal_0 = dpparams_4->primal_0[int(15)];
    (&_S1691)->differential_0 = 0.0f;
    _d_max_0(&_S1690, &_S1691, (*_s_dOut_4)[int(4)]);
    DiffPair_float_0 _S1692;
    (&_S1692)->primal_0 = 0.0f;
    (&_S1692)->differential_0 = 0.0f;
    DiffPair_float_0 _S1693;
    (&_S1693)->primal_0 = dpparams_4->primal_0[int(10)];
    (&_S1693)->differential_0 = 0.0f;
    _d_max_0(&_S1692, &_S1693, (*_s_dOut_4)[int(4)]);
    DiffPair_float_0 _S1694;
    (&_S1694)->primal_0 = 0.0f;
    (&_S1694)->differential_0 = 0.0f;
    DiffPair_float_0 _S1695;
    (&_S1695)->primal_0 = dpparams_4->primal_0[int(5)];
    (&_S1695)->differential_0 = 0.0f;
    _d_max_0(&_S1694, &_S1695, (*_s_dOut_4)[int(4)]);
    DiffPair_float_0 _S1696;
    (&_S1696)->primal_0 = 0.0f;
    (&_S1696)->differential_0 = 0.0f;
    DiffPair_float_0 _S1697;
    (&_S1697)->primal_0 = dpparams_4->primal_0[int(14)];
    (&_S1697)->differential_0 = 0.0f;
    _d_max_0(&_S1696, &_S1697, (*_s_dOut_4)[int(3)]);
    DiffPair_float_0 _S1698;
    (&_S1698)->primal_0 = 0.0f;
    (&_S1698)->differential_0 = 0.0f;
    DiffPair_float_0 _S1699;
    (&_S1699)->primal_0 = dpparams_4->primal_0[int(9)];
    (&_S1699)->differential_0 = 0.0f;
    _d_max_0(&_S1698, &_S1699, (*_s_dOut_4)[int(3)]);
    DiffPair_float_0 _S1700;
    (&_S1700)->primal_0 = 0.0f;
    (&_S1700)->differential_0 = 0.0f;
    DiffPair_float_0 _S1701;
    (&_S1701)->primal_0 = dpparams_4->primal_0[int(4)];
    (&_S1701)->differential_0 = 0.0f;
    _d_max_0(&_S1700, &_S1701, (*_s_dOut_4)[int(3)]);
    DiffPair_float_0 _S1702;
    (&_S1702)->primal_0 = 0.0f;
    (&_S1702)->differential_0 = 0.0f;
    DiffPair_float_0 _S1703;
    (&_S1703)->primal_0 = dpparams_4->primal_0[int(13)];
    (&_S1703)->differential_0 = 0.0f;
    _d_max_0(&_S1702, &_S1703, (*_s_dOut_4)[int(2)]);
    DiffPair_float_0 _S1704;
    (&_S1704)->primal_0 = 0.0f;
    (&_S1704)->differential_0 = 0.0f;
    DiffPair_float_0 _S1705;
    (&_S1705)->primal_0 = dpparams_4->primal_0[int(8)];
    (&_S1705)->differential_0 = 0.0f;
    _d_max_0(&_S1704, &_S1705, (*_s_dOut_4)[int(2)]);
    DiffPair_float_0 _S1706;
    (&_S1706)->primal_0 = 0.0f;
    (&_S1706)->differential_0 = 0.0f;
    DiffPair_float_0 _S1707;
    (&_S1707)->primal_0 = dpparams_4->primal_0[int(3)];
    (&_S1707)->differential_0 = 0.0f;
    _d_max_0(&_S1706, &_S1707, (*_s_dOut_4)[int(2)]);
    float _S1708 = dpparams_4->primal_0[int(12)] * (*_s_dOut_4)[int(1)];
    float _S1709 = dpparams_4->primal_0[int(11)] * (*_s_dOut_4)[int(1)];
    float _S1710 = dpparams_4->primal_0[int(7)] * (*_s_dOut_4)[int(1)];
    float _S1711 = dpparams_4->primal_0[int(6)] * (*_s_dOut_4)[int(1)];
    float _S1712 = dpparams_4->primal_0[int(2)] * (*_s_dOut_4)[int(1)];
    float _S1713 = dpparams_4->primal_0[int(1)] * (*_s_dOut_4)[int(1)];
    PPISPParams_0 _S1714 = PPISPParams_x24_syn_dzero_0();
    (&_S1714)->color_params_3 = _S1649;
    (&_S1714)->exposure_3 = (*_s_dOut_4)[int(0)];
    _S1588 = _S1714;
    (&(&_S1588)->crf_params_1[int(2)])->center_0 = 0.0f;
    float _S1715 = _S1606 + _S1611 + _S1714.crf_params_1[int(2)].center_0;
    (&(&_S1588)->crf_params_1[int(2)])->gamma_0 = 0.0f;
    float _S1716 = _S1614 + _S1619 + _S1714.crf_params_1[int(2)].gamma_0;
    (&(&_S1588)->crf_params_1[int(2)])->shoulder_0 = 0.0f;
    float _S1717 = _S1622 + _S1627 + _S1714.crf_params_1[int(2)].shoulder_0;
    (&(&_S1588)->crf_params_1[int(2)])->toe_0 = 0.0f;
    float _S1718 = _S1630 + _S1635 + _S1714.crf_params_1[int(2)].toe_0;
    (&(&_S1588)->crf_params_1[int(1)])->center_0 = 0.0f;
    float _S1719 = _S1608 + _S1611 + _S1714.crf_params_1[int(1)].center_0;
    (&(&_S1588)->crf_params_1[int(1)])->gamma_0 = 0.0f;
    float _S1720 = _S1616 + _S1619 + _S1714.crf_params_1[int(1)].gamma_0;
    (&(&_S1588)->crf_params_1[int(1)])->shoulder_0 = 0.0f;
    float _S1721 = _S1624 + _S1627 + _S1714.crf_params_1[int(1)].shoulder_0;
    (&(&_S1588)->crf_params_1[int(1)])->toe_0 = 0.0f;
    float _S1722 = _S1632 + _S1635 + _S1714.crf_params_1[int(1)].toe_0;
    (&(&_S1588)->crf_params_1[int(0)])->center_0 = 0.0f;
    float _S1723 = _S1610 + _S1611 + _S1714.crf_params_1[int(0)].center_0;
    (&(&_S1588)->crf_params_1[int(0)])->gamma_0 = 0.0f;
    float _S1724 = _S1618 + _S1619 + _S1714.crf_params_1[int(0)].gamma_0;
    (&(&_S1588)->crf_params_1[int(0)])->shoulder_0 = 0.0f;
    float _S1725 = _S1626 + _S1627 + _S1714.crf_params_1[int(0)].shoulder_0;
    (&(&_S1588)->crf_params_1[int(0)])->toe_0 = 0.0f;
    float _S1726 = _S1634 + _S1635 + _S1714.crf_params_1[int(0)].toe_0;
    *&((&(&(&_S1588)->color_params_3)->n_0)->y) = 0.0f;
    *&((&(&(&_S1588)->color_params_3)->n_0)->x) = 0.0f;
    *&((&(&(&_S1588)->color_params_3)->g_0)->y) = 0.0f;
    *&((&(&(&_S1588)->color_params_3)->g_0)->x) = 0.0f;
    *&((&(&(&_S1588)->color_params_3)->r_0)->y) = 0.0f;
    *&((&(&(&_S1588)->color_params_3)->r_0)->x) = 0.0f;
    *&((&(&(&_S1588)->color_params_3)->b_0)->y) = 0.0f;
    *&((&(&(&_S1588)->color_params_3)->b_0)->x) = 0.0f;
    (&(&_S1588)->vignette_params_2[int(2)])->alpha2_0 = 0.0f;
    float _S1727 = _S1652 + _S1657 + _S1691.differential_0 + _S1714.vignette_params_2[int(2)].alpha2_0;
    (&(&_S1588)->vignette_params_2[int(2)])->alpha1_0 = 0.0f;
    float _S1728 = _S1660 + _S1665 + _S1697.differential_0 + _S1714.vignette_params_2[int(2)].alpha1_0;
    (&(&_S1588)->vignette_params_2[int(2)])->alpha0_0 = 0.0f;
    float _S1729 = _S1668 + _S1673 + _S1703.differential_0 + _S1714.vignette_params_2[int(2)].alpha0_0;
    (&(&_S1588)->vignette_params_2[int(2)])->cy_0 = 0.0f;
    float _S1730 = _S1676 + _S1681 + _S1708 + _S1708 + _S1714.vignette_params_2[int(2)].cy_0;
    (&(&_S1588)->vignette_params_2[int(2)])->cx_0 = 0.0f;
    float _S1731 = _S1684 + _S1689 + _S1709 + _S1709 + _S1714.vignette_params_2[int(2)].cx_0;
    (&(&_S1588)->vignette_params_2[int(1)])->alpha2_0 = 0.0f;
    float _S1732 = _S1654 + _S1657 + _S1693.differential_0 + _S1714.vignette_params_2[int(1)].alpha2_0;
    (&(&_S1588)->vignette_params_2[int(1)])->alpha1_0 = 0.0f;
    float _S1733 = _S1662 + _S1665 + _S1699.differential_0 + _S1714.vignette_params_2[int(1)].alpha1_0;
    (&(&_S1588)->vignette_params_2[int(1)])->alpha0_0 = 0.0f;
    float _S1734 = _S1670 + _S1673 + _S1705.differential_0 + _S1714.vignette_params_2[int(1)].alpha0_0;
    (&(&_S1588)->vignette_params_2[int(1)])->cy_0 = 0.0f;
    float _S1735 = _S1678 + _S1681 + _S1710 + _S1710 + _S1714.vignette_params_2[int(1)].cy_0;
    (&(&_S1588)->vignette_params_2[int(1)])->cx_0 = 0.0f;
    float _S1736 = _S1686 + _S1689 + _S1711 + _S1711 + _S1714.vignette_params_2[int(1)].cx_0;
    (&(&_S1588)->vignette_params_2[int(0)])->alpha2_0 = 0.0f;
    float _S1737 = _S1656 + _S1657 + _S1695.differential_0 + _S1714.vignette_params_2[int(0)].alpha2_0;
    (&(&_S1588)->vignette_params_2[int(0)])->alpha1_0 = 0.0f;
    float _S1738 = _S1664 + _S1665 + _S1701.differential_0 + _S1714.vignette_params_2[int(0)].alpha1_0;
    (&(&_S1588)->vignette_params_2[int(0)])->alpha0_0 = 0.0f;
    float _S1739 = _S1672 + _S1673 + _S1707.differential_0 + _S1714.vignette_params_2[int(0)].alpha0_0;
    (&(&_S1588)->vignette_params_2[int(0)])->cy_0 = 0.0f;
    float _S1740 = _S1680 + _S1681 + _S1712 + _S1712 + _S1714.vignette_params_2[int(0)].cy_0;
    (&(&_S1588)->vignette_params_2[int(0)])->cx_0 = 0.0f;
    float _S1741 = _S1688 + _S1689 + _S1713 + _S1713 + _S1714.vignette_params_2[int(0)].cx_0;
    FixedArray<float, 36>  _S1742;
    _S1742[int(0)] = 0.0f;
    _S1742[int(1)] = 0.0f;
    _S1742[int(2)] = 0.0f;
    _S1742[int(3)] = 0.0f;
    _S1742[int(4)] = 0.0f;
    _S1742[int(5)] = 0.0f;
    _S1742[int(6)] = 0.0f;
    _S1742[int(7)] = 0.0f;
    _S1742[int(8)] = 0.0f;
    _S1742[int(9)] = 0.0f;
    _S1742[int(10)] = 0.0f;
    _S1742[int(11)] = 0.0f;
    _S1742[int(12)] = 0.0f;
    _S1742[int(13)] = 0.0f;
    _S1742[int(14)] = 0.0f;
    _S1742[int(15)] = 0.0f;
    _S1742[int(16)] = 0.0f;
    _S1742[int(17)] = 0.0f;
    _S1742[int(18)] = 0.0f;
    _S1742[int(19)] = 0.0f;
    _S1742[int(20)] = 0.0f;
    _S1742[int(21)] = 0.0f;
    _S1742[int(22)] = 0.0f;
    _S1742[int(23)] = 0.0f;
    _S1742[int(24)] = 0.0f;
    _S1742[int(25)] = 0.0f;
    _S1742[int(26)] = 0.0f;
    _S1742[int(27)] = 0.0f;
    _S1742[int(28)] = 0.0f;
    _S1742[int(29)] = 0.0f;
    _S1742[int(30)] = 0.0f;
    _S1742[int(31)] = 0.0f;
    _S1742[int(32)] = 0.0f;
    _S1742[int(33)] = 0.0f;
    _S1742[int(34)] = 0.0f;
    _S1742[int(35)] = 0.0f;
    _S1742[int(8)] = _S1734;
    _S1742[int(16)] = _S1714.color_params_3.b_0.x;
    _S1742[int(15)] = _S1727;
    _S1742[int(14)] = _S1728;
    _S1742[int(13)] = _S1729;
    _S1742[int(12)] = _S1730;
    _S1742[int(11)] = _S1731;
    _S1742[int(10)] = _S1732;
    _S1742[int(9)] = _S1733;
    _S1742[int(17)] = _S1714.color_params_3.b_0.y;
    _S1742[int(7)] = _S1735;
    _S1742[int(6)] = _S1736;
    _S1742[int(5)] = _S1737;
    _S1742[int(4)] = _S1738;
    _S1742[int(3)] = _S1739;
    _S1742[int(2)] = _S1740;
    _S1742[int(1)] = _S1741;
    _S1742[int(0)] = _S1588.exposure_3;
    _S1742[int(26)] = _S1724;
    _S1742[int(34)] = _S1716;
    _S1742[int(33)] = _S1717;
    _S1742[int(32)] = _S1718;
    _S1742[int(31)] = _S1719;
    _S1742[int(30)] = _S1720;
    _S1742[int(29)] = _S1721;
    _S1742[int(28)] = _S1722;
    _S1742[int(27)] = _S1723;
    _S1742[int(35)] = _S1715;
    _S1742[int(25)] = _S1725;
    _S1742[int(24)] = _S1726;
    _S1742[int(23)] = _S1714.color_params_3.n_0.y;
    _S1742[int(22)] = _S1714.color_params_3.n_0.x;
    _S1742[int(21)] = _S1714.color_params_3.g_0.y;
    _S1742[int(20)] = _S1714.color_params_3.g_0.x;
    _S1742[int(19)] = _S1714.color_params_3.r_0.y;
    _S1742[int(18)] = _S1714.color_params_3.r_0.x;
    dpparams_4->primal_0 = dpparams_4->primal_0;
    dpparams_4->differential_0 = _S1742;
    return;
}

inline __device__ void s_bwd_compute_raw_ppisp_regularization_loss_0(DiffPair_arrayx3Cfloatx2C36x3E_0 * _S1743, FixedArray<float, 22>  * _S1744)
{
    s_bwd_prop_compute_raw_ppisp_regularization_loss_0(_S1743, _S1744);
    return;
}

inline __device__ void compute_raw_ppisp_regularization_loss_vjp(FixedArray<float, 36>  params_10, FixedArray<float, 22>  grad_out_4, FixedArray<float, 36>  * _S1745)
{
    FixedArray<float, 36>  _S1746 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    DiffPair_arrayx3Cfloatx2C36x3E_0 dp_params_4;
    (&dp_params_4)->primal_0 = params_10;
    (&dp_params_4)->differential_0 = _S1746;
    FixedArray<float, 22>  _S1747 = grad_out_4;
    s_bwd_compute_raw_ppisp_regularization_loss_0(&dp_params_4, &_S1747);
    *_S1745 = (&dp_params_4)->differential_0;
    return;
}

inline __device__ void s_bwd_prop_compute_raw_ppisp_rqs_regularization_loss_0(DiffPair_arrayx3Cfloatx2C39x3E_0 * dpparams_5, FixedArray<float, 23>  * _s_dOut_5)
{
    VignettingChannelParams_0 _S1748 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    FixedArray<VignettingChannelParams_0, 3>  _S1749 = {
        _S1748, _S1748, _S1748
    };
    float2  _S1750 = make_float2 (0.0f);
    ColorPPISPParams_0 _S1751 = { _S1750, _S1750, _S1750, _S1750 };
    RQSCRFPPISPChannelParams_0 _S1752 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    FixedArray<RQSCRFPPISPChannelParams_0, 3>  _S1753 = {
        _S1752, _S1752, _S1752
    };
    PPISPParamsRQS_0 _S1754;
    (&_S1754)->exposure_2 = dpparams_5->primal_0[int(0)];
    (&_S1754)->vignette_params_1 = _S1749;
    (&_S1754)->color_params_2 = _S1751;
    (&_S1754)->crf_params_0 = _S1753;
    (&(&_S1754)->vignette_params_1[int(0)])->cx_0 = dpparams_5->primal_0[int(1)];
    (&(&_S1754)->vignette_params_1[int(0)])->cy_0 = dpparams_5->primal_0[int(2)];
    (&(&_S1754)->vignette_params_1[int(0)])->alpha0_0 = dpparams_5->primal_0[int(3)];
    (&(&_S1754)->vignette_params_1[int(0)])->alpha1_0 = dpparams_5->primal_0[int(4)];
    (&(&_S1754)->vignette_params_1[int(0)])->alpha2_0 = dpparams_5->primal_0[int(5)];
    (&(&_S1754)->vignette_params_1[int(1)])->cx_0 = dpparams_5->primal_0[int(6)];
    (&(&_S1754)->vignette_params_1[int(1)])->cy_0 = dpparams_5->primal_0[int(7)];
    (&(&_S1754)->vignette_params_1[int(1)])->alpha0_0 = dpparams_5->primal_0[int(8)];
    (&(&_S1754)->vignette_params_1[int(1)])->alpha1_0 = dpparams_5->primal_0[int(9)];
    (&(&_S1754)->vignette_params_1[int(1)])->alpha2_0 = dpparams_5->primal_0[int(10)];
    (&(&_S1754)->vignette_params_1[int(2)])->cx_0 = dpparams_5->primal_0[int(11)];
    (&(&_S1754)->vignette_params_1[int(2)])->cy_0 = dpparams_5->primal_0[int(12)];
    (&(&_S1754)->vignette_params_1[int(2)])->alpha0_0 = dpparams_5->primal_0[int(13)];
    (&(&_S1754)->vignette_params_1[int(2)])->alpha1_0 = dpparams_5->primal_0[int(14)];
    (&(&_S1754)->vignette_params_1[int(2)])->alpha2_0 = dpparams_5->primal_0[int(15)];
    *&((&(&(&_S1754)->color_params_2)->b_0)->x) = dpparams_5->primal_0[int(16)];
    *&((&(&(&_S1754)->color_params_2)->b_0)->y) = dpparams_5->primal_0[int(17)];
    *&((&(&(&_S1754)->color_params_2)->r_0)->x) = dpparams_5->primal_0[int(18)];
    *&((&(&(&_S1754)->color_params_2)->r_0)->y) = dpparams_5->primal_0[int(19)];
    *&((&(&(&_S1754)->color_params_2)->g_0)->x) = dpparams_5->primal_0[int(20)];
    *&((&(&(&_S1754)->color_params_2)->g_0)->y) = dpparams_5->primal_0[int(21)];
    *&((&(&(&_S1754)->color_params_2)->n_0)->x) = dpparams_5->primal_0[int(22)];
    *&((&(&(&_S1754)->color_params_2)->n_0)->y) = dpparams_5->primal_0[int(23)];
    (&(&_S1754)->crf_params_0[int(0)])->g0_0 = dpparams_5->primal_0[int(24)];
    (&(&_S1754)->crf_params_0[int(0)])->g1_0 = dpparams_5->primal_0[int(25)];
    (&(&_S1754)->crf_params_0[int(0)])->x0_0 = dpparams_5->primal_0[int(26)];
    (&(&_S1754)->crf_params_0[int(0)])->y0_0 = dpparams_5->primal_0[int(27)];
    (&(&_S1754)->crf_params_0[int(0)])->gc_0 = dpparams_5->primal_0[int(28)];
    (&(&_S1754)->crf_params_0[int(1)])->g0_0 = dpparams_5->primal_0[int(29)];
    (&(&_S1754)->crf_params_0[int(1)])->g1_0 = dpparams_5->primal_0[int(30)];
    (&(&_S1754)->crf_params_0[int(1)])->x0_0 = dpparams_5->primal_0[int(31)];
    (&(&_S1754)->crf_params_0[int(1)])->y0_0 = dpparams_5->primal_0[int(32)];
    (&(&_S1754)->crf_params_0[int(1)])->gc_0 = dpparams_5->primal_0[int(33)];
    (&(&_S1754)->crf_params_0[int(2)])->g0_0 = dpparams_5->primal_0[int(34)];
    (&(&_S1754)->crf_params_0[int(2)])->g1_0 = dpparams_5->primal_0[int(35)];
    (&(&_S1754)->crf_params_0[int(2)])->x0_0 = dpparams_5->primal_0[int(36)];
    (&(&_S1754)->crf_params_0[int(2)])->y0_0 = dpparams_5->primal_0[int(37)];
    (&(&_S1754)->crf_params_0[int(2)])->gc_0 = dpparams_5->primal_0[int(38)];
    float mean_28 = (dpparams_5->primal_0[int(1)] + dpparams_5->primal_0[int(6)] + dpparams_5->primal_0[int(11)]) / 3.0f;
    float _S1755 = dpparams_5->primal_0[int(1)] - mean_28;
    float _S1756 = dpparams_5->primal_0[int(6)] - mean_28;
    float _S1757 = dpparams_5->primal_0[int(11)] - mean_28;
    float mean_29 = (dpparams_5->primal_0[int(2)] + dpparams_5->primal_0[int(7)] + dpparams_5->primal_0[int(12)]) / 3.0f;
    float _S1758 = dpparams_5->primal_0[int(2)] - mean_29;
    float _S1759 = dpparams_5->primal_0[int(7)] - mean_29;
    float _S1760 = dpparams_5->primal_0[int(12)] - mean_29;
    float mean_30 = (dpparams_5->primal_0[int(3)] + dpparams_5->primal_0[int(8)] + dpparams_5->primal_0[int(13)]) / 3.0f;
    float _S1761 = dpparams_5->primal_0[int(3)] - mean_30;
    float _S1762 = dpparams_5->primal_0[int(8)] - mean_30;
    float _S1763 = dpparams_5->primal_0[int(13)] - mean_30;
    float mean_31 = (dpparams_5->primal_0[int(4)] + dpparams_5->primal_0[int(9)] + dpparams_5->primal_0[int(14)]) / 3.0f;
    float _S1764 = dpparams_5->primal_0[int(4)] - mean_31;
    float _S1765 = dpparams_5->primal_0[int(9)] - mean_31;
    float _S1766 = dpparams_5->primal_0[int(14)] - mean_31;
    float mean_32 = (dpparams_5->primal_0[int(5)] + dpparams_5->primal_0[int(10)] + dpparams_5->primal_0[int(15)]) / 3.0f;
    float _S1767 = dpparams_5->primal_0[int(5)] - mean_32;
    float _S1768 = dpparams_5->primal_0[int(10)] - mean_32;
    float _S1769 = dpparams_5->primal_0[int(15)] - mean_32;
    float mean_33 = (dpparams_5->primal_0[int(24)] + dpparams_5->primal_0[int(29)] + dpparams_5->primal_0[int(34)]) / 3.0f;
    float mean_34 = (dpparams_5->primal_0[int(25)] + dpparams_5->primal_0[int(30)] + dpparams_5->primal_0[int(35)]) / 3.0f;
    float mean_35 = (dpparams_5->primal_0[int(26)] + dpparams_5->primal_0[int(31)] + dpparams_5->primal_0[int(36)]) / 3.0f;
    float mean_36 = (dpparams_5->primal_0[int(27)] + dpparams_5->primal_0[int(32)] + dpparams_5->primal_0[int(37)]) / 3.0f;
    float mean_37 = (dpparams_5->primal_0[int(28)] + dpparams_5->primal_0[int(33)] + dpparams_5->primal_0[int(38)]) / 3.0f;
    float _S1770 = 0.3333333432674408f * (*_s_dOut_5)[int(22)];
    float _S1771 = (dpparams_5->primal_0[int(38)] - mean_37) * _S1770;
    float _S1772 = _S1771 + _S1771;
    float _S1773 = (dpparams_5->primal_0[int(33)] - mean_37) * _S1770;
    float _S1774 = _S1773 + _S1773;
    float _S1775 = (dpparams_5->primal_0[int(28)] - mean_37) * _S1770;
    float _S1776 = _S1775 + _S1775;
    float _S1777 = 0.3333333432674408f * (- _S1772 + - _S1774 + - _S1776);
    float _S1778 = 0.3333333432674408f * (*_s_dOut_5)[int(21)];
    float _S1779 = (dpparams_5->primal_0[int(37)] - mean_36) * _S1778;
    float _S1780 = _S1779 + _S1779;
    float _S1781 = (dpparams_5->primal_0[int(32)] - mean_36) * _S1778;
    float _S1782 = _S1781 + _S1781;
    float _S1783 = (dpparams_5->primal_0[int(27)] - mean_36) * _S1778;
    float _S1784 = _S1783 + _S1783;
    float _S1785 = 0.3333333432674408f * (- _S1780 + - _S1782 + - _S1784);
    float _S1786 = 0.3333333432674408f * (*_s_dOut_5)[int(20)];
    float _S1787 = (dpparams_5->primal_0[int(36)] - mean_35) * _S1786;
    float _S1788 = _S1787 + _S1787;
    float _S1789 = (dpparams_5->primal_0[int(31)] - mean_35) * _S1786;
    float _S1790 = _S1789 + _S1789;
    float _S1791 = (dpparams_5->primal_0[int(26)] - mean_35) * _S1786;
    float _S1792 = _S1791 + _S1791;
    float _S1793 = 0.3333333432674408f * (- _S1788 + - _S1790 + - _S1792);
    float _S1794 = 0.3333333432674408f * (*_s_dOut_5)[int(19)];
    float _S1795 = (dpparams_5->primal_0[int(35)] - mean_34) * _S1794;
    float _S1796 = _S1795 + _S1795;
    float _S1797 = (dpparams_5->primal_0[int(30)] - mean_34) * _S1794;
    float _S1798 = _S1797 + _S1797;
    float _S1799 = (dpparams_5->primal_0[int(25)] - mean_34) * _S1794;
    float _S1800 = _S1799 + _S1799;
    float _S1801 = 0.3333333432674408f * (- _S1796 + - _S1798 + - _S1800);
    float _S1802 = 0.3333333432674408f * (*_s_dOut_5)[int(18)];
    float _S1803 = (dpparams_5->primal_0[int(34)] - mean_33) * _S1802;
    float _S1804 = _S1803 + _S1803;
    float _S1805 = (dpparams_5->primal_0[int(29)] - mean_33) * _S1802;
    float _S1806 = _S1805 + _S1805;
    float _S1807 = (dpparams_5->primal_0[int(24)] - mean_33) * _S1802;
    float _S1808 = _S1807 + _S1807;
    float _S1809 = 0.3333333432674408f * (- _S1804 + - _S1806 + - _S1808);
    float2  _S1810 = make_float2 ((*_s_dOut_5)[int(16)], (*_s_dOut_5)[int(17)]);
    Matrix<float, 2, 2>  _S1811 = makeMatrix<float, 2, 2> (0.0f);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1812;
    (&_S1812)->primal_0 = makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f);
    (&_S1812)->differential_0 = _S1811;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1813;
    (&_S1813)->primal_0 = _S1754.color_params_2.n_0;
    (&_S1813)->differential_0 = _S1750;
    s_bwd_prop_mul_2(&_S1812, &_S1813, _S1810);
    float2  _S1814 = make_float2 ((*_s_dOut_5)[int(14)], (*_s_dOut_5)[int(15)]);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1815;
    (&_S1815)->primal_0 = makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f);
    (&_S1815)->differential_0 = _S1811;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1816;
    (&_S1816)->primal_0 = _S1754.color_params_2.g_0;
    (&_S1816)->differential_0 = _S1750;
    s_bwd_prop_mul_2(&_S1815, &_S1816, _S1814);
    float2  _S1817 = make_float2 ((*_s_dOut_5)[int(12)], (*_s_dOut_5)[int(13)]);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1818;
    (&_S1818)->primal_0 = makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f);
    (&_S1818)->differential_0 = _S1811;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1819;
    (&_S1819)->primal_0 = _S1754.color_params_2.r_0;
    (&_S1819)->differential_0 = _S1750;
    s_bwd_prop_mul_2(&_S1818, &_S1819, _S1817);
    float2  _S1820 = make_float2 ((*_s_dOut_5)[int(10)], (*_s_dOut_5)[int(11)]);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1821;
    (&_S1821)->primal_0 = makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f);
    (&_S1821)->differential_0 = _S1811;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1822;
    (&_S1822)->primal_0 = _S1754.color_params_2.b_0;
    (&_S1822)->differential_0 = _S1750;
    s_bwd_prop_mul_2(&_S1821, &_S1822, _S1820);
    ColorPPISPParams_0 _S1823 = ColorPPISPParams_x24_syn_dzero_0();
    (&_S1823)->n_0 = _S1813.differential_0;
    (&_S1823)->g_0 = _S1816.differential_0;
    (&_S1823)->r_0 = _S1819.differential_0;
    (&_S1823)->b_0 = _S1822.differential_0;
    float _S1824 = 0.3333333432674408f * (*_s_dOut_5)[int(9)];
    float _S1825 = _S1769 * _S1824;
    float _S1826 = _S1825 + _S1825;
    float _S1827 = _S1768 * _S1824;
    float _S1828 = _S1827 + _S1827;
    float _S1829 = _S1767 * _S1824;
    float _S1830 = _S1829 + _S1829;
    float _S1831 = 0.3333333432674408f * (- _S1826 + - _S1828 + - _S1830);
    float _S1832 = 0.3333333432674408f * (*_s_dOut_5)[int(8)];
    float _S1833 = _S1766 * _S1832;
    float _S1834 = _S1833 + _S1833;
    float _S1835 = _S1765 * _S1832;
    float _S1836 = _S1835 + _S1835;
    float _S1837 = _S1764 * _S1832;
    float _S1838 = _S1837 + _S1837;
    float _S1839 = 0.3333333432674408f * (- _S1834 + - _S1836 + - _S1838);
    float _S1840 = 0.3333333432674408f * (*_s_dOut_5)[int(7)];
    float _S1841 = _S1763 * _S1840;
    float _S1842 = _S1841 + _S1841;
    float _S1843 = _S1762 * _S1840;
    float _S1844 = _S1843 + _S1843;
    float _S1845 = _S1761 * _S1840;
    float _S1846 = _S1845 + _S1845;
    float _S1847 = 0.3333333432674408f * (- _S1842 + - _S1844 + - _S1846);
    float _S1848 = 0.3333333432674408f * (*_s_dOut_5)[int(6)];
    float _S1849 = _S1760 * _S1848;
    float _S1850 = _S1849 + _S1849;
    float _S1851 = _S1759 * _S1848;
    float _S1852 = _S1851 + _S1851;
    float _S1853 = _S1758 * _S1848;
    float _S1854 = _S1853 + _S1853;
    float _S1855 = 0.3333333432674408f * (- _S1850 + - _S1852 + - _S1854);
    float _S1856 = 0.3333333432674408f * (*_s_dOut_5)[int(5)];
    float _S1857 = _S1757 * _S1856;
    float _S1858 = _S1857 + _S1857;
    float _S1859 = _S1756 * _S1856;
    float _S1860 = _S1859 + _S1859;
    float _S1861 = _S1755 * _S1856;
    float _S1862 = _S1861 + _S1861;
    float _S1863 = 0.3333333432674408f * (- _S1858 + - _S1860 + - _S1862);
    DiffPair_float_0 _S1864;
    (&_S1864)->primal_0 = 0.0f;
    (&_S1864)->differential_0 = 0.0f;
    DiffPair_float_0 _S1865;
    (&_S1865)->primal_0 = dpparams_5->primal_0[int(15)];
    (&_S1865)->differential_0 = 0.0f;
    _d_max_0(&_S1864, &_S1865, (*_s_dOut_5)[int(4)]);
    DiffPair_float_0 _S1866;
    (&_S1866)->primal_0 = 0.0f;
    (&_S1866)->differential_0 = 0.0f;
    DiffPair_float_0 _S1867;
    (&_S1867)->primal_0 = dpparams_5->primal_0[int(10)];
    (&_S1867)->differential_0 = 0.0f;
    _d_max_0(&_S1866, &_S1867, (*_s_dOut_5)[int(4)]);
    DiffPair_float_0 _S1868;
    (&_S1868)->primal_0 = 0.0f;
    (&_S1868)->differential_0 = 0.0f;
    DiffPair_float_0 _S1869;
    (&_S1869)->primal_0 = dpparams_5->primal_0[int(5)];
    (&_S1869)->differential_0 = 0.0f;
    _d_max_0(&_S1868, &_S1869, (*_s_dOut_5)[int(4)]);
    DiffPair_float_0 _S1870;
    (&_S1870)->primal_0 = 0.0f;
    (&_S1870)->differential_0 = 0.0f;
    DiffPair_float_0 _S1871;
    (&_S1871)->primal_0 = dpparams_5->primal_0[int(14)];
    (&_S1871)->differential_0 = 0.0f;
    _d_max_0(&_S1870, &_S1871, (*_s_dOut_5)[int(3)]);
    DiffPair_float_0 _S1872;
    (&_S1872)->primal_0 = 0.0f;
    (&_S1872)->differential_0 = 0.0f;
    DiffPair_float_0 _S1873;
    (&_S1873)->primal_0 = dpparams_5->primal_0[int(9)];
    (&_S1873)->differential_0 = 0.0f;
    _d_max_0(&_S1872, &_S1873, (*_s_dOut_5)[int(3)]);
    DiffPair_float_0 _S1874;
    (&_S1874)->primal_0 = 0.0f;
    (&_S1874)->differential_0 = 0.0f;
    DiffPair_float_0 _S1875;
    (&_S1875)->primal_0 = dpparams_5->primal_0[int(4)];
    (&_S1875)->differential_0 = 0.0f;
    _d_max_0(&_S1874, &_S1875, (*_s_dOut_5)[int(3)]);
    DiffPair_float_0 _S1876;
    (&_S1876)->primal_0 = 0.0f;
    (&_S1876)->differential_0 = 0.0f;
    DiffPair_float_0 _S1877;
    (&_S1877)->primal_0 = dpparams_5->primal_0[int(13)];
    (&_S1877)->differential_0 = 0.0f;
    _d_max_0(&_S1876, &_S1877, (*_s_dOut_5)[int(2)]);
    DiffPair_float_0 _S1878;
    (&_S1878)->primal_0 = 0.0f;
    (&_S1878)->differential_0 = 0.0f;
    DiffPair_float_0 _S1879;
    (&_S1879)->primal_0 = dpparams_5->primal_0[int(8)];
    (&_S1879)->differential_0 = 0.0f;
    _d_max_0(&_S1878, &_S1879, (*_s_dOut_5)[int(2)]);
    DiffPair_float_0 _S1880;
    (&_S1880)->primal_0 = 0.0f;
    (&_S1880)->differential_0 = 0.0f;
    DiffPair_float_0 _S1881;
    (&_S1881)->primal_0 = dpparams_5->primal_0[int(3)];
    (&_S1881)->differential_0 = 0.0f;
    _d_max_0(&_S1880, &_S1881, (*_s_dOut_5)[int(2)]);
    float _S1882 = dpparams_5->primal_0[int(12)] * (*_s_dOut_5)[int(1)];
    float _S1883 = dpparams_5->primal_0[int(11)] * (*_s_dOut_5)[int(1)];
    float _S1884 = dpparams_5->primal_0[int(7)] * (*_s_dOut_5)[int(1)];
    float _S1885 = dpparams_5->primal_0[int(6)] * (*_s_dOut_5)[int(1)];
    float _S1886 = dpparams_5->primal_0[int(2)] * (*_s_dOut_5)[int(1)];
    float _S1887 = dpparams_5->primal_0[int(1)] * (*_s_dOut_5)[int(1)];
    PPISPParamsRQS_0 _S1888 = PPISPParamsRQS_x24_syn_dzero_0();
    (&_S1888)->color_params_2 = _S1823;
    (&_S1888)->exposure_2 = (*_s_dOut_5)[int(0)];
    _S1754 = _S1888;
    (&(&_S1754)->crf_params_0[int(2)])->gc_0 = 0.0f;
    float _S1889 = _S1772 + _S1777 + _S1888.crf_params_0[int(2)].gc_0;
    (&(&_S1754)->crf_params_0[int(2)])->y0_0 = 0.0f;
    float _S1890 = _S1780 + _S1785 + _S1888.crf_params_0[int(2)].y0_0;
    (&(&_S1754)->crf_params_0[int(2)])->x0_0 = 0.0f;
    float _S1891 = _S1788 + _S1793 + _S1888.crf_params_0[int(2)].x0_0;
    (&(&_S1754)->crf_params_0[int(2)])->g1_0 = 0.0f;
    float _S1892 = _S1796 + _S1801 + _S1888.crf_params_0[int(2)].g1_0;
    (&(&_S1754)->crf_params_0[int(2)])->g0_0 = 0.0f;
    float _S1893 = _S1804 + _S1809 + _S1888.crf_params_0[int(2)].g0_0;
    (&(&_S1754)->crf_params_0[int(1)])->gc_0 = 0.0f;
    float _S1894 = _S1774 + _S1777 + _S1888.crf_params_0[int(1)].gc_0;
    (&(&_S1754)->crf_params_0[int(1)])->y0_0 = 0.0f;
    float _S1895 = _S1782 + _S1785 + _S1888.crf_params_0[int(1)].y0_0;
    (&(&_S1754)->crf_params_0[int(1)])->x0_0 = 0.0f;
    float _S1896 = _S1790 + _S1793 + _S1888.crf_params_0[int(1)].x0_0;
    (&(&_S1754)->crf_params_0[int(1)])->g1_0 = 0.0f;
    float _S1897 = _S1798 + _S1801 + _S1888.crf_params_0[int(1)].g1_0;
    (&(&_S1754)->crf_params_0[int(1)])->g0_0 = 0.0f;
    float _S1898 = _S1806 + _S1809 + _S1888.crf_params_0[int(1)].g0_0;
    (&(&_S1754)->crf_params_0[int(0)])->gc_0 = 0.0f;
    float _S1899 = _S1776 + _S1777 + _S1888.crf_params_0[int(0)].gc_0;
    (&(&_S1754)->crf_params_0[int(0)])->y0_0 = 0.0f;
    float _S1900 = _S1784 + _S1785 + _S1888.crf_params_0[int(0)].y0_0;
    (&(&_S1754)->crf_params_0[int(0)])->x0_0 = 0.0f;
    float _S1901 = _S1792 + _S1793 + _S1888.crf_params_0[int(0)].x0_0;
    (&(&_S1754)->crf_params_0[int(0)])->g1_0 = 0.0f;
    float _S1902 = _S1800 + _S1801 + _S1888.crf_params_0[int(0)].g1_0;
    (&(&_S1754)->crf_params_0[int(0)])->g0_0 = 0.0f;
    float _S1903 = _S1808 + _S1809 + _S1888.crf_params_0[int(0)].g0_0;
    *&((&(&(&_S1754)->color_params_2)->n_0)->y) = 0.0f;
    *&((&(&(&_S1754)->color_params_2)->n_0)->x) = 0.0f;
    *&((&(&(&_S1754)->color_params_2)->g_0)->y) = 0.0f;
    *&((&(&(&_S1754)->color_params_2)->g_0)->x) = 0.0f;
    *&((&(&(&_S1754)->color_params_2)->r_0)->y) = 0.0f;
    *&((&(&(&_S1754)->color_params_2)->r_0)->x) = 0.0f;
    *&((&(&(&_S1754)->color_params_2)->b_0)->y) = 0.0f;
    *&((&(&(&_S1754)->color_params_2)->b_0)->x) = 0.0f;
    (&(&_S1754)->vignette_params_1[int(2)])->alpha2_0 = 0.0f;
    float _S1904 = _S1826 + _S1831 + _S1865.differential_0 + _S1888.vignette_params_1[int(2)].alpha2_0;
    (&(&_S1754)->vignette_params_1[int(2)])->alpha1_0 = 0.0f;
    float _S1905 = _S1834 + _S1839 + _S1871.differential_0 + _S1888.vignette_params_1[int(2)].alpha1_0;
    (&(&_S1754)->vignette_params_1[int(2)])->alpha0_0 = 0.0f;
    float _S1906 = _S1842 + _S1847 + _S1877.differential_0 + _S1888.vignette_params_1[int(2)].alpha0_0;
    (&(&_S1754)->vignette_params_1[int(2)])->cy_0 = 0.0f;
    float _S1907 = _S1850 + _S1855 + _S1882 + _S1882 + _S1888.vignette_params_1[int(2)].cy_0;
    (&(&_S1754)->vignette_params_1[int(2)])->cx_0 = 0.0f;
    float _S1908 = _S1858 + _S1863 + _S1883 + _S1883 + _S1888.vignette_params_1[int(2)].cx_0;
    (&(&_S1754)->vignette_params_1[int(1)])->alpha2_0 = 0.0f;
    float _S1909 = _S1828 + _S1831 + _S1867.differential_0 + _S1888.vignette_params_1[int(1)].alpha2_0;
    (&(&_S1754)->vignette_params_1[int(1)])->alpha1_0 = 0.0f;
    float _S1910 = _S1836 + _S1839 + _S1873.differential_0 + _S1888.vignette_params_1[int(1)].alpha1_0;
    (&(&_S1754)->vignette_params_1[int(1)])->alpha0_0 = 0.0f;
    float _S1911 = _S1844 + _S1847 + _S1879.differential_0 + _S1888.vignette_params_1[int(1)].alpha0_0;
    (&(&_S1754)->vignette_params_1[int(1)])->cy_0 = 0.0f;
    float _S1912 = _S1852 + _S1855 + _S1884 + _S1884 + _S1888.vignette_params_1[int(1)].cy_0;
    (&(&_S1754)->vignette_params_1[int(1)])->cx_0 = 0.0f;
    float _S1913 = _S1860 + _S1863 + _S1885 + _S1885 + _S1888.vignette_params_1[int(1)].cx_0;
    (&(&_S1754)->vignette_params_1[int(0)])->alpha2_0 = 0.0f;
    float _S1914 = _S1830 + _S1831 + _S1869.differential_0 + _S1888.vignette_params_1[int(0)].alpha2_0;
    (&(&_S1754)->vignette_params_1[int(0)])->alpha1_0 = 0.0f;
    float _S1915 = _S1838 + _S1839 + _S1875.differential_0 + _S1888.vignette_params_1[int(0)].alpha1_0;
    (&(&_S1754)->vignette_params_1[int(0)])->alpha0_0 = 0.0f;
    float _S1916 = _S1846 + _S1847 + _S1881.differential_0 + _S1888.vignette_params_1[int(0)].alpha0_0;
    (&(&_S1754)->vignette_params_1[int(0)])->cy_0 = 0.0f;
    float _S1917 = _S1854 + _S1855 + _S1886 + _S1886 + _S1888.vignette_params_1[int(0)].cy_0;
    (&(&_S1754)->vignette_params_1[int(0)])->cx_0 = 0.0f;
    float _S1918 = _S1862 + _S1863 + _S1887 + _S1887 + _S1888.vignette_params_1[int(0)].cx_0;
    FixedArray<float, 39>  _S1919;
    _S1919[int(0)] = 0.0f;
    _S1919[int(1)] = 0.0f;
    _S1919[int(2)] = 0.0f;
    _S1919[int(3)] = 0.0f;
    _S1919[int(4)] = 0.0f;
    _S1919[int(5)] = 0.0f;
    _S1919[int(6)] = 0.0f;
    _S1919[int(7)] = 0.0f;
    _S1919[int(8)] = 0.0f;
    _S1919[int(9)] = 0.0f;
    _S1919[int(10)] = 0.0f;
    _S1919[int(11)] = 0.0f;
    _S1919[int(12)] = 0.0f;
    _S1919[int(13)] = 0.0f;
    _S1919[int(14)] = 0.0f;
    _S1919[int(15)] = 0.0f;
    _S1919[int(16)] = 0.0f;
    _S1919[int(17)] = 0.0f;
    _S1919[int(18)] = 0.0f;
    _S1919[int(19)] = 0.0f;
    _S1919[int(20)] = 0.0f;
    _S1919[int(21)] = 0.0f;
    _S1919[int(22)] = 0.0f;
    _S1919[int(23)] = 0.0f;
    _S1919[int(24)] = 0.0f;
    _S1919[int(25)] = 0.0f;
    _S1919[int(26)] = 0.0f;
    _S1919[int(27)] = 0.0f;
    _S1919[int(28)] = 0.0f;
    _S1919[int(29)] = 0.0f;
    _S1919[int(30)] = 0.0f;
    _S1919[int(31)] = 0.0f;
    _S1919[int(32)] = 0.0f;
    _S1919[int(33)] = 0.0f;
    _S1919[int(34)] = 0.0f;
    _S1919[int(35)] = 0.0f;
    _S1919[int(36)] = 0.0f;
    _S1919[int(37)] = 0.0f;
    _S1919[int(38)] = 0.0f;
    _S1919[int(9)] = _S1910;
    _S1919[int(18)] = _S1888.color_params_2.r_0.x;
    _S1919[int(17)] = _S1888.color_params_2.b_0.y;
    _S1919[int(16)] = _S1888.color_params_2.b_0.x;
    _S1919[int(15)] = _S1904;
    _S1919[int(14)] = _S1905;
    _S1919[int(13)] = _S1906;
    _S1919[int(12)] = _S1907;
    _S1919[int(11)] = _S1908;
    _S1919[int(10)] = _S1909;
    _S1919[int(19)] = _S1888.color_params_2.r_0.y;
    _S1919[int(8)] = _S1911;
    _S1919[int(7)] = _S1912;
    _S1919[int(6)] = _S1913;
    _S1919[int(5)] = _S1914;
    _S1919[int(4)] = _S1915;
    _S1919[int(3)] = _S1916;
    _S1919[int(2)] = _S1917;
    _S1919[int(1)] = _S1918;
    _S1919[int(0)] = _S1754.exposure_2;
    _S1919[int(28)] = _S1899;
    _S1919[int(37)] = _S1890;
    _S1919[int(36)] = _S1891;
    _S1919[int(35)] = _S1892;
    _S1919[int(34)] = _S1893;
    _S1919[int(33)] = _S1894;
    _S1919[int(32)] = _S1895;
    _S1919[int(31)] = _S1896;
    _S1919[int(30)] = _S1897;
    _S1919[int(29)] = _S1898;
    _S1919[int(38)] = _S1889;
    _S1919[int(27)] = _S1900;
    _S1919[int(26)] = _S1901;
    _S1919[int(25)] = _S1902;
    _S1919[int(24)] = _S1903;
    _S1919[int(23)] = _S1888.color_params_2.n_0.y;
    _S1919[int(22)] = _S1888.color_params_2.n_0.x;
    _S1919[int(21)] = _S1888.color_params_2.g_0.y;
    _S1919[int(20)] = _S1888.color_params_2.g_0.x;
    dpparams_5->primal_0 = dpparams_5->primal_0;
    dpparams_5->differential_0 = _S1919;
    return;
}

inline __device__ void s_bwd_compute_raw_ppisp_rqs_regularization_loss_0(DiffPair_arrayx3Cfloatx2C39x3E_0 * _S1920, FixedArray<float, 23>  * _S1921)
{
    s_bwd_prop_compute_raw_ppisp_rqs_regularization_loss_0(_S1920, _S1921);
    return;
}

inline __device__ void compute_raw_ppisp_rqs_regularization_loss_vjp(FixedArray<float, 39>  params_11, FixedArray<float, 23>  grad_out_5, FixedArray<float, 39>  * _S1922)
{
    FixedArray<float, 39>  _S1923 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    DiffPair_arrayx3Cfloatx2C39x3E_0 dp_params_5;
    (&dp_params_5)->primal_0 = params_11;
    (&dp_params_5)->differential_0 = _S1923;
    FixedArray<float, 23>  _S1924 = grad_out_5;
    s_bwd_compute_raw_ppisp_rqs_regularization_loss_0(&dp_params_5, &_S1924);
    *_S1922 = (&dp_params_5)->differential_0;
    return;
}

inline __device__ void compute_raw_ppisp_no_crf_regularization_loss(FixedArray<float, 24>  params_12, FixedArray<float, 18>  * _S1925)
{
    PPISPParamsNoCRF_0 p_6;
    (&p_6)->exposure_1 = params_12[int(0)];
    (&(&p_6)->vignette_params_0[int(0)])->cx_0 = params_12[int(1)];
    (&(&p_6)->vignette_params_0[int(0)])->cy_0 = params_12[int(2)];
    (&(&p_6)->vignette_params_0[int(0)])->alpha0_0 = params_12[int(3)];
    (&(&p_6)->vignette_params_0[int(0)])->alpha1_0 = params_12[int(4)];
    (&(&p_6)->vignette_params_0[int(0)])->alpha2_0 = params_12[int(5)];
    (&(&p_6)->vignette_params_0[int(1)])->cx_0 = params_12[int(6)];
    (&(&p_6)->vignette_params_0[int(1)])->cy_0 = params_12[int(7)];
    (&(&p_6)->vignette_params_0[int(1)])->alpha0_0 = params_12[int(8)];
    (&(&p_6)->vignette_params_0[int(1)])->alpha1_0 = params_12[int(9)];
    (&(&p_6)->vignette_params_0[int(1)])->alpha2_0 = params_12[int(10)];
    (&(&p_6)->vignette_params_0[int(2)])->cx_0 = params_12[int(11)];
    (&(&p_6)->vignette_params_0[int(2)])->cy_0 = params_12[int(12)];
    (&(&p_6)->vignette_params_0[int(2)])->alpha0_0 = params_12[int(13)];
    (&(&p_6)->vignette_params_0[int(2)])->alpha1_0 = params_12[int(14)];
    (&(&p_6)->vignette_params_0[int(2)])->alpha2_0 = params_12[int(15)];
    *&((&(&(&p_6)->color_params_1)->b_0)->x) = params_12[int(16)];
    *&((&(&(&p_6)->color_params_1)->b_0)->y) = params_12[int(17)];
    *&((&(&(&p_6)->color_params_1)->r_0)->x) = params_12[int(18)];
    *&((&(&(&p_6)->color_params_1)->r_0)->y) = params_12[int(19)];
    *&((&(&(&p_6)->color_params_1)->g_0)->x) = params_12[int(20)];
    *&((&(&(&p_6)->color_params_1)->g_0)->y) = params_12[int(21)];
    *&((&(&(&p_6)->color_params_1)->n_0)->x) = params_12[int(22)];
    *&((&(&(&p_6)->color_params_1)->n_0)->y) = params_12[int(23)];
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
    losses_2[int(0)] = p_6.exposure_1;
    float _S1926 = p_6.vignette_params_0[int(0)].cx_0;
    float _S1927 = p_6.vignette_params_0[int(0)].cy_0;
    float _S1928 = p_6.vignette_params_0[int(1)].cx_0;
    float _S1929 = p_6.vignette_params_0[int(1)].cy_0;
    float _S1930 = p_6.vignette_params_0[int(2)].cx_0;
    float _S1931 = p_6.vignette_params_0[int(2)].cy_0;
    losses_2[int(1)] = _S1926 * _S1926 + _S1927 * _S1927 + _S1928 * _S1928 + _S1929 * _S1929 + _S1930 * _S1930 + _S1931 * _S1931;
    losses_2[int(2)] = (F32_max((0.0f), (p_6.vignette_params_0[int(0)].alpha0_0))) + (F32_max((0.0f), (p_6.vignette_params_0[int(1)].alpha0_0))) + (F32_max((0.0f), (p_6.vignette_params_0[int(2)].alpha0_0)));
    losses_2[int(3)] = (F32_max((0.0f), (p_6.vignette_params_0[int(0)].alpha1_0))) + (F32_max((0.0f), (p_6.vignette_params_0[int(1)].alpha1_0))) + (F32_max((0.0f), (p_6.vignette_params_0[int(2)].alpha1_0)));
    losses_2[int(4)] = (F32_max((0.0f), (p_6.vignette_params_0[int(0)].alpha2_0))) + (F32_max((0.0f), (p_6.vignette_params_0[int(1)].alpha2_0))) + (F32_max((0.0f), (p_6.vignette_params_0[int(2)].alpha2_0)));
    float mean_38 = (p_6.vignette_params_0[int(0)].cx_0 + p_6.vignette_params_0[int(1)].cx_0 + p_6.vignette_params_0[int(2)].cx_0) / 3.0f;
    float _S1932 = p_6.vignette_params_0[int(0)].cx_0 - mean_38;
    float _S1933 = p_6.vignette_params_0[int(1)].cx_0 - mean_38;
    float _S1934 = p_6.vignette_params_0[int(2)].cx_0 - mean_38;
    losses_2[int(5)] = (_S1932 * _S1932 + _S1933 * _S1933 + _S1934 * _S1934) / 3.0f;
    float mean_39 = (p_6.vignette_params_0[int(0)].cy_0 + p_6.vignette_params_0[int(1)].cy_0 + p_6.vignette_params_0[int(2)].cy_0) / 3.0f;
    float _S1935 = p_6.vignette_params_0[int(0)].cy_0 - mean_39;
    float _S1936 = p_6.vignette_params_0[int(1)].cy_0 - mean_39;
    float _S1937 = p_6.vignette_params_0[int(2)].cy_0 - mean_39;
    losses_2[int(6)] = (_S1935 * _S1935 + _S1936 * _S1936 + _S1937 * _S1937) / 3.0f;
    float mean_40 = (p_6.vignette_params_0[int(0)].alpha0_0 + p_6.vignette_params_0[int(1)].alpha0_0 + p_6.vignette_params_0[int(2)].alpha0_0) / 3.0f;
    float _S1938 = p_6.vignette_params_0[int(0)].alpha0_0 - mean_40;
    float _S1939 = p_6.vignette_params_0[int(1)].alpha0_0 - mean_40;
    float _S1940 = p_6.vignette_params_0[int(2)].alpha0_0 - mean_40;
    losses_2[int(7)] = (_S1938 * _S1938 + _S1939 * _S1939 + _S1940 * _S1940) / 3.0f;
    float mean_41 = (p_6.vignette_params_0[int(0)].alpha1_0 + p_6.vignette_params_0[int(1)].alpha1_0 + p_6.vignette_params_0[int(2)].alpha1_0) / 3.0f;
    float _S1941 = p_6.vignette_params_0[int(0)].alpha1_0 - mean_41;
    float _S1942 = p_6.vignette_params_0[int(1)].alpha1_0 - mean_41;
    float _S1943 = p_6.vignette_params_0[int(2)].alpha1_0 - mean_41;
    losses_2[int(8)] = (_S1941 * _S1941 + _S1942 * _S1942 + _S1943 * _S1943) / 3.0f;
    float mean_42 = (p_6.vignette_params_0[int(0)].alpha2_0 + p_6.vignette_params_0[int(1)].alpha2_0 + p_6.vignette_params_0[int(2)].alpha2_0) / 3.0f;
    float _S1944 = p_6.vignette_params_0[int(0)].alpha2_0 - mean_42;
    float _S1945 = p_6.vignette_params_0[int(1)].alpha2_0 - mean_42;
    float _S1946 = p_6.vignette_params_0[int(2)].alpha2_0 - mean_42;
    losses_2[int(9)] = (_S1944 * _S1944 + _S1945 * _S1945 + _S1946 * _S1946) / 3.0f;
    float2  bd_6 = mul_0(makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f), p_6.color_params_1.b_0);
    float2  rd_6 = mul_0(makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f), p_6.color_params_1.r_0);
    float2  gd_6 = mul_0(makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f), p_6.color_params_1.g_0);
    float2  nd_6 = mul_0(makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f), p_6.color_params_1.n_0);
    losses_2[int(10)] = bd_6.x;
    losses_2[int(11)] = bd_6.y;
    losses_2[int(12)] = rd_6.x;
    losses_2[int(13)] = rd_6.y;
    losses_2[int(14)] = gd_6.x;
    losses_2[int(15)] = gd_6.y;
    losses_2[int(16)] = nd_6.x;
    losses_2[int(17)] = nd_6.y;
    *_S1925 = losses_2;
    return;
}

inline __device__ void s_bwd_prop_compute_raw_ppisp_no_crf_regularization_loss_0(DiffPair_arrayx3Cfloatx2C24x3E_0 * dpparams_6, FixedArray<float, 18>  * _s_dOut_6)
{
    VignettingChannelParams_0 _S1947 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    FixedArray<VignettingChannelParams_0, 3>  _S1948 = {
        _S1947, _S1947, _S1947
    };
    float2  _S1949 = make_float2 (0.0f);
    ColorPPISPParams_0 _S1950 = { _S1949, _S1949, _S1949, _S1949 };
    PPISPParamsNoCRF_0 _S1951;
    (&_S1951)->exposure_1 = dpparams_6->primal_0[int(0)];
    (&_S1951)->vignette_params_0 = _S1948;
    (&_S1951)->color_params_1 = _S1950;
    (&(&_S1951)->vignette_params_0[int(0)])->cx_0 = dpparams_6->primal_0[int(1)];
    (&(&_S1951)->vignette_params_0[int(0)])->cy_0 = dpparams_6->primal_0[int(2)];
    (&(&_S1951)->vignette_params_0[int(0)])->alpha0_0 = dpparams_6->primal_0[int(3)];
    (&(&_S1951)->vignette_params_0[int(0)])->alpha1_0 = dpparams_6->primal_0[int(4)];
    (&(&_S1951)->vignette_params_0[int(0)])->alpha2_0 = dpparams_6->primal_0[int(5)];
    (&(&_S1951)->vignette_params_0[int(1)])->cx_0 = dpparams_6->primal_0[int(6)];
    (&(&_S1951)->vignette_params_0[int(1)])->cy_0 = dpparams_6->primal_0[int(7)];
    (&(&_S1951)->vignette_params_0[int(1)])->alpha0_0 = dpparams_6->primal_0[int(8)];
    (&(&_S1951)->vignette_params_0[int(1)])->alpha1_0 = dpparams_6->primal_0[int(9)];
    (&(&_S1951)->vignette_params_0[int(1)])->alpha2_0 = dpparams_6->primal_0[int(10)];
    (&(&_S1951)->vignette_params_0[int(2)])->cx_0 = dpparams_6->primal_0[int(11)];
    (&(&_S1951)->vignette_params_0[int(2)])->cy_0 = dpparams_6->primal_0[int(12)];
    (&(&_S1951)->vignette_params_0[int(2)])->alpha0_0 = dpparams_6->primal_0[int(13)];
    (&(&_S1951)->vignette_params_0[int(2)])->alpha1_0 = dpparams_6->primal_0[int(14)];
    (&(&_S1951)->vignette_params_0[int(2)])->alpha2_0 = dpparams_6->primal_0[int(15)];
    *&((&(&(&_S1951)->color_params_1)->b_0)->x) = dpparams_6->primal_0[int(16)];
    *&((&(&(&_S1951)->color_params_1)->b_0)->y) = dpparams_6->primal_0[int(17)];
    *&((&(&(&_S1951)->color_params_1)->r_0)->x) = dpparams_6->primal_0[int(18)];
    *&((&(&(&_S1951)->color_params_1)->r_0)->y) = dpparams_6->primal_0[int(19)];
    *&((&(&(&_S1951)->color_params_1)->g_0)->x) = dpparams_6->primal_0[int(20)];
    *&((&(&(&_S1951)->color_params_1)->g_0)->y) = dpparams_6->primal_0[int(21)];
    *&((&(&(&_S1951)->color_params_1)->n_0)->x) = dpparams_6->primal_0[int(22)];
    *&((&(&(&_S1951)->color_params_1)->n_0)->y) = dpparams_6->primal_0[int(23)];
    float mean_43 = (dpparams_6->primal_0[int(1)] + dpparams_6->primal_0[int(6)] + dpparams_6->primal_0[int(11)]) / 3.0f;
    float _S1952 = dpparams_6->primal_0[int(1)] - mean_43;
    float _S1953 = dpparams_6->primal_0[int(6)] - mean_43;
    float _S1954 = dpparams_6->primal_0[int(11)] - mean_43;
    float mean_44 = (dpparams_6->primal_0[int(2)] + dpparams_6->primal_0[int(7)] + dpparams_6->primal_0[int(12)]) / 3.0f;
    float _S1955 = dpparams_6->primal_0[int(2)] - mean_44;
    float _S1956 = dpparams_6->primal_0[int(7)] - mean_44;
    float _S1957 = dpparams_6->primal_0[int(12)] - mean_44;
    float mean_45 = (dpparams_6->primal_0[int(3)] + dpparams_6->primal_0[int(8)] + dpparams_6->primal_0[int(13)]) / 3.0f;
    float _S1958 = dpparams_6->primal_0[int(3)] - mean_45;
    float _S1959 = dpparams_6->primal_0[int(8)] - mean_45;
    float _S1960 = dpparams_6->primal_0[int(13)] - mean_45;
    float mean_46 = (dpparams_6->primal_0[int(4)] + dpparams_6->primal_0[int(9)] + dpparams_6->primal_0[int(14)]) / 3.0f;
    float _S1961 = dpparams_6->primal_0[int(4)] - mean_46;
    float _S1962 = dpparams_6->primal_0[int(9)] - mean_46;
    float _S1963 = dpparams_6->primal_0[int(14)] - mean_46;
    float mean_47 = (dpparams_6->primal_0[int(5)] + dpparams_6->primal_0[int(10)] + dpparams_6->primal_0[int(15)]) / 3.0f;
    float _S1964 = dpparams_6->primal_0[int(5)] - mean_47;
    float _S1965 = dpparams_6->primal_0[int(10)] - mean_47;
    float _S1966 = dpparams_6->primal_0[int(15)] - mean_47;
    float2  _S1967 = make_float2 ((*_s_dOut_6)[int(16)], (*_s_dOut_6)[int(17)]);
    Matrix<float, 2, 2>  _S1968 = makeMatrix<float, 2, 2> (0.0f);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1969;
    (&_S1969)->primal_0 = makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f);
    (&_S1969)->differential_0 = _S1968;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1970;
    (&_S1970)->primal_0 = _S1951.color_params_1.n_0;
    (&_S1970)->differential_0 = _S1949;
    s_bwd_prop_mul_2(&_S1969, &_S1970, _S1967);
    float2  _S1971 = make_float2 ((*_s_dOut_6)[int(14)], (*_s_dOut_6)[int(15)]);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1972;
    (&_S1972)->primal_0 = makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f);
    (&_S1972)->differential_0 = _S1968;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1973;
    (&_S1973)->primal_0 = _S1951.color_params_1.g_0;
    (&_S1973)->differential_0 = _S1949;
    s_bwd_prop_mul_2(&_S1972, &_S1973, _S1971);
    float2  _S1974 = make_float2 ((*_s_dOut_6)[int(12)], (*_s_dOut_6)[int(13)]);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1975;
    (&_S1975)->primal_0 = makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f);
    (&_S1975)->differential_0 = _S1968;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1976;
    (&_S1976)->primal_0 = _S1951.color_params_1.r_0;
    (&_S1976)->differential_0 = _S1949;
    s_bwd_prop_mul_2(&_S1975, &_S1976, _S1974);
    float2  _S1977 = make_float2 ((*_s_dOut_6)[int(10)], (*_s_dOut_6)[int(11)]);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S1978;
    (&_S1978)->primal_0 = makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f);
    (&_S1978)->differential_0 = _S1968;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1979;
    (&_S1979)->primal_0 = _S1951.color_params_1.b_0;
    (&_S1979)->differential_0 = _S1949;
    s_bwd_prop_mul_2(&_S1978, &_S1979, _S1977);
    ColorPPISPParams_0 _S1980 = ColorPPISPParams_x24_syn_dzero_0();
    (&_S1980)->n_0 = _S1970.differential_0;
    (&_S1980)->g_0 = _S1973.differential_0;
    (&_S1980)->r_0 = _S1976.differential_0;
    (&_S1980)->b_0 = _S1979.differential_0;
    float _S1981 = 0.3333333432674408f * (*_s_dOut_6)[int(9)];
    float _S1982 = _S1966 * _S1981;
    float _S1983 = _S1982 + _S1982;
    float _S1984 = _S1965 * _S1981;
    float _S1985 = _S1984 + _S1984;
    float _S1986 = _S1964 * _S1981;
    float _S1987 = _S1986 + _S1986;
    float _S1988 = 0.3333333432674408f * (- _S1983 + - _S1985 + - _S1987);
    float _S1989 = 0.3333333432674408f * (*_s_dOut_6)[int(8)];
    float _S1990 = _S1963 * _S1989;
    float _S1991 = _S1990 + _S1990;
    float _S1992 = _S1962 * _S1989;
    float _S1993 = _S1992 + _S1992;
    float _S1994 = _S1961 * _S1989;
    float _S1995 = _S1994 + _S1994;
    float _S1996 = 0.3333333432674408f * (- _S1991 + - _S1993 + - _S1995);
    float _S1997 = 0.3333333432674408f * (*_s_dOut_6)[int(7)];
    float _S1998 = _S1960 * _S1997;
    float _S1999 = _S1998 + _S1998;
    float _S2000 = _S1959 * _S1997;
    float _S2001 = _S2000 + _S2000;
    float _S2002 = _S1958 * _S1997;
    float _S2003 = _S2002 + _S2002;
    float _S2004 = 0.3333333432674408f * (- _S1999 + - _S2001 + - _S2003);
    float _S2005 = 0.3333333432674408f * (*_s_dOut_6)[int(6)];
    float _S2006 = _S1957 * _S2005;
    float _S2007 = _S2006 + _S2006;
    float _S2008 = _S1956 * _S2005;
    float _S2009 = _S2008 + _S2008;
    float _S2010 = _S1955 * _S2005;
    float _S2011 = _S2010 + _S2010;
    float _S2012 = 0.3333333432674408f * (- _S2007 + - _S2009 + - _S2011);
    float _S2013 = 0.3333333432674408f * (*_s_dOut_6)[int(5)];
    float _S2014 = _S1954 * _S2013;
    float _S2015 = _S2014 + _S2014;
    float _S2016 = _S1953 * _S2013;
    float _S2017 = _S2016 + _S2016;
    float _S2018 = _S1952 * _S2013;
    float _S2019 = _S2018 + _S2018;
    float _S2020 = 0.3333333432674408f * (- _S2015 + - _S2017 + - _S2019);
    DiffPair_float_0 _S2021;
    (&_S2021)->primal_0 = 0.0f;
    (&_S2021)->differential_0 = 0.0f;
    DiffPair_float_0 _S2022;
    (&_S2022)->primal_0 = dpparams_6->primal_0[int(15)];
    (&_S2022)->differential_0 = 0.0f;
    _d_max_0(&_S2021, &_S2022, (*_s_dOut_6)[int(4)]);
    DiffPair_float_0 _S2023;
    (&_S2023)->primal_0 = 0.0f;
    (&_S2023)->differential_0 = 0.0f;
    DiffPair_float_0 _S2024;
    (&_S2024)->primal_0 = dpparams_6->primal_0[int(10)];
    (&_S2024)->differential_0 = 0.0f;
    _d_max_0(&_S2023, &_S2024, (*_s_dOut_6)[int(4)]);
    DiffPair_float_0 _S2025;
    (&_S2025)->primal_0 = 0.0f;
    (&_S2025)->differential_0 = 0.0f;
    DiffPair_float_0 _S2026;
    (&_S2026)->primal_0 = dpparams_6->primal_0[int(5)];
    (&_S2026)->differential_0 = 0.0f;
    _d_max_0(&_S2025, &_S2026, (*_s_dOut_6)[int(4)]);
    DiffPair_float_0 _S2027;
    (&_S2027)->primal_0 = 0.0f;
    (&_S2027)->differential_0 = 0.0f;
    DiffPair_float_0 _S2028;
    (&_S2028)->primal_0 = dpparams_6->primal_0[int(14)];
    (&_S2028)->differential_0 = 0.0f;
    _d_max_0(&_S2027, &_S2028, (*_s_dOut_6)[int(3)]);
    DiffPair_float_0 _S2029;
    (&_S2029)->primal_0 = 0.0f;
    (&_S2029)->differential_0 = 0.0f;
    DiffPair_float_0 _S2030;
    (&_S2030)->primal_0 = dpparams_6->primal_0[int(9)];
    (&_S2030)->differential_0 = 0.0f;
    _d_max_0(&_S2029, &_S2030, (*_s_dOut_6)[int(3)]);
    DiffPair_float_0 _S2031;
    (&_S2031)->primal_0 = 0.0f;
    (&_S2031)->differential_0 = 0.0f;
    DiffPair_float_0 _S2032;
    (&_S2032)->primal_0 = dpparams_6->primal_0[int(4)];
    (&_S2032)->differential_0 = 0.0f;
    _d_max_0(&_S2031, &_S2032, (*_s_dOut_6)[int(3)]);
    DiffPair_float_0 _S2033;
    (&_S2033)->primal_0 = 0.0f;
    (&_S2033)->differential_0 = 0.0f;
    DiffPair_float_0 _S2034;
    (&_S2034)->primal_0 = dpparams_6->primal_0[int(13)];
    (&_S2034)->differential_0 = 0.0f;
    _d_max_0(&_S2033, &_S2034, (*_s_dOut_6)[int(2)]);
    DiffPair_float_0 _S2035;
    (&_S2035)->primal_0 = 0.0f;
    (&_S2035)->differential_0 = 0.0f;
    DiffPair_float_0 _S2036;
    (&_S2036)->primal_0 = dpparams_6->primal_0[int(8)];
    (&_S2036)->differential_0 = 0.0f;
    _d_max_0(&_S2035, &_S2036, (*_s_dOut_6)[int(2)]);
    DiffPair_float_0 _S2037;
    (&_S2037)->primal_0 = 0.0f;
    (&_S2037)->differential_0 = 0.0f;
    DiffPair_float_0 _S2038;
    (&_S2038)->primal_0 = dpparams_6->primal_0[int(3)];
    (&_S2038)->differential_0 = 0.0f;
    _d_max_0(&_S2037, &_S2038, (*_s_dOut_6)[int(2)]);
    float _S2039 = dpparams_6->primal_0[int(12)] * (*_s_dOut_6)[int(1)];
    float _S2040 = dpparams_6->primal_0[int(11)] * (*_s_dOut_6)[int(1)];
    float _S2041 = dpparams_6->primal_0[int(7)] * (*_s_dOut_6)[int(1)];
    float _S2042 = dpparams_6->primal_0[int(6)] * (*_s_dOut_6)[int(1)];
    float _S2043 = dpparams_6->primal_0[int(2)] * (*_s_dOut_6)[int(1)];
    float _S2044 = dpparams_6->primal_0[int(1)] * (*_s_dOut_6)[int(1)];
    PPISPParamsNoCRF_0 _S2045 = PPISPParamsNoCRF_x24_syn_dzero_0();
    (&_S2045)->color_params_1 = _S1980;
    (&_S2045)->exposure_1 = (*_s_dOut_6)[int(0)];
    _S1951 = _S2045;
    *&((&(&(&_S1951)->color_params_1)->n_0)->y) = 0.0f;
    *&((&(&(&_S1951)->color_params_1)->n_0)->x) = 0.0f;
    *&((&(&(&_S1951)->color_params_1)->g_0)->y) = 0.0f;
    *&((&(&(&_S1951)->color_params_1)->g_0)->x) = 0.0f;
    *&((&(&(&_S1951)->color_params_1)->r_0)->y) = 0.0f;
    *&((&(&(&_S1951)->color_params_1)->r_0)->x) = 0.0f;
    *&((&(&(&_S1951)->color_params_1)->b_0)->y) = 0.0f;
    *&((&(&(&_S1951)->color_params_1)->b_0)->x) = 0.0f;
    (&(&_S1951)->vignette_params_0[int(2)])->alpha2_0 = 0.0f;
    float _S2046 = _S1983 + _S1988 + _S2022.differential_0 + _S2045.vignette_params_0[int(2)].alpha2_0;
    (&(&_S1951)->vignette_params_0[int(2)])->alpha1_0 = 0.0f;
    float _S2047 = _S1991 + _S1996 + _S2028.differential_0 + _S2045.vignette_params_0[int(2)].alpha1_0;
    (&(&_S1951)->vignette_params_0[int(2)])->alpha0_0 = 0.0f;
    float _S2048 = _S1999 + _S2004 + _S2034.differential_0 + _S2045.vignette_params_0[int(2)].alpha0_0;
    (&(&_S1951)->vignette_params_0[int(2)])->cy_0 = 0.0f;
    float _S2049 = _S2007 + _S2012 + _S2039 + _S2039 + _S2045.vignette_params_0[int(2)].cy_0;
    (&(&_S1951)->vignette_params_0[int(2)])->cx_0 = 0.0f;
    float _S2050 = _S2015 + _S2020 + _S2040 + _S2040 + _S2045.vignette_params_0[int(2)].cx_0;
    (&(&_S1951)->vignette_params_0[int(1)])->alpha2_0 = 0.0f;
    float _S2051 = _S1985 + _S1988 + _S2024.differential_0 + _S2045.vignette_params_0[int(1)].alpha2_0;
    (&(&_S1951)->vignette_params_0[int(1)])->alpha1_0 = 0.0f;
    float _S2052 = _S1993 + _S1996 + _S2030.differential_0 + _S2045.vignette_params_0[int(1)].alpha1_0;
    (&(&_S1951)->vignette_params_0[int(1)])->alpha0_0 = 0.0f;
    float _S2053 = _S2001 + _S2004 + _S2036.differential_0 + _S2045.vignette_params_0[int(1)].alpha0_0;
    (&(&_S1951)->vignette_params_0[int(1)])->cy_0 = 0.0f;
    float _S2054 = _S2009 + _S2012 + _S2041 + _S2041 + _S2045.vignette_params_0[int(1)].cy_0;
    (&(&_S1951)->vignette_params_0[int(1)])->cx_0 = 0.0f;
    float _S2055 = _S2017 + _S2020 + _S2042 + _S2042 + _S2045.vignette_params_0[int(1)].cx_0;
    (&(&_S1951)->vignette_params_0[int(0)])->alpha2_0 = 0.0f;
    float _S2056 = _S1987 + _S1988 + _S2026.differential_0 + _S2045.vignette_params_0[int(0)].alpha2_0;
    (&(&_S1951)->vignette_params_0[int(0)])->alpha1_0 = 0.0f;
    float _S2057 = _S1995 + _S1996 + _S2032.differential_0 + _S2045.vignette_params_0[int(0)].alpha1_0;
    (&(&_S1951)->vignette_params_0[int(0)])->alpha0_0 = 0.0f;
    float _S2058 = _S2003 + _S2004 + _S2038.differential_0 + _S2045.vignette_params_0[int(0)].alpha0_0;
    (&(&_S1951)->vignette_params_0[int(0)])->cy_0 = 0.0f;
    float _S2059 = _S2011 + _S2012 + _S2043 + _S2043 + _S2045.vignette_params_0[int(0)].cy_0;
    (&(&_S1951)->vignette_params_0[int(0)])->cx_0 = 0.0f;
    float _S2060 = _S2019 + _S2020 + _S2044 + _S2044 + _S2045.vignette_params_0[int(0)].cx_0;
    FixedArray<float, 24>  _S2061;
    _S2061[int(0)] = 0.0f;
    _S2061[int(1)] = 0.0f;
    _S2061[int(2)] = 0.0f;
    _S2061[int(3)] = 0.0f;
    _S2061[int(4)] = 0.0f;
    _S2061[int(5)] = 0.0f;
    _S2061[int(6)] = 0.0f;
    _S2061[int(7)] = 0.0f;
    _S2061[int(8)] = 0.0f;
    _S2061[int(9)] = 0.0f;
    _S2061[int(10)] = 0.0f;
    _S2061[int(11)] = 0.0f;
    _S2061[int(12)] = 0.0f;
    _S2061[int(13)] = 0.0f;
    _S2061[int(14)] = 0.0f;
    _S2061[int(15)] = 0.0f;
    _S2061[int(16)] = 0.0f;
    _S2061[int(17)] = 0.0f;
    _S2061[int(18)] = 0.0f;
    _S2061[int(19)] = 0.0f;
    _S2061[int(20)] = 0.0f;
    _S2061[int(21)] = 0.0f;
    _S2061[int(22)] = 0.0f;
    _S2061[int(23)] = 0.0f;
    _S2061[int(11)] = _S2050;
    _S2061[int(0)] = _S1951.exposure_1;
    _S2061[int(1)] = _S2060;
    _S2061[int(2)] = _S2059;
    _S2061[int(3)] = _S2058;
    _S2061[int(4)] = _S2057;
    _S2061[int(5)] = _S2056;
    _S2061[int(6)] = _S2055;
    _S2061[int(7)] = _S2054;
    _S2061[int(8)] = _S2053;
    _S2061[int(9)] = _S2052;
    _S2061[int(10)] = _S2051;
    _S2061[int(23)] = _S2045.color_params_1.n_0.y;
    _S2061[int(12)] = _S2049;
    _S2061[int(13)] = _S2048;
    _S2061[int(14)] = _S2047;
    _S2061[int(15)] = _S2046;
    _S2061[int(16)] = _S2045.color_params_1.b_0.x;
    _S2061[int(17)] = _S2045.color_params_1.b_0.y;
    _S2061[int(18)] = _S2045.color_params_1.r_0.x;
    _S2061[int(19)] = _S2045.color_params_1.r_0.y;
    _S2061[int(20)] = _S2045.color_params_1.g_0.x;
    _S2061[int(21)] = _S2045.color_params_1.g_0.y;
    _S2061[int(22)] = _S2045.color_params_1.n_0.x;
    dpparams_6->primal_0 = dpparams_6->primal_0;
    dpparams_6->differential_0 = _S2061;
    return;
}

inline __device__ void s_bwd_compute_raw_ppisp_no_crf_regularization_loss_0(DiffPair_arrayx3Cfloatx2C24x3E_0 * _S2062, FixedArray<float, 18>  * _S2063)
{
    s_bwd_prop_compute_raw_ppisp_no_crf_regularization_loss_0(_S2062, _S2063);
    return;
}

inline __device__ void compute_raw_ppisp_no_crf_regularization_loss_vjp(FixedArray<float, 24>  params_13, FixedArray<float, 18>  grad_out_6, FixedArray<float, 24>  * _S2064)
{
    FixedArray<float, 24>  _S2065 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    DiffPair_arrayx3Cfloatx2C24x3E_0 dp_params_6;
    (&dp_params_6)->primal_0 = params_13;
    (&dp_params_6)->differential_0 = _S2065;
    FixedArray<float, 18>  _S2066 = grad_out_6;
    s_bwd_compute_raw_ppisp_no_crf_regularization_loss_0(&dp_params_6, &_S2066);
    *_S2064 = (&dp_params_6)->differential_0;
    return;
}

inline __device__ void compute_raw_ppisp_no_crf_no_vig_regularization_loss(FixedArray<float, 9>  params_14, FixedArray<float, 9>  * _S2067)
{
    PPISPParamsNoCRFNoVig_0 p_7;
    (&p_7)->exposure_0 = params_14[int(0)];
    *&((&(&(&p_7)->color_params_0)->b_0)->x) = params_14[int(1)];
    *&((&(&(&p_7)->color_params_0)->b_0)->y) = params_14[int(2)];
    *&((&(&(&p_7)->color_params_0)->r_0)->x) = params_14[int(3)];
    *&((&(&(&p_7)->color_params_0)->r_0)->y) = params_14[int(4)];
    *&((&(&(&p_7)->color_params_0)->g_0)->x) = params_14[int(5)];
    *&((&(&(&p_7)->color_params_0)->g_0)->y) = params_14[int(6)];
    *&((&(&(&p_7)->color_params_0)->n_0)->x) = params_14[int(7)];
    *&((&(&(&p_7)->color_params_0)->n_0)->y) = params_14[int(8)];
    FixedArray<float, 9>  losses_3;
    losses_3[int(0)] = 0.0f;
    losses_3[int(1)] = 0.0f;
    losses_3[int(2)] = 0.0f;
    losses_3[int(3)] = 0.0f;
    losses_3[int(4)] = 0.0f;
    losses_3[int(5)] = 0.0f;
    losses_3[int(6)] = 0.0f;
    losses_3[int(7)] = 0.0f;
    losses_3[int(8)] = 0.0f;
    losses_3[int(0)] = p_7.exposure_0;
    float2  bd_7 = mul_0(makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f), p_7.color_params_0.b_0);
    float2  rd_7 = mul_0(makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f), p_7.color_params_0.r_0);
    float2  gd_7 = mul_0(makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f), p_7.color_params_0.g_0);
    float2  nd_7 = mul_0(makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f), p_7.color_params_0.n_0);
    losses_3[int(1)] = bd_7.x;
    losses_3[int(2)] = bd_7.y;
    losses_3[int(3)] = rd_7.x;
    losses_3[int(4)] = rd_7.y;
    losses_3[int(5)] = gd_7.x;
    losses_3[int(6)] = gd_7.y;
    losses_3[int(7)] = nd_7.x;
    losses_3[int(8)] = nd_7.y;
    *_S2067 = losses_3;
    return;
}

inline __device__ void s_bwd_prop_compute_raw_ppisp_no_crf_no_vig_regularization_loss_0(DiffPair_arrayx3Cfloatx2C9x3E_0 * dpparams_7, FixedArray<float, 9>  * _s_dOut_7)
{
    float2  _S2068 = make_float2 (0.0f);
    ColorPPISPParams_0 _S2069 = { _S2068, _S2068, _S2068, _S2068 };
    PPISPParamsNoCRFNoVig_0 _S2070;
    (&_S2070)->exposure_0 = dpparams_7->primal_0[int(0)];
    (&_S2070)->color_params_0 = _S2069;
    *&((&(&(&_S2070)->color_params_0)->b_0)->x) = dpparams_7->primal_0[int(1)];
    *&((&(&(&_S2070)->color_params_0)->b_0)->y) = dpparams_7->primal_0[int(2)];
    *&((&(&(&_S2070)->color_params_0)->r_0)->x) = dpparams_7->primal_0[int(3)];
    *&((&(&(&_S2070)->color_params_0)->r_0)->y) = dpparams_7->primal_0[int(4)];
    *&((&(&(&_S2070)->color_params_0)->g_0)->x) = dpparams_7->primal_0[int(5)];
    *&((&(&(&_S2070)->color_params_0)->g_0)->y) = dpparams_7->primal_0[int(6)];
    *&((&(&(&_S2070)->color_params_0)->n_0)->x) = dpparams_7->primal_0[int(7)];
    *&((&(&(&_S2070)->color_params_0)->n_0)->y) = dpparams_7->primal_0[int(8)];
    float2  _S2071 = make_float2 ((*_s_dOut_7)[int(7)], (*_s_dOut_7)[int(8)]);
    Matrix<float, 2, 2>  _S2072 = makeMatrix<float, 2, 2> (0.0f);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S2073;
    (&_S2073)->primal_0 = makeMatrix<float, 2, 2> (0.01283689960837364f, -0.00346540007740259f, -0.00346540007740259f, 0.01281579956412315f);
    (&_S2073)->differential_0 = _S2072;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S2074;
    (&_S2074)->primal_0 = _S2070.color_params_0.n_0;
    (&_S2074)->differential_0 = _S2068;
    s_bwd_prop_mul_2(&_S2073, &_S2074, _S2071);
    float2  _S2075 = make_float2 ((*_s_dOut_7)[int(5)], (*_s_dOut_7)[int(6)]);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S2076;
    (&_S2076)->primal_0 = makeMatrix<float, 2, 2> (0.04333360120654106f, -0.01805369928479195f, -0.01805369928479195f, 0.0580499991774559f);
    (&_S2076)->differential_0 = _S2072;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S2077;
    (&_S2077)->primal_0 = _S2070.color_params_0.g_0;
    (&_S2077)->differential_0 = _S2068;
    s_bwd_prop_mul_2(&_S2076, &_S2077, _S2075);
    float2  _S2078 = make_float2 ((*_s_dOut_7)[int(3)], (*_s_dOut_7)[int(4)]);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S2079;
    (&_S2079)->primal_0 = makeMatrix<float, 2, 2> (0.05805699899792671f, -0.0179871991276741f, -0.0179871991276741f, 0.04310610145330429f);
    (&_S2079)->differential_0 = _S2072;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S2080;
    (&_S2080)->primal_0 = _S2070.color_params_0.r_0;
    (&_S2080)->differential_0 = _S2068;
    s_bwd_prop_mul_2(&_S2079, &_S2080, _S2078);
    float2  _S2081 = make_float2 ((*_s_dOut_7)[int(1)], (*_s_dOut_7)[int(2)]);
    DiffPair_matrixx3Cfloatx2C2x2C2x3E_0 _S2082;
    (&_S2082)->primal_0 = makeMatrix<float, 2, 2> (0.04805419966578484f, -0.0043631000444293f, -0.0043631000444293f, 0.04812829941511154f);
    (&_S2082)->differential_0 = _S2072;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S2083;
    (&_S2083)->primal_0 = _S2070.color_params_0.b_0;
    (&_S2083)->differential_0 = _S2068;
    s_bwd_prop_mul_2(&_S2082, &_S2083, _S2081);
    ColorPPISPParams_0 _S2084 = ColorPPISPParams_x24_syn_dzero_0();
    (&_S2084)->n_0 = _S2074.differential_0;
    (&_S2084)->g_0 = _S2077.differential_0;
    (&_S2084)->r_0 = _S2080.differential_0;
    (&_S2084)->b_0 = _S2083.differential_0;
    PPISPParamsNoCRFNoVig_0 _S2085 = PPISPParamsNoCRFNoVig_x24_syn_dzero_0();
    (&_S2085)->color_params_0 = _S2084;
    (&_S2085)->exposure_0 = (*_s_dOut_7)[int(0)];
    _S2070 = _S2085;
    *&((&(&(&_S2070)->color_params_0)->n_0)->y) = 0.0f;
    *&((&(&(&_S2070)->color_params_0)->n_0)->x) = 0.0f;
    *&((&(&(&_S2070)->color_params_0)->g_0)->y) = 0.0f;
    *&((&(&(&_S2070)->color_params_0)->g_0)->x) = 0.0f;
    *&((&(&(&_S2070)->color_params_0)->r_0)->y) = 0.0f;
    *&((&(&(&_S2070)->color_params_0)->r_0)->x) = 0.0f;
    *&((&(&(&_S2070)->color_params_0)->b_0)->y) = 0.0f;
    *&((&(&(&_S2070)->color_params_0)->b_0)->x) = 0.0f;
    FixedArray<float, 9>  _S2086;
    _S2086[int(0)] = 0.0f;
    _S2086[int(1)] = 0.0f;
    _S2086[int(2)] = 0.0f;
    _S2086[int(3)] = 0.0f;
    _S2086[int(4)] = 0.0f;
    _S2086[int(5)] = 0.0f;
    _S2086[int(6)] = 0.0f;
    _S2086[int(7)] = 0.0f;
    _S2086[int(8)] = 0.0f;
    _S2086[int(8)] = _S2085.color_params_0.n_0.y;
    _S2086[int(7)] = _S2085.color_params_0.n_0.x;
    _S2086[int(6)] = _S2085.color_params_0.g_0.y;
    _S2086[int(5)] = _S2085.color_params_0.g_0.x;
    _S2086[int(4)] = _S2085.color_params_0.r_0.y;
    _S2086[int(3)] = _S2085.color_params_0.r_0.x;
    _S2086[int(2)] = _S2085.color_params_0.b_0.y;
    _S2086[int(1)] = _S2085.color_params_0.b_0.x;
    _S2086[int(0)] = _S2070.exposure_0;
    dpparams_7->primal_0 = dpparams_7->primal_0;
    dpparams_7->differential_0 = _S2086;
    return;
}

inline __device__ void s_bwd_compute_raw_ppisp_no_crf_no_vig_regularization_loss_0(DiffPair_arrayx3Cfloatx2C9x3E_0 * _S2087, FixedArray<float, 9>  * _S2088)
{
    s_bwd_prop_compute_raw_ppisp_no_crf_no_vig_regularization_loss_0(_S2087, _S2088);
    return;
}

inline __device__ void compute_raw_ppisp_no_crf_no_vig_regularization_loss_vjp(FixedArray<float, 9>  params_15, FixedArray<float, 9>  grad_out_7, FixedArray<float, 9>  * _S2089)
{
    FixedArray<float, 9>  _S2090 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    DiffPair_arrayx3Cfloatx2C9x3E_0 dp_params_7;
    (&dp_params_7)->primal_0 = params_15;
    (&dp_params_7)->differential_0 = _S2090;
    FixedArray<float, 9>  _S2091 = grad_out_7;
    s_bwd_compute_raw_ppisp_no_crf_no_vig_regularization_loss_0(&dp_params_7, &_S2091);
    *_S2089 = (&dp_params_7)->differential_0;
    return;
}

inline __device__ void compute_ppisp_regularization_loss(FixedArray<float, 22>  raw_losses_0, int num_cameras_0, FixedArray<float, 6>  loss_weights_0, FixedArray<float, 6>  * _S2092)
{
    float _S2093;
    FixedArray<float, 6>  losses_4;
    float _S2094 = float(num_cameras_0);
    float _S2095 = raw_losses_0[int(0)] / _S2094;
    for(;;)
    {
        float _S2096 = (F32_abs((_S2095)));
        if(_S2096 < 0.10000000149011612f)
        {
            _S2093 = 0.5f * _S2095 * _S2095 / 0.10000000149011612f;
            break;
        }
        else
        {
            _S2093 = _S2096 - 0.05000000074505806f;
            break;
        }
    }
    losses_4[int(0)] = _S2093;
    losses_4[int(1)] = raw_losses_0[int(1)] / (3.0f * _S2094);
    losses_4[int(2)] = (raw_losses_0[int(2)] + raw_losses_0[int(3)] + raw_losses_0[int(4)]) / (9.0f * _S2094);
    losses_4[int(3)] = (raw_losses_0[int(5)] + raw_losses_0[int(6)] + raw_losses_0[int(7)] + raw_losses_0[int(8)] + raw_losses_0[int(9)]) / (5.0f * _S2094);
    float _S2097 = raw_losses_0[int(10)] / _S2094;
    for(;;)
    {
        float _S2098 = (F32_abs((_S2097)));
        if(_S2098 < 0.00499999988824129f)
        {
            _S2093 = 0.5f * _S2097 * _S2097 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2093 = _S2098 - 0.00249999994412065f;
            break;
        }
    }
    float _S2099;
    float _S2100 = raw_losses_0[int(11)] / _S2094;
    for(;;)
    {
        float _S2101 = (F32_abs((_S2100)));
        if(_S2101 < 0.00499999988824129f)
        {
            _S2099 = 0.5f * _S2100 * _S2100 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2099 = _S2101 - 0.00249999994412065f;
            break;
        }
    }
    float _S2102 = _S2093 + _S2099;
    float _S2103 = raw_losses_0[int(12)] / _S2094;
    for(;;)
    {
        float _S2104 = (F32_abs((_S2103)));
        if(_S2104 < 0.00499999988824129f)
        {
            _S2093 = 0.5f * _S2103 * _S2103 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2093 = _S2104 - 0.00249999994412065f;
            break;
        }
    }
    float _S2105 = _S2102 + _S2093;
    float _S2106 = raw_losses_0[int(13)] / _S2094;
    for(;;)
    {
        float _S2107 = (F32_abs((_S2106)));
        if(_S2107 < 0.00499999988824129f)
        {
            _S2093 = 0.5f * _S2106 * _S2106 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2093 = _S2107 - 0.00249999994412065f;
            break;
        }
    }
    float _S2108 = _S2105 + _S2093;
    float _S2109 = raw_losses_0[int(14)] / _S2094;
    for(;;)
    {
        float _S2110 = (F32_abs((_S2109)));
        if(_S2110 < 0.00499999988824129f)
        {
            _S2093 = 0.5f * _S2109 * _S2109 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2093 = _S2110 - 0.00249999994412065f;
            break;
        }
    }
    float _S2111 = _S2108 + _S2093;
    float _S2112 = raw_losses_0[int(15)] / _S2094;
    for(;;)
    {
        float _S2113 = (F32_abs((_S2112)));
        if(_S2113 < 0.00499999988824129f)
        {
            _S2093 = 0.5f * _S2112 * _S2112 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2093 = _S2113 - 0.00249999994412065f;
            break;
        }
    }
    float _S2114 = _S2111 + _S2093;
    float _S2115 = raw_losses_0[int(16)] / _S2094;
    for(;;)
    {
        float _S2116 = (F32_abs((_S2115)));
        if(_S2116 < 0.00499999988824129f)
        {
            _S2093 = 0.5f * _S2115 * _S2115 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2093 = _S2116 - 0.00249999994412065f;
            break;
        }
    }
    float _S2117 = _S2114 + _S2093;
    float _S2118 = raw_losses_0[int(17)] / _S2094;
    for(;;)
    {
        float _S2119 = (F32_abs((_S2118)));
        if(_S2119 < 0.00499999988824129f)
        {
            _S2093 = 0.5f * _S2118 * _S2118 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2093 = _S2119 - 0.00249999994412065f;
            break;
        }
    }
    float _S2120 = (_S2117 + _S2093) / 8.0f;
    float _S2121 = (raw_losses_0[int(18)] + raw_losses_0[int(19)] + raw_losses_0[int(20)] + raw_losses_0[int(21)]) / (4.0f * _S2094);
    losses_4[int(0)] = losses_4[int(0)] * loss_weights_0[int(0)];
    losses_4[int(1)] = losses_4[int(1)] * loss_weights_0[int(1)];
    losses_4[int(2)] = losses_4[int(2)] * loss_weights_0[int(2)];
    losses_4[int(3)] = losses_4[int(3)] * loss_weights_0[int(3)];
    losses_4[int(4)] = _S2120 * loss_weights_0[int(4)];
    losses_4[int(5)] = _S2121 * loss_weights_0[int(5)];
    *_S2092 = losses_4;
    return;
}

inline __device__ void compute_ppisp_rqs_regularization_loss(FixedArray<float, 23>  raw_losses_1, int num_cameras_1, FixedArray<float, 6>  loss_weights_1, FixedArray<float, 6>  * _S2122)
{
    float _S2123;
    FixedArray<float, 6>  losses_5;
    float _S2124 = float(num_cameras_1);
    float _S2125 = raw_losses_1[int(0)] / _S2124;
    for(;;)
    {
        float _S2126 = (F32_abs((_S2125)));
        if(_S2126 < 0.10000000149011612f)
        {
            _S2123 = 0.5f * _S2125 * _S2125 / 0.10000000149011612f;
            break;
        }
        else
        {
            _S2123 = _S2126 - 0.05000000074505806f;
            break;
        }
    }
    losses_5[int(0)] = _S2123;
    losses_5[int(1)] = raw_losses_1[int(1)] / (3.0f * _S2124);
    losses_5[int(2)] = (raw_losses_1[int(2)] + raw_losses_1[int(3)] + raw_losses_1[int(4)]) / (9.0f * _S2124);
    float _S2127 = 5.0f * _S2124;
    losses_5[int(3)] = (raw_losses_1[int(5)] + raw_losses_1[int(6)] + raw_losses_1[int(7)] + raw_losses_1[int(8)] + raw_losses_1[int(9)]) / _S2127;
    float _S2128 = raw_losses_1[int(10)] / _S2124;
    for(;;)
    {
        float _S2129 = (F32_abs((_S2128)));
        if(_S2129 < 0.00499999988824129f)
        {
            _S2123 = 0.5f * _S2128 * _S2128 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2123 = _S2129 - 0.00249999994412065f;
            break;
        }
    }
    float _S2130;
    float _S2131 = raw_losses_1[int(11)] / _S2124;
    for(;;)
    {
        float _S2132 = (F32_abs((_S2131)));
        if(_S2132 < 0.00499999988824129f)
        {
            _S2130 = 0.5f * _S2131 * _S2131 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2130 = _S2132 - 0.00249999994412065f;
            break;
        }
    }
    float _S2133 = _S2123 + _S2130;
    float _S2134 = raw_losses_1[int(12)] / _S2124;
    for(;;)
    {
        float _S2135 = (F32_abs((_S2134)));
        if(_S2135 < 0.00499999988824129f)
        {
            _S2123 = 0.5f * _S2134 * _S2134 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2123 = _S2135 - 0.00249999994412065f;
            break;
        }
    }
    float _S2136 = _S2133 + _S2123;
    float _S2137 = raw_losses_1[int(13)] / _S2124;
    for(;;)
    {
        float _S2138 = (F32_abs((_S2137)));
        if(_S2138 < 0.00499999988824129f)
        {
            _S2123 = 0.5f * _S2137 * _S2137 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2123 = _S2138 - 0.00249999994412065f;
            break;
        }
    }
    float _S2139 = _S2136 + _S2123;
    float _S2140 = raw_losses_1[int(14)] / _S2124;
    for(;;)
    {
        float _S2141 = (F32_abs((_S2140)));
        if(_S2141 < 0.00499999988824129f)
        {
            _S2123 = 0.5f * _S2140 * _S2140 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2123 = _S2141 - 0.00249999994412065f;
            break;
        }
    }
    float _S2142 = _S2139 + _S2123;
    float _S2143 = raw_losses_1[int(15)] / _S2124;
    for(;;)
    {
        float _S2144 = (F32_abs((_S2143)));
        if(_S2144 < 0.00499999988824129f)
        {
            _S2123 = 0.5f * _S2143 * _S2143 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2123 = _S2144 - 0.00249999994412065f;
            break;
        }
    }
    float _S2145 = _S2142 + _S2123;
    float _S2146 = raw_losses_1[int(16)] / _S2124;
    for(;;)
    {
        float _S2147 = (F32_abs((_S2146)));
        if(_S2147 < 0.00499999988824129f)
        {
            _S2123 = 0.5f * _S2146 * _S2146 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2123 = _S2147 - 0.00249999994412065f;
            break;
        }
    }
    float _S2148 = _S2145 + _S2123;
    float _S2149 = raw_losses_1[int(17)] / _S2124;
    for(;;)
    {
        float _S2150 = (F32_abs((_S2149)));
        if(_S2150 < 0.00499999988824129f)
        {
            _S2123 = 0.5f * _S2149 * _S2149 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2123 = _S2150 - 0.00249999994412065f;
            break;
        }
    }
    float _S2151 = (_S2148 + _S2123) / 8.0f;
    float _S2152 = (raw_losses_1[int(18)] + raw_losses_1[int(19)] + raw_losses_1[int(20)] + raw_losses_1[int(21)] + raw_losses_1[int(22)]) / _S2127;
    losses_5[int(0)] = losses_5[int(0)] * loss_weights_1[int(0)];
    losses_5[int(1)] = losses_5[int(1)] * loss_weights_1[int(1)];
    losses_5[int(2)] = losses_5[int(2)] * loss_weights_1[int(2)];
    losses_5[int(3)] = losses_5[int(3)] * loss_weights_1[int(3)];
    losses_5[int(4)] = _S2151 * loss_weights_1[int(4)];
    losses_5[int(5)] = _S2152 * loss_weights_1[int(5)];
    *_S2122 = losses_5;
    return;
}

struct DiffPair_arrayx3Cfloatx2C22x3E_0
{
    FixedArray<float, 22>  primal_0;
    FixedArray<float, 22>  differential_0;
};

inline __device__ void s_bwd_prop_compute_ppisp_regularization_loss_0(DiffPair_arrayx3Cfloatx2C22x3E_0 * dpraw_losses_0, int num_cameras_2, FixedArray<float, 6>  * loss_weights_2, FixedArray<float, 6>  * _s_dOut_8)
{
    FixedArray<float, 22>  _S2153 = dpraw_losses_0->primal_0;
    float _S2154 = float(num_cameras_2);
    float _S2155 = dpraw_losses_0->primal_0[int(0)] / _S2154;
    bool _S2156 = (s_primal_ctx_abs_0(_S2155)) < 0.10000000149011612f;
    float _S2157;
    if(_S2156)
    {
        _S2157 = 0.5f * _S2155;
    }
    else
    {
        _S2157 = 0.0f;
    }
    float _S2158 = 3.0f * _S2154;
    float _S2159 = 9.0f * _S2154;
    float _S2160 = 5.0f * _S2154;
    float _S2161 = _S2153[int(10)] / _S2154;
    bool _S2162 = (s_primal_ctx_abs_0(_S2161)) < 0.00499999988824129f;
    float _S2163;
    if(_S2162)
    {
        _S2163 = 0.5f * _S2161;
    }
    else
    {
        _S2163 = 0.0f;
    }
    float _S2164 = _S2153[int(11)] / _S2154;
    bool _S2165 = (s_primal_ctx_abs_0(_S2164)) < 0.00499999988824129f;
    float _S2166;
    if(_S2165)
    {
        _S2166 = 0.5f * _S2164;
    }
    else
    {
        _S2166 = 0.0f;
    }
    float _S2167 = _S2153[int(12)] / _S2154;
    bool _S2168 = (s_primal_ctx_abs_0(_S2167)) < 0.00499999988824129f;
    float _S2169;
    if(_S2168)
    {
        _S2169 = 0.5f * _S2167;
    }
    else
    {
        _S2169 = 0.0f;
    }
    float _S2170 = _S2153[int(13)] / _S2154;
    bool _S2171 = (s_primal_ctx_abs_0(_S2170)) < 0.00499999988824129f;
    float _S2172;
    if(_S2171)
    {
        _S2172 = 0.5f * _S2170;
    }
    else
    {
        _S2172 = 0.0f;
    }
    float _S2173 = _S2153[int(14)] / _S2154;
    bool _S2174 = (s_primal_ctx_abs_0(_S2173)) < 0.00499999988824129f;
    float _S2175;
    if(_S2174)
    {
        _S2175 = 0.5f * _S2173;
    }
    else
    {
        _S2175 = 0.0f;
    }
    float _S2176 = _S2153[int(15)] / _S2154;
    bool _S2177 = (s_primal_ctx_abs_0(_S2176)) < 0.00499999988824129f;
    float _S2178;
    if(_S2177)
    {
        _S2178 = 0.5f * _S2176;
    }
    else
    {
        _S2178 = 0.0f;
    }
    float _S2179 = _S2153[int(16)] / _S2154;
    bool _S2180 = (s_primal_ctx_abs_0(_S2179)) < 0.00499999988824129f;
    float _S2181;
    if(_S2180)
    {
        _S2181 = 0.5f * _S2179;
    }
    else
    {
        _S2181 = 0.0f;
    }
    float _S2182 = _S2153[int(17)] / _S2154;
    bool _S2183 = (s_primal_ctx_abs_0(_S2182)) < 0.00499999988824129f;
    float _S2184;
    if(_S2183)
    {
        _S2184 = 0.5f * _S2182;
    }
    else
    {
        _S2184 = 0.0f;
    }
    float _S2185 = (*loss_weights_2)[int(3)] * (*_s_dOut_8)[int(3)];
    float _S2186 = (*loss_weights_2)[int(2)] * (*_s_dOut_8)[int(2)];
    float _S2187 = (*loss_weights_2)[int(1)] * (*_s_dOut_8)[int(1)];
    float _S2188 = (*loss_weights_2)[int(0)] * (*_s_dOut_8)[int(0)];
    float _S2189 = (*loss_weights_2)[int(5)] * (*_s_dOut_8)[int(5)] / (4.0f * _S2154);
    float _S2190 = 0.125f * ((*loss_weights_2)[int(4)] * (*_s_dOut_8)[int(4)]);
    FixedArray<float, 22>  _S2191;
    _S2191[int(0)] = 0.0f;
    _S2191[int(1)] = 0.0f;
    _S2191[int(2)] = 0.0f;
    _S2191[int(3)] = 0.0f;
    _S2191[int(4)] = 0.0f;
    _S2191[int(5)] = 0.0f;
    _S2191[int(6)] = 0.0f;
    _S2191[int(7)] = 0.0f;
    _S2191[int(8)] = 0.0f;
    _S2191[int(9)] = 0.0f;
    _S2191[int(10)] = 0.0f;
    _S2191[int(11)] = 0.0f;
    _S2191[int(12)] = 0.0f;
    _S2191[int(13)] = 0.0f;
    _S2191[int(14)] = 0.0f;
    _S2191[int(15)] = 0.0f;
    _S2191[int(16)] = 0.0f;
    _S2191[int(17)] = 0.0f;
    _S2191[int(18)] = 0.0f;
    _S2191[int(19)] = 0.0f;
    _S2191[int(20)] = 0.0f;
    _S2191[int(21)] = 0.0f;
    _S2191[int(21)] = _S2189;
    _S2191[int(20)] = _S2189;
    _S2191[int(19)] = _S2189;
    _S2191[int(18)] = _S2189;
    float _S2192 = _S2191[int(0)];
    float _S2193 = _S2191[int(1)];
    float _S2194 = _S2191[int(2)];
    float _S2195 = _S2191[int(3)];
    float _S2196 = _S2191[int(4)];
    float _S2197 = _S2191[int(5)];
    float _S2198 = _S2191[int(6)];
    float _S2199 = _S2191[int(7)];
    float _S2200 = _S2191[int(8)];
    float _S2201 = _S2191[int(9)];
    float _S2202 = _S2191[int(10)];
    float _S2203 = _S2191[int(11)];
    float _S2204 = _S2191[int(12)];
    float _S2205 = _S2191[int(13)];
    float _S2206 = _S2191[int(14)];
    float _S2207 = _S2191[int(15)];
    float _S2208 = _S2191[int(16)];
    float _S2209 = _S2191[int(17)];
    float _S2210 = _S2191[int(18)];
    float _S2211 = _S2191[int(19)];
    float _S2212 = _S2191[int(20)];
    float _S2213 = _S2191[int(21)];
    float _S2214;
    if(_S2183)
    {
        float _S2215 = 200.0f * _S2190;
        float _S2216 = _S2184 * _S2215 + 0.5f * (_S2182 * _S2215);
        _S2184 = 0.0f;
        _S2214 = _S2216;
    }
    else
    {
        _S2184 = _S2190;
        _S2214 = 0.0f;
    }
    DiffPair_float_0 _S2217;
    (&_S2217)->primal_0 = _S2182;
    (&_S2217)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2217, _S2184);
    float _S2218 = (_S2217.differential_0 + _S2214) / _S2154;
    FixedArray<float, 22>  _S2219;
    _S2219[int(0)] = 0.0f;
    _S2219[int(1)] = 0.0f;
    _S2219[int(2)] = 0.0f;
    _S2219[int(3)] = 0.0f;
    _S2219[int(4)] = 0.0f;
    _S2219[int(5)] = 0.0f;
    _S2219[int(6)] = 0.0f;
    _S2219[int(7)] = 0.0f;
    _S2219[int(8)] = 0.0f;
    _S2219[int(9)] = 0.0f;
    _S2219[int(10)] = 0.0f;
    _S2219[int(11)] = 0.0f;
    _S2219[int(12)] = 0.0f;
    _S2219[int(13)] = 0.0f;
    _S2219[int(14)] = 0.0f;
    _S2219[int(15)] = 0.0f;
    _S2219[int(16)] = 0.0f;
    _S2219[int(17)] = 0.0f;
    _S2219[int(18)] = 0.0f;
    _S2219[int(19)] = 0.0f;
    _S2219[int(20)] = 0.0f;
    _S2219[int(21)] = 0.0f;
    _S2219[int(17)] = _S2218;
    float _S2220 = _S2192 + _S2219[int(0)];
    float _S2221 = _S2193 + _S2219[int(1)];
    float _S2222 = _S2194 + _S2219[int(2)];
    float _S2223 = _S2195 + _S2219[int(3)];
    float _S2224 = _S2196 + _S2219[int(4)];
    float _S2225 = _S2197 + _S2219[int(5)];
    float _S2226 = _S2198 + _S2219[int(6)];
    float _S2227 = _S2199 + _S2219[int(7)];
    float _S2228 = _S2200 + _S2219[int(8)];
    float _S2229 = _S2201 + _S2219[int(9)];
    float _S2230 = _S2202 + _S2219[int(10)];
    float _S2231 = _S2203 + _S2219[int(11)];
    float _S2232 = _S2204 + _S2219[int(12)];
    float _S2233 = _S2205 + _S2219[int(13)];
    float _S2234 = _S2206 + _S2219[int(14)];
    float _S2235 = _S2207 + _S2219[int(15)];
    float _S2236 = _S2208 + _S2219[int(16)];
    float _S2237 = _S2209 + _S2219[int(17)];
    float _S2238 = _S2210 + _S2219[int(18)];
    float _S2239 = _S2211 + _S2219[int(19)];
    float _S2240 = _S2212 + _S2219[int(20)];
    float _S2241 = _S2213 + _S2219[int(21)];
    if(_S2180)
    {
        float _S2242 = 200.0f * _S2190;
        float _S2243 = _S2181 * _S2242 + 0.5f * (_S2179 * _S2242);
        _S2181 = 0.0f;
        _S2184 = _S2243;
    }
    else
    {
        _S2181 = _S2190;
        _S2184 = 0.0f;
    }
    DiffPair_float_0 _S2244;
    (&_S2244)->primal_0 = _S2179;
    (&_S2244)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2244, _S2181);
    float _S2245 = (_S2244.differential_0 + _S2184) / _S2154;
    FixedArray<float, 22>  _S2246;
    _S2246[int(0)] = 0.0f;
    _S2246[int(1)] = 0.0f;
    _S2246[int(2)] = 0.0f;
    _S2246[int(3)] = 0.0f;
    _S2246[int(4)] = 0.0f;
    _S2246[int(5)] = 0.0f;
    _S2246[int(6)] = 0.0f;
    _S2246[int(7)] = 0.0f;
    _S2246[int(8)] = 0.0f;
    _S2246[int(9)] = 0.0f;
    _S2246[int(10)] = 0.0f;
    _S2246[int(11)] = 0.0f;
    _S2246[int(12)] = 0.0f;
    _S2246[int(13)] = 0.0f;
    _S2246[int(14)] = 0.0f;
    _S2246[int(15)] = 0.0f;
    _S2246[int(16)] = 0.0f;
    _S2246[int(17)] = 0.0f;
    _S2246[int(18)] = 0.0f;
    _S2246[int(19)] = 0.0f;
    _S2246[int(20)] = 0.0f;
    _S2246[int(21)] = 0.0f;
    _S2246[int(16)] = _S2245;
    float _S2247 = _S2220 + _S2246[int(0)];
    float _S2248 = _S2221 + _S2246[int(1)];
    float _S2249 = _S2222 + _S2246[int(2)];
    float _S2250 = _S2223 + _S2246[int(3)];
    float _S2251 = _S2224 + _S2246[int(4)];
    float _S2252 = _S2225 + _S2246[int(5)];
    float _S2253 = _S2226 + _S2246[int(6)];
    float _S2254 = _S2227 + _S2246[int(7)];
    float _S2255 = _S2228 + _S2246[int(8)];
    float _S2256 = _S2229 + _S2246[int(9)];
    float _S2257 = _S2230 + _S2246[int(10)];
    float _S2258 = _S2231 + _S2246[int(11)];
    float _S2259 = _S2232 + _S2246[int(12)];
    float _S2260 = _S2233 + _S2246[int(13)];
    float _S2261 = _S2234 + _S2246[int(14)];
    float _S2262 = _S2235 + _S2246[int(15)];
    float _S2263 = _S2236 + _S2246[int(16)];
    float _S2264 = _S2237 + _S2246[int(17)];
    float _S2265 = _S2238 + _S2246[int(18)];
    float _S2266 = _S2239 + _S2246[int(19)];
    float _S2267 = _S2240 + _S2246[int(20)];
    float _S2268 = _S2241 + _S2246[int(21)];
    if(_S2177)
    {
        float _S2269 = 200.0f * _S2190;
        float _S2270 = _S2178 * _S2269 + 0.5f * (_S2176 * _S2269);
        _S2178 = 0.0f;
        _S2181 = _S2270;
    }
    else
    {
        _S2178 = _S2190;
        _S2181 = 0.0f;
    }
    DiffPair_float_0 _S2271;
    (&_S2271)->primal_0 = _S2176;
    (&_S2271)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2271, _S2178);
    float _S2272 = (_S2271.differential_0 + _S2181) / _S2154;
    FixedArray<float, 22>  _S2273;
    _S2273[int(0)] = 0.0f;
    _S2273[int(1)] = 0.0f;
    _S2273[int(2)] = 0.0f;
    _S2273[int(3)] = 0.0f;
    _S2273[int(4)] = 0.0f;
    _S2273[int(5)] = 0.0f;
    _S2273[int(6)] = 0.0f;
    _S2273[int(7)] = 0.0f;
    _S2273[int(8)] = 0.0f;
    _S2273[int(9)] = 0.0f;
    _S2273[int(10)] = 0.0f;
    _S2273[int(11)] = 0.0f;
    _S2273[int(12)] = 0.0f;
    _S2273[int(13)] = 0.0f;
    _S2273[int(14)] = 0.0f;
    _S2273[int(15)] = 0.0f;
    _S2273[int(16)] = 0.0f;
    _S2273[int(17)] = 0.0f;
    _S2273[int(18)] = 0.0f;
    _S2273[int(19)] = 0.0f;
    _S2273[int(20)] = 0.0f;
    _S2273[int(21)] = 0.0f;
    _S2273[int(15)] = _S2272;
    float _S2274 = _S2247 + _S2273[int(0)];
    float _S2275 = _S2248 + _S2273[int(1)];
    float _S2276 = _S2249 + _S2273[int(2)];
    float _S2277 = _S2250 + _S2273[int(3)];
    float _S2278 = _S2251 + _S2273[int(4)];
    float _S2279 = _S2252 + _S2273[int(5)];
    float _S2280 = _S2253 + _S2273[int(6)];
    float _S2281 = _S2254 + _S2273[int(7)];
    float _S2282 = _S2255 + _S2273[int(8)];
    float _S2283 = _S2256 + _S2273[int(9)];
    float _S2284 = _S2257 + _S2273[int(10)];
    float _S2285 = _S2258 + _S2273[int(11)];
    float _S2286 = _S2259 + _S2273[int(12)];
    float _S2287 = _S2260 + _S2273[int(13)];
    float _S2288 = _S2261 + _S2273[int(14)];
    float _S2289 = _S2262 + _S2273[int(15)];
    float _S2290 = _S2263 + _S2273[int(16)];
    float _S2291 = _S2264 + _S2273[int(17)];
    float _S2292 = _S2265 + _S2273[int(18)];
    float _S2293 = _S2266 + _S2273[int(19)];
    float _S2294 = _S2267 + _S2273[int(20)];
    float _S2295 = _S2268 + _S2273[int(21)];
    if(_S2174)
    {
        float _S2296 = 200.0f * _S2190;
        float _S2297 = _S2175 * _S2296 + 0.5f * (_S2173 * _S2296);
        _S2175 = 0.0f;
        _S2178 = _S2297;
    }
    else
    {
        _S2175 = _S2190;
        _S2178 = 0.0f;
    }
    DiffPair_float_0 _S2298;
    (&_S2298)->primal_0 = _S2173;
    (&_S2298)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2298, _S2175);
    float _S2299 = (_S2298.differential_0 + _S2178) / _S2154;
    FixedArray<float, 22>  _S2300;
    _S2300[int(0)] = 0.0f;
    _S2300[int(1)] = 0.0f;
    _S2300[int(2)] = 0.0f;
    _S2300[int(3)] = 0.0f;
    _S2300[int(4)] = 0.0f;
    _S2300[int(5)] = 0.0f;
    _S2300[int(6)] = 0.0f;
    _S2300[int(7)] = 0.0f;
    _S2300[int(8)] = 0.0f;
    _S2300[int(9)] = 0.0f;
    _S2300[int(10)] = 0.0f;
    _S2300[int(11)] = 0.0f;
    _S2300[int(12)] = 0.0f;
    _S2300[int(13)] = 0.0f;
    _S2300[int(14)] = 0.0f;
    _S2300[int(15)] = 0.0f;
    _S2300[int(16)] = 0.0f;
    _S2300[int(17)] = 0.0f;
    _S2300[int(18)] = 0.0f;
    _S2300[int(19)] = 0.0f;
    _S2300[int(20)] = 0.0f;
    _S2300[int(21)] = 0.0f;
    _S2300[int(14)] = _S2299;
    float _S2301 = _S2274 + _S2300[int(0)];
    float _S2302 = _S2275 + _S2300[int(1)];
    float _S2303 = _S2276 + _S2300[int(2)];
    float _S2304 = _S2277 + _S2300[int(3)];
    float _S2305 = _S2278 + _S2300[int(4)];
    float _S2306 = _S2279 + _S2300[int(5)];
    float _S2307 = _S2280 + _S2300[int(6)];
    float _S2308 = _S2281 + _S2300[int(7)];
    float _S2309 = _S2282 + _S2300[int(8)];
    float _S2310 = _S2283 + _S2300[int(9)];
    float _S2311 = _S2284 + _S2300[int(10)];
    float _S2312 = _S2285 + _S2300[int(11)];
    float _S2313 = _S2286 + _S2300[int(12)];
    float _S2314 = _S2287 + _S2300[int(13)];
    float _S2315 = _S2288 + _S2300[int(14)];
    float _S2316 = _S2289 + _S2300[int(15)];
    float _S2317 = _S2290 + _S2300[int(16)];
    float _S2318 = _S2291 + _S2300[int(17)];
    float _S2319 = _S2292 + _S2300[int(18)];
    float _S2320 = _S2293 + _S2300[int(19)];
    float _S2321 = _S2294 + _S2300[int(20)];
    float _S2322 = _S2295 + _S2300[int(21)];
    if(_S2171)
    {
        float _S2323 = 200.0f * _S2190;
        float _S2324 = _S2172 * _S2323 + 0.5f * (_S2170 * _S2323);
        _S2172 = 0.0f;
        _S2175 = _S2324;
    }
    else
    {
        _S2172 = _S2190;
        _S2175 = 0.0f;
    }
    DiffPair_float_0 _S2325;
    (&_S2325)->primal_0 = _S2170;
    (&_S2325)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2325, _S2172);
    float _S2326 = (_S2325.differential_0 + _S2175) / _S2154;
    FixedArray<float, 22>  _S2327;
    _S2327[int(0)] = 0.0f;
    _S2327[int(1)] = 0.0f;
    _S2327[int(2)] = 0.0f;
    _S2327[int(3)] = 0.0f;
    _S2327[int(4)] = 0.0f;
    _S2327[int(5)] = 0.0f;
    _S2327[int(6)] = 0.0f;
    _S2327[int(7)] = 0.0f;
    _S2327[int(8)] = 0.0f;
    _S2327[int(9)] = 0.0f;
    _S2327[int(10)] = 0.0f;
    _S2327[int(11)] = 0.0f;
    _S2327[int(12)] = 0.0f;
    _S2327[int(13)] = 0.0f;
    _S2327[int(14)] = 0.0f;
    _S2327[int(15)] = 0.0f;
    _S2327[int(16)] = 0.0f;
    _S2327[int(17)] = 0.0f;
    _S2327[int(18)] = 0.0f;
    _S2327[int(19)] = 0.0f;
    _S2327[int(20)] = 0.0f;
    _S2327[int(21)] = 0.0f;
    _S2327[int(13)] = _S2326;
    float _S2328 = _S2301 + _S2327[int(0)];
    float _S2329 = _S2302 + _S2327[int(1)];
    float _S2330 = _S2303 + _S2327[int(2)];
    float _S2331 = _S2304 + _S2327[int(3)];
    float _S2332 = _S2305 + _S2327[int(4)];
    float _S2333 = _S2306 + _S2327[int(5)];
    float _S2334 = _S2307 + _S2327[int(6)];
    float _S2335 = _S2308 + _S2327[int(7)];
    float _S2336 = _S2309 + _S2327[int(8)];
    float _S2337 = _S2310 + _S2327[int(9)];
    float _S2338 = _S2311 + _S2327[int(10)];
    float _S2339 = _S2312 + _S2327[int(11)];
    float _S2340 = _S2313 + _S2327[int(12)];
    float _S2341 = _S2314 + _S2327[int(13)];
    float _S2342 = _S2315 + _S2327[int(14)];
    float _S2343 = _S2316 + _S2327[int(15)];
    float _S2344 = _S2317 + _S2327[int(16)];
    float _S2345 = _S2318 + _S2327[int(17)];
    float _S2346 = _S2319 + _S2327[int(18)];
    float _S2347 = _S2320 + _S2327[int(19)];
    float _S2348 = _S2321 + _S2327[int(20)];
    float _S2349 = _S2322 + _S2327[int(21)];
    if(_S2168)
    {
        float _S2350 = 200.0f * _S2190;
        float _S2351 = _S2169 * _S2350 + 0.5f * (_S2167 * _S2350);
        _S2169 = 0.0f;
        _S2172 = _S2351;
    }
    else
    {
        _S2169 = _S2190;
        _S2172 = 0.0f;
    }
    DiffPair_float_0 _S2352;
    (&_S2352)->primal_0 = _S2167;
    (&_S2352)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2352, _S2169);
    float _S2353 = (_S2352.differential_0 + _S2172) / _S2154;
    FixedArray<float, 22>  _S2354;
    _S2354[int(0)] = 0.0f;
    _S2354[int(1)] = 0.0f;
    _S2354[int(2)] = 0.0f;
    _S2354[int(3)] = 0.0f;
    _S2354[int(4)] = 0.0f;
    _S2354[int(5)] = 0.0f;
    _S2354[int(6)] = 0.0f;
    _S2354[int(7)] = 0.0f;
    _S2354[int(8)] = 0.0f;
    _S2354[int(9)] = 0.0f;
    _S2354[int(10)] = 0.0f;
    _S2354[int(11)] = 0.0f;
    _S2354[int(12)] = 0.0f;
    _S2354[int(13)] = 0.0f;
    _S2354[int(14)] = 0.0f;
    _S2354[int(15)] = 0.0f;
    _S2354[int(16)] = 0.0f;
    _S2354[int(17)] = 0.0f;
    _S2354[int(18)] = 0.0f;
    _S2354[int(19)] = 0.0f;
    _S2354[int(20)] = 0.0f;
    _S2354[int(21)] = 0.0f;
    _S2354[int(12)] = _S2353;
    float _S2355 = _S2328 + _S2354[int(0)];
    float _S2356 = _S2329 + _S2354[int(1)];
    float _S2357 = _S2330 + _S2354[int(2)];
    float _S2358 = _S2331 + _S2354[int(3)];
    float _S2359 = _S2332 + _S2354[int(4)];
    float _S2360 = _S2333 + _S2354[int(5)];
    float _S2361 = _S2334 + _S2354[int(6)];
    float _S2362 = _S2335 + _S2354[int(7)];
    float _S2363 = _S2336 + _S2354[int(8)];
    float _S2364 = _S2337 + _S2354[int(9)];
    float _S2365 = _S2338 + _S2354[int(10)];
    float _S2366 = _S2339 + _S2354[int(11)];
    float _S2367 = _S2340 + _S2354[int(12)];
    float _S2368 = _S2341 + _S2354[int(13)];
    float _S2369 = _S2342 + _S2354[int(14)];
    float _S2370 = _S2343 + _S2354[int(15)];
    float _S2371 = _S2344 + _S2354[int(16)];
    float _S2372 = _S2345 + _S2354[int(17)];
    float _S2373 = _S2346 + _S2354[int(18)];
    float _S2374 = _S2347 + _S2354[int(19)];
    float _S2375 = _S2348 + _S2354[int(20)];
    float _S2376 = _S2349 + _S2354[int(21)];
    if(_S2165)
    {
        float _S2377 = 200.0f * _S2190;
        float _S2378 = _S2166 * _S2377 + 0.5f * (_S2164 * _S2377);
        _S2166 = 0.0f;
        _S2169 = _S2378;
    }
    else
    {
        _S2166 = _S2190;
        _S2169 = 0.0f;
    }
    DiffPair_float_0 _S2379;
    (&_S2379)->primal_0 = _S2164;
    (&_S2379)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2379, _S2166);
    float _S2380 = (_S2379.differential_0 + _S2169) / _S2154;
    FixedArray<float, 22>  _S2381;
    _S2381[int(0)] = 0.0f;
    _S2381[int(1)] = 0.0f;
    _S2381[int(2)] = 0.0f;
    _S2381[int(3)] = 0.0f;
    _S2381[int(4)] = 0.0f;
    _S2381[int(5)] = 0.0f;
    _S2381[int(6)] = 0.0f;
    _S2381[int(7)] = 0.0f;
    _S2381[int(8)] = 0.0f;
    _S2381[int(9)] = 0.0f;
    _S2381[int(10)] = 0.0f;
    _S2381[int(11)] = 0.0f;
    _S2381[int(12)] = 0.0f;
    _S2381[int(13)] = 0.0f;
    _S2381[int(14)] = 0.0f;
    _S2381[int(15)] = 0.0f;
    _S2381[int(16)] = 0.0f;
    _S2381[int(17)] = 0.0f;
    _S2381[int(18)] = 0.0f;
    _S2381[int(19)] = 0.0f;
    _S2381[int(20)] = 0.0f;
    _S2381[int(21)] = 0.0f;
    _S2381[int(11)] = _S2380;
    float _S2382 = _S2355 + _S2381[int(0)];
    float _S2383 = _S2356 + _S2381[int(1)];
    float _S2384 = _S2357 + _S2381[int(2)];
    float _S2385 = _S2358 + _S2381[int(3)];
    float _S2386 = _S2359 + _S2381[int(4)];
    float _S2387 = _S2360 + _S2381[int(5)];
    float _S2388 = _S2361 + _S2381[int(6)];
    float _S2389 = _S2362 + _S2381[int(7)];
    float _S2390 = _S2363 + _S2381[int(8)];
    float _S2391 = _S2364 + _S2381[int(9)];
    float _S2392 = _S2365 + _S2381[int(10)];
    float _S2393 = _S2366 + _S2381[int(11)];
    float _S2394 = _S2367 + _S2381[int(12)];
    float _S2395 = _S2368 + _S2381[int(13)];
    float _S2396 = _S2369 + _S2381[int(14)];
    float _S2397 = _S2370 + _S2381[int(15)];
    float _S2398 = _S2371 + _S2381[int(16)];
    float _S2399 = _S2372 + _S2381[int(17)];
    float _S2400 = _S2373 + _S2381[int(18)];
    float _S2401 = _S2374 + _S2381[int(19)];
    float _S2402 = _S2375 + _S2381[int(20)];
    float _S2403 = _S2376 + _S2381[int(21)];
    if(_S2162)
    {
        float _S2404 = 200.0f * _S2190;
        float _S2405 = _S2163 * _S2404 + 0.5f * (_S2161 * _S2404);
        _S2163 = 0.0f;
        _S2166 = _S2405;
    }
    else
    {
        _S2163 = _S2190;
        _S2166 = 0.0f;
    }
    DiffPair_float_0 _S2406;
    (&_S2406)->primal_0 = _S2161;
    (&_S2406)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2406, _S2163);
    float _S2407 = (_S2406.differential_0 + _S2166) / _S2154;
    float _S2408 = _S2185 / _S2160;
    float _S2409 = _S2186 / _S2159;
    float _S2410 = _S2187 / _S2158;
    FixedArray<float, 22>  _S2411;
    _S2411[int(0)] = 0.0f;
    _S2411[int(1)] = 0.0f;
    _S2411[int(2)] = 0.0f;
    _S2411[int(3)] = 0.0f;
    _S2411[int(4)] = 0.0f;
    _S2411[int(5)] = 0.0f;
    _S2411[int(6)] = 0.0f;
    _S2411[int(7)] = 0.0f;
    _S2411[int(8)] = 0.0f;
    _S2411[int(9)] = 0.0f;
    _S2411[int(10)] = 0.0f;
    _S2411[int(11)] = 0.0f;
    _S2411[int(12)] = 0.0f;
    _S2411[int(13)] = 0.0f;
    _S2411[int(14)] = 0.0f;
    _S2411[int(15)] = 0.0f;
    _S2411[int(16)] = 0.0f;
    _S2411[int(17)] = 0.0f;
    _S2411[int(18)] = 0.0f;
    _S2411[int(19)] = 0.0f;
    _S2411[int(20)] = 0.0f;
    _S2411[int(21)] = 0.0f;
    _S2411[int(10)] = _S2407;
    _S2411[int(9)] = _S2408;
    _S2411[int(8)] = _S2408;
    _S2411[int(7)] = _S2408;
    _S2411[int(6)] = _S2408;
    _S2411[int(5)] = _S2408;
    _S2411[int(4)] = _S2409;
    _S2411[int(3)] = _S2409;
    _S2411[int(2)] = _S2409;
    _S2411[int(1)] = _S2410;
    float _S2412 = _S2382 + _S2411[int(0)];
    float _S2413 = _S2383 + _S2411[int(1)];
    float _S2414 = _S2384 + _S2411[int(2)];
    float _S2415 = _S2385 + _S2411[int(3)];
    float _S2416 = _S2386 + _S2411[int(4)];
    float _S2417 = _S2387 + _S2411[int(5)];
    float _S2418 = _S2388 + _S2411[int(6)];
    float _S2419 = _S2389 + _S2411[int(7)];
    float _S2420 = _S2390 + _S2411[int(8)];
    float _S2421 = _S2391 + _S2411[int(9)];
    float _S2422 = _S2392 + _S2411[int(10)];
    float _S2423 = _S2393 + _S2411[int(11)];
    float _S2424 = _S2394 + _S2411[int(12)];
    float _S2425 = _S2395 + _S2411[int(13)];
    float _S2426 = _S2396 + _S2411[int(14)];
    float _S2427 = _S2397 + _S2411[int(15)];
    float _S2428 = _S2398 + _S2411[int(16)];
    float _S2429 = _S2399 + _S2411[int(17)];
    float _S2430 = _S2400 + _S2411[int(18)];
    float _S2431 = _S2401 + _S2411[int(19)];
    float _S2432 = _S2402 + _S2411[int(20)];
    float _S2433 = _S2403 + _S2411[int(21)];
    if(_S2156)
    {
        float _S2434 = 10.0f * _S2188;
        float _S2435 = _S2157 * _S2434 + 0.5f * (_S2155 * _S2434);
        _S2157 = 0.0f;
        _S2163 = _S2435;
    }
    else
    {
        _S2157 = _S2188;
        _S2163 = 0.0f;
    }
    DiffPair_float_0 _S2436;
    (&_S2436)->primal_0 = _S2155;
    (&_S2436)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2436, _S2157);
    float _S2437 = (_S2436.differential_0 + _S2163) / _S2154;
    FixedArray<float, 22>  _S2438;
    _S2438[int(0)] = 0.0f;
    _S2438[int(1)] = 0.0f;
    _S2438[int(2)] = 0.0f;
    _S2438[int(3)] = 0.0f;
    _S2438[int(4)] = 0.0f;
    _S2438[int(5)] = 0.0f;
    _S2438[int(6)] = 0.0f;
    _S2438[int(7)] = 0.0f;
    _S2438[int(8)] = 0.0f;
    _S2438[int(9)] = 0.0f;
    _S2438[int(10)] = 0.0f;
    _S2438[int(11)] = 0.0f;
    _S2438[int(12)] = 0.0f;
    _S2438[int(13)] = 0.0f;
    _S2438[int(14)] = 0.0f;
    _S2438[int(15)] = 0.0f;
    _S2438[int(16)] = 0.0f;
    _S2438[int(17)] = 0.0f;
    _S2438[int(18)] = 0.0f;
    _S2438[int(19)] = 0.0f;
    _S2438[int(20)] = 0.0f;
    _S2438[int(21)] = 0.0f;
    _S2438[int(0)] = _S2437;
    FixedArray<float, 22>  _S2439 = {
        _S2412 + _S2438[int(0)], _S2413 + _S2438[int(1)], _S2414 + _S2438[int(2)], _S2415 + _S2438[int(3)], _S2416 + _S2438[int(4)], _S2417 + _S2438[int(5)], _S2418 + _S2438[int(6)], _S2419 + _S2438[int(7)], _S2420 + _S2438[int(8)], _S2421 + _S2438[int(9)], _S2422 + _S2438[int(10)], _S2423 + _S2438[int(11)], _S2424 + _S2438[int(12)], _S2425 + _S2438[int(13)], _S2426 + _S2438[int(14)], _S2427 + _S2438[int(15)], _S2428 + _S2438[int(16)], _S2429 + _S2438[int(17)], _S2430 + _S2438[int(18)], _S2431 + _S2438[int(19)], _S2432 + _S2438[int(20)], _S2433 + _S2438[int(21)]
    };
    dpraw_losses_0->primal_0 = dpraw_losses_0->primal_0;
    dpraw_losses_0->differential_0 = _S2439;
    return;
}

inline __device__ void s_bwd_compute_ppisp_regularization_loss_0(DiffPair_arrayx3Cfloatx2C22x3E_0 * _S2440, int _S2441, FixedArray<float, 6>  * _S2442, FixedArray<float, 6>  * _S2443)
{
    s_bwd_prop_compute_ppisp_regularization_loss_0(_S2440, _S2441, _S2442, _S2443);
    return;
}

inline __device__ void compute_ppisp_regularization_loss_vjp(FixedArray<float, 22>  raw_losses_2, int num_cameras_3, FixedArray<float, 6>  loss_weights_3, FixedArray<float, 6>  grad_out_8, FixedArray<float, 22>  * _S2444)
{
    FixedArray<float, 22>  _S2445 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    DiffPair_arrayx3Cfloatx2C22x3E_0 dp_raw_losses_0;
    (&dp_raw_losses_0)->primal_0 = raw_losses_2;
    (&dp_raw_losses_0)->differential_0 = _S2445;
    FixedArray<float, 6>  _S2446 = loss_weights_3;
    FixedArray<float, 6>  _S2447 = grad_out_8;
    s_bwd_compute_ppisp_regularization_loss_0(&dp_raw_losses_0, num_cameras_3, &_S2446, &_S2447);
    *_S2444 = (&dp_raw_losses_0)->differential_0;
    return;
}

struct DiffPair_arrayx3Cfloatx2C23x3E_0
{
    FixedArray<float, 23>  primal_0;
    FixedArray<float, 23>  differential_0;
};

inline __device__ void s_bwd_prop_compute_ppisp_rqs_regularization_loss_0(DiffPair_arrayx3Cfloatx2C23x3E_0 * dpraw_losses_1, int num_cameras_4, FixedArray<float, 6>  * loss_weights_4, FixedArray<float, 6>  * _s_dOut_9)
{
    FixedArray<float, 23>  _S2448 = dpraw_losses_1->primal_0;
    float _S2449 = float(num_cameras_4);
    float _S2450 = dpraw_losses_1->primal_0[int(0)] / _S2449;
    bool _S2451 = (s_primal_ctx_abs_0(_S2450)) < 0.10000000149011612f;
    float _S2452;
    if(_S2451)
    {
        _S2452 = 0.5f * _S2450;
    }
    else
    {
        _S2452 = 0.0f;
    }
    float _S2453 = 3.0f * _S2449;
    float _S2454 = 9.0f * _S2449;
    float _S2455 = 5.0f * _S2449;
    float _S2456 = _S2448[int(10)] / _S2449;
    bool _S2457 = (s_primal_ctx_abs_0(_S2456)) < 0.00499999988824129f;
    float _S2458;
    if(_S2457)
    {
        _S2458 = 0.5f * _S2456;
    }
    else
    {
        _S2458 = 0.0f;
    }
    float _S2459 = _S2448[int(11)] / _S2449;
    bool _S2460 = (s_primal_ctx_abs_0(_S2459)) < 0.00499999988824129f;
    float _S2461;
    if(_S2460)
    {
        _S2461 = 0.5f * _S2459;
    }
    else
    {
        _S2461 = 0.0f;
    }
    float _S2462 = _S2448[int(12)] / _S2449;
    bool _S2463 = (s_primal_ctx_abs_0(_S2462)) < 0.00499999988824129f;
    float _S2464;
    if(_S2463)
    {
        _S2464 = 0.5f * _S2462;
    }
    else
    {
        _S2464 = 0.0f;
    }
    float _S2465 = _S2448[int(13)] / _S2449;
    bool _S2466 = (s_primal_ctx_abs_0(_S2465)) < 0.00499999988824129f;
    float _S2467;
    if(_S2466)
    {
        _S2467 = 0.5f * _S2465;
    }
    else
    {
        _S2467 = 0.0f;
    }
    float _S2468 = _S2448[int(14)] / _S2449;
    bool _S2469 = (s_primal_ctx_abs_0(_S2468)) < 0.00499999988824129f;
    float _S2470;
    if(_S2469)
    {
        _S2470 = 0.5f * _S2468;
    }
    else
    {
        _S2470 = 0.0f;
    }
    float _S2471 = _S2448[int(15)] / _S2449;
    bool _S2472 = (s_primal_ctx_abs_0(_S2471)) < 0.00499999988824129f;
    float _S2473;
    if(_S2472)
    {
        _S2473 = 0.5f * _S2471;
    }
    else
    {
        _S2473 = 0.0f;
    }
    float _S2474 = _S2448[int(16)] / _S2449;
    bool _S2475 = (s_primal_ctx_abs_0(_S2474)) < 0.00499999988824129f;
    float _S2476;
    if(_S2475)
    {
        _S2476 = 0.5f * _S2474;
    }
    else
    {
        _S2476 = 0.0f;
    }
    float _S2477 = _S2448[int(17)] / _S2449;
    bool _S2478 = (s_primal_ctx_abs_0(_S2477)) < 0.00499999988824129f;
    float _S2479;
    if(_S2478)
    {
        _S2479 = 0.5f * _S2477;
    }
    else
    {
        _S2479 = 0.0f;
    }
    float _S2480 = (*loss_weights_4)[int(3)] * (*_s_dOut_9)[int(3)];
    float _S2481 = (*loss_weights_4)[int(2)] * (*_s_dOut_9)[int(2)];
    float _S2482 = (*loss_weights_4)[int(1)] * (*_s_dOut_9)[int(1)];
    float _S2483 = (*loss_weights_4)[int(0)] * (*_s_dOut_9)[int(0)];
    float _S2484 = (*loss_weights_4)[int(5)] * (*_s_dOut_9)[int(5)] / _S2455;
    float _S2485 = 0.125f * ((*loss_weights_4)[int(4)] * (*_s_dOut_9)[int(4)]);
    FixedArray<float, 23>  _S2486;
    _S2486[int(0)] = 0.0f;
    _S2486[int(1)] = 0.0f;
    _S2486[int(2)] = 0.0f;
    _S2486[int(3)] = 0.0f;
    _S2486[int(4)] = 0.0f;
    _S2486[int(5)] = 0.0f;
    _S2486[int(6)] = 0.0f;
    _S2486[int(7)] = 0.0f;
    _S2486[int(8)] = 0.0f;
    _S2486[int(9)] = 0.0f;
    _S2486[int(10)] = 0.0f;
    _S2486[int(11)] = 0.0f;
    _S2486[int(12)] = 0.0f;
    _S2486[int(13)] = 0.0f;
    _S2486[int(14)] = 0.0f;
    _S2486[int(15)] = 0.0f;
    _S2486[int(16)] = 0.0f;
    _S2486[int(17)] = 0.0f;
    _S2486[int(18)] = 0.0f;
    _S2486[int(19)] = 0.0f;
    _S2486[int(20)] = 0.0f;
    _S2486[int(21)] = 0.0f;
    _S2486[int(22)] = 0.0f;
    _S2486[int(22)] = _S2484;
    _S2486[int(21)] = _S2484;
    _S2486[int(20)] = _S2484;
    _S2486[int(19)] = _S2484;
    _S2486[int(18)] = _S2484;
    float _S2487 = _S2486[int(0)];
    float _S2488 = _S2486[int(1)];
    float _S2489 = _S2486[int(2)];
    float _S2490 = _S2486[int(3)];
    float _S2491 = _S2486[int(4)];
    float _S2492 = _S2486[int(5)];
    float _S2493 = _S2486[int(6)];
    float _S2494 = _S2486[int(7)];
    float _S2495 = _S2486[int(8)];
    float _S2496 = _S2486[int(9)];
    float _S2497 = _S2486[int(10)];
    float _S2498 = _S2486[int(11)];
    float _S2499 = _S2486[int(12)];
    float _S2500 = _S2486[int(13)];
    float _S2501 = _S2486[int(14)];
    float _S2502 = _S2486[int(15)];
    float _S2503 = _S2486[int(16)];
    float _S2504 = _S2486[int(17)];
    float _S2505 = _S2486[int(18)];
    float _S2506 = _S2486[int(19)];
    float _S2507 = _S2486[int(20)];
    float _S2508 = _S2486[int(21)];
    float _S2509 = _S2486[int(22)];
    float _S2510;
    if(_S2478)
    {
        float _S2511 = 200.0f * _S2485;
        float _S2512 = _S2479 * _S2511 + 0.5f * (_S2477 * _S2511);
        _S2479 = 0.0f;
        _S2510 = _S2512;
    }
    else
    {
        _S2479 = _S2485;
        _S2510 = 0.0f;
    }
    DiffPair_float_0 _S2513;
    (&_S2513)->primal_0 = _S2477;
    (&_S2513)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2513, _S2479);
    float _S2514 = (_S2513.differential_0 + _S2510) / _S2449;
    FixedArray<float, 23>  _S2515;
    _S2515[int(0)] = 0.0f;
    _S2515[int(1)] = 0.0f;
    _S2515[int(2)] = 0.0f;
    _S2515[int(3)] = 0.0f;
    _S2515[int(4)] = 0.0f;
    _S2515[int(5)] = 0.0f;
    _S2515[int(6)] = 0.0f;
    _S2515[int(7)] = 0.0f;
    _S2515[int(8)] = 0.0f;
    _S2515[int(9)] = 0.0f;
    _S2515[int(10)] = 0.0f;
    _S2515[int(11)] = 0.0f;
    _S2515[int(12)] = 0.0f;
    _S2515[int(13)] = 0.0f;
    _S2515[int(14)] = 0.0f;
    _S2515[int(15)] = 0.0f;
    _S2515[int(16)] = 0.0f;
    _S2515[int(17)] = 0.0f;
    _S2515[int(18)] = 0.0f;
    _S2515[int(19)] = 0.0f;
    _S2515[int(20)] = 0.0f;
    _S2515[int(21)] = 0.0f;
    _S2515[int(22)] = 0.0f;
    _S2515[int(17)] = _S2514;
    float _S2516 = _S2487 + _S2515[int(0)];
    float _S2517 = _S2488 + _S2515[int(1)];
    float _S2518 = _S2489 + _S2515[int(2)];
    float _S2519 = _S2490 + _S2515[int(3)];
    float _S2520 = _S2491 + _S2515[int(4)];
    float _S2521 = _S2492 + _S2515[int(5)];
    float _S2522 = _S2493 + _S2515[int(6)];
    float _S2523 = _S2494 + _S2515[int(7)];
    float _S2524 = _S2495 + _S2515[int(8)];
    float _S2525 = _S2496 + _S2515[int(9)];
    float _S2526 = _S2497 + _S2515[int(10)];
    float _S2527 = _S2498 + _S2515[int(11)];
    float _S2528 = _S2499 + _S2515[int(12)];
    float _S2529 = _S2500 + _S2515[int(13)];
    float _S2530 = _S2501 + _S2515[int(14)];
    float _S2531 = _S2502 + _S2515[int(15)];
    float _S2532 = _S2503 + _S2515[int(16)];
    float _S2533 = _S2504 + _S2515[int(17)];
    float _S2534 = _S2505 + _S2515[int(18)];
    float _S2535 = _S2506 + _S2515[int(19)];
    float _S2536 = _S2507 + _S2515[int(20)];
    float _S2537 = _S2508 + _S2515[int(21)];
    float _S2538 = _S2509 + _S2515[int(22)];
    if(_S2475)
    {
        float _S2539 = 200.0f * _S2485;
        float _S2540 = _S2476 * _S2539 + 0.5f * (_S2474 * _S2539);
        _S2476 = 0.0f;
        _S2479 = _S2540;
    }
    else
    {
        _S2476 = _S2485;
        _S2479 = 0.0f;
    }
    DiffPair_float_0 _S2541;
    (&_S2541)->primal_0 = _S2474;
    (&_S2541)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2541, _S2476);
    float _S2542 = (_S2541.differential_0 + _S2479) / _S2449;
    FixedArray<float, 23>  _S2543;
    _S2543[int(0)] = 0.0f;
    _S2543[int(1)] = 0.0f;
    _S2543[int(2)] = 0.0f;
    _S2543[int(3)] = 0.0f;
    _S2543[int(4)] = 0.0f;
    _S2543[int(5)] = 0.0f;
    _S2543[int(6)] = 0.0f;
    _S2543[int(7)] = 0.0f;
    _S2543[int(8)] = 0.0f;
    _S2543[int(9)] = 0.0f;
    _S2543[int(10)] = 0.0f;
    _S2543[int(11)] = 0.0f;
    _S2543[int(12)] = 0.0f;
    _S2543[int(13)] = 0.0f;
    _S2543[int(14)] = 0.0f;
    _S2543[int(15)] = 0.0f;
    _S2543[int(16)] = 0.0f;
    _S2543[int(17)] = 0.0f;
    _S2543[int(18)] = 0.0f;
    _S2543[int(19)] = 0.0f;
    _S2543[int(20)] = 0.0f;
    _S2543[int(21)] = 0.0f;
    _S2543[int(22)] = 0.0f;
    _S2543[int(16)] = _S2542;
    float _S2544 = _S2516 + _S2543[int(0)];
    float _S2545 = _S2517 + _S2543[int(1)];
    float _S2546 = _S2518 + _S2543[int(2)];
    float _S2547 = _S2519 + _S2543[int(3)];
    float _S2548 = _S2520 + _S2543[int(4)];
    float _S2549 = _S2521 + _S2543[int(5)];
    float _S2550 = _S2522 + _S2543[int(6)];
    float _S2551 = _S2523 + _S2543[int(7)];
    float _S2552 = _S2524 + _S2543[int(8)];
    float _S2553 = _S2525 + _S2543[int(9)];
    float _S2554 = _S2526 + _S2543[int(10)];
    float _S2555 = _S2527 + _S2543[int(11)];
    float _S2556 = _S2528 + _S2543[int(12)];
    float _S2557 = _S2529 + _S2543[int(13)];
    float _S2558 = _S2530 + _S2543[int(14)];
    float _S2559 = _S2531 + _S2543[int(15)];
    float _S2560 = _S2532 + _S2543[int(16)];
    float _S2561 = _S2533 + _S2543[int(17)];
    float _S2562 = _S2534 + _S2543[int(18)];
    float _S2563 = _S2535 + _S2543[int(19)];
    float _S2564 = _S2536 + _S2543[int(20)];
    float _S2565 = _S2537 + _S2543[int(21)];
    float _S2566 = _S2538 + _S2543[int(22)];
    if(_S2472)
    {
        float _S2567 = 200.0f * _S2485;
        float _S2568 = _S2473 * _S2567 + 0.5f * (_S2471 * _S2567);
        _S2473 = 0.0f;
        _S2476 = _S2568;
    }
    else
    {
        _S2473 = _S2485;
        _S2476 = 0.0f;
    }
    DiffPair_float_0 _S2569;
    (&_S2569)->primal_0 = _S2471;
    (&_S2569)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2569, _S2473);
    float _S2570 = (_S2569.differential_0 + _S2476) / _S2449;
    FixedArray<float, 23>  _S2571;
    _S2571[int(0)] = 0.0f;
    _S2571[int(1)] = 0.0f;
    _S2571[int(2)] = 0.0f;
    _S2571[int(3)] = 0.0f;
    _S2571[int(4)] = 0.0f;
    _S2571[int(5)] = 0.0f;
    _S2571[int(6)] = 0.0f;
    _S2571[int(7)] = 0.0f;
    _S2571[int(8)] = 0.0f;
    _S2571[int(9)] = 0.0f;
    _S2571[int(10)] = 0.0f;
    _S2571[int(11)] = 0.0f;
    _S2571[int(12)] = 0.0f;
    _S2571[int(13)] = 0.0f;
    _S2571[int(14)] = 0.0f;
    _S2571[int(15)] = 0.0f;
    _S2571[int(16)] = 0.0f;
    _S2571[int(17)] = 0.0f;
    _S2571[int(18)] = 0.0f;
    _S2571[int(19)] = 0.0f;
    _S2571[int(20)] = 0.0f;
    _S2571[int(21)] = 0.0f;
    _S2571[int(22)] = 0.0f;
    _S2571[int(15)] = _S2570;
    float _S2572 = _S2544 + _S2571[int(0)];
    float _S2573 = _S2545 + _S2571[int(1)];
    float _S2574 = _S2546 + _S2571[int(2)];
    float _S2575 = _S2547 + _S2571[int(3)];
    float _S2576 = _S2548 + _S2571[int(4)];
    float _S2577 = _S2549 + _S2571[int(5)];
    float _S2578 = _S2550 + _S2571[int(6)];
    float _S2579 = _S2551 + _S2571[int(7)];
    float _S2580 = _S2552 + _S2571[int(8)];
    float _S2581 = _S2553 + _S2571[int(9)];
    float _S2582 = _S2554 + _S2571[int(10)];
    float _S2583 = _S2555 + _S2571[int(11)];
    float _S2584 = _S2556 + _S2571[int(12)];
    float _S2585 = _S2557 + _S2571[int(13)];
    float _S2586 = _S2558 + _S2571[int(14)];
    float _S2587 = _S2559 + _S2571[int(15)];
    float _S2588 = _S2560 + _S2571[int(16)];
    float _S2589 = _S2561 + _S2571[int(17)];
    float _S2590 = _S2562 + _S2571[int(18)];
    float _S2591 = _S2563 + _S2571[int(19)];
    float _S2592 = _S2564 + _S2571[int(20)];
    float _S2593 = _S2565 + _S2571[int(21)];
    float _S2594 = _S2566 + _S2571[int(22)];
    if(_S2469)
    {
        float _S2595 = 200.0f * _S2485;
        float _S2596 = _S2470 * _S2595 + 0.5f * (_S2468 * _S2595);
        _S2470 = 0.0f;
        _S2473 = _S2596;
    }
    else
    {
        _S2470 = _S2485;
        _S2473 = 0.0f;
    }
    DiffPair_float_0 _S2597;
    (&_S2597)->primal_0 = _S2468;
    (&_S2597)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2597, _S2470);
    float _S2598 = (_S2597.differential_0 + _S2473) / _S2449;
    FixedArray<float, 23>  _S2599;
    _S2599[int(0)] = 0.0f;
    _S2599[int(1)] = 0.0f;
    _S2599[int(2)] = 0.0f;
    _S2599[int(3)] = 0.0f;
    _S2599[int(4)] = 0.0f;
    _S2599[int(5)] = 0.0f;
    _S2599[int(6)] = 0.0f;
    _S2599[int(7)] = 0.0f;
    _S2599[int(8)] = 0.0f;
    _S2599[int(9)] = 0.0f;
    _S2599[int(10)] = 0.0f;
    _S2599[int(11)] = 0.0f;
    _S2599[int(12)] = 0.0f;
    _S2599[int(13)] = 0.0f;
    _S2599[int(14)] = 0.0f;
    _S2599[int(15)] = 0.0f;
    _S2599[int(16)] = 0.0f;
    _S2599[int(17)] = 0.0f;
    _S2599[int(18)] = 0.0f;
    _S2599[int(19)] = 0.0f;
    _S2599[int(20)] = 0.0f;
    _S2599[int(21)] = 0.0f;
    _S2599[int(22)] = 0.0f;
    _S2599[int(14)] = _S2598;
    float _S2600 = _S2572 + _S2599[int(0)];
    float _S2601 = _S2573 + _S2599[int(1)];
    float _S2602 = _S2574 + _S2599[int(2)];
    float _S2603 = _S2575 + _S2599[int(3)];
    float _S2604 = _S2576 + _S2599[int(4)];
    float _S2605 = _S2577 + _S2599[int(5)];
    float _S2606 = _S2578 + _S2599[int(6)];
    float _S2607 = _S2579 + _S2599[int(7)];
    float _S2608 = _S2580 + _S2599[int(8)];
    float _S2609 = _S2581 + _S2599[int(9)];
    float _S2610 = _S2582 + _S2599[int(10)];
    float _S2611 = _S2583 + _S2599[int(11)];
    float _S2612 = _S2584 + _S2599[int(12)];
    float _S2613 = _S2585 + _S2599[int(13)];
    float _S2614 = _S2586 + _S2599[int(14)];
    float _S2615 = _S2587 + _S2599[int(15)];
    float _S2616 = _S2588 + _S2599[int(16)];
    float _S2617 = _S2589 + _S2599[int(17)];
    float _S2618 = _S2590 + _S2599[int(18)];
    float _S2619 = _S2591 + _S2599[int(19)];
    float _S2620 = _S2592 + _S2599[int(20)];
    float _S2621 = _S2593 + _S2599[int(21)];
    float _S2622 = _S2594 + _S2599[int(22)];
    if(_S2466)
    {
        float _S2623 = 200.0f * _S2485;
        float _S2624 = _S2467 * _S2623 + 0.5f * (_S2465 * _S2623);
        _S2467 = 0.0f;
        _S2470 = _S2624;
    }
    else
    {
        _S2467 = _S2485;
        _S2470 = 0.0f;
    }
    DiffPair_float_0 _S2625;
    (&_S2625)->primal_0 = _S2465;
    (&_S2625)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2625, _S2467);
    float _S2626 = (_S2625.differential_0 + _S2470) / _S2449;
    FixedArray<float, 23>  _S2627;
    _S2627[int(0)] = 0.0f;
    _S2627[int(1)] = 0.0f;
    _S2627[int(2)] = 0.0f;
    _S2627[int(3)] = 0.0f;
    _S2627[int(4)] = 0.0f;
    _S2627[int(5)] = 0.0f;
    _S2627[int(6)] = 0.0f;
    _S2627[int(7)] = 0.0f;
    _S2627[int(8)] = 0.0f;
    _S2627[int(9)] = 0.0f;
    _S2627[int(10)] = 0.0f;
    _S2627[int(11)] = 0.0f;
    _S2627[int(12)] = 0.0f;
    _S2627[int(13)] = 0.0f;
    _S2627[int(14)] = 0.0f;
    _S2627[int(15)] = 0.0f;
    _S2627[int(16)] = 0.0f;
    _S2627[int(17)] = 0.0f;
    _S2627[int(18)] = 0.0f;
    _S2627[int(19)] = 0.0f;
    _S2627[int(20)] = 0.0f;
    _S2627[int(21)] = 0.0f;
    _S2627[int(22)] = 0.0f;
    _S2627[int(13)] = _S2626;
    float _S2628 = _S2600 + _S2627[int(0)];
    float _S2629 = _S2601 + _S2627[int(1)];
    float _S2630 = _S2602 + _S2627[int(2)];
    float _S2631 = _S2603 + _S2627[int(3)];
    float _S2632 = _S2604 + _S2627[int(4)];
    float _S2633 = _S2605 + _S2627[int(5)];
    float _S2634 = _S2606 + _S2627[int(6)];
    float _S2635 = _S2607 + _S2627[int(7)];
    float _S2636 = _S2608 + _S2627[int(8)];
    float _S2637 = _S2609 + _S2627[int(9)];
    float _S2638 = _S2610 + _S2627[int(10)];
    float _S2639 = _S2611 + _S2627[int(11)];
    float _S2640 = _S2612 + _S2627[int(12)];
    float _S2641 = _S2613 + _S2627[int(13)];
    float _S2642 = _S2614 + _S2627[int(14)];
    float _S2643 = _S2615 + _S2627[int(15)];
    float _S2644 = _S2616 + _S2627[int(16)];
    float _S2645 = _S2617 + _S2627[int(17)];
    float _S2646 = _S2618 + _S2627[int(18)];
    float _S2647 = _S2619 + _S2627[int(19)];
    float _S2648 = _S2620 + _S2627[int(20)];
    float _S2649 = _S2621 + _S2627[int(21)];
    float _S2650 = _S2622 + _S2627[int(22)];
    if(_S2463)
    {
        float _S2651 = 200.0f * _S2485;
        float _S2652 = _S2464 * _S2651 + 0.5f * (_S2462 * _S2651);
        _S2464 = 0.0f;
        _S2467 = _S2652;
    }
    else
    {
        _S2464 = _S2485;
        _S2467 = 0.0f;
    }
    DiffPair_float_0 _S2653;
    (&_S2653)->primal_0 = _S2462;
    (&_S2653)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2653, _S2464);
    float _S2654 = (_S2653.differential_0 + _S2467) / _S2449;
    FixedArray<float, 23>  _S2655;
    _S2655[int(0)] = 0.0f;
    _S2655[int(1)] = 0.0f;
    _S2655[int(2)] = 0.0f;
    _S2655[int(3)] = 0.0f;
    _S2655[int(4)] = 0.0f;
    _S2655[int(5)] = 0.0f;
    _S2655[int(6)] = 0.0f;
    _S2655[int(7)] = 0.0f;
    _S2655[int(8)] = 0.0f;
    _S2655[int(9)] = 0.0f;
    _S2655[int(10)] = 0.0f;
    _S2655[int(11)] = 0.0f;
    _S2655[int(12)] = 0.0f;
    _S2655[int(13)] = 0.0f;
    _S2655[int(14)] = 0.0f;
    _S2655[int(15)] = 0.0f;
    _S2655[int(16)] = 0.0f;
    _S2655[int(17)] = 0.0f;
    _S2655[int(18)] = 0.0f;
    _S2655[int(19)] = 0.0f;
    _S2655[int(20)] = 0.0f;
    _S2655[int(21)] = 0.0f;
    _S2655[int(22)] = 0.0f;
    _S2655[int(12)] = _S2654;
    float _S2656 = _S2628 + _S2655[int(0)];
    float _S2657 = _S2629 + _S2655[int(1)];
    float _S2658 = _S2630 + _S2655[int(2)];
    float _S2659 = _S2631 + _S2655[int(3)];
    float _S2660 = _S2632 + _S2655[int(4)];
    float _S2661 = _S2633 + _S2655[int(5)];
    float _S2662 = _S2634 + _S2655[int(6)];
    float _S2663 = _S2635 + _S2655[int(7)];
    float _S2664 = _S2636 + _S2655[int(8)];
    float _S2665 = _S2637 + _S2655[int(9)];
    float _S2666 = _S2638 + _S2655[int(10)];
    float _S2667 = _S2639 + _S2655[int(11)];
    float _S2668 = _S2640 + _S2655[int(12)];
    float _S2669 = _S2641 + _S2655[int(13)];
    float _S2670 = _S2642 + _S2655[int(14)];
    float _S2671 = _S2643 + _S2655[int(15)];
    float _S2672 = _S2644 + _S2655[int(16)];
    float _S2673 = _S2645 + _S2655[int(17)];
    float _S2674 = _S2646 + _S2655[int(18)];
    float _S2675 = _S2647 + _S2655[int(19)];
    float _S2676 = _S2648 + _S2655[int(20)];
    float _S2677 = _S2649 + _S2655[int(21)];
    float _S2678 = _S2650 + _S2655[int(22)];
    if(_S2460)
    {
        float _S2679 = 200.0f * _S2485;
        float _S2680 = _S2461 * _S2679 + 0.5f * (_S2459 * _S2679);
        _S2461 = 0.0f;
        _S2464 = _S2680;
    }
    else
    {
        _S2461 = _S2485;
        _S2464 = 0.0f;
    }
    DiffPair_float_0 _S2681;
    (&_S2681)->primal_0 = _S2459;
    (&_S2681)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2681, _S2461);
    float _S2682 = (_S2681.differential_0 + _S2464) / _S2449;
    FixedArray<float, 23>  _S2683;
    _S2683[int(0)] = 0.0f;
    _S2683[int(1)] = 0.0f;
    _S2683[int(2)] = 0.0f;
    _S2683[int(3)] = 0.0f;
    _S2683[int(4)] = 0.0f;
    _S2683[int(5)] = 0.0f;
    _S2683[int(6)] = 0.0f;
    _S2683[int(7)] = 0.0f;
    _S2683[int(8)] = 0.0f;
    _S2683[int(9)] = 0.0f;
    _S2683[int(10)] = 0.0f;
    _S2683[int(11)] = 0.0f;
    _S2683[int(12)] = 0.0f;
    _S2683[int(13)] = 0.0f;
    _S2683[int(14)] = 0.0f;
    _S2683[int(15)] = 0.0f;
    _S2683[int(16)] = 0.0f;
    _S2683[int(17)] = 0.0f;
    _S2683[int(18)] = 0.0f;
    _S2683[int(19)] = 0.0f;
    _S2683[int(20)] = 0.0f;
    _S2683[int(21)] = 0.0f;
    _S2683[int(22)] = 0.0f;
    _S2683[int(11)] = _S2682;
    float _S2684 = _S2656 + _S2683[int(0)];
    float _S2685 = _S2657 + _S2683[int(1)];
    float _S2686 = _S2658 + _S2683[int(2)];
    float _S2687 = _S2659 + _S2683[int(3)];
    float _S2688 = _S2660 + _S2683[int(4)];
    float _S2689 = _S2661 + _S2683[int(5)];
    float _S2690 = _S2662 + _S2683[int(6)];
    float _S2691 = _S2663 + _S2683[int(7)];
    float _S2692 = _S2664 + _S2683[int(8)];
    float _S2693 = _S2665 + _S2683[int(9)];
    float _S2694 = _S2666 + _S2683[int(10)];
    float _S2695 = _S2667 + _S2683[int(11)];
    float _S2696 = _S2668 + _S2683[int(12)];
    float _S2697 = _S2669 + _S2683[int(13)];
    float _S2698 = _S2670 + _S2683[int(14)];
    float _S2699 = _S2671 + _S2683[int(15)];
    float _S2700 = _S2672 + _S2683[int(16)];
    float _S2701 = _S2673 + _S2683[int(17)];
    float _S2702 = _S2674 + _S2683[int(18)];
    float _S2703 = _S2675 + _S2683[int(19)];
    float _S2704 = _S2676 + _S2683[int(20)];
    float _S2705 = _S2677 + _S2683[int(21)];
    float _S2706 = _S2678 + _S2683[int(22)];
    if(_S2457)
    {
        float _S2707 = 200.0f * _S2485;
        float _S2708 = _S2458 * _S2707 + 0.5f * (_S2456 * _S2707);
        _S2458 = 0.0f;
        _S2461 = _S2708;
    }
    else
    {
        _S2458 = _S2485;
        _S2461 = 0.0f;
    }
    DiffPair_float_0 _S2709;
    (&_S2709)->primal_0 = _S2456;
    (&_S2709)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2709, _S2458);
    float _S2710 = (_S2709.differential_0 + _S2461) / _S2449;
    float _S2711 = _S2480 / _S2455;
    float _S2712 = _S2481 / _S2454;
    float _S2713 = _S2482 / _S2453;
    FixedArray<float, 23>  _S2714;
    _S2714[int(0)] = 0.0f;
    _S2714[int(1)] = 0.0f;
    _S2714[int(2)] = 0.0f;
    _S2714[int(3)] = 0.0f;
    _S2714[int(4)] = 0.0f;
    _S2714[int(5)] = 0.0f;
    _S2714[int(6)] = 0.0f;
    _S2714[int(7)] = 0.0f;
    _S2714[int(8)] = 0.0f;
    _S2714[int(9)] = 0.0f;
    _S2714[int(10)] = 0.0f;
    _S2714[int(11)] = 0.0f;
    _S2714[int(12)] = 0.0f;
    _S2714[int(13)] = 0.0f;
    _S2714[int(14)] = 0.0f;
    _S2714[int(15)] = 0.0f;
    _S2714[int(16)] = 0.0f;
    _S2714[int(17)] = 0.0f;
    _S2714[int(18)] = 0.0f;
    _S2714[int(19)] = 0.0f;
    _S2714[int(20)] = 0.0f;
    _S2714[int(21)] = 0.0f;
    _S2714[int(22)] = 0.0f;
    _S2714[int(10)] = _S2710;
    _S2714[int(9)] = _S2711;
    _S2714[int(8)] = _S2711;
    _S2714[int(7)] = _S2711;
    _S2714[int(6)] = _S2711;
    _S2714[int(5)] = _S2711;
    _S2714[int(4)] = _S2712;
    _S2714[int(3)] = _S2712;
    _S2714[int(2)] = _S2712;
    _S2714[int(1)] = _S2713;
    float _S2715 = _S2684 + _S2714[int(0)];
    float _S2716 = _S2685 + _S2714[int(1)];
    float _S2717 = _S2686 + _S2714[int(2)];
    float _S2718 = _S2687 + _S2714[int(3)];
    float _S2719 = _S2688 + _S2714[int(4)];
    float _S2720 = _S2689 + _S2714[int(5)];
    float _S2721 = _S2690 + _S2714[int(6)];
    float _S2722 = _S2691 + _S2714[int(7)];
    float _S2723 = _S2692 + _S2714[int(8)];
    float _S2724 = _S2693 + _S2714[int(9)];
    float _S2725 = _S2694 + _S2714[int(10)];
    float _S2726 = _S2695 + _S2714[int(11)];
    float _S2727 = _S2696 + _S2714[int(12)];
    float _S2728 = _S2697 + _S2714[int(13)];
    float _S2729 = _S2698 + _S2714[int(14)];
    float _S2730 = _S2699 + _S2714[int(15)];
    float _S2731 = _S2700 + _S2714[int(16)];
    float _S2732 = _S2701 + _S2714[int(17)];
    float _S2733 = _S2702 + _S2714[int(18)];
    float _S2734 = _S2703 + _S2714[int(19)];
    float _S2735 = _S2704 + _S2714[int(20)];
    float _S2736 = _S2705 + _S2714[int(21)];
    float _S2737 = _S2706 + _S2714[int(22)];
    if(_S2451)
    {
        float _S2738 = 10.0f * _S2483;
        float _S2739 = _S2452 * _S2738 + 0.5f * (_S2450 * _S2738);
        _S2452 = 0.0f;
        _S2458 = _S2739;
    }
    else
    {
        _S2452 = _S2483;
        _S2458 = 0.0f;
    }
    DiffPair_float_0 _S2740;
    (&_S2740)->primal_0 = _S2450;
    (&_S2740)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2740, _S2452);
    float _S2741 = (_S2740.differential_0 + _S2458) / _S2449;
    FixedArray<float, 23>  _S2742;
    _S2742[int(0)] = 0.0f;
    _S2742[int(1)] = 0.0f;
    _S2742[int(2)] = 0.0f;
    _S2742[int(3)] = 0.0f;
    _S2742[int(4)] = 0.0f;
    _S2742[int(5)] = 0.0f;
    _S2742[int(6)] = 0.0f;
    _S2742[int(7)] = 0.0f;
    _S2742[int(8)] = 0.0f;
    _S2742[int(9)] = 0.0f;
    _S2742[int(10)] = 0.0f;
    _S2742[int(11)] = 0.0f;
    _S2742[int(12)] = 0.0f;
    _S2742[int(13)] = 0.0f;
    _S2742[int(14)] = 0.0f;
    _S2742[int(15)] = 0.0f;
    _S2742[int(16)] = 0.0f;
    _S2742[int(17)] = 0.0f;
    _S2742[int(18)] = 0.0f;
    _S2742[int(19)] = 0.0f;
    _S2742[int(20)] = 0.0f;
    _S2742[int(21)] = 0.0f;
    _S2742[int(22)] = 0.0f;
    _S2742[int(0)] = _S2741;
    FixedArray<float, 23>  _S2743 = {
        _S2715 + _S2742[int(0)], _S2716 + _S2742[int(1)], _S2717 + _S2742[int(2)], _S2718 + _S2742[int(3)], _S2719 + _S2742[int(4)], _S2720 + _S2742[int(5)], _S2721 + _S2742[int(6)], _S2722 + _S2742[int(7)], _S2723 + _S2742[int(8)], _S2724 + _S2742[int(9)], _S2725 + _S2742[int(10)], _S2726 + _S2742[int(11)], _S2727 + _S2742[int(12)], _S2728 + _S2742[int(13)], _S2729 + _S2742[int(14)], _S2730 + _S2742[int(15)], _S2731 + _S2742[int(16)], _S2732 + _S2742[int(17)], _S2733 + _S2742[int(18)], _S2734 + _S2742[int(19)], _S2735 + _S2742[int(20)], _S2736 + _S2742[int(21)], _S2737 + _S2742[int(22)]
    };
    dpraw_losses_1->primal_0 = dpraw_losses_1->primal_0;
    dpraw_losses_1->differential_0 = _S2743;
    return;
}

inline __device__ void s_bwd_compute_ppisp_rqs_regularization_loss_0(DiffPair_arrayx3Cfloatx2C23x3E_0 * _S2744, int _S2745, FixedArray<float, 6>  * _S2746, FixedArray<float, 6>  * _S2747)
{
    s_bwd_prop_compute_ppisp_rqs_regularization_loss_0(_S2744, _S2745, _S2746, _S2747);
    return;
}

inline __device__ void compute_ppisp_rqs_regularization_loss_vjp(FixedArray<float, 23>  raw_losses_3, int num_cameras_5, FixedArray<float, 6>  loss_weights_5, FixedArray<float, 6>  grad_out_9, FixedArray<float, 23>  * _S2748)
{
    FixedArray<float, 23>  _S2749 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    DiffPair_arrayx3Cfloatx2C23x3E_0 dp_raw_losses_1;
    (&dp_raw_losses_1)->primal_0 = raw_losses_3;
    (&dp_raw_losses_1)->differential_0 = _S2749;
    FixedArray<float, 6>  _S2750 = loss_weights_5;
    FixedArray<float, 6>  _S2751 = grad_out_9;
    s_bwd_compute_ppisp_rqs_regularization_loss_0(&dp_raw_losses_1, num_cameras_5, &_S2750, &_S2751);
    *_S2748 = (&dp_raw_losses_1)->differential_0;
    return;
}

inline __device__ void compute_ppisp_no_crf_regularization_loss(FixedArray<float, 18>  raw_losses_4, int num_cameras_6, FixedArray<float, 6>  loss_weights_6, FixedArray<float, 6>  * _S2752)
{
    float _S2753;
    FixedArray<float, 6>  losses_6;
    float _S2754 = float(num_cameras_6);
    float _S2755 = raw_losses_4[int(0)] / _S2754;
    for(;;)
    {
        float _S2756 = (F32_abs((_S2755)));
        if(_S2756 < 0.10000000149011612f)
        {
            _S2753 = 0.5f * _S2755 * _S2755 / 0.10000000149011612f;
            break;
        }
        else
        {
            _S2753 = _S2756 - 0.05000000074505806f;
            break;
        }
    }
    losses_6[int(0)] = _S2753;
    losses_6[int(1)] = raw_losses_4[int(1)] / (3.0f * _S2754);
    losses_6[int(2)] = (raw_losses_4[int(2)] + raw_losses_4[int(3)] + raw_losses_4[int(4)]) / (9.0f * _S2754);
    losses_6[int(3)] = (raw_losses_4[int(5)] + raw_losses_4[int(6)] + raw_losses_4[int(7)] + raw_losses_4[int(8)] + raw_losses_4[int(9)]) / (5.0f * _S2754);
    float _S2757 = raw_losses_4[int(10)] / _S2754;
    for(;;)
    {
        float _S2758 = (F32_abs((_S2757)));
        if(_S2758 < 0.00499999988824129f)
        {
            _S2753 = 0.5f * _S2757 * _S2757 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2753 = _S2758 - 0.00249999994412065f;
            break;
        }
    }
    float _S2759;
    float _S2760 = raw_losses_4[int(11)] / _S2754;
    for(;;)
    {
        float _S2761 = (F32_abs((_S2760)));
        if(_S2761 < 0.00499999988824129f)
        {
            _S2759 = 0.5f * _S2760 * _S2760 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2759 = _S2761 - 0.00249999994412065f;
            break;
        }
    }
    float _S2762 = _S2753 + _S2759;
    float _S2763 = raw_losses_4[int(12)] / _S2754;
    for(;;)
    {
        float _S2764 = (F32_abs((_S2763)));
        if(_S2764 < 0.00499999988824129f)
        {
            _S2753 = 0.5f * _S2763 * _S2763 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2753 = _S2764 - 0.00249999994412065f;
            break;
        }
    }
    float _S2765 = _S2762 + _S2753;
    float _S2766 = raw_losses_4[int(13)] / _S2754;
    for(;;)
    {
        float _S2767 = (F32_abs((_S2766)));
        if(_S2767 < 0.00499999988824129f)
        {
            _S2753 = 0.5f * _S2766 * _S2766 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2753 = _S2767 - 0.00249999994412065f;
            break;
        }
    }
    float _S2768 = _S2765 + _S2753;
    float _S2769 = raw_losses_4[int(14)] / _S2754;
    for(;;)
    {
        float _S2770 = (F32_abs((_S2769)));
        if(_S2770 < 0.00499999988824129f)
        {
            _S2753 = 0.5f * _S2769 * _S2769 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2753 = _S2770 - 0.00249999994412065f;
            break;
        }
    }
    float _S2771 = _S2768 + _S2753;
    float _S2772 = raw_losses_4[int(15)] / _S2754;
    for(;;)
    {
        float _S2773 = (F32_abs((_S2772)));
        if(_S2773 < 0.00499999988824129f)
        {
            _S2753 = 0.5f * _S2772 * _S2772 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2753 = _S2773 - 0.00249999994412065f;
            break;
        }
    }
    float _S2774 = _S2771 + _S2753;
    float _S2775 = raw_losses_4[int(16)] / _S2754;
    for(;;)
    {
        float _S2776 = (F32_abs((_S2775)));
        if(_S2776 < 0.00499999988824129f)
        {
            _S2753 = 0.5f * _S2775 * _S2775 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2753 = _S2776 - 0.00249999994412065f;
            break;
        }
    }
    float _S2777 = _S2774 + _S2753;
    float _S2778 = raw_losses_4[int(17)] / _S2754;
    for(;;)
    {
        float _S2779 = (F32_abs((_S2778)));
        if(_S2779 < 0.00499999988824129f)
        {
            _S2753 = 0.5f * _S2778 * _S2778 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S2753 = _S2779 - 0.00249999994412065f;
            break;
        }
    }
    float _S2780 = (_S2777 + _S2753) / 8.0f;
    losses_6[int(5)] = 0.0f;
    losses_6[int(0)] = losses_6[int(0)] * loss_weights_6[int(0)];
    losses_6[int(1)] = losses_6[int(1)] * loss_weights_6[int(1)];
    losses_6[int(2)] = losses_6[int(2)] * loss_weights_6[int(2)];
    losses_6[int(3)] = losses_6[int(3)] * loss_weights_6[int(3)];
    losses_6[int(4)] = _S2780 * loss_weights_6[int(4)];
    *_S2752 = losses_6;
    return;
}

struct DiffPair_arrayx3Cfloatx2C18x3E_0
{
    FixedArray<float, 18>  primal_0;
    FixedArray<float, 18>  differential_0;
};

inline __device__ void s_bwd_prop_compute_ppisp_no_crf_regularization_loss_0(DiffPair_arrayx3Cfloatx2C18x3E_0 * dpraw_losses_2, int num_cameras_7, FixedArray<float, 6>  * loss_weights_7, FixedArray<float, 6>  * _s_dOut_10)
{
    FixedArray<float, 18>  _S2781 = dpraw_losses_2->primal_0;
    float _S2782 = float(num_cameras_7);
    float _S2783 = dpraw_losses_2->primal_0[int(0)] / _S2782;
    bool _S2784 = (s_primal_ctx_abs_0(_S2783)) < 0.10000000149011612f;
    float _S2785;
    if(_S2784)
    {
        _S2785 = 0.5f * _S2783;
    }
    else
    {
        _S2785 = 0.0f;
    }
    float _S2786 = 3.0f * _S2782;
    float _S2787 = 9.0f * _S2782;
    float _S2788 = 5.0f * _S2782;
    float _S2789 = _S2781[int(10)] / _S2782;
    bool _S2790 = (s_primal_ctx_abs_0(_S2789)) < 0.00499999988824129f;
    float _S2791;
    if(_S2790)
    {
        _S2791 = 0.5f * _S2789;
    }
    else
    {
        _S2791 = 0.0f;
    }
    float _S2792 = _S2781[int(11)] / _S2782;
    bool _S2793 = (s_primal_ctx_abs_0(_S2792)) < 0.00499999988824129f;
    float _S2794;
    if(_S2793)
    {
        _S2794 = 0.5f * _S2792;
    }
    else
    {
        _S2794 = 0.0f;
    }
    float _S2795 = _S2781[int(12)] / _S2782;
    bool _S2796 = (s_primal_ctx_abs_0(_S2795)) < 0.00499999988824129f;
    float _S2797;
    if(_S2796)
    {
        _S2797 = 0.5f * _S2795;
    }
    else
    {
        _S2797 = 0.0f;
    }
    float _S2798 = _S2781[int(13)] / _S2782;
    bool _S2799 = (s_primal_ctx_abs_0(_S2798)) < 0.00499999988824129f;
    float _S2800;
    if(_S2799)
    {
        _S2800 = 0.5f * _S2798;
    }
    else
    {
        _S2800 = 0.0f;
    }
    float _S2801 = _S2781[int(14)] / _S2782;
    bool _S2802 = (s_primal_ctx_abs_0(_S2801)) < 0.00499999988824129f;
    float _S2803;
    if(_S2802)
    {
        _S2803 = 0.5f * _S2801;
    }
    else
    {
        _S2803 = 0.0f;
    }
    float _S2804 = _S2781[int(15)] / _S2782;
    bool _S2805 = (s_primal_ctx_abs_0(_S2804)) < 0.00499999988824129f;
    float _S2806;
    if(_S2805)
    {
        _S2806 = 0.5f * _S2804;
    }
    else
    {
        _S2806 = 0.0f;
    }
    float _S2807 = _S2781[int(16)] / _S2782;
    bool _S2808 = (s_primal_ctx_abs_0(_S2807)) < 0.00499999988824129f;
    float _S2809;
    if(_S2808)
    {
        _S2809 = 0.5f * _S2807;
    }
    else
    {
        _S2809 = 0.0f;
    }
    float _S2810 = _S2781[int(17)] / _S2782;
    bool _S2811 = (s_primal_ctx_abs_0(_S2810)) < 0.00499999988824129f;
    float _S2812;
    if(_S2811)
    {
        _S2812 = 0.5f * _S2810;
    }
    else
    {
        _S2812 = 0.0f;
    }
    float _S2813 = (*loss_weights_7)[int(3)] * (*_s_dOut_10)[int(3)];
    float _S2814 = (*loss_weights_7)[int(2)] * (*_s_dOut_10)[int(2)];
    float _S2815 = (*loss_weights_7)[int(1)] * (*_s_dOut_10)[int(1)];
    float _S2816 = (*loss_weights_7)[int(0)] * (*_s_dOut_10)[int(0)];
    float _S2817 = 0.125f * ((*loss_weights_7)[int(4)] * (*_s_dOut_10)[int(4)]);
    float _S2818;
    if(_S2811)
    {
        float _S2819 = 200.0f * _S2817;
        float _S2820 = _S2812 * _S2819 + 0.5f * (_S2810 * _S2819);
        _S2812 = 0.0f;
        _S2818 = _S2820;
    }
    else
    {
        _S2812 = _S2817;
        _S2818 = 0.0f;
    }
    DiffPair_float_0 _S2821;
    (&_S2821)->primal_0 = _S2810;
    (&_S2821)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2821, _S2812);
    float _S2822 = (_S2821.differential_0 + _S2818) / _S2782;
    FixedArray<float, 18>  _S2823;
    _S2823[int(0)] = 0.0f;
    _S2823[int(1)] = 0.0f;
    _S2823[int(2)] = 0.0f;
    _S2823[int(3)] = 0.0f;
    _S2823[int(4)] = 0.0f;
    _S2823[int(5)] = 0.0f;
    _S2823[int(6)] = 0.0f;
    _S2823[int(7)] = 0.0f;
    _S2823[int(8)] = 0.0f;
    _S2823[int(9)] = 0.0f;
    _S2823[int(10)] = 0.0f;
    _S2823[int(11)] = 0.0f;
    _S2823[int(12)] = 0.0f;
    _S2823[int(13)] = 0.0f;
    _S2823[int(14)] = 0.0f;
    _S2823[int(15)] = 0.0f;
    _S2823[int(16)] = 0.0f;
    _S2823[int(17)] = 0.0f;
    _S2823[int(17)] = _S2822;
    float _S2824 = _S2823[int(0)];
    float _S2825 = _S2823[int(1)];
    float _S2826 = _S2823[int(2)];
    float _S2827 = _S2823[int(3)];
    float _S2828 = _S2823[int(4)];
    float _S2829 = _S2823[int(5)];
    float _S2830 = _S2823[int(6)];
    float _S2831 = _S2823[int(7)];
    float _S2832 = _S2823[int(8)];
    float _S2833 = _S2823[int(9)];
    float _S2834 = _S2823[int(10)];
    float _S2835 = _S2823[int(11)];
    float _S2836 = _S2823[int(12)];
    float _S2837 = _S2823[int(13)];
    float _S2838 = _S2823[int(14)];
    float _S2839 = _S2823[int(15)];
    float _S2840 = _S2823[int(16)];
    float _S2841 = _S2823[int(17)];
    if(_S2808)
    {
        float _S2842 = 200.0f * _S2817;
        float _S2843 = _S2809 * _S2842 + 0.5f * (_S2807 * _S2842);
        _S2809 = 0.0f;
        _S2812 = _S2843;
    }
    else
    {
        _S2809 = _S2817;
        _S2812 = 0.0f;
    }
    DiffPair_float_0 _S2844;
    (&_S2844)->primal_0 = _S2807;
    (&_S2844)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2844, _S2809);
    float _S2845 = (_S2844.differential_0 + _S2812) / _S2782;
    FixedArray<float, 18>  _S2846;
    _S2846[int(0)] = 0.0f;
    _S2846[int(1)] = 0.0f;
    _S2846[int(2)] = 0.0f;
    _S2846[int(3)] = 0.0f;
    _S2846[int(4)] = 0.0f;
    _S2846[int(5)] = 0.0f;
    _S2846[int(6)] = 0.0f;
    _S2846[int(7)] = 0.0f;
    _S2846[int(8)] = 0.0f;
    _S2846[int(9)] = 0.0f;
    _S2846[int(10)] = 0.0f;
    _S2846[int(11)] = 0.0f;
    _S2846[int(12)] = 0.0f;
    _S2846[int(13)] = 0.0f;
    _S2846[int(14)] = 0.0f;
    _S2846[int(15)] = 0.0f;
    _S2846[int(16)] = 0.0f;
    _S2846[int(17)] = 0.0f;
    _S2846[int(16)] = _S2845;
    float _S2847 = _S2824 + _S2846[int(0)];
    float _S2848 = _S2825 + _S2846[int(1)];
    float _S2849 = _S2826 + _S2846[int(2)];
    float _S2850 = _S2827 + _S2846[int(3)];
    float _S2851 = _S2828 + _S2846[int(4)];
    float _S2852 = _S2829 + _S2846[int(5)];
    float _S2853 = _S2830 + _S2846[int(6)];
    float _S2854 = _S2831 + _S2846[int(7)];
    float _S2855 = _S2832 + _S2846[int(8)];
    float _S2856 = _S2833 + _S2846[int(9)];
    float _S2857 = _S2834 + _S2846[int(10)];
    float _S2858 = _S2835 + _S2846[int(11)];
    float _S2859 = _S2836 + _S2846[int(12)];
    float _S2860 = _S2837 + _S2846[int(13)];
    float _S2861 = _S2838 + _S2846[int(14)];
    float _S2862 = _S2839 + _S2846[int(15)];
    float _S2863 = _S2840 + _S2846[int(16)];
    float _S2864 = _S2841 + _S2846[int(17)];
    if(_S2805)
    {
        float _S2865 = 200.0f * _S2817;
        float _S2866 = _S2806 * _S2865 + 0.5f * (_S2804 * _S2865);
        _S2806 = 0.0f;
        _S2809 = _S2866;
    }
    else
    {
        _S2806 = _S2817;
        _S2809 = 0.0f;
    }
    DiffPair_float_0 _S2867;
    (&_S2867)->primal_0 = _S2804;
    (&_S2867)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2867, _S2806);
    float _S2868 = (_S2867.differential_0 + _S2809) / _S2782;
    FixedArray<float, 18>  _S2869;
    _S2869[int(0)] = 0.0f;
    _S2869[int(1)] = 0.0f;
    _S2869[int(2)] = 0.0f;
    _S2869[int(3)] = 0.0f;
    _S2869[int(4)] = 0.0f;
    _S2869[int(5)] = 0.0f;
    _S2869[int(6)] = 0.0f;
    _S2869[int(7)] = 0.0f;
    _S2869[int(8)] = 0.0f;
    _S2869[int(9)] = 0.0f;
    _S2869[int(10)] = 0.0f;
    _S2869[int(11)] = 0.0f;
    _S2869[int(12)] = 0.0f;
    _S2869[int(13)] = 0.0f;
    _S2869[int(14)] = 0.0f;
    _S2869[int(15)] = 0.0f;
    _S2869[int(16)] = 0.0f;
    _S2869[int(17)] = 0.0f;
    _S2869[int(15)] = _S2868;
    float _S2870 = _S2847 + _S2869[int(0)];
    float _S2871 = _S2848 + _S2869[int(1)];
    float _S2872 = _S2849 + _S2869[int(2)];
    float _S2873 = _S2850 + _S2869[int(3)];
    float _S2874 = _S2851 + _S2869[int(4)];
    float _S2875 = _S2852 + _S2869[int(5)];
    float _S2876 = _S2853 + _S2869[int(6)];
    float _S2877 = _S2854 + _S2869[int(7)];
    float _S2878 = _S2855 + _S2869[int(8)];
    float _S2879 = _S2856 + _S2869[int(9)];
    float _S2880 = _S2857 + _S2869[int(10)];
    float _S2881 = _S2858 + _S2869[int(11)];
    float _S2882 = _S2859 + _S2869[int(12)];
    float _S2883 = _S2860 + _S2869[int(13)];
    float _S2884 = _S2861 + _S2869[int(14)];
    float _S2885 = _S2862 + _S2869[int(15)];
    float _S2886 = _S2863 + _S2869[int(16)];
    float _S2887 = _S2864 + _S2869[int(17)];
    if(_S2802)
    {
        float _S2888 = 200.0f * _S2817;
        float _S2889 = _S2803 * _S2888 + 0.5f * (_S2801 * _S2888);
        _S2803 = 0.0f;
        _S2806 = _S2889;
    }
    else
    {
        _S2803 = _S2817;
        _S2806 = 0.0f;
    }
    DiffPair_float_0 _S2890;
    (&_S2890)->primal_0 = _S2801;
    (&_S2890)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2890, _S2803);
    float _S2891 = (_S2890.differential_0 + _S2806) / _S2782;
    FixedArray<float, 18>  _S2892;
    _S2892[int(0)] = 0.0f;
    _S2892[int(1)] = 0.0f;
    _S2892[int(2)] = 0.0f;
    _S2892[int(3)] = 0.0f;
    _S2892[int(4)] = 0.0f;
    _S2892[int(5)] = 0.0f;
    _S2892[int(6)] = 0.0f;
    _S2892[int(7)] = 0.0f;
    _S2892[int(8)] = 0.0f;
    _S2892[int(9)] = 0.0f;
    _S2892[int(10)] = 0.0f;
    _S2892[int(11)] = 0.0f;
    _S2892[int(12)] = 0.0f;
    _S2892[int(13)] = 0.0f;
    _S2892[int(14)] = 0.0f;
    _S2892[int(15)] = 0.0f;
    _S2892[int(16)] = 0.0f;
    _S2892[int(17)] = 0.0f;
    _S2892[int(14)] = _S2891;
    float _S2893 = _S2870 + _S2892[int(0)];
    float _S2894 = _S2871 + _S2892[int(1)];
    float _S2895 = _S2872 + _S2892[int(2)];
    float _S2896 = _S2873 + _S2892[int(3)];
    float _S2897 = _S2874 + _S2892[int(4)];
    float _S2898 = _S2875 + _S2892[int(5)];
    float _S2899 = _S2876 + _S2892[int(6)];
    float _S2900 = _S2877 + _S2892[int(7)];
    float _S2901 = _S2878 + _S2892[int(8)];
    float _S2902 = _S2879 + _S2892[int(9)];
    float _S2903 = _S2880 + _S2892[int(10)];
    float _S2904 = _S2881 + _S2892[int(11)];
    float _S2905 = _S2882 + _S2892[int(12)];
    float _S2906 = _S2883 + _S2892[int(13)];
    float _S2907 = _S2884 + _S2892[int(14)];
    float _S2908 = _S2885 + _S2892[int(15)];
    float _S2909 = _S2886 + _S2892[int(16)];
    float _S2910 = _S2887 + _S2892[int(17)];
    if(_S2799)
    {
        float _S2911 = 200.0f * _S2817;
        float _S2912 = _S2800 * _S2911 + 0.5f * (_S2798 * _S2911);
        _S2800 = 0.0f;
        _S2803 = _S2912;
    }
    else
    {
        _S2800 = _S2817;
        _S2803 = 0.0f;
    }
    DiffPair_float_0 _S2913;
    (&_S2913)->primal_0 = _S2798;
    (&_S2913)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2913, _S2800);
    float _S2914 = (_S2913.differential_0 + _S2803) / _S2782;
    FixedArray<float, 18>  _S2915;
    _S2915[int(0)] = 0.0f;
    _S2915[int(1)] = 0.0f;
    _S2915[int(2)] = 0.0f;
    _S2915[int(3)] = 0.0f;
    _S2915[int(4)] = 0.0f;
    _S2915[int(5)] = 0.0f;
    _S2915[int(6)] = 0.0f;
    _S2915[int(7)] = 0.0f;
    _S2915[int(8)] = 0.0f;
    _S2915[int(9)] = 0.0f;
    _S2915[int(10)] = 0.0f;
    _S2915[int(11)] = 0.0f;
    _S2915[int(12)] = 0.0f;
    _S2915[int(13)] = 0.0f;
    _S2915[int(14)] = 0.0f;
    _S2915[int(15)] = 0.0f;
    _S2915[int(16)] = 0.0f;
    _S2915[int(17)] = 0.0f;
    _S2915[int(13)] = _S2914;
    float _S2916 = _S2893 + _S2915[int(0)];
    float _S2917 = _S2894 + _S2915[int(1)];
    float _S2918 = _S2895 + _S2915[int(2)];
    float _S2919 = _S2896 + _S2915[int(3)];
    float _S2920 = _S2897 + _S2915[int(4)];
    float _S2921 = _S2898 + _S2915[int(5)];
    float _S2922 = _S2899 + _S2915[int(6)];
    float _S2923 = _S2900 + _S2915[int(7)];
    float _S2924 = _S2901 + _S2915[int(8)];
    float _S2925 = _S2902 + _S2915[int(9)];
    float _S2926 = _S2903 + _S2915[int(10)];
    float _S2927 = _S2904 + _S2915[int(11)];
    float _S2928 = _S2905 + _S2915[int(12)];
    float _S2929 = _S2906 + _S2915[int(13)];
    float _S2930 = _S2907 + _S2915[int(14)];
    float _S2931 = _S2908 + _S2915[int(15)];
    float _S2932 = _S2909 + _S2915[int(16)];
    float _S2933 = _S2910 + _S2915[int(17)];
    if(_S2796)
    {
        float _S2934 = 200.0f * _S2817;
        float _S2935 = _S2797 * _S2934 + 0.5f * (_S2795 * _S2934);
        _S2797 = 0.0f;
        _S2800 = _S2935;
    }
    else
    {
        _S2797 = _S2817;
        _S2800 = 0.0f;
    }
    DiffPair_float_0 _S2936;
    (&_S2936)->primal_0 = _S2795;
    (&_S2936)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2936, _S2797);
    float _S2937 = (_S2936.differential_0 + _S2800) / _S2782;
    FixedArray<float, 18>  _S2938;
    _S2938[int(0)] = 0.0f;
    _S2938[int(1)] = 0.0f;
    _S2938[int(2)] = 0.0f;
    _S2938[int(3)] = 0.0f;
    _S2938[int(4)] = 0.0f;
    _S2938[int(5)] = 0.0f;
    _S2938[int(6)] = 0.0f;
    _S2938[int(7)] = 0.0f;
    _S2938[int(8)] = 0.0f;
    _S2938[int(9)] = 0.0f;
    _S2938[int(10)] = 0.0f;
    _S2938[int(11)] = 0.0f;
    _S2938[int(12)] = 0.0f;
    _S2938[int(13)] = 0.0f;
    _S2938[int(14)] = 0.0f;
    _S2938[int(15)] = 0.0f;
    _S2938[int(16)] = 0.0f;
    _S2938[int(17)] = 0.0f;
    _S2938[int(12)] = _S2937;
    float _S2939 = _S2916 + _S2938[int(0)];
    float _S2940 = _S2917 + _S2938[int(1)];
    float _S2941 = _S2918 + _S2938[int(2)];
    float _S2942 = _S2919 + _S2938[int(3)];
    float _S2943 = _S2920 + _S2938[int(4)];
    float _S2944 = _S2921 + _S2938[int(5)];
    float _S2945 = _S2922 + _S2938[int(6)];
    float _S2946 = _S2923 + _S2938[int(7)];
    float _S2947 = _S2924 + _S2938[int(8)];
    float _S2948 = _S2925 + _S2938[int(9)];
    float _S2949 = _S2926 + _S2938[int(10)];
    float _S2950 = _S2927 + _S2938[int(11)];
    float _S2951 = _S2928 + _S2938[int(12)];
    float _S2952 = _S2929 + _S2938[int(13)];
    float _S2953 = _S2930 + _S2938[int(14)];
    float _S2954 = _S2931 + _S2938[int(15)];
    float _S2955 = _S2932 + _S2938[int(16)];
    float _S2956 = _S2933 + _S2938[int(17)];
    if(_S2793)
    {
        float _S2957 = 200.0f * _S2817;
        float _S2958 = _S2794 * _S2957 + 0.5f * (_S2792 * _S2957);
        _S2794 = 0.0f;
        _S2797 = _S2958;
    }
    else
    {
        _S2794 = _S2817;
        _S2797 = 0.0f;
    }
    DiffPair_float_0 _S2959;
    (&_S2959)->primal_0 = _S2792;
    (&_S2959)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2959, _S2794);
    float _S2960 = (_S2959.differential_0 + _S2797) / _S2782;
    FixedArray<float, 18>  _S2961;
    _S2961[int(0)] = 0.0f;
    _S2961[int(1)] = 0.0f;
    _S2961[int(2)] = 0.0f;
    _S2961[int(3)] = 0.0f;
    _S2961[int(4)] = 0.0f;
    _S2961[int(5)] = 0.0f;
    _S2961[int(6)] = 0.0f;
    _S2961[int(7)] = 0.0f;
    _S2961[int(8)] = 0.0f;
    _S2961[int(9)] = 0.0f;
    _S2961[int(10)] = 0.0f;
    _S2961[int(11)] = 0.0f;
    _S2961[int(12)] = 0.0f;
    _S2961[int(13)] = 0.0f;
    _S2961[int(14)] = 0.0f;
    _S2961[int(15)] = 0.0f;
    _S2961[int(16)] = 0.0f;
    _S2961[int(17)] = 0.0f;
    _S2961[int(11)] = _S2960;
    float _S2962 = _S2939 + _S2961[int(0)];
    float _S2963 = _S2940 + _S2961[int(1)];
    float _S2964 = _S2941 + _S2961[int(2)];
    float _S2965 = _S2942 + _S2961[int(3)];
    float _S2966 = _S2943 + _S2961[int(4)];
    float _S2967 = _S2944 + _S2961[int(5)];
    float _S2968 = _S2945 + _S2961[int(6)];
    float _S2969 = _S2946 + _S2961[int(7)];
    float _S2970 = _S2947 + _S2961[int(8)];
    float _S2971 = _S2948 + _S2961[int(9)];
    float _S2972 = _S2949 + _S2961[int(10)];
    float _S2973 = _S2950 + _S2961[int(11)];
    float _S2974 = _S2951 + _S2961[int(12)];
    float _S2975 = _S2952 + _S2961[int(13)];
    float _S2976 = _S2953 + _S2961[int(14)];
    float _S2977 = _S2954 + _S2961[int(15)];
    float _S2978 = _S2955 + _S2961[int(16)];
    float _S2979 = _S2956 + _S2961[int(17)];
    if(_S2790)
    {
        float _S2980 = 200.0f * _S2817;
        float _S2981 = _S2791 * _S2980 + 0.5f * (_S2789 * _S2980);
        _S2791 = 0.0f;
        _S2794 = _S2981;
    }
    else
    {
        _S2791 = _S2817;
        _S2794 = 0.0f;
    }
    DiffPair_float_0 _S2982;
    (&_S2982)->primal_0 = _S2789;
    (&_S2982)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S2982, _S2791);
    float _S2983 = (_S2982.differential_0 + _S2794) / _S2782;
    float _S2984 = _S2813 / _S2788;
    float _S2985 = _S2814 / _S2787;
    float _S2986 = _S2815 / _S2786;
    FixedArray<float, 18>  _S2987;
    _S2987[int(0)] = 0.0f;
    _S2987[int(1)] = 0.0f;
    _S2987[int(2)] = 0.0f;
    _S2987[int(3)] = 0.0f;
    _S2987[int(4)] = 0.0f;
    _S2987[int(5)] = 0.0f;
    _S2987[int(6)] = 0.0f;
    _S2987[int(7)] = 0.0f;
    _S2987[int(8)] = 0.0f;
    _S2987[int(9)] = 0.0f;
    _S2987[int(10)] = 0.0f;
    _S2987[int(11)] = 0.0f;
    _S2987[int(12)] = 0.0f;
    _S2987[int(13)] = 0.0f;
    _S2987[int(14)] = 0.0f;
    _S2987[int(15)] = 0.0f;
    _S2987[int(16)] = 0.0f;
    _S2987[int(17)] = 0.0f;
    _S2987[int(10)] = _S2983;
    _S2987[int(9)] = _S2984;
    _S2987[int(8)] = _S2984;
    _S2987[int(7)] = _S2984;
    _S2987[int(6)] = _S2984;
    _S2987[int(5)] = _S2984;
    _S2987[int(4)] = _S2985;
    _S2987[int(3)] = _S2985;
    _S2987[int(2)] = _S2985;
    _S2987[int(1)] = _S2986;
    float _S2988 = _S2962 + _S2987[int(0)];
    float _S2989 = _S2963 + _S2987[int(1)];
    float _S2990 = _S2964 + _S2987[int(2)];
    float _S2991 = _S2965 + _S2987[int(3)];
    float _S2992 = _S2966 + _S2987[int(4)];
    float _S2993 = _S2967 + _S2987[int(5)];
    float _S2994 = _S2968 + _S2987[int(6)];
    float _S2995 = _S2969 + _S2987[int(7)];
    float _S2996 = _S2970 + _S2987[int(8)];
    float _S2997 = _S2971 + _S2987[int(9)];
    float _S2998 = _S2972 + _S2987[int(10)];
    float _S2999 = _S2973 + _S2987[int(11)];
    float _S3000 = _S2974 + _S2987[int(12)];
    float _S3001 = _S2975 + _S2987[int(13)];
    float _S3002 = _S2976 + _S2987[int(14)];
    float _S3003 = _S2977 + _S2987[int(15)];
    float _S3004 = _S2978 + _S2987[int(16)];
    float _S3005 = _S2979 + _S2987[int(17)];
    if(_S2784)
    {
        float _S3006 = 10.0f * _S2816;
        float _S3007 = _S2785 * _S3006 + 0.5f * (_S2783 * _S3006);
        _S2785 = 0.0f;
        _S2791 = _S3007;
    }
    else
    {
        _S2785 = _S2816;
        _S2791 = 0.0f;
    }
    DiffPair_float_0 _S3008;
    (&_S3008)->primal_0 = _S2783;
    (&_S3008)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S3008, _S2785);
    float _S3009 = (_S3008.differential_0 + _S2791) / _S2782;
    FixedArray<float, 18>  _S3010;
    _S3010[int(0)] = 0.0f;
    _S3010[int(1)] = 0.0f;
    _S3010[int(2)] = 0.0f;
    _S3010[int(3)] = 0.0f;
    _S3010[int(4)] = 0.0f;
    _S3010[int(5)] = 0.0f;
    _S3010[int(6)] = 0.0f;
    _S3010[int(7)] = 0.0f;
    _S3010[int(8)] = 0.0f;
    _S3010[int(9)] = 0.0f;
    _S3010[int(10)] = 0.0f;
    _S3010[int(11)] = 0.0f;
    _S3010[int(12)] = 0.0f;
    _S3010[int(13)] = 0.0f;
    _S3010[int(14)] = 0.0f;
    _S3010[int(15)] = 0.0f;
    _S3010[int(16)] = 0.0f;
    _S3010[int(17)] = 0.0f;
    _S3010[int(0)] = _S3009;
    FixedArray<float, 18>  _S3011 = {
        _S2988 + _S3010[int(0)], _S2989 + _S3010[int(1)], _S2990 + _S3010[int(2)], _S2991 + _S3010[int(3)], _S2992 + _S3010[int(4)], _S2993 + _S3010[int(5)], _S2994 + _S3010[int(6)], _S2995 + _S3010[int(7)], _S2996 + _S3010[int(8)], _S2997 + _S3010[int(9)], _S2998 + _S3010[int(10)], _S2999 + _S3010[int(11)], _S3000 + _S3010[int(12)], _S3001 + _S3010[int(13)], _S3002 + _S3010[int(14)], _S3003 + _S3010[int(15)], _S3004 + _S3010[int(16)], _S3005 + _S3010[int(17)]
    };
    dpraw_losses_2->primal_0 = dpraw_losses_2->primal_0;
    dpraw_losses_2->differential_0 = _S3011;
    return;
}

inline __device__ void s_bwd_compute_ppisp_no_crf_regularization_loss_0(DiffPair_arrayx3Cfloatx2C18x3E_0 * _S3012, int _S3013, FixedArray<float, 6>  * _S3014, FixedArray<float, 6>  * _S3015)
{
    s_bwd_prop_compute_ppisp_no_crf_regularization_loss_0(_S3012, _S3013, _S3014, _S3015);
    return;
}

inline __device__ void compute_ppisp_no_crf_regularization_loss_vjp(FixedArray<float, 18>  raw_losses_5, int num_cameras_8, FixedArray<float, 6>  loss_weights_8, FixedArray<float, 6>  grad_out_10, FixedArray<float, 18>  * _S3016)
{
    FixedArray<float, 18>  _S3017 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    DiffPair_arrayx3Cfloatx2C18x3E_0 dp_raw_losses_2;
    (&dp_raw_losses_2)->primal_0 = raw_losses_5;
    (&dp_raw_losses_2)->differential_0 = _S3017;
    FixedArray<float, 6>  _S3018 = loss_weights_8;
    FixedArray<float, 6>  _S3019 = grad_out_10;
    s_bwd_compute_ppisp_no_crf_regularization_loss_0(&dp_raw_losses_2, num_cameras_8, &_S3018, &_S3019);
    *_S3016 = (&dp_raw_losses_2)->differential_0;
    return;
}

inline __device__ void compute_ppisp_no_crf_no_vig_regularization_loss(FixedArray<float, 9>  raw_losses_6, int num_cameras_9, FixedArray<float, 6>  loss_weights_9, FixedArray<float, 6>  * _S3020)
{
    float _S3021;
    FixedArray<float, 6>  losses_7;
    float _S3022 = float(num_cameras_9);
    float _S3023 = raw_losses_6[int(0)] / _S3022;
    for(;;)
    {
        float _S3024 = (F32_abs((_S3023)));
        if(_S3024 < 0.10000000149011612f)
        {
            _S3021 = 0.5f * _S3023 * _S3023 / 0.10000000149011612f;
            break;
        }
        else
        {
            _S3021 = _S3024 - 0.05000000074505806f;
            break;
        }
    }
    losses_7[int(0)] = _S3021;
    float _S3025 = raw_losses_6[int(1)] / _S3022;
    for(;;)
    {
        float _S3026 = (F32_abs((_S3025)));
        if(_S3026 < 0.00499999988824129f)
        {
            _S3021 = 0.5f * _S3025 * _S3025 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S3021 = _S3026 - 0.00249999994412065f;
            break;
        }
    }
    float _S3027;
    float _S3028 = raw_losses_6[int(2)] / _S3022;
    for(;;)
    {
        float _S3029 = (F32_abs((_S3028)));
        if(_S3029 < 0.00499999988824129f)
        {
            _S3027 = 0.5f * _S3028 * _S3028 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S3027 = _S3029 - 0.00249999994412065f;
            break;
        }
    }
    float _S3030 = _S3021 + _S3027;
    float _S3031 = raw_losses_6[int(3)] / _S3022;
    for(;;)
    {
        float _S3032 = (F32_abs((_S3031)));
        if(_S3032 < 0.00499999988824129f)
        {
            _S3021 = 0.5f * _S3031 * _S3031 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S3021 = _S3032 - 0.00249999994412065f;
            break;
        }
    }
    float _S3033 = _S3030 + _S3021;
    float _S3034 = raw_losses_6[int(4)] / _S3022;
    for(;;)
    {
        float _S3035 = (F32_abs((_S3034)));
        if(_S3035 < 0.00499999988824129f)
        {
            _S3021 = 0.5f * _S3034 * _S3034 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S3021 = _S3035 - 0.00249999994412065f;
            break;
        }
    }
    float _S3036 = _S3033 + _S3021;
    float _S3037 = raw_losses_6[int(5)] / _S3022;
    for(;;)
    {
        float _S3038 = (F32_abs((_S3037)));
        if(_S3038 < 0.00499999988824129f)
        {
            _S3021 = 0.5f * _S3037 * _S3037 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S3021 = _S3038 - 0.00249999994412065f;
            break;
        }
    }
    float _S3039 = _S3036 + _S3021;
    float _S3040 = raw_losses_6[int(6)] / _S3022;
    for(;;)
    {
        float _S3041 = (F32_abs((_S3040)));
        if(_S3041 < 0.00499999988824129f)
        {
            _S3021 = 0.5f * _S3040 * _S3040 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S3021 = _S3041 - 0.00249999994412065f;
            break;
        }
    }
    float _S3042 = _S3039 + _S3021;
    float _S3043 = raw_losses_6[int(7)] / _S3022;
    for(;;)
    {
        float _S3044 = (F32_abs((_S3043)));
        if(_S3044 < 0.00499999988824129f)
        {
            _S3021 = 0.5f * _S3043 * _S3043 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S3021 = _S3044 - 0.00249999994412065f;
            break;
        }
    }
    float _S3045 = _S3042 + _S3021;
    float _S3046 = raw_losses_6[int(8)] / _S3022;
    for(;;)
    {
        float _S3047 = (F32_abs((_S3046)));
        if(_S3047 < 0.00499999988824129f)
        {
            _S3021 = 0.5f * _S3046 * _S3046 / 0.00499999988824129f;
            break;
        }
        else
        {
            _S3021 = _S3047 - 0.00249999994412065f;
            break;
        }
    }
    float _S3048 = (_S3045 + _S3021) / 8.0f;
    losses_7[int(1)] = 0.0f;
    losses_7[int(2)] = 0.0f;
    losses_7[int(3)] = 0.0f;
    losses_7[int(5)] = 0.0f;
    losses_7[int(0)] = losses_7[int(0)] * loss_weights_9[int(0)];
    losses_7[int(4)] = _S3048 * loss_weights_9[int(4)];
    *_S3020 = losses_7;
    return;
}

inline __device__ void s_bwd_prop_compute_ppisp_no_crf_no_vig_regularization_loss_0(DiffPair_arrayx3Cfloatx2C9x3E_0 * dpraw_losses_3, int num_cameras_10, FixedArray<float, 6>  * loss_weights_10, FixedArray<float, 6>  * _s_dOut_11)
{
    FixedArray<float, 9>  _S3049 = dpraw_losses_3->primal_0;
    float _S3050 = float(num_cameras_10);
    float _S3051 = dpraw_losses_3->primal_0[int(0)] / _S3050;
    bool _S3052 = (s_primal_ctx_abs_0(_S3051)) < 0.10000000149011612f;
    float _S3053;
    if(_S3052)
    {
        _S3053 = 0.5f * _S3051;
    }
    else
    {
        _S3053 = 0.0f;
    }
    float _S3054 = _S3049[int(1)] / _S3050;
    bool _S3055 = (s_primal_ctx_abs_0(_S3054)) < 0.00499999988824129f;
    float _S3056;
    if(_S3055)
    {
        _S3056 = 0.5f * _S3054;
    }
    else
    {
        _S3056 = 0.0f;
    }
    float _S3057 = _S3049[int(2)] / _S3050;
    bool _S3058 = (s_primal_ctx_abs_0(_S3057)) < 0.00499999988824129f;
    float _S3059;
    if(_S3058)
    {
        _S3059 = 0.5f * _S3057;
    }
    else
    {
        _S3059 = 0.0f;
    }
    float _S3060 = _S3049[int(3)] / _S3050;
    bool _S3061 = (s_primal_ctx_abs_0(_S3060)) < 0.00499999988824129f;
    float _S3062;
    if(_S3061)
    {
        _S3062 = 0.5f * _S3060;
    }
    else
    {
        _S3062 = 0.0f;
    }
    float _S3063 = _S3049[int(4)] / _S3050;
    bool _S3064 = (s_primal_ctx_abs_0(_S3063)) < 0.00499999988824129f;
    float _S3065;
    if(_S3064)
    {
        _S3065 = 0.5f * _S3063;
    }
    else
    {
        _S3065 = 0.0f;
    }
    float _S3066 = _S3049[int(5)] / _S3050;
    bool _S3067 = (s_primal_ctx_abs_0(_S3066)) < 0.00499999988824129f;
    float _S3068;
    if(_S3067)
    {
        _S3068 = 0.5f * _S3066;
    }
    else
    {
        _S3068 = 0.0f;
    }
    float _S3069 = _S3049[int(6)] / _S3050;
    bool _S3070 = (s_primal_ctx_abs_0(_S3069)) < 0.00499999988824129f;
    float _S3071;
    if(_S3070)
    {
        _S3071 = 0.5f * _S3069;
    }
    else
    {
        _S3071 = 0.0f;
    }
    float _S3072 = _S3049[int(7)] / _S3050;
    bool _S3073 = (s_primal_ctx_abs_0(_S3072)) < 0.00499999988824129f;
    float _S3074;
    if(_S3073)
    {
        _S3074 = 0.5f * _S3072;
    }
    else
    {
        _S3074 = 0.0f;
    }
    float _S3075 = _S3049[int(8)] / _S3050;
    bool _S3076 = (s_primal_ctx_abs_0(_S3075)) < 0.00499999988824129f;
    float _S3077;
    if(_S3076)
    {
        _S3077 = 0.5f * _S3075;
    }
    else
    {
        _S3077 = 0.0f;
    }
    float _S3078 = (*loss_weights_10)[int(0)] * (*_s_dOut_11)[int(0)];
    float _S3079 = 0.125f * ((*loss_weights_10)[int(4)] * (*_s_dOut_11)[int(4)]);
    float _S3080;
    if(_S3076)
    {
        float _S3081 = 200.0f * _S3079;
        float _S3082 = _S3077 * _S3081 + 0.5f * (_S3075 * _S3081);
        _S3077 = 0.0f;
        _S3080 = _S3082;
    }
    else
    {
        _S3077 = _S3079;
        _S3080 = 0.0f;
    }
    DiffPair_float_0 _S3083;
    (&_S3083)->primal_0 = _S3075;
    (&_S3083)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S3083, _S3077);
    float _S3084 = (_S3083.differential_0 + _S3080) / _S3050;
    FixedArray<float, 9>  _S3085;
    _S3085[int(0)] = 0.0f;
    _S3085[int(1)] = 0.0f;
    _S3085[int(2)] = 0.0f;
    _S3085[int(3)] = 0.0f;
    _S3085[int(4)] = 0.0f;
    _S3085[int(5)] = 0.0f;
    _S3085[int(6)] = 0.0f;
    _S3085[int(7)] = 0.0f;
    _S3085[int(8)] = 0.0f;
    _S3085[int(8)] = _S3084;
    float _S3086 = _S3085[int(0)];
    float _S3087 = _S3085[int(1)];
    float _S3088 = _S3085[int(2)];
    float _S3089 = _S3085[int(3)];
    float _S3090 = _S3085[int(4)];
    float _S3091 = _S3085[int(5)];
    float _S3092 = _S3085[int(6)];
    float _S3093 = _S3085[int(7)];
    float _S3094 = _S3085[int(8)];
    if(_S3073)
    {
        float _S3095 = 200.0f * _S3079;
        float _S3096 = _S3074 * _S3095 + 0.5f * (_S3072 * _S3095);
        _S3074 = 0.0f;
        _S3077 = _S3096;
    }
    else
    {
        _S3074 = _S3079;
        _S3077 = 0.0f;
    }
    DiffPair_float_0 _S3097;
    (&_S3097)->primal_0 = _S3072;
    (&_S3097)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S3097, _S3074);
    float _S3098 = (_S3097.differential_0 + _S3077) / _S3050;
    FixedArray<float, 9>  _S3099;
    _S3099[int(0)] = 0.0f;
    _S3099[int(1)] = 0.0f;
    _S3099[int(2)] = 0.0f;
    _S3099[int(3)] = 0.0f;
    _S3099[int(4)] = 0.0f;
    _S3099[int(5)] = 0.0f;
    _S3099[int(6)] = 0.0f;
    _S3099[int(7)] = 0.0f;
    _S3099[int(8)] = 0.0f;
    _S3099[int(7)] = _S3098;
    float _S3100 = _S3086 + _S3099[int(0)];
    float _S3101 = _S3087 + _S3099[int(1)];
    float _S3102 = _S3088 + _S3099[int(2)];
    float _S3103 = _S3089 + _S3099[int(3)];
    float _S3104 = _S3090 + _S3099[int(4)];
    float _S3105 = _S3091 + _S3099[int(5)];
    float _S3106 = _S3092 + _S3099[int(6)];
    float _S3107 = _S3093 + _S3099[int(7)];
    float _S3108 = _S3094 + _S3099[int(8)];
    if(_S3070)
    {
        float _S3109 = 200.0f * _S3079;
        float _S3110 = _S3071 * _S3109 + 0.5f * (_S3069 * _S3109);
        _S3071 = 0.0f;
        _S3074 = _S3110;
    }
    else
    {
        _S3071 = _S3079;
        _S3074 = 0.0f;
    }
    DiffPair_float_0 _S3111;
    (&_S3111)->primal_0 = _S3069;
    (&_S3111)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S3111, _S3071);
    float _S3112 = (_S3111.differential_0 + _S3074) / _S3050;
    FixedArray<float, 9>  _S3113;
    _S3113[int(0)] = 0.0f;
    _S3113[int(1)] = 0.0f;
    _S3113[int(2)] = 0.0f;
    _S3113[int(3)] = 0.0f;
    _S3113[int(4)] = 0.0f;
    _S3113[int(5)] = 0.0f;
    _S3113[int(6)] = 0.0f;
    _S3113[int(7)] = 0.0f;
    _S3113[int(8)] = 0.0f;
    _S3113[int(6)] = _S3112;
    float _S3114 = _S3100 + _S3113[int(0)];
    float _S3115 = _S3101 + _S3113[int(1)];
    float _S3116 = _S3102 + _S3113[int(2)];
    float _S3117 = _S3103 + _S3113[int(3)];
    float _S3118 = _S3104 + _S3113[int(4)];
    float _S3119 = _S3105 + _S3113[int(5)];
    float _S3120 = _S3106 + _S3113[int(6)];
    float _S3121 = _S3107 + _S3113[int(7)];
    float _S3122 = _S3108 + _S3113[int(8)];
    if(_S3067)
    {
        float _S3123 = 200.0f * _S3079;
        float _S3124 = _S3068 * _S3123 + 0.5f * (_S3066 * _S3123);
        _S3068 = 0.0f;
        _S3071 = _S3124;
    }
    else
    {
        _S3068 = _S3079;
        _S3071 = 0.0f;
    }
    DiffPair_float_0 _S3125;
    (&_S3125)->primal_0 = _S3066;
    (&_S3125)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S3125, _S3068);
    float _S3126 = (_S3125.differential_0 + _S3071) / _S3050;
    FixedArray<float, 9>  _S3127;
    _S3127[int(0)] = 0.0f;
    _S3127[int(1)] = 0.0f;
    _S3127[int(2)] = 0.0f;
    _S3127[int(3)] = 0.0f;
    _S3127[int(4)] = 0.0f;
    _S3127[int(5)] = 0.0f;
    _S3127[int(6)] = 0.0f;
    _S3127[int(7)] = 0.0f;
    _S3127[int(8)] = 0.0f;
    _S3127[int(5)] = _S3126;
    float _S3128 = _S3114 + _S3127[int(0)];
    float _S3129 = _S3115 + _S3127[int(1)];
    float _S3130 = _S3116 + _S3127[int(2)];
    float _S3131 = _S3117 + _S3127[int(3)];
    float _S3132 = _S3118 + _S3127[int(4)];
    float _S3133 = _S3119 + _S3127[int(5)];
    float _S3134 = _S3120 + _S3127[int(6)];
    float _S3135 = _S3121 + _S3127[int(7)];
    float _S3136 = _S3122 + _S3127[int(8)];
    if(_S3064)
    {
        float _S3137 = 200.0f * _S3079;
        float _S3138 = _S3065 * _S3137 + 0.5f * (_S3063 * _S3137);
        _S3065 = 0.0f;
        _S3068 = _S3138;
    }
    else
    {
        _S3065 = _S3079;
        _S3068 = 0.0f;
    }
    DiffPair_float_0 _S3139;
    (&_S3139)->primal_0 = _S3063;
    (&_S3139)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S3139, _S3065);
    float _S3140 = (_S3139.differential_0 + _S3068) / _S3050;
    FixedArray<float, 9>  _S3141;
    _S3141[int(0)] = 0.0f;
    _S3141[int(1)] = 0.0f;
    _S3141[int(2)] = 0.0f;
    _S3141[int(3)] = 0.0f;
    _S3141[int(4)] = 0.0f;
    _S3141[int(5)] = 0.0f;
    _S3141[int(6)] = 0.0f;
    _S3141[int(7)] = 0.0f;
    _S3141[int(8)] = 0.0f;
    _S3141[int(4)] = _S3140;
    float _S3142 = _S3128 + _S3141[int(0)];
    float _S3143 = _S3129 + _S3141[int(1)];
    float _S3144 = _S3130 + _S3141[int(2)];
    float _S3145 = _S3131 + _S3141[int(3)];
    float _S3146 = _S3132 + _S3141[int(4)];
    float _S3147 = _S3133 + _S3141[int(5)];
    float _S3148 = _S3134 + _S3141[int(6)];
    float _S3149 = _S3135 + _S3141[int(7)];
    float _S3150 = _S3136 + _S3141[int(8)];
    if(_S3061)
    {
        float _S3151 = 200.0f * _S3079;
        float _S3152 = _S3062 * _S3151 + 0.5f * (_S3060 * _S3151);
        _S3062 = 0.0f;
        _S3065 = _S3152;
    }
    else
    {
        _S3062 = _S3079;
        _S3065 = 0.0f;
    }
    DiffPair_float_0 _S3153;
    (&_S3153)->primal_0 = _S3060;
    (&_S3153)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S3153, _S3062);
    float _S3154 = (_S3153.differential_0 + _S3065) / _S3050;
    FixedArray<float, 9>  _S3155;
    _S3155[int(0)] = 0.0f;
    _S3155[int(1)] = 0.0f;
    _S3155[int(2)] = 0.0f;
    _S3155[int(3)] = 0.0f;
    _S3155[int(4)] = 0.0f;
    _S3155[int(5)] = 0.0f;
    _S3155[int(6)] = 0.0f;
    _S3155[int(7)] = 0.0f;
    _S3155[int(8)] = 0.0f;
    _S3155[int(3)] = _S3154;
    float _S3156 = _S3142 + _S3155[int(0)];
    float _S3157 = _S3143 + _S3155[int(1)];
    float _S3158 = _S3144 + _S3155[int(2)];
    float _S3159 = _S3145 + _S3155[int(3)];
    float _S3160 = _S3146 + _S3155[int(4)];
    float _S3161 = _S3147 + _S3155[int(5)];
    float _S3162 = _S3148 + _S3155[int(6)];
    float _S3163 = _S3149 + _S3155[int(7)];
    float _S3164 = _S3150 + _S3155[int(8)];
    if(_S3058)
    {
        float _S3165 = 200.0f * _S3079;
        float _S3166 = _S3059 * _S3165 + 0.5f * (_S3057 * _S3165);
        _S3059 = 0.0f;
        _S3062 = _S3166;
    }
    else
    {
        _S3059 = _S3079;
        _S3062 = 0.0f;
    }
    DiffPair_float_0 _S3167;
    (&_S3167)->primal_0 = _S3057;
    (&_S3167)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S3167, _S3059);
    float _S3168 = (_S3167.differential_0 + _S3062) / _S3050;
    FixedArray<float, 9>  _S3169;
    _S3169[int(0)] = 0.0f;
    _S3169[int(1)] = 0.0f;
    _S3169[int(2)] = 0.0f;
    _S3169[int(3)] = 0.0f;
    _S3169[int(4)] = 0.0f;
    _S3169[int(5)] = 0.0f;
    _S3169[int(6)] = 0.0f;
    _S3169[int(7)] = 0.0f;
    _S3169[int(8)] = 0.0f;
    _S3169[int(2)] = _S3168;
    float _S3170 = _S3156 + _S3169[int(0)];
    float _S3171 = _S3157 + _S3169[int(1)];
    float _S3172 = _S3158 + _S3169[int(2)];
    float _S3173 = _S3159 + _S3169[int(3)];
    float _S3174 = _S3160 + _S3169[int(4)];
    float _S3175 = _S3161 + _S3169[int(5)];
    float _S3176 = _S3162 + _S3169[int(6)];
    float _S3177 = _S3163 + _S3169[int(7)];
    float _S3178 = _S3164 + _S3169[int(8)];
    if(_S3055)
    {
        float _S3179 = 200.0f * _S3079;
        float _S3180 = _S3056 * _S3179 + 0.5f * (_S3054 * _S3179);
        _S3056 = 0.0f;
        _S3059 = _S3180;
    }
    else
    {
        _S3056 = _S3079;
        _S3059 = 0.0f;
    }
    DiffPair_float_0 _S3181;
    (&_S3181)->primal_0 = _S3054;
    (&_S3181)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S3181, _S3056);
    float _S3182 = (_S3181.differential_0 + _S3059) / _S3050;
    FixedArray<float, 9>  _S3183;
    _S3183[int(0)] = 0.0f;
    _S3183[int(1)] = 0.0f;
    _S3183[int(2)] = 0.0f;
    _S3183[int(3)] = 0.0f;
    _S3183[int(4)] = 0.0f;
    _S3183[int(5)] = 0.0f;
    _S3183[int(6)] = 0.0f;
    _S3183[int(7)] = 0.0f;
    _S3183[int(8)] = 0.0f;
    _S3183[int(1)] = _S3182;
    float _S3184 = _S3170 + _S3183[int(0)];
    float _S3185 = _S3171 + _S3183[int(1)];
    float _S3186 = _S3172 + _S3183[int(2)];
    float _S3187 = _S3173 + _S3183[int(3)];
    float _S3188 = _S3174 + _S3183[int(4)];
    float _S3189 = _S3175 + _S3183[int(5)];
    float _S3190 = _S3176 + _S3183[int(6)];
    float _S3191 = _S3177 + _S3183[int(7)];
    float _S3192 = _S3178 + _S3183[int(8)];
    if(_S3052)
    {
        float _S3193 = 10.0f * _S3078;
        float _S3194 = _S3053 * _S3193 + 0.5f * (_S3051 * _S3193);
        _S3053 = 0.0f;
        _S3056 = _S3194;
    }
    else
    {
        _S3053 = _S3078;
        _S3056 = 0.0f;
    }
    DiffPair_float_0 _S3195;
    (&_S3195)->primal_0 = _S3051;
    (&_S3195)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S3195, _S3053);
    float _S3196 = (_S3195.differential_0 + _S3056) / _S3050;
    FixedArray<float, 9>  _S3197;
    _S3197[int(0)] = 0.0f;
    _S3197[int(1)] = 0.0f;
    _S3197[int(2)] = 0.0f;
    _S3197[int(3)] = 0.0f;
    _S3197[int(4)] = 0.0f;
    _S3197[int(5)] = 0.0f;
    _S3197[int(6)] = 0.0f;
    _S3197[int(7)] = 0.0f;
    _S3197[int(8)] = 0.0f;
    _S3197[int(0)] = _S3196;
    FixedArray<float, 9>  _S3198 = {
        _S3184 + _S3197[int(0)], _S3185 + _S3197[int(1)], _S3186 + _S3197[int(2)], _S3187 + _S3197[int(3)], _S3188 + _S3197[int(4)], _S3189 + _S3197[int(5)], _S3190 + _S3197[int(6)], _S3191 + _S3197[int(7)], _S3192 + _S3197[int(8)]
    };
    dpraw_losses_3->primal_0 = dpraw_losses_3->primal_0;
    dpraw_losses_3->differential_0 = _S3198;
    return;
}

inline __device__ void s_bwd_compute_ppisp_no_crf_no_vig_regularization_loss_0(DiffPair_arrayx3Cfloatx2C9x3E_0 * _S3199, int _S3200, FixedArray<float, 6>  * _S3201, FixedArray<float, 6>  * _S3202)
{
    s_bwd_prop_compute_ppisp_no_crf_no_vig_regularization_loss_0(_S3199, _S3200, _S3201, _S3202);
    return;
}

inline __device__ void compute_ppisp_no_crf_no_vig_regularization_loss_vjp(FixedArray<float, 9>  raw_losses_7, int num_cameras_11, FixedArray<float, 6>  loss_weights_11, FixedArray<float, 6>  grad_out_11, FixedArray<float, 9>  * _S3203)
{
    FixedArray<float, 9>  _S3204 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    DiffPair_arrayx3Cfloatx2C9x3E_0 dp_raw_losses_3;
    (&dp_raw_losses_3)->primal_0 = raw_losses_7;
    (&dp_raw_losses_3)->differential_0 = _S3204;
    FixedArray<float, 6>  _S3205 = loss_weights_11;
    FixedArray<float, 6>  _S3206 = grad_out_11;
    s_bwd_compute_ppisp_no_crf_no_vig_regularization_loss_0(&dp_raw_losses_3, num_cameras_11, &_S3205, &_S3206);
    *_S3203 = (&dp_raw_losses_3)->differential_0;
    return;
}

