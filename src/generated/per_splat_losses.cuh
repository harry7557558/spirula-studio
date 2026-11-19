#pragma once

#include "generated/slang.cuh"

struct DiffPair_float_0
{
    float primal_0;
    float differential_0;
};

inline __device__ void _d_max_0(DiffPair_float_0 * dpx_0, DiffPair_float_0 * dpy_0, float dOut_0)
{
    DiffPair_float_0 _S1 = *dpx_0;
    float _S2;
    if(((*dpx_0).primal_0) > ((*dpy_0).primal_0))
    {
        _S2 = dOut_0;
    }
    else
    {
        if(((*dpx_0).primal_0) < ((*dpy_0).primal_0))
        {
            _S2 = 0.0f;
        }
        else
        {
            _S2 = 0.5f * dOut_0;
        }
    }
    dpx_0->primal_0 = _S1.primal_0;
    dpx_0->differential_0 = _S2;
    DiffPair_float_0 _S3 = *dpy_0;
    if(((*dpy_0).primal_0) > (_S1.primal_0))
    {
        _S2 = dOut_0;
    }
    else
    {
        if(((*dpy_0).primal_0) < ((*dpx_0).primal_0))
        {
            _S2 = 0.0f;
        }
        else
        {
            _S2 = 0.5f * dOut_0;
        }
    }
    dpy_0->primal_0 = _S3.primal_0;
    dpy_0->differential_0 = _S2;
    return;
}

struct DiffPair_vectorx3Cfloatx2C3x3E_0
{
    float3  primal_0;
    float3  differential_0;
};

inline __device__ void _d_max_vector_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_1, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpy_1, float3  dOut_1)
{
    DiffPair_float_0 left_dp_0;
    (&left_dp_0)->primal_0 = (*dpx_1).primal_0.x;
    (&left_dp_0)->differential_0 = 0.0f;
    DiffPair_float_0 right_dp_0;
    (&right_dp_0)->primal_0 = (*dpy_1).primal_0.x;
    (&right_dp_0)->differential_0 = 0.0f;
    _d_max_0(&left_dp_0, &right_dp_0, dOut_1.x);
    float3  left_d_result_0;
    *&((&left_d_result_0)->x) = left_dp_0.differential_0;
    float3  right_d_result_0;
    *&((&right_d_result_0)->x) = right_dp_0.differential_0;
    DiffPair_float_0 left_dp_1;
    (&left_dp_1)->primal_0 = (*dpx_1).primal_0.y;
    (&left_dp_1)->differential_0 = 0.0f;
    DiffPair_float_0 right_dp_1;
    (&right_dp_1)->primal_0 = (*dpy_1).primal_0.y;
    (&right_dp_1)->differential_0 = 0.0f;
    _d_max_0(&left_dp_1, &right_dp_1, dOut_1.y);
    *&((&left_d_result_0)->y) = left_dp_1.differential_0;
    *&((&right_d_result_0)->y) = right_dp_1.differential_0;
    DiffPair_float_0 left_dp_2;
    (&left_dp_2)->primal_0 = (*dpx_1).primal_0.z;
    (&left_dp_2)->differential_0 = 0.0f;
    DiffPair_float_0 right_dp_2;
    (&right_dp_2)->primal_0 = (*dpy_1).primal_0.z;
    (&right_dp_2)->differential_0 = 0.0f;
    _d_max_0(&left_dp_2, &right_dp_2, dOut_1.z);
    *&((&left_d_result_0)->z) = left_dp_2.differential_0;
    *&((&right_d_result_0)->z) = right_dp_2.differential_0;
    dpx_1->primal_0 = (*dpx_1).primal_0;
    dpx_1->differential_0 = left_d_result_0;
    dpy_1->primal_0 = (*dpy_1).primal_0;
    dpy_1->differential_0 = right_d_result_0;
    return;
}

struct DiffPair_vectorx3Cfloatx2C2x3E_0
{
    float2  primal_0;
    float2  differential_0;
};

inline __device__ void _d_max_vector_1(DiffPair_vectorx3Cfloatx2C2x3E_0 * dpx_2, DiffPair_vectorx3Cfloatx2C2x3E_0 * dpy_2, float2  dOut_2)
{
    DiffPair_float_0 left_dp_3;
    (&left_dp_3)->primal_0 = (*dpx_2).primal_0.x;
    (&left_dp_3)->differential_0 = 0.0f;
    DiffPair_float_0 right_dp_3;
    (&right_dp_3)->primal_0 = (*dpy_2).primal_0.x;
    (&right_dp_3)->differential_0 = 0.0f;
    _d_max_0(&left_dp_3, &right_dp_3, dOut_2.x);
    float2  left_d_result_1;
    *&((&left_d_result_1)->x) = left_dp_3.differential_0;
    float2  right_d_result_1;
    *&((&right_d_result_1)->x) = right_dp_3.differential_0;
    DiffPair_float_0 left_dp_4;
    (&left_dp_4)->primal_0 = (*dpx_2).primal_0.y;
    (&left_dp_4)->differential_0 = 0.0f;
    DiffPair_float_0 right_dp_4;
    (&right_dp_4)->primal_0 = (*dpy_2).primal_0.y;
    (&right_dp_4)->differential_0 = 0.0f;
    _d_max_0(&left_dp_4, &right_dp_4, dOut_2.y);
    *&((&left_d_result_1)->y) = left_dp_4.differential_0;
    *&((&right_d_result_1)->y) = right_dp_4.differential_0;
    dpx_2->primal_0 = (*dpx_2).primal_0;
    dpx_2->differential_0 = left_d_result_1;
    dpy_2->primal_0 = (*dpy_2).primal_0;
    dpy_2->differential_0 = right_d_result_1;
    return;
}

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

