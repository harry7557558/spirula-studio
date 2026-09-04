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

inline __device__ void _d_exp_0(DiffPair_float_0 * dpx_2, float dOut_2)
{
    float _S4 = (F32_exp(((*dpx_2).primal_0))) * dOut_2;
    dpx_2->primal_0 = (*dpx_2).primal_0;
    dpx_2->differential_0 = _S4;
    return;
}

inline __device__ void _d_sqrt_0(DiffPair_float_0 * dpx_3, float dOut_3)
{
    float _S5 = 0.5f / (F32_sqrt(((F32_max((1.00000001168609742e-07f), ((*dpx_3).primal_0)))))) * dOut_3;
    dpx_3->primal_0 = (*dpx_3).primal_0;
    dpx_3->differential_0 = _S5;
    return;
}

inline __device__ float dot_0(float4  x_1, float4  y_1)
{
    int i_1 = int(0);
    float result_1 = 0.0f;
    for(;;)
    {
        if(i_1 < int(4))
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

inline __device__ float length_0(float4  x_2)
{
    return (F32_sqrt((dot_0(x_2, x_2))));
}

inline __device__ void _d_log_0(DiffPair_float_0 * dpx_4, float dOut_4)
{
    float _S6 = 1.0f / (*dpx_4).primal_0 * dOut_4;
    dpx_4->primal_0 = (*dpx_4).primal_0;
    dpx_4->differential_0 = _S6;
    return;
}

inline __device__ void _d_min_0(DiffPair_float_0 * dpx_5, DiffPair_float_0 * dpy_2, float dOut_5)
{
    DiffPair_float_0 _S7 = *dpx_5;
    float _S8;
    if(((*dpx_5).primal_0) < ((*dpy_2).primal_0))
    {
        _S8 = dOut_5;
    }
    else
    {
        if(((*dpx_5).primal_0) > ((*dpy_2).primal_0))
        {
            _S8 = 0.0f;
        }
        else
        {
            _S8 = 0.5f * dOut_5;
        }
    }
    dpx_5->primal_0 = _S7.primal_0;
    dpx_5->differential_0 = _S8;
    DiffPair_float_0 _S9 = *dpy_2;
    if(((*dpy_2).primal_0) < (_S7.primal_0))
    {
        _S8 = dOut_5;
    }
    else
    {
        if(((*dpy_2).primal_0) > ((*dpx_5).primal_0))
        {
            _S8 = 0.0f;
        }
        else
        {
            _S8 = 0.5f * dOut_5;
        }
    }
    dpy_2->primal_0 = _S9.primal_0;
    dpy_2->differential_0 = _S8;
    return;
}

inline __device__ float3  exp_0(float3  x_3)
{
    float3  result_3;
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
        *_slang_vector_get_element_ptr(&result_3, i_2) = (F32_exp((_slang_vector_get_element(x_3, i_2))));
        i_2 = i_2 + int(1);
    }
    return result_3;
}

inline __device__ void _d_exp_vector_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_6, float3  dOut_6)
{
    float3  _S10 = exp_0((*dpx_6).primal_0) * dOut_6;
    dpx_6->primal_0 = (*dpx_6).primal_0;
    dpx_6->differential_0 = _S10;
    return;
}

