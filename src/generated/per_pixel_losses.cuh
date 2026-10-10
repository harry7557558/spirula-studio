#pragma once

#include "generated/slang.cuh"

struct s_bwdCallableCtx_lerp_0
{
    float _S1;
    float _S2;
    float _S3;
};

struct DiffPair_float_0
{
    float primal_0;
    float differential_0;
};

inline __device__ void _d_lerp_0(DiffPair_float_0 * dpx_0, DiffPair_float_0 * dpy_0, DiffPair_float_0 * dps_0, float dOut_0)
{
    float _S4 = (1.0f - (*dps_0).primal_0) * dOut_0;
    dpx_0->primal_0 = (*dpx_0).primal_0;
    dpx_0->differential_0 = _S4;
    DiffPair_float_0 _S5 = *dpy_0;
    float _S6 = (*dps_0).primal_0 * dOut_0;
    dpy_0->primal_0 = (*dpy_0).primal_0;
    dpy_0->differential_0 = _S6;
    float _S7 = (_S5.primal_0 - (*dpx_0).primal_0) * dOut_0;
    dps_0->primal_0 = _S5.primal_0;
    dps_0->differential_0 = _S7;
    return;
}

inline __device__ void s_bwdProp_lerp_0(s_bwdCallableCtx_lerp_0 * ctx_0, float * d_x_0, float * d_y_0, float * d_s_0, float d_out_0)
{
    DiffPair_float_0 _S8;
    (&_S8)->primal_0 = ctx_0->_S1;
    DiffPair_float_0 _S9;
    (&_S9)->primal_0 = ctx_0->_S2;
    DiffPair_float_0 _S10;
    (&_S10)->primal_0 = ctx_0->_S3;
    _d_lerp_0(&_S8, &_S9, &_S10, d_out_0);
    *d_x_0 = (&_S8)->differential_0;
    *d_y_0 = (&_S9)->differential_0;
    *d_s_0 = (&_S10)->differential_0;
    return;
}

struct Tuple_0
{
    float value0_0;
};

inline __device__ Tuple_0 s_apply_rsqrt_0(float x_0)
{
    Tuple_0 _S11 = { (F32_rsqrt((x_0))) };
    return _S11;
}

struct s_bwdCallableCtx_rsqrt_0
{
    float _S12;
};

inline __device__ void _d_rsqrt_0(DiffPair_float_0 * dpx_1, float dOut_1)
{
    float _S13 = -0.5f / ((*dpx_1).primal_0 * (F32_sqrt(((*dpx_1).primal_0)))) * dOut_1;
    dpx_1->primal_0 = (*dpx_1).primal_0;
    dpx_1->differential_0 = _S13;
    return;
}

inline __device__ void s_bwdProp_rsqrt_0(s_bwdCallableCtx_rsqrt_0 * ctx_1, float * d_x_1, float d_out_1)
{
    DiffPair_float_0 _S14;
    (&_S14)->primal_0 = ctx_1->_S12;
    _d_rsqrt_0(&_S14, d_out_1);
    *d_x_1 = (&_S14)->differential_0;
    return;
}

struct s_bwdCallableCtx_abs_0
{
    float3  _S15;
};

struct DiffPair_vectorx3Cfloatx2C3x3E_0
{
    float3  primal_0;
    float3  differential_0;
};

inline __device__ void _d_abs_vector_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_2, float3  dOut_2)
{
    float3  _S16 = _slang_select(((*dpx_2).primal_0) > make_float3 (0.0f), make_float3 (1.0f),_slang_select(((*dpx_2).primal_0) == make_float3 (0.0f), make_float3 (0.0f),make_float3 (-1.0f))) * dOut_2;
    dpx_2->primal_0 = (*dpx_2).primal_0;
    dpx_2->differential_0 = _S16;
    return;
}

inline __device__ void s_bwdProp_abs_0(s_bwdCallableCtx_abs_0 * ctx_2, float3  * d_x_2, float3  d_out_2)
{
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S17;
    (&_S17)->primal_0 = ctx_2->_S15;
    _d_abs_vector_0(&_S17, d_out_2);
    *d_x_2 = (&_S17)->differential_0;
    return;
}

struct Tuple_1
{
    float value0_1;
};

inline __device__ Tuple_1 s_apply_sqrt_0(float x_1)
{
    Tuple_1 _S18 = { (F32_sqrt((x_1))) };
    return _S18;
}

struct s_bwdCallableCtx_sqrt_0
{
    float _S19;
};

inline __device__ void _d_sqrt_0(DiffPair_float_0 * dpx_3, float dOut_3)
{
    float _S20 = 0.5f / (F32_sqrt(((F32_max((1.00000001168609742e-07f), ((*dpx_3).primal_0)))))) * dOut_3;
    dpx_3->primal_0 = (*dpx_3).primal_0;
    dpx_3->differential_0 = _S20;
    return;
}

inline __device__ void s_bwdProp_sqrt_0(s_bwdCallableCtx_sqrt_0 * ctx_3, float * d_x_3, float d_out_3)
{
    DiffPair_float_0 _S21;
    (&_S21)->primal_0 = ctx_3->_S19;
    _d_sqrt_0(&_S21, d_out_3);
    *d_x_3 = (&_S21)->differential_0;
    return;
}

struct s_bwdCallableCtx_log10_0
{
    float _S22;
};

inline __device__ void _d_log10_0(DiffPair_float_0 * dpx_4, float dOut_4)
{
    float _S23 = 1.0f / ((*dpx_4).primal_0 * 2.30258512496948242f) * dOut_4;
    dpx_4->primal_0 = (*dpx_4).primal_0;
    dpx_4->differential_0 = _S23;
    return;
}

inline __device__ void s_bwdProp_log10_0(s_bwdCallableCtx_log10_0 * ctx_4, float * d_x_4, float d_out_4)
{
    DiffPair_float_0 _S24;
    (&_S24)->primal_0 = ctx_4->_S22;
    _d_log10_0(&_S24, d_out_4);
    *d_x_4 = (&_S24)->differential_0;
    return;
}

struct Tuple_2
{
    float value0_2;
};

inline __device__ Tuple_2 s_apply_log_0(float x_2)
{
    Tuple_2 _S25 = { (F32_log((x_2))) };
    return _S25;
}

struct s_bwdCallableCtx_log_0
{
    float _S26;
};

inline __device__ void _d_log_0(DiffPair_float_0 * dpx_5, float dOut_5)
{
    float _S27 = 1.0f / (*dpx_5).primal_0 * dOut_5;
    dpx_5->primal_0 = (*dpx_5).primal_0;
    dpx_5->differential_0 = _S27;
    return;
}

inline __device__ void s_bwdProp_log_0(s_bwdCallableCtx_log_0 * ctx_5, float * d_x_5, float d_out_5)
{
    DiffPair_float_0 _S28;
    (&_S28)->primal_0 = ctx_5->_S26;
    _d_log_0(&_S28, d_out_5);
    *d_x_5 = (&_S28)->differential_0;
    return;
}

struct Tuple_3
{
    float value0_3;
};

inline __device__ Tuple_3 s_apply_max_0(float x_3, float y_0)
{
    Tuple_3 _S29 = { (F32_max((x_3), (y_0))) };
    return _S29;
}

struct s_bwdCallableCtx_max_0
{
    float _S30;
    float _S31;
};

inline __device__ void _d_max_0(DiffPair_float_0 * dpx_6, DiffPair_float_0 * dpy_1, float dOut_6)
{
    DiffPair_float_0 _S32 = *dpx_6;
    float _S33;
    if(((*dpx_6).primal_0) > ((*dpy_1).primal_0))
    {
        _S33 = dOut_6;
    }
    else
    {
        if(((*dpx_6).primal_0) < ((*dpy_1).primal_0))
        {
            _S33 = 0.0f;
        }
        else
        {
            _S33 = 0.5f * dOut_6;
        }
    }
    dpx_6->primal_0 = _S32.primal_0;
    dpx_6->differential_0 = _S33;
    DiffPair_float_0 _S34 = *dpy_1;
    if(((*dpy_1).primal_0) > (_S32.primal_0))
    {
        _S33 = dOut_6;
    }
    else
    {
        if(((*dpy_1).primal_0) < ((*dpx_6).primal_0))
        {
            _S33 = 0.0f;
        }
        else
        {
            _S33 = 0.5f * dOut_6;
        }
    }
    dpy_1->primal_0 = _S34.primal_0;
    dpy_1->differential_0 = _S33;
    return;
}

inline __device__ void s_bwdProp_max_0(s_bwdCallableCtx_max_0 * ctx_6, float * d_x_6, float * d_y_1, float d_out_6)
{
    DiffPair_float_0 _S35;
    (&_S35)->primal_0 = ctx_6->_S30;
    DiffPair_float_0 _S36;
    (&_S36)->primal_0 = ctx_6->_S31;
    _d_max_0(&_S35, &_S36, d_out_6);
    *d_x_6 = (&_S35)->differential_0;
    *d_y_1 = (&_S36)->differential_0;
    return;
}

struct Tuple_4
{
    float value0_4;
};

inline __device__ float clamp_0(float x_4, float minBound_0, float maxBound_0)
{
    return (F32_min(((F32_max((x_4), (minBound_0)))), (maxBound_0)));
}

inline __device__ Tuple_4 s_apply_clamp_0(float x_5, float minBound_1, float maxBound_1)
{
    Tuple_4 _S37 = { clamp_0(x_5, minBound_1, maxBound_1) };
    return _S37;
}

struct s_bwdCallableCtx_clamp_0
{
    float _S38;
    float _S39;
    float _S40;
};

inline __device__ void _d_clamp_0(DiffPair_float_0 * dpx_7, DiffPair_float_0 * dpMin_0, DiffPair_float_0 * dpMax_0, float dOut_7)
{
    DiffPair_float_0 _S41 = *dpx_7;
    bool _S42;
    if(((*dpx_7).primal_0) >= ((*dpMin_0).primal_0))
    {
        _S42 = ((*dpx_7).primal_0) <= ((*dpMax_0).primal_0);
    }
    else
    {
        _S42 = false;
    }
    float _S43;
    if(_S42)
    {
        _S43 = dOut_7;
    }
    else
    {
        _S43 = 0.0f;
    }
    dpx_7->primal_0 = _S41.primal_0;
    dpx_7->differential_0 = _S43;
    DiffPair_float_0 _S44 = *dpMin_0;
    if((_S41.primal_0) < ((*dpMin_0).primal_0))
    {
        _S43 = dOut_7;
    }
    else
    {
        _S43 = 0.0f;
    }
    dpMin_0->primal_0 = _S44.primal_0;
    dpMin_0->differential_0 = _S43;
    DiffPair_float_0 _S45 = *dpMax_0;
    if(((*dpx_7).primal_0) > ((*dpMax_0).primal_0))
    {
        _S43 = dOut_7;
    }
    else
    {
        _S43 = 0.0f;
    }
    dpMax_0->primal_0 = _S45.primal_0;
    dpMax_0->differential_0 = _S43;
    return;
}

inline __device__ void s_bwdProp_clamp_0(s_bwdCallableCtx_clamp_0 * ctx_7, float * d_x_7, float * d_minBound_0, float * d_maxBound_0, float d_out_7)
{
    DiffPair_float_0 _S46;
    (&_S46)->primal_0 = ctx_7->_S38;
    DiffPair_float_0 _S47;
    (&_S47)->primal_0 = ctx_7->_S39;
    DiffPair_float_0 _S48;
    (&_S48)->primal_0 = ctx_7->_S40;
    _d_clamp_0(&_S46, &_S47, &_S48, d_out_7);
    *d_x_7 = (&_S46)->differential_0;
    *d_minBound_0 = (&_S47)->differential_0;
    *d_maxBound_0 = (&_S48)->differential_0;
    return;
}

struct s_bwdCallableCtx_abs_1
{
    float _S49;
};

inline __device__ void _d_abs_0(DiffPair_float_0 * dpx_8, float dOut_8)
{
    float _S50 = _slang_select(((*dpx_8).primal_0) > 0.0f, 1.0f,_slang_select(((*dpx_8).primal_0) == 0.0f, 0.0f,-1.0f)) * dOut_8;
    dpx_8->primal_0 = (*dpx_8).primal_0;
    dpx_8->differential_0 = _S50;
    return;
}

inline __device__ void s_bwdProp_abs_1(s_bwdCallableCtx_abs_1 * ctx_8, float * d_x_8, float d_out_8)
{
    DiffPair_float_0 _S51;
    (&_S51)->primal_0 = ctx_8->_S49;
    _d_abs_0(&_S51, d_out_8);
    *d_x_8 = (&_S51)->differential_0;
    return;
}

struct Tuple_5
{
    float value0_5;
};

inline __device__ float dot_0(float3  x_6, float3  y_1)
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
        float result_1 = result_0 + _slang_vector_get_element(x_6, i_0) * _slang_vector_get_element(y_1, i_0);
        i_0 = i_0 + int(1);
        result_0 = result_1;
    }
    return result_0;
}

inline __device__ Tuple_5 s_apply_dot_0(float3  x_7, float3  y_2)
{
    Tuple_5 _S52 = { dot_0(x_7, y_2) };
    return _S52;
}

struct s_bwdCallableCtx_dot_0
{
    float3  _S53;
    float3  _S54;
};