inline __device__ float2  max_1(float2  x_1, float2  y_1)
{
    float2  result_1;
    int i_1 = int(0);
    for(;;)
    {
        if(i_1 < int(2))
        {
        }
        else
        {
            break;
        }
        *_slang_vector_get_element_ptr(&result_1, i_1) = (F32_max((_slang_vector_get_element(x_1, i_1)), (_slang_vector_get_element(y_1, i_1))));
        i_1 = i_1 + int(1);
    }
    return result_1;
}

inline __device__ void _d_exp_0(DiffPair_float_0 * dpx_3, float dOut_3)
{
    float _S4 = (F32_exp(((*dpx_3).primal_0))) * dOut_3;
    dpx_3->primal_0 = (*dpx_3).primal_0;
    dpx_3->differential_0 = _S4;
    return;
}

inline __device__ void _d_sqrt_0(DiffPair_float_0 * dpx_4, float dOut_4)
{
    float _S5 = 0.5f / (F32_sqrt(((F32_max((1.00000001168609742e-07f), ((*dpx_4).primal_0)))))) * dOut_4;
    dpx_4->primal_0 = (*dpx_4).primal_0;
    dpx_4->differential_0 = _S5;
    return;
}

inline __device__ float dot_0(float4  x_2, float4  y_2)
{
    int i_2 = int(0);
    float result_2 = 0.0f;
    for(;;)
    {
        if(i_2 < int(4))
        {
        }
        else
        {
            break;
        }
        float result_3 = result_2 + _slang_vector_get_element(x_2, i_2) * _slang_vector_get_element(y_2, i_2);
        i_2 = i_2 + int(1);
        result_2 = result_3;
    }
    return result_2;
}

inline __device__ float length_0(float4  x_3)
{
    return (F32_sqrt((dot_0(x_3, x_3))));
}

inline __device__ void _d_log_0(DiffPair_float_0 * dpx_5, float dOut_5)
{
    float _S6 = 1.0f / (*dpx_5).primal_0 * dOut_5;
    dpx_5->primal_0 = (*dpx_5).primal_0;
    dpx_5->differential_0 = _S6;
    return;
}

inline __device__ void _d_min_0(DiffPair_float_0 * dpx_6, DiffPair_float_0 * dpy_3, float dOut_6)
{
    DiffPair_float_0 _S7 = *dpx_6;
    float _S8;
    if(((*dpx_6).primal_0) < ((*dpy_3).primal_0))
    {
        _S8 = dOut_6;
    }
    else
    {
        if(((*dpx_6).primal_0) > ((*dpy_3).primal_0))
        {
            _S8 = 0.0f;
        }
        else
        {
            _S8 = 0.5f * dOut_6;
        }
    }
    dpx_6->primal_0 = _S7.primal_0;
    dpx_6->differential_0 = _S8;
    DiffPair_float_0 _S9 = *dpy_3;
    if(((*dpy_3).primal_0) < (_S7.primal_0))
    {
        _S8 = dOut_6;
    }
    else
    {
        if(((*dpy_3).primal_0) > ((*dpx_6).primal_0))
        {
            _S8 = 0.0f;
        }
        else
        {
            _S8 = 0.5f * dOut_6;
        }
    }
    dpy_3->primal_0 = _S9.primal_0;
    dpy_3->differential_0 = _S8;
    return;
}

inline __device__ float3  exp_0(float3  x_4)
{
    float3  result_4;
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
        *_slang_vector_get_element_ptr(&result_4, i_3) = (F32_exp((_slang_vector_get_element(x_4, i_3))));
        i_3 = i_3 + int(1);
    }
    return result_4;
}

inline __device__ void _d_exp_vector_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_7, float3  dOut_7)
{
    float3  _S10 = exp_0((*dpx_7).primal_0) * dOut_7;
    dpx_7->primal_0 = (*dpx_7).primal_0;
    dpx_7->differential_0 = _S10;
    return;
}

inline __device__ float2  exp_1(float2  x_5)
{
    float2  result_5;
    int i_4 = int(0);
    for(;;)
    {
        if(i_4 < int(2))
        {
        }
        else
        {
            break;
        }
        *_slang_vector_get_element_ptr(&result_5, i_4) = (F32_exp((_slang_vector_get_element(x_5, i_4))));
        i_4 = i_4 + int(1);
    }
    return result_5;
}

inline __device__ void _d_exp_vector_1(DiffPair_vectorx3Cfloatx2C2x3E_0 * dpx_8, float2  dOut_8)
{
    float2  _S11 = exp_1((*dpx_8).primal_0) * dOut_8;
    dpx_8->primal_0 = (*dpx_8).primal_0;
    dpx_8->differential_0 = _S11;
    return;
}