inline __device__ void per_splat_losses(float3  scales_0, float opacity_0, float4  quat_0, float mcmc_opacity_reg_weight_0, float mcmc_scale_reg_weight_0, float max_gauss_ratio_0, float scale_regularization_weight_0, float erank_reg_weight_0, float erank_reg_weight_s3_0, float quat_norm_reg_weight_0, FixedArray<float, 5>  * _S11)
{
    float3  _S12 = max_0(scales_0, make_float3 (-10000.0f));
    FixedArray<float, 5>  losses_0;
    losses_0[int(0)] = mcmc_opacity_reg_weight_0 * (F32_max((1.0f / (1.0f + (F32_exp((- opacity_0))))), (0.5f + 0.25f * opacity_0)));
    float quat_norm_0 = length_0(quat_0);
    losses_0[int(4)] = quat_norm_reg_weight_0 * (quat_norm_0 - 1.0f - (F32_log((quat_norm_0))));
    float3  _S13 = max_0(_S12, make_float3 (-40.0f));
    losses_0[int(1)] = mcmc_scale_reg_weight_0 * 0.00999999977648258f * (_S13.x + _S13.y + _S13.z) / 3.0f;
    float _S14 = _S12.x;
    float _S15 = _S12.y;
    float _S16 = _S12.z;
    float _S17 = (F32_max(((F32_max((_S14), (_S15)))), (_S16)));
    losses_0[int(2)] = scale_regularization_weight_0 * ((F32_max(((F32_exp(((F32_min((_S17 - (F32_min(((F32_min((_S14), (_S15)))), (_S16)))), (80.0f))))))), (max_gauss_ratio_0))) - max_gauss_ratio_0);
    float3  _S18 = exp_0(make_float3 (2.0f) * (_S12 - make_float3 (_S17)));
    float x_4 = _S18.x;
    float y_2 = _S18.y;
    float z_0 = _S18.z;
    float s_0 = x_4 + y_2 + z_0;
    float s1_0 = (F32_max(((F32_max((x_4), (y_2)))), (z_0))) / s_0;
    float _S19 = (F32_max(((F32_min(((F32_min((x_4), (y_2)))), (z_0))) / s_0), (1.00000000317107685e-30f)));
    float _S20 = (F32_max((1.0f - s1_0 - _S19), (1.00000000317107685e-30f)));
    losses_0[int(3)] = erank_reg_weight_0 * (F32_max((- (F32_log(((F32_exp((- s1_0 * (F32_log((s1_0))) - _S20 * (F32_log((_S20))) - _S19 * (F32_log((_S19)))))) - 0.99998998641967773f)))), (0.0f))) + erank_reg_weight_s3_0 * _S19;
    *_S11 = losses_0;
    return;
}

inline __device__ float3  s_primal_ctx_max_0(float3  _S21, float3  _S22)
{
    return max_0(_S21, _S22);
}

inline __device__ float s_primal_ctx_exp_0(float _S23)
{
    return (F32_exp((_S23)));
}

inline __device__ float3  s_primal_ctx_exp_1(float3  _S24)
{
    return exp_0(_S24);
}

inline __device__ float s_primal_ctx_log_0(float _S25)
{
    return (F32_log((_S25)));
}

inline __device__ void s_bwd_prop_log_0(DiffPair_float_0 * _S26, float _S27)
{
    _d_log_0(_S26, _S27);
    return;
}

inline __device__ void s_bwd_prop_exp_0(DiffPair_float_0 * _S28, float _S29)
{
    _d_exp_0(_S28, _S29);
    return;
}

inline __device__ void s_bwd_prop_exp_1(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S30, float3  _S31)
{
    _d_exp_vector_0(_S30, _S31);
    return;
}

inline __device__ void s_bwd_prop_max_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S32, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S33, float3  _S34)
{
    _d_max_vector_0(_S32, _S33, _S34);
    return;
}

struct DiffPair_vectorx3Cfloatx2C4x3E_0
{
    float4  primal_0;
    float4  differential_0;
};

inline __device__ void s_bwd_prop_sqrt_0(DiffPair_float_0 * _S35, float _S36)
{
    _d_sqrt_0(_S35, _S36);
    return;
}

inline __device__ void s_bwd_prop_length_impl_0(DiffPair_vectorx3Cfloatx2C4x3E_0 * dpx_7, float _s_dOut_0)
{
    float _S37 = (*dpx_7).primal_0.x;
    float _S38 = (*dpx_7).primal_0.y;
    float _S39 = (*dpx_7).primal_0.z;
    float _S40 = (*dpx_7).primal_0.w;
    DiffPair_float_0 _S41;
    (&_S41)->primal_0 = _S37 * _S37 + _S38 * _S38 + _S39 * _S39 + _S40 * _S40;
    (&_S41)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S41, _s_dOut_0);
    float _S42 = (*dpx_7).primal_0.w * _S41.differential_0;
    float _S43 = _S42 + _S42;
    float _S44 = (*dpx_7).primal_0.z * _S41.differential_0;
    float _S45 = _S44 + _S44;
    float _S46 = (*dpx_7).primal_0.y * _S41.differential_0;
    float _S47 = _S46 + _S46;
    float _S48 = (*dpx_7).primal_0.x * _S41.differential_0;
    float _S49 = _S48 + _S48;
    float4  _S50 = make_float4 (0.0f);
    *&((&_S50)->w) = _S43;
    *&((&_S50)->z) = _S45;
    *&((&_S50)->y) = _S47;
    *&((&_S50)->x) = _S49;
    dpx_7->primal_0 = (*dpx_7).primal_0;
    dpx_7->differential_0 = _S50;
    return;
}