inline __device__ void _d_dot_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_9, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpy_2, float dOut_9)
{
    float3  x_d_result_0;
    *&((&x_d_result_0)->x) = (*dpy_2).primal_0.x * dOut_9;
    float3  y_d_result_0;
    *&((&y_d_result_0)->x) = (*dpx_9).primal_0.x * dOut_9;
    *&((&x_d_result_0)->y) = (*dpy_2).primal_0.y * dOut_9;
    *&((&y_d_result_0)->y) = (*dpx_9).primal_0.y * dOut_9;
    *&((&x_d_result_0)->z) = (*dpy_2).primal_0.z * dOut_9;
    *&((&y_d_result_0)->z) = (*dpx_9).primal_0.z * dOut_9;
    dpx_9->primal_0 = (*dpx_9).primal_0;
    dpx_9->differential_0 = x_d_result_0;
    dpy_2->primal_0 = (*dpy_2).primal_0;
    dpy_2->differential_0 = y_d_result_0;
    return;
}

inline __device__ void s_bwdProp_dot_0(s_bwdCallableCtx_dot_0 * ctx_9, float3  * d_x_9, float3  * d_y_2, float d_out_9)
{
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S55;
    (&_S55)->primal_0 = ctx_9->_S53;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S56;
    (&_S56)->primal_0 = ctx_9->_S54;
    _d_dot_0(&_S55, &_S56, d_out_9);
    *d_x_9 = (&_S55)->differential_0;
    *d_y_2 = (&_S56)->differential_0;
    return;
}

struct Tuple_6
{
    float value0_6;
};

inline __device__ Tuple_6 s_apply_min_0(float x_8, float y_3)
{
    Tuple_6 _S57 = { (F32_min((x_8), (y_3))) };
    return _S57;
}

struct s_bwdCallableCtx_min_0
{
    float _S58;
    float _S59;
};

inline __device__ void _d_min_0(DiffPair_float_0 * dpx_10, DiffPair_float_0 * dpy_3, float dOut_10)
{
    DiffPair_float_0 _S60 = *dpx_10;
    float _S61;
    if(((*dpx_10).primal_0) < ((*dpy_3).primal_0))
    {
        _S61 = dOut_10;
    }
    else
    {
        if(((*dpx_10).primal_0) > ((*dpy_3).primal_0))
        {
            _S61 = 0.0f;
        }
        else
        {
            _S61 = 0.5f * dOut_10;
        }
    }
    dpx_10->primal_0 = _S60.primal_0;
    dpx_10->differential_0 = _S61;
    DiffPair_float_0 _S62 = *dpy_3;
    if(((*dpy_3).primal_0) < (_S60.primal_0))
    {
        _S61 = dOut_10;
    }
    else
    {
        if(((*dpy_3).primal_0) > ((*dpx_10).primal_0))
        {
            _S61 = 0.0f;
        }
        else
        {
            _S61 = 0.5f * dOut_10;
        }
    }
    dpy_3->primal_0 = _S62.primal_0;
    dpy_3->differential_0 = _S61;
    return;
}

inline __device__ void s_bwdProp_min_0(s_bwdCallableCtx_min_0 * ctx_10, float * d_x_10, float * d_y_3, float d_out_10)
{
    DiffPair_float_0 _S63;
    (&_S63)->primal_0 = ctx_10->_S58;
    DiffPair_float_0 _S64;
    (&_S64)->primal_0 = ctx_10->_S59;
    _d_min_0(&_S63, &_S64, d_out_10);
    *d_x_10 = (&_S63)->differential_0;
    *d_y_3 = (&_S64)->differential_0;
    return;
}

inline __device__ void dadd_0(FixedArray<float, 32>  * a_0, FixedArray<float, 32>  * b_0, FixedArray<float, 32>  * _S65)
{
    FixedArray<float, 32>  result_2;
    result_2[int(0)] = (*a_0)[int(0)] + (*b_0)[int(0)];
    result_2[int(1)] = (*a_0)[int(1)] + (*b_0)[int(1)];
    result_2[int(2)] = (*a_0)[int(2)] + (*b_0)[int(2)];
    result_2[int(3)] = (*a_0)[int(3)] + (*b_0)[int(3)];
    result_2[int(4)] = (*a_0)[int(4)] + (*b_0)[int(4)];
    result_2[int(5)] = (*a_0)[int(5)] + (*b_0)[int(5)];
    result_2[int(6)] = (*a_0)[int(6)] + (*b_0)[int(6)];
    result_2[int(7)] = (*a_0)[int(7)] + (*b_0)[int(7)];
    result_2[int(8)] = (*a_0)[int(8)] + (*b_0)[int(8)];
    result_2[int(9)] = (*a_0)[int(9)] + (*b_0)[int(9)];
    result_2[int(10)] = (*a_0)[int(10)] + (*b_0)[int(10)];
    result_2[int(11)] = (*a_0)[int(11)] + (*b_0)[int(11)];
    result_2[int(12)] = (*a_0)[int(12)] + (*b_0)[int(12)];
    result_2[int(13)] = (*a_0)[int(13)] + (*b_0)[int(13)];
    result_2[int(14)] = (*a_0)[int(14)] + (*b_0)[int(14)];
    result_2[int(15)] = (*a_0)[int(15)] + (*b_0)[int(15)];
    result_2[int(16)] = (*a_0)[int(16)] + (*b_0)[int(16)];
    result_2[int(17)] = (*a_0)[int(17)] + (*b_0)[int(17)];
    result_2[int(18)] = (*a_0)[int(18)] + (*b_0)[int(18)];
    result_2[int(19)] = (*a_0)[int(19)] + (*b_0)[int(19)];
    result_2[int(20)] = (*a_0)[int(20)] + (*b_0)[int(20)];
    result_2[int(21)] = (*a_0)[int(21)] + (*b_0)[int(21)];
    result_2[int(22)] = (*a_0)[int(22)] + (*b_0)[int(22)];
    result_2[int(23)] = (*a_0)[int(23)] + (*b_0)[int(23)];
    result_2[int(24)] = (*a_0)[int(24)] + (*b_0)[int(24)];
    result_2[int(25)] = (*a_0)[int(25)] + (*b_0)[int(25)];
    result_2[int(26)] = (*a_0)[int(26)] + (*b_0)[int(26)];
    result_2[int(27)] = (*a_0)[int(27)] + (*b_0)[int(27)];
    result_2[int(28)] = (*a_0)[int(28)] + (*b_0)[int(28)];
    result_2[int(29)] = (*a_0)[int(29)] + (*b_0)[int(29)];
    result_2[int(30)] = (*a_0)[int(30)] + (*b_0)[int(30)];
    result_2[int(31)] = (*a_0)[int(31)] + (*b_0)[int(31)];
    *_S65 = result_2;
    return;
}

inline __device__ void dzero_0(FixedArray<float, 32>  * _S66)
{
    (*_S66)[int(0)] = 0.0f;
    (*_S66)[int(1)] = 0.0f;
    (*_S66)[int(2)] = 0.0f;
    (*_S66)[int(3)] = 0.0f;
    (*_S66)[int(4)] = 0.0f;
    (*_S66)[int(5)] = 0.0f;
    (*_S66)[int(6)] = 0.0f;
    (*_S66)[int(7)] = 0.0f;
    (*_S66)[int(8)] = 0.0f;
    (*_S66)[int(9)] = 0.0f;
    (*_S66)[int(10)] = 0.0f;
    (*_S66)[int(11)] = 0.0f;
    (*_S66)[int(12)] = 0.0f;
    (*_S66)[int(13)] = 0.0f;
    (*_S66)[int(14)] = 0.0f;
    (*_S66)[int(15)] = 0.0f;
    (*_S66)[int(16)] = 0.0f;
    (*_S66)[int(17)] = 0.0f;
    (*_S66)[int(18)] = 0.0f;
    (*_S66)[int(19)] = 0.0f;
    (*_S66)[int(20)] = 0.0f;
    (*_S66)[int(21)] = 0.0f;
    (*_S66)[int(22)] = 0.0f;
    (*_S66)[int(23)] = 0.0f;
    (*_S66)[int(24)] = 0.0f;
    (*_S66)[int(25)] = 0.0f;
    (*_S66)[int(26)] = 0.0f;
    (*_S66)[int(27)] = 0.0f;
    (*_S66)[int(28)] = 0.0f;
    (*_S66)[int(29)] = 0.0f;
    (*_S66)[int(30)] = 0.0f;
    (*_S66)[int(31)] = 0.0f;
    return;
}

struct DiffPair_arrayx3Cfloatx2C32x3E_0
{
    FixedArray<float, 32>  primal_0;
    FixedArray<float, 32>  differential_0;
};

struct s_paramCtx_s_bwdCallableCtx_per_pixel_losses_reduce_0
{
    FixedArray<float, 32>  value0_7;
    FixedArray<float, 19>  value1_0;
};

struct s_bwdCallableCtx_per_pixel_losses_reduce_0
{
    s_paramCtx_s_bwdCallableCtx_per_pixel_losses_reduce_0 value0_8;
};

