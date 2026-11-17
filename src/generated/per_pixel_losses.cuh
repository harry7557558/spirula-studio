#pragma once

#include "generated/slang.cuh"

struct DiffPair_float_0
{
    float primal_0;
    float differential_0;
};

inline __device__ void _d_min_0(DiffPair_float_0 * dpx_0, DiffPair_float_0 * dpy_0, float dOut_0)
{
    DiffPair_float_0 _S1 = *dpx_0;
    float _S2;
    if(((*dpx_0).primal_0) < ((*dpy_0).primal_0))
    {
        _S2 = dOut_0;
    }
    else
    {
        if(((*dpx_0).primal_0) > ((*dpy_0).primal_0))
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
    if(((*dpy_0).primal_0) < (_S1.primal_0))
    {
        _S2 = dOut_0;
    }
    else
    {
        if(((*dpy_0).primal_0) > ((*dpx_0).primal_0))
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

inline __device__ void _d_abs_0(DiffPair_float_0 * dpx_2, float dOut_2)
{
    float _S4 = _slang_select(((*dpx_2).primal_0) > 0.0f, 1.0f,_slang_select(((*dpx_2).primal_0) == 0.0f, 0.0f,-1.0f)) * dOut_2;
    dpx_2->primal_0 = (*dpx_2).primal_0;
    dpx_2->differential_0 = _S4;
    return;
}

inline __device__ void _d_abs_vector_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_3, float3  dOut_3)
{
    float3  _S5 = _slang_select(((*dpx_3).primal_0) > make_float3 (0.0f), make_float3 (1.0f),_slang_select(((*dpx_3).primal_0) == make_float3 (0.0f), make_float3 (0.0f),make_float3 (-1.0f))) * dOut_3;
    dpx_3->primal_0 = (*dpx_3).primal_0;
    dpx_3->differential_0 = _S5;
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

inline __device__ void _d_max_0(DiffPair_float_0 * dpx_4, DiffPair_float_0 * dpy_2, float dOut_4)
{
    DiffPair_float_0 _S6 = *dpx_4;
    float _S7;
    if(((*dpx_4).primal_0) > ((*dpy_2).primal_0))
    {
        _S7 = dOut_4;
    }
    else
    {
        if(((*dpx_4).primal_0) < ((*dpy_2).primal_0))
        {
            _S7 = 0.0f;
        }
        else
        {
            _S7 = 0.5f * dOut_4;
        }
    }
    dpx_4->primal_0 = _S6.primal_0;
    dpx_4->differential_0 = _S7;
    DiffPair_float_0 _S8 = *dpy_2;
    if(((*dpy_2).primal_0) > (_S6.primal_0))
    {
        _S7 = dOut_4;
    }
    else
    {
        if(((*dpy_2).primal_0) < ((*dpx_4).primal_0))
        {
            _S7 = 0.0f;
        }
        else
        {
            _S7 = 0.5f * dOut_4;
        }
    }
    dpy_2->primal_0 = _S8.primal_0;
    dpy_2->differential_0 = _S7;
    return;
}

inline __device__ void _d_clamp_0(DiffPair_float_0 * dpx_5, DiffPair_float_0 * dpMin_0, DiffPair_float_0 * dpMax_0, float dOut_5)
{
    DiffPair_float_0 _S9 = *dpx_5;
    bool _S10;
    if(((*dpx_5).primal_0) >= ((*dpMin_0).primal_0))
    {
        _S10 = ((*dpx_5).primal_0) <= ((*dpMax_0).primal_0);
    }
    else
    {
        _S10 = false;
    }
    float _S11;
    if(_S10)
    {
        _S11 = dOut_5;
    }
    else
    {
        _S11 = 0.0f;
    }
    dpx_5->primal_0 = _S9.primal_0;
    dpx_5->differential_0 = _S11;
    DiffPair_float_0 _S12 = *dpMin_0;
    if((_S9.primal_0) < ((*dpMin_0).primal_0))
    {
        _S11 = dOut_5;
    }
    else
    {
        _S11 = 0.0f;
    }
    dpMin_0->primal_0 = _S12.primal_0;
    dpMin_0->differential_0 = _S11;
    DiffPair_float_0 _S13 = *dpMax_0;
    if(((*dpx_5).primal_0) > ((*dpMax_0).primal_0))
    {
        _S11 = dOut_5;
    }
    else
    {
        _S11 = 0.0f;
    }
    dpMax_0->primal_0 = _S13.primal_0;
    dpMax_0->differential_0 = _S11;
    return;
}

inline __device__ float clamp_0(float x_2, float minBound_0, float maxBound_0)
{
    return (F32_min(((F32_max((x_2), (minBound_0)))), (maxBound_0)));
}

inline __device__ void _d_log_0(DiffPair_float_0 * dpx_6, float dOut_6)
{
    float _S14 = 1.0f / (*dpx_6).primal_0 * dOut_6;
    dpx_6->primal_0 = (*dpx_6).primal_0;
    dpx_6->differential_0 = _S14;
    return;
}

inline __device__ void _d_sqrt_0(DiffPair_float_0 * dpx_7, float dOut_7)
{
    float _S15 = 0.5f / (F32_sqrt(((F32_max((1.00000001168609742e-07f), ((*dpx_7).primal_0)))))) * dOut_7;
    dpx_7->primal_0 = (*dpx_7).primal_0;
    dpx_7->differential_0 = _S15;
    return;
}

inline __device__ void _d_rsqrt_0(DiffPair_float_0 * dpx_8, float dOut_8)
{
    float _S16 = -0.5f / ((*dpx_8).primal_0 * (F32_sqrt(((*dpx_8).primal_0)))) * dOut_8;
    dpx_8->primal_0 = (*dpx_8).primal_0;
    dpx_8->differential_0 = _S16;
    return;
}

inline __device__ void _d_lerp_0(DiffPair_float_0 * dpx_9, DiffPair_float_0 * dpy_3, DiffPair_float_0 * dps_0, float dOut_9)
{
    float _S17 = (1.0f - (*dps_0).primal_0) * dOut_9;
    dpx_9->primal_0 = (*dpx_9).primal_0;
    dpx_9->differential_0 = _S17;
    DiffPair_float_0 _S18 = *dpy_3;
    float _S19 = (*dps_0).primal_0 * dOut_9;
    dpy_3->primal_0 = (*dpy_3).primal_0;
    dpy_3->differential_0 = _S19;
    float _S20 = (_S18.primal_0 - (*dpx_9).primal_0) * dOut_9;
    dps_0->primal_0 = _S18.primal_0;
    dps_0->differential_0 = _S20;
    return;
}

inline __device__ float lerp_0(float x_3, float y_1, float s_0)
{
    return x_3 + (y_1 - x_3) * s_0;
}

inline __device__ void per_pixel_losses(float3  render_rgb_0, float3  ref_rgb_0, float render_depth_0, float ref_depth_0, float3  render_normal_0, float3  depth_normal_0, float3  ref_normal_0, float render_Ts_0, float3  rgb_dist_0, float depth_dist_0, float3  normal_dist_0, float median_depth_0, float3  median_normal_0, bool ref_alpha_0, bool has_mask_0, float saturation_threshold_0, FixedArray<float, 21>  weights_0, FixedArray<float, 33>  * _S21)
{
    bool _S22;
    bool _S23;
    bool _S24;
    FixedArray<float, 33>  losses_0;
    bool mask_0;
    if(has_mask_0)
    {
        mask_0 = ref_alpha_0;
    }
    else
    {
        mask_0 = true;
    }
    bool normal_mask_0;
    if(saturation_threshold_0 > 0.0f)
    {
        normal_mask_0 = (F32_min(((F32_min((render_rgb_0.x), (render_rgb_0.y)))), (render_rgb_0.z))) > saturation_threshold_0;
    }
    else
    {
        normal_mask_0 = false;
    }
    if(normal_mask_0)
    {
        normal_mask_0 = (F32_min(((F32_min((ref_rgb_0.x), (ref_rgb_0.y)))), (ref_rgb_0.z))) > saturation_threshold_0;
    }
    else
    {
        normal_mask_0 = false;
    }
    if(normal_mask_0)
    {
        mask_0 = false;
    }
    bool depth_mask_0 = ref_depth_0 != 0.0f;
    if((ref_normal_0.x + ref_normal_0.y + ref_normal_0.z) > -2.36599993705749512f)
    {
        normal_mask_0 = (dot_0(ref_normal_0, ref_normal_0)) > 0.25f;
    }
    else
    {
        normal_mask_0 = false;
    }
    float _S25 = render_rgb_0.x;
    float _S26 = render_rgb_0.y;
    float _S27 = render_rgb_0.z;
    float _S28 = ref_rgb_0.x;
    float _S29 = ref_rgb_0.y;
    float _S30 = ref_rgb_0.z;
    float dY_0 = 0.29899999499320984f * _S25 + 0.58700001239776611f * _S26 + 0.11400000005960464f * _S27 - (0.29899999499320984f * _S28 + 0.58700001239776611f * _S29 + 0.11400000005960464f * _S30);
    float dU_0 = -0.14712999761104584f * _S25 - 0.28885999321937561f * _S26 + 0.43599998950958252f * _S27 - (-0.14712999761104584f * _S28 - 0.28885999321937561f * _S29 + 0.43599998950958252f * _S30);
    float dV_0 = 0.61500000953674316f * _S25 - 0.51498997211456299f * _S26 - 0.10001000016927719f * _S27 - (0.61500000953674316f * _S28 - 0.51498997211456299f * _S29 - 0.10001000016927719f * _S30);
    float _S31 = float(mask_0);
    float3  _S32 = ref_rgb_0 - render_rgb_0;
    float3  _S33 = abs_0(_S32);
    float _S34 = dot_0(_S32, _S32) * 0.3333333432674408f;
    losses_0[int(0)] = _S31 * (weights_0[int(0)] * ((_S33.x + _S33.y + _S33.z) * 0.3333333432674408f) + weights_0[int(1)] * _S34 + weights_0[int(2)] * (F32_abs((dY_0))) + weights_0[int(3)] * dY_0 * dY_0 + weights_0[int(4)] * dU_0 * dU_0 + weights_0[int(5)] * dV_0 * dV_0);
    losses_0[int(1)] = _S31 * clamp_0(_S34, 0.0f, 1.0f);
    bool depth_mask_1 = depth_mask_0 & mask_0;
    bool metric_mask_0;
    if(depth_mask_1)
    {
        metric_mask_0 = render_depth_0 > 0.00100000004749745f;
    }
    else
    {
        metric_mask_0 = false;
    }
    float3  _S35;
    float log_ratio_0 = ((F32_log(((F32_max((render_depth_0), (0.00100000004749745f)))))) - (F32_log(((F32_max((ref_depth_0), (0.00100000004749745f)))))) - weights_0[int(8)]) / 0.02999999932944775f;
    losses_0[int(7)] = weights_0[int(7)] * float(metric_mask_0) * 0.01499999966472387f * (F32_log((1.0f + log_ratio_0 * log_ratio_0)));
    float _S36 = float(depth_mask_1);
    float _S37 = _S36 * (F32_max((render_depth_0), (0.00009999999747379f)));
    float _S38 = _S36 * (F32_max((ref_depth_0), (0.00009999999747379f)));
    losses_0[int(2)] = _S37;
    losses_0[int(3)] = _S38;
    losses_0[int(4)] = _S37 * _S37;
    losses_0[int(5)] = _S38 * _S38;
    losses_0[int(6)] = _S37 * _S38;
    bool _S39 = normal_mask_0 & mask_0;
    for(;;)
    {
        float norm2_0 = dot_0(render_normal_0, render_normal_0);
        bool _S40 = norm2_0 == 0.0f;
        _S22 = _S40;
        if(_S40)
        {
            _S35 = make_float3 (0.0f);
            break;
        }
        _S35 = render_normal_0 * make_float3 ((F32_rsqrt((norm2_0))));
        break;
    }
    float3  _S41;
    bool _S42 = !_S22;
    for(;;)
    {
        float norm2_1 = dot_0(depth_normal_0, depth_normal_0);
        bool _S43 = norm2_1 == 0.0f;
        _S23 = _S43;
        if(_S43)
        {
            _S41 = make_float3 (0.0f);
            break;
        }
        _S41 = depth_normal_0 * make_float3 ((F32_rsqrt((norm2_1))));
        break;
    }
    float3  _S44;
    bool _S45 = !_S23;
    for(;;)
    {
        float norm2_2 = dot_0(ref_normal_0, ref_normal_0);
        if(norm2_2 == 0.0f)
        {
            _S44 = make_float3 (0.0f);
            normal_mask_0 = false;
            break;
        }
        _S44 = ref_normal_0 * make_float3 ((F32_rsqrt((norm2_2))));
        normal_mask_0 = _S39;
        break;
    }
    float3  _S46;
    float _S47 = float(_S42 & normal_mask_0);
    float cos_sim_loss_0 = 0.5f - 0.5f * dot_0(_S35, _S44);
    losses_0[int(8)] = weights_0[int(9)] * _S47 * (cos_sim_loss_0 + (F32_sqrt(((F32_max((cos_sim_loss_0), (9.999999960041972e-13f)))))));
    float _S48 = float(_S45 & normal_mask_0);
    float cos_sim_loss_1 = 0.5f - 0.5f * dot_0(_S41, _S44);
    losses_0[int(9)] = weights_0[int(9)] * _S48 * (cos_sim_loss_1 + (F32_sqrt(((F32_max((cos_sim_loss_1), (9.999999960041972e-13f)))))));
    float _S49 = float((_S42 & _S45) & mask_0);
    float cos_sim_loss_2 = 0.5f - 0.5f * dot_0(_S35, _S41);
    losses_0[int(12)] = weights_0[int(12)] * _S49 * (cos_sim_loss_2 + (F32_sqrt(((F32_max((cos_sim_loss_2), (9.999999960041972e-13f)))))));
    for(;;)
    {
        float norm2_3 = dot_0(median_normal_0, median_normal_0);
        bool _S50 = norm2_3 == 0.0f;
        _S24 = _S50;
        if(_S50)
        {
            _S46 = make_float3 (0.0f);
            break;
        }
        _S46 = median_normal_0 * make_float3 ((F32_rsqrt((norm2_3))));
        break;
    }
    bool _S51 = !_S24;
    if(mask_0)
    {
        metric_mask_0 = render_depth_0 > 1.00000001335143196e-10f;
    }
    else
    {
        metric_mask_0 = false;
    }
    bool mean_median_mask_0;
    if(metric_mask_0)
    {
        mean_median_mask_0 = median_depth_0 > 1.00000001335143196e-10f;
    }
    else
    {
        mean_median_mask_0 = false;
    }
    float _S52 = float(mean_median_mask_0);
    losses_0[int(17)] = weights_0[int(17)] * _S52 * (F32_abs(((F32_log(((F32_max((render_depth_0), (1.00000001335143196e-10f)))))) - (F32_log(((F32_max((median_depth_0), (1.00000001335143196e-10f)))))))));
    float _S53 = float((_S51 & _S45) & mask_0);
    float cos_sim_loss_3 = 0.5f - 0.5f * dot_0(_S46, _S41);
    losses_0[int(18)] = weights_0[int(18)] * _S53 * (cos_sim_loss_3 + (F32_sqrt(((F32_max((cos_sim_loss_3), (9.999999960041972e-13f)))))));
    float _S54 = float(_S51 & normal_mask_0);
    float cos_sim_loss_4 = 0.5f - 0.5f * dot_0(_S46, _S44);
    losses_0[int(19)] = weights_0[int(19)] * _S54 * (cos_sim_loss_4 + (F32_sqrt(((F32_max((cos_sim_loss_4), (9.999999960041972e-13f)))))));
    float _S55 = float((_S51 & _S42) & mask_0);
    float cos_sim_loss_5 = 0.5f - 0.5f * dot_0(_S46, _S35);
    losses_0[int(20)] = weights_0[int(20)] * _S55 * (cos_sim_loss_5 + (F32_sqrt(((F32_max((cos_sim_loss_5), (9.999999960041972e-13f)))))));
    float render_alpha_0 = clamp_0(1.0f - render_Ts_0, 0.0f, 1.0f);
    float _S56 = float(has_mask_0);
    float _S57 = float(ref_alpha_0);
    float _S58 = (F32_max((render_alpha_0), (_S57)));
    losses_0[int(10)] = weights_0[int(10)] * _S56 * - lerp_0((F32_log(((F32_max((1.0f - _S58), (9.99999997475242708e-07f)))))), (F32_log(((F32_max((_S58), (9.99999997475242708e-07f)))))), _S57);
    float _S59 = 1.0f - render_alpha_0;
    float _S60 = 1.0f - _S57;
    float _S61 = (F32_max((_S59), (_S60)));
    losses_0[int(11)] = weights_0[int(11)] * _S56 * - lerp_0((F32_log(((F32_max((1.0f - _S61), (9.99999997475242708e-07f)))))), (F32_log(((F32_max((_S61), (9.99999997475242708e-07f)))))), _S60);
    losses_0[int(13)] = weights_0[int(13)] * _S31 * 4.0f * render_alpha_0 * _S59;
    losses_0[int(14)] = weights_0[int(14)] * _S31 * ((rgb_dist_0.x + rgb_dist_0.y + rgb_dist_0.z) * 0.3333333432674408f);
    losses_0[int(15)] = weights_0[int(15)] * _S31 * depth_dist_0;
    losses_0[int(16)] = weights_0[int(16)] * _S31 * ((normal_dist_0.x + normal_dist_0.y + normal_dist_0.z) * 0.3333333432674408f);
    losses_0[int(21)] = 1.0f;
    losses_0[int(22)] = _S31;
    losses_0[int(23)] = _S36;
    losses_0[int(24)] = _S47;
    losses_0[int(25)] = _S48;
    losses_0[int(26)] = _S49;
    if(has_mask_0)
    {
        mask_0 = !ref_alpha_0;
    }
    else
    {
        mask_0 = false;
    }
    losses_0[int(27)] = float(mask_0);
    if(has_mask_0)
    {
        mask_0 = ref_alpha_0;
    }
    else
    {
        mask_0 = false;
    }
    losses_0[int(28)] = float(mask_0);
    losses_0[int(29)] = _S52;
    losses_0[int(30)] = _S53;
    losses_0[int(31)] = _S54;
    losses_0[int(32)] = _S55;
    *_S21 = losses_0;
    return;
}

inline __device__ float s_primal_ctx_dot_0(float3  _S62, float3  _S63)
{
    return dot_0(_S62, _S63);
}

inline __device__ float s_primal_ctx_log_0(float _S64)
{
    return (F32_log((_S64)));
}

inline __device__ float s_primal_ctx_rsqrt_0(float _S65)
{
    return (F32_rsqrt((_S65)));
}

inline __device__ float s_primal_ctx_clamp_0(float _S66, float _S67, float _S68)
{
    return clamp_0(_S66, _S67, _S68);
}

inline __device__ void s_bwd_prop_lerp_0(DiffPair_float_0 * _S69, DiffPair_float_0 * _S70, DiffPair_float_0 * _S71, float _S72)
{
    _d_lerp_0(_S69, _S70, _S71, _S72);
    return;
}

inline __device__ void s_bwd_prop_log_0(DiffPair_float_0 * _S73, float _S74)
{
    _d_log_0(_S73, _S74);
    return;
}

inline __device__ void s_bwd_prop_clamp_0(DiffPair_float_0 * _S75, DiffPair_float_0 * _S76, DiffPair_float_0 * _S77, float _S78)
{
    _d_clamp_0(_S75, _S76, _S77, _S78);
    return;
}

inline __device__ void s_bwd_prop_sqrt_0(DiffPair_float_0 * _S79, float _S80)
{
    _d_sqrt_0(_S79, _S80);
    return;
}

inline __device__ void s_bwd_prop_dot_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S81, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S82, float _S83)
{
    _d_dot_0(_S81, _S82, _S83);
    return;
}

inline __device__ void s_bwd_prop_abs_0(DiffPair_float_0 * _S84, float _S85)
{
    _d_abs_0(_S84, _S85);
    return;
}

inline __device__ void s_bwd_prop_rsqrt_0(DiffPair_float_0 * _S86, float _S87)
{
    _d_rsqrt_0(_S86, _S87);
    return;
}

inline __device__ void s_bwd_prop_abs_1(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S88, float3  _S89)
{
    _d_abs_vector_0(_S88, _S89);
    return;
}

inline __device__ void s_bwd_prop_per_pixel_losses_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dprender_rgb_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpref_rgb_0, DiffPair_float_0 * dprender_depth_0, DiffPair_float_0 * dpref_depth_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dprender_normal_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpdepth_normal_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpref_normal_0, DiffPair_float_0 * dprender_Ts_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dprgb_dist_0, DiffPair_float_0 * dpdepth_dist_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpnormal_dist_0, DiffPair_float_0 * dpmedian_depth_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpmedian_normal_0, bool ref_alpha_1, bool has_mask_1, float saturation_threshold_1, FixedArray<float, 21>  * weights_1, FixedArray<float, 33>  * _s_dOut_0)
{
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S90 = *dprender_rgb_0;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S91 = *dpref_rgb_0;
    DiffPair_float_0 _S92 = *dprender_depth_0;
    DiffPair_float_0 _S93 = *dpref_depth_0;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S94 = *dprender_normal_0;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S95 = *dpdepth_normal_0;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S96 = *dpref_normal_0;
    DiffPair_float_0 _S97 = *dprender_Ts_0;
    DiffPair_float_0 _S98 = *dpmedian_depth_0;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S99 = *dpmedian_normal_0;
    float3  _S100 = make_float3 (0.0f);
    bool mask_1;
    if(has_mask_1)
    {
        mask_1 = ref_alpha_1;
    }
    else
    {
        mask_1 = true;
    }
    bool _S101 = saturation_threshold_1 > 0.0f;
    bool _S102;
    float _S103;
    float _S104;
    float _S105;
    float _S106;
    if(_S101)
    {
        float _S107 = _S90.primal_0.x;
        float _S108 = _S90.primal_0.y;
        float _S109 = (F32_min((_S107), (_S108)));
        float _S110 = _S90.primal_0.z;
        _S102 = (F32_min((_S109), (_S110))) > saturation_threshold_1;
        _S103 = _S109;
        _S104 = _S110;
        _S105 = _S107;
        _S106 = _S108;
    }
    else
    {
        _S102 = false;
        _S103 = 0.0f;
        _S104 = 0.0f;
        _S105 = 0.0f;
        _S106 = 0.0f;
    }
    bool normal_mask_1;
    float _S111;
    float _S112;
    float _S113;
    float _S114;
    if(_S102)
    {
        float _S115 = _S91.primal_0.x;
        float _S116 = _S91.primal_0.y;
        float _S117 = (F32_min((_S115), (_S116)));
        float _S118 = _S91.primal_0.z;
        normal_mask_1 = (F32_min((_S117), (_S118))) > saturation_threshold_1;
        _S111 = _S117;
        _S112 = _S118;
        _S113 = _S115;
        _S114 = _S116;
    }
    else
    {
        normal_mask_1 = false;
        _S111 = 0.0f;
        _S112 = 0.0f;
        _S113 = 0.0f;
        _S114 = 0.0f;
    }
    if(normal_mask_1)
    {
        mask_1 = false;
    }
    bool depth_mask_2 = (_S93.primal_0) != 0.0f;
    bool _S119 = (_S96.primal_0.x + _S96.primal_0.y + _S96.primal_0.z) > -2.36599993705749512f;
    if(_S119)
    {
        normal_mask_1 = (s_primal_ctx_dot_0(_S96.primal_0, _S96.primal_0)) > 0.25f;
    }
    else
    {
        normal_mask_1 = false;
    }
    float _S120 = _S90.primal_0.x;
    float _S121 = _S90.primal_0.y;
    float _S122 = _S90.primal_0.z;
    float _S123 = _S91.primal_0.x;
    float _S124 = _S91.primal_0.y;
    float _S125 = _S91.primal_0.z;
    float dY_1 = 0.29899999499320984f * _S120 + 0.58700001239776611f * _S121 + 0.11400000005960464f * _S122 - (0.29899999499320984f * _S123 + 0.58700001239776611f * _S124 + 0.11400000005960464f * _S125);
    float dU_1 = -0.14712999761104584f * _S120 - 0.28885999321937561f * _S121 + 0.43599998950958252f * _S122 - (-0.14712999761104584f * _S123 - 0.28885999321937561f * _S124 + 0.43599998950958252f * _S125);
    float dV_1 = 0.61500000953674316f * _S120 - 0.51498997211456299f * _S121 - 0.10001000016927719f * _S122 - (0.61500000953674316f * _S123 - 0.51498997211456299f * _S124 - 0.10001000016927719f * _S125);
    float _S126 = float(mask_1);
    float _S127 = (*weights_1)[int(0)];
    float3  _S128 = _S91.primal_0 - _S90.primal_0;
    float _S129 = (*weights_1)[int(1)];
    float _S130 = s_primal_ctx_dot_0(_S128, _S128) * 0.3333333432674408f;
    float _S131 = (*weights_1)[int(2)];
    float _S132 = (*weights_1)[int(3)];
    float _S133 = (*weights_1)[int(3)] * dY_1;
    float _S134 = (*weights_1)[int(4)];
    float _S135 = (*weights_1)[int(4)] * dU_1;
    float _S136 = (*weights_1)[int(5)];
    float _S137 = (*weights_1)[int(5)] * dV_1;
    bool depth_mask_3 = depth_mask_2 & mask_1;
    bool metric_mask_1;
    if(depth_mask_3)
    {
        metric_mask_1 = (_S92.primal_0) > 0.00100000004749745f;
    }
    else
    {
        metric_mask_1 = false;
    }
    float _S138 = (F32_max((_S92.primal_0), (0.00100000004749745f)));
    float _S139 = (F32_max((_S93.primal_0), (0.00100000004749745f)));
    float log_ratio_1 = (s_primal_ctx_log_0(_S138) - s_primal_ctx_log_0(_S139) - (*weights_1)[int(8)]) / 0.02999999932944775f;
    float _S140 = (*weights_1)[int(7)] * float(metric_mask_1) * 0.01499999966472387f;
    float _S141 = 1.0f + log_ratio_1 * log_ratio_1;
    float _S142 = float(depth_mask_3);
    float _S143 = _S142 * (F32_max((_S92.primal_0), (0.00009999999747379f)));
    float _S144 = _S142 * (F32_max((_S93.primal_0), (0.00009999999747379f)));
    bool _S145 = normal_mask_1 & mask_1;
    float _S146 = s_primal_ctx_dot_0(_S94.primal_0, _S94.primal_0);
    bool _S147 = _S146 == 0.0f;
    float3  _S148;
    if(_S147)
    {
        _S148 = make_float3 (0.0f);
    }
    bool _S149 = !_S147;
    float3  _S150;
    if(_S149)
    {
        float _S151 = s_primal_ctx_rsqrt_0(_S146);
        float3  _S152 = make_float3 (_S151);
        _S148 = _S94.primal_0 * make_float3 (_S151);
        _S150 = _S152;
    }
    else
    {
        _S150 = _S100;
    }
    float _S153 = s_primal_ctx_dot_0(_S95.primal_0, _S95.primal_0);
    bool _S154 = _S153 == 0.0f;
    float3  _S155;
    if(_S154)
    {
        _S155 = make_float3 (0.0f);
    }
    bool _S156 = !_S154;
    float3  _S157;
    if(_S156)
    {
        float _S158 = s_primal_ctx_rsqrt_0(_S153);
        float3  _S159 = make_float3 (_S158);
        _S155 = _S95.primal_0 * make_float3 (_S158);
        _S157 = _S159;
    }
    else
    {
        _S157 = _S100;
    }
    float _S160 = s_primal_ctx_dot_0(_S96.primal_0, _S96.primal_0);
    bool _S161 = _S160 == 0.0f;
    float3  _S162;
    if(_S161)
    {
        float3  _S163 = make_float3 (0.0f);
        normal_mask_1 = false;
        _S162 = _S163;
    }
    else
    {
        normal_mask_1 = _S145;
    }
    bool _S164 = !_S161;
    float3  _S165;
    if(_S164)
    {
        float _S166 = s_primal_ctx_rsqrt_0(_S160);
        float3  _S167 = make_float3 (_S166);
        _S162 = _S96.primal_0 * make_float3 (_S166);
        _S165 = _S167;
    }
    else
    {
        _S165 = _S100;
    }
    float _S168 = (*weights_1)[int(9)] * float(_S149 & normal_mask_1);
    float cos_sim_loss_6 = 0.5f - 0.5f * s_primal_ctx_dot_0(_S148, _S162);
    float _S169 = (F32_max((cos_sim_loss_6), (9.999999960041972e-13f)));
    float _S170 = (*weights_1)[int(9)] * float(_S156 & normal_mask_1);
    float cos_sim_loss_7 = 0.5f - 0.5f * s_primal_ctx_dot_0(_S155, _S162);
    float _S171 = (F32_max((cos_sim_loss_7), (9.999999960041972e-13f)));
    float _S172 = (*weights_1)[int(12)] * float((_S149 & _S156) & mask_1);
    float cos_sim_loss_8 = 0.5f - 0.5f * s_primal_ctx_dot_0(_S148, _S155);
    float _S173 = (F32_max((cos_sim_loss_8), (9.999999960041972e-13f)));
    float _S174 = s_primal_ctx_dot_0(_S99.primal_0, _S99.primal_0);
    bool _S175 = _S174 == 0.0f;
    float3  _S176;
    if(_S175)
    {
        _S176 = make_float3 (0.0f);
    }
    bool _S177 = !_S175;
    float3  _S178;
    if(_S177)
    {
        float _S179 = s_primal_ctx_rsqrt_0(_S174);
        float3  _S180 = make_float3 (_S179);
        _S176 = _S99.primal_0 * make_float3 (_S179);
        _S178 = _S180;
    }
    else
    {
        _S178 = _S100;
    }
    if(mask_1)
    {
        metric_mask_1 = (_S92.primal_0) > 1.00000001335143196e-10f;
    }
    else
    {
        metric_mask_1 = false;
    }
    bool mean_median_mask_1;
    if(metric_mask_1)
    {
        mean_median_mask_1 = (_S98.primal_0) > 1.00000001335143196e-10f;
    }
    else
    {
        mean_median_mask_1 = false;
    }
    float _S181 = (*weights_1)[int(17)] * float(mean_median_mask_1);
    float _S182 = (F32_max((_S92.primal_0), (1.00000001335143196e-10f)));
    float _S183 = (F32_max((_S98.primal_0), (1.00000001335143196e-10f)));
    float _S184 = s_primal_ctx_log_0(_S182) - s_primal_ctx_log_0(_S183);
    float _S185 = (*weights_1)[int(18)] * float((_S177 & _S156) & mask_1);
    float cos_sim_loss_9 = 0.5f - 0.5f * s_primal_ctx_dot_0(_S176, _S155);
    float _S186 = (F32_max((cos_sim_loss_9), (9.999999960041972e-13f)));
    float _S187 = (*weights_1)[int(19)] * float(_S177 & normal_mask_1);
    float cos_sim_loss_10 = 0.5f - 0.5f * s_primal_ctx_dot_0(_S176, _S162);
    float _S188 = (F32_max((cos_sim_loss_10), (9.999999960041972e-13f)));
    float _S189 = (*weights_1)[int(20)] * float((_S177 & _S149) & mask_1);
    float cos_sim_loss_11 = 0.5f - 0.5f * s_primal_ctx_dot_0(_S176, _S148);
    float _S190 = (F32_max((cos_sim_loss_11), (9.999999960041972e-13f)));
    float _S191 = 1.0f - _S97.primal_0;
    float _S192 = s_primal_ctx_clamp_0(_S191, 0.0f, 1.0f);
    float _S193 = float(has_mask_1);
    float _S194 = (*weights_1)[int(10)] * _S193;
    float _S195 = float(ref_alpha_1);
    float _S196 = (F32_max((_S192), (_S195)));
    float _S197 = 1.0f - _S196;
    float _S198 = (F32_max((_S197), (9.99999997475242708e-07f)));
    float _S199 = s_primal_ctx_log_0(_S198);
    float _S200 = (F32_max((_S196), (9.99999997475242708e-07f)));
    float _S201 = s_primal_ctx_log_0(_S200);
    float _S202 = (*weights_1)[int(11)] * _S193;
    float _S203 = 1.0f - _S192;
    float _S204 = 1.0f - _S195;
    float _S205 = (F32_max((_S203), (_S204)));
    float _S206 = 1.0f - _S205;
    float _S207 = (F32_max((_S206), (9.99999997475242708e-07f)));
    float _S208 = s_primal_ctx_log_0(_S207);
    float _S209 = (F32_max((_S205), (9.99999997475242708e-07f)));
    float _S210 = s_primal_ctx_log_0(_S209);
    float _S211 = (*weights_1)[int(13)] * _S126 * 4.0f;
    float _S212 = _S211 * _S192;
    float _S213 = (*weights_1)[int(14)] * _S126;
    float _S214 = (*weights_1)[int(15)] * _S126;
    float _S215 = (*weights_1)[int(16)] * _S126;
    float _S216 = (*_s_dOut_0)[int(0)];
    float _S217 = (*_s_dOut_0)[int(1)];
    float _S218 = (*_s_dOut_0)[int(2)];
    float _S219 = (*_s_dOut_0)[int(3)];
    float _S220 = (*_s_dOut_0)[int(4)];
    float _S221 = (*_s_dOut_0)[int(5)];
    float _S222 = (*_s_dOut_0)[int(6)];
    float _S223 = (*_s_dOut_0)[int(7)];
    float _S224 = (*_s_dOut_0)[int(8)];
    float _S225 = (*_s_dOut_0)[int(9)];
    float _S226 = (*_s_dOut_0)[int(12)];
    float _S227 = 0.3333333432674408f * (_S215 * (*_s_dOut_0)[int(16)]);
    float _S228 = _S214 * (*_s_dOut_0)[int(15)];
    float _S229 = 0.3333333432674408f * (_S213 * (*_s_dOut_0)[int(14)]);
    float _S230 = _S212 * (*_s_dOut_0)[int(13)];
    float _S231 = _S211 * (_S203 * (*_s_dOut_0)[int(13)]);
    float _S232 = - (_S202 * (*_s_dOut_0)[int(11)]);
    DiffPair_float_0 _S233;
    (&_S233)->primal_0 = _S208;
    (&_S233)->differential_0 = 0.0f;
    DiffPair_float_0 _S234;
    (&_S234)->primal_0 = _S210;
    (&_S234)->differential_0 = 0.0f;
    DiffPair_float_0 _S235;
    (&_S235)->primal_0 = _S204;
    (&_S235)->differential_0 = 0.0f;
    s_bwd_prop_lerp_0(&_S233, &_S234, &_S235, _S232);
    DiffPair_float_0 _S236;
    (&_S236)->primal_0 = _S209;
    (&_S236)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S236, _S234.differential_0);
    DiffPair_float_0 _S237;
    (&_S237)->primal_0 = _S205;
    (&_S237)->differential_0 = 0.0f;
    DiffPair_float_0 _S238;
    (&_S238)->primal_0 = 9.99999997475242708e-07f;
    (&_S238)->differential_0 = 0.0f;
    _d_max_0(&_S237, &_S238, _S236.differential_0);
    DiffPair_float_0 _S239;
    (&_S239)->primal_0 = _S207;
    (&_S239)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S239, _S233.differential_0);
    DiffPair_float_0 _S240;
    (&_S240)->primal_0 = _S206;
    (&_S240)->differential_0 = 0.0f;
    DiffPair_float_0 _S241;
    (&_S241)->primal_0 = 9.99999997475242708e-07f;
    (&_S241)->differential_0 = 0.0f;
    _d_max_0(&_S240, &_S241, _S239.differential_0);
    float _S242 = _S237.differential_0 + - _S240.differential_0;
    DiffPair_float_0 _S243;
    (&_S243)->primal_0 = _S203;
    (&_S243)->differential_0 = 0.0f;
    DiffPair_float_0 _S244;
    (&_S244)->primal_0 = _S204;
    (&_S244)->differential_0 = 0.0f;
    _d_max_0(&_S243, &_S244, _S242);
    float _S245 = - (_S230 + _S243.differential_0);
    float _S246 = - (_S194 * (*_s_dOut_0)[int(10)]);
    DiffPair_float_0 _S247;
    (&_S247)->primal_0 = _S199;
    (&_S247)->differential_0 = 0.0f;
    DiffPair_float_0 _S248;
    (&_S248)->primal_0 = _S201;
    (&_S248)->differential_0 = 0.0f;
    DiffPair_float_0 _S249;
    (&_S249)->primal_0 = _S195;
    (&_S249)->differential_0 = 0.0f;
    s_bwd_prop_lerp_0(&_S247, &_S248, &_S249, _S246);
    DiffPair_float_0 _S250;
    (&_S250)->primal_0 = _S200;
    (&_S250)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S250, _S248.differential_0);
    DiffPair_float_0 _S251;
    (&_S251)->primal_0 = _S196;
    (&_S251)->differential_0 = 0.0f;
    DiffPair_float_0 _S252;
    (&_S252)->primal_0 = 9.99999997475242708e-07f;
    (&_S252)->differential_0 = 0.0f;
    _d_max_0(&_S251, &_S252, _S250.differential_0);
    DiffPair_float_0 _S253;
    (&_S253)->primal_0 = _S198;
    (&_S253)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S253, _S247.differential_0);
    DiffPair_float_0 _S254;
    (&_S254)->primal_0 = _S197;
    (&_S254)->differential_0 = 0.0f;
    DiffPair_float_0 _S255;
    (&_S255)->primal_0 = 9.99999997475242708e-07f;
    (&_S255)->differential_0 = 0.0f;
    _d_max_0(&_S254, &_S255, _S253.differential_0);
    float _S256 = _S251.differential_0 + - _S254.differential_0;
    DiffPair_float_0 _S257;
    (&_S257)->primal_0 = _S192;
    (&_S257)->differential_0 = 0.0f;
    DiffPair_float_0 _S258;
    (&_S258)->primal_0 = _S195;
    (&_S258)->differential_0 = 0.0f;
    _d_max_0(&_S257, &_S258, _S256);
    float _S259 = _S231 + _S245 + _S257.differential_0;
    DiffPair_float_0 _S260;
    (&_S260)->primal_0 = _S191;
    (&_S260)->differential_0 = 0.0f;
    DiffPair_float_0 _S261;
    (&_S261)->primal_0 = 0.0f;
    (&_S261)->differential_0 = 0.0f;
    DiffPair_float_0 _S262;
    (&_S262)->primal_0 = 1.0f;
    (&_S262)->differential_0 = 0.0f;
    s_bwd_prop_clamp_0(&_S260, &_S261, &_S262, _S259);
    float _S263 = - _S260.differential_0;
    float _S264 = _S189 * (*_s_dOut_0)[int(20)];
    DiffPair_float_0 _S265;
    (&_S265)->primal_0 = _S190;
    (&_S265)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S265, _S264);
    DiffPair_float_0 _S266;
    (&_S266)->primal_0 = cos_sim_loss_11;
    (&_S266)->differential_0 = 0.0f;
    DiffPair_float_0 _S267;
    (&_S267)->primal_0 = 9.999999960041972e-13f;
    (&_S267)->differential_0 = 0.0f;
    _d_max_0(&_S266, &_S267, _S265.differential_0);
    float _S268 = 0.5f * - (_S264 + _S266.differential_0);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S269;
    (&_S269)->primal_0 = _S176;
    (&_S269)->differential_0 = _S100;
    float3  _S270 = _S148;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S271;
    (&_S271)->primal_0 = _S148;
    (&_S271)->differential_0 = _S100;
    s_bwd_prop_dot_0(&_S269, &_S271, _S268);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S272 = _S271;
    float _S273 = _S187 * (*_s_dOut_0)[int(19)];
    DiffPair_float_0 _S274;
    (&_S274)->primal_0 = _S188;
    (&_S274)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S274, _S273);
    DiffPair_float_0 _S275;
    (&_S275)->primal_0 = cos_sim_loss_10;
    (&_S275)->differential_0 = 0.0f;
    DiffPair_float_0 _S276;
    (&_S276)->primal_0 = 9.999999960041972e-13f;
    (&_S276)->differential_0 = 0.0f;
    _d_max_0(&_S275, &_S276, _S274.differential_0);
    float _S277 = 0.5f * - (_S273 + _S275.differential_0);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S278;
    (&_S278)->primal_0 = _S176;
    (&_S278)->differential_0 = _S100;
    float3  _S279 = _S162;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S280;
    (&_S280)->primal_0 = _S162;
    (&_S280)->differential_0 = _S100;
    s_bwd_prop_dot_0(&_S278, &_S280, _S277);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S281 = _S280;
    float _S282 = _S185 * (*_s_dOut_0)[int(18)];
    DiffPair_float_0 _S283;
    (&_S283)->primal_0 = _S186;
    (&_S283)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S283, _S282);
    DiffPair_float_0 _S284;
    (&_S284)->primal_0 = cos_sim_loss_9;
    (&_S284)->differential_0 = 0.0f;
    DiffPair_float_0 _S285;
    (&_S285)->primal_0 = 9.999999960041972e-13f;
    (&_S285)->differential_0 = 0.0f;
    _d_max_0(&_S284, &_S285, _S283.differential_0);
    float _S286 = 0.5f * - (_S282 + _S284.differential_0);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S287;
    (&_S287)->primal_0 = _S176;
    (&_S287)->differential_0 = _S100;
    float3  _S288 = _S155;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S289;
    (&_S289)->primal_0 = _S155;
    (&_S289)->differential_0 = _S100;
    s_bwd_prop_dot_0(&_S287, &_S289, _S286);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S290 = _S289;
    float _S291 = _S181 * (*_s_dOut_0)[int(17)];
    DiffPair_float_0 _S292;
    (&_S292)->primal_0 = _S184;
    (&_S292)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S292, _S291);
    float _S293 = - _S292.differential_0;
    DiffPair_float_0 _S294;
    (&_S294)->primal_0 = _S183;
    (&_S294)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S294, _S293);
    DiffPair_float_0 _S295;
    (&_S295)->primal_0 = _S98.primal_0;
    (&_S295)->differential_0 = 0.0f;
    DiffPair_float_0 _S296;
    (&_S296)->primal_0 = 1.00000001335143196e-10f;
    (&_S296)->differential_0 = 0.0f;
    _d_max_0(&_S295, &_S296, _S294.differential_0);
    DiffPair_float_0 _S297 = _S295;
    DiffPair_float_0 _S298;
    (&_S298)->primal_0 = _S182;
    (&_S298)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S298, _S292.differential_0);
    DiffPair_float_0 _S299;
    (&_S299)->primal_0 = _S92.primal_0;
    (&_S299)->differential_0 = 0.0f;
    DiffPair_float_0 _S300;
    (&_S300)->primal_0 = 1.00000001335143196e-10f;
    (&_S300)->differential_0 = 0.0f;
    _d_max_0(&_S299, &_S300, _S298.differential_0);
    DiffPair_float_0 _S301 = _S299;
    float3  _S302 = make_float3 (_S227, _S227, _S227);
    float3  _S303 = make_float3 (_S229, _S229, _S229);
    float3  _S304 = _S269.differential_0 + _S278.differential_0 + _S287.differential_0;
    float _S305;
    if(_S177)
    {
        float3  _S306 = _S99.primal_0 * _S304;
        float3  _S307 = _S178 * _S304;
        float _S308 = _S306.x + _S306.y + _S306.z;
        DiffPair_float_0 _S309;
        (&_S309)->primal_0 = _S174;
        (&_S309)->differential_0 = 0.0f;
        s_bwd_prop_rsqrt_0(&_S309, _S308);
        _S305 = _S309.differential_0;
        _S148 = _S307;
    }
    else
    {
        _S305 = 0.0f;
        _S148 = _S100;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S310;
    (&_S310)->primal_0 = _S99.primal_0;
    (&_S310)->differential_0 = _S100;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S311;
    (&_S311)->primal_0 = _S99.primal_0;
    (&_S311)->differential_0 = _S100;
    s_bwd_prop_dot_0(&_S310, &_S311, _S305);
    float _S312 = _S172 * _S226;
    DiffPair_float_0 _S313;
    (&_S313)->primal_0 = _S173;
    (&_S313)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S313, _S312);
    DiffPair_float_0 _S314;
    (&_S314)->primal_0 = cos_sim_loss_8;
    (&_S314)->differential_0 = 0.0f;
    DiffPair_float_0 _S315;
    (&_S315)->primal_0 = 9.999999960041972e-13f;
    (&_S315)->differential_0 = 0.0f;
    _d_max_0(&_S314, &_S315, _S313.differential_0);
    float _S316 = 0.5f * - (_S312 + _S314.differential_0);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S317;
    (&_S317)->primal_0 = _S270;
    (&_S317)->differential_0 = _S100;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S318;
    (&_S318)->primal_0 = _S288;
    (&_S318)->differential_0 = _S100;
    s_bwd_prop_dot_0(&_S317, &_S318, _S316);
    float _S319 = _S170 * _S225;
    DiffPair_float_0 _S320;
    (&_S320)->primal_0 = _S171;
    (&_S320)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S320, _S319);
    DiffPair_float_0 _S321;
    (&_S321)->primal_0 = cos_sim_loss_7;
    (&_S321)->differential_0 = 0.0f;
    DiffPair_float_0 _S322;
    (&_S322)->primal_0 = 9.999999960041972e-13f;
    (&_S322)->differential_0 = 0.0f;
    _d_max_0(&_S321, &_S322, _S320.differential_0);
    float _S323 = 0.5f * - (_S319 + _S321.differential_0);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S324;
    (&_S324)->primal_0 = _S288;
    (&_S324)->differential_0 = _S100;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S325;
    (&_S325)->primal_0 = _S279;
    (&_S325)->differential_0 = _S100;
    s_bwd_prop_dot_0(&_S324, &_S325, _S323);
    float _S326 = _S168 * _S224;
    DiffPair_float_0 _S327;
    (&_S327)->primal_0 = _S169;
    (&_S327)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S327, _S326);
    DiffPair_float_0 _S328;
    (&_S328)->primal_0 = cos_sim_loss_6;
    (&_S328)->differential_0 = 0.0f;
    DiffPair_float_0 _S329;
    (&_S329)->primal_0 = 9.999999960041972e-13f;
    (&_S329)->differential_0 = 0.0f;
    _d_max_0(&_S328, &_S329, _S327.differential_0);
    float _S330 = 0.5f * - (_S326 + _S328.differential_0);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S331;
    (&_S331)->primal_0 = _S270;
    (&_S331)->differential_0 = _S100;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S332;
    (&_S332)->primal_0 = _S279;
    (&_S332)->differential_0 = _S100;
    s_bwd_prop_dot_0(&_S331, &_S332, _S330);
    float3  _S333 = _S311.differential_0 + _S310.differential_0 + _S148;
    float3  _S334 = _S318.differential_0 + _S324.differential_0 + _S290.differential_0;
    float3  _S335 = _S317.differential_0 + _S331.differential_0 + _S272.differential_0;
    float3  _S336 = _S325.differential_0 + _S332.differential_0 + _S281.differential_0;
    if(_S164)
    {
        float3  _S337 = _S96.primal_0 * _S336;
        float3  _S338 = _S165 * _S336;
        float _S339 = _S337.x + _S337.y + _S337.z;
        DiffPair_float_0 _S340;
        (&_S340)->primal_0 = _S160;
        (&_S340)->differential_0 = 0.0f;
        s_bwd_prop_rsqrt_0(&_S340, _S339);
        _S305 = _S340.differential_0;
        _S148 = _S338;
    }
    else
    {
        _S305 = 0.0f;
        _S148 = _S100;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S341;
    (&_S341)->primal_0 = _S96.primal_0;
    (&_S341)->differential_0 = _S100;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S342;
    (&_S342)->primal_0 = _S96.primal_0;
    (&_S342)->differential_0 = _S100;
    s_bwd_prop_dot_0(&_S341, &_S342, _S305);
    float3  _S343 = _S342.differential_0 + _S341.differential_0 + _S148;
    if(_S156)
    {
        float3  _S344 = _S95.primal_0 * _S334;
        float3  _S345 = _S157 * _S334;
        float _S346 = _S344.x + _S344.y + _S344.z;
        DiffPair_float_0 _S347;
        (&_S347)->primal_0 = _S153;
        (&_S347)->differential_0 = 0.0f;
        s_bwd_prop_rsqrt_0(&_S347, _S346);
        _S305 = _S347.differential_0;
        _S148 = _S345;
    }
    else
    {
        _S305 = 0.0f;
        _S148 = _S100;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S348;
    (&_S348)->primal_0 = _S95.primal_0;
    (&_S348)->differential_0 = _S100;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S349;
    (&_S349)->primal_0 = _S95.primal_0;
    (&_S349)->differential_0 = _S100;
    s_bwd_prop_dot_0(&_S348, &_S349, _S305);
    float3  _S350 = _S349.differential_0 + _S348.differential_0 + _S148;
    if(_S149)
    {
        float3  _S351 = _S94.primal_0 * _S335;
        float3  _S352 = _S150 * _S335;
        float _S353 = _S351.x + _S351.y + _S351.z;
        DiffPair_float_0 _S354;
        (&_S354)->primal_0 = _S146;
        (&_S354)->differential_0 = 0.0f;
        s_bwd_prop_rsqrt_0(&_S354, _S353);
        _S305 = _S354.differential_0;
        _S148 = _S352;
    }
    else
    {
        _S305 = 0.0f;
        _S148 = _S100;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S355;
    (&_S355)->primal_0 = _S94.primal_0;
    (&_S355)->differential_0 = _S100;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S356;
    (&_S356)->primal_0 = _S94.primal_0;
    (&_S356)->differential_0 = _S100;
    s_bwd_prop_dot_0(&_S355, &_S356, _S305);
    float3  _S357 = _S356.differential_0 + _S355.differential_0 + _S148;
    float _S358 = _S144 * _S222;
    float _S359 = _S144 * _S221;
    float _S360 = _S143 * _S220;
    float _S361 = _S142 * (_S143 * _S222 + _S359 + _S359 + _S219);
    DiffPair_float_0 _S362;
    (&_S362)->primal_0 = _S93.primal_0;
    (&_S362)->differential_0 = 0.0f;
    DiffPair_float_0 _S363;
    (&_S363)->primal_0 = 0.00009999999747379f;
    (&_S363)->differential_0 = 0.0f;
    _d_max_0(&_S362, &_S363, _S361);
    float _S364 = _S142 * (_S358 + _S360 + _S360 + _S218);
    DiffPair_float_0 _S365;
    (&_S365)->primal_0 = _S92.primal_0;
    (&_S365)->differential_0 = 0.0f;
    DiffPair_float_0 _S366;
    (&_S366)->primal_0 = 0.00009999999747379f;
    (&_S366)->differential_0 = 0.0f;
    _d_max_0(&_S365, &_S366, _S364);
    float _S367 = _S140 * _S223;
    DiffPair_float_0 _S368;
    (&_S368)->primal_0 = _S141;
    (&_S368)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S368, _S367);
    float _S369 = log_ratio_1 * _S368.differential_0;
    float _S370 = 33.33333587646484375f * (_S369 + _S369);
    float _S371 = - _S370;
    DiffPair_float_0 _S372;
    (&_S372)->primal_0 = _S139;
    (&_S372)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S372, _S371);
    DiffPair_float_0 _S373;
    (&_S373)->primal_0 = _S93.primal_0;
    (&_S373)->differential_0 = 0.0f;
    DiffPair_float_0 _S374;
    (&_S374)->primal_0 = 0.00100000004749745f;
    (&_S374)->differential_0 = 0.0f;
    _d_max_0(&_S373, &_S374, _S372.differential_0);
    DiffPair_float_0 _S375;
    (&_S375)->primal_0 = _S138;
    (&_S375)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S375, _S370);
    DiffPair_float_0 _S376;
    (&_S376)->primal_0 = _S92.primal_0;
    (&_S376)->differential_0 = 0.0f;
    DiffPair_float_0 _S377;
    (&_S377)->primal_0 = 0.00100000004749745f;
    (&_S377)->differential_0 = 0.0f;
    _d_max_0(&_S376, &_S377, _S375.differential_0);
    float _S378 = _S362.differential_0 + _S373.differential_0;
    float _S379 = _S365.differential_0 + _S376.differential_0 + _S301.differential_0;
    float _S380 = _S126 * _S217;
    DiffPair_float_0 _S381;
    (&_S381)->primal_0 = _S130;
    (&_S381)->differential_0 = 0.0f;
    DiffPair_float_0 _S382;
    (&_S382)->primal_0 = 0.0f;
    (&_S382)->differential_0 = 0.0f;
    DiffPair_float_0 _S383;
    (&_S383)->primal_0 = 1.0f;
    (&_S383)->differential_0 = 0.0f;
    s_bwd_prop_clamp_0(&_S381, &_S382, &_S383, _S380);
    float _S384 = _S126 * _S216;
    float _S385 = _S137 * _S384;
    float _S386 = _S136 * (dV_1 * _S384);
    float _S387 = _S135 * _S384;
    float _S388 = _S134 * (dU_1 * _S384);
    float _S389 = _S133 * _S384;
    float _S390 = _S132 * (dY_1 * _S384);
    float _S391 = _S131 * _S384;
    DiffPair_float_0 _S392;
    (&_S392)->primal_0 = dY_1;
    (&_S392)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S392, _S391);
    float _S393 = 0.3333333432674408f * (_S381.differential_0 + _S129 * _S384);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S394;
    (&_S394)->primal_0 = _S128;
    (&_S394)->differential_0 = _S100;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S395;
    (&_S395)->primal_0 = _S128;
    (&_S395)->differential_0 = _S100;
    s_bwd_prop_dot_0(&_S394, &_S395, _S393);
    float _S396 = 0.3333333432674408f * (_S127 * _S384);
    float3  _S397 = make_float3 (_S396, _S396, _S396);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S398;
    (&_S398)->primal_0 = _S128;
    (&_S398)->differential_0 = _S100;
    s_bwd_prop_abs_1(&_S398, _S397);
    float3  _S399 = _S395.differential_0 + _S394.differential_0 + _S398.differential_0;
    float _S400 = _S385 + _S386;
    float s_diff_V_T_0 = - _S400;
    float _S401 = _S387 + _S388;
    float s_diff_U_T_0 = - _S401;
    float _S402 = _S389 + _S390 + _S392.differential_0;
    float s_diff_Y_T_0 = - _S402;
    float _S403 = - s_diff_V_T_0;
    float3  _S404 = _S399 + make_float3 (0.61500000953674316f * s_diff_V_T_0 + -0.14712999761104584f * s_diff_U_T_0 + 0.29899999499320984f * s_diff_Y_T_0, 0.51498997211456299f * _S403 + 0.28885999321937561f * - s_diff_U_T_0 + 0.58700001239776611f * s_diff_Y_T_0, 0.10001000016927719f * _S403 + 0.43599998950958252f * s_diff_U_T_0 + 0.11400000005960464f * s_diff_Y_T_0);
    float3  _S405 = - _S399 + make_float3 (0.61500000953674316f * _S400 + -0.14712999761104584f * _S401 + 0.29899999499320984f * _S402, 0.51498997211456299f * s_diff_V_T_0 + 0.28885999321937561f * s_diff_U_T_0 + 0.58700001239776611f * _S402, 0.10001000016927719f * s_diff_V_T_0 + 0.43599998950958252f * _S401 + 0.11400000005960464f * _S402);
    if(_S119)
    {
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S406;
        (&_S406)->primal_0 = _S96.primal_0;
        (&_S406)->differential_0 = _S100;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S407;
        (&_S407)->primal_0 = _S96.primal_0;
        (&_S407)->differential_0 = _S100;
        s_bwd_prop_dot_0(&_S406, &_S407, 0.0f);
        _S148 = _S407.differential_0 + _S406.differential_0 + _S343;
    }
    else
    {
        _S148 = _S343;
    }
    if(_S102)
    {
        DiffPair_float_0 _S408;
        (&_S408)->primal_0 = _S111;
        (&_S408)->differential_0 = 0.0f;
        DiffPair_float_0 _S409;
        (&_S409)->primal_0 = _S112;
        (&_S409)->differential_0 = 0.0f;
        _d_min_0(&_S408, &_S409, 0.0f);
        DiffPair_float_0 _S410;
        (&_S410)->primal_0 = _S113;
        (&_S410)->differential_0 = 0.0f;
        DiffPair_float_0 _S411;
        (&_S411)->primal_0 = _S114;
        (&_S411)->differential_0 = 0.0f;
        _d_min_0(&_S410, &_S411, _S408.differential_0);
        _S150 = _S404 + make_float3 (_S410.differential_0, _S411.differential_0, _S409.differential_0);
    }
    else
    {
        _S150 = _S404;
    }
    if(_S101)
    {
        DiffPair_float_0 _S412;
        (&_S412)->primal_0 = _S103;
        (&_S412)->differential_0 = 0.0f;
        DiffPair_float_0 _S413;
        (&_S413)->primal_0 = _S104;
        (&_S413)->differential_0 = 0.0f;
        _d_min_0(&_S412, &_S413, 0.0f);
        DiffPair_float_0 _S414;
        (&_S414)->primal_0 = _S105;
        (&_S414)->differential_0 = 0.0f;
        DiffPair_float_0 _S415;
        (&_S415)->primal_0 = _S106;
        (&_S415)->differential_0 = 0.0f;
        _d_min_0(&_S414, &_S415, _S412.differential_0);
        _S155 = _S405 + make_float3 (_S414.differential_0, _S415.differential_0, _S413.differential_0);
    }
    else
    {
        _S155 = _S405;
    }
    dpmedian_normal_0->primal_0 = (*dpmedian_normal_0).primal_0;
    dpmedian_normal_0->differential_0 = _S333;
    dpmedian_depth_0->primal_0 = (*dpmedian_depth_0).primal_0;
    dpmedian_depth_0->differential_0 = _S297.differential_0;
    dpnormal_dist_0->primal_0 = (*dpnormal_dist_0).primal_0;
    dpnormal_dist_0->differential_0 = _S302;
    dpdepth_dist_0->primal_0 = (*dpdepth_dist_0).primal_0;
    dpdepth_dist_0->differential_0 = _S228;
    dprgb_dist_0->primal_0 = (*dprgb_dist_0).primal_0;
    dprgb_dist_0->differential_0 = _S303;
    dprender_Ts_0->primal_0 = (*dprender_Ts_0).primal_0;
    dprender_Ts_0->differential_0 = _S263;
    dpref_normal_0->primal_0 = (*dpref_normal_0).primal_0;
    dpref_normal_0->differential_0 = _S148;
    dpdepth_normal_0->primal_0 = (*dpdepth_normal_0).primal_0;
    dpdepth_normal_0->differential_0 = _S350;
    dprender_normal_0->primal_0 = (*dprender_normal_0).primal_0;
    dprender_normal_0->differential_0 = _S357;
    dpref_depth_0->primal_0 = (*dpref_depth_0).primal_0;
    dpref_depth_0->differential_0 = _S378;
    dprender_depth_0->primal_0 = (*dprender_depth_0).primal_0;
    dprender_depth_0->differential_0 = _S379;
    dpref_rgb_0->primal_0 = (*dpref_rgb_0).primal_0;
    dpref_rgb_0->differential_0 = _S150;
    dprender_rgb_0->primal_0 = (*dprender_rgb_0).primal_0;
    dprender_rgb_0->differential_0 = _S155;
    return;
}

