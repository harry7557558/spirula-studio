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

inline __device__ void _d_sqrt_0(DiffPair_float_0 * dpx_6, float dOut_6)
{
    float _S14 = 0.5f / (F32_sqrt(((F32_max((1.00000001168609742e-07f), ((*dpx_6).primal_0)))))) * dOut_6;
    dpx_6->primal_0 = (*dpx_6).primal_0;
    dpx_6->differential_0 = _S14;
    return;
}

inline __device__ void _d_rsqrt_0(DiffPair_float_0 * dpx_7, float dOut_7)
{
    float _S15 = -0.5f / ((*dpx_7).primal_0 * (F32_sqrt(((*dpx_7).primal_0)))) * dOut_7;
    dpx_7->primal_0 = (*dpx_7).primal_0;
    dpx_7->differential_0 = _S15;
    return;
}

inline __device__ void _d_log_0(DiffPair_float_0 * dpx_8, float dOut_8)
{
    float _S16 = 1.0f / (*dpx_8).primal_0 * dOut_8;
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

inline __device__ void per_pixel_losses(float3  render_rgb_0, float3  ref_rgb_0, float render_depth_0, float ref_depth_0, float3  render_normal_0, float3  depth_normal_0, float3  ref_normal_0, float render_Ts_0, float3  rgb_dist_0, float depth_dist_0, float3  normal_dist_0, float median_depth_0, float3  median_normal_0, bool ref_alpha_0, bool has_mask_0, bool alpha_mask_0, float saturation_threshold_0, FixedArray<float, 19>  weights_0, FixedArray<float, 32>  * _S21)
{
    bool _S22;
    bool _S23;
    bool _S24;
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
    float3  _S25;
    float _S26 = render_rgb_0.x;
    float _S27 = render_rgb_0.y;
    float _S28 = render_rgb_0.z;
    float _S29 = ref_rgb_0.x;
    float _S30 = ref_rgb_0.y;
    float _S31 = ref_rgb_0.z;
    float dY_0 = 0.29899999499320984f * _S26 + 0.58700001239776611f * _S27 + 0.11400000005960464f * _S28 - (0.29899999499320984f * _S29 + 0.58700001239776611f * _S30 + 0.11400000005960464f * _S31);
    float dU_0 = -0.14712999761104584f * _S26 - 0.28885999321937561f * _S27 + 0.43599998950958252f * _S28 - (-0.14712999761104584f * _S29 - 0.28885999321937561f * _S30 + 0.43599998950958252f * _S31);
    float dV_0 = 0.61500000953674316f * _S26 - 0.51498997211456299f * _S27 - 0.10001000016927719f * _S28 - (0.61500000953674316f * _S29 - 0.51498997211456299f * _S30 - 0.10001000016927719f * _S31);
    float _S32 = float(mask_0);
    float3  _S33 = ref_rgb_0 - render_rgb_0;
    float3  _S34 = abs_0(_S33);
    float _S35 = dot_0(_S33, _S33) * 0.3333333432674408f;
    losses_0[int(0)] = _S32 * (weights_0[int(0)] * ((_S34.x + _S34.y + _S34.z) * 0.3333333432674408f) + weights_0[int(1)] * _S35 + weights_0[int(2)] * (F32_abs((dY_0))) + weights_0[int(3)] * dY_0 * dY_0 + weights_0[int(4)] * dU_0 * dU_0 + weights_0[int(5)] * dV_0 * dV_0);
    losses_0[int(1)] = _S32 * clamp_0(_S35, 0.0f, 1.0f);
    float _S36 = float(depth_mask_0 & mask_0);
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
            _S25 = make_float3 (0.0f);
            break;
        }
        _S25 = render_normal_0 * make_float3 ((F32_rsqrt((norm2_0))));
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
    float cos_sim_loss_0 = 0.5f - 0.5f * dot_0(_S25, _S44);
    losses_0[int(7)] = weights_0[int(7)] * _S47 * (cos_sim_loss_0 + (F32_sqrt(((F32_max((cos_sim_loss_0), (9.999999960041972e-13f)))))));
    float _S48 = float(_S45 & normal_mask_0);
    float cos_sim_loss_1 = 0.5f - 0.5f * dot_0(_S41, _S44);
    losses_0[int(8)] = weights_0[int(7)] * _S48 * (cos_sim_loss_1 + (F32_sqrt(((F32_max((cos_sim_loss_1), (9.999999960041972e-13f)))))));
    float _S49 = float((_S42 & _S45) & mask_0);
    float cos_sim_loss_2 = 0.5f - 0.5f * dot_0(_S25, _S41);
    losses_0[int(11)] = weights_0[int(10)] * _S49 * (cos_sim_loss_2 + (F32_sqrt(((F32_max((cos_sim_loss_2), (9.999999960041972e-13f)))))));
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
    float _S52 = float(mean_median_mask_0);
    losses_0[int(16)] = weights_0[int(15)] * _S52 * (F32_abs(((F32_log(((F32_max((render_depth_0), (1.00000001335143196e-10f)))))) - (F32_log(((F32_max((median_depth_0), (1.00000001335143196e-10f)))))))));
    float _S53 = float((_S51 & _S45) & mask_0);
    float cos_sim_loss_3 = 0.5f - 0.5f * dot_0(_S46, _S41);
    losses_0[int(17)] = weights_0[int(16)] * _S53 * (cos_sim_loss_3 + (F32_sqrt(((F32_max((cos_sim_loss_3), (9.999999960041972e-13f)))))));
    float _S54 = float(_S51 & normal_mask_0);
    float cos_sim_loss_4 = 0.5f - 0.5f * dot_0(_S46, _S44);
    losses_0[int(18)] = weights_0[int(17)] * _S54 * (cos_sim_loss_4 + (F32_sqrt(((F32_max((cos_sim_loss_4), (9.999999960041972e-13f)))))));
    float _S55 = float((_S51 & _S42) & mask_0);
    float cos_sim_loss_5 = 0.5f - 0.5f * dot_0(_S46, _S25);
    losses_0[int(19)] = weights_0[int(18)] * _S55 * (cos_sim_loss_5 + (F32_sqrt(((F32_max((cos_sim_loss_5), (9.999999960041972e-13f)))))));
    float render_alpha_0 = clamp_0(1.0f - render_Ts_0, 0.0f, 1.0f);
    float _S56 = float(alpha_mask_0);
    float _S57 = float(ref_alpha_0);
    float _S58 = (F32_max((render_alpha_0), (_S57)));
    losses_0[int(9)] = weights_0[int(8)] * _S56 * - lerp_0((F32_log(((F32_max((1.0f - _S58), (9.99999997475242708e-07f)))))), (F32_log(((F32_max((_S58), (9.99999997475242708e-07f)))))), _S57);
    float _S59 = 1.0f - render_alpha_0;
    float _S60 = 1.0f - _S57;
    float _S61 = (F32_max((_S59), (_S60)));
    losses_0[int(10)] = weights_0[int(9)] * _S56 * - lerp_0((F32_log(((F32_max((1.0f - _S61), (9.99999997475242708e-07f)))))), (F32_log(((F32_max((_S61), (9.99999997475242708e-07f)))))), _S60);
    losses_0[int(12)] = weights_0[int(11)] * _S32 * 4.0f * render_alpha_0 * _S59;
    losses_0[int(13)] = weights_0[int(12)] * _S32 * ((rgb_dist_0.x + rgb_dist_0.y + rgb_dist_0.z) * 0.3333333432674408f);
    losses_0[int(14)] = weights_0[int(13)] * _S32 * depth_dist_0;
    losses_0[int(15)] = weights_0[int(14)] * _S32 * ((normal_dist_0.x + normal_dist_0.y + normal_dist_0.z) * 0.3333333432674408f);
    losses_0[int(20)] = 1.0f;
    losses_0[int(21)] = _S32;
    losses_0[int(22)] = _S36;
    losses_0[int(23)] = _S47;
    losses_0[int(24)] = _S48;
    losses_0[int(25)] = _S49;
    if(alpha_mask_0)
    {
        mask_0 = !ref_alpha_0;
    }
    else
    {
        mask_0 = false;
    }
    losses_0[int(26)] = float(mask_0);
    if(alpha_mask_0)
    {
        mask_0 = ref_alpha_0;
    }
    else
    {
        mask_0 = false;
    }
    losses_0[int(27)] = float(mask_0);
    losses_0[int(28)] = _S52;
    losses_0[int(29)] = _S53;
    losses_0[int(30)] = _S54;
    losses_0[int(31)] = _S55;
    *_S21 = losses_0;
    return;
}

inline __device__ float s_primal_ctx_dot_0(float3  _S62, float3  _S63)
{
    return dot_0(_S62, _S63);
}

inline __device__ float s_primal_ctx_rsqrt_0(float _S64)
{
    return (F32_rsqrt((_S64)));
}