inline __device__ void s_bwdProp_per_pixel_losses_reduce_0(s_bwdCallableCtx_per_pixel_losses_reduce_0 * _S67, FixedArray<float, 32>  * _S68, FixedArray<float, 14>  * _S69)
{
    float _S70 = (&_S67->value0_8)->value0_7[int(0)];
    Tuple_3 _S71 = s_apply_max_0((&_S67->value0_8)->value0_7[int(21)], 1.0f);
    s_bwdCallableCtx_max_0 _S72;
    (&_S72)->_S30 = (&_S67->value0_8)->value0_7[int(21)];
    (&_S72)->_S31 = 1.0f;
    s_bwdCallableCtx_max_0 _S73 = _S72;
    float _S74 = _S71.value0_3 * _S71.value0_3;
    float _S75 = (&_S67->value0_8)->value0_7[int(1)];
    s_bwdCallableCtx_log10_0 _S76;
    (&_S76)->_S22 = (&_S67->value0_8)->value0_7[int(1)] / _S71.value0_3;
    s_bwdCallableCtx_log10_0 _S77 = _S76;
    float _S78 = (&_S67->value0_8)->value0_7[int(22)];
    bool _S79 = ((&_S67->value0_8)->value0_7[int(22)]) > 0.0f;
    bool _S80;
    if(_S79)
    {
        _S80 = ((&_S67->value0_8)->value0_7[int(3)]) != 0.0f;
    }
    else
    {
        _S80 = false;
    }
    float _S81;
    float _S82;
    float _S83;
    float _S84;
    float _S85;
    float _S86;
    float _S87;
    float _S88;
    float _S89;
    float _S90;
    float _S91;
    float _S92;
    s_bwdCallableCtx_clamp_0 _S93;
    s_bwdCallableCtx_sqrt_0 _S94;
    s_bwdCallableCtx_max_0 _S95;
    if(_S80)
    {
        float _S96 = (&_S67->value0_8)->value0_7[int(2)] * (&_S67->value0_8)->value0_7[int(3)];
        float _S97 = _S78 * _S78;
        float _S98 = (&_S67->value0_8)->value0_7[int(6)] - _S96 / _S78;
        float _S99 = (&_S67->value0_8)->value0_7[int(2)] * (&_S67->value0_8)->value0_7[int(2)];
        float _S100 = (&_S67->value0_8)->value0_7[int(4)] - _S99 / _S78;
        float _S101 = (&_S67->value0_8)->value0_7[int(3)] * (&_S67->value0_8)->value0_7[int(3)];
        float _S102 = (&_S67->value0_8)->value0_7[int(5)] - _S101 / _S78;
        float _S103 = _S100 * _S102 + 1.0f;
        Tuple_3 _S104 = s_apply_max_0(9.999999960041972e-13f, _S103);
        s_bwdCallableCtx_max_0 _S105;
        (&_S105)->_S30 = 9.999999960041972e-13f;
        (&_S105)->_S31 = _S103;
        Tuple_1 _S106 = s_apply_sqrt_0(_S104.value0_3);
        s_bwdCallableCtx_sqrt_0 _S107;
        (&_S107)->_S19 = _S104.value0_3;
        float _S108 = _S106.value0_1 * _S106.value0_1;
        s_bwdCallableCtx_clamp_0 _S109;
        (&_S109)->_S38 = 1.0f - _S98 / _S106.value0_1;
        (&_S109)->_S39 = 0.0f;
        (&_S109)->_S40 = 2.0f;
        _S81 = (&_S67->value0_8)->value1_0[int(6)];
        _S93 = _S109;
        _S82 = _S108;
        _S83 = _S98;
        _S84 = _S106.value0_1;
        _S94 = _S107;
        _S95 = _S105;
        _S85 = _S100;
        _S86 = _S102;
        _S87 = _S97;
        _S88 = _S101;
        _S89 = (&_S67->value0_8)->value0_7[int(3)];
        _S90 = _S99;
        _S91 = (&_S67->value0_8)->value0_7[int(2)];
        _S92 = _S96;
    }
    else
    {
        _S81 = 0.0f;
        (&_S93)->_S38 = 0.0f;
        (&_S93)->_S39 = 0.0f;
        (&_S93)->_S40 = 0.0f;
        _S82 = 0.0f;
        _S83 = 0.0f;
        _S84 = 0.0f;
        (&_S94)->_S19 = 0.0f;
        (&_S95)->_S30 = 0.0f;
        (&_S95)->_S31 = 0.0f;
        _S85 = 0.0f;
        _S86 = 0.0f;
        _S87 = 0.0f;
        _S88 = 0.0f;
        _S89 = 0.0f;
        _S90 = 0.0f;
        _S91 = 0.0f;
        _S92 = 0.0f;
    }
    float _S110 = (&_S67->value0_8)->value0_7[int(7)];
    Tuple_3 _S111 = s_apply_max_0((&_S67->value0_8)->value0_7[int(23)], 1.0f);
    s_bwdCallableCtx_max_0 _S112;
    (&_S112)->_S30 = (&_S67->value0_8)->value0_7[int(23)];
    (&_S112)->_S31 = 1.0f;
    s_bwdCallableCtx_max_0 _S113 = _S112;
    float _S114 = _S111.value0_3 * _S111.value0_3;
    float _S115 = (&_S67->value0_8)->value0_7[int(8)];
    Tuple_3 _S116 = s_apply_max_0((&_S67->value0_8)->value0_7[int(24)], 1.0f);
    s_bwdCallableCtx_max_0 _S117;
    (&_S117)->_S30 = (&_S67->value0_8)->value0_7[int(24)];
    (&_S117)->_S31 = 1.0f;
    s_bwdCallableCtx_max_0 _S118 = _S117;
    float _S119 = _S116.value0_3 * _S116.value0_3;
    float _S120 = float((I32_max((int(((&_S67->value0_8)->value0_7[int(23)]) > 0.5f) + int(((&_S67->value0_8)->value0_7[int(24)]) > 0.5f)), (int(1)))));
    float _S121 = (&_S67->value0_8)->value0_7[int(9)];
    Tuple_3 _S122 = s_apply_max_0((&_S67->value0_8)->value0_7[int(26)], 1.0f);
    s_bwdCallableCtx_max_0 _S123;
    (&_S123)->_S30 = (&_S67->value0_8)->value0_7[int(26)];
    (&_S123)->_S31 = 1.0f;
    s_bwdCallableCtx_max_0 _S124 = _S123;
    float _S125 = _S122.value0_3 * _S122.value0_3;
    float _S126 = (&_S67->value0_8)->value0_7[int(10)];
    Tuple_3 _S127 = s_apply_max_0((&_S67->value0_8)->value0_7[int(27)], 1.0f);
    s_bwdCallableCtx_max_0 _S128;
    (&_S128)->_S30 = (&_S67->value0_8)->value0_7[int(27)];
    (&_S128)->_S31 = 1.0f;
    s_bwdCallableCtx_max_0 _S129 = _S128;
    float _S130 = _S127.value0_3 * _S127.value0_3;
    float _S131 = (&_S67->value0_8)->value0_7[int(11)];
    Tuple_3 _S132 = s_apply_max_0((&_S67->value0_8)->value0_7[int(25)], 1.0f);
    s_bwdCallableCtx_max_0 _S133;
    (&_S133)->_S30 = (&_S67->value0_8)->value0_7[int(25)];
    (&_S133)->_S31 = 1.0f;
    s_bwdCallableCtx_max_0 _S134 = _S133;
    float _S135 = _S132.value0_3 * _S132.value0_3;
    float _S136 = (&_S67->value0_8)->value0_7[int(12)];
    float _S137 = (&_S67->value0_8)->value0_7[int(13)];
    float _S138 = (&_S67->value0_8)->value0_7[int(14)];
    float _S139 = (&_S67->value0_8)->value0_7[int(15)];
    float _S140 = (&_S67->value0_8)->value0_7[int(16)];
    Tuple_3 _S141 = s_apply_max_0((&_S67->value0_8)->value0_7[int(28)], 1.0f);
    s_bwdCallableCtx_max_0 _S142;
    (&_S142)->_S30 = (&_S67->value0_8)->value0_7[int(28)];
    (&_S142)->_S31 = 1.0f;
    s_bwdCallableCtx_max_0 _S143 = _S142;
    float _S144 = _S141.value0_3 * _S141.value0_3;
    float _S145 = (&_S67->value0_8)->value0_7[int(17)];
    Tuple_3 _S146 = s_apply_max_0((&_S67->value0_8)->value0_7[int(29)], 1.0f);
    s_bwdCallableCtx_max_0 _S147;
    (&_S147)->_S30 = (&_S67->value0_8)->value0_7[int(29)];
    (&_S147)->_S31 = 1.0f;
    s_bwdCallableCtx_max_0 _S148 = _S147;
    float _S149 = _S146.value0_3 * _S146.value0_3;
    float _S150 = (&_S67->value0_8)->value0_7[int(18)];
    Tuple_3 _S151 = s_apply_max_0((&_S67->value0_8)->value0_7[int(30)], 1.0f);
    s_bwdCallableCtx_max_0 _S152;
    (&_S152)->_S30 = (&_S67->value0_8)->value0_7[int(30)];
    (&_S152)->_S31 = 1.0f;
    s_bwdCallableCtx_max_0 _S153 = _S152;
    float _S154 = _S151.value0_3 * _S151.value0_3;
    float _S155 = (&_S67->value0_8)->value0_7[int(19)];
    Tuple_3 _S156 = s_apply_max_0((&_S67->value0_8)->value0_7[int(31)], 1.0f);
    s_bwdCallableCtx_max_0 _S157;
    (&_S157)->_S30 = (&_S67->value0_8)->value0_7[int(31)];
    (&_S157)->_S31 = 1.0f;
    s_bwdCallableCtx_max_0 _S158 = _S157;
    float _S159 = _S156.value0_3 * _S156.value0_3;
    FixedArray<float, 32>  _S160;
    dzero_0(&_S160);
    FixedArray<float, 32>  _S161 = _S160;
    float _S162 = (*_S69)[int(0)];
    float _S163 = (*_S69)[int(1)];
    float _S164 = (*_S69)[int(2)];
    float _S165 = (*_S69)[int(13)] / _S159;
    float _S166 = _S155 * - _S165;
    float _S167 = _S156.value0_3 * _S165;
    float _S168 = 0.0f;
    float _S169 = 0.0f;
    s_bwdCallableCtx_max_0 _S170 = _S158;
    s_bwdProp_max_0(&_S170, &_S168, &_S169, _S166);
    float _S171 = (*_S69)[int(12)] / _S154;
    float _S172 = _S150 * - _S171;
    float _S173 = _S151.value0_3 * _S171;
    float _S174 = 0.0f;
    float _S175 = 0.0f;
    s_bwdCallableCtx_max_0 _S176 = _S153;
    s_bwdProp_max_0(&_S176, &_S174, &_S175, _S172);
    float _S177 = (*_S69)[int(11)] / _S149;
    float _S178 = _S145 * - _S177;
    float _S179 = _S146.value0_3 * _S177;
    float _S180 = 0.0f;
    float _S181 = 0.0f;
    s_bwdCallableCtx_max_0 _S182 = _S148;
    s_bwdProp_max_0(&_S182, &_S180, &_S181, _S178);
    float _S183 = (*_S69)[int(10)] / _S144;
    float _S184 = _S140 * - _S183;
    float _S185 = _S141.value0_3 * _S183;
    float _S186 = 0.0f;
    float _S187 = 0.0f;
    s_bwdCallableCtx_max_0 _S188 = _S143;
    s_bwdProp_max_0(&_S188, &_S186, &_S187, _S184);
    float _S189 = (*_S69)[int(9)] / _S74;
    float _S190 = _S139 * - _S189;
    float _S191 = _S71.value0_3 * _S189;
    float _S192 = (*_S69)[int(8)] / _S74;
    float _S193 = _S138 * - _S192;
    float _S194 = _S71.value0_3 * _S192;
    float _S195 = (*_S69)[int(7)] / _S74;
    float _S196 = _S137 * - _S195;
    float _S197 = _S71.value0_3 * _S195;
    float _S198 = (*_S69)[int(6)] / _S74;
    float _S199 = _S136 * - _S198;
    float _S200 = _S71.value0_3 * _S198;
    float _S201 = (*_S69)[int(5)] / _S135;
    float _S202 = _S131 * - _S201;
    float _S203 = _S132.value0_3 * _S201;
    float _S204 = 0.0f;
    float _S205 = 0.0f;
    s_bwdCallableCtx_max_0 _S206 = _S134;
    s_bwdProp_max_0(&_S206, &_S204, &_S205, _S202);
    float _S207 = (*_S69)[int(4)] / _S130;
    float _S208 = _S126 * - _S207;
    float _S209 = _S127.value0_3 * _S207;
    float _S210 = 0.0f;
    float _S211 = 0.0f;
    s_bwdCallableCtx_max_0 _S212 = _S129;
    s_bwdProp_max_0(&_S212, &_S210, &_S211, _S208);
    float _S213 = (*_S69)[int(4)] / _S125;
    float _S214 = _S121 * - _S213;
    float _S215 = _S122.value0_3 * _S213;
    float _S216 = 0.0f;
    float _S217 = 0.0f;
    s_bwdCallableCtx_max_0 _S218 = _S124;
    s_bwdProp_max_0(&_S218, &_S216, &_S217, _S214);
    float _S219 = (*_S69)[int(3)] / _S120;
    float _S220 = _S219 / _S119;
    float _S221 = _S115 * - _S220;
    float _S222 = _S116.value0_3 * _S220;
    float _S223 = 0.0f;
    float _S224 = 0.0f;
    s_bwdCallableCtx_max_0 _S225 = _S118;
    s_bwdProp_max_0(&_S225, &_S223, &_S224, _S221);
    float _S226 = _S219 / _S114;
    float _S227 = _S110 * - _S226;
    float _S228 = _S111.value0_3 * _S226;
    float _S229 = 0.0f;
    float _S230 = 0.0f;
    s_bwdCallableCtx_max_0 _S231 = _S113;
    s_bwdProp_max_0(&_S231, &_S229, &_S230, _S227);
    float _S232 = _S190 + _S193 + _S196 + _S199;
    FixedArray<float, 32>  _S233 = _S160;
    _S233[int(31)] = _S168;
    _S233[int(19)] = _S167;
    _S233[int(30)] = _S174;
    _S233[int(18)] = _S173;
    _S233[int(29)] = _S180;
    _S233[int(17)] = _S179;
    _S233[int(28)] = _S186;
    _S233[int(16)] = _S185;
    _S233[int(15)] = _S191;
    _S233[int(14)] = _S194;
    _S233[int(13)] = _S197;
    _S233[int(12)] = _S200;
    _S233[int(25)] = _S204;
    _S233[int(11)] = _S203;
    _S233[int(27)] = _S210;
    _S233[int(10)] = _S209;
    _S233[int(26)] = _S216;
    _S233[int(9)] = _S215;
    _S233[int(24)] = _S223;
    _S233[int(8)] = _S222;
    _S233[int(23)] = _S229;
    _S233[int(7)] = _S228;
    FixedArray<float, 32>  _S234 = _S160;
    FixedArray<float, 32>  _S235 = _S233;
    FixedArray<float, 32>  _S236;
    dadd_0(&_S234, &_S235, &_S236);
    FixedArray<float, 32>  _S237 = _S236;
    FixedArray<float, 32>  _S238;
    if(_S80)
    {
        float _S239 = _S81 * _S164;
        float _S240 = 0.0f;
        float _S241 = 0.0f;
        float _S242 = 0.0f;
        s_bwdCallableCtx_clamp_0 _S243 = _S93;
        s_bwdProp_clamp_0(&_S243, &_S240, &_S241, &_S242, _S239);
        float _S244 = - _S240 / _S82;
        float _S245 = _S83 * - _S244;
        float _S246 = _S84 * _S244;
        float _S247 = 0.0f;
        s_bwdCallableCtx_sqrt_0 _S248 = _S94;
        s_bwdProp_sqrt_0(&_S248, &_S247, _S245);
        float _S249 = 0.0f;
        float _S250 = 0.0f;
        s_bwdCallableCtx_max_0 _S251 = _S95;
        s_bwdProp_max_0(&_S251, &_S249, &_S250, _S247);
        float _S252 = _S85 * _S250;
        float _S253 = _S86 * _S250;
        float _S254 = - _S252 / _S87;
        float _S255 = _S89 * (_S78 * _S254);
        float _S256 = - _S253 / _S87;
        float _S257 = _S91 * (_S78 * _S256);
        float _S258 = - _S246 / _S87;
        float _S259 = _S78 * _S258;
        float _S260 = _S255 + _S255 + _S91 * _S259;
        float _S261 = _S257 + _S257 + _S89 * _S259;
        float _S262 = _S88 * - _S254 + _S90 * - _S256 + _S92 * - _S258;
        FixedArray<float, 32>  _S263 = _S161;
        _S263[int(5)] = _S252;
        _S263[int(4)] = _S253;
        _S263[int(3)] = _S260;
        _S263[int(2)] = _S261;
        _S263[int(6)] = _S246;
        FixedArray<float, 32>  _S264 = _S237;
        FixedArray<float, 32>  _S265 = _S263;
        FixedArray<float, 32>  _S266;
        dadd_0(&_S264, &_S265, &_S266);
        _S238 = _S266;
        _S81 = _S262;
    }
    else
    {
        _S238 = _S237;
        _S81 = 0.0f;
    }
    if(_S79)
    {
        FixedArray<float, 32>  _S267 = _S161;
        _S267[int(3)] = 0.0f;
        FixedArray<float, 32>  _S268 = _S238;
        FixedArray<float, 32>  _S269 = _S267;
        FixedArray<float, 32>  _S270;
        dadd_0(&_S268, &_S269, &_S270);
        _S238 = _S270;
    }
    else
    {
    }
    float _S271 = -10.0f * _S163;
    float _S272 = 0.0f;
    s_bwdCallableCtx_log10_0 _S273 = _S77;
    s_bwdProp_log10_0(&_S273, &_S272, _S271);
    float _S274 = _S272 / _S74;
    float _S275 = _S71.value0_3 * _S274;
    float _S276 = _S162 / _S74;
    float _S277 = _S71.value0_3 * _S276;
    float _S278 = _S75 * - _S274 + _S70 * - _S276 + _S232;
    float _S279 = 0.0f;
    float _S280 = 0.0f;
    s_bwdCallableCtx_max_0 _S281 = _S73;
    s_bwdProp_max_0(&_S281, &_S279, &_S280, _S278);
    FixedArray<float, 32>  _S282 = _S161;
    _S282[int(22)] = _S81;
    _S282[int(1)] = _S275;
    _S282[int(21)] = _S279;
    _S282[int(0)] = _S277;
    FixedArray<float, 32>  _S283 = _S238;
    FixedArray<float, 32>  _S284 = _S282;
    FixedArray<float, 32>  _S285;
    dadd_0(&_S283, &_S284, &_S285);
    *_S68 = _S285;
    return;
}