inline __device__ void per_splat_losses(float3  scales_0, float opacity_0, float4  quat_0, float mcmc_opacity_reg_weight_0, float mcmc_scale_reg_weight_0, float max_gauss_ratio_0, float scale_regularization_weight_0, float erank_reg_weight_0, float erank_reg_weight_s3_0, float quat_norm_reg_weight_0, bool surfel_0, FixedArray<float, 5>  * _S12)
{
    float3  _S13 = max_0(scales_0, make_float3 (-10000.0f));
    FixedArray<float, 5>  losses_0;
    losses_0[int(0)] = mcmc_opacity_reg_weight_0 * (F32_max((1.0f / (1.0f + (F32_exp((- opacity_0))))), (0.5f + 0.25f * opacity_0)));
    float quat_norm_0 = length_0(quat_0);
    losses_0[int(4)] = quat_norm_reg_weight_0 * (quat_norm_0 - 1.0f - (F32_log((quat_norm_0))));
    if(!surfel_0)
    {
        float3  _S14 = max_0(_S13, make_float3 (-40.0f));
        losses_0[int(1)] = mcmc_scale_reg_weight_0 * 0.00999999977648258f * (_S14.x + _S14.y + _S14.z) / 3.0f;
        float _S15 = _S13.x;
        float _S16 = _S13.y;
        float _S17 = _S13.z;
        float _S18 = (F32_max(((F32_max((_S15), (_S16)))), (_S17)));
        losses_0[int(2)] = scale_regularization_weight_0 * ((F32_max(((F32_exp(((F32_min((_S18 - (F32_min(((F32_min((_S15), (_S16)))), (_S17)))), (80.0f))))))), (max_gauss_ratio_0))) - max_gauss_ratio_0);
        float3  _S19 = exp_0(make_float3 (2.0f) * (_S13 - make_float3 (_S18)));
        float x_6 = _S19.x;
        float y_3 = _S19.y;
        float z_0 = _S19.z;
        float s_0 = x_6 + y_3 + z_0;
        float s1_0 = (F32_max(((F32_max((x_6), (y_3)))), (z_0))) / s_0;
        float _S20 = (F32_max(((F32_min(((F32_min((x_6), (y_3)))), (z_0))) / s_0), (1.00000000317107685e-30f)));
        float _S21 = (F32_max((1.0f - s1_0 - _S20), (1.00000000317107685e-30f)));
        losses_0[int(3)] = erank_reg_weight_0 * (F32_max((- (F32_log(((F32_exp((- s1_0 * (F32_log((s1_0))) - _S21 * (F32_log((_S21))) - _S20 * (F32_log((_S20)))))) - 0.99998998641967773f)))), (0.0f))) + erank_reg_weight_s3_0 * _S20;
    }
    else
    {
        float2  _S22 = float2 {_S13.x, _S13.y};
        float2  _S23 = max_1(_S22, make_float2 (-40.0f));
        losses_0[int(1)] = mcmc_scale_reg_weight_0 * 0.00999999977648258f * (_S23.x + _S23.y) / 2.0f;
        float _S24 = _S22.x;
        float _S25 = _S22.y;
        float _S26 = (F32_max((_S24), (_S25)));
        losses_0[int(2)] = scale_regularization_weight_0 * ((F32_max(((F32_exp(((F32_min((_S26 - (F32_min((_S24), (_S25)))), (80.0f))))))), (max_gauss_ratio_0))) - max_gauss_ratio_0);
        float2  _S27 = exp_1(make_float2 (2.0f) * (_S22 - make_float2 (_S26)));
        float x_7 = _S27.x;
        float y_4 = _S27.y;
        float s_1 = x_7 + y_4;
        float s1_1 = (F32_max((x_7), (y_4))) / s_1;
        float _S28 = (F32_max(((F32_min((x_7), (y_4))) / s_1), (1.00000000317107685e-30f)));
        losses_0[int(3)] = erank_reg_weight_0 * (F32_max((- (F32_log(((F32_exp((- s1_1 * (F32_log((s1_1))) - _S28 * (F32_log((_S28)))))) - 0.99998998641967773f)))), (0.0f)));
    }
    *_S12 = losses_0;
    return;
}

inline __device__ float3  s_primal_ctx_max_0(float3  _S29, float3  _S30)
{
    return max_0(_S29, _S30);
}

inline __device__ float s_primal_ctx_exp_0(float _S31)
{
    return (F32_exp((_S31)));
}

inline __device__ float3  s_primal_ctx_exp_1(float3  _S32)
{
    return exp_0(_S32);
}

inline __device__ float s_primal_ctx_log_0(float _S33)
{
    return (F32_log((_S33)));
}

inline __device__ float2  s_primal_ctx_exp_2(float2  _S34)
{
    return exp_1(_S34);
}

inline __device__ void s_bwd_prop_log_0(DiffPair_float_0 * _S35, float _S36)
{
    _d_log_0(_S35, _S36);
    return;
}

inline __device__ void s_bwd_prop_exp_0(DiffPair_float_0 * _S37, float _S38)
{
    _d_exp_0(_S37, _S38);
    return;
}

inline __device__ void s_bwd_prop_exp_1(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S39, float3  _S40)
{
    _d_exp_vector_0(_S39, _S40);
    return;
}

inline __device__ void s_bwd_prop_max_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S41, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S42, float3  _S43)
{
    _d_max_vector_0(_S41, _S42, _S43);
    return;
}

inline __device__ void s_bwd_prop_exp_2(DiffPair_vectorx3Cfloatx2C2x3E_0 * _S44, float2  _S45)
{
    _d_exp_vector_1(_S44, _S45);
    return;
}

inline __device__ void s_bwd_prop_max_1(DiffPair_vectorx3Cfloatx2C2x3E_0 * _S46, DiffPair_vectorx3Cfloatx2C2x3E_0 * _S47, float2  _S48)
{
    _d_max_vector_1(_S46, _S47, _S48);
    return;
}

struct DiffPair_vectorx3Cfloatx2C4x3E_0
{
    float4  primal_0;
    float4  differential_0;
};

