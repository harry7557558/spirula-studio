#pragma once

#include "generated/slang.cuh"

struct DiffPair_vectorx3Cfloatx2C3x3E_0
{
    float3  primal_0;
    float3  differential_0;
};

inline __device__ void _d_dot_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpy_0, float dOut_0)
{
    float3  x_d_result_0;
    *&((&x_d_result_0)->x) = (*dpy_0).primal_0.x * dOut_0;
    float3  y_d_result_0;
    *&((&y_d_result_0)->x) = (*dpx_0).primal_0.x * dOut_0;
    *&((&x_d_result_0)->y) = (*dpy_0).primal_0.y * dOut_0;
    *&((&y_d_result_0)->y) = (*dpx_0).primal_0.y * dOut_0;
    *&((&x_d_result_0)->z) = (*dpy_0).primal_0.z * dOut_0;
    *&((&y_d_result_0)->z) = (*dpx_0).primal_0.z * dOut_0;
    dpx_0->primal_0 = (*dpx_0).primal_0;
    dpx_0->differential_0 = x_d_result_0;
    dpy_0->primal_0 = (*dpy_0).primal_0;
    dpy_0->differential_0 = y_d_result_0;
    return;
}

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

struct DiffPair_float_0
{
    float primal_0;
    float differential_0;
};

inline __device__ void _d_abs_0(DiffPair_float_0 * dpx_1, float dOut_1)
{
    float _S1 = _slang_select(((*dpx_1).primal_0) > 0.0f, 1.0f,_slang_select(((*dpx_1).primal_0) == 0.0f, 0.0f,-1.0f)) * dOut_1;
    dpx_1->primal_0 = (*dpx_1).primal_0;
    dpx_1->differential_0 = _S1;
    return;
}

inline __device__ void _d_abs_vector_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_2, float3  dOut_2)
{
    float3  _S2 = _slang_select(((*dpx_2).primal_0) > make_float3 (0.0f), make_float3 (1.0f),_slang_select(((*dpx_2).primal_0) == make_float3 (0.0f), make_float3 (0.0f),make_float3 (-1.0f))) * dOut_2;
    dpx_2->primal_0 = (*dpx_2).primal_0;
    dpx_2->differential_0 = _S2;
    return;
}

inline __device__ float3  abs_0(float3  x_1)
{
    float3  result_2;
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
        *_slang_vector_get_element_ptr(&result_2, i_1) = (F32_abs((_slang_vector_get_element(x_1, i_1))));
        i_1 = i_1 + int(1);
    }
    return result_2;
}

inline __device__ void _d_max_0(DiffPair_float_0 * dpx_3, DiffPair_float_0 * dpy_1, float dOut_3)
{
    DiffPair_float_0 _S3 = *dpx_3;
    float _S4;
    if(((*dpx_3).primal_0) > ((*dpy_1).primal_0))
    {
        _S4 = dOut_3;
    }
    else
    {
        if(((*dpx_3).primal_0) < ((*dpy_1).primal_0))
        {
            _S4 = 0.0f;
        }
        else
        {
            _S4 = 0.5f * dOut_3;
        }
    }
    dpx_3->primal_0 = _S3.primal_0;
    dpx_3->differential_0 = _S4;
    DiffPair_float_0 _S5 = *dpy_1;
    if(((*dpy_1).primal_0) > (_S3.primal_0))
    {
        _S4 = dOut_3;
    }
    else
    {
        if(((*dpy_1).primal_0) < ((*dpx_3).primal_0))
        {
            _S4 = 0.0f;
        }
        else
        {
            _S4 = 0.5f * dOut_3;
        }
    }
    dpy_1->primal_0 = _S5.primal_0;
    dpy_1->differential_0 = _S4;
    return;
}

inline __device__ void _d_clamp_0(DiffPair_float_0 * dpx_4, DiffPair_float_0 * dpMin_0, DiffPair_float_0 * dpMax_0, float dOut_4)
{
    DiffPair_float_0 _S6 = *dpx_4;
    bool _S7;
    if(((*dpx_4).primal_0) >= ((*dpMin_0).primal_0))
    {
        _S7 = ((*dpx_4).primal_0) <= ((*dpMax_0).primal_0);
    }
    else
    {
        _S7 = false;
    }
    float _S8;
    if(_S7)
    {
        _S8 = dOut_4;
    }
    else
    {
        _S8 = 0.0f;
    }
    dpx_4->primal_0 = _S6.primal_0;
    dpx_4->differential_0 = _S8;
    DiffPair_float_0 _S9 = *dpMin_0;
    if((_S6.primal_0) < ((*dpMin_0).primal_0))
    {
        _S8 = dOut_4;
    }
    else
    {
        _S8 = 0.0f;
    }
    dpMin_0->primal_0 = _S9.primal_0;
    dpMin_0->differential_0 = _S8;
    DiffPair_float_0 _S10 = *dpMax_0;
    if(((*dpx_4).primal_0) > ((*dpMax_0).primal_0))
    {
        _S8 = dOut_4;
    }
    else
    {
        _S8 = 0.0f;
    }
    dpMax_0->primal_0 = _S10.primal_0;
    dpMax_0->differential_0 = _S8;
    return;
}

inline __device__ float clamp_0(float x_2, float minBound_0, float maxBound_0)
{
    return (F32_min(((F32_max((x_2), (minBound_0)))), (maxBound_0)));
}

inline __device__ void _d_sqrt_0(DiffPair_float_0 * dpx_5, float dOut_5)
{
    float _S11 = 0.5f / (F32_sqrt(((F32_max((1.00000001168609742e-07f), ((*dpx_5).primal_0)))))) * dOut_5;
    dpx_5->primal_0 = (*dpx_5).primal_0;
    dpx_5->differential_0 = _S11;
    return;
}

inline __device__ void _d_rsqrt_0(DiffPair_float_0 * dpx_6, float dOut_6)
{
    float _S12 = -0.5f / ((*dpx_6).primal_0 * (F32_sqrt(((*dpx_6).primal_0)))) * dOut_6;
    dpx_6->primal_0 = (*dpx_6).primal_0;
    dpx_6->differential_0 = _S12;
    return;
}

inline __device__ void _d_log_0(DiffPair_float_0 * dpx_7, float dOut_7)
{
    float _S13 = 1.0f / (*dpx_7).primal_0 * dOut_7;
    dpx_7->primal_0 = (*dpx_7).primal_0;
    dpx_7->differential_0 = _S13;
    return;
}

inline __device__ void _d_lerp_0(DiffPair_float_0 * dpx_8, DiffPair_float_0 * dpy_2, DiffPair_float_0 * dps_0, float dOut_8)
{
    float _S14 = (1.0f - (*dps_0).primal_0) * dOut_8;
    dpx_8->primal_0 = (*dpx_8).primal_0;
    dpx_8->differential_0 = _S14;
    DiffPair_float_0 _S15 = *dpy_2;
    float _S16 = (*dps_0).primal_0 * dOut_8;
    dpy_2->primal_0 = (*dpy_2).primal_0;
    dpy_2->differential_0 = _S16;
    float _S17 = (_S15.primal_0 - (*dpx_8).primal_0) * dOut_8;
    dps_0->primal_0 = _S15.primal_0;
    dps_0->differential_0 = _S17;
    return;
}

inline __device__ float lerp_0(float x_3, float y_1, float s_0)
{
    return x_3 + (y_1 - x_3) * s_0;
}