inline __device__ void _S286(DiffPair_arrayx3Cfloatx2C32x3E_0 * _S287, FixedArray<float, 19>  * _S288, FixedArray<float, 14>  * _S289)
{
    s_paramCtx_s_bwdCallableCtx_per_pixel_losses_reduce_0 _S290;
    (&_S290)->value0_7 = _S287->primal_0;
    (&_S290)->value1_0 = *_S288;
    s_bwdCallableCtx_per_pixel_losses_reduce_0 _S291;
    (&_S291)->value0_8 = _S290;
    s_bwdProp_per_pixel_losses_reduce_0(&_S291, &_S287->differential_0, _S289);
    return;
}

struct s_paramCtx_s_bwdCallableCtx_per_pixel_losses_0
{
    float3  value0_9;
    float3  value1_1;
    float value2_0;
    float value3_0;
    float3  value4_0;
    float3  value5_0;
    float3  value6_0;
    float value7_0;
    float value11_0;
    float3  value12_0;
    uint value13_0;
    bool value14_0;
    float value15_0;
    FixedArray<float, 19>  value16_0;
};

struct s_bwdCallableCtx_per_pixel_losses_0
{
    s_paramCtx_s_bwdCallableCtx_per_pixel_losses_0 value0_10;
};

struct Tuple_7
{
    float3  value0_11;
};

inline __device__ Tuple_7 s_apply_rgb_to_yuv_0(float3  _S292)
{
    float _S293 = _S292.x;
    float _S294 = _S292.y;
    float _S295 = _S292.z;
    Tuple_7 _S296;
    (&_S296)->value0_11 = make_float3 (0.29899999499320984f * _S293 + 0.58700001239776611f * _S294 + 0.11400000005960464f * _S295, -0.14712999761104584f * _S293 - 0.28885999321937561f * _S294 + 0.43599998950958252f * _S295, 0.61500000953674316f * _S293 - 0.51498997211456299f * _S294 - 0.10001000016927719f * _S295);
    return _S296;
}

struct s_paramCtx_s_bwdCallableCtx_l1_loss_0
{
    float3  value0_12;
    float3  value1_2;
};

struct Tuple_8
{
    float value0_13;
};

inline __device__ Tuple_8 s_apply_l2_loss_0(float3  _S297, float3  _S298)
{
    float3  _S299 = _S298 - _S297;
    Tuple_8 _S300;
    (&_S300)->value0_13 = s_apply_dot_0(_S299, _S299).value0_5 * 0.3333333432674408f;
    return _S300;
}

struct s_paramCtx_s_bwdCallableCtx_l2_loss_0
{
    float3  value0_14;
    float3  value1_3;
};

struct Tuple_9
{
    float3  value0_15;
};

inline __device__ Tuple_9 s_apply_normalize_normal_0(float3  _S301, bool * mask_0)
{
    bool _S302 = *mask_0;
    Tuple_5 _S303 = s_apply_dot_0(_S301, _S301);
    bool _S304 = (_S303.value0_5) == 0.0f;
    float3  _S305;
    bool _S306;
    if(_S304)
    {
        _S305 = make_float3 (0.0f);
        _S306 = false;
    }
    else
    {
        _S306 = _S302;
    }
    bool _S307 = !_S304;
    if(_S307)
    {
        _S305 = _S301 * make_float3 (s_apply_rsqrt_0(_S303.value0_5).value0_0);
    }
    else
    {
    }
    *mask_0 = _S306;
    Tuple_9 _S308;
    (&_S308)->value0_15 = _S305;
    return _S308;
}

struct s_paramCtx_s_bwdCallableCtx_normalize_normal_0
{
    float3  value0_16;
};

struct s_paramCtx_s_bwdCallableCtx_normal_loss_0
{
    float3  value0_17;
    float3  value1_4;
};

struct s_paramCtx_s_bwdCallableCtx_alpha_loss_0
{
    float value0_18;
    float value1_5;
};

inline __device__ void s_bwdProp_mean3_0(float3  * _S309, float _S310)
{
    float _S311 = 0.3333333432674408f * _S310;
    *_S309 = make_float3 (_S311, _S311, _S311);
    return;
}

struct s_bwdCallableCtx_alpha_loss_0
{
    s_paramCtx_s_bwdCallableCtx_alpha_loss_0 value0_19;
};

struct s_paramCtx_s_bwdCallableCtx_bce_loss_0
{
    float value0_20;
    float value1_6;
};

struct s_bwdCallableCtx_bce_loss_0
{
    s_paramCtx_s_bwdCallableCtx_bce_loss_0 value0_21;
};

inline __device__ void s_bwdProp_bce_loss_0(s_bwdCallableCtx_bce_loss_0 * _S312, float * _S313, float * _S314, float _S315)
{
    float _S316 = 1.0f - (&_S312->value0_21)->value0_20;
    Tuple_3 _S317 = s_apply_max_0(_S316, 9.99999997475242708e-07f);
    s_bwdCallableCtx_max_0 _S318;
    (&_S318)->_S30 = _S316;
    (&_S318)->_S31 = 9.99999997475242708e-07f;
    Tuple_2 _S319 = s_apply_log_0(_S317.value0_3);
    s_bwdCallableCtx_log_0 _S320;
    (&_S320)->_S26 = _S317.value0_3;
    Tuple_3 _S321 = s_apply_max_0((&_S312->value0_21)->value0_20, 9.99999997475242708e-07f);
    s_bwdCallableCtx_max_0 _S322;
    (&_S322)->_S30 = (&_S312->value0_21)->value0_20;
    (&_S322)->_S31 = 9.99999997475242708e-07f;
    Tuple_2 _S323 = s_apply_log_0(_S321.value0_3);
    s_bwdCallableCtx_log_0 _S324;
    (&_S324)->_S26 = _S321.value0_3;
    s_bwdCallableCtx_lerp_0 _S325;
    (&_S325)->_S1 = _S319.value0_2;
    (&_S325)->_S2 = _S323.value0_2;
    (&_S325)->_S3 = (&_S312->value0_21)->value1_6;
    float _S326 = - _S315;
    float _S327 = 0.0f;
    float _S328 = 0.0f;
    float _S329 = 0.0f;
    s_bwdCallableCtx_lerp_0 _S330 = _S325;
    s_bwdProp_lerp_0(&_S330, &_S327, &_S328, &_S329, _S326);
    float _S331 = 0.0f;
    s_bwdCallableCtx_log_0 _S332 = _S324;
    s_bwdProp_log_0(&_S332, &_S331, _S328);
    float _S333 = 0.0f;
    float _S334 = 0.0f;
    s_bwdCallableCtx_max_0 _S335 = _S322;
    s_bwdProp_max_0(&_S335, &_S333, &_S334, _S331);
    float _S336 = 0.0f;
    s_bwdCallableCtx_log_0 _S337 = _S320;
    s_bwdProp_log_0(&_S337, &_S336, _S327);
    float _S338 = 0.0f;
    float _S339 = 0.0f;
    s_bwdCallableCtx_max_0 _S340 = _S318;
    s_bwdProp_max_0(&_S340, &_S338, &_S339, _S336);
    *_S313 = _S333 + - _S338;
    *_S314 = _S329;
    return;
}

inline __device__ void s_bwdProp_alpha_loss_0(s_bwdCallableCtx_alpha_loss_0 * _S341, float * _S342, float * _S343, float _S344)
{
    Tuple_3 _S345 = s_apply_max_0((&_S341->value0_19)->value0_18, (&_S341->value0_19)->value1_5);
    s_bwdCallableCtx_max_0 _S346;
    (&_S346)->_S30 = (&_S341->value0_19)->value0_18;
    (&_S346)->_S31 = (&_S341->value0_19)->value1_5;
    s_paramCtx_s_bwdCallableCtx_bce_loss_0 _S347;
    (&_S347)->value0_20 = _S345.value0_3;
    (&_S347)->value1_6 = (&_S341->value0_19)->value1_5;
    float _S348 = 0.0f;
    float _S349 = 0.0f;
    s_bwdCallableCtx_bce_loss_0 _S350;
    (&_S350)->value0_21 = _S347;
    s_bwdProp_bce_loss_0(&_S350, &_S348, &_S349, _S344);
    float _S351 = 0.0f;
    float _S352 = 0.0f;
    s_bwdCallableCtx_max_0 _S353 = _S346;
    s_bwdProp_max_0(&_S353, &_S351, &_S352, _S348);
    float _S354 = _S349 + _S352;
    *_S342 = _S351;
    *_S343 = _S354;
    return;
}

struct s_bwdCallableCtx_normal_loss_0
{
    s_paramCtx_s_bwdCallableCtx_normal_loss_0 value0_22;
};

inline __device__ void s_bwdProp_normal_loss_0(s_bwdCallableCtx_normal_loss_0 * _S355, float3  * _S356, float3  * _S357, float _S358)
{
    Tuple_5 _S359 = s_apply_dot_0((&_S355->value0_22)->value0_17, (&_S355->value0_22)->value1_4);
    s_bwdCallableCtx_dot_0 _S360;
    (&_S360)->_S53 = (&_S355->value0_22)->value0_17;
    (&_S360)->_S54 = (&_S355->value0_22)->value1_4;
    float cos_sim_loss_0 = 0.5f - 0.5f * _S359.value0_5;
    Tuple_3 _S361 = s_apply_max_0(cos_sim_loss_0, 9.999999960041972e-13f);
    s_bwdCallableCtx_max_0 _S362;
    (&_S362)->_S30 = cos_sim_loss_0;
    (&_S362)->_S31 = 9.999999960041972e-13f;
    s_bwdCallableCtx_sqrt_0 _S363;
    (&_S363)->_S19 = _S361.value0_3;
    float _S364 = 0.0f;
    s_bwdCallableCtx_sqrt_0 _S365 = _S363;
    s_bwdProp_sqrt_0(&_S365, &_S364, _S358);
    float _S366 = 0.0f;
    float _S367 = 0.0f;
    s_bwdCallableCtx_max_0 _S368 = _S362;
    s_bwdProp_max_0(&_S368, &_S366, &_S367, _S364);
    float _S369 = 0.5f * - (_S358 + _S366);
    float3  _S370 = make_float3 (0.0f);
    float3  _S371 = _S370;
    float3  _S372 = _S370;
    s_bwdCallableCtx_dot_0 _S373 = _S360;
    s_bwdProp_dot_0(&_S373, &_S371, &_S372, _S369);
    *_S356 = _S371;
    *_S357 = _S372;
    return;
}

struct s_bwdCallableCtx_normalize_normal_0
{
    s_paramCtx_s_bwdCallableCtx_normalize_normal_0 value0_23;
};

inline __device__ void s_bwdProp_normalize_normal_0(s_bwdCallableCtx_normalize_normal_0 * _S374, float3  * _S375, float3  _S376)
{
    float3  _S377 = make_float3 (0.0f);
    float3  _S378 = (&_S374->value0_23)->value0_16;
    Tuple_5 _S379 = s_apply_dot_0((&_S374->value0_23)->value0_16, (&_S374->value0_23)->value0_16);
    s_bwdCallableCtx_dot_0 _S380;
    (&_S380)->_S53 = (&_S374->value0_23)->value0_16;
    (&_S380)->_S54 = (&_S374->value0_23)->value0_16;
    s_bwdCallableCtx_dot_0 _S381 = _S380;
    bool _S382 = !((_S379.value0_5) == 0.0f);
    float3  _S383;
    s_bwdCallableCtx_rsqrt_0 _S384;
    if(_S382)
    {
        float3  _S385 = make_float3 (s_apply_rsqrt_0(_S379.value0_5).value0_0);
        s_bwdCallableCtx_rsqrt_0 _S386;
        (&_S386)->_S12 = _S379.value0_5;
        _S383 = _S385;
        _S384 = _S386;
    }
    else
    {
        _S383 = _S377;
        (&_S384)->_S12 = 0.0f;
    }
    float _S387;
    if(_S382)
    {
        float3  _S388 = _S378 * _S376;
        float3  _S389 = _S383 * _S376;
        float _S390 = _S388.x + _S388.y + _S388.z;
        float _S391 = 0.0f;
        s_bwdCallableCtx_rsqrt_0 _S392 = _S384;
        s_bwdProp_rsqrt_0(&_S392, &_S391, _S390);
        _S387 = _S391;
        _S383 = _S389;
    }
    else
    {
        _S387 = 0.0f;
        _S383 = _S377;
    }
    float3  _S393 = _S377;
    float3  _S394 = _S377;
    s_bwdCallableCtx_dot_0 _S395 = _S381;
    s_bwdProp_dot_0(&_S395, &_S393, &_S394, _S387);
    *_S375 = _S393 + _S394 + _S383;
    return;
}