inline __device__ float s_primal_ctx_log_0(float _S65)
{
    return (F32_log((_S65)));
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

inline __device__ void s_bwd_prop_per_pixel_losses_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dprender_rgb_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpref_rgb_0, DiffPair_float_0 * dprender_depth_0, DiffPair_float_0 * dpref_depth_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dprender_normal_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpdepth_normal_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpref_normal_0, DiffPair_float_0 * dprender_Ts_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dprgb_dist_0, DiffPair_float_0 * dpdepth_dist_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpnormal_dist_0, DiffPair_float_0 * dpmedian_depth_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpmedian_normal_0, bool ref_alpha_1, bool has_mask_1, bool alpha_mask_1, float saturation_threshold_1, FixedArray<float, 19>  * weights_1, FixedArray<float, 32>  * _s_dOut_0)
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
    bool depth_mask_1 = (_S93.primal_0) != 0.0f;
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
    float _S138 = float(depth_mask_1 & mask_1);
    float _S139 = _S138 * (F32_max((_S92.primal_0), (0.00009999999747379f)));
    float _S140 = _S138 * (F32_max((_S93.primal_0), (0.00009999999747379f)));
    bool _S141 = normal_mask_1 & mask_1;
    float _S142 = s_primal_ctx_dot_0(_S94.primal_0, _S94.primal_0);
    bool _S143 = _S142 == 0.0f;
    float3  _S144;
    if(_S143)
    {
        _S144 = make_float3 (0.0f);
    }
    bool _S145 = !_S143;
    float3  _S146;
    if(_S145)
    {
        float _S147 = s_primal_ctx_rsqrt_0(_S142);
        float3  _S148 = make_float3 (_S147);
        _S144 = _S94.primal_0 * make_float3 (_S147);
        _S146 = _S148;
    }
    else
    {
        _S146 = _S100;
    }
    float _S149 = s_primal_ctx_dot_0(_S95.primal_0, _S95.primal_0);
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
        _S151 = _S95.primal_0 * make_float3 (_S154);
        _S153 = _S155;
    }
    else
    {
        _S153 = _S100;
    }
    float _S156 = s_primal_ctx_dot_0(_S96.primal_0, _S96.primal_0);
    bool _S157 = _S156 == 0.0f;
    float3  _S158;
    if(_S157)
    {
        float3  _S159 = make_float3 (0.0f);
        normal_mask_1 = false;
        _S158 = _S159;
    }
    else
    {
        normal_mask_1 = _S141;
    }
    bool _S160 = !_S157;
    float3  _S161;
    if(_S160)
    {
        float _S162 = s_primal_ctx_rsqrt_0(_S156);
        float3  _S163 = make_float3 (_S162);
        _S158 = _S96.primal_0 * make_float3 (_S162);
        _S161 = _S163;
    }
    else
    {
        _S161 = _S100;
    }
    float _S164 = (*weights_1)[int(7)] * float(_S145 & normal_mask_1);
    float cos_sim_loss_6 = 0.5f - 0.5f * s_primal_ctx_dot_0(_S144, _S158);
    float _S165 = (F32_max((cos_sim_loss_6), (9.999999960041972e-13f)));
    float _S166 = (*weights_1)[int(7)] * float(_S152 & normal_mask_1);
    float cos_sim_loss_7 = 0.5f - 0.5f * s_primal_ctx_dot_0(_S151, _S158);
    float _S167 = (F32_max((cos_sim_loss_7), (9.999999960041972e-13f)));
    float _S168 = (*weights_1)[int(10)] * float((_S145 & _S152) & mask_1);
    float cos_sim_loss_8 = 0.5f - 0.5f * s_primal_ctx_dot_0(_S144, _S151);
    float _S169 = (F32_max((cos_sim_loss_8), (9.999999960041972e-13f)));
    float _S170 = s_primal_ctx_dot_0(_S99.primal_0, _S99.primal_0);
    bool _S171 = _S170 == 0.0f;
    float3  _S172;
    if(_S171)
    {
        _S172 = make_float3 (0.0f);
    }
    bool _S173 = !_S171;
    float3  _S174;
    if(_S173)
    {
        float _S175 = s_primal_ctx_rsqrt_0(_S170);
        float3  _S176 = make_float3 (_S175);
        _S172 = _S99.primal_0 * make_float3 (_S175);
        _S174 = _S176;
    }
    else
    {
        _S174 = _S100;
    }
    bool mean_median_mask_1;
    if(mask_1)
    {
        mean_median_mask_1 = (_S92.primal_0) > 1.00000001335143196e-10f;
    }
    else
    {
        mean_median_mask_1 = false;
    }
    if(mean_median_mask_1)
    {
        mean_median_mask_1 = (_S98.primal_0) > 1.00000001335143196e-10f;
    }
    else
    {
        mean_median_mask_1 = false;
    }
    float _S177 = (*weights_1)[int(15)] * float(mean_median_mask_1);
    float _S178 = (F32_max((_S92.primal_0), (1.00000001335143196e-10f)));
    float _S179 = (F32_max((_S98.primal_0), (1.00000001335143196e-10f)));
    float _S180 = s_primal_ctx_log_0(_S178) - s_primal_ctx_log_0(_S179);
    float _S181 = (*weights_1)[int(16)] * float((_S173 & _S152) & mask_1);
    float cos_sim_loss_9 = 0.5f - 0.5f * s_primal_ctx_dot_0(_S172, _S151);
    float _S182 = (F32_max((cos_sim_loss_9), (9.999999960041972e-13f)));
    float _S183 = (*weights_1)[int(17)] * float(_S173 & normal_mask_1);
    float cos_sim_loss_10 = 0.5f - 0.5f * s_primal_ctx_dot_0(_S172, _S158);
    float _S184 = (F32_max((cos_sim_loss_10), (9.999999960041972e-13f)));
    float _S185 = (*weights_1)[int(18)] * float((_S173 & _S145) & mask_1);
    float cos_sim_loss_11 = 0.5f - 0.5f * s_primal_ctx_dot_0(_S172, _S144);
    float _S186 = (F32_max((cos_sim_loss_11), (9.999999960041972e-13f)));
    float _S187 = 1.0f - _S97.primal_0;
    float _S188 = s_primal_ctx_clamp_0(_S187, 0.0f, 1.0f);
    float _S189 = float(alpha_mask_1);
    float _S190 = (*weights_1)[int(8)] * _S189;
    float _S191 = float(ref_alpha_1);
    float _S192 = (F32_max((_S188), (_S191)));
    float _S193 = 1.0f - _S192;
    float _S194 = (F32_max((_S193), (9.99999997475242708e-07f)));
    float _S195 = s_primal_ctx_log_0(_S194);
    float _S196 = (F32_max((_S192), (9.99999997475242708e-07f)));
    float _S197 = s_primal_ctx_log_0(_S196);
    float _S198 = (*weights_1)[int(9)] * _S189;
    float _S199 = 1.0f - _S188;
    float _S200 = 1.0f - _S191;
    float _S201 = (F32_max((_S199), (_S200)));
    float _S202 = 1.0f - _S201;
    float _S203 = (F32_max((_S202), (9.99999997475242708e-07f)));
    float _S204 = s_primal_ctx_log_0(_S203);
    float _S205 = (F32_max((_S201), (9.99999997475242708e-07f)));
    float _S206 = s_primal_ctx_log_0(_S205);
    float _S207 = (*weights_1)[int(11)] * _S126 * 4.0f;
    float _S208 = _S207 * _S188;
    float _S209 = (*weights_1)[int(12)] * _S126;
    float _S210 = (*weights_1)[int(13)] * _S126;
    float _S211 = (*weights_1)[int(14)] * _S126;
    float _S212 = (*_s_dOut_0)[int(0)];
    float _S213 = (*_s_dOut_0)[int(1)];
    float _S214 = (*_s_dOut_0)[int(2)];
    float _S215 = (*_s_dOut_0)[int(3)];
    float _S216 = (*_s_dOut_0)[int(4)];
    float _S217 = (*_s_dOut_0)[int(5)];
    float _S218 = (*_s_dOut_0)[int(6)];
    float _S219 = (*_s_dOut_0)[int(7)];
    float _S220 = (*_s_dOut_0)[int(8)];
    float _S221 = (*_s_dOut_0)[int(11)];
    float _S222 = 0.3333333432674408f * (_S211 * (*_s_dOut_0)[int(15)]);
    float _S223 = _S210 * (*_s_dOut_0)[int(14)];
    float _S224 = 0.3333333432674408f * (_S209 * (*_s_dOut_0)[int(13)]);
    float _S225 = _S208 * (*_s_dOut_0)[int(12)];
    float _S226 = _S207 * (_S199 * (*_s_dOut_0)[int(12)]);
    float _S227 = - (_S198 * (*_s_dOut_0)[int(10)]);
    DiffPair_float_0 _S228;
    (&_S228)->primal_0 = _S204;
    (&_S228)->differential_0 = 0.0f;
    DiffPair_float_0 _S229;
    (&_S229)->primal_0 = _S206;
    (&_S229)->differential_0 = 0.0f;
    DiffPair_float_0 _S230;
    (&_S230)->primal_0 = _S200;
    (&_S230)->differential_0 = 0.0f;
    s_bwd_prop_lerp_0(&_S228, &_S229, &_S230, _S227);
    DiffPair_float_0 _S231;
    (&_S231)->primal_0 = _S205;
    (&_S231)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S231, _S229.differential_0);
    DiffPair_float_0 _S232;
    (&_S232)->primal_0 = _S201;
    (&_S232)->differential_0 = 0.0f;
    DiffPair_float_0 _S233;
    (&_S233)->primal_0 = 9.99999997475242708e-07f;
    (&_S233)->differential_0 = 0.0f;
    _d_max_0(&_S232, &_S233, _S231.differential_0);
    DiffPair_float_0 _S234;
    (&_S234)->primal_0 = _S203;
    (&_S234)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S234, _S228.differential_0);
    DiffPair_float_0 _S235;
    (&_S235)->primal_0 = _S202;
    (&_S235)->differential_0 = 0.0f;
    DiffPair_float_0 _S236;
    (&_S236)->primal_0 = 9.99999997475242708e-07f;
    (&_S236)->differential_0 = 0.0f;
    _d_max_0(&_S235, &_S236, _S234.differential_0);
    float _S237 = _S232.differential_0 + - _S235.differential_0;
    DiffPair_float_0 _S238;
    (&_S238)->primal_0 = _S199;
    (&_S238)->differential_0 = 0.0f;
    DiffPair_float_0 _S239;
    (&_S239)->primal_0 = _S200;
    (&_S239)->differential_0 = 0.0f;
    _d_max_0(&_S238, &_S239, _S237);
    float _S240 = - (_S225 + _S238.differential_0);
    float _S241 = - (_S190 * (*_s_dOut_0)[int(9)]);
    DiffPair_float_0 _S242;
    (&_S242)->primal_0 = _S195;
    (&_S242)->differential_0 = 0.0f;
    DiffPair_float_0 _S243;
    (&_S243)->primal_0 = _S197;
    (&_S243)->differential_0 = 0.0f;
    DiffPair_float_0 _S244;
    (&_S244)->primal_0 = _S191;
    (&_S244)->differential_0 = 0.0f;
    s_bwd_prop_lerp_0(&_S242, &_S243, &_S244, _S241);
    DiffPair_float_0 _S245;
    (&_S245)->primal_0 = _S196;
    (&_S245)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S245, _S243.differential_0);
    DiffPair_float_0 _S246;
    (&_S246)->primal_0 = _S192;
    (&_S246)->differential_0 = 0.0f;
    DiffPair_float_0 _S247;
    (&_S247)->primal_0 = 9.99999997475242708e-07f;
    (&_S247)->differential_0 = 0.0f;
    _d_max_0(&_S246, &_S247, _S245.differential_0);
    DiffPair_float_0 _S248;
    (&_S248)->primal_0 = _S194;
    (&_S248)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S248, _S242.differential_0);
    DiffPair_float_0 _S249;
    (&_S249)->primal_0 = _S193;
    (&_S249)->differential_0 = 0.0f;
    DiffPair_float_0 _S250;
    (&_S250)->primal_0 = 9.99999997475242708e-07f;
    (&_S250)->differential_0 = 0.0f;
    _d_max_0(&_S249, &_S250, _S248.differential_0);
    float _S251 = _S246.differential_0 + - _S249.differential_0;
    DiffPair_float_0 _S252;
    (&_S252)->primal_0 = _S188;
    (&_S252)->differential_0 = 0.0f;
    DiffPair_float_0 _S253;
    (&_S253)->primal_0 = _S191;
    (&_S253)->differential_0 = 0.0f;
    _d_max_0(&_S252, &_S253, _S251);
    float _S254 = _S226 + _S240 + _S252.differential_0;
    DiffPair_float_0 _S255;
    (&_S255)->primal_0 = _S187;
    (&_S255)->differential_0 = 0.0f;
    DiffPair_float_0 _S256;
    (&_S256)->primal_0 = 0.0f;
    (&_S256)->differential_0 = 0.0f;
    DiffPair_float_0 _S257;
    (&_S257)->primal_0 = 1.0f;
    (&_S257)->differential_0 = 0.0f;
    s_bwd_prop_clamp_0(&_S255, &_S256, &_S257, _S254);
    float _S258 = - _S255.differential_0;
    float _S259 = _S185 * (*_s_dOut_0)[int(19)];
    DiffPair_float_0 _S260;
    (&_S260)->primal_0 = _S186;
    (&_S260)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S260, _S259);
    DiffPair_float_0 _S261;
    (&_S261)->primal_0 = cos_sim_loss_11;
    (&_S261)->differential_0 = 0.0f;
    DiffPair_float_0 _S262;
    (&_S262)->primal_0 = 9.999999960041972e-13f;
    (&_S262)->differential_0 = 0.0f;
    _d_max_0(&_S261, &_S262, _S260.differential_0);
    float _S263 = 0.5f * - (_S259 + _S261.differential_0);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S264;
    (&_S264)->primal_0 = _S172;
    (&_S264)->differential_0 = _S100;
    float3  _S265 = _S144;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S266;
    (&_S266)->primal_0 = _S144;
    (&_S266)->differential_0 = _S100;
    s_bwd_prop_dot_0(&_S264, &_S266, _S263);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S267 = _S266;
    float _S268 = _S183 * (*_s_dOut_0)[int(18)];
    DiffPair_float_0 _S269;
    (&_S269)->primal_0 = _S184;
    (&_S269)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S269, _S268);
    DiffPair_float_0 _S270;
    (&_S270)->primal_0 = cos_sim_loss_10;
    (&_S270)->differential_0 = 0.0f;
    DiffPair_float_0 _S271;
    (&_S271)->primal_0 = 9.999999960041972e-13f;
    (&_S271)->differential_0 = 0.0f;
    _d_max_0(&_S270, &_S271, _S269.differential_0);
    float _S272 = 0.5f * - (_S268 + _S270.differential_0);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S273;
    (&_S273)->primal_0 = _S172;
    (&_S273)->differential_0 = _S100;
    float3  _S274 = _S158;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S275;
    (&_S275)->primal_0 = _S158;
    (&_S275)->differential_0 = _S100;
    s_bwd_prop_dot_0(&_S273, &_S275, _S272);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S276 = _S275;
    float _S277 = _S181 * (*_s_dOut_0)[int(17)];
    DiffPair_float_0 _S278;
    (&_S278)->primal_0 = _S182;
    (&_S278)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S278, _S277);
    DiffPair_float_0 _S279;
    (&_S279)->primal_0 = cos_sim_loss_9;
    (&_S279)->differential_0 = 0.0f;
    DiffPair_float_0 _S280;
    (&_S280)->primal_0 = 9.999999960041972e-13f;
    (&_S280)->differential_0 = 0.0f;
    _d_max_0(&_S279, &_S280, _S278.differential_0);
    float _S281 = 0.5f * - (_S277 + _S279.differential_0);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S282;
    (&_S282)->primal_0 = _S172;
    (&_S282)->differential_0 = _S100;
    float3  _S283 = _S151;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S284;
    (&_S284)->primal_0 = _S151;
    (&_S284)->differential_0 = _S100;
    s_bwd_prop_dot_0(&_S282, &_S284, _S281);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S285 = _S284;
    float _S286 = _S177 * (*_s_dOut_0)[int(16)];
    DiffPair_float_0 _S287;
    (&_S287)->primal_0 = _S180;
    (&_S287)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S287, _S286);
    float _S288 = - _S287.differential_0;
    DiffPair_float_0 _S289;
    (&_S289)->primal_0 = _S179;
    (&_S289)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S289, _S288);
    DiffPair_float_0 _S290;
    (&_S290)->primal_0 = _S98.primal_0;
    (&_S290)->differential_0 = 0.0f;
    DiffPair_float_0 _S291;
    (&_S291)->primal_0 = 1.00000001335143196e-10f;
    (&_S291)->differential_0 = 0.0f;
    _d_max_0(&_S290, &_S291, _S289.differential_0);
    DiffPair_float_0 _S292 = _S290;
    DiffPair_float_0 _S293;
    (&_S293)->primal_0 = _S178;
    (&_S293)->differential_0 = 0.0f;
    s_bwd_prop_log_0(&_S293, _S287.differential_0);
    DiffPair_float_0 _S294;
    (&_S294)->primal_0 = _S92.primal_0;
    (&_S294)->differential_0 = 0.0f;
    DiffPair_float_0 _S295;
    (&_S295)->primal_0 = 1.00000001335143196e-10f;
    (&_S295)->differential_0 = 0.0f;
    _d_max_0(&_S294, &_S295, _S293.differential_0);
    DiffPair_float_0 _S296 = _S294;
    float3  _S297 = make_float3 (_S222, _S222, _S222);
    float3  _S298 = make_float3 (_S224, _S224, _S224);
    float3  _S299 = _S264.differential_0 + _S273.differential_0 + _S282.differential_0;
    float _S300;
    if(_S173)
    {
        float3  _S301 = _S99.primal_0 * _S299;
        float3  _S302 = _S174 * _S299;
        float _S303 = _S301.x + _S301.y + _S301.z;
        DiffPair_float_0 _S304;
        (&_S304)->primal_0 = _S170;
        (&_S304)->differential_0 = 0.0f;
        s_bwd_prop_rsqrt_0(&_S304, _S303);
        _S300 = _S304.differential_0;
        _S144 = _S302;
    }
    else
    {
        _S300 = 0.0f;
        _S144 = _S100;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S305;
    (&_S305)->primal_0 = _S99.primal_0;
    (&_S305)->differential_0 = _S100;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S306;
    (&_S306)->primal_0 = _S99.primal_0;
    (&_S306)->differential_0 = _S100;
    s_bwd_prop_dot_0(&_S305, &_S306, _S300);
    float _S307 = _S168 * _S221;
    DiffPair_float_0 _S308;
    (&_S308)->primal_0 = _S169;
    (&_S308)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S308, _S307);
    DiffPair_float_0 _S309;
    (&_S309)->primal_0 = cos_sim_loss_8;
    (&_S309)->differential_0 = 0.0f;
    DiffPair_float_0 _S310;
    (&_S310)->primal_0 = 9.999999960041972e-13f;
    (&_S310)->differential_0 = 0.0f;
    _d_max_0(&_S309, &_S310, _S308.differential_0);
    float _S311 = 0.5f * - (_S307 + _S309.differential_0);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S312;
    (&_S312)->primal_0 = _S265;
    (&_S312)->differential_0 = _S100;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S313;
    (&_S313)->primal_0 = _S283;
    (&_S313)->differential_0 = _S100;
    s_bwd_prop_dot_0(&_S312, &_S313, _S311);
    float _S314 = _S166 * _S220;
    DiffPair_float_0 _S315;
    (&_S315)->primal_0 = _S167;
    (&_S315)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S315, _S314);
    DiffPair_float_0 _S316;
    (&_S316)->primal_0 = cos_sim_loss_7;
    (&_S316)->differential_0 = 0.0f;
    DiffPair_float_0 _S317;
    (&_S317)->primal_0 = 9.999999960041972e-13f;
    (&_S317)->differential_0 = 0.0f;
    _d_max_0(&_S316, &_S317, _S315.differential_0);
    float _S318 = 0.5f * - (_S314 + _S316.differential_0);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S319;
    (&_S319)->primal_0 = _S283;
    (&_S319)->differential_0 = _S100;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S320;
    (&_S320)->primal_0 = _S274;
    (&_S320)->differential_0 = _S100;
    s_bwd_prop_dot_0(&_S319, &_S320, _S318);
    float _S321 = _S164 * _S219;
    DiffPair_float_0 _S322;
    (&_S322)->primal_0 = _S165;
    (&_S322)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S322, _S321);
    DiffPair_float_0 _S323;
    (&_S323)->primal_0 = cos_sim_loss_6;
    (&_S323)->differential_0 = 0.0f;
    DiffPair_float_0 _S324;
    (&_S324)->primal_0 = 9.999999960041972e-13f;
    (&_S324)->differential_0 = 0.0f;
    _d_max_0(&_S323, &_S324, _S322.differential_0);
    float _S325 = 0.5f * - (_S321 + _S323.differential_0);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S326;
    (&_S326)->primal_0 = _S265;
    (&_S326)->differential_0 = _S100;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S327;
    (&_S327)->primal_0 = _S274;
    (&_S327)->differential_0 = _S100;
    s_bwd_prop_dot_0(&_S326, &_S327, _S325);
    float3  _S328 = _S306.differential_0 + _S305.differential_0 + _S144;
    float3  _S329 = _S313.differential_0 + _S319.differential_0 + _S285.differential_0;
    float3  _S330 = _S312.differential_0 + _S326.differential_0 + _S267.differential_0;
    float3  _S331 = _S320.differential_0 + _S327.differential_0 + _S276.differential_0;
    if(_S160)
    {
        float3  _S332 = _S96.primal_0 * _S331;
        float3  _S333 = _S161 * _S331;
        float _S334 = _S332.x + _S332.y + _S332.z;
        DiffPair_float_0 _S335;
        (&_S335)->primal_0 = _S156;
        (&_S335)->differential_0 = 0.0f;
        s_bwd_prop_rsqrt_0(&_S335, _S334);
        _S300 = _S335.differential_0;
        _S144 = _S333;
    }
    else
    {
        _S300 = 0.0f;
        _S144 = _S100;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S336;
    (&_S336)->primal_0 = _S96.primal_0;
    (&_S336)->differential_0 = _S100;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S337;
    (&_S337)->primal_0 = _S96.primal_0;
    (&_S337)->differential_0 = _S100;
    s_bwd_prop_dot_0(&_S336, &_S337, _S300);
    float3  _S338 = _S337.differential_0 + _S336.differential_0 + _S144;
    if(_S152)
    {
        float3  _S339 = _S95.primal_0 * _S329;
        float3  _S340 = _S153 * _S329;
        float _S341 = _S339.x + _S339.y + _S339.z;
        DiffPair_float_0 _S342;
        (&_S342)->primal_0 = _S149;
        (&_S342)->differential_0 = 0.0f;
        s_bwd_prop_rsqrt_0(&_S342, _S341);
        _S300 = _S342.differential_0;
        _S144 = _S340;
    }
    else
    {
        _S300 = 0.0f;
        _S144 = _S100;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S343;
    (&_S343)->primal_0 = _S95.primal_0;
    (&_S343)->differential_0 = _S100;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S344;
    (&_S344)->primal_0 = _S95.primal_0;
    (&_S344)->differential_0 = _S100;
    s_bwd_prop_dot_0(&_S343, &_S344, _S300);
    float3  _S345 = _S344.differential_0 + _S343.differential_0 + _S144;
    if(_S145)
    {
        float3  _S346 = _S94.primal_0 * _S330;
        float3  _S347 = _S146 * _S330;
        float _S348 = _S346.x + _S346.y + _S346.z;
        DiffPair_float_0 _S349;
        (&_S349)->primal_0 = _S142;
        (&_S349)->differential_0 = 0.0f;
        s_bwd_prop_rsqrt_0(&_S349, _S348);
        _S300 = _S349.differential_0;
        _S144 = _S347;
    }
    else
    {
        _S300 = 0.0f;
        _S144 = _S100;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S350;
    (&_S350)->primal_0 = _S94.primal_0;
    (&_S350)->differential_0 = _S100;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S351;
    (&_S351)->primal_0 = _S94.primal_0;
    (&_S351)->differential_0 = _S100;
    s_bwd_prop_dot_0(&_S350, &_S351, _S300);
    float3  _S352 = _S351.differential_0 + _S350.differential_0 + _S144;
    float _S353 = _S140 * _S218;
    float _S354 = _S140 * _S217;
    float _S355 = _S139 * _S216;
    float _S356 = _S138 * (_S139 * _S218 + _S354 + _S354 + _S215);
    DiffPair_float_0 _S357;
    (&_S357)->primal_0 = _S93.primal_0;
    (&_S357)->differential_0 = 0.0f;
    DiffPair_float_0 _S358;
    (&_S358)->primal_0 = 0.00009999999747379f;
    (&_S358)->differential_0 = 0.0f;
    _d_max_0(&_S357, &_S358, _S356);
    DiffPair_float_0 _S359 = _S357;
    float _S360 = _S138 * (_S353 + _S355 + _S355 + _S214);
    DiffPair_float_0 _S361;
    (&_S361)->primal_0 = _S92.primal_0;
    (&_S361)->differential_0 = 0.0f;
    DiffPair_float_0 _S362;
    (&_S362)->primal_0 = 0.00009999999747379f;
    (&_S362)->differential_0 = 0.0f;
    _d_max_0(&_S361, &_S362, _S360);
    float _S363 = _S126 * _S213;
    DiffPair_float_0 _S364;
    (&_S364)->primal_0 = _S130;
    (&_S364)->differential_0 = 0.0f;
    DiffPair_float_0 _S365;
    (&_S365)->primal_0 = 0.0f;
    (&_S365)->differential_0 = 0.0f;
    DiffPair_float_0 _S366;
    (&_S366)->primal_0 = 1.0f;
    (&_S366)->differential_0 = 0.0f;
    s_bwd_prop_clamp_0(&_S364, &_S365, &_S366, _S363);
    float _S367 = _S126 * _S212;
    float _S368 = _S137 * _S367;
    float _S369 = _S136 * (dV_1 * _S367);
    float _S370 = _S135 * _S367;
    float _S371 = _S134 * (dU_1 * _S367);
    float _S372 = _S133 * _S367;
    float _S373 = _S132 * (dY_1 * _S367);
    float _S374 = _S131 * _S367;
    DiffPair_float_0 _S375;
    (&_S375)->primal_0 = dY_1;
    (&_S375)->differential_0 = 0.0f;
    s_bwd_prop_abs_0(&_S375, _S374);
    float _S376 = 0.3333333432674408f * (_S364.differential_0 + _S129 * _S367);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S377;
    (&_S377)->primal_0 = _S128;
    (&_S377)->differential_0 = _S100;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S378;
    (&_S378)->primal_0 = _S128;
    (&_S378)->differential_0 = _S100;
    s_bwd_prop_dot_0(&_S377, &_S378, _S376);
    float _S379 = 0.3333333432674408f * (_S127 * _S367);
    float3  _S380 = make_float3 (_S379, _S379, _S379);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S381;
    (&_S381)->primal_0 = _S128;
    (&_S381)->differential_0 = _S100;
    s_bwd_prop_abs_1(&_S381, _S380);
    float3  _S382 = _S378.differential_0 + _S377.differential_0 + _S381.differential_0;
    float _S383 = _S368 + _S369;
    float s_diff_V_T_0 = - _S383;
    float _S384 = _S370 + _S371;
    float s_diff_U_T_0 = - _S384;
    float _S385 = _S372 + _S373 + _S375.differential_0;
    float s_diff_Y_T_0 = - _S385;
    float _S386 = - s_diff_V_T_0;
    float _S387 = _S361.differential_0 + _S296.differential_0;
    float3  _S388 = _S382 + make_float3 (0.61500000953674316f * s_diff_V_T_0 + -0.14712999761104584f * s_diff_U_T_0 + 0.29899999499320984f * s_diff_Y_T_0, 0.51498997211456299f * _S386 + 0.28885999321937561f * - s_diff_U_T_0 + 0.58700001239776611f * s_diff_Y_T_0, 0.10001000016927719f * _S386 + 0.43599998950958252f * s_diff_U_T_0 + 0.11400000005960464f * s_diff_Y_T_0);
    float3  _S389 = - _S382 + make_float3 (0.61500000953674316f * _S383 + -0.14712999761104584f * _S384 + 0.29899999499320984f * _S385, 0.51498997211456299f * s_diff_V_T_0 + 0.28885999321937561f * s_diff_U_T_0 + 0.58700001239776611f * _S385, 0.10001000016927719f * s_diff_V_T_0 + 0.43599998950958252f * _S384 + 0.11400000005960464f * _S385);
    if(_S119)
    {
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S390;
        (&_S390)->primal_0 = _S96.primal_0;
        (&_S390)->differential_0 = _S100;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S391;
        (&_S391)->primal_0 = _S96.primal_0;
        (&_S391)->differential_0 = _S100;
        s_bwd_prop_dot_0(&_S390, &_S391, 0.0f);
        _S144 = _S391.differential_0 + _S390.differential_0 + _S338;
    }
    else
    {
        _S144 = _S338;
    }
    if(_S102)
    {
        DiffPair_float_0 _S392;
        (&_S392)->primal_0 = _S111;
        (&_S392)->differential_0 = 0.0f;
        DiffPair_float_0 _S393;
        (&_S393)->primal_0 = _S112;
        (&_S393)->differential_0 = 0.0f;
        _d_min_0(&_S392, &_S393, 0.0f);
        DiffPair_float_0 _S394;
        (&_S394)->primal_0 = _S113;
        (&_S394)->differential_0 = 0.0f;
        DiffPair_float_0 _S395;
        (&_S395)->primal_0 = _S114;
        (&_S395)->differential_0 = 0.0f;
        _d_min_0(&_S394, &_S395, _S392.differential_0);
        _S146 = _S388 + make_float3 (_S394.differential_0, _S395.differential_0, _S393.differential_0);
    }
    else
    {
        _S146 = _S388;
    }
    if(_S101)
    {
        DiffPair_float_0 _S396;
        (&_S396)->primal_0 = _S103;
        (&_S396)->differential_0 = 0.0f;
        DiffPair_float_0 _S397;
        (&_S397)->primal_0 = _S104;
        (&_S397)->differential_0 = 0.0f;
        _d_min_0(&_S396, &_S397, 0.0f);
        DiffPair_float_0 _S398;
        (&_S398)->primal_0 = _S105;
        (&_S398)->differential_0 = 0.0f;
        DiffPair_float_0 _S399;
        (&_S399)->primal_0 = _S106;
        (&_S399)->differential_0 = 0.0f;
        _d_min_0(&_S398, &_S399, _S396.differential_0);
        _S151 = _S389 + make_float3 (_S398.differential_0, _S399.differential_0, _S397.differential_0);
    }
    else
    {
        _S151 = _S389;
    }
    dpmedian_normal_0->primal_0 = (*dpmedian_normal_0).primal_0;
    dpmedian_normal_0->differential_0 = _S328;
    dpmedian_depth_0->primal_0 = (*dpmedian_depth_0).primal_0;
    dpmedian_depth_0->differential_0 = _S292.differential_0;
    dpnormal_dist_0->primal_0 = (*dpnormal_dist_0).primal_0;
    dpnormal_dist_0->differential_0 = _S297;
    dpdepth_dist_0->primal_0 = (*dpdepth_dist_0).primal_0;
    dpdepth_dist_0->differential_0 = _S223;
    dprgb_dist_0->primal_0 = (*dprgb_dist_0).primal_0;
    dprgb_dist_0->differential_0 = _S298;
    dprender_Ts_0->primal_0 = (*dprender_Ts_0).primal_0;
    dprender_Ts_0->differential_0 = _S258;
    dpref_normal_0->primal_0 = (*dpref_normal_0).primal_0;
    dpref_normal_0->differential_0 = _S144;
    dpdepth_normal_0->primal_0 = (*dpdepth_normal_0).primal_0;
    dpdepth_normal_0->differential_0 = _S345;
    dprender_normal_0->primal_0 = (*dprender_normal_0).primal_0;
    dprender_normal_0->differential_0 = _S352;
    dpref_depth_0->primal_0 = (*dpref_depth_0).primal_0;
    dpref_depth_0->differential_0 = _S359.differential_0;
    dprender_depth_0->primal_0 = (*dprender_depth_0).primal_0;
    dprender_depth_0->differential_0 = _S387;
    dpref_rgb_0->primal_0 = (*dpref_rgb_0).primal_0;
    dpref_rgb_0->differential_0 = _S146;
    dprender_rgb_0->primal_0 = (*dprender_rgb_0).primal_0;
    dprender_rgb_0->differential_0 = _S151;
    return;
}

inline __device__ void s_bwd_per_pixel_losses_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S400, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S401, DiffPair_float_0 * _S402, DiffPair_float_0 * _S403, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S404, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S405, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S406, DiffPair_float_0 * _S407, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S408, DiffPair_float_0 * _S409, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S410, DiffPair_float_0 * _S411, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S412, bool _S413, bool _S414, bool _S415, float _S416, FixedArray<float, 19>  * _S417, FixedArray<float, 32>  * _S418)
{
    s_bwd_prop_per_pixel_losses_0(_S400, _S401, _S402, _S403, _S404, _S405, _S406, _S407, _S408, _S409, _S410, _S411, _S412, _S413, _S414, _S415, _S416, _S417, _S418);
    return;
}

inline __device__ void per_pixel_losses_bwd(float3  render_rgb_1, float3  ref_rgb_1, float render_depth_1, float ref_depth_1, float3  render_normal_1, float3  depth_normal_1, float3  ref_normal_1, float render_Ts_1, float3  rgb_dist_1, float depth_dist_1, float3  normal_dist_1, float median_depth_1, float3  median_normal_1, bool ref_alpha_2, bool has_mask_2, bool alpha_mask_2, float saturation_threshold_2, FixedArray<float, 19>  weights_2, FixedArray<float, 32>  v_losses_0, float3  * v_render_rgb_0, float3  * v_ref_rgb_0, float * v_render_depth_0, float * v_ref_depth_0, float3  * v_render_normal_0, float3  * v_depth_normal_0, float3  * v_ref_normal_0, float * v_render_Ts_0, float3  * v_rgb_dist_0, float * v_depth_dist_0, float3  * v_normal_dist_0, float * v_median_depth_0, float3  * v_median_normal_0)
{
    float3  _S419 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_render_rgb_0;
    (&dp_render_rgb_0)->primal_0 = render_rgb_1;
    (&dp_render_rgb_0)->differential_0 = _S419;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ref_rgb_0;
    (&dp_ref_rgb_0)->primal_0 = ref_rgb_1;
    (&dp_ref_rgb_0)->differential_0 = _S419;
    DiffPair_float_0 dp_render_depth_0;
    (&dp_render_depth_0)->primal_0 = render_depth_1;
    (&dp_render_depth_0)->differential_0 = 0.0f;
    DiffPair_float_0 dp_ref_depth_0;
    (&dp_ref_depth_0)->primal_0 = ref_depth_1;
    (&dp_ref_depth_0)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_render_normal_0;
    (&dp_render_normal_0)->primal_0 = render_normal_1;
    (&dp_render_normal_0)->differential_0 = _S419;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_depth_normal_0;
    (&dp_depth_normal_0)->primal_0 = depth_normal_1;
    (&dp_depth_normal_0)->differential_0 = _S419;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ref_normal_0;
    (&dp_ref_normal_0)->primal_0 = ref_normal_1;
    (&dp_ref_normal_0)->differential_0 = _S419;
    DiffPair_float_0 dp_render_Ts_0;
    (&dp_render_Ts_0)->primal_0 = render_Ts_1;
    (&dp_render_Ts_0)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_rgb_dist_0;
    (&dp_rgb_dist_0)->primal_0 = rgb_dist_1;
    (&dp_rgb_dist_0)->differential_0 = _S419;
    DiffPair_float_0 dp_depth_dist_0;
    (&dp_depth_dist_0)->primal_0 = depth_dist_1;
    (&dp_depth_dist_0)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_normal_dist_0;
    (&dp_normal_dist_0)->primal_0 = normal_dist_1;
    (&dp_normal_dist_0)->differential_0 = _S419;
    DiffPair_float_0 dp_median_depth_0;
    (&dp_median_depth_0)->primal_0 = median_depth_1;
    (&dp_median_depth_0)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_median_normal_0;
    (&dp_median_normal_0)->primal_0 = median_normal_1;
    (&dp_median_normal_0)->differential_0 = _S419;
    FixedArray<float, 19>  _S420 = weights_2;
    FixedArray<float, 32>  _S421 = v_losses_0;
    s_bwd_per_pixel_losses_0(&dp_render_rgb_0, &dp_ref_rgb_0, &dp_render_depth_0, &dp_ref_depth_0, &dp_render_normal_0, &dp_depth_normal_0, &dp_ref_normal_0, &dp_render_Ts_0, &dp_rgb_dist_0, &dp_depth_dist_0, &dp_normal_dist_0, &dp_median_depth_0, &dp_median_normal_0, ref_alpha_2, has_mask_2, alpha_mask_2, saturation_threshold_2, &_S420, &_S421);
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
    float _S422 = 1.0f / ((*dpx_10).primal_0 * 2.30258512496948242f) * dOut_10;
    dpx_10->primal_0 = (*dpx_10).primal_0;
    dpx_10->differential_0 = _S422;
    return;
}

inline __device__ void per_pixel_losses_reduce(FixedArray<float, 32>  raw_losses_0, FixedArray<float, 19>  weights_3, FixedArray<float, 14>  * _S423)
{
    FixedArray<float, 14>  losses_1;
    float _S424 = (F32_max((raw_losses_0[int(21)]), (1.0f)));
    losses_1[int(0)] = raw_losses_0[int(0)] / _S424;
    losses_1[int(1)] = -10.0f * (F32_log10((raw_losses_0[int(1)] / _S424)));
    bool _S425;
    if((raw_losses_0[int(22)]) > 0.0f)
    {
        _S425 = (raw_losses_0[int(3)]) != 0.0f;
    }
    else
    {
        _S425 = false;
    }
    float _S426;
    if(_S425)
    {
        _S426 = weights_3[int(6)] * clamp_0(1.0f - (raw_losses_0[int(6)] - raw_losses_0[int(2)] * raw_losses_0[int(3)] / raw_losses_0[int(22)]) / (F32_sqrt(((F32_max((9.999999960041972e-13f), ((raw_losses_0[int(4)] - raw_losses_0[int(2)] * raw_losses_0[int(2)] / raw_losses_0[int(22)]) * (raw_losses_0[int(5)] - raw_losses_0[int(3)] * raw_losses_0[int(3)] / raw_losses_0[int(22)]) + 1.0f)))))), 0.0f, 2.0f);
    }
    else
    {
        _S426 = 0.0f;
    }
    losses_1[int(2)] = _S426;
    losses_1[int(3)] = (raw_losses_0[int(7)] / (F32_max((raw_losses_0[int(23)]), (1.0f))) + raw_losses_0[int(8)] / (F32_max((raw_losses_0[int(24)]), (1.0f)))) / float((I32_max((int((raw_losses_0[int(23)]) > 0.5f) + int((raw_losses_0[int(24)]) > 0.5f)), (int(1)))));
    losses_1[int(4)] = raw_losses_0[int(9)] / (F32_max((raw_losses_0[int(26)]), (1.0f))) + raw_losses_0[int(10)] / (F32_max((raw_losses_0[int(27)]), (1.0f)));
    losses_1[int(5)] = raw_losses_0[int(11)] / (F32_max((raw_losses_0[int(25)]), (1.0f)));
    losses_1[int(6)] = raw_losses_0[int(12)] / _S424;
    losses_1[int(7)] = raw_losses_0[int(13)] / _S424;
    losses_1[int(8)] = raw_losses_0[int(14)] / _S424;
    losses_1[int(9)] = raw_losses_0[int(15)] / _S424;
    losses_1[int(10)] = raw_losses_0[int(16)] / (F32_max((raw_losses_0[int(28)]), (1.0f)));
    losses_1[int(11)] = raw_losses_0[int(17)] / (F32_max((raw_losses_0[int(29)]), (1.0f)));
    losses_1[int(12)] = raw_losses_0[int(18)] / (F32_max((raw_losses_0[int(30)]), (1.0f)));
    losses_1[int(13)] = raw_losses_0[int(19)] / (F32_max((raw_losses_0[int(31)]), (1.0f)));
    *_S423 = losses_1;
    return;
}

struct DiffPair_arrayx3Cfloatx2C32x3E_0
{
    FixedArray<float, 32>  primal_0;
    FixedArray<float, 32>  differential_0;
};

inline __device__ float s_primal_ctx_sqrt_0(float _S427)
{
    return (F32_sqrt((_S427)));
}

inline __device__ void s_bwd_prop_log10_0(DiffPair_float_0 * _S428, float _S429)
{
    _d_log10_0(_S428, _S429);
    return;
}

inline __device__ void s_bwd_prop_per_pixel_losses_reduce_0(DiffPair_arrayx3Cfloatx2C32x3E_0 * dpraw_losses_0, FixedArray<float, 19>  * weights_4, FixedArray<float, 14>  * _s_dOut_1)
{
    FixedArray<float, 32>  _S430 = dpraw_losses_0->primal_0;
    float _S431 = (F32_max((dpraw_losses_0->primal_0[int(21)]), (1.0f)));
    float _S432 = _S431 * _S431;
    float _S433 = dpraw_losses_0->primal_0[int(1)] / _S431;
    bool _S434 = (dpraw_losses_0->primal_0[int(22)]) > 0.0f;
    bool _S435;
    if(_S434)
    {
        _S435 = (_S430[int(3)]) != 0.0f;
    }
    else
    {
        _S435 = false;
    }
    float _S436;
    float _S437;
    float _S438;
    float _S439;
    float _S440;
    float _S441;
    float _S442;
    float _S443;
    float _S444;
    float _S445;
    float _S446;
    float _S447;
    float _S448;
    float _S449;
    float _S450;
    if(_S435)
    {
        float _S451 = _S430[int(2)] * _S430[int(3)];
        float _S452 = _S430[int(22)] * _S430[int(22)];
        float _S453 = _S430[int(6)] - _S451 / _S430[int(22)];
        float _S454 = _S430[int(2)] * _S430[int(2)];
        float _S455 = _S430[int(4)] - _S454 / _S430[int(22)];
        float _S456 = _S430[int(3)] * _S430[int(3)];
        float _S457 = _S430[int(5)] - _S456 / _S430[int(22)];
        float _S458 = _S455 * _S457 + 1.0f;
        float _S459 = (F32_max((9.999999960041972e-13f), (_S458)));
        float _S460 = s_primal_ctx_sqrt_0(_S459);
        float _S461 = _S460 * _S460;
        float _S462 = 1.0f - _S453 / _S460;
        _S436 = (*weights_4)[int(6)];
        _S437 = _S462;
        _S438 = _S461;
        _S439 = _S453;
        _S440 = _S460;
        _S441 = _S459;
        _S442 = _S458;
        _S443 = _S455;
        _S444 = _S457;
        _S445 = _S452;
        _S446 = _S456;
        _S447 = _S430[int(3)];
        _S448 = _S454;
        _S449 = _S430[int(2)];
        _S450 = _S451;
    }
    else
    {
        _S436 = 0.0f;
        _S437 = 0.0f;
        _S438 = 0.0f;
        _S439 = 0.0f;
        _S440 = 0.0f;
        _S441 = 0.0f;
        _S442 = 0.0f;
        _S443 = 0.0f;
        _S444 = 0.0f;
        _S445 = 0.0f;
        _S446 = 0.0f;
        _S447 = 0.0f;
        _S448 = 0.0f;
        _S449 = 0.0f;
        _S450 = 0.0f;
    }
    float _S463 = (F32_max((_S430[int(23)]), (1.0f)));
    float _S464 = _S463 * _S463;
    float _S465 = (F32_max((_S430[int(24)]), (1.0f)));
    float _S466 = _S465 * _S465;
    float _S467 = float((I32_max((int((_S430[int(23)]) > 0.5f) + int((_S430[int(24)]) > 0.5f)), (int(1)))));
    float _S468 = (F32_max((_S430[int(26)]), (1.0f)));
    float _S469 = _S468 * _S468;
    float _S470 = (F32_max((_S430[int(27)]), (1.0f)));
    float _S471 = _S470 * _S470;
    float _S472 = (F32_max((_S430[int(25)]), (1.0f)));
    float _S473 = _S472 * _S472;
    float _S474 = (F32_max((_S430[int(28)]), (1.0f)));
    float _S475 = _S474 * _S474;
    float _S476 = (F32_max((_S430[int(29)]), (1.0f)));
    float _S477 = _S476 * _S476;
    float _S478 = (F32_max((_S430[int(30)]), (1.0f)));
    float _S479 = _S478 * _S478;
    float _S480 = (F32_max((_S430[int(31)]), (1.0f)));
    float _S481 = _S480 * _S480;
    float _S482 = (*_s_dOut_1)[int(0)];
    float _S483 = (*_s_dOut_1)[int(1)];
    float _S484 = (*_s_dOut_1)[int(2)];
    float _S485 = (*_s_dOut_1)[int(13)] / _S481;
    float _S486 = _S430[int(19)] * - _S485;
    float _S487 = _S480 * _S485;
    DiffPair_float_0 _S488;
    (&_S488)->primal_0 = _S430[int(31)];
    (&_S488)->differential_0 = 0.0f;
    DiffPair_float_0 _S489;
    (&_S489)->primal_0 = 1.0f;
    (&_S489)->differential_0 = 0.0f;
    _d_max_0(&_S488, &_S489, _S486);
    float _S490 = (*_s_dOut_1)[int(12)] / _S479;
    float _S491 = _S430[int(18)] * - _S490;
    float _S492 = _S478 * _S490;
    DiffPair_float_0 _S493;
    (&_S493)->primal_0 = _S430[int(30)];
    (&_S493)->differential_0 = 0.0f;
    DiffPair_float_0 _S494;
    (&_S494)->primal_0 = 1.0f;
    (&_S494)->differential_0 = 0.0f;
    _d_max_0(&_S493, &_S494, _S491);
    float _S495 = (*_s_dOut_1)[int(11)] / _S477;
    float _S496 = _S430[int(17)] * - _S495;
    float _S497 = _S476 * _S495;
    DiffPair_float_0 _S498;
    (&_S498)->primal_0 = _S430[int(29)];
    (&_S498)->differential_0 = 0.0f;
    DiffPair_float_0 _S499;
    (&_S499)->primal_0 = 1.0f;
    (&_S499)->differential_0 = 0.0f;
    _d_max_0(&_S498, &_S499, _S496);
    float _S500 = (*_s_dOut_1)[int(10)] / _S475;
    float _S501 = _S430[int(16)] * - _S500;
    float _S502 = _S474 * _S500;
    DiffPair_float_0 _S503;
    (&_S503)->primal_0 = _S430[int(28)];
    (&_S503)->differential_0 = 0.0f;
    DiffPair_float_0 _S504;
    (&_S504)->primal_0 = 1.0f;
    (&_S504)->differential_0 = 0.0f;
    _d_max_0(&_S503, &_S504, _S501);
    float _S505 = (*_s_dOut_1)[int(9)] / _S432;
    float _S506 = _S430[int(15)] * - _S505;
    float _S507 = _S431 * _S505;
    float _S508 = (*_s_dOut_1)[int(8)] / _S432;
    float _S509 = _S430[int(14)] * - _S508;
    float _S510 = _S431 * _S508;
    float _S511 = (*_s_dOut_1)[int(7)] / _S432;
    float _S512 = _S430[int(13)] * - _S511;
    float _S513 = _S431 * _S511;
    float _S514 = (*_s_dOut_1)[int(6)] / _S432;
    float _S515 = _S430[int(12)] * - _S514;
    float _S516 = _S431 * _S514;
    float _S517 = (*_s_dOut_1)[int(5)] / _S473;
    float _S518 = _S430[int(11)] * - _S517;
    float _S519 = _S472 * _S517;
    DiffPair_float_0 _S520;
    (&_S520)->primal_0 = _S430[int(25)];
    (&_S520)->differential_0 = 0.0f;
    DiffPair_float_0 _S521;
    (&_S521)->primal_0 = 1.0f;
    (&_S521)->differential_0 = 0.0f;
    _d_max_0(&_S520, &_S521, _S518);
    float _S522 = (*_s_dOut_1)[int(4)] / _S471;
    float _S523 = _S430[int(10)] * - _S522;
    float _S524 = _S470 * _S522;
    DiffPair_float_0 _S525;
    (&_S525)->primal_0 = _S430[int(27)];
    (&_S525)->differential_0 = 0.0f;
    DiffPair_float_0 _S526;
    (&_S526)->primal_0 = 1.0f;
    (&_S526)->differential_0 = 0.0f;
    _d_max_0(&_S525, &_S526, _S523);
    float _S527 = (*_s_dOut_1)[int(4)] / _S469;
    float _S528 = _S430[int(9)] * - _S527;
    float _S529 = _S468 * _S527;
    DiffPair_float_0 _S530;
    (&_S530)->primal_0 = _S430[int(26)];
    (&_S530)->differential_0 = 0.0f;
    DiffPair_float_0 _S531;
    (&_S531)->primal_0 = 1.0f;
    (&_S531)->differential_0 = 0.0f;
    _d_max_0(&_S530, &_S531, _S528);
    float _S532 = (*_s_dOut_1)[int(3)] / _S467;
    float _S533 = _S532 / _S466;
    float _S534 = _S430[int(8)] * - _S533;
    float _S535 = _S465 * _S533;
    DiffPair_float_0 _S536;
    (&_S536)->primal_0 = _S430[int(24)];
    (&_S536)->differential_0 = 0.0f;
    DiffPair_float_0 _S537;
    (&_S537)->primal_0 = 1.0f;
    (&_S537)->differential_0 = 0.0f;
    _d_max_0(&_S536, &_S537, _S534);
    float _S538 = _S532 / _S464;
    float _S539 = _S430[int(7)] * - _S538;
    float _S540 = _S463 * _S538;
    DiffPair_float_0 _S541;
    (&_S541)->primal_0 = _S430[int(23)];
    (&_S541)->differential_0 = 0.0f;
    DiffPair_float_0 _S542;
    (&_S542)->primal_0 = 1.0f;
    (&_S542)->differential_0 = 0.0f;
    _d_max_0(&_S541, &_S542, _S539);
    float _S543 = _S506 + _S509 + _S512 + _S515;
    FixedArray<float, 32>  _S544;
    _S544[int(0)] = 0.0f;
    _S544[int(1)] = 0.0f;
    _S544[int(2)] = 0.0f;
    _S544[int(3)] = 0.0f;
    _S544[int(4)] = 0.0f;
    _S544[int(5)] = 0.0f;
    _S544[int(6)] = 0.0f;
    _S544[int(7)] = 0.0f;
    _S544[int(8)] = 0.0f;
    _S544[int(9)] = 0.0f;
    _S544[int(10)] = 0.0f;
    _S544[int(11)] = 0.0f;
    _S544[int(12)] = 0.0f;
    _S544[int(13)] = 0.0f;
    _S544[int(14)] = 0.0f;
    _S544[int(15)] = 0.0f;
    _S544[int(16)] = 0.0f;
    _S544[int(17)] = 0.0f;
    _S544[int(18)] = 0.0f;
    _S544[int(19)] = 0.0f;
    _S544[int(20)] = 0.0f;
    _S544[int(21)] = 0.0f;
    _S544[int(22)] = 0.0f;
    _S544[int(23)] = 0.0f;
    _S544[int(24)] = 0.0f;
    _S544[int(25)] = 0.0f;
    _S544[int(26)] = 0.0f;
    _S544[int(27)] = 0.0f;
    _S544[int(28)] = 0.0f;
    _S544[int(29)] = 0.0f;
    _S544[int(30)] = 0.0f;
    _S544[int(31)] = 0.0f;
    _S544[int(12)] = _S516;
    _S544[int(7)] = _S540;
    _S544[int(23)] = _S541.differential_0;
    _S544[int(8)] = _S535;
    _S544[int(24)] = _S536.differential_0;
    _S544[int(9)] = _S529;
    _S544[int(26)] = _S530.differential_0;
    _S544[int(10)] = _S524;
    _S544[int(27)] = _S525.differential_0;
    _S544[int(11)] = _S519;
    _S544[int(25)] = _S520.differential_0;
    _S544[int(31)] = _S488.differential_0;
    _S544[int(13)] = _S513;
    _S544[int(14)] = _S510;
    _S544[int(15)] = _S507;
    _S544[int(16)] = _S502;
    _S544[int(28)] = _S503.differential_0;
    _S544[int(17)] = _S497;
    _S544[int(29)] = _S498.differential_0;
    _S544[int(18)] = _S492;
    _S544[int(30)] = _S493.differential_0;
    _S544[int(19)] = _S487;
    float _S545 = _S544[int(0)];
    float _S546 = _S544[int(1)];
    float _S547 = _S544[int(2)];
    float _S548 = _S544[int(3)];
    float _S549 = _S544[int(4)];
    float _S550 = _S544[int(5)];
    float _S551 = _S544[int(6)];
    float _S552 = _S544[int(7)];
    float _S553 = _S544[int(8)];
    float _S554 = _S544[int(9)];
    float _S555 = _S544[int(10)];
    float _S556 = _S544[int(11)];
    float _S557 = _S544[int(12)];
    float _S558 = _S544[int(13)];
    float _S559 = _S544[int(14)];
    float _S560 = _S544[int(15)];
    float _S561 = _S544[int(16)];
    float _S562 = _S544[int(17)];
    float _S563 = _S544[int(18)];
    float _S564 = _S544[int(19)];
    float _S565 = _S544[int(20)];
    float _S566 = _S544[int(21)];
    float _S567 = _S544[int(22)];
    float _S568 = _S544[int(23)];
    float _S569 = _S544[int(24)];
    float _S570 = _S544[int(25)];
    float _S571 = _S544[int(26)];
    float _S572 = _S544[int(27)];
    float _S573 = _S544[int(28)];
    float _S574 = _S544[int(29)];
    float _S575 = _S544[int(30)];
    float _S576 = _S544[int(31)];
    FixedArray<float, 32>  _S577;
    if(_S435)
    {
        float _S578 = _S436 * _S484;
        DiffPair_float_0 _S579;
        (&_S579)->primal_0 = _S437;
        (&_S579)->differential_0 = 0.0f;
        DiffPair_float_0 _S580;
        (&_S580)->primal_0 = 0.0f;
        (&_S580)->differential_0 = 0.0f;
        DiffPair_float_0 _S581;
        (&_S581)->primal_0 = 2.0f;
        (&_S581)->differential_0 = 0.0f;
        s_bwd_prop_clamp_0(&_S579, &_S580, &_S581, _S578);
        float _S582 = - _S579.differential_0 / _S438;
        float _S583 = _S439 * - _S582;
        float _S584 = _S440 * _S582;
        DiffPair_float_0 _S585;
        (&_S585)->primal_0 = _S441;
        (&_S585)->differential_0 = 0.0f;
        s_bwd_prop_sqrt_0(&_S585, _S583);
        DiffPair_float_0 _S586;
        (&_S586)->primal_0 = 9.999999960041972e-13f;
        (&_S586)->differential_0 = 0.0f;
        DiffPair_float_0 _S587;
        (&_S587)->primal_0 = _S442;
        (&_S587)->differential_0 = 0.0f;
        _d_max_0(&_S586, &_S587, _S585.differential_0);
        float _S588 = _S443 * _S587.differential_0;
        float _S589 = _S444 * _S587.differential_0;
        float _S590 = - _S588 / _S445;
        float _S591 = _S447 * (_S430[int(22)] * _S590);
        float _S592 = - _S589 / _S445;
        float _S593 = _S449 * (_S430[int(22)] * _S592);
        float _S594 = - _S584 / _S445;
        float _S595 = _S430[int(22)] * _S594;
        float _S596 = _S591 + _S591 + _S449 * _S595;
        float _S597 = _S593 + _S593 + _S447 * _S595;
        float _S598 = _S446 * - _S590 + _S448 * - _S592 + _S450 * - _S594;
        FixedArray<float, 32>  _S599;
        _S599[int(0)] = 0.0f;
        _S599[int(1)] = 0.0f;
        _S599[int(2)] = 0.0f;
        _S599[int(3)] = 0.0f;
        _S599[int(4)] = 0.0f;
        _S599[int(5)] = 0.0f;
        _S599[int(6)] = 0.0f;
        _S599[int(7)] = 0.0f;
        _S599[int(8)] = 0.0f;
        _S599[int(9)] = 0.0f;
        _S599[int(10)] = 0.0f;
        _S599[int(11)] = 0.0f;
        _S599[int(12)] = 0.0f;
        _S599[int(13)] = 0.0f;
        _S599[int(14)] = 0.0f;
        _S599[int(15)] = 0.0f;
        _S599[int(16)] = 0.0f;
        _S599[int(17)] = 0.0f;
        _S599[int(18)] = 0.0f;
        _S599[int(19)] = 0.0f;
        _S599[int(20)] = 0.0f;
        _S599[int(21)] = 0.0f;
        _S599[int(22)] = 0.0f;
        _S599[int(23)] = 0.0f;
        _S599[int(24)] = 0.0f;
        _S599[int(25)] = 0.0f;
        _S599[int(26)] = 0.0f;
        _S599[int(27)] = 0.0f;
        _S599[int(28)] = 0.0f;
        _S599[int(29)] = 0.0f;
        _S599[int(30)] = 0.0f;
        _S599[int(31)] = 0.0f;
        _S599[int(5)] = _S588;
        _S599[int(4)] = _S589;
        _S599[int(3)] = _S596;
        _S599[int(2)] = _S597;
        _S599[int(6)] = _S584;
        float _S600 = _S546 + _S599[int(1)];
        float _S601 = _S547 + _S599[int(2)];
        float _S602 = _S548 + _S599[int(3)];
        float _S603 = _S549 + _S599[int(4)];
        float _S604 = _S550 + _S599[int(5)];
        float _S605 = _S551 + _S599[int(6)];
        float _S606 = _S552 + _S599[int(7)];
        float _S607 = _S553 + _S599[int(8)];
        float _S608 = _S554 + _S599[int(9)];
        float _S609 = _S555 + _S599[int(10)];
        float _S610 = _S556 + _S599[int(11)];
        float _S611 = _S557 + _S599[int(12)];
        float _S612 = _S558 + _S599[int(13)];
        float _S613 = _S559 + _S599[int(14)];
        float _S614 = _S560 + _S599[int(15)];
        float _S615 = _S561 + _S599[int(16)];
        float _S616 = _S562 + _S599[int(17)];
        float _S617 = _S563 + _S599[int(18)];
        float _S618 = _S564 + _S599[int(19)];
        float _S619 = _S565 + _S599[int(20)];
        float _S620 = _S566 + _S599[int(21)];
        float _S621 = _S567 + _S599[int(22)];
        float _S622 = _S568 + _S599[int(23)];
        float _S623 = _S569 + _S599[int(24)];
        float _S624 = _S570 + _S599[int(25)];
        float _S625 = _S571 + _S599[int(26)];
        float _S626 = _S572 + _S599[int(27)];
        float _S627 = _S573 + _S599[int(28)];
        float _S628 = _S574 + _S599[int(29)];
        float _S629 = _S575 + _S599[int(30)];
        float _S630 = _S576 + _S599[int(31)];
        _S577[int(0)] = _S545 + _S599[int(0)];
        _S577[int(1)] = _S600;
        _S577[int(2)] = _S601;
        _S577[int(3)] = _S602;
        _S577[int(4)] = _S603;
        _S577[int(5)] = _S604;
        _S577[int(6)] = _S605;
        _S577[int(7)] = _S606;
        _S577[int(8)] = _S607;
        _S577[int(9)] = _S608;
        _S577[int(10)] = _S609;
        _S577[int(11)] = _S610;
        _S577[int(12)] = _S611;
        _S577[int(13)] = _S612;
        _S577[int(14)] = _S613;
        _S577[int(15)] = _S614;
        _S577[int(16)] = _S615;
        _S577[int(17)] = _S616;
        _S577[int(18)] = _S617;
        _S577[int(19)] = _S618;
        _S577[int(20)] = _S619;
        _S577[int(21)] = _S620;
        _S577[int(22)] = _S621;
        _S577[int(23)] = _S622;
        _S577[int(24)] = _S623;
        _S577[int(25)] = _S624;
        _S577[int(26)] = _S625;
        _S577[int(27)] = _S626;
        _S577[int(28)] = _S627;
        _S577[int(29)] = _S628;
        _S577[int(30)] = _S629;
        _S577[int(31)] = _S630;
        _S436 = _S598;
    }
    else
    {
        _S577[int(0)] = _S545;
        _S577[int(1)] = _S546;
        _S577[int(2)] = _S547;
        _S577[int(3)] = _S548;
        _S577[int(4)] = _S549;
        _S577[int(5)] = _S550;
        _S577[int(6)] = _S551;
        _S577[int(7)] = _S552;
        _S577[int(8)] = _S553;
        _S577[int(9)] = _S554;
        _S577[int(10)] = _S555;
        _S577[int(11)] = _S556;
        _S577[int(12)] = _S557;
        _S577[int(13)] = _S558;
        _S577[int(14)] = _S559;
        _S577[int(15)] = _S560;
        _S577[int(16)] = _S561;
        _S577[int(17)] = _S562;
        _S577[int(18)] = _S563;
        _S577[int(19)] = _S564;
        _S577[int(20)] = _S565;
        _S577[int(21)] = _S566;
        _S577[int(22)] = _S567;
        _S577[int(23)] = _S568;
        _S577[int(24)] = _S569;
        _S577[int(25)] = _S570;
        _S577[int(26)] = _S571;
        _S577[int(27)] = _S572;
        _S577[int(28)] = _S573;
        _S577[int(29)] = _S574;
        _S577[int(30)] = _S575;
        _S577[int(31)] = _S576;
        _S436 = 0.0f;
    }
    if(_S434)
    {
        FixedArray<float, 32>  _S631;
        _S631[int(0)] = 0.0f;
        _S631[int(1)] = 0.0f;
        _S631[int(2)] = 0.0f;
        _S631[int(3)] = 0.0f;
        _S631[int(4)] = 0.0f;
        _S631[int(5)] = 0.0f;
        _S631[int(6)] = 0.0f;
        _S631[int(7)] = 0.0f;
        _S631[int(8)] = 0.0f;
        _S631[int(9)] = 0.0f;
        _S631[int(10)] = 0.0f;
        _S631[int(11)] = 0.0f;
        _S631[int(12)] = 0.0f;
        _S631[int(13)] = 0.0f;
        _S631[int(14)] = 0.0f;
        _S631[int(15)] = 0.0f;
        _S631[int(16)] = 0.0f;
        _S631[int(17)] = 0.0f;
        _S631[int(18)] = 0.0f;
        _S631[int(19)] = 0.0f;
        _S631[int(20)] = 0.0f;
        _S631[int(21)] = 0.0f;
        _S631[int(22)] = 0.0f;
        _S631[int(23)] = 0.0f;
        _S631[int(24)] = 0.0f;
        _S631[int(25)] = 0.0f;
        _S631[int(26)] = 0.0f;
        _S631[int(27)] = 0.0f;
        _S631[int(28)] = 0.0f;
        _S631[int(29)] = 0.0f;
        _S631[int(30)] = 0.0f;
        _S631[int(31)] = 0.0f;
        _S631[int(3)] = 0.0f;
        float _S632 = _S577[int(1)] + _S631[int(1)];
        float _S633 = _S577[int(2)] + _S631[int(2)];
        float _S634 = _S577[int(3)] + _S631[int(3)];
        float _S635 = _S577[int(4)] + _S631[int(4)];
        float _S636 = _S577[int(5)] + _S631[int(5)];
        float _S637 = _S577[int(6)] + _S631[int(6)];
        float _S638 = _S577[int(7)] + _S631[int(7)];
        float _S639 = _S577[int(8)] + _S631[int(8)];
        float _S640 = _S577[int(9)] + _S631[int(9)];
        float _S641 = _S577[int(10)] + _S631[int(10)];
        float _S642 = _S577[int(11)] + _S631[int(11)];
        float _S643 = _S577[int(12)] + _S631[int(12)];
        float _S644 = _S577[int(13)] + _S631[int(13)];
        float _S645 = _S577[int(14)] + _S631[int(14)];
        float _S646 = _S577[int(15)] + _S631[int(15)];
        float _S647 = _S577[int(16)] + _S631[int(16)];
        float _S648 = _S577[int(17)] + _S631[int(17)];
        float _S649 = _S577[int(18)] + _S631[int(18)];
        float _S650 = _S577[int(19)] + _S631[int(19)];
        float _S651 = _S577[int(20)] + _S631[int(20)];
        float _S652 = _S577[int(21)] + _S631[int(21)];
        float _S653 = _S577[int(22)] + _S631[int(22)];
        float _S654 = _S577[int(23)] + _S631[int(23)];
        float _S655 = _S577[int(24)] + _S631[int(24)];
        float _S656 = _S577[int(25)] + _S631[int(25)];
        float _S657 = _S577[int(26)] + _S631[int(26)];
        float _S658 = _S577[int(27)] + _S631[int(27)];
        float _S659 = _S577[int(28)] + _S631[int(28)];
        float _S660 = _S577[int(29)] + _S631[int(29)];
        float _S661 = _S577[int(30)] + _S631[int(30)];
        float _S662 = _S577[int(31)] + _S631[int(31)];
        _S577[int(0)] = _S577[int(0)] + _S631[int(0)];
        _S577[int(1)] = _S632;
        _S577[int(2)] = _S633;
        _S577[int(3)] = _S634;
        _S577[int(4)] = _S635;
        _S577[int(5)] = _S636;
        _S577[int(6)] = _S637;
        _S577[int(7)] = _S638;
        _S577[int(8)] = _S639;
        _S577[int(9)] = _S640;
        _S577[int(10)] = _S641;
        _S577[int(11)] = _S642;
        _S577[int(12)] = _S643;
        _S577[int(13)] = _S644;
        _S577[int(14)] = _S645;
        _S577[int(15)] = _S646;
        _S577[int(16)] = _S647;
        _S577[int(17)] = _S648;
        _S577[int(18)] = _S649;
        _S577[int(19)] = _S650;
        _S577[int(20)] = _S651;
        _S577[int(21)] = _S652;
        _S577[int(22)] = _S653;
        _S577[int(23)] = _S654;
        _S577[int(24)] = _S655;
        _S577[int(25)] = _S656;
        _S577[int(26)] = _S657;
        _S577[int(27)] = _S658;
        _S577[int(28)] = _S659;
        _S577[int(29)] = _S660;
        _S577[int(30)] = _S661;
        _S577[int(31)] = _S662;
    }
    float _S663 = -10.0f * _S483;
    DiffPair_float_0 _S664;
    (&_S664)->primal_0 = _S433;
    (&_S664)->differential_0 = 0.0f;
    s_bwd_prop_log10_0(&_S664, _S663);
    float _S665 = _S664.differential_0 / _S432;
    float _S666 = _S431 * _S665;
    float _S667 = _S482 / _S432;
    float _S668 = _S431 * _S667;
    float _S669 = _S430[int(1)] * - _S665 + _S430[int(0)] * - _S667 + _S543;
    DiffPair_float_0 _S670;
    (&_S670)->primal_0 = _S430[int(21)];
    (&_S670)->differential_0 = 0.0f;
    DiffPair_float_0 _S671;
    (&_S671)->primal_0 = 1.0f;
    (&_S671)->differential_0 = 0.0f;
    _d_max_0(&_S670, &_S671, _S669);
    FixedArray<float, 32>  _S672;
    _S672[int(0)] = 0.0f;
    _S672[int(1)] = 0.0f;
    _S672[int(2)] = 0.0f;
    _S672[int(3)] = 0.0f;
    _S672[int(4)] = 0.0f;
    _S672[int(5)] = 0.0f;
    _S672[int(6)] = 0.0f;
    _S672[int(7)] = 0.0f;
    _S672[int(8)] = 0.0f;
    _S672[int(9)] = 0.0f;
    _S672[int(10)] = 0.0f;
    _S672[int(11)] = 0.0f;
    _S672[int(12)] = 0.0f;
    _S672[int(13)] = 0.0f;
    _S672[int(14)] = 0.0f;
    _S672[int(15)] = 0.0f;
    _S672[int(16)] = 0.0f;
    _S672[int(17)] = 0.0f;
    _S672[int(18)] = 0.0f;
    _S672[int(19)] = 0.0f;
    _S672[int(20)] = 0.0f;
    _S672[int(21)] = 0.0f;
    _S672[int(22)] = 0.0f;
    _S672[int(23)] = 0.0f;
    _S672[int(24)] = 0.0f;
    _S672[int(25)] = 0.0f;
    _S672[int(26)] = 0.0f;
    _S672[int(27)] = 0.0f;
    _S672[int(28)] = 0.0f;
    _S672[int(29)] = 0.0f;
    _S672[int(30)] = 0.0f;
    _S672[int(31)] = 0.0f;
    _S672[int(22)] = _S436;
    _S672[int(1)] = _S666;
    _S672[int(21)] = _S670.differential_0;
    _S672[int(0)] = _S668;
    FixedArray<float, 32>  _S673 = {
        _S577[int(0)] + _S672[int(0)], _S577[int(1)] + _S672[int(1)], _S577[int(2)] + _S672[int(2)], _S577[int(3)] + _S672[int(3)], _S577[int(4)] + _S672[int(4)], _S577[int(5)] + _S672[int(5)], _S577[int(6)] + _S672[int(6)], _S577[int(7)] + _S672[int(7)], _S577[int(8)] + _S672[int(8)], _S577[int(9)] + _S672[int(9)], _S577[int(10)] + _S672[int(10)], _S577[int(11)] + _S672[int(11)], _S577[int(12)] + _S672[int(12)], _S577[int(13)] + _S672[int(13)], _S577[int(14)] + _S672[int(14)], _S577[int(15)] + _S672[int(15)], _S577[int(16)] + _S672[int(16)], _S577[int(17)] + _S672[int(17)], _S577[int(18)] + _S672[int(18)], _S577[int(19)] + _S672[int(19)], _S577[int(20)] + _S672[int(20)], _S577[int(21)] + _S672[int(21)], _S577[int(22)] + _S672[int(22)], _S577[int(23)] + _S672[int(23)], _S577[int(24)] + _S672[int(24)], _S577[int(25)] + _S672[int(25)], _S577[int(26)] + _S672[int(26)], _S577[int(27)] + _S672[int(27)], _S577[int(28)] + _S672[int(28)], _S577[int(29)] + _S672[int(29)], _S577[int(30)] + _S672[int(30)], _S577[int(31)] + _S672[int(31)]
    };
    dpraw_losses_0->primal_0 = dpraw_losses_0->primal_0;
    dpraw_losses_0->differential_0 = _S673;
    return;
}

inline __device__ void s_bwd_per_pixel_losses_reduce_0(DiffPair_arrayx3Cfloatx2C32x3E_0 * _S674, FixedArray<float, 19>  * _S675, FixedArray<float, 14>  * _S676)
{
    s_bwd_prop_per_pixel_losses_reduce_0(_S674, _S675, _S676);
    return;
}

inline __device__ void per_pixel_losses_reduce_bwd(FixedArray<float, 32>  raw_losses_1, FixedArray<float, 19>  weights_5, FixedArray<float, 14>  v_losses_1, FixedArray<float, 32>  * _S677)
{
    FixedArray<float, 32>  _S678 = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    DiffPair_arrayx3Cfloatx2C32x3E_0 dp_raw_losses_0;
    (&dp_raw_losses_0)->primal_0 = raw_losses_1;
    (&dp_raw_losses_0)->differential_0 = _S678;
    FixedArray<float, 19>  _S679 = weights_5;
    FixedArray<float, 14>  _S680 = v_losses_1;
    s_bwd_per_pixel_losses_reduce_0(&dp_raw_losses_0, &_S679, &_S680);
    *_S677 = (&dp_raw_losses_0)->differential_0;
    return;
}