inline __device__ void per_pixel_losses(float3  render_rgb_0, float3  ref_rgb_0, float render_depth_0, float ref_depth_0, float3  render_normal_0, float3  depth_normal_0, float3  ref_normal_0, float render_Ts_0, float3  rgb_dist_0, float depth_dist_0, float3  normal_dist_0, float median_depth_0, float3  median_normal_0, bool ref_alpha_0, bool has_mask_0, FixedArray<float, 19>  weights_0, FixedArray<float, 32>  * _S18)
{
    bool _S19;
    bool _S20;
    bool _S21;
    FixedArray<float, 32>  losses_0;
    bool mask_0;
    if(has_mask_0)
    {
        mask_0 = ref_alpha_0;
    }
    else
    {
        mask_0 = true;
    }
    bool depth_mask_0 = ref_depth_0 != 0.0f;
    bool normal_mask_0;
    if((ref_normal_0.x + ref_normal_0.y + ref_normal_0.z) > -2.36599993705749512f)
    {
        normal_mask_0 = (dot_0(ref_normal_0, ref_normal_0)) > 0.25f;
    }
    else
    {
        normal_mask_0 = false;
    }
    float3  _S22;
    float _S23 = render_rgb_0.x;
    float _S24 = render_rgb_0.y;
    float _S25 = render_rgb_0.z;
    float _S26 = ref_rgb_0.x;
    float _S27 = ref_rgb_0.y;
    float _S28 = ref_rgb_0.z;
    float dY_0 = 0.29899999499320984f * _S23 + 0.58700001239776611f * _S24 + 0.11400000005960464f * _S25 - (0.29899999499320984f * _S26 + 0.58700001239776611f * _S27 + 0.11400000005960464f * _S28);
    float dU_0 = -0.14712999761104584f * _S23 - 0.28885999321937561f * _S24 + 0.43599998950958252f * _S25 - (-0.14712999761104584f * _S26 - 0.28885999321937561f * _S27 + 0.43599998950958252f * _S28);
    float dV_0 = 0.61500000953674316f * _S23 - 0.51498997211456299f * _S24 - 0.10001000016927719f * _S25 - (0.61500000953674316f * _S26 - 0.51498997211456299f * _S27 - 0.10001000016927719f * _S28);
    float _S29 = float(mask_0);
    float3  _S30 = ref_rgb_0 - render_rgb_0;
    float3  _S31 = abs_0(_S30);
    float _S32 = dot_0(_S30, _S30) * 0.3333333432674408f;
    losses_0[int(0)] = _S29 * (weights_0[int(0)] * ((_S31.x + _S31.y + _S31.z) * 0.3333333432674408f) + weights_0[int(1)] * _S32 + weights_0[int(2)] * (F32_abs((dY_0))) + weights_0[int(3)] * dY_0 * dY_0 + weights_0[int(4)] * dU_0 * dU_0 + weights_0[int(5)] * dV_0 * dV_0);
    losses_0[int(1)] = _S29 * clamp_0(_S32, 0.0f, 1.0f);
    float _S33 = float(depth_mask_0 & mask_0);
    float _S34 = _S33 * (F32_max((render_depth_0), (0.00009999999747379f)));
    float _S35 = _S33 * (F32_max((ref_depth_0), (0.00009999999747379f)));
    losses_0[int(2)] = _S34;
    losses_0[int(3)] = _S35;
    losses_0[int(4)] = _S34 * _S34;
    losses_0[int(5)] = _S35 * _S35;
    losses_0[int(6)] = _S34 * _S35;
    bool _S36 = normal_mask_0 & mask_0;
    for(;;)
    {
        float norm2_0 = dot_0(render_normal_0, render_normal_0);
        bool _S37 = norm2_0 == 0.0f;
        _S19 = _S37;
        if(_S37)
        {
            _S22 = make_float3 (0.0f);
            break;
        }
        _S22 = render_normal_0 * make_float3 ((F32_rsqrt((norm2_0))));
        break;
    }
    float3  _S38;
    bool _S39 = !_S19;
    for(;;)
    {
        float norm2_1 = dot_0(depth_normal_0, depth_normal_0);
        bool _S40 = norm2_1 == 0.0f;
        _S20 = _S40;
        if(_S40)
        {
            _S38 = make_float3 (0.0f);
            break;
        }
        _S38 = depth_normal_0 * make_float3 ((F32_rsqrt((norm2_1))));
        break;
    }
    float3  _S41;
    bool _S42 = !_S20;
    for(;;)
    {
        float norm2_2 = dot_0(ref_normal_0, ref_normal_0);
        if(norm2_2 == 0.0f)
        {
            _S41 = make_float3 (0.0f);
            normal_mask_0 = false;
            break;
        }
        _S41 = ref_normal_0 * make_float3 ((F32_rsqrt((norm2_2))));
        normal_mask_0 = _S36;
        break;
    }
    float3  _S43;
    float _S44 = float(_S39 & normal_mask_0);
    float cos_sim_loss_0 = 0.5f - 0.5f * dot_0(_S22, _S41);
    losses_0[int(7)] = weights_0[int(7)] * _S44 * (cos_sim_loss_0 + (F32_sqrt(((F32_max((cos_sim_loss_0), (9.999999960041972e-13f)))))));
    float _S45 = float(_S42 & normal_mask_0);
    float cos_sim_loss_1 = 0.5f - 0.5f * dot_0(_S38, _S41);
    losses_0[int(8)] = weights_0[int(7)] * _S45 * (cos_sim_loss_1 + (F32_sqrt(((F32_max((cos_sim_loss_1), (9.999999960041972e-13f)))))));
    float _S46 = float((_S39 & _S42) & mask_0);
    float cos_sim_loss_2 = 0.5f - 0.5f * dot_0(_S22, _S38);
    losses_0[int(11)] = weights_0[int(10)] * _S46 * (cos_sim_loss_2 + (F32_sqrt(((F32_max((cos_sim_loss_2), (9.999999960041972e-13f)))))));
    for(;;)
    {
        float norm2_3 = dot_0(median_normal_0, median_normal_0);
        bool _S47 = norm2_3 == 0.0f;
        _S21 = _S47;
        if(_S47)
        {
            _S43 = make_float3 (0.0f);
            break;
        }
        _S43 = median_normal_0 * make_float3 ((F32_rsqrt((norm2_3))));
        break;
    }
    bool _S48 = !_S21;
    bool mean_median_mask_0;
    if(mask_0)
    {
        mean_median_mask_0 = render_depth_0 > 1.00000001335143196e-10f;
    }
    else
    {
        mean_median_mask_0 = false;
    }
    if(mean_median_mask_0)
    {
        mean_median_mask_0 = median_depth_0 > 1.00000001335143196e-10f;
    }
    else
    {
        mean_median_mask_0 = false;
    }
    float _S49 = float(mean_median_mask_0);
    losses_0[int(16)] = weights_0[int(15)] * _S49 * (F32_abs(((F32_log(((F32_max((render_depth_0), (1.00000001335143196e-10f)))))) - (F32_log(((F32_max((median_depth_0), (1.00000001335143196e-10f)))))))));
    float _S50 = float((_S48 & _S42) & mask_0);
    float cos_sim_loss_3 = 0.5f - 0.5f * dot_0(_S43, _S38);
    losses_0[int(17)] = weights_0[int(16)] * _S50 * (cos_sim_loss_3 + (F32_sqrt(((F32_max((cos_sim_loss_3), (9.999999960041972e-13f)))))));
    float _S51 = float(_S48 & normal_mask_0);
    float cos_sim_loss_4 = 0.5f - 0.5f * dot_0(_S43, _S41);
    losses_0[int(18)] = weights_0[int(17)] * _S51 * (cos_sim_loss_4 + (F32_sqrt(((F32_max((cos_sim_loss_4), (9.999999960041972e-13f)))))));
    float _S52 = float((_S48 & _S39) & mask_0);
    float cos_sim_loss_5 = 0.5f - 0.5f * dot_0(_S43, _S22);
    losses_0[int(19)] = weights_0[int(18)] * _S52 * (cos_sim_loss_5 + (F32_sqrt(((F32_max((cos_sim_loss_5), (9.999999960041972e-13f)))))));
    float render_alpha_0 = clamp_0(1.0f - render_Ts_0, 0.0f, 1.0f);
    float _S53 = float(has_mask_0);
    float _S54 = float(ref_alpha_0);
    float _S55 = (F32_max((render_alpha_0), (_S54)));
    losses_0[int(9)] = weights_0[int(8)] * _S53 * - lerp_0((F32_log(((F32_max((1.0f - _S55), (9.99999997475242708e-07f)))))), (F32_log(((F32_max((_S55), (9.99999997475242708e-07f)))))), _S54);
    float _S56 = 1.0f - render_alpha_0;
    float _S57 = 1.0f - _S54;
    float _S58 = (F32_max((_S56), (_S57)));
    losses_0[int(10)] = weights_0[int(9)] * _S53 * - lerp_0((F32_log(((F32_max((1.0f - _S58), (9.99999997475242708e-07f)))))), (F32_log(((F32_max((_S58), (9.99999997475242708e-07f)))))), _S57);
    losses_0[int(12)] = weights_0[int(11)] * _S29 * 4.0f * render_alpha_0 * _S56;
    losses_0[int(13)] = weights_0[int(12)] * _S29 * ((rgb_dist_0.x + rgb_dist_0.y + rgb_dist_0.z) * 0.3333333432674408f);
    losses_0[int(14)] = weights_0[int(13)] * _S29 * depth_dist_0;
    losses_0[int(15)] = weights_0[int(14)] * _S29 * ((normal_dist_0.x + normal_dist_0.y + normal_dist_0.z) * 0.3333333432674408f);
    losses_0[int(20)] = 1.0f;
    losses_0[int(21)] = _S29;
    losses_0[int(22)] = _S33;
    losses_0[int(23)] = _S44;
    losses_0[int(24)] = _S45;
    losses_0[int(25)] = _S46;
    if(has_mask_0)
    {
        mask_0 = !ref_alpha_0;
    }
    else
    {
        mask_0 = false;
    }
    losses_0[int(26)] = float(mask_0);
    if(has_mask_0)
    {
        mask_0 = ref_alpha_0;
    }
    else
    {
        mask_0 = false;
    }
    losses_0[int(27)] = float(mask_0);
    losses_0[int(28)] = _S49;
    losses_0[int(29)] = _S50;
    losses_0[int(30)] = _S51;
    losses_0[int(31)] = _S52;
    *_S18 = losses_0;
    return;
}

inline __device__ float s_primal_ctx_dot_0(float3  _S59, float3  _S60)
{
    return dot_0(_S59, _S60);
}

inline __device__ float s_primal_ctx_rsqrt_0(float _S61)
{
    return (F32_rsqrt((_S61)));
}

inline __device__ float s_primal_ctx_log_0(float _S62)
{
    return (F32_log((_S62)));
}

inline __device__ float s_primal_ctx_clamp_0(float _S63, float _S64, float _S65)
{
    return clamp_0(_S63, _S64, _S65);
}

inline __device__ void s_bwd_prop_lerp_0(DiffPair_float_0 * _S66, DiffPair_float_0 * _S67, DiffPair_float_0 * _S68, float _S69)
{
    _d_lerp_0(_S66, _S67, _S68, _S69);
    return;
}

inline __device__ void s_bwd_prop_log_0(DiffPair_float_0 * _S70, float _S71)
{
    _d_log_0(_S70, _S71);
    return;
}

inline __device__ void s_bwd_prop_clamp_0(DiffPair_float_0 * _S72, DiffPair_float_0 * _S73, DiffPair_float_0 * _S74, float _S75)
{
    _d_clamp_0(_S72, _S73, _S74, _S75);
    return;
}

inline __device__ void s_bwd_prop_sqrt_0(DiffPair_float_0 * _S76, float _S77)
{
    _d_sqrt_0(_S76, _S77);
    return;
}

inline __device__ void s_bwd_prop_dot_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S78, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S79, float _S80)
{
    _d_dot_0(_S78, _S79, _S80);
    return;
}

inline __device__ void s_bwd_prop_abs_0(DiffPair_float_0 * _S81, float _S82)
{
    _d_abs_0(_S81, _S82);
    return;
}

inline __device__ void s_bwd_prop_rsqrt_0(DiffPair_float_0 * _S83, float _S84)
{
    _d_rsqrt_0(_S83, _S84);
    return;
}

inline __device__ void s_bwd_prop_abs_1(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S85, float3  _S86)
{
    _d_abs_vector_0(_S85, _S86);
    return;
}