struct s_bwdCallableCtx_l2_loss_0
{
    s_paramCtx_s_bwdCallableCtx_l2_loss_0 value0_24;
};

inline __device__ void s_bwdProp_l2_loss_0(s_bwdCallableCtx_l2_loss_0 * _S396, float3  * _S397, float3  * _S398, float _S399)
{
    float3  _S400 = (&_S396->value0_24)->value1_3 - (&_S396->value0_24)->value0_14;
    s_bwdCallableCtx_dot_0 _S401;
    (&_S401)->_S53 = _S400;
    (&_S401)->_S54 = _S400;
    float _S402 = 0.3333333432674408f * _S399;
    float3  _S403 = make_float3 (0.0f);
    float3  _S404 = _S403;
    float3  _S405 = _S403;
    s_bwdCallableCtx_dot_0 _S406 = _S401;
    s_bwdProp_dot_0(&_S406, &_S404, &_S405, _S402);
    float3  _S407 = _S404 + _S405;
    *_S397 = - _S407;
    *_S398 = _S407;
    return;
}

struct s_bwdCallableCtx_l1_loss_0
{
    s_paramCtx_s_bwdCallableCtx_l1_loss_0 value0_25;
};

inline __device__ void s_bwdProp_l1_loss_0(s_bwdCallableCtx_l1_loss_0 * _S408, float3  * _S409, float3  * _S410, float _S411)
{
    s_bwdCallableCtx_abs_0 _S412;
    (&_S412)->_S15 = (&_S408->value0_25)->value1_2 - (&_S408->value0_25)->value0_12;
    float3  _S413 = make_float3 (0.0f);
    float3  _S414 = _S413;
    s_bwdProp_mean3_0(&_S414, _S411);
    float3  _S415 = _S413;
    s_bwdCallableCtx_abs_0 _S416 = _S412;
    s_bwdProp_abs_0(&_S416, &_S415, _S414);
    *_S409 = - _S415;
    *_S410 = _S415;
    return;
}

inline __device__ void s_bwdProp_rgb_to_yuv_0(float3  * _S417, float3  _S418)
{
    float _S419 = - _S418.z;
    *_S417 = make_float3 (0.61500000953674316f * _S418.z + -0.14712999761104584f * _S418.y + 0.29899999499320984f * _S418.x, 0.51498997211456299f * _S419 + 0.28885999321937561f * - _S418.y + 0.58700001239776611f * _S418.x, 0.10001000016927719f * _S419 + 0.43599998950958252f * _S418.y + 0.11400000005960464f * _S418.x);
    return;
}