inline __device__ void s_bwd_prop_sqrt_0(DiffPair_float_0 * _S49, float _S50)
{
    _d_sqrt_0(_S49, _S50);
    return;
}

inline __device__ void s_bwd_prop_length_impl_0(DiffPair_vectorx3Cfloatx2C4x3E_0 * dpx_9, float _s_dOut_0)
{
    float _S51 = (*dpx_9).primal_0.x;
    float _S52 = (*dpx_9).primal_0.y;
    float _S53 = (*dpx_9).primal_0.z;
    float _S54 = (*dpx_9).primal_0.w;
    DiffPair_float_0 _S55;
    (&_S55)->primal_0 = _S51 * _S51 + _S52 * _S52 + _S53 * _S53 + _S54 * _S54;
    (&_S55)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S55, _s_dOut_0);
    float _S56 = (*dpx_9).primal_0.w * _S55.differential_0;
    float _S57 = _S56 + _S56;
    float _S58 = (*dpx_9).primal_0.z * _S55.differential_0;
    float _S59 = _S58 + _S58;
    float _S60 = (*dpx_9).primal_0.y * _S55.differential_0;
    float _S61 = _S60 + _S60;
    float _S62 = (*dpx_9).primal_0.x * _S55.differential_0;
    float _S63 = _S62 + _S62;
    float4  _S64 = make_float4 (0.0f);
    *&((&_S64)->w) = _S57;
    *&((&_S64)->z) = _S59;
    *&((&_S64)->y) = _S61;
    *&((&_S64)->x) = _S63;
    dpx_9->primal_0 = (*dpx_9).primal_0;
    dpx_9->differential_0 = _S64;
    return;
}

inline __device__ void s_bwd_length_impl_0(DiffPair_vectorx3Cfloatx2C4x3E_0 * _S65, float _S66)
{
    s_bwd_prop_length_impl_0(_S65, _S66);
    return;
}