inline __device__ void s_bwd_prop_per_pixel_losses_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dprender_rgb_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpref_rgb_0, DiffPair_float_0 * dprender_depth_0, DiffPair_float_0 * dpref_depth_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dprender_normal_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpdepth_normal_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpref_normal_0, DiffPair_float_0 * dprender_Ts_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dprgb_dist_0, DiffPair_float_0 * dpdepth_dist_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpnormal_dist_0, DiffPair_float_0 * dpmedian_depth_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpmedian_normal_0, bool ref_alpha_1, bool has_mask_1, FixedArray<float, 19>  * weights_1, FixedArray<float, 32>  * _s_dOut_0)
{
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S87 = *dprender_rgb_0;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S88 = *dpref_rgb_0;
    DiffPair_float_0 _S89 = *dprender_depth_0;
    DiffPair_float_0 _S90 = *dpref_depth_0;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S91 = *dprender_normal_0;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S92 = *dpdepth_normal_0;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S93 = *dpref_normal_0;
    DiffPair_float_0 _S94 = *dprender_Ts_0;
    DiffPair_float_0 _S95 = *dpmedian_depth_0;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S96 = *dpmedian_normal_0;
    float3  _S97 = make_float3 (0.0f);
    bool mask_1;
    if(has_mask_1)
    {
        mask_1 = ref_alpha_1;
    }
    else
    {
        mask_1 = true;
    }
    bool depth_mask_1 = (_S90.primal_0) != 0.0f;
    bool _S98 = (_S93.primal_0.x + _S93.primal_0.y + _S93.primal_0.z) > -2.36599993705749512f;
    bool normal_mask_1;
    if(_S98)
    {
        normal_mask_1 = (s_primal_ctx_dot_0(_S93.primal_0, _S93.primal_0)) > 0.25f;
    }
    else
    {
        normal_mask_1 = false;
    }
    float _S99 = _S87.primal_0.x;
    float _S100 = _S87.primal_0.y;
    float _S101 = _S87.primal_0.z;
    float _S102 = _S88.primal_0.x;
    float _S103 = _S88.primal_0.y;
    float _S104 = _S88.primal_0.z;
    float dY_1 = 0.29899999499320984f * _S99 + 0.58700001239776611f * _S100 + 0.11400000005960464f * _S101 - (0.29899999499320984f * _S102 + 0.58700001239776611f * _S103 + 0.11400000005960464f * _S104);
    float dU_1 = -0.14712999761104584f * _S99 - 0.28885999321937561f * _S100 + 0.43599998950958252f * _S101 - (-0.14712999761104584f * _S102 - 0.28885999321937561f * _S103 + 0.43599998950958252f * _S104);
    float dV_1 = 0.61500000953674316f * _S99 - 0.51498997211456299f * _S100 - 0.10001000016927719f * _S101 - (0.61500000953674316f * _S102 - 0.51498997211456299f * _S103 - 0.10001000016927719f * _S104);
    float _S105 = float(mask_1);
    float _S106 = (*weights_1)[int(0)];
    float3  _S107 = _S88.primal_0 - _S87.primal_0;
    float _S108 = (*weights_1)[int(1)];
    float _S109 = s_primal_ctx_dot_0(_S107, _S107) * 0.3333333432674408f;
    float _S110 = (*weights_1)[int(2)];
    float _S111 = (*weights_1)[int(3)];
    float _S112 = (*weights_1)[int(3)] * dY_1;
    float _S113 = (*weights_1)[int(4)];
    float _S114 = (*weights_1)[int(4)] * dU_1;
    float _S115 = (*weights_1)[int(5)];
    float _S116 = (*weights_1)[int(5)] * dV_1;
    float _S117 = float(depth_mask_1 & mask_1);
    float _S118 = _S117 * (F32_max((_S89.primal_0), (0.00009999999747379f)));
    float _S119 = _S117 * (F32_max((_S90.primal_0), (0.00009999999747379f)));
    bool _S120 = normal_mask_1 & mask_1;
    float _S121 = s_primal_ctx_dot_0(_S91.primal_0, _S91.primal_0);
    bool _S122 = _S121 == 0.0f;
    float3  _S123;
    if(_S122)
    {
        _S123 = make_float3 (0.0f);
    }
    bool _S124 = !_S122;
    float3  _S125;
    if(_S124)
    {
        float _S126 = s_primal_ctx_rsqrt_0(_S121);
        float3  _S127 = make_float3 (_S126);
        _S123 = _S91.primal_0 * make_float3 (_S126);
        _S125 = _S127;
    }
    else
    {
        _S125 = _S97;
    }
    float _S128 = s_primal_ctx_dot_0(_S92.primal_0, _S92.primal_0);
    bool _S129 = _S128 == 0.0f;
    float3  _S130;
    if(_S129)
    {
        _S130 = make_float3 (0.0f);
    }
    bool _S131 = !_S129;
    float3  _S132;
    if(_S131)
    {
        float _S133 = s_primal_ctx_rsqrt_0(_S128);
        float3  _S134 = make_float3 (_S133);
        _S130 = _S92.primal_0 * make_float3 (_S133);
        _S132 = _S134;
    }
    else
    {
        _S132 = _S97;
    }
    float _S135 = s_primal_ctx_dot_0(_S93.primal_0, _S93.primal_0);
    bool _S136 = _S135 == 0.0f;
    float3  _S137;
    if(_S136)
    {
        float3  _S138 = make_float3 (0.0f);
        normal_mask_1 = false;
        _S137 = _S138;
    }
    else
    {
        normal_mask_1 = _S120;
    }
    bool _S139 = !_S136;
    float3  _S140;
    if(_S139)
    {
        float _S141 = s_primal_ctx_rsqrt_0(_S135);
        float3  _S142 = make_float3 (_S141);
        _S137 = _S93.primal_0 * make_float3 (_S141);
        _S140 = _S142;
    }
    else
    {
        _S140 = _S97;
    }
    float _S143 = (*weights_1)[int(7)] * float(_S124 & normal_mask_1);
    float cos_sim_loss_6 = 0.5f - 0.5f * s_primal_ctx_dot_0(_S123, _S137);
    float _S144 = (F32_max((cos_sim_loss_6), (9.999999960041972e-13f)));
    float _S145 = (*weights_1)[int(7)] * float(_S131 & normal_mask_1);
    float cos_sim_loss_7 = 0.5f - 0.5f * s_primal_ctx_dot_0(_S130, _S137);
    float _S146 = (F32_max((cos_sim_loss_7), (9.999999960041972e-13f)));
    float _S147 = (*weights_1)[int(10)] * float((_S124 & _S131) & mask_1);
    float cos_sim_loss_8 = 0.5f - 0.5f * s_primal_ctx_dot_0(_S123, _S130);
    float _S148 = (F32_max((cos_sim_loss_8), (9.999999960041972e-13f)));
    float _S149 = s_primal_ctx_dot_0(_S96.primal_0, _S96.primal_0);
    bool _S150 = _S149 == 0.0f;
    float3  _S151;
    if(_S150)
    {
        _S151 = make_float3 (0.0f);
    }
    bool _S152 = !_S150;
    float3  _S153;
    if(_S152)
    {
        float _S154 = s_primal_ctx_rsqrt_0(_S149);
        float3  _S155 = make_float3 (_S154);
        _S151 = _S96.primal_0 * make_float3 (_S154);
        _S153 = _S155;
    }
    else
    {
        _S153 = _S97;
    }
    bool mean_median_mask_1;
    if(mask_1)
    {
        mean_median_mask_1 = (_S89.primal_0) > 1.00000001335143196e-10f;
    }
    else
    {
        mean_median_mask_1 = false;
    }
    if(mean_median_mask_1)
    {
        mean_median_mask_1 = (_S95.primal_0) > 1.00000001335143196e-10f;
    }
    else
    {
        mean_median_mask_1 = false;
    }
    float _S156 = (*weights_1)[int(15)] * float(mean_median_mask_1);
    float _S157 = (F32_max((_S89.primal_0), (1.00000001335143196e-10f)));
    float _S158 = (F32_max((_S95.primal_0), (1.00000001335143196e-10f)));
    float _S159 = s_primal_ctx_log_0(_S157) - s_primal_ctx_log_0(_S158);
    float _S160 = (*weights_1)[int(16)] * float((_S152 & _S131) & mask_1);
    float cos_sim_loss_9 = 0.5f - 0.5f * s_primal_ctx_dot_0(_S151, _S130);
    float _S161 = (F32_max((cos_sim_loss_9), (9.999999960041972e-13f)));
    float _S162 = (*weights_1)[int(17)] * float(_S152 & normal_mask_1);
    float cos_sim_loss_10 = 0.5f - 0.5f * s_primal_ctx_dot_0(_S151, _S137);
    float _S163 = (F32_max((cos_sim_loss_10), (9.999999960041972e-13f)));
    float _S164 = (*weights_1)[int(18)] * float((_S152 & _S124) & mask_1);
    float cos_sim_loss_11 = 0.5f - 0.5f * s_primal_ctx_dot_0(_S151, _S123);
    float _S165 = (F32_max((cos_sim_loss_11), (9.999999960041972e-13f)));
    float _S166 = 1.0f - _S94.primal_0;
    float _S167 = s_primal_ctx_clamp_0(_S166, 0.0f, 1.0f);
    float _S168 = float(has_mask_1);
    float _S169 = (*weights_1)[int(8)] * _S168;
    float _S170 = float(ref_alpha_1);
    float _S171 = (F32_max((_S167), (_S170)));
    float _S172 = 1.0f - _S171;
    float _S173 = (F32_max((_S172), (9.99999997475242708e-07f)));
    float _S174 = s_primal_ctx_log_0(_S173);
    float _S175 = (F32_max((_S171), (9.99999997475242708e-07f)));
    float _S176 = s_primal_ctx_log_0(_S175);
    float _S177 = (*weights_1)[int(9)] * _S168;
    float _S178 = 1.0f - _S167;
    float _S179 = 1.0f - _S170;
    float _S180 = (F32_max((_S178), (_S179)));
    float _S181 = 1.0f - _S180;
    float _S182 = (F32_max((_S181), (9.99999997475242708e-07f)));
    float _S183 = s_primal_ctx_log_0(_S182);
    float _S184 = (F32_max((_S180), (9.99999997475242708e-07f)));
    float _S185 = s_primal_ctx_log_0(_S184);
    float _S186 = (*weights_1)[int(11)] * _S105 * 4.0f;
    float _S187 = _S186 * _S167;
    float _S188 = (*weights_1)[int(12)] * _S105;
    float _S189 = (*weights_1)[int(13)] * _S105;
    float _S190 = (*weights_1)[int(14)] * _S105;
    float _S191 = (*_s_dOut_0)[int(0)];
    float _S192 = (*_s_dOut_0)[int(1)];
    float _S193 = (*_s_dOut_0)[int(2)];
    float _S194 = (*_s_dOut_0)[int(3)];
    float _S195 = (*_s_dOut_0)[int(4)];
    float _S196 = (*_s_dOut_0)[int(5)];
    float _S197 = (*_s_dOut_0)[int(6)];
    float _S198 = (*_s_dOut_0)[int(7)];
    float _S199 = (*_s_dOut_0)[int(8)];
    float _S200 = (*_s_dOut_0)[int(11)];
    float _S201 = 0.3333333432674408f * (_S190 * (*_s_dOut_0)[int(15)]);
    float _S202 = _S189 * (*_s_dOut_0)[int(14)];
    float _S203 = 0.3333333432674408f * (_S188 * (*_s_dOut_0)[int(13)]);
    float _S204 = _S187 * (*_s_dOut_0)[int(12)];
    float _S205 = _S186 * (_S178 * (*_s_dOut_0)[int(12)]);
    float _S206 = - (_S177 * (*_s_dOut_0)[int(10)]);
    DiffPair_float_0 _S207;
    (&_S207)->primal_0 = _S183;
    (&_S207)->differential_0 = 0.0f;
    DiffPair_float_0 _S208;
    (&_S208)->primal_0 = _S185;
    (&_S208)->differential_0 = 0.0f;
    DiffPair_float_0 _S209;
    (&_S209)->primal_0 = _S179;
    (&_S209)->differential_0 = 0.0f;
    s_bwd_prop_lerp_0(&_S207, &_S208, &_S209, _S206);
    DiffPair_float_0 _S210;
    (&_S210)->primal_0 = _S184;
    (&_S210)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S210, _S208.differential_0);
    DiffPair_float_0 _S211;
    (&_S211)->primal_0 = _S180;
    (&_S211)->differential_0 = 0.0f;
    DiffPair_float_0 _S212;
    (&_S212)->primal_0 = 9.99999997475242708e-07f;
    (&_S212)->differential_0 = 0.0f;
    _d_max_0(&_S211, &_S212, _S210.differential_0);
    DiffPair_float_0 _S213;
    (&_S213)->primal_0 = _S182;
    (&_S213)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S213, _S207.differential_0);
    DiffPair_float_0 _S214;
    (&_S214)->primal_0 = _S181;
    (&_S214)->differential_0 = 0.0f;
    DiffPair_float_0 _S215;
    (&_S215)->primal_0 = 9.99999997475242708e-07f;
    (&_S215)->differential_0 = 0.0f;
    _d_max_0(&_S214, &_S215, _S213.differential_0);
    float _S216 = _S211.differential_0 + - _S214.differential_0;
    DiffPair_float_0 _S217;
    (&_S217)->primal_0 = _S178;
    (&_S217)->differential_0 = 0.0f;
    DiffPair_float_0 _S218;
    (&_S218)->primal_0 = _S179;
    (&_S218)->differential_0 = 0.0f;
    _d_max_0(&_S217, &_S218, _S216);
    float _S219 = - (_S204 + _S217.differential_0);
    float _S220 = - (_S169 * (*_s_dOut_0)[int(9)]);
    DiffPair_float_0 _S221;
    (&_S221)->primal_0 = _S174;
    (&_S221)->differential_0 = 0.0f;
    DiffPair_float_0 _S222;
    (&_S222)->primal_0 = _S176;
    (&_S222)->differential_0 = 0.0f;
    DiffPair_float_0 _S223;
    (&_S223)->primal_0 = _S170;
    (&_S223)->differential_0 = 0.0f;
    s_bwd_prop_lerp_0(&_S221, &_S222, &_S223, _S220);
    DiffPair_float_0 _S224;
    (&_S224)->primal_0 = _S175;
    (&_S224)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S224, _S222.differential_0);
    DiffPair_float_0 _S225;
    (&_S225)->primal_0 = _S171;
    (&_S225)->differential_0 = 0.0f;
    DiffPair_float_0 _S226;
    (&_S226)->primal_0 = 9.99999997475242708e-07f;
    (&_S226)->differential_0 = 0.0f;
    _d_max_0(&_S225, &_S226, _S224.differential_0);
    DiffPair_float_0 _S227;
    (&_S227)->primal_0 = _S173;
    (&_S227)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S227, _S221.differential_0);
    DiffPair_float_0 _S228;
    (&_S228)->primal_0 = _S172;
    (&_S228)->differential_0 = 0.0f;
    DiffPair_float_0 _S229;
    (&_S229)->primal_0 = 9.99999997475242708e-07f;
    (&_S229)->differential_0 = 0.0f;
    _d_max_0(&_S228, &_S229, _S227.differential_0);
    float _S230 = _S225.differential_0 + - _S228.differential_0;
    DiffPair_float_0 _S231;
    (&_S231)->primal_0 = _S167;
    (&_S231)->differential_0 = 0.0f;
    DiffPair_float_0 _S232;
    (&_S232)->primal_0 = _S170;
    (&_S232)->differential_0 = 0.0f;
    _d_max_0(&_S231, &_S232, _S230);
    float _S233 = _S205 + _S219 + _S231.differential_0;
    DiffPair_float_0 _S234;
    (&_S234)->primal_0 = _S166;
    (&_S234)->differential_0 = 0.0f;
    DiffPair_float_0 _S235;
    (&_S235)->primal_0 = 0.0f;
    (&_S235)->differential_0 = 0.0f;
    DiffPair_float_0 _S236;
    (&_S236)->primal_0 = 1.0f;
    (&_S236)->differential_0 = 0.0f;
    s_bwd_prop_clamp_0(&_S234, &_S235, &_S236, _S233);
    float _S237 = - _S234.differential_0;
    float _S238 = _S164 * (*_s_dOut_0)[int(19)];
    DiffPair_float_0 _S239;
    (&_S239)->primal_0 = _S165;
    (&_S239)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S239, _S238);
    DiffPair_float_0 _S240;
    (&_S240)->primal_0 = cos_sim_loss_11;
    (&_S240)->differential_0 = 0.0f;
    DiffPair_float_0 _S241;
    (&_S241)->primal_0 = 9.999999960041972e-13f;
    (&_S241)->differential_0 = 0.0f;
    _d_max_0(&_S240, &_S241, _S239.differential_0);
    float _S242 = 0.5f * - (_S238 + _S240.differential_0);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S243;
    (&_S243)->primal_0 = _S151;
    (&_S243)->differential_0 = _S97;
    float3  _S244 = _S123;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S245;
    (&_S245)->primal_0 = _S123;
    (&_S245)->differential_0 = _S97;
    s_bwd_prop_dot_0(&_S243, &_S245, _S242);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S246 = _S245;
    float _S247 = _S162 * (*_s_dOut_0)[int(18)];
    DiffPair_float_0 _S248;
    (&_S248)->primal_0 = _S163;
    (&_S248)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S248, _S247);
    DiffPair_float_0 _S249;
    (&_S249)->primal_0 = cos_sim_loss_10;
    (&_S249)->differential_0 = 0.0f;
    DiffPair_float_0 _S250;
    (&_S250)->primal_0 = 9.999999960041972e-13f;
    (&_S250)->differential_0 = 0.0f;
    _d_max_0(&_S249, &_S250, _S248.differential_0);
    float _S251 = 0.5f * - (_S247 + _S249.differential_0);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S252;
    (&_S252)->primal_0 = _S151;
    (&_S252)->differential_0 = _S97;
    float3  _S253 = _S137;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S254;
    (&_S254)->primal_0 = _S137;
    (&_S254)->differential_0 = _S97;
    s_bwd_prop_dot_0(&_S252, &_S254, _S251);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S255 = _S254;
    float _S256 = _S160 * (*_s_dOut_0)[int(17)];
    DiffPair_float_0 _S257;
    (&_S257)->primal_0 = _S161;
    (&_S257)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S257, _S256);
    DiffPair_float_0 _S258;
    (&_S258)->primal_0 = cos_sim_loss_9;
    (&_S258)->differential_0 = 0.0f;
    DiffPair_float_0 _S259;
    (&_S259)->primal_0 = 9.999999960041972e-13f;
    (&_S259)->differential_0 = 0.0f;
    _d_max_0(&_S258, &_S259, _S257.differential_0);
    float _S260 = 0.5f * - (_S256 + _S258.differential_0);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S261;
    (&_S261)->primal_0 = _S151;
    (&_S261)->differential_0 = _S97;
    float3  _S262 = _S130;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S263;
    (&_S263)->primal_0 = _S130;
    (&_S263)->differential_0 = _S97;
    s_bwd_prop_dot_0(&_S261, &_S263, _S260);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S264 = _S263;
    float _S265 = _S156 * (*_s_dOut_0)[int(16)];
    DiffPair_float_0 _S266;
    (&_S266)->primal_0 = _S159;
    (&_S266)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S266, _S265);
    float _S267 = - _S266.differential_0;
    DiffPair_float_0 _S268;
    (&_S268)->primal_0 = _S158;
    (&_S268)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S268, _S267);
    DiffPair_float_0 _S269;
    (&_S269)->primal_0 = _S95.primal_0;
    (&_S269)->differential_0 = 0.0f;
    DiffPair_float_0 _S270;
    (&_S270)->primal_0 = 1.00000001335143196e-10f;
    (&_S270)->differential_0 = 0.0f;
    _d_max_0(&_S269, &_S270, _S268.differential_0);
    DiffPair_float_0 _S271 = _S269;
    DiffPair_float_0 _S272;
    (&_S272)->primal_0 = _S157;
    (&_S272)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S272, _S266.differential_0);
    DiffPair_float_0 _S273;
    (&_S273)->primal_0 = _S89.primal_0;
    (&_S273)->differential_0 = 0.0f;
    DiffPair_float_0 _S274;
    (&_S274)->primal_0 = 1.00000001335143196e-10f;
    (&_S274)->differential_0 = 0.0f;
    _d_max_0(&_S273, &_S274, _S272.differential_0);
    DiffPair_float_0 _S275 = _S273;
    float3  _S276 = make_float3 (_S201, _S201, _S201);
    float3  _S277 = make_float3 (_S203, _S203, _S203);
    float3  _S278 = _S243.differential_0 + _S252.differential_0 + _S261.differential_0;
    float _S279;
    if(_S152)
    {
        float3  _S280 = _S96.primal_0 * _S278;
        float3  _S281 = _S153 * _S278;
        float _S282 = _S280.x + _S280.y + _S280.z;
        DiffPair_float_0 _S283;
        (&_S283)->primal_0 = _S149;
        (&_S283)->differential_0 = 0.0f;
        s_bwd_prop_rsqrt_0(&_S283, _S282);
        _S279 = _S283.differential_0;
        _S123 = _S281;
    }
    else
    {
        _S279 = 0.0f;
        _S123 = _S97;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S284;
    (&_S284)->primal_0 = _S96.primal_0;
    (&_S284)->differential_0 = _S97;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S285;
    (&_S285)->primal_0 = _S96.primal_0;
    (&_S285)->differential_0 = _S97;
    s_bwd_prop_dot_0(&_S284, &_S285, _S279);
    float _S286 = _S147 * _S200;
    DiffPair_float_0 _S287;
    (&_S287)->primal_0 = _S148;
    (&_S287)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S287, _S286);
    DiffPair_float_0 _S288;
    (&_S288)->primal_0 = cos_sim_loss_8;
    (&_S288)->differential_0 = 0.0f;
    DiffPair_float_0 _S289;
    (&_S289)->primal_0 = 9.999999960041972e-13f;
    (&_S289)->differential_0 = 0.0f;
    _d_max_0(&_S288, &_S289, _S287.differential_0);
    float _S290 = 0.5f * - (_S286 + _S288.differential_0);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S291;
    (&_S291)->primal_0 = _S244;
    (&_S291)->differential_0 = _S97;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S292;
    (&_S292)->primal_0 = _S262;
    (&_S292)->differential_0 = _S97;
    s_bwd_prop_dot_0(&_S291, &_S292, _S290);
    float _S293 = _S145 * _S199;
    DiffPair_float_0 _S294;
    (&_S294)->primal_0 = _S146;
    (&_S294)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S294, _S293);
    DiffPair_float_0 _S295;
    (&_S295)->primal_0 = cos_sim_loss_7;
    (&_S295)->differential_0 = 0.0f;
    DiffPair_float_0 _S296;
    (&_S296)->primal_0 = 9.999999960041972e-13f;
    (&_S296)->differential_0 = 0.0f;
    _d_max_0(&_S295, &_S296, _S294.differential_0);
    float _S297 = 0.5f * - (_S293 + _S295.differential_0);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S298;
    (&_S298)->primal_0 = _S262;
    (&_S298)->differential_0 = _S97;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S299;
    (&_S299)->primal_0 = _S253;
    (&_S299)->differential_0 = _S97;
    s_bwd_prop_dot_0(&_S298, &_S299, _S297);
    float _S300 = _S143 * _S198;
    DiffPair_float_0 _S301;
    (&_S301)->primal_0 = _S144;
    (&_S301)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S301, _S300);
    DiffPair_float_0 _S302;
    (&_S302)->primal_0 = cos_sim_loss_6;
    (&_S302)->differential_0 = 0.0f;
    DiffPair_float_0 _S303;
    (&_S303)->primal_0 = 9.999999960041972e-13f;
    (&_S303)->differential_0 = 0.0f;
    _d_max_0(&_S302, &_S303, _S301.differential_0);
    float _S304 = 0.5f * - (_S300 + _S302.differential_0);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S305;
    (&_S305)->primal_0 = _S244;
    (&_S305)->differential_0 = _S97;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S306;
    (&_S306)->primal_0 = _S253;
    (&_S306)->differential_0 = _S97;
    s_bwd_prop_dot_0(&_S305, &_S306, _S304);
    float3  _S307 = _S285.differential_0 + _S284.differential_0 + _S123;
    float3  _S308 = _S292.differential_0 + _S298.differential_0 + _S264.differential_0;
    float3  _S309 = _S291.differential_0 + _S305.differential_0 + _S246.differential_0;
    float3  _S310 = _S299.differential_0 + _S306.differential_0 + _S255.differential_0;
    if(_S139)
    {
        float3  _S311 = _S93.primal_0 * _S310;
        float3  _S312 = _S140 * _S310;
        float _S313 = _S311.x + _S311.y + _S311.z;
        DiffPair_float_0 _S314;
        (&_S314)->primal_0 = _S135;
        (&_S314)->differential_0 = 0.0f;
        s_bwd_prop_rsqrt_0(&_S314, _S313);
        _S279 = _S314.differential_0;
        _S123 = _S312;
    }
    else
    {
        _S279 = 0.0f;
        _S123 = _S97;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S315;
    (&_S315)->primal_0 = _S93.primal_0;
    (&_S315)->differential_0 = _S97;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S316;
    (&_S316)->primal_0 = _S93.primal_0;
    (&_S316)->differential_0 = _S97;
    s_bwd_prop_dot_0(&_S315, &_S316, _S279);
    float3  _S317 = _S316.differential_0 + _S315.differential_0 + _S123;
    if(_S131)
    {
        float3  _S318 = _S92.primal_0 * _S308;
        float3  _S319 = _S132 * _S308;
        float _S320 = _S318.x + _S318.y + _S318.z;
        DiffPair_float_0 _S321;
        (&_S321)->primal_0 = _S128;
        (&_S321)->differential_0 = 0.0f;
        s_bwd_prop_rsqrt_0(&_S321, _S320);
        _S279 = _S321.differential_0;
        _S123 = _S319;
    }
    else
    {
        _S279 = 0.0f;
        _S123 = _S97;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S322;
    (&_S322)->primal_0 = _S92.primal_0;
    (&_S322)->differential_0 = _S97;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S323;
    (&_S323)->primal_0 = _S92.primal_0;
    (&_S323)->differential_0 = _S97;
    s_bwd_prop_dot_0(&_S322, &_S323, _S279);
    float3  _S324 = _S323.differential_0 + _S322.differential_0 + _S123;
    if(_S124)
    {
        float3  _S325 = _S91.primal_0 * _S309;
        float3  _S326 = _S125 * _S309;
        float _S327 = _S325.x + _S325.y + _S325.z;
        DiffPair_float_0 _S328;
        (&_S328)->primal_0 = _S121;
        (&_S328)->differential_0 = 0.0f;
        s_bwd_prop_rsqrt_0(&_S328, _S327);
        _S279 = _S328.differential_0;
        _S123 = _S326;
    }
    else
    {
        _S279 = 0.0f;
        _S123 = _S97;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S329;
    (&_S329)->primal_0 = _S91.primal_0;
    (&_S329)->differential_0 = _S97;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S330;
    (&_S330)->primal_0 = _S91.primal_0;
    (&_S330)->differential_0 = _S97;
    s_bwd_prop_dot_0(&_S329, &_S330, _S279);
    float3  _S331 = _S330.differential_0 + _S329.differential_0 + _S123;
    float _S332 = _S119 * _S197;
    float _S333 = _S119 * _S196;
    float _S334 = _S118 * _S195;
    float _S335 = _S117 * (_S118 * _S197 + _S333 + _S333 + _S194);
    DiffPair_float_0 _S336;
    (&_S336)->primal_0 = _S90.primal_0;
    (&_S336)->differential_0 = 0.0f;
    DiffPair_float_0 _S337;
    (&_S337)->primal_0 = 0.00009999999747379f;
    (&_S337)->differential_0 = 0.0f;
    _d_max_0(&_S336, &_S337, _S335);
    DiffPair_float_0 _S338 = _S336;
    float _S339 = _S117 * (_S332 + _S334 + _S334 + _S193);
    DiffPair_float_0 _S340;
    (&_S340)->primal_0 = _S89.primal_0;
    (&_S340)->differential_0 = 0.0f;
    DiffPair_float_0 _S341;
    (&_S341)->primal_0 = 0.00009999999747379f;
    (&_S341)->differential_0 = 0.0f;
    _d_max_0(&_S340, &_S341, _S339);
    float _S342 = _S105 * _S192;
    DiffPair_float_0 _S343;
    (&_S343)->primal_0 = _S109;
    (&_S343)->differential_0 = 0.0f;
    DiffPair_float_0 _S344;
    (&_S344)->primal_0 = 0.0f;
    (&_S344)->differential_0 = 0.0f;
    DiffPair_float_0 _S345;
    (&_S345)->primal_0 = 1.0f;
    (&_S345)->differential_0 = 0.0f;
    s_bwd_prop_clamp_0(&_S343, &_S344, &_S345, _S342);
    float _S346 = _S105 * _S191;
    float _S347 = _S116 * _S346;
    float _S348 = _S115 * (dV_1 * _S346);
    float _S349 = _S114 * _S346;
    float _S350 = _S113 * (dU_1 * _S346);
    float _S351 = _S112 * _S346;
    float _S352 = _S111 * (dY_1 * _S346);
    float _S353 = _S110 * _S346;
    DiffPair_float_0 _S354;
    (&_S354)->primal_0 = dY_1;
    (&_S354)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S354, _S353);
    float _S355 = 0.3333333432674408f * (_S343.differential_0 + _S108 * _S346);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S356;
    (&_S356)->primal_0 = _S107;
    (&_S356)->differential_0 = _S97;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S357;
    (&_S357)->primal_0 = _S107;
    (&_S357)->differential_0 = _S97;
    s_bwd_prop_dot_0(&_S356, &_S357, _S355);
    float _S358 = 0.3333333432674408f * (_S106 * _S346);
    float3  _S359 = make_float3 (_S358, _S358, _S358);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S360;
    (&_S360)->primal_0 = _S107;
    (&_S360)->differential_0 = _S97;
    s_bwd_prop_abs_1(&_S360, _S359);
    float3  _S361 = _S357.differential_0 + _S356.differential_0 + _S360.differential_0;
    float _S362 = _S347 + _S348;
    float s_diff_V_T_0 = - _S362;
    float _S363 = _S349 + _S350;
    float s_diff_U_T_0 = - _S363;
    float _S364 = _S351 + _S352 + _S354.differential_0;
    float s_diff_Y_T_0 = - _S364;
    float _S365 = - s_diff_V_T_0;
    float _S366 = _S340.differential_0 + _S275.differential_0;
    float3  _S367 = _S361 + make_float3 (0.61500000953674316f * s_diff_V_T_0 + -0.14712999761104584f * s_diff_U_T_0 + 0.29899999499320984f * s_diff_Y_T_0, 0.51498997211456299f * _S365 + 0.28885999321937561f * - s_diff_U_T_0 + 0.58700001239776611f * s_diff_Y_T_0, 0.10001000016927719f * _S365 + 0.43599998950958252f * s_diff_U_T_0 + 0.11400000005960464f * s_diff_Y_T_0);
    float3  _S368 = - _S361 + make_float3 (0.61500000953674316f * _S362 + -0.14712999761104584f * _S363 + 0.29899999499320984f * _S364, 0.51498997211456299f * s_diff_V_T_0 + 0.28885999321937561f * s_diff_U_T_0 + 0.58700001239776611f * _S364, 0.10001000016927719f * s_diff_V_T_0 + 0.43599998950958252f * _S363 + 0.11400000005960464f * _S364);
    if(_S98)
    {
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S369;
        (&_S369)->primal_0 = _S93.primal_0;
        (&_S369)->differential_0 = _S97;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S370;
        (&_S370)->primal_0 = _S93.primal_0;
        (&_S370)->differential_0 = _S97;
        s_bwd_prop_dot_0(&_S369, &_S370, 0.0f);
        _S123 = _S370.differential_0 + _S369.differential_0 + _S317;
    }
    else
    {
        _S123 = _S317;
    }
    dpmedian_normal_0->primal_0 = (*dpmedian_normal_0).primal_0;
    dpmedian_normal_0->differential_0 = _S307;
    dpmedian_depth_0->primal_0 = (*dpmedian_depth_0).primal_0;
    dpmedian_depth_0->differential_0 = _S271.differential_0;
    dpnormal_dist_0->primal_0 = (*dpnormal_dist_0).primal_0;
    dpnormal_dist_0->differential_0 = _S276;
    dpdepth_dist_0->primal_0 = (*dpdepth_dist_0).primal_0;
    dpdepth_dist_0->differential_0 = _S202;
    dprgb_dist_0->primal_0 = (*dprgb_dist_0).primal_0;
    dprgb_dist_0->differential_0 = _S277;
    dprender_Ts_0->primal_0 = (*dprender_Ts_0).primal_0;
    dprender_Ts_0->differential_0 = _S237;
    dpref_normal_0->primal_0 = (*dpref_normal_0).primal_0;
    dpref_normal_0->differential_0 = _S123;
    dpdepth_normal_0->primal_0 = (*dpdepth_normal_0).primal_0;
    dpdepth_normal_0->differential_0 = _S324;
    dprender_normal_0->primal_0 = (*dprender_normal_0).primal_0;
    dprender_normal_0->differential_0 = _S331;
    dpref_depth_0->primal_0 = (*dpref_depth_0).primal_0;
    dpref_depth_0->differential_0 = _S338.differential_0;
    dprender_depth_0->primal_0 = (*dprender_depth_0).primal_0;
    dprender_depth_0->differential_0 = _S366;
    dpref_rgb_0->primal_0 = (*dpref_rgb_0).primal_0;
    dpref_rgb_0->differential_0 = _S367;
    dprender_rgb_0->primal_0 = (*dprender_rgb_0).primal_0;
    dprender_rgb_0->differential_0 = _S368;
    return;
}

inline __device__ void s_bwd_per_pixel_losses_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S371, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S372, DiffPair_float_0 * _S373, DiffPair_float_0 * _S374, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S375, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S376, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S377, DiffPair_float_0 * _S378, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S379, DiffPair_float_0 * _S380, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S381, DiffPair_float_0 * _S382, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S383, bool _S384, bool _S385, FixedArray<float, 19>  * _S386, FixedArray<float, 32>  * _S387)
{
    s_bwd_prop_per_pixel_losses_0(_S371, _S372, _S373, _S374, _S375, _S376, _S377, _S378, _S379, _S380, _S381, _S382, _S383, _S384, _S385, _S386, _S387);
    return;
}