inline __device__ void s_bwdProp_per_pixel_losses_0(s_bwdCallableCtx_per_pixel_losses_0 * _S420, float3  * _S421, float3  * _S422, float * _S423, float * _S424, float3  * _S425, float3  * _S426, float3  * _S427, float * _S428, float3  * _S429, float * _S430, float3  * _S431, float * _S432, float3  * _S433, FixedArray<float, 32>  * _S434)
{
    float3  _S435 = make_float3 (0.0f);
    uint _S436 = (&_S420->value0_10)->value13_0;
    bool ref_alpha_0 = ((&_S420->value0_10)->value13_0) == 1U;
    bool _S437 = (&_S420->value0_10)->value14_0;
    bool mask_1;
    if((&_S420->value0_10)->value14_0)
    {
        mask_1 = ref_alpha_0;
    }
    else
    {
        mask_1 = true;
    }
    float _S438 = (&_S420->value0_10)->value15_0;
    bool _S439 = ((&_S420->value0_10)->value15_0) > 0.0f;
    bool _S440;
    s_bwdCallableCtx_min_0 _S441;
    s_bwdCallableCtx_min_0 _S442;
    if(_S439)
    {
        float _S443 = (&_S420->value0_10)->value0_9.x;
        float _S444 = (&_S420->value0_10)->value0_9.y;
        Tuple_6 _S445 = s_apply_min_0(_S443, _S444);
        s_bwdCallableCtx_min_0 _S446;
        (&_S446)->_S58 = _S443;
        (&_S446)->_S59 = _S444;
        float _S447 = (&_S420->value0_10)->value0_9.z;
        Tuple_6 _S448 = s_apply_min_0(_S445.value0_6, _S447);
        s_bwdCallableCtx_min_0 _S449;
        (&_S449)->_S58 = _S445.value0_6;
        (&_S449)->_S59 = _S447;
        _S440 = (_S448.value0_6) > _S438;
        _S441 = _S449;
        _S442 = _S446;
    }
    else
    {
        _S440 = false;
        (&_S441)->_S58 = 0.0f;
        (&_S441)->_S59 = 0.0f;
        (&_S442)->_S58 = 0.0f;
        (&_S442)->_S59 = 0.0f;
    }
    bool normal_mask_0;
    s_bwdCallableCtx_min_0 _S450;
    s_bwdCallableCtx_min_0 _S451;
    if(_S440)
    {
        float _S452 = (&_S420->value0_10)->value1_1.x;
        float _S453 = (&_S420->value0_10)->value1_1.y;
        Tuple_6 _S454 = s_apply_min_0(_S452, _S453);
        s_bwdCallableCtx_min_0 _S455;
        (&_S455)->_S58 = _S452;
        (&_S455)->_S59 = _S453;
        float _S456 = (&_S420->value0_10)->value1_1.z;
        Tuple_6 _S457 = s_apply_min_0(_S454.value0_6, _S456);
        s_bwdCallableCtx_min_0 _S458;
        (&_S458)->_S58 = _S454.value0_6;
        (&_S458)->_S59 = _S456;
        normal_mask_0 = (_S457.value0_6) > _S438;
        _S450 = _S458;
        _S451 = _S455;
    }
    else
    {
        normal_mask_0 = false;
        (&_S450)->_S58 = 0.0f;
        (&_S450)->_S59 = 0.0f;
        (&_S451)->_S58 = 0.0f;
        (&_S451)->_S59 = 0.0f;
    }
    if(normal_mask_0)
    {
        mask_1 = false;
    }
    else
    {
    }
    float _S459 = (&_S420->value0_10)->value3_0;
    bool depth_mask_0 = ((&_S420->value0_10)->value3_0) != 0.0f;
    float3  _S460 = (&_S420->value0_10)->value6_0;
    bool _S461 = ((&_S420->value0_10)->value6_0.x + (&_S420->value0_10)->value6_0.y + (&_S420->value0_10)->value6_0.z) > -2.36599993705749512f;
    s_bwdCallableCtx_dot_0 _S462;
    if(_S461)
    {
        Tuple_5 _S463 = s_apply_dot_0(_S460, _S460);
        s_bwdCallableCtx_dot_0 _S464;
        (&_S464)->_S53 = _S460;
        (&_S464)->_S54 = _S460;
        normal_mask_0 = (_S463.value0_5) > 0.25f;
        _S462 = _S464;
    }
    else
    {
        normal_mask_0 = false;
        (&_S462)->_S53 = _S435;
        (&_S462)->_S54 = _S435;
    }
    bool alpha_mask_0;
    if(_S437)
    {
        alpha_mask_0 = _S436 != 2U;
    }
    else
    {
        alpha_mask_0 = false;
    }
    Tuple_7 _S465 = s_apply_rgb_to_yuv_0((&_S420->value0_10)->value0_9);
    Tuple_7 _S466 = s_apply_rgb_to_yuv_0((&_S420->value0_10)->value1_1);
    float dY_0 = _S465.value0_11.x - _S466.value0_11.x;
    float dU_0 = _S465.value0_11.y - _S466.value0_11.y;
    float dV_0 = _S465.value0_11.z - _S466.value0_11.z;
    float _S467 = float(mask_1);
    float _S468 = (&_S420->value0_10)->value16_0[int(0)];
    s_paramCtx_s_bwdCallableCtx_l1_loss_0 _S469;
    (&_S469)->value0_12 = (&_S420->value0_10)->value0_9;
    (&_S469)->value1_2 = (&_S420->value0_10)->value1_1;
    s_paramCtx_s_bwdCallableCtx_l1_loss_0 _S470 = _S469;
    float _S471 = (&_S420->value0_10)->value16_0[int(1)];
    Tuple_8 _S472 = s_apply_l2_loss_0((&_S420->value0_10)->value0_9, (&_S420->value0_10)->value1_1);
    s_paramCtx_s_bwdCallableCtx_l2_loss_0 _S473;
    (&_S473)->value0_14 = (&_S420->value0_10)->value0_9;
    (&_S473)->value1_3 = (&_S420->value0_10)->value1_1;
    s_paramCtx_s_bwdCallableCtx_l2_loss_0 _S474 = _S473;
    float _S475 = (&_S420->value0_10)->value16_0[int(2)];
    s_bwdCallableCtx_abs_1 _S476;
    (&_S476)->_S49 = dY_0;
    s_bwdCallableCtx_abs_1 _S477 = _S476;
    float _S478 = (&_S420->value0_10)->value16_0[int(3)];
    float _S479 = (&_S420->value0_10)->value16_0[int(3)] * dY_0;
    float _S480 = (&_S420->value0_10)->value16_0[int(4)];
    float _S481 = (&_S420->value0_10)->value16_0[int(4)] * dU_0;
    float _S482 = (&_S420->value0_10)->value16_0[int(5)];
    float _S483 = (&_S420->value0_10)->value16_0[int(5)] * dV_0;
    s_bwdCallableCtx_clamp_0 _S484;
    (&_S484)->_S38 = _S472.value0_13;
    (&_S484)->_S39 = 0.0f;
    (&_S484)->_S40 = 1.0f;
    s_bwdCallableCtx_clamp_0 _S485 = _S484;
    float _S486 = float(depth_mask_0 & mask_1);
    float _S487 = (&_S420->value0_10)->value2_0;
    Tuple_3 _S488 = s_apply_max_0((&_S420->value0_10)->value2_0, 0.00009999999747379f);
    s_bwdCallableCtx_max_0 _S489;
    (&_S489)->_S30 = (&_S420->value0_10)->value2_0;
    (&_S489)->_S31 = 0.00009999999747379f;
    s_bwdCallableCtx_max_0 _S490 = _S489;
    float _S491 = _S486 * _S488.value0_3;
    Tuple_3 _S492 = s_apply_max_0(_S459, 0.00009999999747379f);
    s_bwdCallableCtx_max_0 _S493;
    (&_S493)->_S30 = _S459;
    (&_S493)->_S31 = 0.00009999999747379f;
    s_bwdCallableCtx_max_0 _S494 = _S493;
    float _S495 = _S486 * _S492.value0_3;
    bool _S496 = normal_mask_0 & mask_1;
    bool _S497 = true;
    Tuple_9 _S498 = s_apply_normalize_normal_0((&_S420->value0_10)->value4_0, &_S497);
    s_paramCtx_s_bwdCallableCtx_normalize_normal_0 _S499;
    (&_S499)->value0_16 = (&_S420->value0_10)->value4_0;
    s_paramCtx_s_bwdCallableCtx_normalize_normal_0 _S500 = _S499;
    bool render_normal_mask_0 = _S497;
    bool _S501 = true;
    Tuple_9 _S502 = s_apply_normalize_normal_0((&_S420->value0_10)->value5_0, &_S501);
    s_paramCtx_s_bwdCallableCtx_normalize_normal_0 _S503;
    (&_S503)->value0_16 = (&_S420->value0_10)->value5_0;
    s_paramCtx_s_bwdCallableCtx_normalize_normal_0 _S504 = _S503;
    bool depth_normal_mask_0 = _S501;
    bool _S505 = _S496;
    Tuple_9 _S506 = s_apply_normalize_normal_0(_S460, &_S505);
    s_paramCtx_s_bwdCallableCtx_normalize_normal_0 _S507;
    (&_S507)->value0_16 = _S460;
    s_paramCtx_s_bwdCallableCtx_normalize_normal_0 _S508 = _S507;
    bool normal_mask_1 = _S505;
    float _S509 = (&_S420->value0_10)->value16_0[int(7)] * float(_S497 & _S505);
    s_paramCtx_s_bwdCallableCtx_normal_loss_0 _S510;
    (&_S510)->value0_17 = _S498.value0_15;
    (&_S510)->value1_4 = _S506.value0_15;
    s_paramCtx_s_bwdCallableCtx_normal_loss_0 _S511 = _S510;
    float _S512 = (&_S420->value0_10)->value16_0[int(7)] * float(_S501 & _S505);
    s_paramCtx_s_bwdCallableCtx_normal_loss_0 _S513;
    (&_S513)->value0_17 = _S502.value0_15;
    (&_S513)->value1_4 = _S506.value0_15;
    s_paramCtx_s_bwdCallableCtx_normal_loss_0 _S514 = _S513;
    float _S515 = (&_S420->value0_10)->value16_0[int(10)] * float((_S497 & _S501) & mask_1);
    s_paramCtx_s_bwdCallableCtx_normal_loss_0 _S516;
    (&_S516)->value0_17 = _S498.value0_15;
    (&_S516)->value1_4 = _S502.value0_15;
    s_paramCtx_s_bwdCallableCtx_normal_loss_0 _S517 = _S516;
    bool _S518 = true;
    Tuple_9 _S519 = s_apply_normalize_normal_0((&_S420->value0_10)->value12_0, &_S518);
    s_paramCtx_s_bwdCallableCtx_normalize_normal_0 _S520;
    (&_S520)->value0_16 = (&_S420->value0_10)->value12_0;
    s_paramCtx_s_bwdCallableCtx_normalize_normal_0 _S521 = _S520;
    bool median_normal_mask_0 = _S518;
    if(mask_1)
    {
        normal_mask_0 = _S487 > 1.00000001335143196e-10f;
    }
    else
    {
        normal_mask_0 = false;
    }
    bool mean_median_mask_0;
    if(normal_mask_0)
    {
        mean_median_mask_0 = ((&_S420->value0_10)->value11_0) > 1.00000001335143196e-10f;
    }
    else
    {
        mean_median_mask_0 = false;
    }
    float _S522 = (&_S420->value0_10)->value16_0[int(15)] * float(mean_median_mask_0);
    Tuple_3 _S523 = s_apply_max_0(_S487, 1.00000001335143196e-10f);
    s_bwdCallableCtx_max_0 _S524;
    (&_S524)->_S30 = _S487;
    (&_S524)->_S31 = 1.00000001335143196e-10f;
    s_bwdCallableCtx_max_0 _S525 = _S524;
    Tuple_2 _S526 = s_apply_log_0(_S523.value0_3);
    s_bwdCallableCtx_log_0 _S527;
    (&_S527)->_S26 = _S523.value0_3;
    s_bwdCallableCtx_log_0 _S528 = _S527;
    Tuple_3 _S529 = s_apply_max_0((&_S420->value0_10)->value11_0, 1.00000001335143196e-10f);
    s_bwdCallableCtx_max_0 _S530;
    (&_S530)->_S30 = (&_S420->value0_10)->value11_0;
    (&_S530)->_S31 = 1.00000001335143196e-10f;
    s_bwdCallableCtx_max_0 _S531 = _S530;
    Tuple_2 _S532 = s_apply_log_0(_S529.value0_3);
    s_bwdCallableCtx_log_0 _S533;
    (&_S533)->_S26 = _S529.value0_3;
    s_bwdCallableCtx_log_0 _S534 = _S533;
    s_bwdCallableCtx_abs_1 _S535;
    (&_S535)->_S49 = _S526.value0_2 - _S532.value0_2;
    s_bwdCallableCtx_abs_1 _S536 = _S535;
    float _S537 = (&_S420->value0_10)->value16_0[int(16)] * float((median_normal_mask_0 & depth_normal_mask_0) & mask_1);
    s_paramCtx_s_bwdCallableCtx_normal_loss_0 _S538;
    (&_S538)->value0_17 = _S519.value0_15;
    (&_S538)->value1_4 = _S502.value0_15;
    s_paramCtx_s_bwdCallableCtx_normal_loss_0 _S539 = _S538;
    float _S540 = (&_S420->value0_10)->value16_0[int(17)] * float(median_normal_mask_0 & normal_mask_1);
    s_paramCtx_s_bwdCallableCtx_normal_loss_0 _S541;
    (&_S541)->value0_17 = _S519.value0_15;
    (&_S541)->value1_4 = _S506.value0_15;
    s_paramCtx_s_bwdCallableCtx_normal_loss_0 _S542 = _S541;
    float _S543 = (&_S420->value0_10)->value16_0[int(18)] * float((median_normal_mask_0 & render_normal_mask_0) & mask_1);
    s_paramCtx_s_bwdCallableCtx_normal_loss_0 _S544;
    (&_S544)->value0_17 = _S519.value0_15;
    (&_S544)->value1_4 = _S498.value0_15;
    s_paramCtx_s_bwdCallableCtx_normal_loss_0 _S545 = _S544;
    float _S546 = 1.0f - (&_S420->value0_10)->value7_0;
    Tuple_4 _S547 = s_apply_clamp_0(_S546, 0.0f, 1.0f);
    s_bwdCallableCtx_clamp_0 _S548;
    (&_S548)->_S38 = _S546;
    (&_S548)->_S39 = 0.0f;
    (&_S548)->_S40 = 1.0f;
    s_bwdCallableCtx_clamp_0 _S549 = _S548;
    float _S550 = float(alpha_mask_0);
    float _S551 = (&_S420->value0_10)->value16_0[int(8)] * _S550;
    float _S552 = float(ref_alpha_0);
    s_paramCtx_s_bwdCallableCtx_alpha_loss_0 _S553;
    (&_S553)->value0_18 = _S547.value0_4;
    (&_S553)->value1_5 = _S552;
    s_paramCtx_s_bwdCallableCtx_alpha_loss_0 _S554 = _S553;
    float _S555 = (&_S420->value0_10)->value16_0[int(9)] * _S550;
    float _S556 = 1.0f - _S547.value0_4;
    float _S557 = 1.0f - _S552;
    s_paramCtx_s_bwdCallableCtx_alpha_loss_0 _S558;
    (&_S558)->value0_18 = _S556;
    (&_S558)->value1_5 = _S557;
    s_paramCtx_s_bwdCallableCtx_alpha_loss_0 _S559 = _S558;
    float _S560 = (&_S420->value0_10)->value16_0[int(11)] * _S467 * 4.0f;
    float _S561 = _S560 * _S547.value0_4;
    float _S562 = (&_S420->value0_10)->value16_0[int(12)] * _S467;
    float _S563 = (&_S420->value0_10)->value16_0[int(13)] * _S467;
    float _S564 = (&_S420->value0_10)->value16_0[int(14)] * _S467;
    float _S565 = _S564 * (*_S434)[int(15)];
    float3  _S566 = _S435;
    s_bwdProp_mean3_0(&_S566, _S565);
    float3  _S567 = _S566;
    float _S568 = _S563 * (*_S434)[int(14)];
    float _S569 = _S562 * (*_S434)[int(13)];
    float3  _S570 = _S435;
    s_bwdProp_mean3_0(&_S570, _S569);
    float3  _S571 = _S570;
    float _S572 = _S561 * (*_S434)[int(12)];
    float _S573 = _S560 * (_S556 * (*_S434)[int(12)]);
    float _S574 = _S555 * (*_S434)[int(10)];
    float _S575 = 0.0f;
    float _S576 = 0.0f;
    s_bwdCallableCtx_alpha_loss_0 _S577;
    (&_S577)->value0_19 = _S559;
    s_bwdProp_alpha_loss_0(&_S577, &_S575, &_S576, _S574);
    float _S578 = - (_S572 + _S575);
    float _S579 = _S551 * (*_S434)[int(9)];
    float _S580 = 0.0f;
    float _S581 = 0.0f;
    s_bwdCallableCtx_alpha_loss_0 _S582;
    (&_S582)->value0_19 = _S554;
    s_bwdProp_alpha_loss_0(&_S582, &_S580, &_S581, _S579);
    float _S583 = _S573 + _S578 + _S580;
    float _S584 = 0.0f;
    float _S585 = 0.0f;
    float _S586 = 0.0f;
    s_bwdCallableCtx_clamp_0 _S587 = _S549;
    s_bwdProp_clamp_0(&_S587, &_S584, &_S585, &_S586, _S583);
    float _S588 = - _S584;
    float _S589 = _S543 * (*_S434)[int(19)];
    float3  _S590 = _S435;
    float3  _S591 = _S435;
    s_bwdCallableCtx_normal_loss_0 _S592;
    (&_S592)->value0_22 = _S545;
    s_bwdProp_normal_loss_0(&_S592, &_S590, &_S591, _S589);
    float _S593 = _S540 * (*_S434)[int(18)];
    float3  _S594 = _S435;
    float3  _S595 = _S435;
    s_bwdCallableCtx_normal_loss_0 _S596;
    (&_S596)->value0_22 = _S542;
    s_bwdProp_normal_loss_0(&_S596, &_S594, &_S595, _S593);
    float _S597 = _S537 * (*_S434)[int(17)];
    float3  _S598 = _S435;
    float3  _S599 = _S435;
    s_bwdCallableCtx_normal_loss_0 _S600;
    (&_S600)->value0_22 = _S539;
    s_bwdProp_normal_loss_0(&_S600, &_S598, &_S599, _S597);
    float _S601 = _S522 * (*_S434)[int(16)];
    float _S602 = 0.0f;
    s_bwdCallableCtx_abs_1 _S603 = _S536;
    s_bwdProp_abs_1(&_S603, &_S602, _S601);
    float _S604 = - _S602;
    float _S605 = 0.0f;
    s_bwdCallableCtx_log_0 _S606 = _S534;
    s_bwdProp_log_0(&_S606, &_S605, _S604);
    float _S607 = 0.0f;
    float _S608 = 0.0f;
    s_bwdCallableCtx_max_0 _S609 = _S531;
    s_bwdProp_max_0(&_S609, &_S607, &_S608, _S605);
    float _S610 = _S607;
    float _S611 = 0.0f;
    s_bwdCallableCtx_log_0 _S612 = _S528;
    s_bwdProp_log_0(&_S612, &_S611, _S602);
    float _S613 = 0.0f;
    float _S614 = 0.0f;
    s_bwdCallableCtx_max_0 _S615 = _S525;
    s_bwdProp_max_0(&_S615, &_S613, &_S614, _S611);
    float3  _S616 = _S590 + _S594 + _S598;
    float3  _S617 = _S435;
    s_bwdCallableCtx_normalize_normal_0 _S618;
    (&_S618)->value0_23 = _S521;
    s_bwdProp_normalize_normal_0(&_S618, &_S617, _S616);
    float3  _S619 = _S617;
    float _S620 = _S515 * (*_S434)[int(11)];
    float3  _S621 = _S435;
    float3  _S622 = _S435;
    s_bwdCallableCtx_normal_loss_0 _S623;
    (&_S623)->value0_22 = _S517;
    s_bwdProp_normal_loss_0(&_S623, &_S621, &_S622, _S620);
    float _S624 = _S512 * (*_S434)[int(8)];
    float3  _S625 = _S435;
    float3  _S626 = _S435;
    s_bwdCallableCtx_normal_loss_0 _S627;
    (&_S627)->value0_22 = _S514;
    s_bwdProp_normal_loss_0(&_S627, &_S625, &_S626, _S624);
    float _S628 = _S509 * (*_S434)[int(7)];
    float3  _S629 = _S435;
    float3  _S630 = _S435;
    s_bwdCallableCtx_normal_loss_0 _S631;
    (&_S631)->value0_22 = _S511;
    s_bwdProp_normal_loss_0(&_S631, &_S629, &_S630, _S628);
    float3  _S632 = _S626 + _S630 + _S595;
    float3  _S633 = _S435;
    s_bwdCallableCtx_normalize_normal_0 _S634;
    (&_S634)->value0_23 = _S508;
    s_bwdProp_normalize_normal_0(&_S634, &_S633, _S632);
    float3  _S635 = _S633;
    float3  _S636 = _S622 + _S625 + _S599;
    float3  _S637 = _S435;
    s_bwdCallableCtx_normalize_normal_0 _S638;
    (&_S638)->value0_23 = _S504;
    s_bwdProp_normalize_normal_0(&_S638, &_S637, _S636);
    float3  _S639 = _S637;
    float3  _S640 = _S621 + _S629 + _S591;
    float3  _S641 = _S435;
    s_bwdCallableCtx_normalize_normal_0 _S642;
    (&_S642)->value0_23 = _S500;
    s_bwdProp_normalize_normal_0(&_S642, &_S641, _S640);
    float3  _S643 = _S641;
    float _S644 = _S495 * (*_S434)[int(6)];
    float _S645 = _S495 * (*_S434)[int(5)];
    float _S646 = _S491 * (*_S434)[int(4)];
    float _S647 = _S486 * (_S491 * (*_S434)[int(6)] + _S645 + _S645 + (*_S434)[int(3)]);
    float _S648 = 0.0f;
    float _S649 = 0.0f;
    s_bwdCallableCtx_max_0 _S650 = _S494;
    s_bwdProp_max_0(&_S650, &_S648, &_S649, _S647);
    float _S651 = _S648;
    float _S652 = _S486 * (_S644 + _S646 + _S646 + (*_S434)[int(2)]);
    float _S653 = 0.0f;
    float _S654 = 0.0f;
    s_bwdCallableCtx_max_0 _S655 = _S490;
    s_bwdProp_max_0(&_S655, &_S653, &_S654, _S652);
    float _S656 = _S467 * (*_S434)[int(1)];
    float _S657 = 0.0f;
    float _S658 = 0.0f;
    float _S659 = 0.0f;
    s_bwdCallableCtx_clamp_0 _S660 = _S485;
    s_bwdProp_clamp_0(&_S660, &_S657, &_S658, &_S659, _S656);
    float _S661 = _S467 * (*_S434)[int(0)];
    float _S662 = _S483 * _S661;
    float _S663 = _S482 * (dV_0 * _S661);
    float _S664 = _S481 * _S661;
    float _S665 = _S480 * (dU_0 * _S661);
    float _S666 = _S479 * _S661;
    float _S667 = _S478 * (dY_0 * _S661);
    float _S668 = _S475 * _S661;
    float _S669 = 0.0f;
    s_bwdCallableCtx_abs_1 _S670 = _S477;
    s_bwdProp_abs_1(&_S670, &_S669, _S668);
    float _S671 = _S657 + _S471 * _S661;
    float3  _S672 = _S435;
    float3  _S673 = _S435;
    s_bwdCallableCtx_l2_loss_0 _S674;
    (&_S674)->value0_24 = _S474;
    s_bwdProp_l2_loss_0(&_S674, &_S672, &_S673, _S671);
    float _S675 = _S468 * _S661;
    float3  _S676 = _S435;
    float3  _S677 = _S435;
    s_bwdCallableCtx_l1_loss_0 _S678;
    (&_S678)->value0_25 = _S470;
    s_bwdProp_l1_loss_0(&_S678, &_S676, &_S677, _S675);
    float _S679 = _S662 + _S663;
    float _S680 = _S664 + _S665;
    float _S681 = _S666 + _S667 + _S669;
    float3  _S682 = make_float3 (- _S681, - _S680, - _S679);
    float3  _S683 = _S435;
    s_bwdProp_rgb_to_yuv_0(&_S683, _S682);
    float3  _S684 = make_float3 (_S681, _S680, _S679);
    float3  _S685 = _S435;
    s_bwdProp_rgb_to_yuv_0(&_S685, _S684);
    float _S686 = _S653 + _S613;
    float3  _S687 = _S673 + _S677 + _S683;
    float3  _S688 = _S672 + _S676 + _S685;
    float3  _S689;
    if(_S461)
    {
        float3  _S690 = _S435;
        float3  _S691 = _S435;
        s_bwdCallableCtx_dot_0 _S692 = _S462;
        s_bwdProp_dot_0(&_S692, &_S690, &_S691, 0.0f);
        _S689 = _S690 + _S691 + _S635;
    }
    else
    {
        _S689 = _S635;
    }
    float3  _S693;
    if(_S440)
    {
        float _S694 = 0.0f;
        float _S695 = 0.0f;
        s_bwdCallableCtx_min_0 _S696 = _S450;
        s_bwdProp_min_0(&_S696, &_S694, &_S695, 0.0f);
        float _S697 = 0.0f;
        float _S698 = 0.0f;
        s_bwdCallableCtx_min_0 _S699 = _S451;
        s_bwdProp_min_0(&_S699, &_S697, &_S698, _S694);
        _S693 = _S687 + make_float3 (_S697, _S698, _S695);
    }
    else
    {
        _S693 = _S687;
    }
    float3  _S700;
    if(_S439)
    {
        float _S701 = 0.0f;
        float _S702 = 0.0f;
        s_bwdCallableCtx_min_0 _S703 = _S441;
        s_bwdProp_min_0(&_S703, &_S701, &_S702, 0.0f);
        float _S704 = 0.0f;
        float _S705 = 0.0f;
        s_bwdCallableCtx_min_0 _S706 = _S442;
        s_bwdProp_min_0(&_S706, &_S704, &_S705, _S701);
        _S700 = _S688 + make_float3 (_S704, _S705, _S702);
    }
    else
    {
        _S700 = _S688;
    }
    *_S421 = _S700;
    *_S422 = _S693;
    *_S423 = _S686;
    *_S424 = _S651;
    *_S425 = _S643;
    *_S426 = _S639;
    *_S427 = _S689;
    *_S428 = _S588;
    *_S429 = _S571;
    *_S430 = _S568;
    *_S431 = _S567;
    *_S432 = _S610;
    *_S433 = _S619;
    return;
}