inline __device__ void per_splat_losses_bwd(float3  scales_1, float opacity_1, float4  quat_1, FixedArray<float, 5>  v_loss_0, float3  * v_scales_0, float * v_opacity_0, float4  * v_quat_0, float mcmc_opacity_reg_weight_1, float mcmc_scale_reg_weight_1, float max_gauss_ratio_1, float scale_regularization_weight_1, float erank_reg_weight_1, float erank_reg_weight_s3_1, float quat_norm_reg_weight_1, bool surfel_1)
{
    float3  _S67 = make_float3 (0.0f);
    float2  _S68 = make_float2 (0.0f);
    float3  _S69 = make_float3 (-10000.0f);
    float3  _S70 = s_primal_ctx_max_0(scales_1, _S69);
    float _S71 = - opacity_1;
    float _S72 = 1.0f + s_primal_ctx_exp_0(_S71);
    float reg_0 = 1.0f / _S72;
    float _S73 = _S72 * _S72;
    float _S74 = 0.5f + 0.25f * opacity_1;
    float _S75 = length_0(quat_1);
    bool _S76 = !surfel_1;
    float _S77;
    float _S78;
    float _S79;
    float _S80;
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
    float _S93;
    float _S94;
    float _S95;
    float _S96;
    float _S97;
    float _S98;
    float _S99;
    float _S100;
    float _S101;
    float _S102;
    float _S103;
    float _S104;
    float _S105;
    float _S106;
    float _S107;
    float _S108;
    float _S109;
    float _S110;
    float _S111;
    float _S112;
    float _S113;
    float _S114;
    float _S115;
    float _S116;
    float _S117;
    float _S118;
    float _S119;
    float _S120;
    float _S121;
    float _S122;
    float _S123;
    float _S124;
    float _S125;
    float _S126;
    float _S127;
    float3  _S128;
    float3  _S129;
    float2  _S130;
    float2  _S131;
    float2  _S132;
    if(_S76)
    {
        float3  _S133 = make_float3 (-40.0f);
        float _S134 = mcmc_scale_reg_weight_1 * 0.00999999977648258f;
        float _S135 = _S70.x;
        float _S136 = _S70.y;
        float _S137 = (F32_max((_S135), (_S136)));
        float _S138 = _S70.z;
        float _S139 = (F32_max((_S137), (_S138)));
        float _S140 = (F32_min((_S135), (_S136)));
        float _S141 = _S139 - (F32_min((_S140), (_S138)));
        float _S142 = (F32_min((_S141), (80.0f)));
        float _S143 = s_primal_ctx_exp_0(_S142);
        float3  _S144 = make_float3 (2.0f) * (_S70 - make_float3 (_S139));
        float3  _S145 = s_primal_ctx_exp_1(_S144);
        float x_8 = _S145.x;
        float y_5 = _S145.y;
        float z_1 = _S145.z;
        float s_2 = x_8 + y_5 + z_1;
        float _S146 = (F32_max((x_8), (y_5)));
        float _S147 = (F32_max((_S146), (z_1)));
        float s1_2 = _S147 / s_2;
        float _S148 = s_2 * s_2;
        float _S149 = (F32_min((x_8), (y_5)));
        float _S150 = (F32_min((_S149), (z_1)));
        float _S151 = _S150 / s_2;
        float _S152 = (F32_max((_S151), (1.00000000317107685e-30f)));
        float _S153 = 1.0f - s1_2 - _S152;
        float _S154 = (F32_max((_S153), (1.00000000317107685e-30f)));
        float _S155 = - s1_2;
        float _S156 = s_primal_ctx_log_0(s1_2);
        float _S157 = s_primal_ctx_log_0(_S154);
        float _S158 = s_primal_ctx_log_0(_S152);
        float _S159 = _S155 * _S156 - _S154 * _S157 - _S152 * _S158;
        float _S160 = s_primal_ctx_exp_0(_S159) - 0.99998998641967773f;
        _S77 = - s_primal_ctx_log_0(_S160);
        _S78 = _S160;
        _S79 = _S159;
        _S80 = _S152;
        _S81 = _S158;
        _S82 = _S154;
        _S83 = _S157;
        _S84 = _S155;
        _S85 = _S156;
        _S86 = s1_2;
        _S87 = _S153;
        _S88 = _S151;
        _S89 = _S148;
        _S90 = _S150;
        _S91 = s_2;
        _S92 = _S149;
        _S93 = z_1;
        _S94 = x_8;
        _S95 = y_5;
        _S96 = _S147;
        _S97 = _S146;
        _S128 = _S144;
        _S98 = _S143;
        _S99 = _S142;
        _S100 = _S141;
        _S101 = _S140;
        _S102 = _S138;
        _S103 = _S135;
        _S104 = _S136;
        _S105 = _S137;
        _S106 = _S134;
        _S129 = _S133;
        _S107 = 0.0f;
        _S108 = 0.0f;
        _S109 = 0.0f;
        _S110 = 0.0f;
        _S111 = 0.0f;
        _S112 = 0.0f;
        _S113 = 0.0f;
        _S114 = 0.0f;
        _S115 = 0.0f;
        _S116 = 0.0f;
        _S117 = 0.0f;
        _S118 = 0.0f;
        _S119 = 0.0f;
        _S120 = 0.0f;
        _S121 = 0.0f;
        _S130 = _S68;
        _S122 = 0.0f;
        _S123 = 0.0f;
        _S124 = 0.0f;
        _S125 = 0.0f;
        _S126 = 0.0f;
        _S127 = 0.0f;
        _S131 = _S68;
        _S132 = _S68;
    }
    else
    {
        float2  _S161 = float2 {_S70.x, _S70.y};
        float2  _S162 = make_float2 (-40.0f);
        float _S163 = mcmc_scale_reg_weight_1 * 0.00999999977648258f;
        float _S164 = _S161.x;
        float _S165 = _S161.y;
        float _S166 = (F32_max((_S164), (_S165)));
        float _S167 = _S166 - (F32_min((_S164), (_S165)));
        float _S168 = (F32_min((_S167), (80.0f)));
        float _S169 = s_primal_ctx_exp_0(_S168);
        float2  _S170 = make_float2 (2.0f) * (_S161 - make_float2 (_S166));
        float2  _S171 = s_primal_ctx_exp_2(_S170);
        float x_9 = _S171.x;
        float y_6 = _S171.y;
        float s_3 = x_9 + y_6;
        float _S172 = (F32_max((x_9), (y_6)));
        float s1_3 = _S172 / s_3;
        float _S173 = s_3 * s_3;
        float _S174 = (F32_min((x_9), (y_6)));
        float _S175 = _S174 / s_3;
        float _S176 = (F32_max((_S175), (1.00000000317107685e-30f)));
        float _S177 = - s1_3;
        float _S178 = s_primal_ctx_log_0(s1_3);
        float _S179 = s_primal_ctx_log_0(_S176);
        float _S180 = _S177 * _S178 - _S176 * _S179;
        float _S181 = s_primal_ctx_exp_0(_S180) - 0.99998998641967773f;
        float _S182 = - s_primal_ctx_log_0(_S181);
        _S77 = 0.0f;
        _S78 = 0.0f;
        _S79 = 0.0f;
        _S80 = 0.0f;
        _S81 = 0.0f;
        _S82 = 0.0f;
        _S83 = 0.0f;
        _S84 = 0.0f;
        _S85 = 0.0f;
        _S86 = 0.0f;
        _S87 = 0.0f;
        _S88 = 0.0f;
        _S89 = 0.0f;
        _S90 = 0.0f;
        _S91 = 0.0f;
        _S92 = 0.0f;
        _S93 = 0.0f;
        _S94 = 0.0f;
        _S95 = 0.0f;
        _S96 = 0.0f;
        _S97 = 0.0f;
        _S128 = _S67;
        _S98 = 0.0f;
        _S99 = 0.0f;
        _S100 = 0.0f;
        _S101 = 0.0f;
        _S102 = 0.0f;
        _S103 = 0.0f;
        _S104 = 0.0f;
        _S105 = 0.0f;
        _S106 = 0.0f;
        _S129 = _S67;
        _S107 = _S182;
        _S108 = _S181;
        _S109 = _S180;
        _S110 = _S176;
        _S111 = _S179;
        _S112 = _S177;
        _S113 = _S178;
        _S114 = s1_3;
        _S115 = _S175;
        _S116 = _S173;
        _S117 = _S174;
        _S118 = s_3;
        _S119 = x_9;
        _S120 = y_6;
        _S121 = _S172;
        _S130 = _S170;
        _S122 = _S169;
        _S123 = _S168;
        _S124 = _S167;
        _S125 = _S164;
        _S126 = _S165;
        _S127 = _S163;
        _S131 = _S161;
        _S132 = _S162;
    }
    if(_S76)
    {
        float _S183 = erank_reg_weight_s3_1 * v_loss_0[int(3)];
        float _S184 = erank_reg_weight_1 * v_loss_0[int(3)];
        DiffPair_float_0 _S185;
        (&_S185)->primal_0 = _S77;
        (&_S185)->differential_0 = 0.0f;
        DiffPair_float_0 _S186;
        (&_S186)->primal_0 = 0.0f;
        (&_S186)->differential_0 = 0.0f;
        _d_max_0(&_S185, &_S186, _S184);
        float _S187 = - _S185.differential_0;
        DiffPair_float_0 _S188;
        (&_S188)->primal_0 = _S78;
        (&_S188)->differential_0 = 0.0f;
        s_bwd_prop_log_0(&_S188, _S187);
        DiffPair_float_0 _S189;
        (&_S189)->primal_0 = _S79;
        (&_S189)->differential_0 = 0.0f;
        s_bwd_prop_exp_0(&_S189, _S188.differential_0);
        float _S190 = - _S189.differential_0;
        float _S191 = _S80 * _S190;
        float _S192 = _S81 * _S190;
        DiffPair_float_0 _S193;
        (&_S193)->primal_0 = _S80;
        (&_S193)->differential_0 = 0.0f;
        s_bwd_prop_log_0(&_S193, _S191);
        float _S194 = _S82 * _S190;
        float _S195 = _S83 * _S190;
        DiffPair_float_0 _S196;
        (&_S196)->primal_0 = _S82;
        (&_S196)->differential_0 = 0.0f;
        s_bwd_prop_log_0(&_S196, _S194);
        float _S197 = _S84 * _S189.differential_0;
        float _S198 = _S85 * _S189.differential_0;
        DiffPair_float_0 _S199;
        (&_S199)->primal_0 = _S86;
        (&_S199)->differential_0 = 0.0f;
        s_bwd_prop_log_0(&_S199, _S197);
        float _S200 = - _S198;
        float _S201 = _S195 + _S196.differential_0;
        DiffPair_float_0 _S202;
        (&_S202)->primal_0 = _S87;
        (&_S202)->differential_0 = 0.0f;
        DiffPair_float_0 _S203;
        (&_S203)->primal_0 = 1.00000000317107685e-30f;
        (&_S203)->differential_0 = 0.0f;
        _d_max_0(&_S202, &_S203, _S201);
        float _S204 = - _S202.differential_0;
        float _S205 = _S183 + _S192 + _S193.differential_0 + _S204;
        DiffPair_float_0 _S206;
        (&_S206)->primal_0 = _S88;
        (&_S206)->differential_0 = 0.0f;
        DiffPair_float_0 _S207;
        (&_S207)->primal_0 = 1.00000000317107685e-30f;
        (&_S207)->differential_0 = 0.0f;
        _d_max_0(&_S206, &_S207, _S205);
        float _S208 = _S206.differential_0 / _S89;
        float _S209 = _S90 * - _S208;
        float _S210 = _S91 * _S208;
        DiffPair_float_0 _S211;
        (&_S211)->primal_0 = _S92;
        (&_S211)->differential_0 = 0.0f;
        DiffPair_float_0 _S212;
        (&_S212)->primal_0 = _S93;
        (&_S212)->differential_0 = 0.0f;
        _d_min_0(&_S211, &_S212, _S210);
        DiffPair_float_0 _S213;
        (&_S213)->primal_0 = _S94;
        (&_S213)->differential_0 = 0.0f;
        DiffPair_float_0 _S214;
        (&_S214)->primal_0 = _S95;
        (&_S214)->differential_0 = 0.0f;
        _d_min_0(&_S213, &_S214, _S211.differential_0);
        float _S215 = (_S199.differential_0 + _S200 + _S204) / _S89;
        float _S216 = _S96 * - _S215;
        float _S217 = _S91 * _S215;
        DiffPair_float_0 _S218;
        (&_S218)->primal_0 = _S97;
        (&_S218)->differential_0 = 0.0f;
        DiffPair_float_0 _S219;
        (&_S219)->primal_0 = _S93;
        (&_S219)->differential_0 = 0.0f;
        _d_max_0(&_S218, &_S219, _S217);
        DiffPair_float_0 _S220;
        (&_S220)->primal_0 = _S94;
        (&_S220)->differential_0 = 0.0f;
        DiffPair_float_0 _S221;
        (&_S221)->primal_0 = _S95;
        (&_S221)->differential_0 = 0.0f;
        _d_max_0(&_S220, &_S221, _S218.differential_0);
        float _S222 = _S209 + _S216;
        float3  _S223 = make_float3 (_S213.differential_0 + _S220.differential_0 + _S222, _S214.differential_0 + _S221.differential_0 + _S222, _S212.differential_0 + _S219.differential_0 + _S222);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S224;
        (&_S224)->primal_0 = _S128;
        (&_S224)->differential_0 = _S67;
        s_bwd_prop_exp_1(&_S224, _S223);
        float3  _S225 = make_float3 (2.0f) * _S224.differential_0;
        float3  _S226 = - _S225;
        float s_diff_scale_reg_T_0 = scale_regularization_weight_1 * v_loss_0[int(2)];
        DiffPair_float_0 _S227;
        (&_S227)->primal_0 = _S98;
        (&_S227)->differential_0 = 0.0f;
        DiffPair_float_0 _S228;
        (&_S228)->primal_0 = max_gauss_ratio_1;
        (&_S228)->differential_0 = 0.0f;
        _d_max_0(&_S227, &_S228, s_diff_scale_reg_T_0);
        DiffPair_float_0 _S229;
        (&_S229)->primal_0 = _S99;
        (&_S229)->differential_0 = 0.0f;
        s_bwd_prop_exp_0(&_S229, _S227.differential_0);
        DiffPair_float_0 _S230;
        (&_S230)->primal_0 = _S100;
        (&_S230)->differential_0 = 0.0f;
        DiffPair_float_0 _S231;
        (&_S231)->primal_0 = 80.0f;
        (&_S231)->differential_0 = 0.0f;
        _d_min_0(&_S230, &_S231, _S229.differential_0);
        float _S232 = - _S230.differential_0;
        DiffPair_float_0 _S233;
        (&_S233)->primal_0 = _S101;
        (&_S233)->differential_0 = 0.0f;
        DiffPair_float_0 _S234;
        (&_S234)->primal_0 = _S102;
        (&_S234)->differential_0 = 0.0f;
        _d_min_0(&_S233, &_S234, _S232);
        DiffPair_float_0 _S235;
        (&_S235)->primal_0 = _S103;
        (&_S235)->differential_0 = 0.0f;
        DiffPair_float_0 _S236;
        (&_S236)->primal_0 = _S104;
        (&_S236)->differential_0 = 0.0f;
        _d_min_0(&_S235, &_S236, _S233.differential_0);
        float _S237 = _S230.differential_0 + _S226.x + _S226.y + _S226.z;
        DiffPair_float_0 _S238;
        (&_S238)->primal_0 = _S105;
        (&_S238)->differential_0 = 0.0f;
        DiffPair_float_0 _S239;
        (&_S239)->primal_0 = _S102;
        (&_S239)->differential_0 = 0.0f;
        _d_max_0(&_S238, &_S239, _S237);
        float _S240 = _S234.differential_0 + _S239.differential_0;
        DiffPair_float_0 _S241;
        (&_S241)->primal_0 = _S103;
        (&_S241)->differential_0 = 0.0f;
        DiffPair_float_0 _S242;
        (&_S242)->primal_0 = _S104;
        (&_S242)->differential_0 = 0.0f;
        _d_max_0(&_S241, &_S242, _S238.differential_0);
        float _S243 = _S236.differential_0 + _S242.differential_0;
        float _S244 = _S235.differential_0 + _S241.differential_0;
        float _S245 = _S106 * (0.3333333432674408f * v_loss_0[int(1)]);
        float3  _S246 = make_float3 (_S245, _S245, _S245);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S247;
        (&_S247)->primal_0 = _S70;
        (&_S247)->differential_0 = _S67;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S248;
        (&_S248)->primal_0 = _S129;
        (&_S248)->differential_0 = _S67;
        s_bwd_prop_max_0(&_S247, &_S248, _S246);
        float3  _S249 = _S225 + _S247.differential_0 + make_float3 (_S244, _S243, _S240);
        _S77 = v_loss_0[int(4)];
        _S78 = v_loss_0[int(0)];
        _S128 = _S249;
    }
    else
    {
        float _S250 = erank_reg_weight_1 * v_loss_0[int(3)];
        DiffPair_float_0 _S251;
        (&_S251)->primal_0 = _S107;
        (&_S251)->differential_0 = 0.0f;
        DiffPair_float_0 _S252;
        (&_S252)->primal_0 = 0.0f;
        (&_S252)->differential_0 = 0.0f;
        _d_max_0(&_S251, &_S252, _S250);
        float _S253 = - _S251.differential_0;
        DiffPair_float_0 _S254;
        (&_S254)->primal_0 = _S108;
        (&_S254)->differential_0 = 0.0f;
        s_bwd_prop_log_0(&_S254, _S253);
        DiffPair_float_0 _S255;
        (&_S255)->primal_0 = _S109;
        (&_S255)->differential_0 = 0.0f;
        s_bwd_prop_exp_0(&_S255, _S254.differential_0);
        float _S256 = - _S255.differential_0;
        float _S257 = _S110 * _S256;
        float _S258 = _S111 * _S256;
        DiffPair_float_0 _S259;
        (&_S259)->primal_0 = _S110;
        (&_S259)->differential_0 = 0.0f;
        s_bwd_prop_log_0(&_S259, _S257);
        float _S260 = _S112 * _S255.differential_0;
        float _S261 = _S113 * _S255.differential_0;
        DiffPair_float_0 _S262;
        (&_S262)->primal_0 = _S114;
        (&_S262)->differential_0 = 0.0f;
        s_bwd_prop_log_0(&_S262, _S260);
        float _S263 = - _S261;
        float _S264 = _S258 + _S259.differential_0;
        DiffPair_float_0 _S265;
        (&_S265)->primal_0 = _S115;
        (&_S265)->differential_0 = 0.0f;
        DiffPair_float_0 _S266;
        (&_S266)->primal_0 = 1.00000000317107685e-30f;
        (&_S266)->differential_0 = 0.0f;
        _d_max_0(&_S265, &_S266, _S264);
        float _S267 = _S265.differential_0 / _S116;
        float _S268 = _S117 * - _S267;
        float _S269 = _S118 * _S267;
        DiffPair_float_0 _S270;
        (&_S270)->primal_0 = _S119;
        (&_S270)->differential_0 = 0.0f;
        DiffPair_float_0 _S271;
        (&_S271)->primal_0 = _S120;
        (&_S271)->differential_0 = 0.0f;
        _d_min_0(&_S270, &_S271, _S269);
        float _S272 = (_S262.differential_0 + _S263) / _S116;
        float _S273 = _S121 * - _S272;
        float _S274 = _S118 * _S272;
        DiffPair_float_0 _S275;
        (&_S275)->primal_0 = _S119;
        (&_S275)->differential_0 = 0.0f;
        DiffPair_float_0 _S276;
        (&_S276)->primal_0 = _S120;
        (&_S276)->differential_0 = 0.0f;
        _d_max_0(&_S275, &_S276, _S274);
        float _S277 = _S268 + _S273;
        float2  _S278 = make_float2 (_S270.differential_0 + _S275.differential_0 + _S277, _S271.differential_0 + _S276.differential_0 + _S277);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S279;
        (&_S279)->primal_0 = _S130;
        (&_S279)->differential_0 = _S68;
        s_bwd_prop_exp_2(&_S279, _S278);
        float2  _S280 = make_float2 (2.0f) * _S279.differential_0;
        float2  _S281 = - _S280;
        float s_diff_scale_reg_T_1 = scale_regularization_weight_1 * v_loss_0[int(2)];
        DiffPair_float_0 _S282;
        (&_S282)->primal_0 = _S122;
        (&_S282)->differential_0 = 0.0f;
        DiffPair_float_0 _S283;
        (&_S283)->primal_0 = max_gauss_ratio_1;
        (&_S283)->differential_0 = 0.0f;
        _d_max_0(&_S282, &_S283, s_diff_scale_reg_T_1);
        DiffPair_float_0 _S284;
        (&_S284)->primal_0 = _S123;
        (&_S284)->differential_0 = 0.0f;
        s_bwd_prop_exp_0(&_S284, _S282.differential_0);
        DiffPair_float_0 _S285;
        (&_S285)->primal_0 = _S124;
        (&_S285)->differential_0 = 0.0f;
        DiffPair_float_0 _S286;
        (&_S286)->primal_0 = 80.0f;
        (&_S286)->differential_0 = 0.0f;
        _d_min_0(&_S285, &_S286, _S284.differential_0);
        float _S287 = - _S285.differential_0;
        DiffPair_float_0 _S288;
        (&_S288)->primal_0 = _S125;
        (&_S288)->differential_0 = 0.0f;
        DiffPair_float_0 _S289;
        (&_S289)->primal_0 = _S126;
        (&_S289)->differential_0 = 0.0f;
        _d_min_0(&_S288, &_S289, _S287);
        float _S290 = _S285.differential_0 + _S281.x + _S281.y;
        DiffPair_float_0 _S291;
        (&_S291)->primal_0 = _S125;
        (&_S291)->differential_0 = 0.0f;
        DiffPair_float_0 _S292;
        (&_S292)->primal_0 = _S126;
        (&_S292)->differential_0 = 0.0f;
        _d_max_0(&_S291, &_S292, _S290);
        float _S293 = _S289.differential_0 + _S292.differential_0;
        float _S294 = _S288.differential_0 + _S291.differential_0;
        float _S295 = _S127 * (0.5f * v_loss_0[int(1)]);
        float2  _S296 = make_float2 (_S295, _S295);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S297;
        (&_S297)->primal_0 = _S131;
        (&_S297)->differential_0 = _S68;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S298;
        (&_S298)->primal_0 = _S132;
        (&_S298)->differential_0 = _S68;
        s_bwd_prop_max_1(&_S297, &_S298, _S296);
        float2  _S299 = _S280 + _S297.differential_0 + make_float2 (_S294, _S293);
        float3  _S300 = make_float3 (_S299.x, _S299.y, 0.0f);
        _S77 = v_loss_0[int(4)];
        _S78 = v_loss_0[int(0)];
        _S128 = _S300;
    }
    float s_diff_quat_norm_reg_T_0 = quat_norm_reg_weight_1 * _S77;
    float _S301 = - s_diff_quat_norm_reg_T_0;
    DiffPair_float_0 _S302;
    (&_S302)->primal_0 = _S75;
    (&_S302)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S302, _S301);
    float _S303 = _S302.differential_0 + s_diff_quat_norm_reg_T_0;
    float4  _S304 = make_float4 (0.0f);
    DiffPair_vectorx3Cfloatx2C4x3E_0 _S305;
    (&_S305)->primal_0 = quat_1;
    (&_S305)->differential_0 = _S304;
    s_bwd_length_impl_0(&_S305, _S303);
    DiffPair_vectorx3Cfloatx2C4x3E_0 _S306 = _S305;
    float _S307 = mcmc_opacity_reg_weight_1 * _S78;
    DiffPair_float_0 _S308;
    (&_S308)->primal_0 = reg_0;
    (&_S308)->differential_0 = 0.0f;
    DiffPair_float_0 _S309;
    (&_S309)->primal_0 = _S74;
    (&_S309)->differential_0 = 0.0f;
    _d_max_0(&_S308, &_S309, _S307);
    float _S310 = 0.25f * _S309.differential_0;
    float _S311 = - (_S308.differential_0 / _S73);
    DiffPair_float_0 _S312;
    (&_S312)->primal_0 = _S71;
    (&_S312)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S312, _S311);
    float _S313 = - _S312.differential_0;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S314;
    (&_S314)->primal_0 = scales_1;
    (&_S314)->differential_0 = _S67;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S315;
    (&_S315)->primal_0 = _S69;
    (&_S315)->differential_0 = _S67;
    s_bwd_prop_max_0(&_S314, &_S315, _S128);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S316 = _S314;
    float _S317 = _S310 + _S313;
    *v_scales_0 = _S316.differential_0;
    *v_opacity_0 = _S317;
    *v_quat_0 = _S306.differential_0;
    return;
}