inline __device__ void per_pixel_losses_bwd(float3  render_rgb_1, float3  ref_rgb_1, float render_depth_1, float ref_depth_1, float3  render_normal_1, float3  depth_normal_1, float3  ref_normal_1, float render_Ts_1, float3  rgb_dist_1, float depth_dist_1, float3  normal_dist_1, float median_depth_1, float3  median_normal_1, bool ref_alpha_2, bool has_mask_2, FixedArray<float, 19>  weights_2, FixedArray<float, 32>  v_losses_0, float3  * v_render_rgb_0, float3  * v_ref_rgb_0, float * v_render_depth_0, float * v_ref_depth_0, float3  * v_render_normal_0, float3  * v_depth_normal_0, float3  * v_ref_normal_0, float * v_render_Ts_0, float3  * v_rgb_dist_0, float * v_depth_dist_0, float3  * v_normal_dist_0, float * v_median_depth_0, float3  * v_median_normal_0)
{
    float3  _S388 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_render_rgb_0;
    (&dp_render_rgb_0)->primal_0 = render_rgb_1;
    (&dp_render_rgb_0)->differential_0 = _S388;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ref_rgb_0;
    (&dp_ref_rgb_0)->primal_0 = ref_rgb_1;
    (&dp_ref_rgb_0)->differential_0 = _S388;
    DiffPair_float_0 dp_render_depth_0;
    (&dp_render_depth_0)->primal_0 = render_depth_1;
    (&dp_render_depth_0)->differential_0 = 0.0f;
    DiffPair_float_0 dp_ref_depth_0;
    (&dp_ref_depth_0)->primal_0 = ref_depth_1;
    (&dp_ref_depth_0)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_render_normal_0;
    (&dp_render_normal_0)->primal_0 = render_normal_1;
    (&dp_render_normal_0)->differential_0 = _S388;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_depth_normal_0;
    (&dp_depth_normal_0)->primal_0 = depth_normal_1;
    (&dp_depth_normal_0)->differential_0 = _S388;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ref_normal_0;
    (&dp_ref_normal_0)->primal_0 = ref_normal_1;
    (&dp_ref_normal_0)->differential_0 = _S388;
    DiffPair_float_0 dp_render_Ts_0;
    (&dp_render_Ts_0)->primal_0 = render_Ts_1;
    (&dp_render_Ts_0)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_rgb_dist_0;
    (&dp_rgb_dist_0)->primal_0 = rgb_dist_1;
    (&dp_rgb_dist_0)->differential_0 = _S388;
    DiffPair_float_0 dp_depth_dist_0;
    (&dp_depth_dist_0)->primal_0 = depth_dist_1;
    (&dp_depth_dist_0)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_normal_dist_0;
    (&dp_normal_dist_0)->primal_0 = normal_dist_1;
    (&dp_normal_dist_0)->differential_0 = _S388;
    DiffPair_float_0 dp_median_depth_0;
    (&dp_median_depth_0)->primal_0 = median_depth_1;
    (&dp_median_depth_0)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_median_normal_0;
    (&dp_median_normal_0)->primal_0 = median_normal_1;
    (&dp_median_normal_0)->differential_0 = _S388;
    FixedArray<float, 19>  _S389 = weights_2;
    FixedArray<float, 32>  _S390 = v_losses_0;
    s_bwd_per_pixel_losses_0(&dp_render_rgb_0, &dp_ref_rgb_0, &dp_render_depth_0, &dp_ref_depth_0, &dp_render_normal_0, &dp_depth_normal_0, &dp_ref_normal_0, &dp_render_Ts_0, &dp_rgb_dist_0, &dp_depth_dist_0, &dp_normal_dist_0, &dp_median_depth_0, &dp_median_normal_0, ref_alpha_2, has_mask_2, &_S389, &_S390);
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

inline __device__ void _d_log10_0(DiffPair_float_0 * dpx_9, float dOut_9)
{
    float _S391 = 1.0f / ((*dpx_9).primal_0 * 2.30258512496948242f) * dOut_9;
    dpx_9->primal_0 = (*dpx_9).primal_0;
    dpx_9->differential_0 = _S391;
    return;
}

inline __device__ void per_pixel_losses_reduce(FixedArray<float, 32>  raw_losses_0, FixedArray<float, 19>  weights_3, FixedArray<float, 14>  * _S392)
{
    FixedArray<float, 14>  losses_1;
    float _S393 = (F32_max((raw_losses_0[int(21)]), (1.0f)));
    losses_1[int(0)] = raw_losses_0[int(0)] / _S393;
    losses_1[int(1)] = -10.0f * (F32_log10((raw_losses_0[int(1)] / _S393)));
    bool _S394;
    if((raw_losses_0[int(22)]) > 0.0f)
    {
        _S394 = (raw_losses_0[int(3)]) != 0.0f;
    }
    else
    {
        _S394 = false;
    }
    float _S395;
    if(_S394)
    {
        _S395 = weights_3[int(6)] * clamp_0(1.0f - (raw_losses_0[int(6)] - raw_losses_0[int(2)] * raw_losses_0[int(3)] / raw_losses_0[int(22)]) / (F32_sqrt(((F32_max((9.999999960041972e-13f), ((raw_losses_0[int(4)] - raw_losses_0[int(2)] * raw_losses_0[int(2)] / raw_losses_0[int(22)]) * (raw_losses_0[int(5)] - raw_losses_0[int(3)] * raw_losses_0[int(3)] / raw_losses_0[int(22)]) + 1.0f)))))), 0.0f, 2.0f);
    }
    else
    {
        _S395 = 0.0f;
    }
    losses_1[int(2)] = _S395;
    losses_1[int(3)] = (raw_losses_0[int(7)] / (F32_max((raw_losses_0[int(23)]), (1.0f))) + raw_losses_0[int(8)] / (F32_max((raw_losses_0[int(24)]), (1.0f)))) / float((I32_max((int((raw_losses_0[int(23)]) > 0.5f) + int((raw_losses_0[int(24)]) > 0.5f)), (int(1)))));
    losses_1[int(4)] = raw_losses_0[int(9)] / (F32_max((raw_losses_0[int(26)]), (1.0f))) + raw_losses_0[int(10)] / (F32_max((raw_losses_0[int(27)]), (1.0f)));
    losses_1[int(5)] = raw_losses_0[int(11)] / (F32_max((raw_losses_0[int(25)]), (1.0f)));
    losses_1[int(6)] = raw_losses_0[int(12)] / _S393;
    losses_1[int(7)] = raw_losses_0[int(13)] / _S393;
    losses_1[int(8)] = raw_losses_0[int(14)] / _S393;
    losses_1[int(9)] = raw_losses_0[int(15)] / _S393;
    losses_1[int(10)] = raw_losses_0[int(16)] / (F32_max((raw_losses_0[int(28)]), (1.0f)));
    losses_1[int(11)] = raw_losses_0[int(17)] / (F32_max((raw_losses_0[int(29)]), (1.0f)));
    losses_1[int(12)] = raw_losses_0[int(18)] / (F32_max((raw_losses_0[int(30)]), (1.0f)));
    losses_1[int(13)] = raw_losses_0[int(19)] / (F32_max((raw_losses_0[int(31)]), (1.0f)));
    *_S392 = losses_1;
    return;
}

struct DiffPair_arrayx3Cfloatx2C32x3E_0
{
    FixedArray<float, 32>  primal_0;
    FixedArray<float, 32>  differential_0;
};

inline __device__ float s_primal_ctx_sqrt_0(float _S396)
{
    return (F32_sqrt((_S396)));
}

inline __device__ void s_bwd_prop_log10_0(DiffPair_float_0 * _S397, float _S398)
{
    _d_log10_0(_S397, _S398);
    return;
}

inline __device__ void s_bwd_prop_per_pixel_losses_reduce_0(DiffPair_arrayx3Cfloatx2C32x3E_0 * dpraw_losses_0, FixedArray<float, 19>  * weights_4, FixedArray<float, 14>  * _s_dOut_1)
{
    FixedArray<float, 32>  _S399 = dpraw_losses_0->primal_0;
    float _S400 = (F32_max((dpraw_losses_0->primal_0[int(21)]), (1.0f)));
    float _S401 = _S400 * _S400;
    float _S402 = dpraw_losses_0->primal_0[int(1)] / _S400;
    bool _S403 = (dpraw_losses_0->primal_0[int(22)]) > 0.0f;
    bool _S404;
    if(_S403)
    {
        _S404 = (_S399[int(3)]) != 0.0f;
    }
    else
    {
        _S404 = false;
    }
    float _S405;
    float _S406;
    float _S407;
    float _S408;
    float _S409;
    float _S410;
    float _S411;
    float _S412;
    float _S413;
    float _S414;
    float _S415;
    float _S416;
    float _S417;
    float _S418;
    float _S419;
    if(_S404)
    {
        float _S420 = _S399[int(2)] * _S399[int(3)];
        float _S421 = _S399[int(22)] * _S399[int(22)];
        float _S422 = _S399[int(6)] - _S420 / _S399[int(22)];
        float _S423 = _S399[int(2)] * _S399[int(2)];
        float _S424 = _S399[int(4)] - _S423 / _S399[int(22)];
        float _S425 = _S399[int(3)] * _S399[int(3)];
        float _S426 = _S399[int(5)] - _S425 / _S399[int(22)];
        float _S427 = _S424 * _S426 + 1.0f;
        float _S428 = (F32_max((9.999999960041972e-13f), (_S427)));
        float _S429 = s_primal_ctx_sqrt_0(_S428);
        float _S430 = _S429 * _S429;
        float _S431 = 1.0f - _S422 / _S429;
        _S405 = (*weights_4)[int(6)];
        _S406 = _S431;
        _S407 = _S430;
        _S408 = _S422;
        _S409 = _S429;
        _S410 = _S428;
        _S411 = _S427;
        _S412 = _S424;
        _S413 = _S426;
        _S414 = _S421;
        _S415 = _S425;
        _S416 = _S399[int(3)];
        _S417 = _S423;
        _S418 = _S399[int(2)];
        _S419 = _S420;
    }
    else
    {
        _S405 = 0.0f;
        _S406 = 0.0f;
        _S407 = 0.0f;
        _S408 = 0.0f;
        _S409 = 0.0f;
        _S410 = 0.0f;
        _S411 = 0.0f;
        _S412 = 0.0f;
        _S413 = 0.0f;
        _S414 = 0.0f;
        _S415 = 0.0f;
        _S416 = 0.0f;
        _S417 = 0.0f;
        _S418 = 0.0f;
        _S419 = 0.0f;
    }
    float _S432 = (F32_max((_S399[int(23)]), (1.0f)));
    float _S433 = _S432 * _S432;
    float _S434 = (F32_max((_S399[int(24)]), (1.0f)));
    float _S435 = _S434 * _S434;
    float _S436 = float((I32_max((int((_S399[int(23)]) > 0.5f) + int((_S399[int(24)]) > 0.5f)), (int(1)))));
    float _S437 = (F32_max((_S399[int(26)]), (1.0f)));
    float _S438 = _S437 * _S437;
    float _S439 = (F32_max((_S399[int(27)]), (1.0f)));
    float _S440 = _S439 * _S439;
    float _S441 = (F32_max((_S399[int(25)]), (1.0f)));
    float _S442 = _S441 * _S441;
    float _S443 = (F32_max((_S399[int(28)]), (1.0f)));
    float _S444 = _S443 * _S443;
    float _S445 = (F32_max((_S399[int(29)]), (1.0f)));
    float _S446 = _S445 * _S445;
    float _S447 = (F32_max((_S399[int(30)]), (1.0f)));
    float _S448 = _S447 * _S447;
    float _S449 = (F32_max((_S399[int(31)]), (1.0f)));
    float _S450 = _S449 * _S449;
    float _S451 = (*_s_dOut_1)[int(0)];
    float _S452 = (*_s_dOut_1)[int(1)];
    float _S453 = (*_s_dOut_1)[int(2)];
    float _S454 = (*_s_dOut_1)[int(13)] / _S450;
    float _S455 = _S399[int(19)] * - _S454;
    float _S456 = _S449 * _S454;
    DiffPair_float_0 _S457;
    (&_S457)->primal_0 = _S399[int(31)];
    (&_S457)->differential_0 = 0.0f;
    DiffPair_float_0 _S458;
    (&_S458)->primal_0 = 1.0f;
    (&_S458)->differential_0 = 0.0f;
    _d_max_0(&_S457, &_S458, _S455);
    float _S459 = (*_s_dOut_1)[int(12)] / _S448;
    float _S460 = _S399[int(18)] * - _S459;
    float _S461 = _S447 * _S459;
    DiffPair_float_0 _S462;
    (&_S462)->primal_0 = _S399[int(30)];
    (&_S462)->differential_0 = 0.0f;
    DiffPair_float_0 _S463;
    (&_S463)->primal_0 = 1.0f;
    (&_S463)->differential_0 = 0.0f;
    _d_max_0(&_S462, &_S463, _S460);
    float _S464 = (*_s_dOut_1)[int(11)] / _S446;
    float _S465 = _S399[int(17)] * - _S464;
    float _S466 = _S445 * _S464;
    DiffPair_float_0 _S467;
    (&_S467)->primal_0 = _S399[int(29)];
    (&_S467)->differential_0 = 0.0f;
    DiffPair_float_0 _S468;
    (&_S468)->primal_0 = 1.0f;
    (&_S468)->differential_0 = 0.0f;
    _d_max_0(&_S467, &_S468, _S465);
    float _S469 = (*_s_dOut_1)[int(10)] / _S444;
    float _S470 = _S399[int(16)] * - _S469;
    float _S471 = _S443 * _S469;
    DiffPair_float_0 _S472;
    (&_S472)->primal_0 = _S399[int(28)];
    (&_S472)->differential_0 = 0.0f;
    DiffPair_float_0 _S473;
    (&_S473)->primal_0 = 1.0f;
    (&_S473)->differential_0 = 0.0f;
    _d_max_0(&_S472, &_S473, _S470);
    float _S474 = (*_s_dOut_1)[int(9)] / _S401;
    float _S475 = _S399[int(15)] * - _S474;
    float _S476 = _S400 * _S474;
    float _S477 = (*_s_dOut_1)[int(8)] / _S401;
    float _S478 = _S399[int(14)] * - _S477;
    float _S479 = _S400 * _S477;
    float _S480 = (*_s_dOut_1)[int(7)] / _S401;
    float _S481 = _S399[int(13)] * - _S480;
    float _S482 = _S400 * _S480;
    float _S483 = (*_s_dOut_1)[int(6)] / _S401;
    float _S484 = _S399[int(12)] * - _S483;
    float _S485 = _S400 * _S483;
    float _S486 = (*_s_dOut_1)[int(5)] / _S442;
    float _S487 = _S399[int(11)] * - _S486;
    float _S488 = _S441 * _S486;
    DiffPair_float_0 _S489;
    (&_S489)->primal_0 = _S399[int(25)];
    (&_S489)->differential_0 = 0.0f;
    DiffPair_float_0 _S490;
    (&_S490)->primal_0 = 1.0f;
    (&_S490)->differential_0 = 0.0f;
    _d_max_0(&_S489, &_S490, _S487);
    float _S491 = (*_s_dOut_1)[int(4)] / _S440;
    float _S492 = _S399[int(10)] * - _S491;
    float _S493 = _S439 * _S491;
    DiffPair_float_0 _S494;
    (&_S494)->primal_0 = _S399[int(27)];
    (&_S494)->differential_0 = 0.0f;
    DiffPair_float_0 _S495;
    (&_S495)->primal_0 = 1.0f;
    (&_S495)->differential_0 = 0.0f;
    _d_max_0(&_S494, &_S495, _S492);
    float _S496 = (*_s_dOut_1)[int(4)] / _S438;
    float _S497 = _S399[int(9)] * - _S496;
    float _S498 = _S437 * _S496;
    DiffPair_float_0 _S499;
    (&_S499)->primal_0 = _S399[int(26)];
    (&_S499)->differential_0 = 0.0f;
    DiffPair_float_0 _S500;
    (&_S500)->primal_0 = 1.0f;
    (&_S500)->differential_0 = 0.0f;
    _d_max_0(&_S499, &_S500, _S497);
    float _S501 = (*_s_dOut_1)[int(3)] / _S436;
    float _S502 = _S501 / _S435;
    float _S503 = _S399[int(8)] * - _S502;
    float _S504 = _S434 * _S502;
    DiffPair_float_0 _S505;
    (&_S505)->primal_0 = _S399[int(24)];
    (&_S505)->differential_0 = 0.0f;
    DiffPair_float_0 _S506;
    (&_S506)->primal_0 = 1.0f;
    (&_S506)->differential_0 = 0.0f;
    _d_max_0(&_S505, &_S506, _S503);
    float _S507 = _S501 / _S433;
    float _S508 = _S399[int(7)] * - _S507;
    float _S509 = _S432 * _S507;
    DiffPair_float_0 _S510;
    (&_S510)->primal_0 = _S399[int(23)];
    (&_S510)->differential_0 = 0.0f;
    DiffPair_float_0 _S511;
    (&_S511)->primal_0 = 1.0f;
    (&_S511)->differential_0 = 0.0f;
    _d_max_0(&_S510, &_S511, _S508);
    float _S512 = _S475 + _S478 + _S481 + _S484;
    FixedArray<float, 32>  _S513;
    _S513[int(0)] = 0.0f;
    _S513[int(1)] = 0.0f;
    _S513[int(2)] = 0.0f;
    _S513[int(3)] = 0.0f;
    _S513[int(4)] = 0.0f;
    _S513[int(5)] = 0.0f;
    _S513[int(6)] = 0.0f;
    _S513[int(7)] = 0.0f;
    _S513[int(8)] = 0.0f;
    _S513[int(9)] = 0.0f;
    _S513[int(10)] = 0.0f;
    _S513[int(11)] = 0.0f;
    _S513[int(12)] = 0.0f;
    _S513[int(13)] = 0.0f;
    _S513[int(14)] = 0.0f;
    _S513[int(15)] = 0.0f;
    _S513[int(16)] = 0.0f;
    _S513[int(17)] = 0.0f;
    _S513[int(18)] = 0.0f;
    _S513[int(19)] = 0.0f;
    _S513[int(20)] = 0.0f;
    _S513[int(21)] = 0.0f;
    _S513[int(22)] = 0.0f;
    _S513[int(23)] = 0.0f;
    _S513[int(24)] = 0.0f;
    _S513[int(25)] = 0.0f;
    _S513[int(26)] = 0.0f;
    _S513[int(27)] = 0.0f;
    _S513[int(28)] = 0.0f;
    _S513[int(29)] = 0.0f;
    _S513[int(30)] = 0.0f;
    _S513[int(31)] = 0.0f;
    _S513[int(12)] = _S485;
    _S513[int(7)] = _S509;
    _S513[int(23)] = _S510.differential_0;
    _S513[int(8)] = _S504;
    _S513[int(24)] = _S505.differential_0;
    _S513[int(9)] = _S498;
    _S513[int(26)] = _S499.differential_0;
    _S513[int(10)] = _S493;
    _S513[int(27)] = _S494.differential_0;
    _S513[int(11)] = _S488;
    _S513[int(25)] = _S489.differential_0;
    _S513[int(31)] = _S457.differential_0;
    _S513[int(13)] = _S482;
    _S513[int(14)] = _S479;
    _S513[int(15)] = _S476;
    _S513[int(16)] = _S471;
    _S513[int(28)] = _S472.differential_0;
    _S513[int(17)] = _S466;
    _S513[int(29)] = _S467.differential_0;
    _S513[int(18)] = _S461;
    _S513[int(30)] = _S462.differential_0;
    _S513[int(19)] = _S456;
    float _S514 = _S513[int(0)];
    float _S515 = _S513[int(1)];
    float _S516 = _S513[int(2)];
    float _S517 = _S513[int(3)];
    float _S518 = _S513[int(4)];
    float _S519 = _S513[int(5)];
    float _S520 = _S513[int(6)];
    float _S521 = _S513[int(7)];
    float _S522 = _S513[int(8)];
    float _S523 = _S513[int(9)];
    float _S524 = _S513[int(10)];
    float _S525 = _S513[int(11)];
    float _S526 = _S513[int(12)];
    float _S527 = _S513[int(13)];
    float _S528 = _S513[int(14)];
    float _S529 = _S513[int(15)];
    float _S530 = _S513[int(16)];
    float _S531 = _S513[int(17)];
    float _S532 = _S513[int(18)];
    float _S533 = _S513[int(19)];
    float _S534 = _S513[int(20)];
    float _S535 = _S513[int(21)];
    float _S536 = _S513[int(22)];
    float _S537 = _S513[int(23)];
    float _S538 = _S513[int(24)];
    float _S539 = _S513[int(25)];
    float _S540 = _S513[int(26)];
    float _S541 = _S513[int(27)];
    float _S542 = _S513[int(28)];
    float _S543 = _S513[int(29)];
    float _S544 = _S513[int(30)];
    float _S545 = _S513[int(31)];
    FixedArray<float, 32>  _S546;
    if(_S404)
    {
        float _S547 = _S405 * _S453;
        DiffPair_float_0 _S548;
        (&_S548)->primal_0 = _S406;
        (&_S548)->differential_0 = 0.0f;
        DiffPair_float_0 _S549;
        (&_S549)->primal_0 = 0.0f;
        (&_S549)->differential_0 = 0.0f;
        DiffPair_float_0 _S550;
        (&_S550)->primal_0 = 2.0f;
        (&_S550)->differential_0 = 0.0f;
        s_bwd_prop_clamp_0(&_S548, &_S549, &_S550, _S547);
        float _S551 = - _S548.differential_0 / _S407;
        float _S552 = _S408 * - _S551;
        float _S553 = _S409 * _S551;
        DiffPair_float_0 _S554;
        (&_S554)->primal_0 = _S410;
        (&_S554)->differential_0 = 0.0f;
        s_bwd_prop_sqrt_0(&_S554, _S552);
        DiffPair_float_0 _S555;
        (&_S555)->primal_0 = 9.999999960041972e-13f;
        (&_S555)->differential_0 = 0.0f;
        DiffPair_float_0 _S556;
        (&_S556)->primal_0 = _S411;
        (&_S556)->differential_0 = 0.0f;
        _d_max_0(&_S555, &_S556, _S554.differential_0);
        float _S557 = _S412 * _S556.differential_0;
        float _S558 = _S413 * _S556.differential_0;
        float _S559 = - _S557 / _S414;
        float _S560 = _S416 * (_S399[int(22)] * _S559);
        float _S561 = - _S558 / _S414;
        float _S562 = _S418 * (_S399[int(22)] * _S561);
        float _S563 = - _S553 / _S414;
        float _S564 = _S399[int(22)] * _S563;
        float _S565 = _S560 + _S560 + _S418 * _S564;
        float _S566 = _S562 + _S562 + _S416 * _S564;
        float _S567 = _S415 * - _S559 + _S417 * - _S561 + _S419 * - _S563;
        FixedArray<float, 32>  _S568;
        _S568[int(0)] = 0.0f;
        _S568[int(1)] = 0.0f;
        _S568[int(2)] = 0.0f;
        _S568[int(3)] = 0.0f;
        _S568[int(4)] = 0.0f;
        _S568[int(5)] = 0.0f;
        _S568[int(6)] = 0.0f;
        _S568[int(7)] = 0.0f;
        _S568[int(8)] = 0.0f;
        _S568[int(9)] = 0.0f;
        _S568[int(10)] = 0.0f;
        _S568[int(11)] = 0.0f;
        _S568[int(12)] = 0.0f;
        _S568[int(13)] = 0.0f;
        _S568[int(14)] = 0.0f;
        _S568[int(15)] = 0.0f;
        _S568[int(16)] = 0.0f;
        _S568[int(17)] = 0.0f;
        _S568[int(18)] = 0.0f;
        _S568[int(19)] = 0.0f;
        _S568[int(20)] = 0.0f;
        _S568[int(21)] = 0.0f;
        _S568[int(22)] = 0.0f;
        _S568[int(23)] = 0.0f;
        _S568[int(24)] = 0.0f;
        _S568[int(25)] = 0.0f;
        _S568[int(26)] = 0.0f;
        _S568[int(27)] = 0.0f;
        _S568[int(28)] = 0.0f;
        _S568[int(29)] = 0.0f;
        _S568[int(30)] = 0.0f;
        _S568[int(31)] = 0.0f;
        _S568[int(5)] = _S557;
        _S568[int(4)] = _S558;
        _S568[int(3)] = _S565;
        _S568[int(2)] = _S566;
        _S568[int(6)] = _S553;
        float _S569 = _S515 + _S568[int(1)];
        float _S570 = _S516 + _S568[int(2)];
        float _S571 = _S517 + _S568[int(3)];
        float _S572 = _S518 + _S568[int(4)];
        float _S573 = _S519 + _S568[int(5)];
        float _S574 = _S520 + _S568[int(6)];
        float _S575 = _S521 + _S568[int(7)];
        float _S576 = _S522 + _S568[int(8)];
        float _S577 = _S523 + _S568[int(9)];
        float _S578 = _S524 + _S568[int(10)];
        float _S579 = _S525 + _S568[int(11)];
        float _S580 = _S526 + _S568[int(12)];
        float _S581 = _S527 + _S568[int(13)];
        float _S582 = _S528 + _S568[int(14)];
        float _S583 = _S529 + _S568[int(15)];
        float _S584 = _S530 + _S568[int(16)];
        float _S585 = _S531 + _S568[int(17)];
        float _S586 = _S532 + _S568[int(18)];
        float _S587 = _S533 + _S568[int(19)];
        float _S588 = _S534 + _S568[int(20)];
        float _S589 = _S535 + _S568[int(21)];
        float _S590 = _S536 + _S568[int(22)];
        float _S591 = _S537 + _S568[int(23)];
        float _S592 = _S538 + _S568[int(24)];
        float _S593 = _S539 + _S568[int(25)];
        float _S594 = _S540 + _S568[int(26)];
        float _S595 = _S541 + _S568[int(27)];
        float _S596 = _S542 + _S568[int(28)];
        float _S597 = _S543 + _S568[int(29)];
        float _S598 = _S544 + _S568[int(30)];
        float _S599 = _S545 + _S568[int(31)];
        _S546[int(0)] = _S514 + _S568[int(0)];
        _S546[int(1)] = _S569;
        _S546[int(2)] = _S570;
        _S546[int(3)] = _S571;
        _S546[int(4)] = _S572;
        _S546[int(5)] = _S573;
        _S546[int(6)] = _S574;
        _S546[int(7)] = _S575;
        _S546[int(8)] = _S576;
        _S546[int(9)] = _S577;
        _S546[int(10)] = _S578;
        _S546[int(11)] = _S579;
        _S546[int(12)] = _S580;
        _S546[int(13)] = _S581;
        _S546[int(14)] = _S582;
        _S546[int(15)] = _S583;
        _S546[int(16)] = _S584;
        _S546[int(17)] = _S585;
        _S546[int(18)] = _S586;
        _S546[int(19)] = _S587;
        _S546[int(20)] = _S588;
        _S546[int(21)] = _S589;
        _S546[int(22)] = _S590;
        _S546[int(23)] = _S591;
        _S546[int(24)] = _S592;
        _S546[int(25)] = _S593;
        _S546[int(26)] = _S594;
        _S546[int(27)] = _S595;
        _S546[int(28)] = _S596;
        _S546[int(29)] = _S597;
        _S546[int(30)] = _S598;
        _S546[int(31)] = _S599;
        _S405 = _S567;
    }
    else
    {
        _S546[int(0)] = _S514;
        _S546[int(1)] = _S515;
        _S546[int(2)] = _S516;
        _S546[int(3)] = _S517;
        _S546[int(4)] = _S518;
        _S546[int(5)] = _S519;
        _S546[int(6)] = _S520;
        _S546[int(7)] = _S521;
        _S546[int(8)] = _S522;
        _S546[int(9)] = _S523;
        _S546[int(10)] = _S524;
        _S546[int(11)] = _S525;
        _S546[int(12)] = _S526;
        _S546[int(13)] = _S527;
        _S546[int(14)] = _S528;
        _S546[int(15)] = _S529;
        _S546[int(16)] = _S530;
        _S546[int(17)] = _S531;
        _S546[int(18)] = _S532;
        _S546[int(19)] = _S533;
        _S546[int(20)] = _S534;
        _S546[int(21)] = _S535;
        _S546[int(22)] = _S536;
        _S546[int(23)] = _S537;
        _S546[int(24)] = _S538;
        _S546[int(25)] = _S539;
        _S546[int(26)] = _S540;
        _S546[int(27)] = _S541;
        _S546[int(28)] = _S542;
        _S546[int(29)] = _S543;
        _S546[int(30)] = _S544;
        _S546[int(31)] = _S545;
        _S405 = 0.0f;
    }
    if(_S403)
    {
        FixedArray<float, 32>  _S600;
        _S600[int(0)] = 0.0f;
        _S600[int(1)] = 0.0f;
        _S600[int(2)] = 0.0f;
        _S600[int(3)] = 0.0f;
        _S600[int(4)] = 0.0f;
        _S600[int(5)] = 0.0f;
        _S600[int(6)] = 0.0f;
        _S600[int(7)] = 0.0f;
        _S600[int(8)] = 0.0f;
        _S600[int(9)] = 0.0f;
        _S600[int(10)] = 0.0f;
        _S600[int(11)] = 0.0f;
        _S600[int(12)] = 0.0f;
        _S600[int(13)] = 0.0f;
        _S600[int(14)] = 0.0f;
        _S600[int(15)] = 0.0f;
        _S600[int(16)] = 0.0f;
        _S600[int(17)] = 0.0f;
        _S600[int(18)] = 0.0f;
        _S600[int(19)] = 0.0f;
        _S600[int(20)] = 0.0f;
        _S600[int(21)] = 0.0f;
        _S600[int(22)] = 0.0f;
        _S600[int(23)] = 0.0f;
        _S600[int(24)] = 0.0f;
        _S600[int(25)] = 0.0f;
        _S600[int(26)] = 0.0f;
        _S600[int(27)] = 0.0f;
        _S600[int(28)] = 0.0f;
        _S600[int(29)] = 0.0f;
        _S600[int(30)] = 0.0f;
        _S600[int(31)] = 0.0f;
        _S600[int(3)] = 0.0f;
        float _S601 = _S546[int(1)] + _S600[int(1)];
        float _S602 = _S546[int(2)] + _S600[int(2)];
        float _S603 = _S546[int(3)] + _S600[int(3)];
        float _S604 = _S546[int(4)] + _S600[int(4)];
        float _S605 = _S546[int(5)] + _S600[int(5)];
        float _S606 = _S546[int(6)] + _S600[int(6)];
        float _S607 = _S546[int(7)] + _S600[int(7)];
        float _S608 = _S546[int(8)] + _S600[int(8)];
        float _S609 = _S546[int(9)] + _S600[int(9)];
        float _S610 = _S546[int(10)] + _S600[int(10)];
        float _S611 = _S546[int(11)] + _S600[int(11)];
        float _S612 = _S546[int(12)] + _S600[int(12)];
        float _S613 = _S546[int(13)] + _S600[int(13)];
        float _S614 = _S546[int(14)] + _S600[int(14)];
        float _S615 = _S546[int(15)] + _S600[int(15)];
        float _S616 = _S546[int(16)] + _S600[int(16)];
        float _S617 = _S546[int(17)] + _S600[int(17)];
        float _S618 = _S546[int(18)] + _S600[int(18)];
        float _S619 = _S546[int(19)] + _S600[int(19)];
        float _S620 = _S546[int(20)] + _S600[int(20)];
        float _S621 = _S546[int(21)] + _S600[int(21)];
        float _S622 = _S546[int(22)] + _S600[int(22)];
        float _S623 = _S546[int(23)] + _S600[int(23)];
        float _S624 = _S546[int(24)] + _S600[int(24)];
        float _S625 = _S546[int(25)] + _S600[int(25)];
        float _S626 = _S546[int(26)] + _S600[int(26)];
        float _S627 = _S546[int(27)] + _S600[int(27)];
        float _S628 = _S546[int(28)] + _S600[int(28)];
        float _S629 = _S546[int(29)] + _S600[int(29)];
        float _S630 = _S546[int(30)] + _S600[int(30)];
        float _S631 = _S546[int(31)] + _S600[int(31)];
        _S546[int(0)] = _S546[int(0)] + _S600[int(0)];
        _S546[int(1)] = _S601;
        _S546[int(2)] = _S602;
        _S546[int(3)] = _S603;
        _S546[int(4)] = _S604;
        _S546[int(5)] = _S605;
        _S546[int(6)] = _S606;
        _S546[int(7)] = _S607;
        _S546[int(8)] = _S608;
        _S546[int(9)] = _S609;
        _S546[int(10)] = _S610;
        _S546[int(11)] = _S611;
        _S546[int(12)] = _S612;
        _S546[int(13)] = _S613;
        _S546[int(14)] = _S614;
        _S546[int(15)] = _S615;
        _S546[int(16)] = _S616;
        _S546[int(17)] = _S617;
        _S546[int(18)] = _S618;
        _S546[int(19)] = _S619;
        _S546[int(20)] = _S620;
        _S546[int(21)] = _S621;
        _S546[int(22)] = _S622;
        _S546[int(23)] = _S623;
        _S546[int(24)] = _S624;
        _S546[int(25)] = _S625;
        _S546[int(26)] = _S626;
        _S546[int(27)] = _S627;
        _S546[int(28)] = _S628;
        _S546[int(29)] = _S629;
        _S546[int(30)] = _S630;
        _S546[int(31)] = _S631;
    }
    float _S632 = -10.0f * _S452;
    DiffPair_float_0 _S633;
    (&_S633)->primal_0 = _S402;
    (&_S633)->differential_0 = 0.0f;
    s_bwd_prop_log10_0(&_S633, _S632);
    float _S634 = _S633.differential_0 / _S401;
    float _S635 = _S400 * _S634;
    float _S636 = _S451 / _S401;
    float _S637 = _S400 * _S636;
    float _S638 = _S399[int(1)] * - _S634 + _S399[int(0)] * - _S636 + _S512;
    DiffPair_float_0 _S639;
    (&_S639)->primal_0 = _S399[int(21)];
    (&_S639)->differential_0 = 0.0f;
    DiffPair_float_0 _S640;
    (&_S640)->primal_0 = 1.0f;
    (&_S640)->differential_0 = 0.0f;
    _d_max_0(&_S639, &_S640, _S638);
    FixedArray<float, 32>  _S641;
    _S641[int(0)] = 0.0f;
    _S641[int(1)] = 0.0f;
    _S641[int(2)] = 0.0f;
    _S641[int(3)] = 0.0f;
    _S641[int(4)] = 0.0f;
    _S641[int(5)] = 0.0f;
    _S641[int(6)] = 0.0f;
    _S641[int(7)] = 0.0f;
    _S641[int(8)] = 0.0f;
    _S641[int(9)] = 0.0f;
    _S641[int(10)] = 0.0f;
    _S641[int(11)] = 0.0f;
    _S641[int(12)] = 0.0f;
    _S641[int(13)] = 0.0f;
    _S641[int(14)] = 0.0f;
    _S641[int(15)] = 0.0f;
    _S641[int(16)] = 0.0f;
    _S641[int(17)] = 0.0f;
    _S641[int(18)] = 0.0f;
    _S641[int(19)] = 0.0f;
    _S641[int(20)] = 0.0f;
    _S641[int(21)] = 0.0f;
    _S641[int(22)] = 0.0f;
    _S641[int(23)] = 0.0f;
    _S641[int(24)] = 0.0f;
    _S641[int(25)] = 0.0f;
    _S641[int(26)] = 0.0f;
    _S641[int(27)] = 0.0f;
    _S641[int(28)] = 0.0f;
    _S641[int(29)] = 0.0f;
    _S641[int(30)] = 0.0f;
    _S641[int(31)] = 0.0f;
    _S641[int(22)] = _S405;
    _S641[int(1)] = _S635;
    _S641[int(21)] = _S639.differential_0;
    _S641[int(0)] = _S637;
    FixedArray<float, 32>  _S642 = {
        _S546[int(0)] + _S641[int(0)], _S546[int(1)] + _S641[int(1)], _S546[int(2)] + _S641[int(2)], _S546[int(3)] + _S641[int(3)], _S546[int(4)] + _S641[int(4)], _S546[int(5)] + _S641[int(5)], _S546[int(6)] + _S641[int(6)], _S546[int(7)] + _S641[int(7)], _S546[int(8)] + _S641[int(8)], _S546[int(9)] + _S641[int(9)], _S546[int(10)] + _S641[int(10)], _S546[int(11)] + _S641[int(11)], _S546[int(12)] + _S641[int(12)], _S546[int(13)] + _S641[int(13)], _S546[int(14)] + _S641[int(14)], _S546[int(15)] + _S641[int(15)], _S546[int(16)] + _S641[int(16)], _S546[int(17)] + _S641[int(17)], _S546[int(18)] + _S641[int(18)], _S546[int(19)] + _S641[int(19)], _S546[int(20)] + _S641[int(20)], _S546[int(21)] + _S641[int(21)], _S546[int(22)] + _S641[int(22)], _S546[int(23)] + _S641[int(23)], _S546[int(24)] + _S641[int(24)], _S546[int(25)] + _S641[int(25)], _S546[int(26)] + _S641[int(26)], _S546[int(27)] + _S641[int(27)], _S546[int(28)] + _S641[int(28)], _S546[int(29)] + _S641[int(29)], _S546[int(30)] + _S641[int(30)], _S546[int(31)] + _S641[int(31)]
    };
    dpraw_losses_0->primal_0 = dpraw_losses_0->primal_0;
    dpraw_losses_0->differential_0 = _S642;
    return;
}

inline __device__ void s_bwd_per_pixel_losses_reduce_0(DiffPair_arrayx3Cfloatx2C32x3E_0 * _S643, FixedArray<float, 19>  * _S644, FixedArray<float, 14>  * _S645)
{
    s_bwd_prop_per_pixel_losses_reduce_0(_S643, _S644, _S645);
    return;
}

inline __device__ void per_pixel_losses_reduce_bwd(FixedArray<float, 32>  raw_losses_1, FixedArray<float, 19>  weights_5, FixedArray<float, 14>  v_losses_1, FixedArray<float, 32>  * _S646)
{
    FixedArray<float, 32>  _S647 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    DiffPair_arrayx3Cfloatx2C32x3E_0 dp_raw_losses_0;
    (&dp_raw_losses_0)->primal_0 = raw_losses_1;
    (&dp_raw_losses_0)->differential_0 = _S647;
    FixedArray<float, 19>  _S648 = weights_5;
    FixedArray<float, 14>  _S649 = v_losses_1;
    s_bwd_per_pixel_losses_reduce_0(&dp_raw_losses_0, &_S648, &_S649);
    *_S646 = (&dp_raw_losses_0)->differential_0;
    return;
}