inline __device__ void _S707(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S708, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S709, DiffPair_float_0 * _S710, DiffPair_float_0 * _S711, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S712, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S713, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S714, DiffPair_float_0 * _S715, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S716, DiffPair_float_0 * _S717, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S718, DiffPair_float_0 * _S719, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S720, uint _S721, bool _S722, float _S723, FixedArray<float, 19>  * _S724, FixedArray<float, 32>  * _S725)
{
    s_paramCtx_s_bwdCallableCtx_per_pixel_losses_0 _S726;
    (&_S726)->value0_9 = _S708->primal_0;
    (&_S726)->value1_1 = _S709->primal_0;
    (&_S726)->value2_0 = _S710->primal_0;
    (&_S726)->value3_0 = _S711->primal_0;
    (&_S726)->value4_0 = _S712->primal_0;
    (&_S726)->value5_0 = _S713->primal_0;
    (&_S726)->value6_0 = _S714->primal_0;
    (&_S726)->value7_0 = _S715->primal_0;
    (&_S726)->value11_0 = _S719->primal_0;
    (&_S726)->value12_0 = _S720->primal_0;
    (&_S726)->value13_0 = _S721;
    (&_S726)->value14_0 = _S722;
    (&_S726)->value15_0 = _S723;
    (&_S726)->value16_0 = *_S724;
    s_bwdCallableCtx_per_pixel_losses_0 _S727;
    (&_S727)->value0_10 = _S726;
    s_bwdProp_per_pixel_losses_0(&_S727, &_S708->differential_0, &_S709->differential_0, &_S710->differential_0, &_S711->differential_0, &_S712->differential_0, &_S713->differential_0, &_S714->differential_0, &_S715->differential_0, &_S716->differential_0, &_S717->differential_0, &_S718->differential_0, &_S719->differential_0, &_S720->differential_0, _S725);
    return;
}

inline __device__ float lerp_0(float x_9, float y_4, float s_0)
{
    return x_9 + (y_4 - x_9) * s_0;
}

inline __device__ float3  abs_0(float3  x_10)
{
    float3  result_3;
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
        *_slang_vector_get_element_ptr(&result_3, i_1) = (F32_abs((_slang_vector_get_element(x_10, i_1))));
        i_1 = i_1 + int(1);
    }
    return result_3;
}

inline __device__ void per_pixel_losses(float3  render_rgb_0, float3  ref_rgb_0, float render_depth_0, float ref_depth_0, float3  render_normal_0, float3  depth_normal_0, float3  ref_normal_0, float render_Ts_0, float3  rgb_dist_0, float depth_dist_0, float3  normal_dist_0, float median_depth_0, float3  median_normal_0, uint ref_mask_0, bool has_mask_0, float saturation_threshold_0, FixedArray<float, 19>  weights_0, FixedArray<float, 32>  * _S728)
{
    bool _S729;
    float3  _S730;
    bool _S731;
    float3  _S732;
    float3  _S733;
    bool _S734;
    float3  _S735;
    FixedArray<float, 32>  losses_0;
    bool ref_alpha_1 = ref_mask_0 == 1U;
    bool mask_2;
    if(has_mask_0)
    {
        mask_2 = ref_alpha_1;
    }
    else
    {
        mask_2 = true;
    }
    bool normal_mask_2;
    if(saturation_threshold_0 > 0.0f)
    {
        normal_mask_2 = (F32_min(((F32_min((render_rgb_0.x), (render_rgb_0.y)))), (render_rgb_0.z))) > saturation_threshold_0;
    }
    else
    {
        normal_mask_2 = false;
    }
    if(normal_mask_2)
    {
        normal_mask_2 = (F32_min(((F32_min((ref_rgb_0.x), (ref_rgb_0.y)))), (ref_rgb_0.z))) > saturation_threshold_0;
    }
    else
    {
        normal_mask_2 = false;
    }
    if(normal_mask_2)
    {
        mask_2 = false;
    }
    else
    {
    }
    bool depth_mask_1 = ref_depth_0 != 0.0f;
    if((ref_normal_0.x + ref_normal_0.y + ref_normal_0.z) > -2.36599993705749512f)
    {
        normal_mask_2 = (dot_0(ref_normal_0, ref_normal_0)) > 0.25f;
    }
    else
    {
        normal_mask_2 = false;
    }
    bool alpha_mask_1;
    if(has_mask_0)
    {
        alpha_mask_1 = ref_mask_0 != 2U;
    }
    else
    {
        alpha_mask_1 = false;
    }
    float _S736 = render_rgb_0.x;
    float _S737 = render_rgb_0.y;
    float _S738 = render_rgb_0.z;
    float _S739 = ref_rgb_0.x;
    float _S740 = ref_rgb_0.y;
    float _S741 = ref_rgb_0.z;
    float dY_1 = 0.29899999499320984f * _S736 + 0.58700001239776611f * _S737 + 0.11400000005960464f * _S738 - (0.29899999499320984f * _S739 + 0.58700001239776611f * _S740 + 0.11400000005960464f * _S741);
    float dU_1 = -0.14712999761104584f * _S736 - 0.28885999321937561f * _S737 + 0.43599998950958252f * _S738 - (-0.14712999761104584f * _S739 - 0.28885999321937561f * _S740 + 0.43599998950958252f * _S741);
    float dV_1 = 0.61500000953674316f * _S736 - 0.51498997211456299f * _S737 - 0.10001000016927719f * _S738 - (0.61500000953674316f * _S739 - 0.51498997211456299f * _S740 - 0.10001000016927719f * _S741);
    float _S742 = float(mask_2);
    float3  _S743 = ref_rgb_0 - render_rgb_0;
    float3  _S744 = abs_0(_S743);
    float _S745 = dot_0(_S743, _S743) * 0.3333333432674408f;
    losses_0[int(0)] = _S742 * (weights_0[int(0)] * ((_S744.x + _S744.y + _S744.z) * 0.3333333432674408f) + weights_0[int(1)] * _S745 + weights_0[int(2)] * (F32_abs((dY_1))) + weights_0[int(3)] * dY_1 * dY_1 + weights_0[int(4)] * dU_1 * dU_1 + weights_0[int(5)] * dV_1 * dV_1);
    losses_0[int(1)] = _S742 * clamp_0(_S745, 0.0f, 1.0f);
    float _S746 = float(depth_mask_1 & mask_2);
    float _S747 = _S746 * (F32_max((render_depth_0), (0.00009999999747379f)));
    float _S748 = _S746 * (F32_max((ref_depth_0), (0.00009999999747379f)));
    losses_0[int(2)] = _S747;
    losses_0[int(3)] = _S748;
    losses_0[int(4)] = _S747 * _S747;
    losses_0[int(5)] = _S748 * _S748;
    losses_0[int(6)] = _S747 * _S748;
    bool _S749 = normal_mask_2 & mask_2;
    for(;;)
    {
        float norm2_0 = dot_0(render_normal_0, render_normal_0);
        bool _S750 = norm2_0 == 0.0f;
        _S729 = _S750;
        if(_S750)
        {
            _S730 = make_float3 (0.0f);
            break;
        }
        _S730 = render_normal_0 * make_float3 ((F32_rsqrt((norm2_0))));
        break;
    }
    bool _S751 = !_S729;
    for(;;)
    {
        float norm2_1 = dot_0(depth_normal_0, depth_normal_0);
        bool _S752 = norm2_1 == 0.0f;
        _S731 = _S752;
        if(_S752)
        {
            _S732 = make_float3 (0.0f);
            break;
        }
        _S732 = depth_normal_0 * make_float3 ((F32_rsqrt((norm2_1))));
        break;
    }
    bool _S753 = !_S731;
    for(;;)
    {
        float norm2_2 = dot_0(ref_normal_0, ref_normal_0);
        if(norm2_2 == 0.0f)
        {
            _S733 = make_float3 (0.0f);
            normal_mask_2 = false;
            break;
        }
        _S733 = ref_normal_0 * make_float3 ((F32_rsqrt((norm2_2))));
        normal_mask_2 = _S749;
        break;
    }
    float _S754 = float(_S751 & normal_mask_2);
    float cos_sim_loss_1 = 0.5f - 0.5f * dot_0(_S730, _S733);
    losses_0[int(7)] = weights_0[int(7)] * _S754 * (cos_sim_loss_1 + (F32_sqrt(((F32_max((cos_sim_loss_1), (9.999999960041972e-13f)))))));
    float _S755 = float(_S753 & normal_mask_2);
    float cos_sim_loss_2 = 0.5f - 0.5f * dot_0(_S732, _S733);
    losses_0[int(8)] = weights_0[int(7)] * _S755 * (cos_sim_loss_2 + (F32_sqrt(((F32_max((cos_sim_loss_2), (9.999999960041972e-13f)))))));
    float _S756 = float((_S751 & _S753) & mask_2);
    float cos_sim_loss_3 = 0.5f - 0.5f * dot_0(_S730, _S732);
    losses_0[int(11)] = weights_0[int(10)] * _S756 * (cos_sim_loss_3 + (F32_sqrt(((F32_max((cos_sim_loss_3), (9.999999960041972e-13f)))))));
    for(;;)
    {
        float norm2_3 = dot_0(median_normal_0, median_normal_0);
        bool _S757 = norm2_3 == 0.0f;
        _S734 = _S757;
        if(_S757)
        {
            _S735 = make_float3 (0.0f);
            break;
        }
        _S735 = median_normal_0 * make_float3 ((F32_rsqrt((norm2_3))));
        break;
    }
    bool _S758 = !_S734;
    bool mean_median_mask_1;
    if(mask_2)
    {
        mean_median_mask_1 = render_depth_0 > 1.00000001335143196e-10f;
    }
    else
    {
        mean_median_mask_1 = false;
    }
    if(mean_median_mask_1)
    {
        mean_median_mask_1 = median_depth_0 > 1.00000001335143196e-10f;
    }
    else
    {
        mean_median_mask_1 = false;
    }
    float _S759 = float(mean_median_mask_1);
    losses_0[int(16)] = weights_0[int(15)] * _S759 * (F32_abs(((F32_log(((F32_max((render_depth_0), (1.00000001335143196e-10f)))))) - (F32_log(((F32_max((median_depth_0), (1.00000001335143196e-10f)))))))));
    float _S760 = float((_S758 & _S753) & mask_2);
    float cos_sim_loss_4 = 0.5f - 0.5f * dot_0(_S735, _S732);
    losses_0[int(17)] = weights_0[int(16)] * _S760 * (cos_sim_loss_4 + (F32_sqrt(((F32_max((cos_sim_loss_4), (9.999999960041972e-13f)))))));
    float _S761 = float(_S758 & normal_mask_2);
    float cos_sim_loss_5 = 0.5f - 0.5f * dot_0(_S735, _S733);
    losses_0[int(18)] = weights_0[int(17)] * _S761 * (cos_sim_loss_5 + (F32_sqrt(((F32_max((cos_sim_loss_5), (9.999999960041972e-13f)))))));
    float _S762 = float((_S758 & _S751) & mask_2);
    float cos_sim_loss_6 = 0.5f - 0.5f * dot_0(_S735, _S730);
    losses_0[int(19)] = weights_0[int(18)] * _S762 * (cos_sim_loss_6 + (F32_sqrt(((F32_max((cos_sim_loss_6), (9.999999960041972e-13f)))))));
    float render_alpha_0 = clamp_0(1.0f - render_Ts_0, 0.0f, 1.0f);
    float _S763 = float(alpha_mask_1);
    float _S764 = float(ref_alpha_1);
    float _S765 = (F32_max((render_alpha_0), (_S764)));
    losses_0[int(9)] = weights_0[int(8)] * _S763 * - lerp_0((F32_log(((F32_max((1.0f - _S765), (9.99999997475242708e-07f)))))), (F32_log(((F32_max((_S765), (9.99999997475242708e-07f)))))), _S764);
    float _S766 = 1.0f - render_alpha_0;
    float _S767 = 1.0f - _S764;
    float _S768 = (F32_max((_S766), (_S767)));
    losses_0[int(10)] = weights_0[int(9)] * _S763 * - lerp_0((F32_log(((F32_max((1.0f - _S768), (9.99999997475242708e-07f)))))), (F32_log(((F32_max((_S768), (9.99999997475242708e-07f)))))), _S767);
    losses_0[int(12)] = weights_0[int(11)] * _S742 * 4.0f * render_alpha_0 * _S766;
    losses_0[int(13)] = weights_0[int(12)] * _S742 * ((rgb_dist_0.x + rgb_dist_0.y + rgb_dist_0.z) * 0.3333333432674408f);
    losses_0[int(14)] = weights_0[int(13)] * _S742 * depth_dist_0;
    losses_0[int(15)] = weights_0[int(14)] * _S742 * ((normal_dist_0.x + normal_dist_0.y + normal_dist_0.z) * 0.3333333432674408f);
    losses_0[int(20)] = 1.0f;
    losses_0[int(21)] = _S742;
    losses_0[int(22)] = _S746;
    losses_0[int(23)] = _S754;
    losses_0[int(24)] = _S755;
    losses_0[int(25)] = _S756;
    if(alpha_mask_1)
    {
        mask_2 = !ref_alpha_1;
    }
    else
    {
        mask_2 = false;
    }
    losses_0[int(26)] = float(mask_2);
    if(alpha_mask_1)
    {
        mask_2 = ref_alpha_1;
    }
    else
    {
        mask_2 = false;
    }
    losses_0[int(27)] = float(mask_2);
    losses_0[int(28)] = _S759;
    losses_0[int(29)] = _S760;
    losses_0[int(30)] = _S761;
    losses_0[int(31)] = _S762;
    *_S728 = losses_0;
    return;
}