inline __device__ void s_bwd_per_pixel_losses_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S416, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S417, DiffPair_float_0 * _S418, DiffPair_float_0 * _S419, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S420, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S421, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S422, DiffPair_float_0 * _S423, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S424, DiffPair_float_0 * _S425, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S426, DiffPair_float_0 * _S427, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S428, bool _S429, bool _S430, float _S431, FixedArray<float, 21>  * _S432, FixedArray<float, 33>  * _S433)
{
    s_bwd_prop_per_pixel_losses_0(_S416, _S417, _S418, _S419, _S420, _S421, _S422, _S423, _S424, _S425, _S426, _S427, _S428, _S429, _S430, _S431, _S432, _S433);
    return;
}

inline __device__ void per_pixel_losses_bwd(float3  render_rgb_1, float3  ref_rgb_1, float render_depth_1, float ref_depth_1, float3  render_normal_1, float3  depth_normal_1, float3  ref_normal_1, float render_Ts_1, float3  rgb_dist_1, float depth_dist_1, float3  normal_dist_1, float median_depth_1, float3  median_normal_1, bool ref_alpha_2, bool has_mask_2, float saturation_threshold_2, FixedArray<float, 21>  weights_2, FixedArray<float, 33>  v_losses_0, float3  * v_render_rgb_0, float3  * v_ref_rgb_0, float * v_render_depth_0, float * v_ref_depth_0, float3  * v_render_normal_0, float3  * v_depth_normal_0, float3  * v_ref_normal_0, float * v_render_Ts_0, float3  * v_rgb_dist_0, float * v_depth_dist_0, float3  * v_normal_dist_0, float * v_median_depth_0, float3  * v_median_normal_0)
{
    float3  _S434 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_render_rgb_0;
    (&dp_render_rgb_0)->primal_0 = render_rgb_1;
    (&dp_render_rgb_0)->differential_0 = _S434;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ref_rgb_0;
    (&dp_ref_rgb_0)->primal_0 = ref_rgb_1;
    (&dp_ref_rgb_0)->differential_0 = _S434;
    DiffPair_float_0 dp_render_depth_0;
    (&dp_render_depth_0)->primal_0 = render_depth_1;
    (&dp_render_depth_0)->differential_0 = 0.0f;
    DiffPair_float_0 dp_ref_depth_0;
    (&dp_ref_depth_0)->primal_0 = ref_depth_1;
    (&dp_ref_depth_0)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_render_normal_0;
    (&dp_render_normal_0)->primal_0 = render_normal_1;
    (&dp_render_normal_0)->differential_0 = _S434;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_depth_normal_0;
    (&dp_depth_normal_0)->primal_0 = depth_normal_1;
    (&dp_depth_normal_0)->differential_0 = _S434;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ref_normal_0;
    (&dp_ref_normal_0)->primal_0 = ref_normal_1;
    (&dp_ref_normal_0)->differential_0 = _S434;
    DiffPair_float_0 dp_render_Ts_0;
    (&dp_render_Ts_0)->primal_0 = render_Ts_1;
    (&dp_render_Ts_0)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_rgb_dist_0;
    (&dp_rgb_dist_0)->primal_0 = rgb_dist_1;
    (&dp_rgb_dist_0)->differential_0 = _S434;
    DiffPair_float_0 dp_depth_dist_0;
    (&dp_depth_dist_0)->primal_0 = depth_dist_1;
    (&dp_depth_dist_0)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_normal_dist_0;
    (&dp_normal_dist_0)->primal_0 = normal_dist_1;
    (&dp_normal_dist_0)->differential_0 = _S434;
    DiffPair_float_0 dp_median_depth_0;
    (&dp_median_depth_0)->primal_0 = median_depth_1;
    (&dp_median_depth_0)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_median_normal_0;
    (&dp_median_normal_0)->primal_0 = median_normal_1;
    (&dp_median_normal_0)->differential_0 = _S434;
    FixedArray<float, 21>  _S435 = weights_2;
    FixedArray<float, 33>  _S436 = v_losses_0;
    s_bwd_per_pixel_losses_0(&dp_render_rgb_0, &dp_ref_rgb_0, &dp_render_depth_0, &dp_ref_depth_0, &dp_render_normal_0, &dp_depth_normal_0, &dp_ref_normal_0, &dp_render_Ts_0, &dp_rgb_dist_0, &dp_depth_dist_0, &dp_normal_dist_0, &dp_median_depth_0, &dp_median_normal_0, ref_alpha_2, has_mask_2, saturation_threshold_2, &_S435, &_S436);
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

inline __device__ void _d_log10_0(DiffPair_float_0 * dpx_10, float dOut_10)
{
    float _S437 = 1.0f / ((*dpx_10).primal_0 * 2.30258512496948242f) * dOut_10;
    dpx_10->primal_0 = (*dpx_10).primal_0;
    dpx_10->differential_0 = _S437;
    return;
}

inline __device__ void per_pixel_losses_reduce(FixedArray<float, 33>  raw_losses_0, FixedArray<float, 21>  weights_3, FixedArray<float, 15>  * _S438)
{
    FixedArray<float, 15>  losses_1;
    float _S439 = (F32_max((raw_losses_0[int(22)]), (1.0f)));
    losses_1[int(0)] = raw_losses_0[int(0)] / _S439;
    losses_1[int(1)] = -10.0f * (F32_log10((raw_losses_0[int(1)] / _S439)));
    bool _S440;
    if((raw_losses_0[int(23)]) > 0.0f)
    {
        _S440 = (raw_losses_0[int(3)]) != 0.0f;
    }
    else
    {
        _S440 = false;
    }
    float _S441;
    if(_S440)
    {
        _S441 = weights_3[int(6)] * clamp_0(1.0f - (raw_losses_0[int(6)] - raw_losses_0[int(2)] * raw_losses_0[int(3)] / raw_losses_0[int(23)]) / (F32_sqrt(((F32_max((9.999999960041972e-13f), ((raw_losses_0[int(4)] - raw_losses_0[int(2)] * raw_losses_0[int(2)] / raw_losses_0[int(23)]) * (raw_losses_0[int(5)] - raw_losses_0[int(3)] * raw_losses_0[int(3)] / raw_losses_0[int(23)]) + 1.0f)))))), 0.0f, 2.0f);
    }
    else
    {
        _S441 = 0.0f;
    }
    losses_1[int(2)] = _S441;
    losses_1[int(3)] = raw_losses_0[int(7)] / (F32_max((raw_losses_0[int(23)]), (1.0f)));
    losses_1[int(4)] = (raw_losses_0[int(8)] / (F32_max((raw_losses_0[int(24)]), (1.0f))) + raw_losses_0[int(9)] / (F32_max((raw_losses_0[int(25)]), (1.0f)))) / float((I32_max((int((raw_losses_0[int(24)]) > 0.5f) + int((raw_losses_0[int(25)]) > 0.5f)), (int(1)))));
    losses_1[int(5)] = raw_losses_0[int(10)] / (F32_max((raw_losses_0[int(27)]), (1.0f))) + raw_losses_0[int(11)] / (F32_max((raw_losses_0[int(28)]), (1.0f)));
    losses_1[int(6)] = raw_losses_0[int(12)] / (F32_max((raw_losses_0[int(26)]), (1.0f)));
    losses_1[int(7)] = raw_losses_0[int(13)] / _S439;
    losses_1[int(8)] = raw_losses_0[int(14)] / _S439;
    losses_1[int(9)] = raw_losses_0[int(15)] / _S439;
    losses_1[int(10)] = raw_losses_0[int(16)] / _S439;
    losses_1[int(11)] = raw_losses_0[int(17)] / (F32_max((raw_losses_0[int(29)]), (1.0f)));
    losses_1[int(12)] = raw_losses_0[int(18)] / (F32_max((raw_losses_0[int(30)]), (1.0f)));
    losses_1[int(13)] = raw_losses_0[int(19)] / (F32_max((raw_losses_0[int(31)]), (1.0f)));
    losses_1[int(14)] = raw_losses_0[int(20)] / (F32_max((raw_losses_0[int(32)]), (1.0f)));
    *_S438 = losses_1;
    return;
}

struct DiffPair_arrayx3Cfloatx2C33x3E_0
{
    FixedArray<float, 33>  primal_0;
    FixedArray<float, 33>  differential_0;
};

inline __device__ float s_primal_ctx_sqrt_0(float _S442)
{
    return (F32_sqrt((_S442)));
}

inline __device__ void s_bwd_prop_log10_0(DiffPair_float_0 * _S443, float _S444)
{
    _d_log10_0(_S443, _S444);
    return;
}

inline __device__ void s_bwd_prop_per_pixel_losses_reduce_0(DiffPair_arrayx3Cfloatx2C33x3E_0 * dpraw_losses_0, FixedArray<float, 21>  * weights_4, FixedArray<float, 15>  * _s_dOut_1)
{
    FixedArray<float, 33>  _S445 = dpraw_losses_0->primal_0;
    float _S446 = (F32_max((dpraw_losses_0->primal_0[int(22)]), (1.0f)));
    float _S447 = _S446 * _S446;
    float _S448 = dpraw_losses_0->primal_0[int(1)] / _S446;
    bool _S449 = (dpraw_losses_0->primal_0[int(23)]) > 0.0f;
    bool _S450;
    if(_S449)
    {
        _S450 = (_S445[int(3)]) != 0.0f;
    }
    else
    {
        _S450 = false;
    }
    float _S451;
    float _S452;
    float _S453;
    float _S454;
    float _S455;
    float _S456;
    float _S457;
    float _S458;
    float _S459;
    float _S460;
    float _S461;
    float _S462;
    float _S463;
    float _S464;
    float _S465;
    if(_S450)
    {
        float _S466 = _S445[int(2)] * _S445[int(3)];
        float _S467 = _S445[int(23)] * _S445[int(23)];
        float _S468 = _S445[int(6)] - _S466 / _S445[int(23)];
        float _S469 = _S445[int(2)] * _S445[int(2)];
        float _S470 = _S445[int(4)] - _S469 / _S445[int(23)];
        float _S471 = _S445[int(3)] * _S445[int(3)];
        float _S472 = _S445[int(5)] - _S471 / _S445[int(23)];
        float _S473 = _S470 * _S472 + 1.0f;
        float _S474 = (F32_max((9.999999960041972e-13f), (_S473)));
        float _S475 = s_primal_ctx_sqrt_0(_S474);
        float _S476 = _S475 * _S475;
        float _S477 = 1.0f - _S468 / _S475;
        _S451 = (*weights_4)[int(6)];
        _S452 = _S477;
        _S453 = _S476;
        _S454 = _S468;
        _S455 = _S475;
        _S456 = _S474;
        _S457 = _S473;
        _S458 = _S470;
        _S459 = _S472;
        _S460 = _S467;
        _S461 = _S471;
        _S462 = _S445[int(3)];
        _S463 = _S469;
        _S464 = _S445[int(2)];
        _S465 = _S466;
    }
    else
    {
        _S451 = 0.0f;
        _S452 = 0.0f;
        _S453 = 0.0f;
        _S454 = 0.0f;
        _S455 = 0.0f;
        _S456 = 0.0f;
        _S457 = 0.0f;
        _S458 = 0.0f;
        _S459 = 0.0f;
        _S460 = 0.0f;
        _S461 = 0.0f;
        _S462 = 0.0f;
        _S463 = 0.0f;
        _S464 = 0.0f;
        _S465 = 0.0f;
    }
    float _S478 = (F32_max((_S445[int(23)]), (1.0f)));
    float _S479 = _S478 * _S478;
    float _S480 = (F32_max((_S445[int(24)]), (1.0f)));
    float _S481 = _S480 * _S480;
    float _S482 = (F32_max((_S445[int(25)]), (1.0f)));
    float _S483 = _S482 * _S482;
    float _S484 = float((I32_max((int((_S445[int(24)]) > 0.5f) + int((_S445[int(25)]) > 0.5f)), (int(1)))));
    float _S485 = (F32_max((_S445[int(27)]), (1.0f)));
    float _S486 = _S485 * _S485;
    float _S487 = (F32_max((_S445[int(28)]), (1.0f)));
    float _S488 = _S487 * _S487;
    float _S489 = (F32_max((_S445[int(26)]), (1.0f)));
    float _S490 = _S489 * _S489;
    float _S491 = (F32_max((_S445[int(29)]), (1.0f)));
    float _S492 = _S491 * _S491;
    float _S493 = (F32_max((_S445[int(30)]), (1.0f)));
    float _S494 = _S493 * _S493;
    float _S495 = (F32_max((_S445[int(31)]), (1.0f)));
    float _S496 = _S495 * _S495;
    float _S497 = (F32_max((_S445[int(32)]), (1.0f)));
    float _S498 = _S497 * _S497;
    float _S499 = (*_s_dOut_1)[int(0)];
    float _S500 = (*_s_dOut_1)[int(1)];
    float _S501 = (*_s_dOut_1)[int(2)];
    float _S502 = (*_s_dOut_1)[int(14)] / _S498;
    float _S503 = _S445[int(20)] * - _S502;
    float _S504 = _S497 * _S502;
    DiffPair_float_0 _S505;
    (&_S505)->primal_0 = _S445[int(32)];
    (&_S505)->differential_0 = 0.0f;
    DiffPair_float_0 _S506;
    (&_S506)->primal_0 = 1.0f;
    (&_S506)->differential_0 = 0.0f;
    _d_max_0(&_S505, &_S506, _S503);
    float _S507 = (*_s_dOut_1)[int(13)] / _S496;
    float _S508 = _S445[int(19)] * - _S507;
    float _S509 = _S495 * _S507;
    DiffPair_float_0 _S510;
    (&_S510)->primal_0 = _S445[int(31)];
    (&_S510)->differential_0 = 0.0f;
    DiffPair_float_0 _S511;
    (&_S511)->primal_0 = 1.0f;
    (&_S511)->differential_0 = 0.0f;
    _d_max_0(&_S510, &_S511, _S508);
    float _S512 = (*_s_dOut_1)[int(12)] / _S494;
    float _S513 = _S445[int(18)] * - _S512;
    float _S514 = _S493 * _S512;
    DiffPair_float_0 _S515;
    (&_S515)->primal_0 = _S445[int(30)];
    (&_S515)->differential_0 = 0.0f;
    DiffPair_float_0 _S516;
    (&_S516)->primal_0 = 1.0f;
    (&_S516)->differential_0 = 0.0f;
    _d_max_0(&_S515, &_S516, _S513);
    float _S517 = (*_s_dOut_1)[int(11)] / _S492;
    float _S518 = _S445[int(17)] * - _S517;
    float _S519 = _S491 * _S517;
    DiffPair_float_0 _S520;
    (&_S520)->primal_0 = _S445[int(29)];
    (&_S520)->differential_0 = 0.0f;
    DiffPair_float_0 _S521;
    (&_S521)->primal_0 = 1.0f;
    (&_S521)->differential_0 = 0.0f;
    _d_max_0(&_S520, &_S521, _S518);
    float _S522 = (*_s_dOut_1)[int(10)] / _S447;
    float _S523 = _S445[int(16)] * - _S522;
    float _S524 = _S446 * _S522;
    float _S525 = (*_s_dOut_1)[int(9)] / _S447;
    float _S526 = _S445[int(15)] * - _S525;
    float _S527 = _S446 * _S525;
    float _S528 = (*_s_dOut_1)[int(8)] / _S447;
    float _S529 = _S445[int(14)] * - _S528;
    float _S530 = _S446 * _S528;
    float _S531 = (*_s_dOut_1)[int(7)] / _S447;
    float _S532 = _S445[int(13)] * - _S531;
    float _S533 = _S446 * _S531;
    float _S534 = (*_s_dOut_1)[int(6)] / _S490;
    float _S535 = _S445[int(12)] * - _S534;
    float _S536 = _S489 * _S534;
    DiffPair_float_0 _S537;
    (&_S537)->primal_0 = _S445[int(26)];
    (&_S537)->differential_0 = 0.0f;
    DiffPair_float_0 _S538;
    (&_S538)->primal_0 = 1.0f;
    (&_S538)->differential_0 = 0.0f;
    _d_max_0(&_S537, &_S538, _S535);
    float _S539 = (*_s_dOut_1)[int(5)] / _S488;
    float _S540 = _S445[int(11)] * - _S539;
    float _S541 = _S487 * _S539;
    DiffPair_float_0 _S542;
    (&_S542)->primal_0 = _S445[int(28)];
    (&_S542)->differential_0 = 0.0f;
    DiffPair_float_0 _S543;
    (&_S543)->primal_0 = 1.0f;
    (&_S543)->differential_0 = 0.0f;
    _d_max_0(&_S542, &_S543, _S540);
    float _S544 = (*_s_dOut_1)[int(5)] / _S486;
    float _S545 = _S445[int(10)] * - _S544;
    float _S546 = _S485 * _S544;
    DiffPair_float_0 _S547;
    (&_S547)->primal_0 = _S445[int(27)];
    (&_S547)->differential_0 = 0.0f;
    DiffPair_float_0 _S548;
    (&_S548)->primal_0 = 1.0f;
    (&_S548)->differential_0 = 0.0f;
    _d_max_0(&_S547, &_S548, _S545);
    float _S549 = (*_s_dOut_1)[int(4)] / _S484;
    float _S550 = _S549 / _S483;
    float _S551 = _S445[int(9)] * - _S550;
    float _S552 = _S482 * _S550;
    DiffPair_float_0 _S553;
    (&_S553)->primal_0 = _S445[int(25)];
    (&_S553)->differential_0 = 0.0f;
    DiffPair_float_0 _S554;
    (&_S554)->primal_0 = 1.0f;
    (&_S554)->differential_0 = 0.0f;
    _d_max_0(&_S553, &_S554, _S551);
    float _S555 = _S549 / _S481;
    float _S556 = _S445[int(8)] * - _S555;
    float _S557 = _S480 * _S555;
    DiffPair_float_0 _S558;
    (&_S558)->primal_0 = _S445[int(24)];
    (&_S558)->differential_0 = 0.0f;
    DiffPair_float_0 _S559;
    (&_S559)->primal_0 = 1.0f;
    (&_S559)->differential_0 = 0.0f;
    _d_max_0(&_S558, &_S559, _S556);
    float _S560 = (*_s_dOut_1)[int(3)] / _S479;
    float _S561 = _S445[int(7)] * - _S560;
    float _S562 = _S478 * _S560;
    DiffPair_float_0 _S563;
    (&_S563)->primal_0 = _S445[int(23)];
    (&_S563)->differential_0 = 0.0f;
    DiffPair_float_0 _S564;
    (&_S564)->primal_0 = 1.0f;
    (&_S564)->differential_0 = 0.0f;
    _d_max_0(&_S563, &_S564, _S561);
    DiffPair_float_0 _S565 = _S563;
    float _S566 = _S523 + _S526 + _S529 + _S532;
    FixedArray<float, 33>  _S567;
    _S567[int(0)] = 0.0f;
    _S567[int(1)] = 0.0f;
    _S567[int(2)] = 0.0f;
    _S567[int(3)] = 0.0f;
    _S567[int(4)] = 0.0f;
    _S567[int(5)] = 0.0f;
    _S567[int(6)] = 0.0f;
    _S567[int(7)] = 0.0f;
    _S567[int(8)] = 0.0f;
    _S567[int(9)] = 0.0f;
    _S567[int(10)] = 0.0f;
    _S567[int(11)] = 0.0f;
    _S567[int(12)] = 0.0f;
    _S567[int(13)] = 0.0f;
    _S567[int(14)] = 0.0f;
    _S567[int(15)] = 0.0f;
    _S567[int(16)] = 0.0f;
    _S567[int(17)] = 0.0f;
    _S567[int(18)] = 0.0f;
    _S567[int(19)] = 0.0f;
    _S567[int(20)] = 0.0f;
    _S567[int(21)] = 0.0f;
    _S567[int(22)] = 0.0f;
    _S567[int(23)] = 0.0f;
    _S567[int(24)] = 0.0f;
    _S567[int(25)] = 0.0f;
    _S567[int(26)] = 0.0f;
    _S567[int(27)] = 0.0f;
    _S567[int(28)] = 0.0f;
    _S567[int(29)] = 0.0f;
    _S567[int(30)] = 0.0f;
    _S567[int(31)] = 0.0f;
    _S567[int(32)] = 0.0f;
    _S567[int(32)] = _S505.differential_0;
    _S567[int(20)] = _S504;
    _S567[int(31)] = _S510.differential_0;
    _S567[int(19)] = _S509;
    _S567[int(30)] = _S515.differential_0;
    _S567[int(18)] = _S514;
    _S567[int(29)] = _S520.differential_0;
    _S567[int(17)] = _S519;
    _S567[int(16)] = _S524;
    _S567[int(15)] = _S527;
    _S567[int(14)] = _S530;
    _S567[int(13)] = _S533;
    _S567[int(26)] = _S537.differential_0;
    _S567[int(12)] = _S536;
    _S567[int(28)] = _S542.differential_0;
    _S567[int(11)] = _S541;
    _S567[int(27)] = _S547.differential_0;
    _S567[int(10)] = _S546;
    _S567[int(25)] = _S553.differential_0;
    _S567[int(9)] = _S552;
    _S567[int(24)] = _S558.differential_0;
    _S567[int(8)] = _S557;
    _S567[int(7)] = _S562;
    float _S568 = _S567[int(0)];
    float _S569 = _S567[int(1)];
    float _S570 = _S567[int(2)];
    float _S571 = _S567[int(3)];
    float _S572 = _S567[int(4)];
    float _S573 = _S567[int(5)];
    float _S574 = _S567[int(6)];
    float _S575 = _S567[int(7)];
    float _S576 = _S567[int(8)];
    float _S577 = _S567[int(9)];
    float _S578 = _S567[int(10)];
    float _S579 = _S567[int(11)];
    float _S580 = _S567[int(12)];
    float _S581 = _S567[int(13)];
    float _S582 = _S567[int(14)];
    float _S583 = _S567[int(15)];
    float _S584 = _S567[int(16)];
    float _S585 = _S567[int(17)];
    float _S586 = _S567[int(18)];
    float _S587 = _S567[int(19)];
    float _S588 = _S567[int(20)];
    float _S589 = _S567[int(21)];
    float _S590 = _S567[int(22)];
    float _S591 = _S567[int(23)];
    float _S592 = _S567[int(24)];
    float _S593 = _S567[int(25)];
    float _S594 = _S567[int(26)];
    float _S595 = _S567[int(27)];
    float _S596 = _S567[int(28)];
    float _S597 = _S567[int(29)];
    float _S598 = _S567[int(30)];
    float _S599 = _S567[int(31)];
    float _S600 = _S567[int(32)];
    FixedArray<float, 33>  _S601;
    if(_S450)
    {
        float _S602 = _S451 * _S501;
        DiffPair_float_0 _S603;
        (&_S603)->primal_0 = _S452;
        (&_S603)->differential_0 = 0.0f;
        DiffPair_float_0 _S604;
        (&_S604)->primal_0 = 0.0f;
        (&_S604)->differential_0 = 0.0f;
        DiffPair_float_0 _S605;
        (&_S605)->primal_0 = 2.0f;
        (&_S605)->differential_0 = 0.0f;
        s_bwd_prop_clamp_0(&_S603, &_S604, &_S605, _S602);
        float _S606 = - _S603.differential_0 / _S453;
        float _S607 = _S454 * - _S606;
        float _S608 = _S455 * _S606;
        DiffPair_float_0 _S609;
        (&_S609)->primal_0 = _S456;
        (&_S609)->differential_0 = 0.0f;
        s_bwd_prop_sqrt_0(&_S609, _S607);
        DiffPair_float_0 _S610;
        (&_S610)->primal_0 = 9.999999960041972e-13f;
        (&_S610)->differential_0 = 0.0f;
        DiffPair_float_0 _S611;
        (&_S611)->primal_0 = _S457;
        (&_S611)->differential_0 = 0.0f;
        _d_max_0(&_S610, &_S611, _S609.differential_0);
        float _S612 = _S458 * _S611.differential_0;
        float _S613 = _S459 * _S611.differential_0;
        float _S614 = - _S612 / _S460;
        float _S615 = _S462 * (_S445[int(23)] * _S614);
        float _S616 = - _S613 / _S460;
        float _S617 = _S464 * (_S445[int(23)] * _S616);
        float _S618 = - _S608 / _S460;
        float _S619 = _S445[int(23)] * _S618;
        float _S620 = _S615 + _S615 + _S464 * _S619;
        float _S621 = _S617 + _S617 + _S462 * _S619;
        float _S622 = _S461 * - _S614 + _S463 * - _S616 + _S465 * - _S618 + _S565.differential_0;
        FixedArray<float, 33>  _S623;
        _S623[int(0)] = 0.0f;
        _S623[int(1)] = 0.0f;
        _S623[int(2)] = 0.0f;
        _S623[int(3)] = 0.0f;
        _S623[int(4)] = 0.0f;
        _S623[int(5)] = 0.0f;
        _S623[int(6)] = 0.0f;
        _S623[int(7)] = 0.0f;
        _S623[int(8)] = 0.0f;
        _S623[int(9)] = 0.0f;
        _S623[int(10)] = 0.0f;
        _S623[int(11)] = 0.0f;
        _S623[int(12)] = 0.0f;
        _S623[int(13)] = 0.0f;
        _S623[int(14)] = 0.0f;
        _S623[int(15)] = 0.0f;
        _S623[int(16)] = 0.0f;
        _S623[int(17)] = 0.0f;
        _S623[int(18)] = 0.0f;
        _S623[int(19)] = 0.0f;
        _S623[int(20)] = 0.0f;
        _S623[int(21)] = 0.0f;
        _S623[int(22)] = 0.0f;
        _S623[int(23)] = 0.0f;
        _S623[int(24)] = 0.0f;
        _S623[int(25)] = 0.0f;
        _S623[int(26)] = 0.0f;
        _S623[int(27)] = 0.0f;
        _S623[int(28)] = 0.0f;
        _S623[int(29)] = 0.0f;
        _S623[int(30)] = 0.0f;
        _S623[int(31)] = 0.0f;
        _S623[int(32)] = 0.0f;
        _S623[int(5)] = _S612;
        _S623[int(4)] = _S613;
        _S623[int(3)] = _S620;
        _S623[int(2)] = _S621;
        _S623[int(6)] = _S608;
        float _S624 = _S569 + _S623[int(1)];
        float _S625 = _S570 + _S623[int(2)];
        float _S626 = _S571 + _S623[int(3)];
        float _S627 = _S572 + _S623[int(4)];
        float _S628 = _S573 + _S623[int(5)];
        float _S629 = _S574 + _S623[int(6)];
        float _S630 = _S575 + _S623[int(7)];
        float _S631 = _S576 + _S623[int(8)];
        float _S632 = _S577 + _S623[int(9)];
        float _S633 = _S578 + _S623[int(10)];
        float _S634 = _S579 + _S623[int(11)];
        float _S635 = _S580 + _S623[int(12)];
        float _S636 = _S581 + _S623[int(13)];
        float _S637 = _S582 + _S623[int(14)];
        float _S638 = _S583 + _S623[int(15)];
        float _S639 = _S584 + _S623[int(16)];
        float _S640 = _S585 + _S623[int(17)];
        float _S641 = _S586 + _S623[int(18)];
        float _S642 = _S587 + _S623[int(19)];
        float _S643 = _S588 + _S623[int(20)];
        float _S644 = _S589 + _S623[int(21)];
        float _S645 = _S590 + _S623[int(22)];
        float _S646 = _S591 + _S623[int(23)];
        float _S647 = _S592 + _S623[int(24)];
        float _S648 = _S593 + _S623[int(25)];
        float _S649 = _S594 + _S623[int(26)];
        float _S650 = _S595 + _S623[int(27)];
        float _S651 = _S596 + _S623[int(28)];
        float _S652 = _S597 + _S623[int(29)];
        float _S653 = _S598 + _S623[int(30)];
        float _S654 = _S599 + _S623[int(31)];
        float _S655 = _S600 + _S623[int(32)];
        _S601[int(0)] = _S568 + _S623[int(0)];
        _S601[int(1)] = _S624;
        _S601[int(2)] = _S625;
        _S601[int(3)] = _S626;
        _S601[int(4)] = _S627;
        _S601[int(5)] = _S628;
        _S601[int(6)] = _S629;
        _S601[int(7)] = _S630;
        _S601[int(8)] = _S631;
        _S601[int(9)] = _S632;
        _S601[int(10)] = _S633;
        _S601[int(11)] = _S634;
        _S601[int(12)] = _S635;
        _S601[int(13)] = _S636;
        _S601[int(14)] = _S637;
        _S601[int(15)] = _S638;
        _S601[int(16)] = _S639;
        _S601[int(17)] = _S640;
        _S601[int(18)] = _S641;
        _S601[int(19)] = _S642;
        _S601[int(20)] = _S643;
        _S601[int(21)] = _S644;
        _S601[int(22)] = _S645;
        _S601[int(23)] = _S646;
        _S601[int(24)] = _S647;
        _S601[int(25)] = _S648;
        _S601[int(26)] = _S649;
        _S601[int(27)] = _S650;
        _S601[int(28)] = _S651;
        _S601[int(29)] = _S652;
        _S601[int(30)] = _S653;
        _S601[int(31)] = _S654;
        _S601[int(32)] = _S655;
        _S451 = _S622;
    }
    else
    {
        _S601[int(0)] = _S568;
        _S601[int(1)] = _S569;
        _S601[int(2)] = _S570;
        _S601[int(3)] = _S571;
        _S601[int(4)] = _S572;
        _S601[int(5)] = _S573;
        _S601[int(6)] = _S574;
        _S601[int(7)] = _S575;
        _S601[int(8)] = _S576;
        _S601[int(9)] = _S577;
        _S601[int(10)] = _S578;
        _S601[int(11)] = _S579;
        _S601[int(12)] = _S580;
        _S601[int(13)] = _S581;
        _S601[int(14)] = _S582;
        _S601[int(15)] = _S583;
        _S601[int(16)] = _S584;
        _S601[int(17)] = _S585;
        _S601[int(18)] = _S586;
        _S601[int(19)] = _S587;
        _S601[int(20)] = _S588;
        _S601[int(21)] = _S589;
        _S601[int(22)] = _S590;
        _S601[int(23)] = _S591;
        _S601[int(24)] = _S592;
        _S601[int(25)] = _S593;
        _S601[int(26)] = _S594;
        _S601[int(27)] = _S595;
        _S601[int(28)] = _S596;
        _S601[int(29)] = _S597;
        _S601[int(30)] = _S598;
        _S601[int(31)] = _S599;
        _S601[int(32)] = _S600;
        _S451 = _S565.differential_0;
    }
    if(_S449)
    {
        FixedArray<float, 33>  _S656;
        _S656[int(0)] = 0.0f;
        _S656[int(1)] = 0.0f;
        _S656[int(2)] = 0.0f;
        _S656[int(3)] = 0.0f;
        _S656[int(4)] = 0.0f;
        _S656[int(5)] = 0.0f;
        _S656[int(6)] = 0.0f;
        _S656[int(7)] = 0.0f;
        _S656[int(8)] = 0.0f;
        _S656[int(9)] = 0.0f;
        _S656[int(10)] = 0.0f;
        _S656[int(11)] = 0.0f;
        _S656[int(12)] = 0.0f;
        _S656[int(13)] = 0.0f;
        _S656[int(14)] = 0.0f;
        _S656[int(15)] = 0.0f;
        _S656[int(16)] = 0.0f;
        _S656[int(17)] = 0.0f;
        _S656[int(18)] = 0.0f;
        _S656[int(19)] = 0.0f;
        _S656[int(20)] = 0.0f;
        _S656[int(21)] = 0.0f;
        _S656[int(22)] = 0.0f;
        _S656[int(23)] = 0.0f;
        _S656[int(24)] = 0.0f;
        _S656[int(25)] = 0.0f;
        _S656[int(26)] = 0.0f;
        _S656[int(27)] = 0.0f;
        _S656[int(28)] = 0.0f;
        _S656[int(29)] = 0.0f;
        _S656[int(30)] = 0.0f;
        _S656[int(31)] = 0.0f;
        _S656[int(32)] = 0.0f;
        _S656[int(3)] = 0.0f;
        float _S657 = _S601[int(1)] + _S656[int(1)];
        float _S658 = _S601[int(2)] + _S656[int(2)];
        float _S659 = _S601[int(3)] + _S656[int(3)];
        float _S660 = _S601[int(4)] + _S656[int(4)];
        float _S661 = _S601[int(5)] + _S656[int(5)];
        float _S662 = _S601[int(6)] + _S656[int(6)];
        float _S663 = _S601[int(7)] + _S656[int(7)];
        float _S664 = _S601[int(8)] + _S656[int(8)];
        float _S665 = _S601[int(9)] + _S656[int(9)];
        float _S666 = _S601[int(10)] + _S656[int(10)];
        float _S667 = _S601[int(11)] + _S656[int(11)];
        float _S668 = _S601[int(12)] + _S656[int(12)];
        float _S669 = _S601[int(13)] + _S656[int(13)];
        float _S670 = _S601[int(14)] + _S656[int(14)];
        float _S671 = _S601[int(15)] + _S656[int(15)];
        float _S672 = _S601[int(16)] + _S656[int(16)];
        float _S673 = _S601[int(17)] + _S656[int(17)];
        float _S674 = _S601[int(18)] + _S656[int(18)];
        float _S675 = _S601[int(19)] + _S656[int(19)];
        float _S676 = _S601[int(20)] + _S656[int(20)];
        float _S677 = _S601[int(21)] + _S656[int(21)];
        float _S678 = _S601[int(22)] + _S656[int(22)];
        float _S679 = _S601[int(23)] + _S656[int(23)];
        float _S680 = _S601[int(24)] + _S656[int(24)];
        float _S681 = _S601[int(25)] + _S656[int(25)];
        float _S682 = _S601[int(26)] + _S656[int(26)];
        float _S683 = _S601[int(27)] + _S656[int(27)];
        float _S684 = _S601[int(28)] + _S656[int(28)];
        float _S685 = _S601[int(29)] + _S656[int(29)];
        float _S686 = _S601[int(30)] + _S656[int(30)];
        float _S687 = _S601[int(31)] + _S656[int(31)];
        float _S688 = _S601[int(32)] + _S656[int(32)];
        _S601[int(0)] = _S601[int(0)] + _S656[int(0)];
        _S601[int(1)] = _S657;
        _S601[int(2)] = _S658;
        _S601[int(3)] = _S659;
        _S601[int(4)] = _S660;
        _S601[int(5)] = _S661;
        _S601[int(6)] = _S662;
        _S601[int(7)] = _S663;
        _S601[int(8)] = _S664;
        _S601[int(9)] = _S665;
        _S601[int(10)] = _S666;
        _S601[int(11)] = _S667;
        _S601[int(12)] = _S668;
        _S601[int(13)] = _S669;
        _S601[int(14)] = _S670;
        _S601[int(15)] = _S671;
        _S601[int(16)] = _S672;
        _S601[int(17)] = _S673;
        _S601[int(18)] = _S674;
        _S601[int(19)] = _S675;
        _S601[int(20)] = _S676;
        _S601[int(21)] = _S677;
        _S601[int(22)] = _S678;
        _S601[int(23)] = _S679;
        _S601[int(24)] = _S680;
        _S601[int(25)] = _S681;
        _S601[int(26)] = _S682;
        _S601[int(27)] = _S683;
        _S601[int(28)] = _S684;
        _S601[int(29)] = _S685;
        _S601[int(30)] = _S686;
        _S601[int(31)] = _S687;
        _S601[int(32)] = _S688;
    }
    float _S689 = -10.0f * _S500;
    DiffPair_float_0 _S690;
    (&_S690)->primal_0 = _S448;
    (&_S690)->differential_0 = 0.0f;
    s_bwd_prop_log10_0(&_S690, _S689);
    float _S691 = _S690.differential_0 / _S447;
    float _S692 = _S446 * _S691;
    float _S693 = _S499 / _S447;
    float _S694 = _S446 * _S693;
    float _S695 = _S445[int(1)] * - _S691 + _S445[int(0)] * - _S693 + _S566;
    DiffPair_float_0 _S696;
    (&_S696)->primal_0 = _S445[int(22)];
    (&_S696)->differential_0 = 0.0f;
    DiffPair_float_0 _S697;
    (&_S697)->primal_0 = 1.0f;
    (&_S697)->differential_0 = 0.0f;
    _d_max_0(&_S696, &_S697, _S695);
    FixedArray<float, 33>  _S698;
    _S698[int(0)] = 0.0f;
    _S698[int(1)] = 0.0f;
    _S698[int(2)] = 0.0f;
    _S698[int(3)] = 0.0f;
    _S698[int(4)] = 0.0f;
    _S698[int(5)] = 0.0f;
    _S698[int(6)] = 0.0f;
    _S698[int(7)] = 0.0f;
    _S698[int(8)] = 0.0f;
    _S698[int(9)] = 0.0f;
    _S698[int(10)] = 0.0f;
    _S698[int(11)] = 0.0f;
    _S698[int(12)] = 0.0f;
    _S698[int(13)] = 0.0f;
    _S698[int(14)] = 0.0f;
    _S698[int(15)] = 0.0f;
    _S698[int(16)] = 0.0f;
    _S698[int(17)] = 0.0f;
    _S698[int(18)] = 0.0f;
    _S698[int(19)] = 0.0f;
    _S698[int(20)] = 0.0f;
    _S698[int(21)] = 0.0f;
    _S698[int(22)] = 0.0f;
    _S698[int(23)] = 0.0f;
    _S698[int(24)] = 0.0f;
    _S698[int(25)] = 0.0f;
    _S698[int(26)] = 0.0f;
    _S698[int(27)] = 0.0f;
    _S698[int(28)] = 0.0f;
    _S698[int(29)] = 0.0f;
    _S698[int(30)] = 0.0f;
    _S698[int(31)] = 0.0f;
    _S698[int(32)] = 0.0f;
    _S698[int(23)] = _S451;
    _S698[int(1)] = _S692;
    _S698[int(22)] = _S696.differential_0;
    _S698[int(0)] = _S694;
    FixedArray<float, 33>  _S699 = {
        _S601[int(0)] + _S698[int(0)], _S601[int(1)] + _S698[int(1)], _S601[int(2)] + _S698[int(2)], _S601[int(3)] + _S698[int(3)], _S601[int(4)] + _S698[int(4)], _S601[int(5)] + _S698[int(5)], _S601[int(6)] + _S698[int(6)], _S601[int(7)] + _S698[int(7)], _S601[int(8)] + _S698[int(8)], _S601[int(9)] + _S698[int(9)], _S601[int(10)] + _S698[int(10)], _S601[int(11)] + _S698[int(11)], _S601[int(12)] + _S698[int(12)], _S601[int(13)] + _S698[int(13)], _S601[int(14)] + _S698[int(14)], _S601[int(15)] + _S698[int(15)], _S601[int(16)] + _S698[int(16)], _S601[int(17)] + _S698[int(17)], _S601[int(18)] + _S698[int(18)], _S601[int(19)] + _S698[int(19)], _S601[int(20)] + _S698[int(20)], _S601[int(21)] + _S698[int(21)], _S601[int(22)] + _S698[int(22)], _S601[int(23)] + _S698[int(23)], _S601[int(24)] + _S698[int(24)], _S601[int(25)] + _S698[int(25)], _S601[int(26)] + _S698[int(26)], _S601[int(27)] + _S698[int(27)], _S601[int(28)] + _S698[int(28)], _S601[int(29)] + _S698[int(29)], _S601[int(30)] + _S698[int(30)], _S601[int(31)] + _S698[int(31)], _S601[int(32)] + _S698[int(32)]
    };
    dpraw_losses_0->primal_0 = dpraw_losses_0->primal_0;
    dpraw_losses_0->differential_0 = _S699;
    return;
}

inline __device__ void s_bwd_per_pixel_losses_reduce_0(DiffPair_arrayx3Cfloatx2C33x3E_0 * _S700, FixedArray<float, 21>  * _S701, FixedArray<float, 15>  * _S702)
{
    s_bwd_prop_per_pixel_losses_reduce_0(_S700, _S701, _S702);
    return;
}

inline __device__ void per_pixel_losses_reduce_bwd(FixedArray<float, 33>  raw_losses_1, FixedArray<float, 21>  weights_5, FixedArray<float, 15>  v_losses_1, FixedArray<float, 33>  * _S703)
{
    FixedArray<float, 33>  _S704 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    DiffPair_arrayx3Cfloatx2C33x3E_0 dp_raw_losses_0;
    (&dp_raw_losses_0)->primal_0 = raw_losses_1;
    (&dp_raw_losses_0)->differential_0 = _S704;
    FixedArray<float, 21>  _S705 = weights_5;
    FixedArray<float, 15>  _S706 = v_losses_1;
    s_bwd_per_pixel_losses_reduce_0(&dp_raw_losses_0, &_S705, &_S706);
    *_S703 = (&dp_raw_losses_0)->differential_0;
    return;
}