inline __device__ void s_bwd_length_impl_0(DiffPair_vectorx3Cfloatx2C4x3E_0 * _S51, float _S52)
{
    s_bwd_prop_length_impl_0(_S51, _S52);
    return;
}

inline __device__ void per_splat_losses_bwd(float3  scales_1, float opacity_1, float4  quat_1, FixedArray<float, 5>  v_loss_0, float3  * v_scales_0, float * v_opacity_0, float4  * v_quat_0, float mcmc_opacity_reg_weight_1, float mcmc_scale_reg_weight_1, float max_gauss_ratio_1, float scale_regularization_weight_1, float erank_reg_weight_1, float erank_reg_weight_s3_1, float quat_norm_reg_weight_1)
{
    float3  _S53 = make_float3 (-10000.0f);
    float3  _S54 = s_primal_ctx_max_0(scales_1, _S53);
    float _S55 = - opacity_1;
    float _S56 = 1.0f + s_primal_ctx_exp_0(_S55);
    float reg_0 = 1.0f / _S56;
    float _S57 = _S56 * _S56;
    float _S58 = 0.5f + 0.25f * opacity_1;
    float _S59 = length_0(quat_1);
    float3  _S60 = make_float3 (-40.0f);
    float _S61 = mcmc_scale_reg_weight_1 * 0.00999999977648258f;
    float _S62 = _S54.x;
    float _S63 = _S54.y;
    float _S64 = (F32_max((_S62), (_S63)));
    float _S65 = _S54.z;
    float _S66 = (F32_max((_S64), (_S65)));
    float _S67 = (F32_min((_S62), (_S63)));
    float _S68 = _S66 - (F32_min((_S67), (_S65)));
    float _S69 = (F32_min((_S68), (80.0f)));
    float _S70 = s_primal_ctx_exp_0(_S69);
    float3  _S71 = make_float3 (2.0f) * (_S54 - make_float3 (_S66));
    float3  _S72 = s_primal_ctx_exp_1(_S71);
    float x_5 = _S72.x;
    float y_3 = _S72.y;
    float z_1 = _S72.z;
    float s_1 = x_5 + y_3 + z_1;
    float _S73 = (F32_max((x_5), (y_3)));
    float _S74 = (F32_max((_S73), (z_1)));
    float s1_1 = _S74 / s_1;
    float _S75 = s_1 * s_1;
    float _S76 = (F32_min((x_5), (y_3)));
    float _S77 = (F32_min((_S76), (z_1)));
    float _S78 = _S77 / s_1;
    float _S79 = (F32_max((_S78), (1.00000000317107685e-30f)));
    float _S80 = 1.0f - s1_1 - _S79;
    float _S81 = (F32_max((_S80), (1.00000000317107685e-30f)));
    float _S82 = - s1_1;
    float _S83 = s_primal_ctx_log_0(s1_1);
    float _S84 = s_primal_ctx_log_0(_S81);
    float _S85 = s_primal_ctx_log_0(_S79);
    float _S86 = _S82 * _S83 - _S81 * _S84 - _S79 * _S85;
    float _S87 = s_primal_ctx_exp_0(_S86) - 0.99998998641967773f;
    float _S88 = erank_reg_weight_s3_1 * v_loss_0[int(3)];
    float _S89 = erank_reg_weight_1 * v_loss_0[int(3)];
    DiffPair_float_0 _S90;
    (&_S90)->primal_0 = - s_primal_ctx_log_0(_S87);
    (&_S90)->differential_0 = 0.0f;
    DiffPair_float_0 _S91;
    (&_S91)->primal_0 = 0.0f;
    (&_S91)->differential_0 = 0.0f;
    _d_max_0(&_S90, &_S91, _S89);
    float _S92 = - _S90.differential_0;
    DiffPair_float_0 _S93;
    (&_S93)->primal_0 = _S87;
    (&_S93)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S93, _S92);
    DiffPair_float_0 _S94;
    (&_S94)->primal_0 = _S86;
    (&_S94)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S94, _S93.differential_0);
    float _S95 = - _S94.differential_0;
    float _S96 = _S79 * _S95;
    float _S97 = _S85 * _S95;
    DiffPair_float_0 _S98;
    (&_S98)->primal_0 = _S79;
    (&_S98)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S98, _S96);
    float _S99 = _S81 * _S95;
    float _S100 = _S84 * _S95;
    DiffPair_float_0 _S101;
    (&_S101)->primal_0 = _S81;
    (&_S101)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S101, _S99);
    float _S102 = _S82 * _S94.differential_0;
    float _S103 = _S83 * _S94.differential_0;
    DiffPair_float_0 _S104;
    (&_S104)->primal_0 = s1_1;
    (&_S104)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S104, _S102);
    float _S105 = - _S103;
    float _S106 = _S100 + _S101.differential_0;
    DiffPair_float_0 _S107;
    (&_S107)->primal_0 = _S80;
    (&_S107)->differential_0 = 0.0f;
    DiffPair_float_0 _S108;
    (&_S108)->primal_0 = 1.00000000317107685e-30f;
    (&_S108)->differential_0 = 0.0f;
    _d_max_0(&_S107, &_S108, _S106);
    float _S109 = - _S107.differential_0;
    float _S110 = _S88 + _S97 + _S98.differential_0 + _S109;
    DiffPair_float_0 _S111;
    (&_S111)->primal_0 = _S78;
    (&_S111)->differential_0 = 0.0f;
    DiffPair_float_0 _S112;
    (&_S112)->primal_0 = 1.00000000317107685e-30f;
    (&_S112)->differential_0 = 0.0f;
    _d_max_0(&_S111, &_S112, _S110);
    float _S113 = _S111.differential_0 / _S75;
    float _S114 = _S77 * - _S113;
    float _S115 = s_1 * _S113;
    DiffPair_float_0 _S116;
    (&_S116)->primal_0 = _S76;
    (&_S116)->differential_0 = 0.0f;
    DiffPair_float_0 _S117;
    (&_S117)->primal_0 = z_1;
    (&_S117)->differential_0 = 0.0f;
    _d_min_0(&_S116, &_S117, _S115);
    DiffPair_float_0 _S118;
    (&_S118)->primal_0 = x_5;
    (&_S118)->differential_0 = 0.0f;
    DiffPair_float_0 _S119;
    (&_S119)->primal_0 = y_3;
    (&_S119)->differential_0 = 0.0f;
    _d_min_0(&_S118, &_S119, _S116.differential_0);
    float _S120 = (_S104.differential_0 + _S105 + _S109) / _S75;
    float _S121 = _S74 * - _S120;
    float _S122 = s_1 * _S120;
    DiffPair_float_0 _S123;
    (&_S123)->primal_0 = _S73;
    (&_S123)->differential_0 = 0.0f;
    DiffPair_float_0 _S124;
    (&_S124)->primal_0 = z_1;
    (&_S124)->differential_0 = 0.0f;
    _d_max_0(&_S123, &_S124, _S122);
    DiffPair_float_0 _S125;
    (&_S125)->primal_0 = x_5;
    (&_S125)->differential_0 = 0.0f;
    DiffPair_float_0 _S126;
    (&_S126)->primal_0 = y_3;
    (&_S126)->differential_0 = 0.0f;
    _d_max_0(&_S125, &_S126, _S123.differential_0);
    float _S127 = _S114 + _S121;
    float3  _S128 = make_float3 (_S118.differential_0 + _S125.differential_0 + _S127, _S119.differential_0 + _S126.differential_0 + _S127, _S117.differential_0 + _S124.differential_0 + _S127);
    float3  _S129 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S130;
    (&_S130)->primal_0 = _S71;
    (&_S130)->differential_0 = _S129;
    s_bwd_prop_exp_1(&_S130, _S128);
    float3  _S131 = make_float3 (2.0f) * _S130.differential_0;
    float3  _S132 = - _S131;
    float s_diff_scale_reg_T_0 = scale_regularization_weight_1 * v_loss_0[int(2)];
    DiffPair_float_0 _S133;
    (&_S133)->primal_0 = _S70;
    (&_S133)->differential_0 = 0.0f;
    DiffPair_float_0 _S134;
    (&_S134)->primal_0 = max_gauss_ratio_1;
    (&_S134)->differential_0 = 0.0f;
    _d_max_0(&_S133, &_S134, s_diff_scale_reg_T_0);
    DiffPair_float_0 _S135;
    (&_S135)->primal_0 = _S69;
    (&_S135)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S135, _S133.differential_0);
    DiffPair_float_0 _S136;
    (&_S136)->primal_0 = _S68;
    (&_S136)->differential_0 = 0.0f;
    DiffPair_float_0 _S137;
    (&_S137)->primal_0 = 80.0f;
    (&_S137)->differential_0 = 0.0f;
    _d_min_0(&_S136, &_S137, _S135.differential_0);
    float _S138 = - _S136.differential_0;
    DiffPair_float_0 _S139;
    (&_S139)->primal_0 = _S67;
    (&_S139)->differential_0 = 0.0f;
    DiffPair_float_0 _S140;
    (&_S140)->primal_0 = _S65;
    (&_S140)->differential_0 = 0.0f;
    _d_min_0(&_S139, &_S140, _S138);
    DiffPair_float_0 _S141;
    (&_S141)->primal_0 = _S62;
    (&_S141)->differential_0 = 0.0f;
    DiffPair_float_0 _S142;
    (&_S142)->primal_0 = _S63;
    (&_S142)->differential_0 = 0.0f;
    _d_min_0(&_S141, &_S142, _S139.differential_0);
    float _S143 = _S136.differential_0 + _S132.x + _S132.y + _S132.z;
    DiffPair_float_0 _S144;
    (&_S144)->primal_0 = _S64;
    (&_S144)->differential_0 = 0.0f;
    DiffPair_float_0 _S145;
    (&_S145)->primal_0 = _S65;
    (&_S145)->differential_0 = 0.0f;
    _d_max_0(&_S144, &_S145, _S143);
    float _S146 = _S140.differential_0 + _S145.differential_0;
    DiffPair_float_0 _S147;
    (&_S147)->primal_0 = _S62;
    (&_S147)->differential_0 = 0.0f;
    DiffPair_float_0 _S148;
    (&_S148)->primal_0 = _S63;
    (&_S148)->differential_0 = 0.0f;
    _d_max_0(&_S147, &_S148, _S144.differential_0);
    float _S149 = _S142.differential_0 + _S148.differential_0;
    float _S150 = _S141.differential_0 + _S147.differential_0;
    float _S151 = _S61 * (0.3333333432674408f * v_loss_0[int(1)]);
    float3  _S152 = make_float3 (_S151, _S151, _S151);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S153;
    (&_S153)->primal_0 = _S54;
    (&_S153)->differential_0 = _S129;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S154;
    (&_S154)->primal_0 = _S60;
    (&_S154)->differential_0 = _S129;
    s_bwd_prop_max_0(&_S153, &_S154, _S152);
    float s_diff_quat_norm_reg_T_0 = quat_norm_reg_weight_1 * v_loss_0[int(4)];
    float _S155 = - s_diff_quat_norm_reg_T_0;
    DiffPair_float_0 _S156;
    (&_S156)->primal_0 = _S59;
    (&_S156)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S156, _S155);
    float _S157 = _S156.differential_0 + s_diff_quat_norm_reg_T_0;
    float4  _S158 = make_float4 (0.0f);
    DiffPair_vectorx3Cfloatx2C4x3E_0 _S159;
    (&_S159)->primal_0 = quat_1;
    (&_S159)->differential_0 = _S158;
    s_bwd_length_impl_0(&_S159, _S157);
    float _S160 = mcmc_opacity_reg_weight_1 * v_loss_0[int(0)];
    DiffPair_float_0 _S161;
    (&_S161)->primal_0 = reg_0;
    (&_S161)->differential_0 = 0.0f;
    DiffPair_float_0 _S162;
    (&_S162)->primal_0 = _S58;
    (&_S162)->differential_0 = 0.0f;
    _d_max_0(&_S161, &_S162, _S160);
    float _S163 = 0.25f * _S162.differential_0;
    float _S164 = - (_S161.differential_0 / _S57);
    DiffPair_float_0 _S165;
    (&_S165)->primal_0 = _S55;
    (&_S165)->differential_0 = 0.0f;
    s_bwd_prop_exp_0(&_S165, _S164);
    float _S166 = - _S165.differential_0;
    float3  _S167 = _S131 + _S153.differential_0 + make_float3 (_S150, _S149, _S146);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S168;
    (&_S168)->primal_0 = scales_1;
    (&_S168)->differential_0 = _S129;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S169;
    (&_S169)->primal_0 = _S53;
    (&_S169)->differential_0 = _S129;
    s_bwd_prop_max_0(&_S168, &_S169, _S167);
    float _S170 = _S163 + _S166;
    *v_scales_0 = _S168.differential_0;
    *v_opacity_0 = _S170;
    *v_quat_0 = _S159.differential_0;
    return;
}