inline __device__ void per_pixel_losses_bwd(float3  render_rgb_1, float3  ref_rgb_1, float render_depth_1, float ref_depth_1, float3  render_normal_1, float3  depth_normal_1, float3  ref_normal_1, float render_Ts_1, float3  rgb_dist_1, float depth_dist_1, float3  normal_dist_1, float median_depth_1, float3  median_normal_1, uint ref_mask_1, bool has_mask_1, float saturation_threshold_1, FixedArray<float, 19>  weights_1, FixedArray<float, 32>  v_losses_0, float3  * v_render_rgb_0, float3  * v_ref_rgb_0, float * v_render_depth_0, float * v_ref_depth_0, float3  * v_render_normal_0, float3  * v_depth_normal_0, float3  * v_ref_normal_0, float * v_render_Ts_0, float3  * v_rgb_dist_0, float * v_depth_dist_0, float3  * v_normal_dist_0, float * v_median_depth_0, float3  * v_median_normal_0)
{
    float3  _S769 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_render_rgb_0;
    (&dp_render_rgb_0)->primal_0 = render_rgb_1;
    (&dp_render_rgb_0)->differential_0 = _S769;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ref_rgb_0;
    (&dp_ref_rgb_0)->primal_0 = ref_rgb_1;
    (&dp_ref_rgb_0)->differential_0 = _S769;
    DiffPair_float_0 dp_render_depth_0;
    (&dp_render_depth_0)->primal_0 = render_depth_1;
    (&dp_render_depth_0)->differential_0 = 0.0f;
    DiffPair_float_0 dp_ref_depth_0;
    (&dp_ref_depth_0)->primal_0 = ref_depth_1;
    (&dp_ref_depth_0)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_render_normal_0;
    (&dp_render_normal_0)->primal_0 = render_normal_1;
    (&dp_render_normal_0)->differential_0 = _S769;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_depth_normal_0;
    (&dp_depth_normal_0)->primal_0 = depth_normal_1;
    (&dp_depth_normal_0)->differential_0 = _S769;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ref_normal_0;
    (&dp_ref_normal_0)->primal_0 = ref_normal_1;
    (&dp_ref_normal_0)->differential_0 = _S769;
    DiffPair_float_0 dp_render_Ts_0;
    (&dp_render_Ts_0)->primal_0 = render_Ts_1;
    (&dp_render_Ts_0)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_rgb_dist_0;
    (&dp_rgb_dist_0)->primal_0 = rgb_dist_1;
    (&dp_rgb_dist_0)->differential_0 = _S769;
    DiffPair_float_0 dp_depth_dist_0;
    (&dp_depth_dist_0)->primal_0 = depth_dist_1;
    (&dp_depth_dist_0)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_normal_dist_0;
    (&dp_normal_dist_0)->primal_0 = normal_dist_1;
    (&dp_normal_dist_0)->differential_0 = _S769;
    DiffPair_float_0 dp_median_depth_0;
    (&dp_median_depth_0)->primal_0 = median_depth_1;
    (&dp_median_depth_0)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_median_normal_0;
    (&dp_median_normal_0)->primal_0 = median_normal_1;
    (&dp_median_normal_0)->differential_0 = _S769;
    FixedArray<float, 19>  _S770 = weights_1;
    FixedArray<float, 32>  _S771 = v_losses_0;
    _S707(&dp_render_rgb_0, &dp_ref_rgb_0, &dp_render_depth_0, &dp_ref_depth_0, &dp_render_normal_0, &dp_depth_normal_0, &dp_ref_normal_0, &dp_render_Ts_0, &dp_rgb_dist_0, &dp_depth_dist_0, &dp_normal_dist_0, &dp_median_depth_0, &dp_median_normal_0, ref_mask_1, has_mask_1, saturation_threshold_1, &_S770, &_S771);
    *v_render_rgb_0 = dp_render_rgb_0.differential_0;
    *v_ref_rgb_0 = dp_ref_rgb_0.differential_0;
    *v_render_depth_0 = dp_render_depth_0.differential_0;
    *v_ref_depth_0 = dp_ref_depth_0.differential_0;
    *v_render_normal_0 = dp_render_normal_0.differential_0;
    *v_depth_normal_0 = dp_depth_normal_0.differential_0;
    *v_ref_normal_0 = dp_ref_normal_0.differential_0;
    *v_render_Ts_0 = dp_render_Ts_0.differential_0;
    *v_rgb_dist_0 = dp_rgb_dist_0.differential_0;
    *v_depth_dist_0 = dp_depth_dist_0.differential_0;
    *v_normal_dist_0 = dp_normal_dist_0.differential_0;
    *v_median_depth_0 = dp_median_depth_0.differential_0;
    *v_median_normal_0 = dp_median_normal_0.differential_0;
    return;
}

inline __device__ void per_pixel_losses_reduce(FixedArray<float, 32>  raw_losses_0, FixedArray<float, 19>  weights_2, FixedArray<float, 14>  * _S772)
{
    FixedArray<float, 14>  losses_1;
    float _S773 = (F32_max((raw_losses_0[int(21)]), (1.0f)));
    losses_1[int(0)] = raw_losses_0[int(0)] / _S773;
    losses_1[int(1)] = -10.0f * (F32_log10((raw_losses_0[int(1)] / _S773)));
    bool _S774;
    if((raw_losses_0[int(22)]) > 0.0f)
    {
        _S774 = (raw_losses_0[int(3)]) != 0.0f;
    }
    else
    {
        _S774 = false;
    }
    float _S775;
    if(_S774)
    {
        _S775 = weights_2[int(6)] * clamp_0(1.0f - (raw_losses_0[int(6)] - raw_losses_0[int(2)] * raw_losses_0[int(3)] / raw_losses_0[int(22)]) / (F32_sqrt(((F32_max((9.999999960041972e-13f), ((raw_losses_0[int(4)] - raw_losses_0[int(2)] * raw_losses_0[int(2)] / raw_losses_0[int(22)]) * (raw_losses_0[int(5)] - raw_losses_0[int(3)] * raw_losses_0[int(3)] / raw_losses_0[int(22)]) + 1.0f)))))), 0.0f, 2.0f);
    }
    else
    {
        _S775 = 0.0f;
    }
    losses_1[int(2)] = _S775;
    losses_1[int(3)] = (raw_losses_0[int(7)] / (F32_max((raw_losses_0[int(23)]), (1.0f))) + raw_losses_0[int(8)] / (F32_max((raw_losses_0[int(24)]), (1.0f)))) / float((I32_max((int((raw_losses_0[int(23)]) > 0.5f) + int((raw_losses_0[int(24)]) > 0.5f)), (int(1)))));
    losses_1[int(4)] = raw_losses_0[int(9)] / (F32_max((raw_losses_0[int(26)]), (1.0f))) + raw_losses_0[int(10)] / (F32_max((raw_losses_0[int(27)]), (1.0f)));
    losses_1[int(5)] = raw_losses_0[int(11)] / (F32_max((raw_losses_0[int(25)]), (1.0f)));
    losses_1[int(6)] = raw_losses_0[int(12)] / _S773;
    losses_1[int(7)] = raw_losses_0[int(13)] / _S773;
    losses_1[int(8)] = raw_losses_0[int(14)] / _S773;
    losses_1[int(9)] = raw_losses_0[int(15)] / _S773;
    losses_1[int(10)] = raw_losses_0[int(16)] / (F32_max((raw_losses_0[int(28)]), (1.0f)));
    losses_1[int(11)] = raw_losses_0[int(17)] / (F32_max((raw_losses_0[int(29)]), (1.0f)));
    losses_1[int(12)] = raw_losses_0[int(18)] / (F32_max((raw_losses_0[int(30)]), (1.0f)));
    losses_1[int(13)] = raw_losses_0[int(19)] / (F32_max((raw_losses_0[int(31)]), (1.0f)));
    *_S772 = losses_1;
    return;
}

inline __device__ void per_pixel_losses_reduce_bwd(FixedArray<float, 32>  raw_losses_1, FixedArray<float, 19>  weights_3, FixedArray<float, 14>  v_losses_1, FixedArray<float, 32>  * _S776)
{
    FixedArray<float, 32>  _S777;
    dzero_0(&_S777);
    DiffPair_arrayx3Cfloatx2C32x3E_0 dp_raw_losses_0;
    (&dp_raw_losses_0)->primal_0 = raw_losses_1;
    (&dp_raw_losses_0)->differential_0 = _S777;
    FixedArray<float, 19>  _S778 = weights_3;
    FixedArray<float, 14>  _S779 = v_losses_1;
    _S286(&dp_raw_losses_0, &_S778, &_S779);
    *_S776 = (&dp_raw_losses_0)->differential_0;
    return;
}

