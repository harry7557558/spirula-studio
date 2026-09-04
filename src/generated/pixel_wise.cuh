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

inline __device__ float rendered_depth_to_expected_depth(float depth_0, float transmittance_0)
{
    return depth_0 / (F32_max((1.0f - transmittance_0), (1.00000001335143196e-10f)));
}

inline __device__ void s_bwd_prop_rendered_depth_to_expected_depth_0(DiffPair_float_0 * dpdepth_0, DiffPair_float_0 * dptransmittance_0, float _s_dOut_0)
{
    float _S4 = 1.0f - (*dptransmittance_0).primal_0;
    float _S5 = (F32_max((_S4), (1.00000001335143196e-10f)));
    float _S6 = _s_dOut_0 / (_S5 * _S5);
    float _S7 = (*dpdepth_0).primal_0 * - _S6;
    float _S8 = _S5 * _S6;
    DiffPair_float_0 _S9;
    (&_S9)->primal_0 = _S4;
    (&_S9)->differential_0 = 0.0f;
    DiffPair_float_0 _S10;
    (&_S10)->primal_0 = 1.00000001335143196e-10f;
    (&_S10)->differential_0 = 0.0f;
    _d_max_0(&_S9, &_S10, _S7);
    float _S11 = - _S9.differential_0;
    dptransmittance_0->primal_0 = (*dptransmittance_0).primal_0;
    dptransmittance_0->differential_0 = _S11;
    dpdepth_0->primal_0 = (*dpdepth_0).primal_0;
    dpdepth_0->differential_0 = _S8;
    return;
}

inline __device__ void s_bwd_rendered_depth_to_expected_depth_0(DiffPair_float_0 * _S12, DiffPair_float_0 * _S13, float _S14)
{
    s_bwd_prop_rendered_depth_to_expected_depth_0(_S12, _S13, _S14);
    return;
}

inline __device__ void rendered_depth_to_expected_depth_bwd(float depth_1, float transmittance_1, float v_out_depth_0, float * v_depth_0, float * v_transmittance_0)
{
    DiffPair_float_0 p_depth_0;
    (&p_depth_0)->primal_0 = depth_1;
    (&p_depth_0)->differential_0 = 0.0f;
    DiffPair_float_0 p_transmittance_0;
    (&p_transmittance_0)->primal_0 = transmittance_1;
    (&p_transmittance_0)->differential_0 = 0.0f;
    s_bwd_rendered_depth_to_expected_depth_0(&p_depth_0, &p_transmittance_0, v_out_depth_0);
    *v_depth_0 = p_depth_0.differential_0;
    *v_transmittance_0 = p_transmittance_0.differential_0;
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

inline __device__ float dot_1(float2  x_1, float2  y_1)
{
    int i_1 = int(0);
    float result_2 = 0.0f;
    for(;;)
    {
        if(i_1 < int(2))
        {
        }
        else
        {
            break;
        }
        float result_3 = result_2 + _slang_vector_get_element(x_1, i_1) * _slang_vector_get_element(y_1, i_1);
        i_1 = i_1 + int(1);
        result_2 = result_3;
    }
    return result_2;
}

inline __device__ void blend_background_bwd_impl_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dp_rgb_0, DiffPair_float_0 * dp_transmittance_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dp_background_0, float3  v_out_0)
{
    DiffPair_float_0 _S15 = *dp_transmittance_0;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S16 = *dp_background_0;
    dp_rgb_0->primal_0 = (*dp_rgb_0).primal_0;
    dp_rgb_0->differential_0 = v_out_0;
    float _S17 = dot_0(_S16.primal_0, v_out_0);
    dp_transmittance_0->primal_0 = _S15.primal_0;
    dp_transmittance_0->differential_0 = _S17;
    float3  _S18 = make_float3 (_S15.primal_0) * v_out_0;
    dp_background_0->primal_0 = _S16.primal_0;
    dp_background_0->differential_0 = _S18;
    return;
}

inline __device__ float3  blend_background(float3  rgb_0, float transmittance_2, float3  background_0)
{
    return rgb_0 + make_float3 (transmittance_2) * background_0;
}

inline __device__ float3  min_0(float3  x_2, float3  y_2)
{
    float3  result_4;
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
        *_slang_vector_get_element_ptr(&result_4, i_2) = (F32_min((_slang_vector_get_element(x_2, i_2)), (_slang_vector_get_element(y_2, i_2))));
        i_2 = i_2 + int(1);
    }
    return result_4;
}

inline __device__ float3  max_0(float3  x_3, float3  y_3)
{
    float3  result_5;
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
        *_slang_vector_get_element_ptr(&result_5, i_3) = (F32_max((_slang_vector_get_element(x_3, i_3)), (_slang_vector_get_element(y_3, i_3))));
        i_3 = i_3 + int(1);
    }
    return result_5;
}

inline __device__ float3  overexposure_grad(float3  c_0, float scale_0)
{
    float3  _S19 = make_float3 (0.0f);
    return make_float3 (scale_0) * (min_0(c_0, _S19) + max_0(c_0 - make_float3 (1.0f), _S19));
}

inline __device__ void blend_background_bwd(float3  rgb_1, float transmittance_3, float3  background_1, float3  v_out_rgb_0, float overexposure_scale_0, float3  * v_rgb_0, float * v_transmittance_1, float3  * v_background_0)
{
    float3  _S20;
    if(overexposure_scale_0 != 0.0f)
    {
        _S20 = v_out_rgb_0 + overexposure_grad(rgb_1 + make_float3 (transmittance_3) * background_1, overexposure_scale_0);
    }
    else
    {
        _S20 = v_out_rgb_0;
    }
    float3  _S21 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 p_rgb_0;
    (&p_rgb_0)->primal_0 = rgb_1;
    (&p_rgb_0)->differential_0 = _S21;
    DiffPair_float_0 p_transmittance_1;
    (&p_transmittance_1)->primal_0 = transmittance_3;
    (&p_transmittance_1)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 p_background_0;
    (&p_background_0)->primal_0 = background_1;
    (&p_background_0)->differential_0 = _S21;
    blend_background_bwd_impl_0(&p_rgb_0, &p_transmittance_1, &p_background_0, _S20);
    *v_rgb_0 = p_rgb_0.differential_0;
    *v_transmittance_1 = p_transmittance_1.differential_0;
    *v_background_0 = p_background_0.differential_0;
    return;
}

inline __device__ void _d_pow_0(DiffPair_float_0 * dpx_2, DiffPair_float_0 * dpy_2, float dOut_2)
{
    if(((*dpx_2).primal_0) < 9.99999997475242708e-07f)
    {
        dpx_2->primal_0 = (*dpx_2).primal_0;
        dpx_2->differential_0 = 0.0f;
        dpy_2->primal_0 = (*dpy_2).primal_0;
        dpy_2->differential_0 = 0.0f;
    }
    else
    {
        float val_0 = (F32_pow(((*dpx_2).primal_0), ((*dpy_2).primal_0)));
        DiffPair_float_0 _S22 = *dpx_2;
        float _S23 = val_0 * (*dpy_2).primal_0 / (*dpx_2).primal_0 * dOut_2;
        dpx_2->primal_0 = (*dpx_2).primal_0;
        dpx_2->differential_0 = _S23;
        float _S24 = val_0 * (F32_log((_S22.primal_0))) * dOut_2;
        dpy_2->primal_0 = (*dpy_2).primal_0;
        dpy_2->differential_0 = _S24;
    }
    return;
}

inline __device__ DiffPair_float_0 _d_pow_1(DiffPair_float_0 * dpx_3, DiffPair_float_0 * dpy_3)
{
    float _S25 = dpx_3->primal_0;
    if((dpx_3->primal_0) < 9.99999997475242708e-07f)
    {
        DiffPair_float_0 _S26 = { 0.0f, 0.0f };
        return _S26;
    }
    float val_1 = (F32_pow((_S25), (dpy_3->primal_0)));
    DiffPair_float_0 _S27 = { val_1, val_1 * (F32_log((_S25))) * dpy_3->differential_0 + val_1 * dpy_3->primal_0 / _S25 * dpx_3->differential_0 };
    return _S27;
}

inline __device__ float linear_rgb_to_srgb(float x_4)
{
    float _S28;
    if(x_4 < 0.00313080009073019f)
    {
        _S28 = x_4 * 12.92000007629394531f;
    }
    else
    {
        _S28 = 1.0549999475479126f * (F32_pow((x_4), (0.4166666567325592f))) - 0.05499999970197678f;
    }
    return _S28;
}

inline __device__ float linear_rgb_to_srgb_grad(float x_5)
{
    float _S29;
    if(x_5 < 0.00313080009073019f)
    {
        _S29 = 12.92000007629394531f;
    }
    else
    {
        DiffPair_float_0 _S30;
        (&_S30)->primal_0 = x_5;
        (&_S30)->differential_0 = 1.0f;
        DiffPair_float_0 _S31;
        (&_S31)->primal_0 = 0.4166666567325592f;
        (&_S31)->differential_0 = 0.0f;
        DiffPair_float_0 _S32 = _d_pow_1(&_S30, &_S31);
        _S29 = _S32.differential_0 * 1.0549999475479126f;
    }
    return _S29;
}

inline __device__ float srgb_to_linear_rgb(float x_6)
{
    float _S33;
    if(x_6 < 0.04044999927282333f)
    {
        _S33 = x_6 * 0.07739938050508499f;
    }
    else
    {
        _S33 = (F32_pow((0.94786733388900757f * (x_6 + 0.05499999970197678f)), (2.40000009536743164f)));
    }
    return _S33;
}

inline __device__ float srgb_to_linear_rgb_grad(float x_7)
{
    float _S34;
    if(x_7 < 0.04044999927282333f)
    {
        _S34 = 0.07739938050508499f;
    }
    else
    {
        DiffPair_float_0 _S35;
        (&_S35)->primal_0 = 0.94786733388900757f * (x_7 + 0.05499999970197678f);
        (&_S35)->differential_0 = 0.94786733388900757f;
        DiffPair_float_0 _S36;
        (&_S36)->primal_0 = 2.40000009536743164f;
        (&_S36)->differential_0 = 0.0f;
        DiffPair_float_0 _S37 = _d_pow_1(&_S35, &_S36);
        _S34 = _S37.differential_0;
    }
    return _S34;
}

inline __device__ void _d_sqrt_0(DiffPair_float_0 * dpx_4, float dOut_3)
{
    float _S38 = 0.5f / (F32_sqrt(((F32_max((1.00000001168609742e-07f), ((*dpx_4).primal_0)))))) * dOut_3;
    dpx_4->primal_0 = (*dpx_4).primal_0;
    dpx_4->differential_0 = _S38;
    return;
}

inline __device__ float splat_dc_encode(float dc_0)
{
    float s_0 = (F32_sqrt((0.50001537799835205f)));
    float u_0 = 0.282094806432724f * dc_0 + 0.5f;
    float w_0;
    if(u_0 >= 0.0f)
    {
        w_0 = 2.0f * (F32_sqrt((u_0 + 0.00001537870230095f)));
    }
    else
    {
        w_0 = 0.00784313771873713f + u_0 / 0.00392156885936856f;
    }
    return (w_0 - 2.0f * s_0) * s_0 / 0.282094806432724f;
}

inline __device__ float splat_dc_decode(float x_8)
{
    float s_1 = (F32_sqrt((0.50001537799835205f)));
    float w_1 = x_8 * 0.282094806432724f / s_1 + 2.0f * s_1;
    float u_1;
    if(w_1 >= 0.00784313771873713f)
    {
        u_1 = 0.25f * w_1 * w_1 - 0.00001537870230095f;
    }
    else
    {
        u_1 = (w_1 - 0.00784313771873713f) * 0.00392156885936856f;
    }
    return (u_1 - 0.5f) / 0.282094806432724f;
}

struct DiffPair_matrixx3Cfloatx2C3x2C3x3E_0
{
    Matrix<float, 3, 3>  primal_0;
    Matrix<float, 3, 3>  differential_0;
};

inline __device__ void _d_mul_0(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * left_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * right_0, float3  dOut_4)
{
    float _S39 = (*left_0).primal_0.rows[int(0)].x * dOut_4.x;
    Matrix<float, 3, 3>  left_d_result_0;
    *&(((&left_d_result_0)->rows + (int(0)))->x) = (*right_0).primal_0.x * dOut_4.x;
    float sum_0 = _S39 + (*left_0).primal_0.rows[int(1)].x * dOut_4.y;
    *&(((&left_d_result_0)->rows + (int(1)))->x) = (*right_0).primal_0.x * dOut_4.y;
    float sum_1 = sum_0 + (*left_0).primal_0.rows[int(2)].x * dOut_4.z;
    *&(((&left_d_result_0)->rows + (int(2)))->x) = (*right_0).primal_0.x * dOut_4.z;
    float3  right_d_result_0;
    *&((&right_d_result_0)->x) = sum_1;
    float _S40 = (*left_0).primal_0.rows[int(0)].y * dOut_4.x;
    *&(((&left_d_result_0)->rows + (int(0)))->y) = (*right_0).primal_0.y * dOut_4.x;
    float sum_2 = _S40 + (*left_0).primal_0.rows[int(1)].y * dOut_4.y;
    *&(((&left_d_result_0)->rows + (int(1)))->y) = (*right_0).primal_0.y * dOut_4.y;
    float sum_3 = sum_2 + (*left_0).primal_0.rows[int(2)].y * dOut_4.z;
    *&(((&left_d_result_0)->rows + (int(2)))->y) = (*right_0).primal_0.y * dOut_4.z;
    *&((&right_d_result_0)->y) = sum_3;
    float _S41 = (*left_0).primal_0.rows[int(0)].z * dOut_4.x;
    *&(((&left_d_result_0)->rows + (int(0)))->z) = (*right_0).primal_0.z * dOut_4.x;
    float sum_4 = _S41 + (*left_0).primal_0.rows[int(1)].z * dOut_4.y;
    *&(((&left_d_result_0)->rows + (int(1)))->z) = (*right_0).primal_0.z * dOut_4.y;
    float sum_5 = sum_4 + (*left_0).primal_0.rows[int(2)].z * dOut_4.z;
    *&(((&left_d_result_0)->rows + (int(2)))->z) = (*right_0).primal_0.z * dOut_4.z;
    *&((&right_d_result_0)->z) = sum_5;
    left_0->primal_0 = (*left_0).primal_0;
    left_0->differential_0 = left_d_result_0;
    right_0->primal_0 = (*right_0).primal_0;
    right_0->differential_0 = right_d_result_0;
    return;
}

inline __device__ float3  mul_0(Matrix<float, 3, 3>  left_1, float3  right_1)
{
    float3  result_6;
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
        int j_0 = int(0);
        float sum_6 = 0.0f;
        for(;;)
        {
            if(j_0 < int(3))
            {
            }
            else
            {
                break;
            }
            float sum_7 = sum_6 + _slang_vector_get_element(left_1.rows[i_4], j_0) * _slang_vector_get_element(right_1, j_0);
            j_0 = j_0 + int(1);
            sum_6 = sum_7;
        }
        *_slang_vector_get_element_ptr(&result_6, i_4) = sum_6;
        i_4 = i_4 + int(1);
    }
    return result_6;
}

inline __device__ float xfer_max0_0(float x_9)
{
    return (F32_max((x_9), (0.0f)));
}

inline __device__ float xfer_filmic_0(float x_10)
{
    float t_0 = xfer_max0_0(x_10 - 0.00400000018998981f);
    float _S42 = 6.19999980926513672f * t_0;
    return t_0 * (_S42 + 0.5f) / (t_0 * (_S42 + 1.70000004768371582f) + 0.05999999865889549f);
}

inline __device__ float xfer_aces_0(float x_11)
{
    return x_11 * (2.50999999046325684f * x_11 + 0.02999999932944775f) / (x_11 * (2.43000006675720215f * x_11 + 0.5899999737739563f) + 0.14000000059604645f);
}

inline __device__ void _d_clamp_0(DiffPair_float_0 * dpx_5, DiffPair_float_0 * dpMin_0, DiffPair_float_0 * dpMax_0, float dOut_5)
{
    DiffPair_float_0 _S43 = *dpx_5;
    bool _S44;
    if(((*dpx_5).primal_0) >= ((*dpMin_0).primal_0))
    {
        _S44 = ((*dpx_5).primal_0) <= ((*dpMax_0).primal_0);
    }
    else
    {
        _S44 = false;
    }
    float _S45;
    if(_S44)
    {
        _S45 = dOut_5;
    }
    else
    {
        _S45 = 0.0f;
    }
    dpx_5->primal_0 = _S43.primal_0;
    dpx_5->differential_0 = _S45;
    DiffPair_float_0 _S46 = *dpMin_0;
    if((_S43.primal_0) < ((*dpMin_0).primal_0))
    {
        _S45 = dOut_5;
    }
    else
    {
        _S45 = 0.0f;
    }
    dpMin_0->primal_0 = _S46.primal_0;
    dpMin_0->differential_0 = _S45;
    DiffPair_float_0 _S47 = *dpMax_0;
    if(((*dpx_5).primal_0) > ((*dpMax_0).primal_0))
    {
        _S45 = dOut_5;
    }
    else
    {
        _S45 = 0.0f;
    }
    dpMax_0->primal_0 = _S47.primal_0;
    dpMax_0->differential_0 = _S45;
    return;
}

inline __device__ float clamp_0(float x_12, float minBound_0, float maxBound_0)
{
    return (F32_min(((F32_max((x_12), (minBound_0)))), (maxBound_0)));
}

inline __device__ float xfer_clamp01_0(float x_13)
{
    return clamp_0(x_13, 0.0f, 1.0f);
}

inline __device__ float xfer_hable_0(float x_14)
{
    float _S48 = 0.15000000596046448f * x_14;
    return (x_14 * (_S48 + 0.05000000074505806f) + 0.00400000018998981f) / (x_14 * (_S48 + 0.5f) + 0.06000000238418579f) - 0.06666666269302368f;
}

inline __device__ float xfer_uncharted2_0(float x_15)
{
    return xfer_hable_0(xfer_max0_0(x_15)) / xfer_hable_0(11.19999980926513672f);
}

inline __device__ float tone_encode_0(float x_16, int transfer_0)
{
    if(transfer_0 == int(3))
    {
        return xfer_filmic_0(x_16);
    }
    float _S49;
    if(transfer_0 == int(2))
    {
        float _S50 = xfer_clamp01_0(xfer_aces_0(xfer_max0_0(x_16)));
        if(_S50 < 0.00313080009073019f)
        {
            _S49 = _S50 * 12.92000007629394531f;
        }
        else
        {
            _S49 = 1.0549999475479126f * (F32_pow((_S50), (0.4166666567325592f))) - 0.05499999970197678f;
        }
        return _S49;
    }
    if(transfer_0 == int(4))
    {
        float _S51 = xfer_clamp01_0(xfer_uncharted2_0(x_16));
        if(_S51 < 0.00313080009073019f)
        {
            _S49 = _S51 * 12.92000007629394531f;
        }
        else
        {
            _S49 = 1.0549999475479126f * (F32_pow((_S51), (0.4166666567325592f))) - 0.05499999970197678f;
        }
        return _S49;
    }
    if(transfer_0 == int(1))
    {
        float _S52 = xfer_clamp01_0(x_16);
        if(_S52 < 0.00313080009073019f)
        {
            _S49 = _S52 * 12.92000007629394531f;
        }
        else
        {
            _S49 = 1.0549999475479126f * (F32_pow((_S52), (0.4166666567325592f))) - 0.05499999970197678f;
        }
        return _S49;
    }
    if(x_16 < 0.00313080009073019f)
    {
        _S49 = x_16 * 12.92000007629394531f;
    }
    else
    {
        _S49 = 1.0549999475479126f * (F32_pow((x_16), (0.4166666567325592f))) - 0.05499999970197678f;
    }
    return _S49;
}

inline __device__ float3  working_to_display(float3  rgb_2, Matrix<float, 3, 3>  color_matrix_0, int transfer_1, bool is_linear_0)
{
    float3  _S53;
    if(!is_linear_0)
    {
        float _S54 = rgb_2.x;
        float _S55;
        if(_S54 < 0.04044999927282333f)
        {
            _S55 = _S54 * 0.07739938050508499f;
        }
        else
        {
            _S55 = (F32_pow((0.94786733388900757f * (_S54 + 0.05499999970197678f)), (2.40000009536743164f)));
        }
        float _S56 = rgb_2.y;
        float _S57;
        if(_S56 < 0.04044999927282333f)
        {
            _S57 = _S56 * 0.07739938050508499f;
        }
        else
        {
            _S57 = (F32_pow((0.94786733388900757f * (_S56 + 0.05499999970197678f)), (2.40000009536743164f)));
        }
        float _S58 = rgb_2.z;
        float _S59;
        if(_S58 < 0.04044999927282333f)
        {
            _S59 = _S58 * 0.07739938050508499f;
        }
        else
        {
            _S59 = (F32_pow((0.94786733388900757f * (_S58 + 0.05499999970197678f)), (2.40000009536743164f)));
        }
        _S53 = make_float3 (_S55, _S57, _S59);
    }
    else
    {
        _S53 = rgb_2;
    }
    float3  _S60 = mul_0(color_matrix_0, _S53);
    return make_float3 (tone_encode_0(_S60.x, transfer_1), tone_encode_0(_S60.y, transfer_1), tone_encode_0(_S60.z, transfer_1));
}

inline __device__ float s_primal_ctx_pow_0(float _S61, float _S62)
{
    return (F32_pow((_S61), (_S62)));
}

inline __device__ float3  s_primal_ctx_mul_0(Matrix<float, 3, 3>  _S63, float3  _S64)
{
    return mul_0(_S63, _S64);
}

inline __device__ float s_primal_ctx_xfer_max0_0(float dpx_6)
{
    return (F32_max((dpx_6), (0.0f)));
}

inline __device__ float s_primal_ctx_xfer_aces_0(float dpx_7)
{
    return dpx_7 * (2.50999999046325684f * dpx_7 + 0.02999999932944775f) / (dpx_7 * (2.43000006675720215f * dpx_7 + 0.5899999737739563f) + 0.14000000059604645f);
}

inline __device__ float s_primal_ctx_clamp_0(float _S65, float _S66, float _S67)
{
    return clamp_0(_S65, _S66, _S67);
}

inline __device__ float s_primal_ctx_xfer_clamp01_0(float dpx_8)
{
    return s_primal_ctx_clamp_0(dpx_8, 0.0f, 1.0f);
}

inline __device__ float s_primal_ctx_xfer_hable_0(float dpx_9)
{
    float _S68 = 0.15000000596046448f * dpx_9;
    return (dpx_9 * (_S68 + 0.05000000074505806f) + 0.00400000018998981f) / (dpx_9 * (_S68 + 0.5f) + 0.06000000238418579f) - 0.06666666269302368f;
}

inline __device__ float s_primal_ctx_xfer_uncharted2_0(float dpx_10)
{
    return s_primal_ctx_xfer_hable_0(s_primal_ctx_xfer_max0_0(dpx_10)) / s_primal_ctx_xfer_hable_0(11.19999980926513672f);
}

inline __device__ void s_bwd_prop_pow_0(DiffPair_float_0 * _S69, DiffPair_float_0 * _S70, float _S71)
{
    _d_pow_0(_S69, _S70, _S71);
    return;
}

inline __device__ void s_bwd_prop_clamp_0(DiffPair_float_0 * _S72, DiffPair_float_0 * _S73, DiffPair_float_0 * _S74, float _S75)
{
    _d_clamp_0(_S72, _S73, _S74, _S75);
    return;
}

inline __device__ void s_bwd_prop_xfer_clamp01_0(DiffPair_float_0 * dpx_11, float _s_dOut_1)
{
    DiffPair_float_0 _S76;
    (&_S76)->primal_0 = (*dpx_11).primal_0;
    (&_S76)->differential_0 = 0.0f;
    DiffPair_float_0 _S77;
    (&_S77)->primal_0 = 0.0f;
    (&_S77)->differential_0 = 0.0f;
    DiffPair_float_0 _S78;
    (&_S78)->primal_0 = 1.0f;
    (&_S78)->differential_0 = 0.0f;
    s_bwd_prop_clamp_0(&_S76, &_S77, &_S78, _s_dOut_1);
    dpx_11->primal_0 = (*dpx_11).primal_0;
    dpx_11->differential_0 = _S76.differential_0;
    return;
}

inline __device__ void s_bwd_prop_xfer_hable_0(DiffPair_float_0 * dpx_12, float _s_dOut_2)
{
    float _S79 = 0.15000000596046448f * (*dpx_12).primal_0;
    float _S80 = _S79 + 0.05000000074505806f;
    float _S81 = _S79 + 0.5f;
    float _S82 = (*dpx_12).primal_0 * _S81 + 0.06000000238418579f;
    float _S83 = _s_dOut_2 / (_S82 * _S82);
    float _S84 = ((*dpx_12).primal_0 * _S80 + 0.00400000018998981f) * - _S83;
    float _S85 = _S82 * _S83;
    float _S86 = _S81 * _S84 + _S80 * _S85 + 0.15000000596046448f * ((*dpx_12).primal_0 * _S84 + (*dpx_12).primal_0 * _S85);
    dpx_12->primal_0 = (*dpx_12).primal_0;
    dpx_12->differential_0 = _S86;
    return;
}

inline __device__ void s_bwd_prop_xfer_max0_0(DiffPair_float_0 * dpx_13, float _s_dOut_3)
{
    DiffPair_float_0 _S87;
    (&_S87)->primal_0 = (*dpx_13).primal_0;
    (&_S87)->differential_0 = 0.0f;
    DiffPair_float_0 _S88;
    (&_S88)->primal_0 = 0.0f;
    (&_S88)->differential_0 = 0.0f;
    _d_max_0(&_S87, &_S88, _s_dOut_3);
    dpx_13->primal_0 = (*dpx_13).primal_0;
    dpx_13->differential_0 = _S87.differential_0;
    return;
}

inline __device__ void s_bwd_prop_xfer_uncharted2_0(DiffPair_float_0 * dpx_14, float _s_dOut_4)
{
    float _S89 = s_primal_ctx_xfer_hable_0(11.19999980926513672f);
    float _S90 = _S89 * (_s_dOut_4 / (_S89 * _S89));
    DiffPair_float_0 _S91;
    (&_S91)->primal_0 = s_primal_ctx_xfer_max0_0((*dpx_14).primal_0);
    (&_S91)->differential_0 = 0.0f;
    s_bwd_prop_xfer_hable_0(&_S91, _S90);
    DiffPair_float_0 _S92;
    (&_S92)->primal_0 = (*dpx_14).primal_0;
    (&_S92)->differential_0 = 0.0f;
    s_bwd_prop_xfer_max0_0(&_S92, _S91.differential_0);
    dpx_14->primal_0 = (*dpx_14).primal_0;
    dpx_14->differential_0 = _S92.differential_0;
    return;
}

inline __device__ void s_bwd_prop_xfer_aces_0(DiffPair_float_0 * dpx_15, float _s_dOut_5)
{
    float _S93 = 2.50999999046325684f * (*dpx_15).primal_0 + 0.02999999932944775f;
    float _S94 = 2.43000006675720215f * (*dpx_15).primal_0 + 0.5899999737739563f;
    float _S95 = (*dpx_15).primal_0 * _S94 + 0.14000000059604645f;
    float _S96 = _s_dOut_5 / (_S95 * _S95);
    float _S97 = (*dpx_15).primal_0 * _S93 * - _S96;
    float _S98 = _S95 * _S96;
    float _S99 = _S94 * _S97 + 2.43000006675720215f * ((*dpx_15).primal_0 * _S97) + _S93 * _S98 + 2.50999999046325684f * ((*dpx_15).primal_0 * _S98);
    dpx_15->primal_0 = (*dpx_15).primal_0;
    dpx_15->differential_0 = _S99;
    return;
}

inline __device__ void s_bwd_prop_xfer_filmic_0(DiffPair_float_0 * dpx_16, float _s_dOut_6)
{
    float _S100 = (*dpx_16).primal_0 - 0.00400000018998981f;
    float _S101 = s_primal_ctx_xfer_max0_0(_S100);
    float _S102 = 6.19999980926513672f * _S101;
    float _S103 = _S102 + 0.5f;
    float _S104 = _S102 + 1.70000004768371582f;
    float _S105 = _S101 * _S104 + 0.05999999865889549f;
    float _S106 = _s_dOut_6 / (_S105 * _S105);
    float _S107 = _S101 * _S103 * - _S106;
    float _S108 = _S105 * _S106;
    float _S109 = _S104 * _S107 + _S103 * _S108 + 6.19999980926513672f * (_S101 * _S107 + _S101 * _S108);
    DiffPair_float_0 _S110;
    (&_S110)->primal_0 = _S100;
    (&_S110)->differential_0 = 0.0f;
    s_bwd_prop_xfer_max0_0(&_S110, _S109);
    dpx_16->primal_0 = (*dpx_16).primal_0;
    dpx_16->differential_0 = _S110.differential_0;
    return;
}

inline __device__ void s_bwd_prop_tone_encode_0(DiffPair_float_0 * dpx_17, int transfer_2, float _s_dOut_7)
{
    DiffPair_float_0 _S111 = *dpx_17;
    bool _S112 = transfer_2 == int(3);
    bool _S113 = !_S112;
    bool _runFlag_0;
    bool _runFlag_1;
    bool _runFlag_2;
    bool _S114;
    bool _S115;
    bool _S116;
    float _S117;
    float _S118;
    float _S119;
    float _S120;
    float _S121;
    float _S122;
    if(_S113)
    {
        bool _S123 = transfer_2 == int(2);
        if(_S123)
        {
            float _S124 = s_primal_ctx_xfer_max0_0(_S111.primal_0);
            float _S125 = s_primal_ctx_xfer_aces_0(_S124);
            float _S126 = s_primal_ctx_xfer_clamp01_0(_S125);
            _runFlag_0 = false;
            _S117 = _S126;
            _S118 = _S125;
            _S119 = _S124;
        }
        else
        {
            _runFlag_0 = _S113;
            _S117 = 0.0f;
            _S118 = 0.0f;
            _S119 = 0.0f;
        }
        if(_runFlag_0)
        {
            bool _S127 = transfer_2 == int(4);
            if(_S127)
            {
                float _S128 = s_primal_ctx_xfer_uncharted2_0(_S111.primal_0);
                float _S129 = s_primal_ctx_xfer_clamp01_0(_S128);
                _runFlag_1 = false;
                _S120 = _S129;
                _S121 = _S128;
            }
            else
            {
                _runFlag_1 = _runFlag_0;
                _S120 = 0.0f;
                _S121 = 0.0f;
            }
            if(_runFlag_1)
            {
                bool _S130 = transfer_2 == int(1);
                if(_S130)
                {
                    float _S131 = s_primal_ctx_xfer_clamp01_0(_S111.primal_0);
                    _runFlag_2 = false;
                    _S122 = _S131;
                }
                else
                {
                    _runFlag_2 = _runFlag_1;
                    _S122 = 0.0f;
                }
                _S114 = _S130;
            }
            else
            {
                _runFlag_2 = false;
                _S114 = false;
                _S122 = 0.0f;
            }
            float _S132 = _S120;
            float _S133 = _S121;
            _S120 = _S122;
            _S115 = _S127;
            _S121 = _S132;
            _S122 = _S133;
        }
        else
        {
            _runFlag_1 = false;
            _runFlag_2 = false;
            _S114 = false;
            _S120 = 0.0f;
            _S115 = false;
            _S121 = 0.0f;
            _S122 = 0.0f;
        }
        float _S134 = _S117;
        float _S135 = _S118;
        float _S136 = _S119;
        _S117 = _S120;
        _S118 = _S121;
        _S119 = _S122;
        _S116 = _S123;
        _S120 = _S134;
        _S121 = _S135;
        _S122 = _S136;
    }
    else
    {
        _runFlag_0 = false;
        _runFlag_1 = false;
        _runFlag_2 = false;
        _S114 = false;
        _S117 = 0.0f;
        _S115 = false;
        _S118 = 0.0f;
        _S119 = 0.0f;
        _S116 = false;
        _S120 = 0.0f;
        _S121 = 0.0f;
        _S122 = 0.0f;
    }
    if(_S113)
    {
        if(_runFlag_0)
        {
            float _S137;
            if(_runFlag_1)
            {
                float _S138;
                if(_runFlag_2)
                {
                    if((_S111.primal_0) < 0.00313080009073019f)
                    {
                        _S137 = 12.92000007629394531f * _s_dOut_7;
                    }
                    else
                    {
                        float _S139 = 1.0549999475479126f * _s_dOut_7;
                        DiffPair_float_0 _S140;
                        (&_S140)->primal_0 = _S111.primal_0;
                        (&_S140)->differential_0 = 0.0f;
                        DiffPair_float_0 _S141;
                        (&_S141)->primal_0 = 0.4166666567325592f;
                        (&_S141)->differential_0 = 0.0f;
                        s_bwd_prop_pow_0(&_S140, &_S141, _S139);
                        _S137 = _S140.differential_0;
                    }
                    float _S142 = _S137;
                    _S137 = 0.0f;
                    _S138 = _S142;
                }
                else
                {
                    _S137 = _s_dOut_7;
                    _S138 = 0.0f;
                }
                if(_S114)
                {
                    if(_S117 < 0.00313080009073019f)
                    {
                        _S117 = 12.92000007629394531f * _S137;
                    }
                    else
                    {
                        float _S143 = 1.0549999475479126f * _S137;
                        DiffPair_float_0 _S144;
                        (&_S144)->primal_0 = _S117;
                        (&_S144)->differential_0 = 0.0f;
                        DiffPair_float_0 _S145;
                        (&_S145)->primal_0 = 0.4166666567325592f;
                        (&_S145)->differential_0 = 0.0f;
                        s_bwd_prop_pow_0(&_S144, &_S145, _S143);
                        _S117 = _S144.differential_0;
                    }
                    DiffPair_float_0 _S146;
                    (&_S146)->primal_0 = _S111.primal_0;
                    (&_S146)->differential_0 = 0.0f;
                    s_bwd_prop_xfer_clamp01_0(&_S146, _S117);
                    float _S147 = _S146.differential_0 + _S138;
                    _S117 = 0.0f;
                    _S137 = _S147;
                }
                else
                {
                    _S117 = _S137;
                    _S137 = _S138;
                }
            }
            else
            {
                _S117 = _s_dOut_7;
                _S137 = 0.0f;
            }
            if(_S115)
            {
                if(_S118 < 0.00313080009073019f)
                {
                    _S117 = 12.92000007629394531f * _S117;
                }
                else
                {
                    float _S148 = 1.0549999475479126f * _S117;
                    DiffPair_float_0 _S149;
                    (&_S149)->primal_0 = _S118;
                    (&_S149)->differential_0 = 0.0f;
                    DiffPair_float_0 _S150;
                    (&_S150)->primal_0 = 0.4166666567325592f;
                    (&_S150)->differential_0 = 0.0f;
                    s_bwd_prop_pow_0(&_S149, &_S150, _S148);
                    _S117 = _S149.differential_0;
                }
                DiffPair_float_0 _S151;
                (&_S151)->primal_0 = _S119;
                (&_S151)->differential_0 = 0.0f;
                s_bwd_prop_xfer_clamp01_0(&_S151, _S117);
                DiffPair_float_0 _S152;
                (&_S152)->primal_0 = _S111.primal_0;
                (&_S152)->differential_0 = 0.0f;
                s_bwd_prop_xfer_uncharted2_0(&_S152, _S151.differential_0);
                float _S153 = _S152.differential_0 + _S137;
                _S117 = 0.0f;
                _S118 = _S153;
            }
            else
            {
                _S118 = _S137;
            }
        }
        else
        {
            _S117 = _s_dOut_7;
            _S118 = 0.0f;
        }
        if(_S116)
        {
            if(_S120 < 0.00313080009073019f)
            {
                _S117 = 12.92000007629394531f * _S117;
            }
            else
            {
                float _S154 = 1.0549999475479126f * _S117;
                DiffPair_float_0 _S155;
                (&_S155)->primal_0 = _S120;
                (&_S155)->differential_0 = 0.0f;
                DiffPair_float_0 _S156;
                (&_S156)->primal_0 = 0.4166666567325592f;
                (&_S156)->differential_0 = 0.0f;
                s_bwd_prop_pow_0(&_S155, &_S156, _S154);
                _S117 = _S155.differential_0;
            }
            DiffPair_float_0 _S157;
            (&_S157)->primal_0 = _S121;
            (&_S157)->differential_0 = 0.0f;
            s_bwd_prop_xfer_clamp01_0(&_S157, _S117);
            DiffPair_float_0 _S158;
            (&_S158)->primal_0 = _S122;
            (&_S158)->differential_0 = 0.0f;
            s_bwd_prop_xfer_aces_0(&_S158, _S157.differential_0);
            DiffPair_float_0 _S159;
            (&_S159)->primal_0 = _S111.primal_0;
            (&_S159)->differential_0 = 0.0f;
            s_bwd_prop_xfer_max0_0(&_S159, _S158.differential_0);
            float _S160 = _S159.differential_0 + _S118;
            _S117 = 0.0f;
            _S118 = _S160;
        }
    }
    else
    {
        _S117 = _s_dOut_7;
        _S118 = 0.0f;
    }
    if(_S112)
    {
        DiffPair_float_0 _S161;
        (&_S161)->primal_0 = _S111.primal_0;
        (&_S161)->differential_0 = 0.0f;
        s_bwd_prop_xfer_filmic_0(&_S161, _S117);
        _S117 = _S161.differential_0 + _S118;
    }
    else
    {
        _S117 = _S118;
    }
    dpx_17->primal_0 = (*dpx_17).primal_0;
    dpx_17->differential_0 = _S117;
    return;
}

inline __device__ void s_bwd_prop_mul_0(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S162, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S163, float3  _S164)
{
    _d_mul_0(_S162, _S163, _S164);
    return;
}

inline __device__ void s_bwd_prop_working_to_display_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dprgb_0, Matrix<float, 3, 3>  color_matrix_1, int transfer_3, bool is_linear_1, float3  _s_dOut_8)
{
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S165 = *dprgb_0;
    bool _S166 = !is_linear_1;
    float _S167;
    float _S168;
    float _S169;
    float3  _S170;
    if(_S166)
    {
        float _S171 = _S165.primal_0.x;
        if(_S171 < 0.04044999927282333f)
        {
            _S167 = _S171 * 0.07739938050508499f;
        }
        else
        {
            _S167 = s_primal_ctx_pow_0(0.94786733388900757f * (_S171 + 0.05499999970197678f), 2.40000009536743164f);
        }
        float _S172 = _S165.primal_0.y;
        if(_S172 < 0.04044999927282333f)
        {
            _S168 = _S172 * 0.07739938050508499f;
        }
        else
        {
            _S168 = s_primal_ctx_pow_0(0.94786733388900757f * (_S172 + 0.05499999970197678f), 2.40000009536743164f);
        }
        float _S173 = _S165.primal_0.z;
        if(_S173 < 0.04044999927282333f)
        {
            _S169 = _S173 * 0.07739938050508499f;
        }
        else
        {
            _S169 = s_primal_ctx_pow_0(0.94786733388900757f * (_S173 + 0.05499999970197678f), 2.40000009536743164f);
        }
        _S170 = make_float3 (_S167, _S168, _S169);
        _S167 = _S173;
        _S168 = _S172;
        _S169 = _S171;
    }
    else
    {
        _S170 = _S165.primal_0;
        _S167 = 0.0f;
        _S168 = 0.0f;
        _S169 = 0.0f;
    }
    float3  _S174 = s_primal_ctx_mul_0(color_matrix_1, _S170);
    float _S175 = _S174.x;
    float _S176 = _S174.y;
    float _S177 = _S174.z;
    DiffPair_float_0 _S178;
    (&_S178)->primal_0 = _S177;
    (&_S178)->differential_0 = 0.0f;
    s_bwd_prop_tone_encode_0(&_S178, transfer_3, _s_dOut_8.z);
    DiffPair_float_0 _S179;
    (&_S179)->primal_0 = _S176;
    (&_S179)->differential_0 = 0.0f;
    s_bwd_prop_tone_encode_0(&_S179, transfer_3, _s_dOut_8.y);
    DiffPair_float_0 _S180;
    (&_S180)->primal_0 = _S175;
    (&_S180)->differential_0 = 0.0f;
    s_bwd_prop_tone_encode_0(&_S180, transfer_3, _s_dOut_8.x);
    float3  _S181 = make_float3 (_S180.differential_0, _S179.differential_0, _S178.differential_0);
    Matrix<float, 3, 3>  _S182 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S183;
    (&_S183)->primal_0 = color_matrix_1;
    (&_S183)->differential_0 = _S182;
    float3  _S184 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S185;
    (&_S185)->primal_0 = _S170;
    (&_S185)->differential_0 = _S184;
    s_bwd_prop_mul_0(&_S183, &_S185, _S181);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S186 = _S185;
    if(_S166)
    {
        bool _S187 = _S167 < 0.04044999927282333f;
        if(_S187)
        {
            _S167 = 0.0f;
        }
        else
        {
            _S167 = 0.94786733388900757f * (_S167 + 0.05499999970197678f);
        }
        if(_S187)
        {
            _S167 = 0.07739938050508499f * _S186.differential_0.z;
        }
        else
        {
            DiffPair_float_0 _S188;
            (&_S188)->primal_0 = _S167;
            (&_S188)->differential_0 = 0.0f;
            DiffPair_float_0 _S189;
            (&_S189)->primal_0 = 2.40000009536743164f;
            (&_S189)->differential_0 = 0.0f;
            s_bwd_prop_pow_0(&_S188, &_S189, _S186.differential_0.z);
            _S167 = 0.94786733388900757f * _S188.differential_0;
        }
        bool _S190 = _S168 < 0.04044999927282333f;
        if(_S190)
        {
            _S168 = 0.0f;
        }
        else
        {
            _S168 = 0.94786733388900757f * (_S168 + 0.05499999970197678f);
        }
        if(_S190)
        {
            _S168 = 0.07739938050508499f * _S186.differential_0.y;
        }
        else
        {
            DiffPair_float_0 _S191;
            (&_S191)->primal_0 = _S168;
            (&_S191)->differential_0 = 0.0f;
            DiffPair_float_0 _S192;
            (&_S192)->primal_0 = 2.40000009536743164f;
            (&_S192)->differential_0 = 0.0f;
            s_bwd_prop_pow_0(&_S191, &_S192, _S186.differential_0.y);
            _S168 = 0.94786733388900757f * _S191.differential_0;
        }
        bool _S193 = _S169 < 0.04044999927282333f;
        if(_S193)
        {
            _S169 = 0.0f;
        }
        else
        {
            _S169 = 0.94786733388900757f * (_S169 + 0.05499999970197678f);
        }
        if(_S193)
        {
            _S169 = 0.07739938050508499f * _S186.differential_0.x;
        }
        else
        {
            DiffPair_float_0 _S194;
            (&_S194)->primal_0 = _S169;
            (&_S194)->differential_0 = 0.0f;
            DiffPair_float_0 _S195;
            (&_S195)->primal_0 = 2.40000009536743164f;
            (&_S195)->differential_0 = 0.0f;
            s_bwd_prop_pow_0(&_S194, &_S195, _S186.differential_0.x);
            _S169 = 0.94786733388900757f * _S194.differential_0;
        }
        _S170 = make_float3 (_S169, _S168, _S167);
    }
    else
    {
        _S170 = _S186.differential_0;
    }
    dprgb_0->primal_0 = (*dprgb_0).primal_0;
    dprgb_0->differential_0 = _S170;
    return;
}

inline __device__ void s_bwd_working_to_display_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S196, Matrix<float, 3, 3>  _S197, int _S198, bool _S199, float3  _S200)
{
    s_bwd_prop_working_to_display_0(_S196, _S197, _S198, _S199, _S200);
    return;
}

inline __device__ float3  working_to_display_bwd(float3  rgb_3, Matrix<float, 3, 3>  color_matrix_2, int transfer_4, bool is_linear_2, float3  v_out_rgb_1)
{
    float3  _S201 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 p_rgb_1;
    (&p_rgb_1)->primal_0 = rgb_3;
    (&p_rgb_1)->differential_0 = _S201;
    s_bwd_working_to_display_0(&p_rgb_1, color_matrix_2, transfer_4, is_linear_2, v_out_rgb_1);
    return p_rgb_1.differential_0;
}

inline __device__ float xfer_filmic_inv_0(float y_4)
{
    float _S202 = (F32_min((y_4), (xfer_filmic_0(11.19999980926513672f))));
    float a_0 = 6.19999980926513672f * (1.0f - _S202);
    float b_0 = 0.5f - 1.70000004768371582f * _S202;
    return (- b_0 + (F32_sqrt(((F32_max((b_0 * b_0 - 4.0f * a_0 * (-0.05999999865889549f * _S202)), (0.0f))))))) / (2.0f * a_0) + 0.00400000018998981f;
}

inline __device__ float xfer_aces_inv_0(float y_5)
{
    float a_1 = 2.50999999046325684f - 2.43000006675720215f * y_5;
    float b_1 = 0.02999999932944775f - 0.5899999737739563f * y_5;
    return (- b_1 + (F32_sqrt(((F32_max((b_1 * b_1 - 4.0f * a_1 * (-0.14000000059604645f * y_5)), (0.0f))))))) / (2.0f * a_1);
}

inline __device__ float xfer_uncharted2_inv_0(float y_6)
{
    float r_0 = y_6 * xfer_hable_0(11.19999980926513672f) + 0.06666666269302368f;
    float a_2 = 0.15000000596046448f * (1.0f - r_0);
    float b_2 = 0.5f * (0.10000000149011612f - r_0);
    return (- b_2 + (F32_sqrt(((F32_max((b_2 * b_2 - 4.0f * a_2 * (0.20000000298023224f * (0.01999999955296516f - r_0 * 0.30000001192092896f))), (0.0f))))))) / (2.0f * a_2);
}

inline __device__ float tone_decode_0(float d_0, int transfer_5)
{
    if(transfer_5 == int(3))
    {
        return xfer_filmic_inv_0(d_0);
    }
    float _S203;
    if(transfer_5 == int(2))
    {
        if(d_0 < 0.04044999927282333f)
        {
            _S203 = d_0 * 0.07739938050508499f;
        }
        else
        {
            _S203 = (F32_pow((0.94786733388900757f * (d_0 + 0.05499999970197678f)), (2.40000009536743164f)));
        }
        return xfer_aces_inv_0(_S203);
    }
    if(transfer_5 == int(4))
    {
        if(d_0 < 0.04044999927282333f)
        {
            _S203 = d_0 * 0.07739938050508499f;
        }
        else
        {
            _S203 = (F32_pow((0.94786733388900757f * (d_0 + 0.05499999970197678f)), (2.40000009536743164f)));
        }
        return xfer_uncharted2_inv_0(_S203);
    }
    if(d_0 < 0.04044999927282333f)
    {
        _S203 = d_0 * 0.07739938050508499f;
    }
    else
    {
        _S203 = (F32_pow((0.94786733388900757f * (d_0 + 0.05499999970197678f)), (2.40000009536743164f)));
    }
    return _S203;
}

inline __device__ float3  display_to_working3(float3  rgb_4, int transfer_6, bool is_linear_3)
{
    float _S204 = tone_decode_0(rgb_4.x, transfer_6);
    float _S205 = tone_decode_0(rgb_4.y, transfer_6);
    float _S206 = tone_decode_0(rgb_4.z, transfer_6);
    float3  lin_0 = make_float3 (_S204, _S205, _S206);
    if(is_linear_3)
    {
        return lin_0;
    }
    float _S207;
    if(_S204 < 0.00313080009073019f)
    {
        _S207 = _S204 * 12.92000007629394531f;
    }
    else
    {
        _S207 = 1.0549999475479126f * (F32_pow((_S204), (0.4166666567325592f))) - 0.05499999970197678f;
    }
    float _S208;
    if(_S205 < 0.00313080009073019f)
    {
        _S208 = _S205 * 12.92000007629394531f;
    }
    else
    {
        _S208 = 1.0549999475479126f * (F32_pow((_S205), (0.4166666567325592f))) - 0.05499999970197678f;
    }
    float _S209;
    if(_S206 < 0.00313080009073019f)
    {
        _S209 = _S206 * 12.92000007629394531f;
    }
    else
    {
        _S209 = 1.0549999475479126f * (F32_pow((_S206), (0.4166666567325592f))) - 0.05499999970197678f;
    }
    return make_float3 (_S207, _S208, _S209);
}

inline __device__ void _d_cross_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * a_3, DiffPair_vectorx3Cfloatx2C3x3E_0 * b_3, float3  dOut_6)
{
    float _S210 = dOut_6.y;
    float _S211 = dOut_6.z;
    float _S212 = dOut_6.x;
    float _S213 = (*a_3).primal_0.z * _S210 + - (*a_3).primal_0.y * _S211;
    float _S214 = - (*a_3).primal_0.z * _S212 + (*a_3).primal_0.x * _S211;
    float _S215 = (*a_3).primal_0.y * _S212 + - (*a_3).primal_0.x * _S210;
    float3  _S216 = make_float3 (- (*b_3).primal_0.z * _S210 + (*b_3).primal_0.y * _S211, (*b_3).primal_0.z * _S212 + - (*b_3).primal_0.x * _S211, - (*b_3).primal_0.y * _S212 + (*b_3).primal_0.x * _S210);
    a_3->primal_0 = (*a_3).primal_0;
    a_3->differential_0 = _S216;
    float3  _S217 = make_float3 (_S213, _S214, _S215);
    b_3->primal_0 = (*b_3).primal_0;
    b_3->differential_0 = _S217;
    return;
}

inline __device__ float3  cross_0(float3  left_2, float3  right_2)
{
    float _S218 = left_2.y;
    float _S219 = right_2.z;
    float _S220 = left_2.z;
    float _S221 = right_2.y;
    float _S222 = right_2.x;
    float _S223 = left_2.x;
    return make_float3 (_S218 * _S219 - _S220 * _S221, _S220 * _S222 - _S223 * _S219, _S223 * _S221 - _S218 * _S222);
}

inline __device__ float length_0(float3  x_17)
{
    return (F32_sqrt((dot_0(x_17, x_17))));
}

inline __device__ float length_1(float2  x_18)
{
    return (F32_sqrt((dot_1(x_18, x_18))));
}

inline __device__ float3  points_to_normal(FixedArray<float3 , 4>  points_0)
{
    float3  _S224 = points_0[int(0)];
    bool _S225;
    if((dot_0(_S224, _S224)) == 0.0f)
    {
        _S225 = true;
    }
    else
    {
        float3  _S226 = points_0[int(1)];
        _S225 = (dot_0(_S226, _S226)) == 0.0f;
    }
    if(_S225)
    {
        _S225 = true;
    }
    else
    {
        float3  _S227 = points_0[int(2)];
        _S225 = (dot_0(_S227, _S227)) == 0.0f;
    }
    if(_S225)
    {
        _S225 = true;
    }
    else
    {
        float3  _S228 = points_0[int(3)];
        _S225 = (dot_0(_S228, _S228)) == 0.0f;
    }
    if(_S225)
    {
        return make_float3 (0.0f);
    }
    float3  normal_0 = cross_0(points_0[int(1)] - points_0[int(0)], - (points_0[int(3)] - points_0[int(2)]));
    float3  normal_1;
    if((dot_0(normal_0, normal_0)) != 0.0f)
    {
        normal_1 = normal_0 / make_float3 (length_0(normal_0));
    }
    else
    {
        normal_1 = normal_0;
    }
    return normal_1;
}

struct DiffPair_arrayx3Cvectorx3Cfloatx2C3x3Ex2C4x3E_0
{
    FixedArray<float3 , 4>  primal_0;
    FixedArray<float3 , 4>  differential_0;
};

inline __device__ float s_primal_ctx_dot_0(float3  _S229, float3  _S230)
{
    return dot_0(_S229, _S230);
}

inline __device__ float3  s_primal_ctx_cross_0(float3  _S231, float3  _S232)
{
    return cross_0(_S231, _S232);
}

inline __device__ void s_bwd_prop_sqrt_0(DiffPair_float_0 * _S233, float _S234)
{
    _d_sqrt_0(_S233, _S234);
    return;
}

inline __device__ void s_bwd_prop_length_impl_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_18, float _s_dOut_9)
{
    float _S235 = (*dpx_18).primal_0.x;
    float _S236 = (*dpx_18).primal_0.y;
    float _S237 = (*dpx_18).primal_0.z;
    DiffPair_float_0 _S238;
    (&_S238)->primal_0 = _S235 * _S235 + _S236 * _S236 + _S237 * _S237;
    (&_S238)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S238, _s_dOut_9);
    float _S239 = (*dpx_18).primal_0.z * _S238.differential_0;
    float _S240 = _S239 + _S239;
    float _S241 = (*dpx_18).primal_0.y * _S238.differential_0;
    float _S242 = _S241 + _S241;
    float _S243 = (*dpx_18).primal_0.x * _S238.differential_0;
    float _S244 = _S243 + _S243;
    float3  _S245 = make_float3 (0.0f);
    *&((&_S245)->z) = _S240;
    *&((&_S245)->y) = _S242;
    *&((&_S245)->x) = _S244;
    dpx_18->primal_0 = (*dpx_18).primal_0;
    dpx_18->differential_0 = _S245;
    return;
}

inline __device__ void s_bwd_length_impl_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S246, float _S247)
{
    s_bwd_prop_length_impl_0(_S246, _S247);
    return;
}

inline __device__ void s_bwd_prop_dot_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S248, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S249, float _S250)
{
    _d_dot_0(_S248, _S249, _S250);
    return;
}

inline __device__ void s_bwd_prop_cross_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S251, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S252, float3  _S253)
{
    _d_cross_0(_S251, _S252, _S253);
    return;
}

inline __device__ void s_bwd_prop_points_to_normal_0(DiffPair_arrayx3Cvectorx3Cfloatx2C3x3Ex2C4x3E_0 * dppoints_0, float3  _s_dOut_10)
{
    FixedArray<float3 , 4>  _S254 = dppoints_0->primal_0;
    float3  _S255 = make_float3 (0.0f);
    float3  _S256 = dppoints_0->primal_0[int(0)];
    bool _S257 = (s_primal_ctx_dot_0(_S256, _S256)) == 0.0f;
    bool _S258;
    float3  _S259;
    if(_S257)
    {
        _S258 = true;
        _S259 = _S255;
    }
    else
    {
        float3  _S260 = _S254[int(1)];
        _S258 = (s_primal_ctx_dot_0(_S260, _S260)) == 0.0f;
        _S259 = _S254[int(1)];
    }
    bool _S261;
    float3  _S262;
    if(_S258)
    {
        _S261 = true;
        _S262 = _S255;
    }
    else
    {
        float3  _S263 = _S254[int(2)];
        _S261 = (s_primal_ctx_dot_0(_S263, _S263)) == 0.0f;
        _S262 = _S254[int(2)];
    }
    bool _S264;
    float3  _S265;
    if(_S261)
    {
        _S264 = true;
        _S265 = _S255;
    }
    else
    {
        float3  _S266 = _S254[int(3)];
        _S264 = (s_primal_ctx_dot_0(_S266, _S266)) == 0.0f;
        _S265 = _S254[int(3)];
    }
    bool _S267 = !_S264;
    float3  _S268;
    float3  _S269;
    float3  _S270;
    float3  _S271;
    float3  _S272;
    if(_S267)
    {
        float3  dx_0 = _S254[int(1)] - _S254[int(0)];
        float3  _S273 = - (_S254[int(3)] - _S254[int(2)]);
        float3  _S274 = s_primal_ctx_cross_0(dx_0, _S273);
        bool _S275 = (s_primal_ctx_dot_0(_S274, _S274)) != 0.0f;
        if(_S275)
        {
            float _S276 = length_0(_S274);
            float3  _S277 = make_float3 (_S276);
            _S268 = make_float3 (_S276 * _S276);
            _S269 = _S277;
        }
        else
        {
            _S268 = _S255;
            _S269 = _S255;
        }
        float3  _S278 = _S269;
        _S264 = _S275;
        _S269 = _S274;
        _S270 = _S278;
        _S271 = dx_0;
        _S272 = _S273;
    }
    else
    {
        _S264 = false;
        _S268 = _S255;
        _S269 = _S255;
        _S270 = _S255;
        _S271 = _S255;
        _S272 = _S255;
    }
    FixedArray<float3 , 4>  _S279;
    if(_S267)
    {
        if(_S264)
        {
            float3  _S280 = _s_dOut_10 / _S268;
            float3  _S281 = _S269 * - _S280;
            float3  _S282 = _S270 * _S280;
            float _S283 = _S281.x + _S281.y + _S281.z;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S284;
            (&_S284)->primal_0 = _S269;
            (&_S284)->differential_0 = _S255;
            s_bwd_length_impl_0(&_S284, _S283);
            _S268 = _S282 + _S284.differential_0;
        }
        else
        {
            _S268 = _s_dOut_10;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S285;
        (&_S285)->primal_0 = _S269;
        (&_S285)->differential_0 = _S255;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S286;
        (&_S286)->primal_0 = _S269;
        (&_S286)->differential_0 = _S255;
        s_bwd_prop_dot_0(&_S285, &_S286, 0.0f);
        float3  _S287 = _S286.differential_0 + _S285.differential_0 + _S268;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S288;
        (&_S288)->primal_0 = _S271;
        (&_S288)->differential_0 = _S255;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S289;
        (&_S289)->primal_0 = _S272;
        (&_S289)->differential_0 = _S255;
        s_bwd_prop_cross_0(&_S288, &_S289, _S287);
        float3  s_diff_dy_T_0 = - _S289.differential_0;
        float3  _S290 = - s_diff_dy_T_0;
        float3  _S291 = - _S288.differential_0;
        FixedArray<float3 , 4>  _S292;
        _S292[int(0)] = _S255;
        _S292[int(1)] = _S255;
        _S292[int(2)] = _S255;
        _S292[int(3)] = _S255;
        _S292[int(2)] = _S290;
        _S292[int(3)] = s_diff_dy_T_0;
        _S292[int(1)] = _S288.differential_0;
        _S279[int(0)] = _S292[int(0)];
        _S279[int(1)] = _S292[int(1)];
        _S279[int(2)] = _S292[int(2)];
        _S279[int(3)] = _S292[int(3)];
        _S268 = _S291;
    }
    else
    {
        _S279[int(0)] = _S255;
        _S279[int(1)] = _S255;
        _S279[int(2)] = _S255;
        _S279[int(3)] = _S255;
        _S268 = _S255;
    }
    if(_S261)
    {
    }
    else
    {
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S293;
        (&_S293)->primal_0 = _S265;
        (&_S293)->differential_0 = _S255;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S294;
        (&_S294)->primal_0 = _S265;
        (&_S294)->differential_0 = _S255;
        s_bwd_prop_dot_0(&_S293, &_S294, 0.0f);
        float3  _S295 = _S294.differential_0 + _S293.differential_0;
        FixedArray<float3 , 4>  _S296;
        _S296[int(0)] = _S255;
        _S296[int(1)] = _S255;
        _S296[int(2)] = _S255;
        _S296[int(3)] = _S255;
        _S296[int(3)] = _S295;
        float3  _S297 = _S279[int(1)] + _S296[int(1)];
        float3  _S298 = _S279[int(2)] + _S296[int(2)];
        float3  _S299 = _S279[int(3)] + _S296[int(3)];
        _S279[int(0)] = _S279[int(0)] + _S296[int(0)];
        _S279[int(1)] = _S297;
        _S279[int(2)] = _S298;
        _S279[int(3)] = _S299;
    }
    if(_S258)
    {
    }
    else
    {
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S300;
        (&_S300)->primal_0 = _S262;
        (&_S300)->differential_0 = _S255;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S301;
        (&_S301)->primal_0 = _S262;
        (&_S301)->differential_0 = _S255;
        s_bwd_prop_dot_0(&_S300, &_S301, 0.0f);
        float3  _S302 = _S301.differential_0 + _S300.differential_0;
        FixedArray<float3 , 4>  _S303;
        _S303[int(0)] = _S255;
        _S303[int(1)] = _S255;
        _S303[int(2)] = _S255;
        _S303[int(3)] = _S255;
        _S303[int(2)] = _S302;
        float3  _S304 = _S279[int(1)] + _S303[int(1)];
        float3  _S305 = _S279[int(2)] + _S303[int(2)];
        float3  _S306 = _S279[int(3)] + _S303[int(3)];
        _S279[int(0)] = _S279[int(0)] + _S303[int(0)];
        _S279[int(1)] = _S304;
        _S279[int(2)] = _S305;
        _S279[int(3)] = _S306;
    }
    if(_S257)
    {
    }
    else
    {
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S307;
        (&_S307)->primal_0 = _S259;
        (&_S307)->differential_0 = _S255;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S308;
        (&_S308)->primal_0 = _S259;
        (&_S308)->differential_0 = _S255;
        s_bwd_prop_dot_0(&_S307, &_S308, 0.0f);
        float3  _S309 = _S308.differential_0 + _S307.differential_0;
        FixedArray<float3 , 4>  _S310;
        _S310[int(0)] = _S255;
        _S310[int(1)] = _S255;
        _S310[int(2)] = _S255;
        _S310[int(3)] = _S255;
        _S310[int(1)] = _S309;
        float3  _S311 = _S279[int(1)] + _S310[int(1)];
        float3  _S312 = _S279[int(2)] + _S310[int(2)];
        float3  _S313 = _S279[int(3)] + _S310[int(3)];
        _S279[int(0)] = _S279[int(0)] + _S310[int(0)];
        _S279[int(1)] = _S311;
        _S279[int(2)] = _S312;
        _S279[int(3)] = _S313;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S314;
    (&_S314)->primal_0 = _S254[int(0)];
    (&_S314)->differential_0 = _S255;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S315;
    (&_S315)->primal_0 = _S254[int(0)];
    (&_S315)->differential_0 = _S255;
    s_bwd_prop_dot_0(&_S314, &_S315, 0.0f);
    float3  _S316 = _S315.differential_0 + _S314.differential_0 + _S268;
    FixedArray<float3 , 4>  _S317;
    _S317[int(0)] = _S255;
    _S317[int(1)] = _S255;
    _S317[int(2)] = _S255;
    _S317[int(3)] = _S255;
    _S317[int(0)] = _S316;
    FixedArray<float3 , 4>  _S318 = {
        _S279[int(0)] + _S317[int(0)], _S279[int(1)] + _S317[int(1)], _S279[int(2)] + _S317[int(2)], _S279[int(3)] + _S317[int(3)]
    };
    dppoints_0->primal_0 = dppoints_0->primal_0;
    dppoints_0->differential_0 = _S318;
    return;
}

inline __device__ void s_bwd_points_to_normal_0(DiffPair_arrayx3Cvectorx3Cfloatx2C3x3Ex2C4x3E_0 * _S319, float3  _S320)
{
    s_bwd_prop_points_to_normal_0(_S319, _S320);
    return;
}

inline __device__ void points_to_normal_vjp(FixedArray<float3 , 4>  points_1, float3  v_normal_0, FixedArray<float3 , 4>  * v_points_0)
{
    FixedArray<float3 , 4>  _S321 = { make_float3 (0.0f), make_float3 (0.0f), make_float3 (0.0f), make_float3 (0.0f) };
    DiffPair_arrayx3Cvectorx3Cfloatx2C3x3Ex2C4x3E_0 dp_points_0;
    (&dp_points_0)->primal_0 = points_1;
    (&dp_points_0)->differential_0 = _S321;
    s_bwd_points_to_normal_0(&dp_points_0, v_normal_0);
    *v_points_0 = (&dp_points_0)->differential_0;
    return;
}

inline __device__ Matrix<float, 2, 2>  transpose_0(Matrix<float, 2, 2>  x_19)
{
    Matrix<float, 2, 2>  result_7;
    int r_1 = int(0);
    for(;;)
    {
        if(r_1 < int(2))
        {
        }
        else
        {
            break;
        }
        int c_1 = int(0);
        for(;;)
        {
            if(c_1 < int(2))
            {
            }
            else
            {
                break;
            }
            *_slang_vector_get_element_ptr(((&result_7)->rows + (r_1)), c_1) = _slang_vector_get_element(x_19.rows[c_1], r_1);
            c_1 = c_1 + int(1);
        }
        r_1 = r_1 + int(1);
    }
    return result_7;
}

inline __device__ float determinant_0(Matrix<float, 2, 2>  m_0)
{
    return m_0.rows[int(0)].x * m_0.rows[int(1)].y - m_0.rows[int(0)].y * m_0.rows[int(1)].x;
}

inline __device__ bool undistort_point_0(float2  uv_0, FixedArray<float, 1>  * dist_coeffs_0, int maxiter_0, float2  * uv_undist_0)
{
    *uv_undist_0 = uv_0;
    return true;
}

inline __device__ float2  DistOpenCV_distort_0(float2  uv_1, FixedArray<float, 4>  * coeffs_0)
{
    float u_2 = uv_1.x;
    float v_0 = uv_1.y;
    float r2_0 = u_2 * u_2 + v_0 * v_0;
    return uv_1 * make_float2 (1.0f + r2_0 * ((*coeffs_0)[int(0)] + r2_0 * (*coeffs_0)[int(1)])) + make_float2 (2.0f * (*coeffs_0)[int(2)] * u_2 * v_0 + (*coeffs_0)[int(3)] * (r2_0 + 2.0f * u_2 * u_2), 2.0f * (*coeffs_0)[int(3)] * u_2 * v_0 + (*coeffs_0)[int(2)] * (r2_0 + 2.0f * v_0 * v_0));
}

struct DiffPair_vectorx3Cfloatx2C2x3E_0
{
    float2  primal_0;
    float2  differential_0;
};

inline __device__ DiffPair_vectorx3Cfloatx2C2x3E_0 s_fwd_DistOpenCV_distort_0(DiffPair_vectorx3Cfloatx2C2x3E_0 * dpuv_0, FixedArray<float, 4>  * coeffs_1)
{
    float u_3 = dpuv_0->primal_0.x;
    float s_diff_u_0 = dpuv_0->differential_0.x;
    float v_1 = dpuv_0->primal_0.y;
    float s_diff_v_0 = dpuv_0->differential_0.y;
    float _S322 = s_diff_u_0 * u_3;
    float _S323 = s_diff_v_0 * v_1;
    float r2_1 = u_3 * u_3 + v_1 * v_1;
    float s_diff_r2_0 = _S322 + _S322 + (_S323 + _S323);
    float _S324 = (*coeffs_1)[int(0)] + r2_1 * (*coeffs_1)[int(1)];
    float radial_0 = 1.0f + r2_1 * _S324;
    float _S325 = 2.0f * (*coeffs_1)[int(2)];
    float _S326 = _S325 * u_3;
    float _S327 = 2.0f * u_3;
    float _S328 = 2.0f * (*coeffs_1)[int(3)];
    float _S329 = _S328 * u_3;
    float _S330 = 2.0f * v_1;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S331 = { dpuv_0->primal_0 * make_float2 (radial_0) + make_float2 (_S326 * v_1 + (*coeffs_1)[int(3)] * (r2_1 + _S327 * u_3), _S329 * v_1 + (*coeffs_1)[int(2)] * (r2_1 + _S330 * v_1)), dpuv_0->differential_0 * make_float2 (radial_0) + make_float2 (s_diff_r2_0 * _S324 + s_diff_r2_0 * (*coeffs_1)[int(1)] * r2_1) * dpuv_0->primal_0 + make_float2 (s_diff_u_0 * _S325 * v_1 + s_diff_v_0 * _S326 + (s_diff_r2_0 + (s_diff_u_0 * 2.0f * u_3 + s_diff_u_0 * _S327)) * (*coeffs_1)[int(3)], s_diff_u_0 * _S328 * v_1 + s_diff_v_0 * _S329 + (s_diff_r2_0 + (s_diff_v_0 * 2.0f * v_1 + s_diff_v_0 * _S330)) * (*coeffs_1)[int(2)]) };
    return _S331;
}

inline __device__ bool undistort_point_1(float2  uv_2, FixedArray<float, 4>  * dist_coeffs_1, int maxiter_1, float2  * uv_undist_1)
{
    int i_5 = int(0);
    float2  q_0 = uv_2;
    for(;;)
    {
        if(i_5 < maxiter_1)
        {
        }
        else
        {
            break;
        }
        float2  _S332 = DistOpenCV_distort_0(q_0, dist_coeffs_1);
        float2  r_2 = _S332 - uv_2;
        float2  _S333 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S334;
        (&_S334)->primal_0 = q_0;
        (&_S334)->differential_0 = _S333;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S335 = s_fwd_DistOpenCV_distort_0(&_S334, dist_coeffs_1);
        float2  _S336 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S337;
        (&_S337)->primal_0 = q_0;
        (&_S337)->differential_0 = _S336;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S338 = s_fwd_DistOpenCV_distort_0(&_S337, dist_coeffs_1);
        Matrix<float, 2, 2>  _S339 = transpose_0(makeMatrix<float, 2, 2> (_S335.differential_0, _S338.differential_0));
        float inv_det_0 = 1.0f / (_S339.rows[int(0)].x * _S339.rows[int(1)].y - _S339.rows[int(0)].y * _S339.rows[int(1)].x);
        float _S340 = r_2.x;
        float _S341 = r_2.y;
        float2  q_1 = q_0 - make_float2 ((_S340 * _S339.rows[int(1)].y - _S341 * _S339.rows[int(0)].y) * inv_det_0, (- _S340 * _S339.rows[int(1)].x + _S341 * _S339.rows[int(0)].x) * inv_det_0);
        i_5 = i_5 + int(1);
        q_0 = q_1;
    }
    *uv_undist_1 = q_0;
    float2  _S342 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S343;
    (&_S343)->primal_0 = q_0;
    (&_S343)->differential_0 = _S342;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S344 = s_fwd_DistOpenCV_distort_0(&_S343, dist_coeffs_1);
    float2  _S345 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S346;
    (&_S346)->primal_0 = q_0;
    (&_S346)->differential_0 = _S345;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S347 = s_fwd_DistOpenCV_distort_0(&_S346, dist_coeffs_1);
    Matrix<float, 2, 2>  _S348 = transpose_0(makeMatrix<float, 2, 2> (_S344.differential_0, _S347.differential_0));
    float _S349 = (F32_min((determinant_0(_S348)), ((F32_min((_S348.rows[int(0)].x), (_S348.rows[int(1)].y))))));
    bool _S350;
    if(_S349 > 0.25f)
    {
        _S350 = _S349 < 4.0f;
    }
    else
    {
        _S350 = false;
    }
    if(_S350)
    {
        float2  _S351 = DistOpenCV_distort_0(q_0, dist_coeffs_1);
        _S350 = (dot_1(q_0, _S351)) >= 0.0f;
    }
    else
    {
        _S350 = false;
    }
    if(_S350)
    {
        float2  _S352 = DistOpenCV_distort_0(*uv_undist_1, dist_coeffs_1);
        _S350 = (length_1(_S352 - uv_2)) < 0.00999999977648258f;
    }
    else
    {
        _S350 = false;
    }
    return _S350;
}

inline __device__ float2  DistThinPrism_distort_0(float2  uv_3, FixedArray<float, 8>  * coeffs_2)
{
    float u_4 = uv_3.x;
    float v_2 = uv_3.y;
    float r2_2 = u_4 * u_4 + v_2 * v_2;
    return uv_3 * make_float2 (1.0f + r2_2 * ((*coeffs_2)[int(0)] + r2_2 * ((*coeffs_2)[int(1)] + r2_2 * ((*coeffs_2)[int(2)] + r2_2 * (*coeffs_2)[int(3)])))) + make_float2 (2.0f * (*coeffs_2)[int(4)] * u_4 * v_2 + (*coeffs_2)[int(5)] * (r2_2 + 2.0f * u_4 * u_4) + (*coeffs_2)[int(6)] * r2_2, 2.0f * (*coeffs_2)[int(5)] * u_4 * v_2 + (*coeffs_2)[int(4)] * (r2_2 + 2.0f * v_2 * v_2) + (*coeffs_2)[int(7)] * r2_2);
}

inline __device__ DiffPair_vectorx3Cfloatx2C2x3E_0 s_fwd_DistThinPrism_distort_0(DiffPair_vectorx3Cfloatx2C2x3E_0 * dpuv_1, FixedArray<float, 8>  * coeffs_3)
{
    float u_5 = dpuv_1->primal_0.x;
    float s_diff_u_1 = dpuv_1->differential_0.x;
    float v_3 = dpuv_1->primal_0.y;
    float s_diff_v_1 = dpuv_1->differential_0.y;
    float _S353 = s_diff_u_1 * u_5;
    float _S354 = s_diff_v_1 * v_3;
    float r2_3 = u_5 * u_5 + v_3 * v_3;
    float s_diff_r2_1 = _S353 + _S353 + (_S354 + _S354);
    float _S355 = (*coeffs_3)[int(2)] + r2_3 * (*coeffs_3)[int(3)];
    float _S356 = (*coeffs_3)[int(1)] + r2_3 * _S355;
    float _S357 = (*coeffs_3)[int(0)] + r2_3 * _S356;
    float radial_1 = 1.0f + r2_3 * _S357;
    float _S358 = 2.0f * (*coeffs_3)[int(4)];
    float _S359 = _S358 * u_5;
    float _S360 = 2.0f * u_5;
    float _S361 = 2.0f * (*coeffs_3)[int(5)];
    float _S362 = _S361 * u_5;
    float _S363 = 2.0f * v_3;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S364 = { dpuv_1->primal_0 * make_float2 (radial_1) + make_float2 (_S359 * v_3 + (*coeffs_3)[int(5)] * (r2_3 + _S360 * u_5) + (*coeffs_3)[int(6)] * r2_3, _S362 * v_3 + (*coeffs_3)[int(4)] * (r2_3 + _S363 * v_3) + (*coeffs_3)[int(7)] * r2_3), dpuv_1->differential_0 * make_float2 (radial_1) + make_float2 (s_diff_r2_1 * _S357 + (s_diff_r2_1 * _S356 + (s_diff_r2_1 * _S355 + s_diff_r2_1 * (*coeffs_3)[int(3)] * r2_3) * r2_3) * r2_3) * dpuv_1->primal_0 + make_float2 (s_diff_u_1 * _S358 * v_3 + s_diff_v_1 * _S359 + (s_diff_r2_1 + (s_diff_u_1 * 2.0f * u_5 + s_diff_u_1 * _S360)) * (*coeffs_3)[int(5)] + s_diff_r2_1 * (*coeffs_3)[int(6)], s_diff_u_1 * _S361 * v_3 + s_diff_v_1 * _S362 + (s_diff_r2_1 + (s_diff_v_1 * 2.0f * v_3 + s_diff_v_1 * _S363)) * (*coeffs_3)[int(4)] + s_diff_r2_1 * (*coeffs_3)[int(7)]) };
    return _S364;
}

inline __device__ bool undistort_point_2(float2  uv_4, FixedArray<float, 8>  * dist_coeffs_2, int maxiter_2, float2  * uv_undist_2)
{
    int i_6 = int(0);
    float2  q_2 = uv_4;
    for(;;)
    {
        if(i_6 < maxiter_2)
        {
        }
        else
        {
            break;
        }
        float2  _S365 = DistThinPrism_distort_0(q_2, dist_coeffs_2);
        float2  r_3 = _S365 - uv_4;
        float2  _S366 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S367;
        (&_S367)->primal_0 = q_2;
        (&_S367)->differential_0 = _S366;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S368 = s_fwd_DistThinPrism_distort_0(&_S367, dist_coeffs_2);
        float2  _S369 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S370;
        (&_S370)->primal_0 = q_2;
        (&_S370)->differential_0 = _S369;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S371 = s_fwd_DistThinPrism_distort_0(&_S370, dist_coeffs_2);
        Matrix<float, 2, 2>  _S372 = transpose_0(makeMatrix<float, 2, 2> (_S368.differential_0, _S371.differential_0));
        float inv_det_1 = 1.0f / (_S372.rows[int(0)].x * _S372.rows[int(1)].y - _S372.rows[int(0)].y * _S372.rows[int(1)].x);
        float _S373 = r_3.x;
        float _S374 = r_3.y;
        float2  q_3 = q_2 - make_float2 ((_S373 * _S372.rows[int(1)].y - _S374 * _S372.rows[int(0)].y) * inv_det_1, (- _S373 * _S372.rows[int(1)].x + _S374 * _S372.rows[int(0)].x) * inv_det_1);
        i_6 = i_6 + int(1);
        q_2 = q_3;
    }
    *uv_undist_2 = q_2;
    float2  _S375 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S376;
    (&_S376)->primal_0 = q_2;
    (&_S376)->differential_0 = _S375;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S377 = s_fwd_DistThinPrism_distort_0(&_S376, dist_coeffs_2);
    float2  _S378 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S379;
    (&_S379)->primal_0 = q_2;
    (&_S379)->differential_0 = _S378;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S380 = s_fwd_DistThinPrism_distort_0(&_S379, dist_coeffs_2);
    Matrix<float, 2, 2>  _S381 = transpose_0(makeMatrix<float, 2, 2> (_S377.differential_0, _S380.differential_0));
    float _S382 = (F32_min((determinant_0(_S381)), ((F32_min((_S381.rows[int(0)].x), (_S381.rows[int(1)].y))))));
    bool _S383;
    if(_S382 > 0.25f)
    {
        _S383 = _S382 < 4.0f;
    }
    else
    {
        _S383 = false;
    }
    if(_S383)
    {
        float2  _S384 = DistThinPrism_distort_0(q_2, dist_coeffs_2);
        _S383 = (dot_1(q_2, _S384)) >= 0.0f;
    }
    else
    {
        _S383 = false;
    }
    if(_S383)
    {
        float2  _S385 = DistThinPrism_distort_0(*uv_undist_2, dist_coeffs_2);
        _S383 = (length_1(_S385 - uv_4)) < 0.00999999977648258f;
    }
    else
    {
        _S383 = false;
    }
    return _S383;
}

inline __device__ float2  DistRational_distort_0(float2  uv_5, FixedArray<float, 8>  * coeffs_4)
{
    float u_6 = uv_5.x;
    float v_4 = uv_5.y;
    float r2_4 = u_6 * u_6 + v_4 * v_4;
    return uv_5 * make_float2 ((1.0f + r2_4 * ((*coeffs_4)[int(0)] + r2_4 * ((*coeffs_4)[int(1)] + r2_4 * (*coeffs_4)[int(2)]))) / (1.0f + r2_4 * ((*coeffs_4)[int(3)] + r2_4 * ((*coeffs_4)[int(4)] + r2_4 * (*coeffs_4)[int(5)])))) + make_float2 (2.0f * (*coeffs_4)[int(6)] * u_6 * v_4 + (*coeffs_4)[int(7)] * (r2_4 + 2.0f * u_6 * u_6), 2.0f * (*coeffs_4)[int(7)] * u_6 * v_4 + (*coeffs_4)[int(6)] * (r2_4 + 2.0f * v_4 * v_4));
}

inline __device__ DiffPair_vectorx3Cfloatx2C2x3E_0 s_fwd_DistRational_distort_0(DiffPair_vectorx3Cfloatx2C2x3E_0 * dpuv_2, FixedArray<float, 8>  * coeffs_5)
{
    float u_7 = dpuv_2->primal_0.x;
    float s_diff_u_2 = dpuv_2->differential_0.x;
    float v_5 = dpuv_2->primal_0.y;
    float s_diff_v_2 = dpuv_2->differential_0.y;
    float _S386 = s_diff_u_2 * u_7;
    float _S387 = s_diff_v_2 * v_5;
    float r2_5 = u_7 * u_7 + v_5 * v_5;
    float s_diff_r2_2 = _S386 + _S386 + (_S387 + _S387);
    float _S388 = (*coeffs_5)[int(1)] + r2_5 * (*coeffs_5)[int(2)];
    float _S389 = (*coeffs_5)[int(0)] + r2_5 * _S388;
    float _S390 = 1.0f + r2_5 * _S389;
    float _S391 = (*coeffs_5)[int(4)] + r2_5 * (*coeffs_5)[int(5)];
    float _S392 = (*coeffs_5)[int(3)] + r2_5 * _S391;
    float _S393 = 1.0f + r2_5 * _S392;
    float radial_2 = _S390 / _S393;
    float _S394 = 2.0f * (*coeffs_5)[int(6)];
    float _S395 = _S394 * u_7;
    float _S396 = 2.0f * u_7;
    float _S397 = 2.0f * (*coeffs_5)[int(7)];
    float _S398 = _S397 * u_7;
    float _S399 = 2.0f * v_5;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S400 = { dpuv_2->primal_0 * make_float2 (radial_2) + make_float2 (_S395 * v_5 + (*coeffs_5)[int(7)] * (r2_5 + _S396 * u_7), _S398 * v_5 + (*coeffs_5)[int(6)] * (r2_5 + _S399 * v_5)), dpuv_2->differential_0 * make_float2 (radial_2) + make_float2 (((s_diff_r2_2 * _S389 + (s_diff_r2_2 * _S388 + s_diff_r2_2 * (*coeffs_5)[int(2)] * r2_5) * r2_5) * _S393 - _S390 * (s_diff_r2_2 * _S392 + (s_diff_r2_2 * _S391 + s_diff_r2_2 * (*coeffs_5)[int(5)] * r2_5) * r2_5)) / (_S393 * _S393)) * dpuv_2->primal_0 + make_float2 (s_diff_u_2 * _S394 * v_5 + s_diff_v_2 * _S395 + (s_diff_r2_2 + (s_diff_u_2 * 2.0f * u_7 + s_diff_u_2 * _S396)) * (*coeffs_5)[int(7)], s_diff_u_2 * _S397 * v_5 + s_diff_v_2 * _S398 + (s_diff_r2_2 + (s_diff_v_2 * 2.0f * v_5 + s_diff_v_2 * _S399)) * (*coeffs_5)[int(6)]) };
    return _S400;
}

inline __device__ bool undistort_point_3(float2  uv_6, FixedArray<float, 8>  * dist_coeffs_3, int maxiter_3, float2  * uv_undist_3)
{
    int i_7 = int(0);
    float2  q_4 = uv_6;
    for(;;)
    {
        if(i_7 < maxiter_3)
        {
        }
        else
        {
            break;
        }
        float2  _S401 = DistRational_distort_0(q_4, dist_coeffs_3);
        float2  r_4 = _S401 - uv_6;
        float2  _S402 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S403;
        (&_S403)->primal_0 = q_4;
        (&_S403)->differential_0 = _S402;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S404 = s_fwd_DistRational_distort_0(&_S403, dist_coeffs_3);
        float2  _S405 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S406;
        (&_S406)->primal_0 = q_4;
        (&_S406)->differential_0 = _S405;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S407 = s_fwd_DistRational_distort_0(&_S406, dist_coeffs_3);
        Matrix<float, 2, 2>  _S408 = transpose_0(makeMatrix<float, 2, 2> (_S404.differential_0, _S407.differential_0));
        float inv_det_2 = 1.0f / (_S408.rows[int(0)].x * _S408.rows[int(1)].y - _S408.rows[int(0)].y * _S408.rows[int(1)].x);
        float _S409 = r_4.x;
        float _S410 = r_4.y;
        float2  q_5 = q_4 - make_float2 ((_S409 * _S408.rows[int(1)].y - _S410 * _S408.rows[int(0)].y) * inv_det_2, (- _S409 * _S408.rows[int(1)].x + _S410 * _S408.rows[int(0)].x) * inv_det_2);
        i_7 = i_7 + int(1);
        q_4 = q_5;
    }
    *uv_undist_3 = q_4;
    float2  _S411 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S412;
    (&_S412)->primal_0 = q_4;
    (&_S412)->differential_0 = _S411;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S413 = s_fwd_DistRational_distort_0(&_S412, dist_coeffs_3);
    float2  _S414 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S415;
    (&_S415)->primal_0 = q_4;
    (&_S415)->differential_0 = _S414;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S416 = s_fwd_DistRational_distort_0(&_S415, dist_coeffs_3);
    Matrix<float, 2, 2>  _S417 = transpose_0(makeMatrix<float, 2, 2> (_S413.differential_0, _S416.differential_0));
    float _S418 = (F32_min((determinant_0(_S417)), ((F32_min((_S417.rows[int(0)].x), (_S417.rows[int(1)].y))))));
    bool _S419;
    if(_S418 > 0.25f)
    {
        _S419 = _S418 < 4.0f;
    }
    else
    {
        _S419 = false;
    }
    if(_S419)
    {
        float2  _S420 = DistRational_distort_0(q_4, dist_coeffs_3);
        _S419 = (dot_1(q_4, _S420)) >= 0.0f;
    }
    else
    {
        _S419 = false;
    }
    if(_S419)
    {
        float2  _S421 = DistRational_distort_0(*uv_undist_3, dist_coeffs_3);
        _S419 = (length_1(_S421 - uv_6)) < 0.00999999977648258f;
    }
    else
    {
        _S419 = false;
    }
    return _S419;
}

inline __device__ float3  normalize_0(float3  x_20)
{
    return x_20 / make_float3 (length_0(x_20));
}

inline __device__ float3  unproject_raydir_0(float2  uv_7, int camera_model_0, bool is_ray_depth_0)
{
    float3  raydir_0;
    bool is_unit_0;
    if(camera_model_0 == int(1))
    {
        float theta_0 = length_1(uv_7);
        float3  _S422 = make_float3 ((uv_7 / make_float2 ((F32_max((theta_0), (1.00000001168609742e-07f)))) * make_float2 ((F32_sin((theta_0))))).x, (uv_7 / make_float2 ((F32_max((theta_0), (1.00000001168609742e-07f)))) * make_float2 ((F32_sin((theta_0))))).y, (F32_cos((theta_0))));
        is_unit_0 = true;
        raydir_0 = _S422;
    }
    else
    {
        bool _S423 = camera_model_0 == int(2);
        if(_S423)
        {
            float r_5 = length_1(uv_7);
            raydir_0 = make_float3 ((uv_7 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_5 * r_5)))))))).x, (uv_7 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_5 * r_5)))))))).y, 1.0f - 0.5f * r_5 * r_5);
        }
        else
        {
            raydir_0 = make_float3 (uv_7.x, uv_7.y, 1.0f);
        }
        is_unit_0 = _S423;
    }
    if(is_ray_depth_0)
    {
        if(is_unit_0)
        {
        }
        else
        {
            raydir_0 = normalize_0(raydir_0);
        }
    }
    else
    {
        raydir_0 = raydir_0 / make_float3 (raydir_0.z);
    }
    return raydir_0;
}

inline __device__ float3  generate_ray_d2n_none(float2  pix_pos_0, float4  intrins_0, FixedArray<float, 1>  dist_coeffs_4, int camera_model_1, bool is_ray_depth_1)
{
    float3  _S424;
    for(;;)
    {
        float2  uv_8 = (pix_pos_0 - float2 {intrins_0.z, intrins_0.w}) / float2 {intrins_0.x, intrins_0.y};
        FixedArray<float, 1>  _S425 = dist_coeffs_4;
        float2  uv_u_0;
        bool _S426 = undistort_point_0(uv_8, &_S425, int(12), &uv_u_0);
        if(!_S426)
        {
            int3  _S427 = make_int3 (int(0));
            float3  _S428 = make_float3 ((float)_S427.x, (float)_S427.y, (float)_S427.z);
            _S424 = _S428;
            break;
        }
        _S424 = unproject_raydir_0(uv_u_0, camera_model_1, is_ray_depth_1);
        break;
    }
    return _S424;
}

inline __device__ float3  depth_to_point_none(float2  pix_pos_1, float4  intrins_1, FixedArray<float, 1>  dist_coeffs_5, int camera_model_2, bool is_ray_depth_2, float depth_2)
{
    float3  _S429;
    for(;;)
    {
        float2  uv_9 = (pix_pos_1 - float2 {intrins_1.z, intrins_1.w}) / float2 {intrins_1.x, intrins_1.y};
        FixedArray<float, 1>  _S430 = dist_coeffs_5;
        float2  uv_u_1;
        bool _S431 = undistort_point_0(uv_9, &_S430, int(12), &uv_u_1);
        if(!_S431)
        {
            _S429 = make_float3 (0.0f);
            break;
        }
        _S429 = make_float3 (depth_2) * unproject_raydir_0(uv_u_1, camera_model_2, is_ray_depth_2);
        break;
    }
    return _S429;
}

struct s_bwd_prop_depth_to_point_Intermediates_0
{
    float2  _S432;
    bool _S433;
};

inline __device__ float s_primal_ctx_sin_0(float _S434)
{
    return (F32_sin((_S434)));
}

inline __device__ float s_primal_ctx_cos_0(float _S435)
{
    return (F32_cos((_S435)));
}

inline __device__ float s_primal_ctx_sqrt_0(float _S436)
{
    return (F32_sqrt((_S436)));
}

inline __device__ float3  s_primal_ctx_unproject_raydir_0(float2  dpuv_3, int camera_model_3, bool is_ray_depth_3)
{
    float3  raydir_1;
    bool is_unit_1;
    if(camera_model_3 == int(1))
    {
        float _S437 = length_1(dpuv_3);
        float3  _S438 = make_float3 ((dpuv_3 / make_float2 ((F32_max((_S437), (1.00000001168609742e-07f)))) * make_float2 (s_primal_ctx_sin_0(_S437))).x, (dpuv_3 / make_float2 ((F32_max((_S437), (1.00000001168609742e-07f)))) * make_float2 (s_primal_ctx_sin_0(_S437))).y, s_primal_ctx_cos_0(_S437));
        is_unit_1 = true;
        raydir_1 = _S438;
    }
    else
    {
        bool _S439 = camera_model_3 == int(2);
        if(_S439)
        {
            float _S440 = length_1(dpuv_3);
            raydir_1 = make_float3 ((dpuv_3 * make_float2 (s_primal_ctx_sqrt_0((F32_max((0.0f), (1.0f - 0.25f * _S440 * _S440)))))).x, (dpuv_3 * make_float2 (s_primal_ctx_sqrt_0((F32_max((0.0f), (1.0f - 0.25f * _S440 * _S440)))))).y, 1.0f - 0.5f * _S440 * _S440);
        }
        else
        {
            raydir_1 = make_float3 (dpuv_3.x, dpuv_3.y, 1.0f);
        }
        is_unit_1 = _S439;
    }
    if(is_ray_depth_3)
    {
        if(is_unit_1)
        {
        }
        else
        {
            raydir_1 = normalize_0(raydir_1);
        }
    }
    else
    {
        raydir_1 = raydir_1 / make_float3 (raydir_1.z);
    }
    return raydir_1;
}

inline __device__ float depth_to_point_vjp_none(float2  pix_pos_2, float4  intrins_2, FixedArray<float, 1>  dist_coeffs_6, int camera_model_4, bool is_ray_depth_4, float depth_3, float3  v_point_0)
{
    float2  _S441 = make_float2 (0.0f);
    s_bwd_prop_depth_to_point_Intermediates_0 _S442;
    (&_S442)->_S432 = _S441;
    (&_S442)->_S433 = false;
    float2  uv_10 = (pix_pos_2 - float2 {intrins_2.z, intrins_2.w}) / float2 {intrins_2.x, intrins_2.y};
    float2  _S443 = _S441;
    FixedArray<float, 1>  _S444 = dist_coeffs_6;
    bool _S445 = undistort_point_0(uv_10, &_S444, int(12), &_S443);
    (&_S442)->_S432 = _S443;
    (&_S442)->_S433 = _S445;
    s_bwd_prop_depth_to_point_Intermediates_0 _S446 = _S442;
    float3  _S447 = make_float3 (0.0f);
    bool _S448 = !!_S442._S433;
    float3  _S449;
    if(_S448)
    {
        _S449 = s_primal_ctx_unproject_raydir_0(_S446._S432, camera_model_4, is_ray_depth_4);
    }
    else
    {
        _S449 = _S447;
    }
    if(_S448)
    {
        _S449 = _S449 * v_point_0;
    }
    else
    {
        _S449 = _S447;
    }
    return _S449.x + _S449.y + _S449.z;
}

inline __device__ float3  depth_to_normal_none(float2  pix_center_0, float4  intrins_3, FixedArray<float, 1>  dist_coeffs_7, int camera_model_5, bool is_ray_depth_5, float4  depths_0)
{
    float3  normal_2;
    for(;;)
    {
        bool _S450;
        if((depths_0.x) == 0.0f)
        {
            _S450 = true;
        }
        else
        {
            _S450 = (depths_0.y) == 0.0f;
        }
        if(_S450)
        {
            _S450 = true;
        }
        else
        {
            _S450 = (depths_0.z) == 0.0f;
        }
        if(_S450)
        {
            _S450 = true;
        }
        else
        {
            _S450 = (depths_0.w) == 0.0f;
        }
        if(_S450)
        {
            normal_2 = make_float3 (0.0f);
            break;
        }
        float3  * _S451;
        float3  * _S452;
        float3  * _S453;
        float3  * _S454;
        int _S455;
        FixedArray<float3 , 4>  points_2;
        for(;;)
        {
            float2  _S456 = float2 {intrins_3.z, intrins_3.w};
            float2  _S457 = float2 {intrins_3.x, intrins_3.y};
            float2  uv_11 = (pix_center_0 + make_float2 (-1.0f, -0.0f) - _S456) / _S457;
            FixedArray<float, 1>  _S458 = dist_coeffs_7;
            float2  uv_u_2;
            bool _S459 = undistort_point_0(uv_11, &_S458, int(12), &uv_u_2);
            if(!_S459)
            {
                float3  _S460 = make_float3 (0.0f);
                _S455 = int(0);
                _S454 = nullptr;
                _S453 = nullptr;
                _S452 = nullptr;
                _S451 = nullptr;
                normal_2 = _S460;
                break;
            }
            points_2[int(0)] = make_float3 (depths_0.x) * unproject_raydir_0(uv_u_2, camera_model_5, is_ray_depth_5);
            for(;;)
            {
                float2  uv_12 = (pix_center_0 + make_float2 (1.0f, -0.0f) - _S456) / _S457;
                FixedArray<float, 1>  _S461 = dist_coeffs_7;
                float2  uv_u_3;
                bool _S462 = undistort_point_0(uv_12, &_S461, int(12), &uv_u_3);
                if(!_S462)
                {
                    float3  _S463 = make_float3 (0.0f);
                    _S455 = int(0);
                    _S454 = nullptr;
                    normal_2 = _S463;
                    break;
                }
                points_2[int(1)] = make_float3 (depths_0.y) * unproject_raydir_0(uv_u_3, camera_model_5, is_ray_depth_5);
                _S455 = int(2);
                _S454 = &points_2[int(1)];
                break;
            }
            if(_S455 != int(2))
            {
                _S453 = &points_2[int(0)];
                _S452 = nullptr;
                _S451 = nullptr;
                break;
            }
            float2  uv_13 = (pix_center_0 + make_float2 (0.0f, -1.0f) - _S456) / _S457;
            FixedArray<float, 1>  _S464 = dist_coeffs_7;
            float2  uv_u_4;
            bool _S465 = undistort_point_0(uv_13, &_S464, int(12), &uv_u_4);
            if(!_S465)
            {
                float3  _S466 = make_float3 (0.0f);
                _S455 = int(0);
                _S453 = &points_2[int(0)];
                _S452 = nullptr;
                _S451 = nullptr;
                normal_2 = _S466;
                break;
            }
            points_2[int(2)] = make_float3 (depths_0.z) * unproject_raydir_0(uv_u_4, camera_model_5, is_ray_depth_5);
            for(;;)
            {
                float2  uv_14 = (pix_center_0 + make_float2 (0.0f, 1.0f) - _S456) / _S457;
                FixedArray<float, 1>  _S467 = dist_coeffs_7;
                float2  uv_u_5;
                bool _S468 = undistort_point_0(uv_14, &_S467, int(12), &uv_u_5);
                if(!_S468)
                {
                    float3  _S469 = make_float3 (0.0f);
                    _S455 = int(0);
                    _S453 = nullptr;
                    normal_2 = _S469;
                    break;
                }
                points_2[int(3)] = make_float3 (depths_0.w) * unproject_raydir_0(uv_u_5, camera_model_5, is_ray_depth_5);
                _S455 = int(2);
                _S453 = &points_2[int(3)];
                break;
            }
            if(_S455 != int(2))
            {
                float3  * _S470 = _S453;
                _S453 = &points_2[int(0)];
                _S452 = _S470;
                _S451 = &points_2[int(2)];
                break;
            }
            float3  * _S471 = _S453;
            _S455 = int(1);
            _S453 = &points_2[int(0)];
            _S452 = _S471;
            _S451 = &points_2[int(2)];
            break;
        }
        if(_S455 != int(1))
        {
            break;
        }
        float3  normal_3 = cross_0(*_S454 - *_S453, - (*_S452 - *_S451));
        if((dot_0(normal_3, normal_3)) != 0.0f)
        {
            normal_2 = normal_3 / make_float3 (length_0(normal_3));
        }
        else
        {
            normal_2 = normal_3;
        }
        break;
    }
    return normal_2;
}

struct s_bwd_prop_depth_to_normal_Intermediates_0
{
    float2  _S472;
    bool _S473;
    float2  _S474;
    bool _S475;
    float2  _S476;
    bool _S477;
    float2  _S478;
    bool _S479;
};

inline __device__ void depth_to_normal_vjp_none(float2  pix_center_1, float4  intrins_4, FixedArray<float, 1>  dist_coeffs_8, int camera_model_6, bool is_ray_depth_6, float4  depths_1, float3  v_normal_1, float4  * v_depths_0)
{
    float2  _S480 = make_float2 (0.0f);
    s_bwd_prop_depth_to_normal_Intermediates_0 _S481;
    (&_S481)->_S472 = _S480;
    (&_S481)->_S473 = false;
    (&_S481)->_S474 = _S480;
    (&_S481)->_S475 = false;
    (&_S481)->_S476 = _S480;
    (&_S481)->_S477 = false;
    (&_S481)->_S478 = _S480;
    (&_S481)->_S479 = false;
    (&_S481)->_S472 = _S480;
    (&_S481)->_S473 = false;
    (&_S481)->_S474 = _S480;
    (&_S481)->_S475 = false;
    (&_S481)->_S476 = _S480;
    (&_S481)->_S477 = false;
    (&_S481)->_S478 = _S480;
    (&_S481)->_S479 = false;
    bool _S482 = (depths_1.x) == 0.0f;
    bool _runFlag_3;
    if(_S482)
    {
        _runFlag_3 = true;
    }
    else
    {
        _runFlag_3 = (depths_1.y) == 0.0f;
    }
    if(_runFlag_3)
    {
        _runFlag_3 = true;
    }
    else
    {
        _runFlag_3 = (depths_1.z) == 0.0f;
    }
    if(_runFlag_3)
    {
        _runFlag_3 = true;
    }
    else
    {
        _runFlag_3 = (depths_1.w) == 0.0f;
    }
    int _S483;
    if(!_runFlag_3)
    {
        float2  _S484 = float2 {intrins_4.z, intrins_4.w};
        float2  _S485 = float2 {intrins_4.x, intrins_4.y};
        float2  uv_15 = (pix_center_1 + make_float2 (-1.0f, -0.0f) - _S484) / _S485;
        float2  _S486 = _S480;
        FixedArray<float, 1>  _S487 = dist_coeffs_8;
        bool _S488 = undistort_point_0(uv_15, &_S487, int(12), &_S486);
        (&_S481)->_S472 = _S486;
        (&_S481)->_S473 = _S488;
        bool _S489 = !!_S488;
        if(_S489)
        {
            float2  uv_16 = (pix_center_1 + make_float2 (1.0f, -0.0f) - _S484) / _S485;
            float2  _S490 = _S480;
            FixedArray<float, 1>  _S491 = dist_coeffs_8;
            bool _S492 = undistort_point_0(uv_16, &_S491, int(12), &_S490);
            (&_S481)->_S474 = _S490;
            (&_S481)->_S475 = _S492;
            if(!!_S492)
            {
                _S483 = int(2);
            }
            else
            {
                _S483 = int(0);
            }
            if(_S483 != int(2))
            {
                _runFlag_3 = false;
            }
            else
            {
                _runFlag_3 = _S489;
            }
            if(_runFlag_3)
            {
                float2  uv_17 = (pix_center_1 + make_float2 (0.0f, -1.0f) - _S484) / _S485;
                float2  _S493 = _S480;
                FixedArray<float, 1>  _S494 = dist_coeffs_8;
                bool _S495 = undistort_point_0(uv_17, &_S494, int(12), &_S493);
                (&_S481)->_S476 = _S493;
                (&_S481)->_S477 = _S495;
                if(!_S495)
                {
                    _runFlag_3 = false;
                }
                if(_runFlag_3)
                {
                    float2  uv_18 = (pix_center_1 + make_float2 (0.0f, 1.0f) - _S484) / _S485;
                    float2  _S496 = _S480;
                    FixedArray<float, 1>  _S497 = dist_coeffs_8;
                    bool _S498 = undistort_point_0(uv_18, &_S497, int(12), &_S496);
                    (&_S481)->_S478 = _S496;
                    (&_S481)->_S479 = _S498;
                }
            }
        }
    }
    s_bwd_prop_depth_to_normal_Intermediates_0 _S499 = _S481;
    float3  _S500 = make_float3 (0.0f);
    if(_S482)
    {
        _runFlag_3 = true;
    }
    else
    {
        _runFlag_3 = (depths_1.y) == 0.0f;
    }
    if(_runFlag_3)
    {
        _runFlag_3 = true;
    }
    else
    {
        _runFlag_3 = (depths_1.z) == 0.0f;
    }
    if(_runFlag_3)
    {
        _runFlag_3 = true;
    }
    else
    {
        _runFlag_3 = (depths_1.w) == 0.0f;
    }
    bool _S501 = !_runFlag_3;
    bool _runFlag_4;
    bool _runFlag_5;
    bool _S502;
    bool _runFlag_6;
    bool _S503;
    bool _S504;
    FixedArray<float3 , 4>  points_3;
    float3  _S505;
    float3  _S506;
    float3  _S507;
    float3  _S508;
    float3  _S509;
    float3  _S510;
    float3  _S511;
    float3  _S512;
    float3  _S513;
    if(_S501)
    {
        bool _S514 = !!_S499._S473;
        if(_S514)
        {
            float3  _S515 = s_primal_ctx_unproject_raydir_0(_S499._S472, camera_model_6, is_ray_depth_6);
            float3  _S516 = make_float3 (depths_1.x) * _S515;
            bool _S517 = !!_S499._S475;
            if(_S517)
            {
                float3  _S518 = s_primal_ctx_unproject_raydir_0(_S499._S474, camera_model_6, is_ray_depth_6);
                float3  _S519 = make_float3 (depths_1.y) * _S518;
                _S483 = int(2);
                points_3[int(0)] = _S516;
                points_3[int(1)] = _S519;
                points_3[int(2)] = _S500;
                points_3[int(3)] = _S500;
                _S505 = _S518;
            }
            else
            {
                _S483 = int(0);
                points_3[int(0)] = _S516;
                points_3[int(1)] = _S500;
                points_3[int(2)] = _S500;
                points_3[int(3)] = _S500;
                _S505 = _S500;
            }
            if(_S483 != int(2))
            {
                _runFlag_3 = false;
            }
            else
            {
                _runFlag_3 = _S514;
                _S483 = int(0);
            }
            if(_runFlag_3)
            {
                if(!_S499._S477)
                {
                    _runFlag_4 = false;
                    _S483 = int(0);
                }
                else
                {
                    _runFlag_4 = _runFlag_3;
                }
                if(_runFlag_4)
                {
                    float3  _S520 = s_primal_ctx_unproject_raydir_0(_S499._S476, camera_model_6, is_ray_depth_6);
                    points_3[int(2)] = make_float3 (depths_1.z) * _S520;
                    bool _S521 = !!_S499._S479;
                    int _S522;
                    if(_S521)
                    {
                        float3  _S523 = s_primal_ctx_unproject_raydir_0(_S499._S478, camera_model_6, is_ray_depth_6);
                        points_3[int(3)] = make_float3 (depths_1.w) * _S523;
                        _S522 = int(2);
                        _S506 = _S523;
                    }
                    else
                    {
                        _S522 = int(0);
                        _S506 = _S500;
                    }
                    if(_S522 != int(2))
                    {
                        _runFlag_5 = false;
                        _S483 = _S522;
                    }
                    else
                    {
                        _runFlag_5 = _runFlag_4;
                    }
                    if(_runFlag_5)
                    {
                        _S483 = int(1);
                    }
                    _runFlag_5 = _S521;
                    _S507 = _S520;
                }
                else
                {
                    _runFlag_5 = false;
                    _S506 = _S500;
                    _S507 = _S500;
                }
            }
            else
            {
                _runFlag_4 = false;
                _runFlag_5 = false;
                _S506 = _S500;
                _S507 = _S500;
            }
            float3  _S524 = _S505;
            _S505 = _S506;
            _S506 = _S507;
            _S502 = _S517;
            _S507 = _S524;
            _S508 = _S515;
        }
        else
        {
            _S483 = int(0);
            points_3[int(0)] = _S500;
            points_3[int(1)] = _S500;
            points_3[int(2)] = _S500;
            points_3[int(3)] = _S500;
            _runFlag_3 = false;
            _runFlag_4 = false;
            _runFlag_5 = false;
            _S505 = _S500;
            _S506 = _S500;
            _S502 = false;
            _S507 = _S500;
            _S508 = _S500;
        }
        if(_S483 != int(1))
        {
            _runFlag_6 = false;
        }
        else
        {
            _runFlag_6 = _S501;
        }
        if(_runFlag_6)
        {
            float3  dx_1 = points_3[int(1)] - points_3[int(0)];
            float3  _S525 = - (points_3[int(3)] - points_3[int(2)]);
            float3  _S526 = s_primal_ctx_cross_0(dx_1, _S525);
            bool _S527 = (s_primal_ctx_dot_0(_S526, _S526)) != 0.0f;
            if(_S527)
            {
                float _S528 = length_0(_S526);
                float3  _S529 = make_float3 (_S528);
                _S509 = make_float3 (_S528 * _S528);
                _S510 = _S529;
            }
            else
            {
                _S509 = _S500;
                _S510 = _S500;
            }
            float3  _S530 = _S510;
            _S503 = _S527;
            _S510 = _S526;
            _S511 = _S530;
            _S512 = dx_1;
            _S513 = _S525;
        }
        else
        {
            _S503 = false;
            _S509 = _S500;
            _S510 = _S500;
            _S511 = _S500;
            _S512 = _S500;
            _S513 = _S500;
        }
        bool _S531 = _runFlag_3;
        bool _S532 = _runFlag_4;
        bool _S533 = _runFlag_5;
        float3  _S534 = _S505;
        float3  _S535 = _S506;
        bool _S536 = _S502;
        float3  _S537 = _S507;
        float3  _S538 = _S508;
        _runFlag_3 = _runFlag_6;
        _runFlag_4 = _S503;
        _S505 = _S509;
        _S506 = _S510;
        _S507 = _S511;
        _S508 = _S512;
        _S509 = _S513;
        _runFlag_5 = _S514;
        _S502 = _S531;
        _runFlag_6 = _S532;
        _S503 = _S533;
        _S510 = _S534;
        _S511 = _S535;
        _S504 = _S536;
        _S512 = _S537;
        _S513 = _S538;
    }
    else
    {
        _runFlag_3 = false;
        _runFlag_4 = false;
        _S505 = _S500;
        _S506 = _S500;
        _S507 = _S500;
        _S508 = _S500;
        _S509 = _S500;
        _runFlag_5 = false;
        _S502 = false;
        _runFlag_6 = false;
        _S503 = false;
        _S510 = _S500;
        _S511 = _S500;
        _S504 = false;
        _S512 = _S500;
        _S513 = _S500;
    }
    float4  _S539 = make_float4 (0.0f);
    float4  _S540;
    if(_S501)
    {
        if(_runFlag_3)
        {
            if(_runFlag_4)
            {
                float3  _S541 = v_normal_1 / _S505;
                float3  _S542 = _S506 * - _S541;
                float3  _S543 = _S507 * _S541;
                float _S544 = _S542.x + _S542.y + _S542.z;
                DiffPair_vectorx3Cfloatx2C3x3E_0 _S545;
                (&_S545)->primal_0 = _S506;
                (&_S545)->differential_0 = _S500;
                s_bwd_length_impl_0(&_S545, _S544);
                _S505 = _S543 + _S545.differential_0;
            }
            else
            {
                _S505 = v_normal_1;
            }
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S546;
            (&_S546)->primal_0 = _S506;
            (&_S546)->differential_0 = _S500;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S547;
            (&_S547)->primal_0 = _S506;
            (&_S547)->differential_0 = _S500;
            s_bwd_prop_dot_0(&_S546, &_S547, 0.0f);
            float3  _S548 = _S547.differential_0 + _S546.differential_0 + _S505;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S549;
            (&_S549)->primal_0 = _S508;
            (&_S549)->differential_0 = _S500;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S550;
            (&_S550)->primal_0 = _S509;
            (&_S550)->differential_0 = _S500;
            s_bwd_prop_cross_0(&_S549, &_S550, _S548);
            float3  s_diff_dy_T_1 = - _S550.differential_0;
            float3  _S551 = - s_diff_dy_T_1;
            float3  _S552 = - _S549.differential_0;
            FixedArray<float3 , 4>  _S553;
            _S553[int(0)] = _S500;
            _S553[int(1)] = _S500;
            _S553[int(2)] = _S500;
            _S553[int(3)] = _S500;
            _S553[int(2)] = _S551;
            _S553[int(3)] = s_diff_dy_T_1;
            _S553[int(0)] = _S552;
            _S553[int(1)] = _S549.differential_0;
            points_3[int(0)] = _S553[int(0)];
            points_3[int(1)] = _S553[int(1)];
            points_3[int(2)] = _S553[int(2)];
            points_3[int(3)] = _S553[int(3)];
        }
        else
        {
            points_3[int(0)] = _S500;
            points_3[int(1)] = _S500;
            points_3[int(2)] = _S500;
            points_3[int(3)] = _S500;
        }
        if(_runFlag_5)
        {
            if(_S502)
            {
                if(_runFlag_6)
                {
                    FixedArray<float3 , 4>  _S554 = points_3;
                    FixedArray<float3 , 4>  _S555 = points_3;
                    FixedArray<float3 , 4>  _S556 = points_3;
                    FixedArray<float3 , 4>  _S557 = points_3;
                    if(_S503)
                    {
                        float3  _S558 = _S510 * _S557[int(3)];
                        float _S559 = _S558.x + _S558.y + _S558.z;
                        float4  _S560 = _S539;
                        *&((&_S560)->w) = _S559;
                        points_3[int(0)] = _S554[int(0)];
                        points_3[int(1)] = _S555[int(1)];
                        points_3[int(2)] = _S556[int(2)];
                        points_3[int(3)] = _S500;
                        _S540 = _S560;
                    }
                    else
                    {
                        points_3[int(0)] = _S554[int(0)];
                        points_3[int(1)] = _S555[int(1)];
                        points_3[int(2)] = _S556[int(2)];
                        points_3[int(3)] = _S557[int(3)];
                        _S540 = _S539;
                    }
                    float3  _S561 = _S511 * points_3[int(2)];
                    float _S562 = _S561.x + _S561.y + _S561.z;
                    FixedArray<float3 , 4>  _S563 = points_3;
                    FixedArray<float3 , 4>  _S564 = points_3;
                    float4  _S565 = _S539;
                    *&((&_S565)->z) = _S562;
                    float4  _S566 = _S540 + _S565;
                    points_3[int(0)] = points_3[int(0)];
                    points_3[int(1)] = _S563[int(1)];
                    points_3[int(2)] = _S500;
                    points_3[int(3)] = _S564[int(3)];
                    _S540 = _S566;
                }
                else
                {
                    FixedArray<float3 , 4>  _S567 = points_3;
                    FixedArray<float3 , 4>  _S568 = points_3;
                    FixedArray<float3 , 4>  _S569 = points_3;
                    points_3[int(0)] = points_3[int(0)];
                    points_3[int(1)] = _S567[int(1)];
                    points_3[int(2)] = _S568[int(2)];
                    points_3[int(3)] = _S569[int(3)];
                    _S540 = _S539;
                }
            }
            else
            {
                FixedArray<float3 , 4>  _S570 = points_3;
                FixedArray<float3 , 4>  _S571 = points_3;
                FixedArray<float3 , 4>  _S572 = points_3;
                points_3[int(0)] = points_3[int(0)];
                points_3[int(1)] = _S570[int(1)];
                points_3[int(2)] = _S571[int(2)];
                points_3[int(3)] = _S572[int(3)];
                _S540 = _S539;
            }
            if(_S504)
            {
                FixedArray<float3 , 4>  _S573 = points_3;
                float3  _S574 = _S512 * points_3[int(1)];
                float _S575 = _S574.x + _S574.y + _S574.z;
                float4  _S576 = _S539;
                *&((&_S576)->y) = _S575;
                float4  _S577 = _S540 + _S576;
                points_3[int(0)] = _S500;
                points_3[int(1)] = _S500;
                points_3[int(2)] = _S500;
                points_3[int(3)] = _S500;
                _S505 = _S573[int(0)];
                _S540 = _S577;
            }
            else
            {
                FixedArray<float3 , 4>  _S578 = points_3;
                FixedArray<float3 , 4>  _S579 = points_3;
                FixedArray<float3 , 4>  _S580 = points_3;
                points_3[int(0)] = points_3[int(0)];
                points_3[int(1)] = _S578[int(1)];
                points_3[int(2)] = _S579[int(2)];
                points_3[int(3)] = _S580[int(3)];
                _S505 = _S500;
            }
            float3  _S581 = _S513 * (points_3[int(0)] + _S505);
            float _S582 = _S581.x + _S581.y + _S581.z;
            float4  _S583 = _S539;
            *&((&_S583)->x) = _S582;
            _S540 = _S540 + _S583;
        }
        else
        {
            _S540 = _S539;
        }
    }
    else
    {
        _S540 = _S539;
    }
    *v_depths_0 = _S540;
    return;
}

inline __device__ float ray_depth_to_linear_depth_factor_none(float2  pix_center_2, float4  intrins_5, FixedArray<float, 1>  dist_coeffs_9, int camera_model_7)
{
    float _S584;
    for(;;)
    {
        float2  uv_19 = (pix_center_2 - float2 {intrins_5.z, intrins_5.w}) / float2 {intrins_5.x, intrins_5.y};
        FixedArray<float, 1>  _S585 = dist_coeffs_9;
        float2  uv_u_6;
        bool _S586 = undistort_point_0(uv_19, &_S585, int(12), &uv_u_6);
        if(!_S586)
        {
            _S584 = 0.0f;
            break;
        }
        float3  raydir_2 = unproject_raydir_0(uv_u_6, camera_model_7, false);
        _S584 = float((F32_sign((raydir_2.z)))) / length_0(raydir_2);
        break;
    }
    return _S584;
}

inline __device__ float depth_normal_loss_none(float2  pix_center_3, float4  intrins_6, FixedArray<float, 1>  dist_coeffs_10, int camera_model_8, bool is_ray_depth_7, float4  depths_2, float3  gt_normal_0)
{
    float _S587;
    for(;;)
    {
        float3  _S588;
        float3  * _S589;
        float3  * _S590;
        float3  * _S591;
        float3  * _S592;
        int _S593;
        FixedArray<float3 , 5>  points_4;
        for(;;)
        {
            float2  _S594 = float2 {intrins_6.z, intrins_6.w};
            float2  _S595 = float2 {intrins_6.x, intrins_6.y};
            float2  uv_20 = (pix_center_3 + make_float2 (-1.0f, -0.0f) - _S594) / _S595;
            FixedArray<float, 1>  _S596 = dist_coeffs_10;
            float2  uv_u_7;
            bool _S597 = undistort_point_0(uv_20, &_S596, int(12), &uv_u_7);
            float3  _S598 = make_float3 (0.0f);
            if(!_S597)
            {
                _S593 = int(0);
                _S592 = nullptr;
                _S591 = nullptr;
                _S590 = nullptr;
                _S589 = nullptr;
                _S588 = _S598;
                break;
            }
            float3  raydir_3 = unproject_raydir_0(uv_u_7, camera_model_8, is_ray_depth_7);
            points_4[int(0)] = make_float3 (depths_2.x) * raydir_3;
            float2  uv_21 = (pix_center_3 + make_float2 (1.0f, -0.0f) - _S594) / _S595;
            FixedArray<float, 1>  _S599 = dist_coeffs_10;
            float2  uv_u_8;
            bool _S600 = undistort_point_0(uv_21, &_S599, int(12), &uv_u_8);
            if(!_S600)
            {
                _S593 = int(0);
                _S592 = nullptr;
                _S591 = &points_4[int(0)];
                _S590 = nullptr;
                _S589 = nullptr;
                _S588 = _S598;
                break;
            }
            float3  raydir_4 = unproject_raydir_0(uv_u_8, camera_model_8, is_ray_depth_7);
            points_4[int(1)] = make_float3 (depths_2.y) * raydir_4;
            float2  uv_22 = (pix_center_3 + make_float2 (0.0f, -1.0f) - _S594) / _S595;
            FixedArray<float, 1>  _S601 = dist_coeffs_10;
            float2  uv_u_9;
            bool _S602 = undistort_point_0(uv_22, &_S601, int(12), &uv_u_9);
            if(!_S602)
            {
                _S593 = int(0);
                _S592 = &points_4[int(1)];
                _S591 = &points_4[int(0)];
                _S590 = nullptr;
                _S589 = nullptr;
                _S588 = _S598;
                break;
            }
            float3  raydir_5 = unproject_raydir_0(uv_u_9, camera_model_8, is_ray_depth_7);
            points_4[int(2)] = make_float3 (depths_2.z) * raydir_5;
            float2  uv_23 = (pix_center_3 + make_float2 (0.0f, 1.0f) - _S594) / _S595;
            FixedArray<float, 1>  _S603 = dist_coeffs_10;
            float2  uv_u_10;
            bool _S604 = undistort_point_0(uv_23, &_S603, int(12), &uv_u_10);
            if(!_S604)
            {
                _S593 = int(0);
                _S592 = &points_4[int(1)];
                _S591 = &points_4[int(0)];
                _S590 = nullptr;
                _S589 = &points_4[int(2)];
                _S588 = _S598;
                break;
            }
            float3  raydir_6 = unproject_raydir_0(uv_u_10, camera_model_8, is_ray_depth_7);
            points_4[int(3)] = make_float3 (depths_2.w) * raydir_6;
            float2  uv_24 = (pix_center_3 + make_float2 (0.0f) * make_float2 (0.0f, 3.0f) - _S594) / _S595;
            FixedArray<float, 1>  _S605 = dist_coeffs_10;
            float2  uv_u_11;
            bool _S606 = undistort_point_0(uv_24, &_S605, int(12), &uv_u_11);
            if(!_S606)
            {
                _S593 = int(0);
                _S592 = &points_4[int(1)];
                _S591 = &points_4[int(0)];
                _S590 = &points_4[int(3)];
                _S589 = &points_4[int(2)];
                _S588 = _S598;
                break;
            }
            float3  raydir_7 = unproject_raydir_0(uv_u_11, camera_model_8, is_ray_depth_7);
            _S593 = int(1);
            _S592 = &points_4[int(1)];
            _S591 = &points_4[int(0)];
            _S590 = &points_4[int(3)];
            _S589 = &points_4[int(2)];
            _S588 = raydir_7;
            break;
        }
        if(_S593 != int(1))
        {
            _S587 = 0.0f;
            break;
        }
        float3  normal_4 = cross_0(*_S592 - *_S591, - (*_S590 - *_S589));
        float3  normal_5;
        if((dot_0(normal_4, normal_4)) != 0.0f)
        {
            normal_5 = normalize_0(normal_4);
        }
        else
        {
            normal_5 = normal_4;
        }
        float3  _S607;
        if((dot_0(gt_normal_0, gt_normal_0)) != 0.0f)
        {
            _S607 = normalize_0(gt_normal_0);
        }
        else
        {
            _S607 = gt_normal_0;
        }
        _S587 = (1.0f - dot_0(normal_5, _S607) + 0.00100000004749745f) / ((F32_max((dot_0(normal_5, - normalize_0(_S588))), (0.0f))) + 0.00100000004749745f);
        break;
    }
    return _S587;
}

struct s_bwd_prop_depth_normal_loss_Intermediates_0
{
    float2  _S608;
    bool _S609;
    float2  _S610;
    bool _S611;
    float2  _S612;
    bool _S613;
    float2  _S614;
    bool _S615;
    float2  _S616;
    bool _S617;
};

inline __device__ void s_bwd_prop_normalize_impl_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_19, float3  _s_dOut_11)
{
    float _S618 = length_0((*dpx_19).primal_0);
    float3  _S619 = (*dpx_19).primal_0 * _s_dOut_11;
    float3  _S620 = make_float3 (1.0f / _S618) * _s_dOut_11;
    float _S621 = - ((_S619.x + _S619.y + _S619.z) / (_S618 * _S618));
    float3  _S622 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S623;
    (&_S623)->primal_0 = (*dpx_19).primal_0;
    (&_S623)->differential_0 = _S622;
    s_bwd_length_impl_0(&_S623, _S621);
    float3  _S624 = _S620 + _S623.differential_0;
    dpx_19->primal_0 = (*dpx_19).primal_0;
    dpx_19->differential_0 = _S624;
    return;
}

inline __device__ void s_bwd_normalize_impl_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S625, float3  _S626)
{
    s_bwd_prop_normalize_impl_0(_S625, _S626);
    return;
}

inline __device__ void depth_normal_loss_vjp_none(float2  pix_center_4, float4  intrins_7, FixedArray<float, 1>  dist_coeffs_11, int camera_model_9, bool is_ray_depth_8, float4  depths_3, float3  gt_normal_1, float v_loss_0, float4  * v_depths_1, float3  * v_gt_normal_0)
{
    float2  _S627 = make_float2 (0.0f);
    s_bwd_prop_depth_normal_loss_Intermediates_0 _S628;
    (&_S628)->_S608 = _S627;
    (&_S628)->_S609 = false;
    (&_S628)->_S610 = _S627;
    (&_S628)->_S611 = false;
    (&_S628)->_S612 = _S627;
    (&_S628)->_S613 = false;
    (&_S628)->_S614 = _S627;
    (&_S628)->_S615 = false;
    (&_S628)->_S616 = _S627;
    (&_S628)->_S617 = false;
    (&_S628)->_S610 = _S627;
    (&_S628)->_S611 = false;
    (&_S628)->_S612 = _S627;
    (&_S628)->_S613 = false;
    (&_S628)->_S614 = _S627;
    (&_S628)->_S615 = false;
    (&_S628)->_S616 = _S627;
    (&_S628)->_S617 = false;
    float2  _S629 = float2 {intrins_7.z, intrins_7.w};
    float2  _S630 = float2 {intrins_7.x, intrins_7.y};
    float2  uv_25 = (pix_center_4 + make_float2 (-1.0f, -0.0f) - _S629) / _S630;
    float2  _S631 = _S627;
    FixedArray<float, 1>  _S632 = dist_coeffs_11;
    bool _S633 = undistort_point_0(uv_25, &_S632, int(12), &_S631);
    (&_S628)->_S608 = _S631;
    (&_S628)->_S609 = _S633;
    bool _S634 = !!_S633;
    bool _runFlag_7;
    if(_S634)
    {
        float2  uv_26 = (pix_center_4 + make_float2 (1.0f, -0.0f) - _S629) / _S630;
        float2  _S635 = _S627;
        FixedArray<float, 1>  _S636 = dist_coeffs_11;
        bool _S637 = undistort_point_0(uv_26, &_S636, int(12), &_S635);
        (&_S628)->_S610 = _S635;
        (&_S628)->_S611 = _S637;
        if(!_S637)
        {
            _runFlag_7 = false;
        }
        else
        {
            _runFlag_7 = _S634;
        }
        if(_runFlag_7)
        {
            float2  uv_27 = (pix_center_4 + make_float2 (0.0f, -1.0f) - _S629) / _S630;
            float2  _S638 = _S627;
            FixedArray<float, 1>  _S639 = dist_coeffs_11;
            bool _S640 = undistort_point_0(uv_27, &_S639, int(12), &_S638);
            (&_S628)->_S612 = _S638;
            (&_S628)->_S613 = _S640;
            if(!_S640)
            {
                _runFlag_7 = false;
            }
            if(_runFlag_7)
            {
                float2  uv_28 = (pix_center_4 + make_float2 (0.0f, 1.0f) - _S629) / _S630;
                float2  _S641 = _S627;
                FixedArray<float, 1>  _S642 = dist_coeffs_11;
                bool _S643 = undistort_point_0(uv_28, &_S642, int(12), &_S641);
                (&_S628)->_S614 = _S641;
                (&_S628)->_S615 = _S643;
                if(!_S643)
                {
                    _runFlag_7 = false;
                }
                if(_runFlag_7)
                {
                    float2  uv_29 = (pix_center_4 - _S629) / _S630;
                    float2  _S644 = _S627;
                    FixedArray<float, 1>  _S645 = dist_coeffs_11;
                    bool _S646 = undistort_point_0(uv_29, &_S645, int(12), &_S644);
                    (&_S628)->_S616 = _S644;
                    (&_S628)->_S617 = _S646;
                }
            }
        }
    }
    s_bwd_prop_depth_normal_loss_Intermediates_0 _S647 = _S628;
    float3  _S648 = make_float3 (0.0f);
    bool _S649 = !!_S628._S609;
    bool _runFlag_8;
    bool _runFlag_9;
    bool _runFlag_10;
    int _S650;
    float3  raydir_8;
    float3  _S651;
    float3  _S652;
    float3  _S653;
    float3  _S654;
    FixedArray<float3 , 5>  points_5;
    if(_S649)
    {
        float3  _S655 = s_primal_ctx_unproject_raydir_0(_S647._S608, camera_model_9, is_ray_depth_8);
        float3  _S656 = make_float3 (depths_3.x) * _S655;
        if(!_S647._S611)
        {
            _runFlag_7 = false;
        }
        else
        {
            _runFlag_7 = _S649;
        }
        if(_runFlag_7)
        {
            float3  _S657 = s_primal_ctx_unproject_raydir_0(_S647._S610, camera_model_9, is_ray_depth_8);
            float3  _S658 = make_float3 (depths_3.y) * _S657;
            if(!_S647._S613)
            {
                _runFlag_8 = false;
            }
            else
            {
                _runFlag_8 = _runFlag_7;
            }
            if(_runFlag_8)
            {
                float3  _S659 = s_primal_ctx_unproject_raydir_0(_S647._S612, camera_model_9, is_ray_depth_8);
                float3  _S660 = make_float3 (depths_3.z) * _S659;
                if(!_S647._S615)
                {
                    _runFlag_9 = false;
                }
                else
                {
                    _runFlag_9 = _runFlag_8;
                }
                if(_runFlag_9)
                {
                    float3  _S661 = s_primal_ctx_unproject_raydir_0(_S647._S614, camera_model_9, is_ray_depth_8);
                    float3  _S662 = make_float3 (depths_3.w) * _S661;
                    if(!_S647._S617)
                    {
                        _runFlag_10 = false;
                    }
                    else
                    {
                        _runFlag_10 = _runFlag_9;
                    }
                    if(_runFlag_10)
                    {
                        float3  _S663 = s_primal_ctx_unproject_raydir_0(_S647._S616, camera_model_9, is_ray_depth_8);
                        _S650 = int(1);
                        raydir_8 = _S663;
                    }
                    else
                    {
                        _S650 = int(0);
                        raydir_8 = _S661;
                    }
                    points_5[int(0)] = _S656;
                    points_5[int(1)] = _S658;
                    points_5[int(2)] = _S660;
                    points_5[int(3)] = _S662;
                    points_5[int(4)] = _S648;
                    _S651 = _S661;
                }
                else
                {
                    _S650 = int(0);
                    raydir_8 = _S659;
                    points_5[int(0)] = _S656;
                    points_5[int(1)] = _S658;
                    points_5[int(2)] = _S660;
                    points_5[int(3)] = _S648;
                    points_5[int(4)] = _S648;
                    _S651 = _S648;
                }
                _S652 = _S659;
            }
            else
            {
                _S650 = int(0);
                raydir_8 = _S657;
                points_5[int(0)] = _S656;
                points_5[int(1)] = _S658;
                points_5[int(2)] = _S648;
                points_5[int(3)] = _S648;
                points_5[int(4)] = _S648;
                _runFlag_9 = false;
                _S651 = _S648;
                _S652 = _S648;
            }
            _S653 = _S657;
        }
        else
        {
            _S650 = int(0);
            raydir_8 = _S655;
            points_5[int(0)] = _S656;
            points_5[int(1)] = _S648;
            points_5[int(2)] = _S648;
            points_5[int(3)] = _S648;
            points_5[int(4)] = _S648;
            _runFlag_8 = false;
            _runFlag_9 = false;
            _S651 = _S648;
            _S652 = _S648;
            _S653 = _S648;
        }
        _S654 = _S655;
    }
    else
    {
        _S650 = int(0);
        points_5[int(0)] = _S648;
        points_5[int(1)] = _S648;
        points_5[int(2)] = _S648;
        points_5[int(3)] = _S648;
        points_5[int(4)] = _S648;
        _runFlag_7 = false;
        _runFlag_8 = false;
        _runFlag_9 = false;
        _S651 = _S648;
        _S652 = _S648;
        _S653 = _S648;
        _S654 = _S648;
    }
    bool _S664 = !(_S650 != int(1));
    bool _S665;
    float3  normal_6;
    float3  _S666;
    float3  _S667;
    float3  _S668;
    float3  _S669;
    float _S670;
    float _S671;
    float _S672;
    float _S673;
    if(_S664)
    {
        float3  dx_2 = points_5[int(1)] - points_5[int(0)];
        float3  _S674 = - (points_5[int(3)] - points_5[int(2)]);
        float3  _S675 = s_primal_ctx_cross_0(dx_2, _S674);
        bool _S676 = (s_primal_ctx_dot_0(_S675, _S675)) != 0.0f;
        if(_S676)
        {
            normal_6 = normalize_0(_S675);
        }
        else
        {
            normal_6 = _S675;
        }
        bool _S677 = (s_primal_ctx_dot_0(gt_normal_1, gt_normal_1)) != 0.0f;
        if(_S677)
        {
            _S666 = normalize_0(gt_normal_1);
        }
        else
        {
            _S666 = gt_normal_1;
        }
        float3  _S678 = - normalize_0(raydir_8);
        float _S679 = s_primal_ctx_dot_0(normal_6, _S678);
        float _S680 = 1.0f - s_primal_ctx_dot_0(normal_6, _S666) + 0.00100000004749745f;
        float _S681 = (F32_max((_S679), (0.0f))) + 0.00100000004749745f;
        _S670 = _S681 * _S681;
        _S671 = _S680;
        _S672 = _S681;
        _S673 = _S679;
        raydir_8 = normal_6;
        normal_6 = _S678;
        _runFlag_10 = _S677;
        _S665 = _S676;
        _S667 = _S675;
        _S668 = dx_2;
        _S669 = _S674;
    }
    else
    {
        _S670 = 0.0f;
        _S671 = 0.0f;
        _S672 = 0.0f;
        _S673 = 0.0f;
        raydir_8 = _S648;
        normal_6 = _S648;
        _S666 = _S648;
        _runFlag_10 = false;
        _S665 = false;
        _S667 = _S648;
        _S668 = _S648;
        _S669 = _S648;
    }
    float4  _S682 = make_float4 (0.0f);
    if(_S664)
    {
        float _S683 = v_loss_0 / _S670;
        float _S684 = _S671 * - _S683;
        float s_diff_num_T_0 = _S672 * _S683;
        DiffPair_float_0 _S685;
        (&_S685)->primal_0 = _S673;
        (&_S685)->differential_0 = 0.0f;
        DiffPair_float_0 _S686;
        (&_S686)->primal_0 = 0.0f;
        (&_S686)->differential_0 = 0.0f;
        _d_max_0(&_S685, &_S686, _S684);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S687;
        (&_S687)->primal_0 = raydir_8;
        (&_S687)->differential_0 = _S648;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S688;
        (&_S688)->primal_0 = normal_6;
        (&_S688)->differential_0 = _S648;
        s_bwd_prop_dot_0(&_S687, &_S688, _S685.differential_0);
        float _S689 = - s_diff_num_T_0;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S690;
        (&_S690)->primal_0 = raydir_8;
        (&_S690)->differential_0 = _S648;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S691;
        (&_S691)->primal_0 = _S666;
        (&_S691)->differential_0 = _S648;
        s_bwd_prop_dot_0(&_S690, &_S691, _S689);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S692 = _S691;
        float3  _S693 = _S687.differential_0 + _S690.differential_0;
        if(_runFlag_10)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S694;
            (&_S694)->primal_0 = gt_normal_1;
            (&_S694)->differential_0 = _S648;
            s_bwd_normalize_impl_0(&_S694, _S692.differential_0);
            raydir_8 = _S694.differential_0;
        }
        else
        {
            raydir_8 = _S692.differential_0;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S695;
        (&_S695)->primal_0 = gt_normal_1;
        (&_S695)->differential_0 = _S648;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S696;
        (&_S696)->primal_0 = gt_normal_1;
        (&_S696)->differential_0 = _S648;
        s_bwd_prop_dot_0(&_S695, &_S696, 0.0f);
        float3  _S697 = _S696.differential_0 + _S695.differential_0 + raydir_8;
        if(_S665)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S698;
            (&_S698)->primal_0 = _S667;
            (&_S698)->differential_0 = _S648;
            s_bwd_normalize_impl_0(&_S698, _S693);
            raydir_8 = _S698.differential_0;
        }
        else
        {
            raydir_8 = _S693;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S699;
        (&_S699)->primal_0 = _S667;
        (&_S699)->differential_0 = _S648;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S700;
        (&_S700)->primal_0 = _S667;
        (&_S700)->differential_0 = _S648;
        s_bwd_prop_dot_0(&_S699, &_S700, 0.0f);
        float3  _S701 = _S700.differential_0 + _S699.differential_0 + raydir_8;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S702;
        (&_S702)->primal_0 = _S668;
        (&_S702)->differential_0 = _S648;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S703;
        (&_S703)->primal_0 = _S669;
        (&_S703)->differential_0 = _S648;
        s_bwd_prop_cross_0(&_S702, &_S703, _S701);
        float3  s_diff_dy_T_2 = - _S703.differential_0;
        float3  _S704 = - s_diff_dy_T_2;
        float3  _S705 = - _S702.differential_0;
        FixedArray<float3 , 5>  _S706;
        _S706[int(0)] = _S648;
        _S706[int(1)] = _S648;
        _S706[int(2)] = _S648;
        _S706[int(3)] = _S648;
        _S706[int(4)] = _S648;
        _S706[int(2)] = _S704;
        _S706[int(3)] = s_diff_dy_T_2;
        _S706[int(0)] = _S705;
        _S706[int(1)] = _S702.differential_0;
        points_5[int(0)] = _S706[int(0)];
        points_5[int(1)] = _S706[int(1)];
        points_5[int(2)] = _S706[int(2)];
        points_5[int(3)] = _S706[int(3)];
        points_5[int(4)] = _S706[int(4)];
        raydir_8 = _S697;
    }
    else
    {
        points_5[int(0)] = _S648;
        points_5[int(1)] = _S648;
        points_5[int(2)] = _S648;
        points_5[int(3)] = _S648;
        points_5[int(4)] = _S648;
        raydir_8 = _S648;
    }
    float4  _S707;
    if(_S649)
    {
        if(_runFlag_7)
        {
            if(_runFlag_8)
            {
                if(_runFlag_9)
                {
                    FixedArray<float3 , 5>  _S708 = points_5;
                    FixedArray<float3 , 5>  _S709 = points_5;
                    FixedArray<float3 , 5>  _S710 = points_5;
                    float3  _S711 = _S651 * points_5[int(3)];
                    float _S712 = _S711.x + _S711.y + _S711.z;
                    float4  _S713 = _S682;
                    *&((&_S713)->w) = _S712;
                    points_5[int(0)] = _S648;
                    points_5[int(1)] = _S648;
                    points_5[int(2)] = _S648;
                    points_5[int(3)] = _S648;
                    points_5[int(4)] = _S648;
                    _S651 = _S710[int(2)];
                    normal_6 = _S708[int(0)];
                    _S666 = _S709[int(1)];
                    _S707 = _S713;
                }
                else
                {
                    FixedArray<float3 , 5>  _S714 = points_5;
                    FixedArray<float3 , 5>  _S715 = points_5;
                    FixedArray<float3 , 5>  _S716 = points_5;
                    FixedArray<float3 , 5>  _S717 = points_5;
                    points_5[int(0)] = points_5[int(0)];
                    points_5[int(1)] = _S714[int(1)];
                    points_5[int(2)] = _S715[int(2)];
                    points_5[int(3)] = _S716[int(3)];
                    points_5[int(4)] = _S717[int(4)];
                    _S651 = _S648;
                    normal_6 = _S648;
                    _S666 = _S648;
                    _S707 = _S682;
                }
                float3  _S718 = _S652 * (points_5[int(2)] + _S651);
                float _S719 = _S718.x + _S718.y + _S718.z;
                float3  _S720 = points_5[int(0)] + normal_6;
                float3  _S721 = points_5[int(1)] + _S666;
                float4  _S722 = _S682;
                *&((&_S722)->z) = _S719;
                float4  _S723 = _S707 + _S722;
                points_5[int(0)] = _S648;
                points_5[int(1)] = _S648;
                points_5[int(2)] = _S648;
                points_5[int(3)] = _S648;
                points_5[int(4)] = _S648;
                _S651 = _S721;
                _S652 = _S720;
                _S707 = _S723;
            }
            else
            {
                FixedArray<float3 , 5>  _S724 = points_5;
                FixedArray<float3 , 5>  _S725 = points_5;
                FixedArray<float3 , 5>  _S726 = points_5;
                FixedArray<float3 , 5>  _S727 = points_5;
                points_5[int(0)] = points_5[int(0)];
                points_5[int(1)] = _S724[int(1)];
                points_5[int(2)] = _S725[int(2)];
                points_5[int(3)] = _S726[int(3)];
                points_5[int(4)] = _S727[int(4)];
                _S651 = _S648;
                _S652 = _S648;
                _S707 = _S682;
            }
            float3  _S728 = _S653 * (points_5[int(1)] + _S651);
            float _S729 = _S728.x + _S728.y + _S728.z;
            float3  _S730 = points_5[int(0)] + _S652;
            float4  _S731 = _S682;
            *&((&_S731)->y) = _S729;
            float4  _S732 = _S707 + _S731;
            points_5[int(0)] = _S648;
            points_5[int(1)] = _S648;
            points_5[int(2)] = _S648;
            points_5[int(3)] = _S648;
            points_5[int(4)] = _S648;
            _S651 = _S730;
            _S707 = _S732;
        }
        else
        {
            FixedArray<float3 , 5>  _S733 = points_5;
            FixedArray<float3 , 5>  _S734 = points_5;
            FixedArray<float3 , 5>  _S735 = points_5;
            FixedArray<float3 , 5>  _S736 = points_5;
            points_5[int(0)] = points_5[int(0)];
            points_5[int(1)] = _S733[int(1)];
            points_5[int(2)] = _S734[int(2)];
            points_5[int(3)] = _S735[int(3)];
            points_5[int(4)] = _S736[int(4)];
            _S651 = _S648;
            _S707 = _S682;
        }
        float3  _S737 = _S654 * (points_5[int(0)] + _S651);
        float _S738 = _S737.x + _S737.y + _S737.z;
        float4  _S739 = _S682;
        *&((&_S739)->x) = _S738;
        _S707 = _S707 + _S739;
    }
    else
    {
        _S707 = _S682;
    }
    *v_depths_1 = _S707;
    *v_gt_normal_0 = raydir_8;
    return;
}

inline __device__ float3  generate_ray_d2n_opencv(float2  pix_pos_3, float4  intrins_8, FixedArray<float, 4>  dist_coeffs_12, int camera_model_10, bool is_ray_depth_9)
{
    float3  _S740;
    for(;;)
    {
        float2  uv_30 = (pix_pos_3 - float2 {intrins_8.z, intrins_8.w}) / float2 {intrins_8.x, intrins_8.y};
        FixedArray<float, 4>  _S741 = dist_coeffs_12;
        float2  uv_u_12;
        bool _S742 = undistort_point_1(uv_30, &_S741, int(12), &uv_u_12);
        if(!_S742)
        {
            int3  _S743 = make_int3 (int(0));
            float3  _S744 = make_float3 ((float)_S743.x, (float)_S743.y, (float)_S743.z);
            _S740 = _S744;
            break;
        }
        _S740 = unproject_raydir_0(uv_u_12, camera_model_10, is_ray_depth_9);
        break;
    }
    return _S740;
}

inline __device__ float3  depth_to_point_opencv(float2  pix_pos_4, float4  intrins_9, FixedArray<float, 4>  dist_coeffs_13, int camera_model_11, bool is_ray_depth_10, float depth_4)
{
    float3  _S745;
    for(;;)
    {
        float2  uv_31 = (pix_pos_4 - float2 {intrins_9.z, intrins_9.w}) / float2 {intrins_9.x, intrins_9.y};
        FixedArray<float, 4>  _S746 = dist_coeffs_13;
        float2  uv_u_13;
        bool _S747 = undistort_point_1(uv_31, &_S746, int(12), &uv_u_13);
        if(!_S747)
        {
            _S745 = make_float3 (0.0f);
            break;
        }
        _S745 = make_float3 (depth_4) * unproject_raydir_0(uv_u_13, camera_model_11, is_ray_depth_10);
        break;
    }
    return _S745;
}

struct s_bwd_prop_depth_to_point_Intermediates_1
{
    float2  _S748;
    bool _S749;
};

inline __device__ float depth_to_point_vjp_opencv(float2  pix_pos_5, float4  intrins_10, FixedArray<float, 4>  dist_coeffs_14, int camera_model_12, bool is_ray_depth_11, float depth_5, float3  v_point_1)
{
    float2  _S750 = make_float2 (0.0f);
    s_bwd_prop_depth_to_point_Intermediates_1 _S751;
    (&_S751)->_S748 = _S750;
    (&_S751)->_S749 = false;
    float2  uv_32 = (pix_pos_5 - float2 {intrins_10.z, intrins_10.w}) / float2 {intrins_10.x, intrins_10.y};
    float2  _S752 = _S750;
    FixedArray<float, 4>  _S753 = dist_coeffs_14;
    bool _S754 = undistort_point_1(uv_32, &_S753, int(12), &_S752);
    (&_S751)->_S748 = _S752;
    (&_S751)->_S749 = _S754;
    s_bwd_prop_depth_to_point_Intermediates_1 _S755 = _S751;
    float3  _S756 = make_float3 (0.0f);
    bool _S757 = !!_S751._S749;
    float3  _S758;
    if(_S757)
    {
        _S758 = s_primal_ctx_unproject_raydir_0(_S755._S748, camera_model_12, is_ray_depth_11);
    }
    else
    {
        _S758 = _S756;
    }
    if(_S757)
    {
        _S758 = _S758 * v_point_1;
    }
    else
    {
        _S758 = _S756;
    }
    return _S758.x + _S758.y + _S758.z;
}

inline __device__ float3  depth_to_normal_opencv(float2  pix_center_5, float4  intrins_11, FixedArray<float, 4>  dist_coeffs_15, int camera_model_13, bool is_ray_depth_12, float4  depths_4)
{
    float3  normal_7;
    for(;;)
    {
        bool _S759;
        if((depths_4.x) == 0.0f)
        {
            _S759 = true;
        }
        else
        {
            _S759 = (depths_4.y) == 0.0f;
        }
        if(_S759)
        {
            _S759 = true;
        }
        else
        {
            _S759 = (depths_4.z) == 0.0f;
        }
        if(_S759)
        {
            _S759 = true;
        }
        else
        {
            _S759 = (depths_4.w) == 0.0f;
        }
        if(_S759)
        {
            normal_7 = make_float3 (0.0f);
            break;
        }
        float3  * _S760;
        float3  * _S761;
        float3  * _S762;
        float3  * _S763;
        int _S764;
        FixedArray<float3 , 4>  points_6;
        for(;;)
        {
            float2  _S765 = float2 {intrins_11.z, intrins_11.w};
            float2  _S766 = float2 {intrins_11.x, intrins_11.y};
            float2  uv_33 = (pix_center_5 + make_float2 (-1.0f, -0.0f) - _S765) / _S766;
            FixedArray<float, 4>  _S767 = dist_coeffs_15;
            float2  uv_u_14;
            bool _S768 = undistort_point_1(uv_33, &_S767, int(12), &uv_u_14);
            if(!_S768)
            {
                float3  _S769 = make_float3 (0.0f);
                _S764 = int(0);
                _S763 = nullptr;
                _S762 = nullptr;
                _S761 = nullptr;
                _S760 = nullptr;
                normal_7 = _S769;
                break;
            }
            points_6[int(0)] = make_float3 (depths_4.x) * unproject_raydir_0(uv_u_14, camera_model_13, is_ray_depth_12);
            for(;;)
            {
                float2  uv_34 = (pix_center_5 + make_float2 (1.0f, -0.0f) - _S765) / _S766;
                FixedArray<float, 4>  _S770 = dist_coeffs_15;
                float2  uv_u_15;
                bool _S771 = undistort_point_1(uv_34, &_S770, int(12), &uv_u_15);
                if(!_S771)
                {
                    float3  _S772 = make_float3 (0.0f);
                    _S764 = int(0);
                    _S763 = nullptr;
                    normal_7 = _S772;
                    break;
                }
                points_6[int(1)] = make_float3 (depths_4.y) * unproject_raydir_0(uv_u_15, camera_model_13, is_ray_depth_12);
                _S764 = int(2);
                _S763 = &points_6[int(1)];
                break;
            }
            if(_S764 != int(2))
            {
                _S762 = &points_6[int(0)];
                _S761 = nullptr;
                _S760 = nullptr;
                break;
            }
            float2  uv_35 = (pix_center_5 + make_float2 (0.0f, -1.0f) - _S765) / _S766;
            FixedArray<float, 4>  _S773 = dist_coeffs_15;
            float2  uv_u_16;
            bool _S774 = undistort_point_1(uv_35, &_S773, int(12), &uv_u_16);
            if(!_S774)
            {
                float3  _S775 = make_float3 (0.0f);
                _S764 = int(0);
                _S762 = &points_6[int(0)];
                _S761 = nullptr;
                _S760 = nullptr;
                normal_7 = _S775;
                break;
            }
            points_6[int(2)] = make_float3 (depths_4.z) * unproject_raydir_0(uv_u_16, camera_model_13, is_ray_depth_12);
            for(;;)
            {
                float2  uv_36 = (pix_center_5 + make_float2 (0.0f, 1.0f) - _S765) / _S766;
                FixedArray<float, 4>  _S776 = dist_coeffs_15;
                float2  uv_u_17;
                bool _S777 = undistort_point_1(uv_36, &_S776, int(12), &uv_u_17);
                if(!_S777)
                {
                    float3  _S778 = make_float3 (0.0f);
                    _S764 = int(0);
                    _S762 = nullptr;
                    normal_7 = _S778;
                    break;
                }
                points_6[int(3)] = make_float3 (depths_4.w) * unproject_raydir_0(uv_u_17, camera_model_13, is_ray_depth_12);
                _S764 = int(2);
                _S762 = &points_6[int(3)];
                break;
            }
            if(_S764 != int(2))
            {
                float3  * _S779 = _S762;
                _S762 = &points_6[int(0)];
                _S761 = _S779;
                _S760 = &points_6[int(2)];
                break;
            }
            float3  * _S780 = _S762;
            _S764 = int(1);
            _S762 = &points_6[int(0)];
            _S761 = _S780;
            _S760 = &points_6[int(2)];
            break;
        }
        if(_S764 != int(1))
        {
            break;
        }
        float3  normal_8 = cross_0(*_S763 - *_S762, - (*_S761 - *_S760));
        if((dot_0(normal_8, normal_8)) != 0.0f)
        {
            normal_7 = normal_8 / make_float3 (length_0(normal_8));
        }
        else
        {
            normal_7 = normal_8;
        }
        break;
    }
    return normal_7;
}

struct s_bwd_prop_depth_to_normal_Intermediates_1
{
    float2  _S781;
    bool _S782;
    float2  _S783;
    bool _S784;
    float2  _S785;
    bool _S786;
    float2  _S787;
    bool _S788;
};

inline __device__ void depth_to_normal_vjp_opencv(float2  pix_center_6, float4  intrins_12, FixedArray<float, 4>  dist_coeffs_16, int camera_model_14, bool is_ray_depth_13, float4  depths_5, float3  v_normal_2, float4  * v_depths_2)
{
    float2  _S789 = make_float2 (0.0f);
    s_bwd_prop_depth_to_normal_Intermediates_1 _S790;
    (&_S790)->_S781 = _S789;
    (&_S790)->_S782 = false;
    (&_S790)->_S783 = _S789;
    (&_S790)->_S784 = false;
    (&_S790)->_S785 = _S789;
    (&_S790)->_S786 = false;
    (&_S790)->_S787 = _S789;
    (&_S790)->_S788 = false;
    (&_S790)->_S781 = _S789;
    (&_S790)->_S782 = false;
    (&_S790)->_S783 = _S789;
    (&_S790)->_S784 = false;
    (&_S790)->_S785 = _S789;
    (&_S790)->_S786 = false;
    (&_S790)->_S787 = _S789;
    (&_S790)->_S788 = false;
    bool _S791 = (depths_5.x) == 0.0f;
    bool _runFlag_11;
    if(_S791)
    {
        _runFlag_11 = true;
    }
    else
    {
        _runFlag_11 = (depths_5.y) == 0.0f;
    }
    if(_runFlag_11)
    {
        _runFlag_11 = true;
    }
    else
    {
        _runFlag_11 = (depths_5.z) == 0.0f;
    }
    if(_runFlag_11)
    {
        _runFlag_11 = true;
    }
    else
    {
        _runFlag_11 = (depths_5.w) == 0.0f;
    }
    int _S792;
    if(!_runFlag_11)
    {
        float2  _S793 = float2 {intrins_12.z, intrins_12.w};
        float2  _S794 = float2 {intrins_12.x, intrins_12.y};
        float2  uv_37 = (pix_center_6 + make_float2 (-1.0f, -0.0f) - _S793) / _S794;
        float2  _S795 = _S789;
        FixedArray<float, 4>  _S796 = dist_coeffs_16;
        bool _S797 = undistort_point_1(uv_37, &_S796, int(12), &_S795);
        (&_S790)->_S781 = _S795;
        (&_S790)->_S782 = _S797;
        bool _S798 = !!_S797;
        if(_S798)
        {
            float2  uv_38 = (pix_center_6 + make_float2 (1.0f, -0.0f) - _S793) / _S794;
            float2  _S799 = _S789;
            FixedArray<float, 4>  _S800 = dist_coeffs_16;
            bool _S801 = undistort_point_1(uv_38, &_S800, int(12), &_S799);
            (&_S790)->_S783 = _S799;
            (&_S790)->_S784 = _S801;
            if(!!_S801)
            {
                _S792 = int(2);
            }
            else
            {
                _S792 = int(0);
            }
            if(_S792 != int(2))
            {
                _runFlag_11 = false;
            }
            else
            {
                _runFlag_11 = _S798;
            }
            if(_runFlag_11)
            {
                float2  uv_39 = (pix_center_6 + make_float2 (0.0f, -1.0f) - _S793) / _S794;
                float2  _S802 = _S789;
                FixedArray<float, 4>  _S803 = dist_coeffs_16;
                bool _S804 = undistort_point_1(uv_39, &_S803, int(12), &_S802);
                (&_S790)->_S785 = _S802;
                (&_S790)->_S786 = _S804;
                if(!_S804)
                {
                    _runFlag_11 = false;
                }
                if(_runFlag_11)
                {
                    float2  uv_40 = (pix_center_6 + make_float2 (0.0f, 1.0f) - _S793) / _S794;
                    float2  _S805 = _S789;
                    FixedArray<float, 4>  _S806 = dist_coeffs_16;
                    bool _S807 = undistort_point_1(uv_40, &_S806, int(12), &_S805);
                    (&_S790)->_S787 = _S805;
                    (&_S790)->_S788 = _S807;
                }
            }
        }
    }
    s_bwd_prop_depth_to_normal_Intermediates_1 _S808 = _S790;
    float3  _S809 = make_float3 (0.0f);
    if(_S791)
    {
        _runFlag_11 = true;
    }
    else
    {
        _runFlag_11 = (depths_5.y) == 0.0f;
    }
    if(_runFlag_11)
    {
        _runFlag_11 = true;
    }
    else
    {
        _runFlag_11 = (depths_5.z) == 0.0f;
    }
    if(_runFlag_11)
    {
        _runFlag_11 = true;
    }
    else
    {
        _runFlag_11 = (depths_5.w) == 0.0f;
    }
    bool _S810 = !_runFlag_11;
    bool _runFlag_12;
    bool _runFlag_13;
    bool _S811;
    bool _runFlag_14;
    bool _S812;
    bool _S813;
    FixedArray<float3 , 4>  points_7;
    float3  _S814;
    float3  _S815;
    float3  _S816;
    float3  _S817;
    float3  _S818;
    float3  _S819;
    float3  _S820;
    float3  _S821;
    float3  _S822;
    if(_S810)
    {
        bool _S823 = !!_S808._S782;
        if(_S823)
        {
            float3  _S824 = s_primal_ctx_unproject_raydir_0(_S808._S781, camera_model_14, is_ray_depth_13);
            float3  _S825 = make_float3 (depths_5.x) * _S824;
            bool _S826 = !!_S808._S784;
            if(_S826)
            {
                float3  _S827 = s_primal_ctx_unproject_raydir_0(_S808._S783, camera_model_14, is_ray_depth_13);
                float3  _S828 = make_float3 (depths_5.y) * _S827;
                _S792 = int(2);
                points_7[int(0)] = _S825;
                points_7[int(1)] = _S828;
                points_7[int(2)] = _S809;
                points_7[int(3)] = _S809;
                _S814 = _S827;
            }
            else
            {
                _S792 = int(0);
                points_7[int(0)] = _S825;
                points_7[int(1)] = _S809;
                points_7[int(2)] = _S809;
                points_7[int(3)] = _S809;
                _S814 = _S809;
            }
            if(_S792 != int(2))
            {
                _runFlag_11 = false;
            }
            else
            {
                _runFlag_11 = _S823;
                _S792 = int(0);
            }
            if(_runFlag_11)
            {
                if(!_S808._S786)
                {
                    _runFlag_12 = false;
                    _S792 = int(0);
                }
                else
                {
                    _runFlag_12 = _runFlag_11;
                }
                if(_runFlag_12)
                {
                    float3  _S829 = s_primal_ctx_unproject_raydir_0(_S808._S785, camera_model_14, is_ray_depth_13);
                    points_7[int(2)] = make_float3 (depths_5.z) * _S829;
                    bool _S830 = !!_S808._S788;
                    int _S831;
                    if(_S830)
                    {
                        float3  _S832 = s_primal_ctx_unproject_raydir_0(_S808._S787, camera_model_14, is_ray_depth_13);
                        points_7[int(3)] = make_float3 (depths_5.w) * _S832;
                        _S831 = int(2);
                        _S815 = _S832;
                    }
                    else
                    {
                        _S831 = int(0);
                        _S815 = _S809;
                    }
                    if(_S831 != int(2))
                    {
                        _runFlag_13 = false;
                        _S792 = _S831;
                    }
                    else
                    {
                        _runFlag_13 = _runFlag_12;
                    }
                    if(_runFlag_13)
                    {
                        _S792 = int(1);
                    }
                    _runFlag_13 = _S830;
                    _S816 = _S829;
                }
                else
                {
                    _runFlag_13 = false;
                    _S815 = _S809;
                    _S816 = _S809;
                }
            }
            else
            {
                _runFlag_12 = false;
                _runFlag_13 = false;
                _S815 = _S809;
                _S816 = _S809;
            }
            float3  _S833 = _S814;
            _S814 = _S815;
            _S815 = _S816;
            _S811 = _S826;
            _S816 = _S833;
            _S817 = _S824;
        }
        else
        {
            _S792 = int(0);
            points_7[int(0)] = _S809;
            points_7[int(1)] = _S809;
            points_7[int(2)] = _S809;
            points_7[int(3)] = _S809;
            _runFlag_11 = false;
            _runFlag_12 = false;
            _runFlag_13 = false;
            _S814 = _S809;
            _S815 = _S809;
            _S811 = false;
            _S816 = _S809;
            _S817 = _S809;
        }
        if(_S792 != int(1))
        {
            _runFlag_14 = false;
        }
        else
        {
            _runFlag_14 = _S810;
        }
        if(_runFlag_14)
        {
            float3  dx_3 = points_7[int(1)] - points_7[int(0)];
            float3  _S834 = - (points_7[int(3)] - points_7[int(2)]);
            float3  _S835 = s_primal_ctx_cross_0(dx_3, _S834);
            bool _S836 = (s_primal_ctx_dot_0(_S835, _S835)) != 0.0f;
            if(_S836)
            {
                float _S837 = length_0(_S835);
                float3  _S838 = make_float3 (_S837);
                _S818 = make_float3 (_S837 * _S837);
                _S819 = _S838;
            }
            else
            {
                _S818 = _S809;
                _S819 = _S809;
            }
            float3  _S839 = _S819;
            _S812 = _S836;
            _S819 = _S835;
            _S820 = _S839;
            _S821 = dx_3;
            _S822 = _S834;
        }
        else
        {
            _S812 = false;
            _S818 = _S809;
            _S819 = _S809;
            _S820 = _S809;
            _S821 = _S809;
            _S822 = _S809;
        }
        bool _S840 = _runFlag_11;
        bool _S841 = _runFlag_12;
        bool _S842 = _runFlag_13;
        float3  _S843 = _S814;
        float3  _S844 = _S815;
        bool _S845 = _S811;
        float3  _S846 = _S816;
        float3  _S847 = _S817;
        _runFlag_11 = _runFlag_14;
        _runFlag_12 = _S812;
        _S814 = _S818;
        _S815 = _S819;
        _S816 = _S820;
        _S817 = _S821;
        _S818 = _S822;
        _runFlag_13 = _S823;
        _S811 = _S840;
        _runFlag_14 = _S841;
        _S812 = _S842;
        _S819 = _S843;
        _S820 = _S844;
        _S813 = _S845;
        _S821 = _S846;
        _S822 = _S847;
    }
    else
    {
        _runFlag_11 = false;
        _runFlag_12 = false;
        _S814 = _S809;
        _S815 = _S809;
        _S816 = _S809;
        _S817 = _S809;
        _S818 = _S809;
        _runFlag_13 = false;
        _S811 = false;
        _runFlag_14 = false;
        _S812 = false;
        _S819 = _S809;
        _S820 = _S809;
        _S813 = false;
        _S821 = _S809;
        _S822 = _S809;
    }
    float4  _S848 = make_float4 (0.0f);
    float4  _S849;
    if(_S810)
    {
        if(_runFlag_11)
        {
            if(_runFlag_12)
            {
                float3  _S850 = v_normal_2 / _S814;
                float3  _S851 = _S815 * - _S850;
                float3  _S852 = _S816 * _S850;
                float _S853 = _S851.x + _S851.y + _S851.z;
                DiffPair_vectorx3Cfloatx2C3x3E_0 _S854;
                (&_S854)->primal_0 = _S815;
                (&_S854)->differential_0 = _S809;
                s_bwd_length_impl_0(&_S854, _S853);
                _S814 = _S852 + _S854.differential_0;
            }
            else
            {
                _S814 = v_normal_2;
            }
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S855;
            (&_S855)->primal_0 = _S815;
            (&_S855)->differential_0 = _S809;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S856;
            (&_S856)->primal_0 = _S815;
            (&_S856)->differential_0 = _S809;
            s_bwd_prop_dot_0(&_S855, &_S856, 0.0f);
            float3  _S857 = _S856.differential_0 + _S855.differential_0 + _S814;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S858;
            (&_S858)->primal_0 = _S817;
            (&_S858)->differential_0 = _S809;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S859;
            (&_S859)->primal_0 = _S818;
            (&_S859)->differential_0 = _S809;
            s_bwd_prop_cross_0(&_S858, &_S859, _S857);
            float3  s_diff_dy_T_3 = - _S859.differential_0;
            float3  _S860 = - s_diff_dy_T_3;
            float3  _S861 = - _S858.differential_0;
            FixedArray<float3 , 4>  _S862;
            _S862[int(0)] = _S809;
            _S862[int(1)] = _S809;
            _S862[int(2)] = _S809;
            _S862[int(3)] = _S809;
            _S862[int(2)] = _S860;
            _S862[int(3)] = s_diff_dy_T_3;
            _S862[int(0)] = _S861;
            _S862[int(1)] = _S858.differential_0;
            points_7[int(0)] = _S862[int(0)];
            points_7[int(1)] = _S862[int(1)];
            points_7[int(2)] = _S862[int(2)];
            points_7[int(3)] = _S862[int(3)];
        }
        else
        {
            points_7[int(0)] = _S809;
            points_7[int(1)] = _S809;
            points_7[int(2)] = _S809;
            points_7[int(3)] = _S809;
        }
        if(_runFlag_13)
        {
            if(_S811)
            {
                if(_runFlag_14)
                {
                    FixedArray<float3 , 4>  _S863 = points_7;
                    FixedArray<float3 , 4>  _S864 = points_7;
                    FixedArray<float3 , 4>  _S865 = points_7;
                    FixedArray<float3 , 4>  _S866 = points_7;
                    if(_S812)
                    {
                        float3  _S867 = _S819 * _S866[int(3)];
                        float _S868 = _S867.x + _S867.y + _S867.z;
                        float4  _S869 = _S848;
                        *&((&_S869)->w) = _S868;
                        points_7[int(0)] = _S863[int(0)];
                        points_7[int(1)] = _S864[int(1)];
                        points_7[int(2)] = _S865[int(2)];
                        points_7[int(3)] = _S809;
                        _S849 = _S869;
                    }
                    else
                    {
                        points_7[int(0)] = _S863[int(0)];
                        points_7[int(1)] = _S864[int(1)];
                        points_7[int(2)] = _S865[int(2)];
                        points_7[int(3)] = _S866[int(3)];
                        _S849 = _S848;
                    }
                    float3  _S870 = _S820 * points_7[int(2)];
                    float _S871 = _S870.x + _S870.y + _S870.z;
                    FixedArray<float3 , 4>  _S872 = points_7;
                    FixedArray<float3 , 4>  _S873 = points_7;
                    float4  _S874 = _S848;
                    *&((&_S874)->z) = _S871;
                    float4  _S875 = _S849 + _S874;
                    points_7[int(0)] = points_7[int(0)];
                    points_7[int(1)] = _S872[int(1)];
                    points_7[int(2)] = _S809;
                    points_7[int(3)] = _S873[int(3)];
                    _S849 = _S875;
                }
                else
                {
                    FixedArray<float3 , 4>  _S876 = points_7;
                    FixedArray<float3 , 4>  _S877 = points_7;
                    FixedArray<float3 , 4>  _S878 = points_7;
                    points_7[int(0)] = points_7[int(0)];
                    points_7[int(1)] = _S876[int(1)];
                    points_7[int(2)] = _S877[int(2)];
                    points_7[int(3)] = _S878[int(3)];
                    _S849 = _S848;
                }
            }
            else
            {
                FixedArray<float3 , 4>  _S879 = points_7;
                FixedArray<float3 , 4>  _S880 = points_7;
                FixedArray<float3 , 4>  _S881 = points_7;
                points_7[int(0)] = points_7[int(0)];
                points_7[int(1)] = _S879[int(1)];
                points_7[int(2)] = _S880[int(2)];
                points_7[int(3)] = _S881[int(3)];
                _S849 = _S848;
            }
            if(_S813)
            {
                FixedArray<float3 , 4>  _S882 = points_7;
                float3  _S883 = _S821 * points_7[int(1)];
                float _S884 = _S883.x + _S883.y + _S883.z;
                float4  _S885 = _S848;
                *&((&_S885)->y) = _S884;
                float4  _S886 = _S849 + _S885;
                points_7[int(0)] = _S809;
                points_7[int(1)] = _S809;
                points_7[int(2)] = _S809;
                points_7[int(3)] = _S809;
                _S814 = _S882[int(0)];
                _S849 = _S886;
            }
            else
            {
                FixedArray<float3 , 4>  _S887 = points_7;
                FixedArray<float3 , 4>  _S888 = points_7;
                FixedArray<float3 , 4>  _S889 = points_7;
                points_7[int(0)] = points_7[int(0)];
                points_7[int(1)] = _S887[int(1)];
                points_7[int(2)] = _S888[int(2)];
                points_7[int(3)] = _S889[int(3)];
                _S814 = _S809;
            }
            float3  _S890 = _S822 * (points_7[int(0)] + _S814);
            float _S891 = _S890.x + _S890.y + _S890.z;
            float4  _S892 = _S848;
            *&((&_S892)->x) = _S891;
            _S849 = _S849 + _S892;
        }
        else
        {
            _S849 = _S848;
        }
    }
    else
    {
        _S849 = _S848;
    }
    *v_depths_2 = _S849;
    return;
}

inline __device__ float ray_depth_to_linear_depth_factor_opencv(float2  pix_center_7, float4  intrins_13, FixedArray<float, 4>  dist_coeffs_17, int camera_model_15)
{
    float _S893;
    for(;;)
    {
        float2  uv_41 = (pix_center_7 - float2 {intrins_13.z, intrins_13.w}) / float2 {intrins_13.x, intrins_13.y};
        FixedArray<float, 4>  _S894 = dist_coeffs_17;
        float2  uv_u_18;
        bool _S895 = undistort_point_1(uv_41, &_S894, int(12), &uv_u_18);
        if(!_S895)
        {
            _S893 = 0.0f;
            break;
        }
        float3  raydir_9 = unproject_raydir_0(uv_u_18, camera_model_15, false);
        _S893 = float((F32_sign((raydir_9.z)))) / length_0(raydir_9);
        break;
    }
    return _S893;
}

inline __device__ float depth_normal_loss_opencv(float2  pix_center_8, float4  intrins_14, FixedArray<float, 4>  dist_coeffs_18, int camera_model_16, bool is_ray_depth_14, float4  depths_6, float3  gt_normal_2)
{
    float _S896;
    for(;;)
    {
        float3  _S897;
        float3  * _S898;
        float3  * _S899;
        float3  * _S900;
        float3  * _S901;
        int _S902;
        FixedArray<float3 , 5>  points_8;
        for(;;)
        {
            float2  _S903 = float2 {intrins_14.z, intrins_14.w};
            float2  _S904 = float2 {intrins_14.x, intrins_14.y};
            float2  uv_42 = (pix_center_8 + make_float2 (-1.0f, -0.0f) - _S903) / _S904;
            FixedArray<float, 4>  _S905 = dist_coeffs_18;
            float2  uv_u_19;
            bool _S906 = undistort_point_1(uv_42, &_S905, int(12), &uv_u_19);
            float3  _S907 = make_float3 (0.0f);
            if(!_S906)
            {
                _S902 = int(0);
                _S901 = nullptr;
                _S900 = nullptr;
                _S899 = nullptr;
                _S898 = nullptr;
                _S897 = _S907;
                break;
            }
            float3  raydir_10 = unproject_raydir_0(uv_u_19, camera_model_16, is_ray_depth_14);
            points_8[int(0)] = make_float3 (depths_6.x) * raydir_10;
            float2  uv_43 = (pix_center_8 + make_float2 (1.0f, -0.0f) - _S903) / _S904;
            FixedArray<float, 4>  _S908 = dist_coeffs_18;
            float2  uv_u_20;
            bool _S909 = undistort_point_1(uv_43, &_S908, int(12), &uv_u_20);
            if(!_S909)
            {
                _S902 = int(0);
                _S901 = nullptr;
                _S900 = &points_8[int(0)];
                _S899 = nullptr;
                _S898 = nullptr;
                _S897 = _S907;
                break;
            }
            float3  raydir_11 = unproject_raydir_0(uv_u_20, camera_model_16, is_ray_depth_14);
            points_8[int(1)] = make_float3 (depths_6.y) * raydir_11;
            float2  uv_44 = (pix_center_8 + make_float2 (0.0f, -1.0f) - _S903) / _S904;
            FixedArray<float, 4>  _S910 = dist_coeffs_18;
            float2  uv_u_21;
            bool _S911 = undistort_point_1(uv_44, &_S910, int(12), &uv_u_21);
            if(!_S911)
            {
                _S902 = int(0);
                _S901 = &points_8[int(1)];
                _S900 = &points_8[int(0)];
                _S899 = nullptr;
                _S898 = nullptr;
                _S897 = _S907;
                break;
            }
            float3  raydir_12 = unproject_raydir_0(uv_u_21, camera_model_16, is_ray_depth_14);
            points_8[int(2)] = make_float3 (depths_6.z) * raydir_12;
            float2  uv_45 = (pix_center_8 + make_float2 (0.0f, 1.0f) - _S903) / _S904;
            FixedArray<float, 4>  _S912 = dist_coeffs_18;
            float2  uv_u_22;
            bool _S913 = undistort_point_1(uv_45, &_S912, int(12), &uv_u_22);
            if(!_S913)
            {
                _S902 = int(0);
                _S901 = &points_8[int(1)];
                _S900 = &points_8[int(0)];
                _S899 = nullptr;
                _S898 = &points_8[int(2)];
                _S897 = _S907;
                break;
            }
            float3  raydir_13 = unproject_raydir_0(uv_u_22, camera_model_16, is_ray_depth_14);
            points_8[int(3)] = make_float3 (depths_6.w) * raydir_13;
            float2  uv_46 = (pix_center_8 + make_float2 (0.0f) * make_float2 (0.0f, 3.0f) - _S903) / _S904;
            FixedArray<float, 4>  _S914 = dist_coeffs_18;
            float2  uv_u_23;
            bool _S915 = undistort_point_1(uv_46, &_S914, int(12), &uv_u_23);
            if(!_S915)
            {
                _S902 = int(0);
                _S901 = &points_8[int(1)];
                _S900 = &points_8[int(0)];
                _S899 = &points_8[int(3)];
                _S898 = &points_8[int(2)];
                _S897 = _S907;
                break;
            }
            float3  raydir_14 = unproject_raydir_0(uv_u_23, camera_model_16, is_ray_depth_14);
            _S902 = int(1);
            _S901 = &points_8[int(1)];
            _S900 = &points_8[int(0)];
            _S899 = &points_8[int(3)];
            _S898 = &points_8[int(2)];
            _S897 = raydir_14;
            break;
        }
        if(_S902 != int(1))
        {
            _S896 = 0.0f;
            break;
        }
        float3  normal_9 = cross_0(*_S901 - *_S900, - (*_S899 - *_S898));
        float3  normal_10;
        if((dot_0(normal_9, normal_9)) != 0.0f)
        {
            normal_10 = normalize_0(normal_9);
        }
        else
        {
            normal_10 = normal_9;
        }
        float3  _S916;
        if((dot_0(gt_normal_2, gt_normal_2)) != 0.0f)
        {
            _S916 = normalize_0(gt_normal_2);
        }
        else
        {
            _S916 = gt_normal_2;
        }
        _S896 = (1.0f - dot_0(normal_10, _S916) + 0.00100000004749745f) / ((F32_max((dot_0(normal_10, - normalize_0(_S897))), (0.0f))) + 0.00100000004749745f);
        break;
    }
    return _S896;
}

struct s_bwd_prop_depth_normal_loss_Intermediates_1
{
    float2  _S917;
    bool _S918;
    float2  _S919;
    bool _S920;
    float2  _S921;
    bool _S922;
    float2  _S923;
    bool _S924;
    float2  _S925;
    bool _S926;
};

inline __device__ void depth_normal_loss_vjp_opencv(float2  pix_center_9, float4  intrins_15, FixedArray<float, 4>  dist_coeffs_19, int camera_model_17, bool is_ray_depth_15, float4  depths_7, float3  gt_normal_3, float v_loss_1, float4  * v_depths_3, float3  * v_gt_normal_1)
{
    float2  _S927 = make_float2 (0.0f);
    s_bwd_prop_depth_normal_loss_Intermediates_1 _S928;
    (&_S928)->_S917 = _S927;
    (&_S928)->_S918 = false;
    (&_S928)->_S919 = _S927;
    (&_S928)->_S920 = false;
    (&_S928)->_S921 = _S927;
    (&_S928)->_S922 = false;
    (&_S928)->_S923 = _S927;
    (&_S928)->_S924 = false;
    (&_S928)->_S925 = _S927;
    (&_S928)->_S926 = false;
    (&_S928)->_S919 = _S927;
    (&_S928)->_S920 = false;
    (&_S928)->_S921 = _S927;
    (&_S928)->_S922 = false;
    (&_S928)->_S923 = _S927;
    (&_S928)->_S924 = false;
    (&_S928)->_S925 = _S927;
    (&_S928)->_S926 = false;
    float2  _S929 = float2 {intrins_15.z, intrins_15.w};
    float2  _S930 = float2 {intrins_15.x, intrins_15.y};
    float2  uv_47 = (pix_center_9 + make_float2 (-1.0f, -0.0f) - _S929) / _S930;
    float2  _S931 = _S927;
    FixedArray<float, 4>  _S932 = dist_coeffs_19;
    bool _S933 = undistort_point_1(uv_47, &_S932, int(12), &_S931);
    (&_S928)->_S917 = _S931;
    (&_S928)->_S918 = _S933;
    bool _S934 = !!_S933;
    bool _runFlag_15;
    if(_S934)
    {
        float2  uv_48 = (pix_center_9 + make_float2 (1.0f, -0.0f) - _S929) / _S930;
        float2  _S935 = _S927;
        FixedArray<float, 4>  _S936 = dist_coeffs_19;
        bool _S937 = undistort_point_1(uv_48, &_S936, int(12), &_S935);
        (&_S928)->_S919 = _S935;
        (&_S928)->_S920 = _S937;
        if(!_S937)
        {
            _runFlag_15 = false;
        }
        else
        {
            _runFlag_15 = _S934;
        }
        if(_runFlag_15)
        {
            float2  uv_49 = (pix_center_9 + make_float2 (0.0f, -1.0f) - _S929) / _S930;
            float2  _S938 = _S927;
            FixedArray<float, 4>  _S939 = dist_coeffs_19;
            bool _S940 = undistort_point_1(uv_49, &_S939, int(12), &_S938);
            (&_S928)->_S921 = _S938;
            (&_S928)->_S922 = _S940;
            if(!_S940)
            {
                _runFlag_15 = false;
            }
            if(_runFlag_15)
            {
                float2  uv_50 = (pix_center_9 + make_float2 (0.0f, 1.0f) - _S929) / _S930;
                float2  _S941 = _S927;
                FixedArray<float, 4>  _S942 = dist_coeffs_19;
                bool _S943 = undistort_point_1(uv_50, &_S942, int(12), &_S941);
                (&_S928)->_S923 = _S941;
                (&_S928)->_S924 = _S943;
                if(!_S943)
                {
                    _runFlag_15 = false;
                }
                if(_runFlag_15)
                {
                    float2  uv_51 = (pix_center_9 - _S929) / _S930;
                    float2  _S944 = _S927;
                    FixedArray<float, 4>  _S945 = dist_coeffs_19;
                    bool _S946 = undistort_point_1(uv_51, &_S945, int(12), &_S944);
                    (&_S928)->_S925 = _S944;
                    (&_S928)->_S926 = _S946;
                }
            }
        }
    }
    s_bwd_prop_depth_normal_loss_Intermediates_1 _S947 = _S928;
    float3  _S948 = make_float3 (0.0f);
    bool _S949 = !!_S928._S918;
    bool _runFlag_16;
    bool _runFlag_17;
    bool _runFlag_18;
    int _S950;
    float3  raydir_15;
    float3  _S951;
    float3  _S952;
    float3  _S953;
    float3  _S954;
    FixedArray<float3 , 5>  points_9;
    if(_S949)
    {
        float3  _S955 = s_primal_ctx_unproject_raydir_0(_S947._S917, camera_model_17, is_ray_depth_15);
        float3  _S956 = make_float3 (depths_7.x) * _S955;
        if(!_S947._S920)
        {
            _runFlag_15 = false;
        }
        else
        {
            _runFlag_15 = _S949;
        }
        if(_runFlag_15)
        {
            float3  _S957 = s_primal_ctx_unproject_raydir_0(_S947._S919, camera_model_17, is_ray_depth_15);
            float3  _S958 = make_float3 (depths_7.y) * _S957;
            if(!_S947._S922)
            {
                _runFlag_16 = false;
            }
            else
            {
                _runFlag_16 = _runFlag_15;
            }
            if(_runFlag_16)
            {
                float3  _S959 = s_primal_ctx_unproject_raydir_0(_S947._S921, camera_model_17, is_ray_depth_15);
                float3  _S960 = make_float3 (depths_7.z) * _S959;
                if(!_S947._S924)
                {
                    _runFlag_17 = false;
                }
                else
                {
                    _runFlag_17 = _runFlag_16;
                }
                if(_runFlag_17)
                {
                    float3  _S961 = s_primal_ctx_unproject_raydir_0(_S947._S923, camera_model_17, is_ray_depth_15);
                    float3  _S962 = make_float3 (depths_7.w) * _S961;
                    if(!_S947._S926)
                    {
                        _runFlag_18 = false;
                    }
                    else
                    {
                        _runFlag_18 = _runFlag_17;
                    }
                    if(_runFlag_18)
                    {
                        float3  _S963 = s_primal_ctx_unproject_raydir_0(_S947._S925, camera_model_17, is_ray_depth_15);
                        _S950 = int(1);
                        raydir_15 = _S963;
                    }
                    else
                    {
                        _S950 = int(0);
                        raydir_15 = _S961;
                    }
                    points_9[int(0)] = _S956;
                    points_9[int(1)] = _S958;
                    points_9[int(2)] = _S960;
                    points_9[int(3)] = _S962;
                    points_9[int(4)] = _S948;
                    _S951 = _S961;
                }
                else
                {
                    _S950 = int(0);
                    raydir_15 = _S959;
                    points_9[int(0)] = _S956;
                    points_9[int(1)] = _S958;
                    points_9[int(2)] = _S960;
                    points_9[int(3)] = _S948;
                    points_9[int(4)] = _S948;
                    _S951 = _S948;
                }
                _S952 = _S959;
            }
            else
            {
                _S950 = int(0);
                raydir_15 = _S957;
                points_9[int(0)] = _S956;
                points_9[int(1)] = _S958;
                points_9[int(2)] = _S948;
                points_9[int(3)] = _S948;
                points_9[int(4)] = _S948;
                _runFlag_17 = false;
                _S951 = _S948;
                _S952 = _S948;
            }
            _S953 = _S957;
        }
        else
        {
            _S950 = int(0);
            raydir_15 = _S955;
            points_9[int(0)] = _S956;
            points_9[int(1)] = _S948;
            points_9[int(2)] = _S948;
            points_9[int(3)] = _S948;
            points_9[int(4)] = _S948;
            _runFlag_16 = false;
            _runFlag_17 = false;
            _S951 = _S948;
            _S952 = _S948;
            _S953 = _S948;
        }
        _S954 = _S955;
    }
    else
    {
        _S950 = int(0);
        points_9[int(0)] = _S948;
        points_9[int(1)] = _S948;
        points_9[int(2)] = _S948;
        points_9[int(3)] = _S948;
        points_9[int(4)] = _S948;
        _runFlag_15 = false;
        _runFlag_16 = false;
        _runFlag_17 = false;
        _S951 = _S948;
        _S952 = _S948;
        _S953 = _S948;
        _S954 = _S948;
    }
    bool _S964 = !(_S950 != int(1));
    bool _S965;
    float3  normal_11;
    float3  _S966;
    float3  _S967;
    float3  _S968;
    float3  _S969;
    float _S970;
    float _S971;
    float _S972;
    float _S973;
    if(_S964)
    {
        float3  dx_4 = points_9[int(1)] - points_9[int(0)];
        float3  _S974 = - (points_9[int(3)] - points_9[int(2)]);
        float3  _S975 = s_primal_ctx_cross_0(dx_4, _S974);
        bool _S976 = (s_primal_ctx_dot_0(_S975, _S975)) != 0.0f;
        if(_S976)
        {
            normal_11 = normalize_0(_S975);
        }
        else
        {
            normal_11 = _S975;
        }
        bool _S977 = (s_primal_ctx_dot_0(gt_normal_3, gt_normal_3)) != 0.0f;
        if(_S977)
        {
            _S966 = normalize_0(gt_normal_3);
        }
        else
        {
            _S966 = gt_normal_3;
        }
        float3  _S978 = - normalize_0(raydir_15);
        float _S979 = s_primal_ctx_dot_0(normal_11, _S978);
        float _S980 = 1.0f - s_primal_ctx_dot_0(normal_11, _S966) + 0.00100000004749745f;
        float _S981 = (F32_max((_S979), (0.0f))) + 0.00100000004749745f;
        _S970 = _S981 * _S981;
        _S971 = _S980;
        _S972 = _S981;
        _S973 = _S979;
        raydir_15 = normal_11;
        normal_11 = _S978;
        _runFlag_18 = _S977;
        _S965 = _S976;
        _S967 = _S975;
        _S968 = dx_4;
        _S969 = _S974;
    }
    else
    {
        _S970 = 0.0f;
        _S971 = 0.0f;
        _S972 = 0.0f;
        _S973 = 0.0f;
        raydir_15 = _S948;
        normal_11 = _S948;
        _S966 = _S948;
        _runFlag_18 = false;
        _S965 = false;
        _S967 = _S948;
        _S968 = _S948;
        _S969 = _S948;
    }
    float4  _S982 = make_float4 (0.0f);
    if(_S964)
    {
        float _S983 = v_loss_1 / _S970;
        float _S984 = _S971 * - _S983;
        float s_diff_num_T_1 = _S972 * _S983;
        DiffPair_float_0 _S985;
        (&_S985)->primal_0 = _S973;
        (&_S985)->differential_0 = 0.0f;
        DiffPair_float_0 _S986;
        (&_S986)->primal_0 = 0.0f;
        (&_S986)->differential_0 = 0.0f;
        _d_max_0(&_S985, &_S986, _S984);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S987;
        (&_S987)->primal_0 = raydir_15;
        (&_S987)->differential_0 = _S948;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S988;
        (&_S988)->primal_0 = normal_11;
        (&_S988)->differential_0 = _S948;
        s_bwd_prop_dot_0(&_S987, &_S988, _S985.differential_0);
        float _S989 = - s_diff_num_T_1;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S990;
        (&_S990)->primal_0 = raydir_15;
        (&_S990)->differential_0 = _S948;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S991;
        (&_S991)->primal_0 = _S966;
        (&_S991)->differential_0 = _S948;
        s_bwd_prop_dot_0(&_S990, &_S991, _S989);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S992 = _S991;
        float3  _S993 = _S987.differential_0 + _S990.differential_0;
        if(_runFlag_18)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S994;
            (&_S994)->primal_0 = gt_normal_3;
            (&_S994)->differential_0 = _S948;
            s_bwd_normalize_impl_0(&_S994, _S992.differential_0);
            raydir_15 = _S994.differential_0;
        }
        else
        {
            raydir_15 = _S992.differential_0;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S995;
        (&_S995)->primal_0 = gt_normal_3;
        (&_S995)->differential_0 = _S948;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S996;
        (&_S996)->primal_0 = gt_normal_3;
        (&_S996)->differential_0 = _S948;
        s_bwd_prop_dot_0(&_S995, &_S996, 0.0f);
        float3  _S997 = _S996.differential_0 + _S995.differential_0 + raydir_15;
        if(_S965)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S998;
            (&_S998)->primal_0 = _S967;
            (&_S998)->differential_0 = _S948;
            s_bwd_normalize_impl_0(&_S998, _S993);
            raydir_15 = _S998.differential_0;
        }
        else
        {
            raydir_15 = _S993;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S999;
        (&_S999)->primal_0 = _S967;
        (&_S999)->differential_0 = _S948;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1000;
        (&_S1000)->primal_0 = _S967;
        (&_S1000)->differential_0 = _S948;
        s_bwd_prop_dot_0(&_S999, &_S1000, 0.0f);
        float3  _S1001 = _S1000.differential_0 + _S999.differential_0 + raydir_15;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1002;
        (&_S1002)->primal_0 = _S968;
        (&_S1002)->differential_0 = _S948;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1003;
        (&_S1003)->primal_0 = _S969;
        (&_S1003)->differential_0 = _S948;
        s_bwd_prop_cross_0(&_S1002, &_S1003, _S1001);
        float3  s_diff_dy_T_4 = - _S1003.differential_0;
        float3  _S1004 = - s_diff_dy_T_4;
        float3  _S1005 = - _S1002.differential_0;
        FixedArray<float3 , 5>  _S1006;
        _S1006[int(0)] = _S948;
        _S1006[int(1)] = _S948;
        _S1006[int(2)] = _S948;
        _S1006[int(3)] = _S948;
        _S1006[int(4)] = _S948;
        _S1006[int(2)] = _S1004;
        _S1006[int(3)] = s_diff_dy_T_4;
        _S1006[int(0)] = _S1005;
        _S1006[int(1)] = _S1002.differential_0;
        points_9[int(0)] = _S1006[int(0)];
        points_9[int(1)] = _S1006[int(1)];
        points_9[int(2)] = _S1006[int(2)];
        points_9[int(3)] = _S1006[int(3)];
        points_9[int(4)] = _S1006[int(4)];
        raydir_15 = _S997;
    }
    else
    {
        points_9[int(0)] = _S948;
        points_9[int(1)] = _S948;
        points_9[int(2)] = _S948;
        points_9[int(3)] = _S948;
        points_9[int(4)] = _S948;
        raydir_15 = _S948;
    }
    float4  _S1007;
    if(_S949)
    {
        if(_runFlag_15)
        {
            if(_runFlag_16)
            {
                if(_runFlag_17)
                {
                    FixedArray<float3 , 5>  _S1008 = points_9;
                    FixedArray<float3 , 5>  _S1009 = points_9;
                    FixedArray<float3 , 5>  _S1010 = points_9;
                    float3  _S1011 = _S951 * points_9[int(3)];
                    float _S1012 = _S1011.x + _S1011.y + _S1011.z;
                    float4  _S1013 = _S982;
                    *&((&_S1013)->w) = _S1012;
                    points_9[int(0)] = _S948;
                    points_9[int(1)] = _S948;
                    points_9[int(2)] = _S948;
                    points_9[int(3)] = _S948;
                    points_9[int(4)] = _S948;
                    _S951 = _S1010[int(2)];
                    normal_11 = _S1008[int(0)];
                    _S966 = _S1009[int(1)];
                    _S1007 = _S1013;
                }
                else
                {
                    FixedArray<float3 , 5>  _S1014 = points_9;
                    FixedArray<float3 , 5>  _S1015 = points_9;
                    FixedArray<float3 , 5>  _S1016 = points_9;
                    FixedArray<float3 , 5>  _S1017 = points_9;
                    points_9[int(0)] = points_9[int(0)];
                    points_9[int(1)] = _S1014[int(1)];
                    points_9[int(2)] = _S1015[int(2)];
                    points_9[int(3)] = _S1016[int(3)];
                    points_9[int(4)] = _S1017[int(4)];
                    _S951 = _S948;
                    normal_11 = _S948;
                    _S966 = _S948;
                    _S1007 = _S982;
                }
                float3  _S1018 = _S952 * (points_9[int(2)] + _S951);
                float _S1019 = _S1018.x + _S1018.y + _S1018.z;
                float3  _S1020 = points_9[int(0)] + normal_11;
                float3  _S1021 = points_9[int(1)] + _S966;
                float4  _S1022 = _S982;
                *&((&_S1022)->z) = _S1019;
                float4  _S1023 = _S1007 + _S1022;
                points_9[int(0)] = _S948;
                points_9[int(1)] = _S948;
                points_9[int(2)] = _S948;
                points_9[int(3)] = _S948;
                points_9[int(4)] = _S948;
                _S951 = _S1021;
                _S952 = _S1020;
                _S1007 = _S1023;
            }
            else
            {
                FixedArray<float3 , 5>  _S1024 = points_9;
                FixedArray<float3 , 5>  _S1025 = points_9;
                FixedArray<float3 , 5>  _S1026 = points_9;
                FixedArray<float3 , 5>  _S1027 = points_9;
                points_9[int(0)] = points_9[int(0)];
                points_9[int(1)] = _S1024[int(1)];
                points_9[int(2)] = _S1025[int(2)];
                points_9[int(3)] = _S1026[int(3)];
                points_9[int(4)] = _S1027[int(4)];
                _S951 = _S948;
                _S952 = _S948;
                _S1007 = _S982;
            }
            float3  _S1028 = _S953 * (points_9[int(1)] + _S951);
            float _S1029 = _S1028.x + _S1028.y + _S1028.z;
            float3  _S1030 = points_9[int(0)] + _S952;
            float4  _S1031 = _S982;
            *&((&_S1031)->y) = _S1029;
            float4  _S1032 = _S1007 + _S1031;
            points_9[int(0)] = _S948;
            points_9[int(1)] = _S948;
            points_9[int(2)] = _S948;
            points_9[int(3)] = _S948;
            points_9[int(4)] = _S948;
            _S951 = _S1030;
            _S1007 = _S1032;
        }
        else
        {
            FixedArray<float3 , 5>  _S1033 = points_9;
            FixedArray<float3 , 5>  _S1034 = points_9;
            FixedArray<float3 , 5>  _S1035 = points_9;
            FixedArray<float3 , 5>  _S1036 = points_9;
            points_9[int(0)] = points_9[int(0)];
            points_9[int(1)] = _S1033[int(1)];
            points_9[int(2)] = _S1034[int(2)];
            points_9[int(3)] = _S1035[int(3)];
            points_9[int(4)] = _S1036[int(4)];
            _S951 = _S948;
            _S1007 = _S982;
        }
        float3  _S1037 = _S954 * (points_9[int(0)] + _S951);
        float _S1038 = _S1037.x + _S1037.y + _S1037.z;
        float4  _S1039 = _S982;
        *&((&_S1039)->x) = _S1038;
        _S1007 = _S1007 + _S1039;
    }
    else
    {
        _S1007 = _S982;
    }
    *v_depths_3 = _S1007;
    *v_gt_normal_1 = raydir_15;
    return;
}

inline __device__ float3  generate_ray_d2n_prism(float2  pix_pos_6, float4  intrins_16, FixedArray<float, 8>  dist_coeffs_20, int camera_model_18, bool is_ray_depth_16)
{
    float3  _S1040;
    for(;;)
    {
        float2  uv_52 = (pix_pos_6 - float2 {intrins_16.z, intrins_16.w}) / float2 {intrins_16.x, intrins_16.y};
        FixedArray<float, 8>  _S1041 = dist_coeffs_20;
        float2  uv_u_24;
        bool _S1042 = undistort_point_2(uv_52, &_S1041, int(12), &uv_u_24);
        if(!_S1042)
        {
            int3  _S1043 = make_int3 (int(0));
            float3  _S1044 = make_float3 ((float)_S1043.x, (float)_S1043.y, (float)_S1043.z);
            _S1040 = _S1044;
            break;
        }
        _S1040 = unproject_raydir_0(uv_u_24, camera_model_18, is_ray_depth_16);
        break;
    }
    return _S1040;
}

inline __device__ float3  depth_to_point_prism(float2  pix_pos_7, float4  intrins_17, FixedArray<float, 8>  dist_coeffs_21, int camera_model_19, bool is_ray_depth_17, float depth_6)
{
    float3  _S1045;
    for(;;)
    {
        float2  uv_53 = (pix_pos_7 - float2 {intrins_17.z, intrins_17.w}) / float2 {intrins_17.x, intrins_17.y};
        FixedArray<float, 8>  _S1046 = dist_coeffs_21;
        float2  uv_u_25;
        bool _S1047 = undistort_point_2(uv_53, &_S1046, int(12), &uv_u_25);
        if(!_S1047)
        {
            _S1045 = make_float3 (0.0f);
            break;
        }
        _S1045 = make_float3 (depth_6) * unproject_raydir_0(uv_u_25, camera_model_19, is_ray_depth_17);
        break;
    }
    return _S1045;
}

struct s_bwd_prop_depth_to_point_Intermediates_2
{
    float2  _S1048;
    bool _S1049;
};

inline __device__ float depth_to_point_vjp_prism(float2  pix_pos_8, float4  intrins_18, FixedArray<float, 8>  dist_coeffs_22, int camera_model_20, bool is_ray_depth_18, float depth_7, float3  v_point_2)
{
    float2  _S1050 = make_float2 (0.0f);
    s_bwd_prop_depth_to_point_Intermediates_2 _S1051;
    (&_S1051)->_S1048 = _S1050;
    (&_S1051)->_S1049 = false;
    float2  uv_54 = (pix_pos_8 - float2 {intrins_18.z, intrins_18.w}) / float2 {intrins_18.x, intrins_18.y};
    float2  _S1052 = _S1050;
    FixedArray<float, 8>  _S1053 = dist_coeffs_22;
    bool _S1054 = undistort_point_2(uv_54, &_S1053, int(12), &_S1052);
    (&_S1051)->_S1048 = _S1052;
    (&_S1051)->_S1049 = _S1054;
    s_bwd_prop_depth_to_point_Intermediates_2 _S1055 = _S1051;
    float3  _S1056 = make_float3 (0.0f);
    bool _S1057 = !!_S1051._S1049;
    float3  _S1058;
    if(_S1057)
    {
        _S1058 = s_primal_ctx_unproject_raydir_0(_S1055._S1048, camera_model_20, is_ray_depth_18);
    }
    else
    {
        _S1058 = _S1056;
    }
    if(_S1057)
    {
        _S1058 = _S1058 * v_point_2;
    }
    else
    {
        _S1058 = _S1056;
    }
    return _S1058.x + _S1058.y + _S1058.z;
}

inline __device__ float3  depth_to_normal_prism(float2  pix_center_10, float4  intrins_19, FixedArray<float, 8>  dist_coeffs_23, int camera_model_21, bool is_ray_depth_19, float4  depths_8)
{
    float3  normal_12;
    for(;;)
    {
        bool _S1059;
        if((depths_8.x) == 0.0f)
        {
            _S1059 = true;
        }
        else
        {
            _S1059 = (depths_8.y) == 0.0f;
        }
        if(_S1059)
        {
            _S1059 = true;
        }
        else
        {
            _S1059 = (depths_8.z) == 0.0f;
        }
        if(_S1059)
        {
            _S1059 = true;
        }
        else
        {
            _S1059 = (depths_8.w) == 0.0f;
        }
        if(_S1059)
        {
            normal_12 = make_float3 (0.0f);
            break;
        }
        float3  * _S1060;
        float3  * _S1061;
        float3  * _S1062;
        float3  * _S1063;
        int _S1064;
        FixedArray<float3 , 4>  points_10;
        for(;;)
        {
            float2  _S1065 = float2 {intrins_19.z, intrins_19.w};
            float2  _S1066 = float2 {intrins_19.x, intrins_19.y};
            float2  uv_55 = (pix_center_10 + make_float2 (-1.0f, -0.0f) - _S1065) / _S1066;
            FixedArray<float, 8>  _S1067 = dist_coeffs_23;
            float2  uv_u_26;
            bool _S1068 = undistort_point_2(uv_55, &_S1067, int(12), &uv_u_26);
            if(!_S1068)
            {
                float3  _S1069 = make_float3 (0.0f);
                _S1064 = int(0);
                _S1063 = nullptr;
                _S1062 = nullptr;
                _S1061 = nullptr;
                _S1060 = nullptr;
                normal_12 = _S1069;
                break;
            }
            points_10[int(0)] = make_float3 (depths_8.x) * unproject_raydir_0(uv_u_26, camera_model_21, is_ray_depth_19);
            for(;;)
            {
                float2  uv_56 = (pix_center_10 + make_float2 (1.0f, -0.0f) - _S1065) / _S1066;
                FixedArray<float, 8>  _S1070 = dist_coeffs_23;
                float2  uv_u_27;
                bool _S1071 = undistort_point_2(uv_56, &_S1070, int(12), &uv_u_27);
                if(!_S1071)
                {
                    float3  _S1072 = make_float3 (0.0f);
                    _S1064 = int(0);
                    _S1063 = nullptr;
                    normal_12 = _S1072;
                    break;
                }
                points_10[int(1)] = make_float3 (depths_8.y) * unproject_raydir_0(uv_u_27, camera_model_21, is_ray_depth_19);
                _S1064 = int(2);
                _S1063 = &points_10[int(1)];
                break;
            }
            if(_S1064 != int(2))
            {
                _S1062 = &points_10[int(0)];
                _S1061 = nullptr;
                _S1060 = nullptr;
                break;
            }
            float2  uv_57 = (pix_center_10 + make_float2 (0.0f, -1.0f) - _S1065) / _S1066;
            FixedArray<float, 8>  _S1073 = dist_coeffs_23;
            float2  uv_u_28;
            bool _S1074 = undistort_point_2(uv_57, &_S1073, int(12), &uv_u_28);
            if(!_S1074)
            {
                float3  _S1075 = make_float3 (0.0f);
                _S1064 = int(0);
                _S1062 = &points_10[int(0)];
                _S1061 = nullptr;
                _S1060 = nullptr;
                normal_12 = _S1075;
                break;
            }
            points_10[int(2)] = make_float3 (depths_8.z) * unproject_raydir_0(uv_u_28, camera_model_21, is_ray_depth_19);
            for(;;)
            {
                float2  uv_58 = (pix_center_10 + make_float2 (0.0f, 1.0f) - _S1065) / _S1066;
                FixedArray<float, 8>  _S1076 = dist_coeffs_23;
                float2  uv_u_29;
                bool _S1077 = undistort_point_2(uv_58, &_S1076, int(12), &uv_u_29);
                if(!_S1077)
                {
                    float3  _S1078 = make_float3 (0.0f);
                    _S1064 = int(0);
                    _S1062 = nullptr;
                    normal_12 = _S1078;
                    break;
                }
                points_10[int(3)] = make_float3 (depths_8.w) * unproject_raydir_0(uv_u_29, camera_model_21, is_ray_depth_19);
                _S1064 = int(2);
                _S1062 = &points_10[int(3)];
                break;
            }
            if(_S1064 != int(2))
            {
                float3  * _S1079 = _S1062;
                _S1062 = &points_10[int(0)];
                _S1061 = _S1079;
                _S1060 = &points_10[int(2)];
                break;
            }
            float3  * _S1080 = _S1062;
            _S1064 = int(1);
            _S1062 = &points_10[int(0)];
            _S1061 = _S1080;
            _S1060 = &points_10[int(2)];
            break;
        }
        if(_S1064 != int(1))
        {
            break;
        }
        float3  normal_13 = cross_0(*_S1063 - *_S1062, - (*_S1061 - *_S1060));
        if((dot_0(normal_13, normal_13)) != 0.0f)
        {
            normal_12 = normal_13 / make_float3 (length_0(normal_13));
        }
        else
        {
            normal_12 = normal_13;
        }
        break;
    }
    return normal_12;
}

struct s_bwd_prop_depth_to_normal_Intermediates_2
{
    float2  _S1081;
    bool _S1082;
    float2  _S1083;
    bool _S1084;
    float2  _S1085;
    bool _S1086;
    float2  _S1087;
    bool _S1088;
};

inline __device__ void depth_to_normal_vjp_prism(float2  pix_center_11, float4  intrins_20, FixedArray<float, 8>  dist_coeffs_24, int camera_model_22, bool is_ray_depth_20, float4  depths_9, float3  v_normal_3, float4  * v_depths_4)
{
    float2  _S1089 = make_float2 (0.0f);
    s_bwd_prop_depth_to_normal_Intermediates_2 _S1090;
    (&_S1090)->_S1081 = _S1089;
    (&_S1090)->_S1082 = false;
    (&_S1090)->_S1083 = _S1089;
    (&_S1090)->_S1084 = false;
    (&_S1090)->_S1085 = _S1089;
    (&_S1090)->_S1086 = false;
    (&_S1090)->_S1087 = _S1089;
    (&_S1090)->_S1088 = false;
    (&_S1090)->_S1081 = _S1089;
    (&_S1090)->_S1082 = false;
    (&_S1090)->_S1083 = _S1089;
    (&_S1090)->_S1084 = false;
    (&_S1090)->_S1085 = _S1089;
    (&_S1090)->_S1086 = false;
    (&_S1090)->_S1087 = _S1089;
    (&_S1090)->_S1088 = false;
    bool _S1091 = (depths_9.x) == 0.0f;
    bool _runFlag_19;
    if(_S1091)
    {
        _runFlag_19 = true;
    }
    else
    {
        _runFlag_19 = (depths_9.y) == 0.0f;
    }
    if(_runFlag_19)
    {
        _runFlag_19 = true;
    }
    else
    {
        _runFlag_19 = (depths_9.z) == 0.0f;
    }
    if(_runFlag_19)
    {
        _runFlag_19 = true;
    }
    else
    {
        _runFlag_19 = (depths_9.w) == 0.0f;
    }
    int _S1092;
    if(!_runFlag_19)
    {
        float2  _S1093 = float2 {intrins_20.z, intrins_20.w};
        float2  _S1094 = float2 {intrins_20.x, intrins_20.y};
        float2  uv_59 = (pix_center_11 + make_float2 (-1.0f, -0.0f) - _S1093) / _S1094;
        float2  _S1095 = _S1089;
        FixedArray<float, 8>  _S1096 = dist_coeffs_24;
        bool _S1097 = undistort_point_2(uv_59, &_S1096, int(12), &_S1095);
        (&_S1090)->_S1081 = _S1095;
        (&_S1090)->_S1082 = _S1097;
        bool _S1098 = !!_S1097;
        if(_S1098)
        {
            float2  uv_60 = (pix_center_11 + make_float2 (1.0f, -0.0f) - _S1093) / _S1094;
            float2  _S1099 = _S1089;
            FixedArray<float, 8>  _S1100 = dist_coeffs_24;
            bool _S1101 = undistort_point_2(uv_60, &_S1100, int(12), &_S1099);
            (&_S1090)->_S1083 = _S1099;
            (&_S1090)->_S1084 = _S1101;
            if(!!_S1101)
            {
                _S1092 = int(2);
            }
            else
            {
                _S1092 = int(0);
            }
            if(_S1092 != int(2))
            {
                _runFlag_19 = false;
            }
            else
            {
                _runFlag_19 = _S1098;
            }
            if(_runFlag_19)
            {
                float2  uv_61 = (pix_center_11 + make_float2 (0.0f, -1.0f) - _S1093) / _S1094;
                float2  _S1102 = _S1089;
                FixedArray<float, 8>  _S1103 = dist_coeffs_24;
                bool _S1104 = undistort_point_2(uv_61, &_S1103, int(12), &_S1102);
                (&_S1090)->_S1085 = _S1102;
                (&_S1090)->_S1086 = _S1104;
                if(!_S1104)
                {
                    _runFlag_19 = false;
                }
                if(_runFlag_19)
                {
                    float2  uv_62 = (pix_center_11 + make_float2 (0.0f, 1.0f) - _S1093) / _S1094;
                    float2  _S1105 = _S1089;
                    FixedArray<float, 8>  _S1106 = dist_coeffs_24;
                    bool _S1107 = undistort_point_2(uv_62, &_S1106, int(12), &_S1105);
                    (&_S1090)->_S1087 = _S1105;
                    (&_S1090)->_S1088 = _S1107;
                }
            }
        }
    }
    s_bwd_prop_depth_to_normal_Intermediates_2 _S1108 = _S1090;
    float3  _S1109 = make_float3 (0.0f);
    if(_S1091)
    {
        _runFlag_19 = true;
    }
    else
    {
        _runFlag_19 = (depths_9.y) == 0.0f;
    }
    if(_runFlag_19)
    {
        _runFlag_19 = true;
    }
    else
    {
        _runFlag_19 = (depths_9.z) == 0.0f;
    }
    if(_runFlag_19)
    {
        _runFlag_19 = true;
    }
    else
    {
        _runFlag_19 = (depths_9.w) == 0.0f;
    }
    bool _S1110 = !_runFlag_19;
    bool _runFlag_20;
    bool _runFlag_21;
    bool _S1111;
    bool _runFlag_22;
    bool _S1112;
    bool _S1113;
    FixedArray<float3 , 4>  points_11;
    float3  _S1114;
    float3  _S1115;
    float3  _S1116;
    float3  _S1117;
    float3  _S1118;
    float3  _S1119;
    float3  _S1120;
    float3  _S1121;
    float3  _S1122;
    if(_S1110)
    {
        bool _S1123 = !!_S1108._S1082;
        if(_S1123)
        {
            float3  _S1124 = s_primal_ctx_unproject_raydir_0(_S1108._S1081, camera_model_22, is_ray_depth_20);
            float3  _S1125 = make_float3 (depths_9.x) * _S1124;
            bool _S1126 = !!_S1108._S1084;
            if(_S1126)
            {
                float3  _S1127 = s_primal_ctx_unproject_raydir_0(_S1108._S1083, camera_model_22, is_ray_depth_20);
                float3  _S1128 = make_float3 (depths_9.y) * _S1127;
                _S1092 = int(2);
                points_11[int(0)] = _S1125;
                points_11[int(1)] = _S1128;
                points_11[int(2)] = _S1109;
                points_11[int(3)] = _S1109;
                _S1114 = _S1127;
            }
            else
            {
                _S1092 = int(0);
                points_11[int(0)] = _S1125;
                points_11[int(1)] = _S1109;
                points_11[int(2)] = _S1109;
                points_11[int(3)] = _S1109;
                _S1114 = _S1109;
            }
            if(_S1092 != int(2))
            {
                _runFlag_19 = false;
            }
            else
            {
                _runFlag_19 = _S1123;
                _S1092 = int(0);
            }
            if(_runFlag_19)
            {
                if(!_S1108._S1086)
                {
                    _runFlag_20 = false;
                    _S1092 = int(0);
                }
                else
                {
                    _runFlag_20 = _runFlag_19;
                }
                if(_runFlag_20)
                {
                    float3  _S1129 = s_primal_ctx_unproject_raydir_0(_S1108._S1085, camera_model_22, is_ray_depth_20);
                    points_11[int(2)] = make_float3 (depths_9.z) * _S1129;
                    bool _S1130 = !!_S1108._S1088;
                    int _S1131;
                    if(_S1130)
                    {
                        float3  _S1132 = s_primal_ctx_unproject_raydir_0(_S1108._S1087, camera_model_22, is_ray_depth_20);
                        points_11[int(3)] = make_float3 (depths_9.w) * _S1132;
                        _S1131 = int(2);
                        _S1115 = _S1132;
                    }
                    else
                    {
                        _S1131 = int(0);
                        _S1115 = _S1109;
                    }
                    if(_S1131 != int(2))
                    {
                        _runFlag_21 = false;
                        _S1092 = _S1131;
                    }
                    else
                    {
                        _runFlag_21 = _runFlag_20;
                    }
                    if(_runFlag_21)
                    {
                        _S1092 = int(1);
                    }
                    _runFlag_21 = _S1130;
                    _S1116 = _S1129;
                }
                else
                {
                    _runFlag_21 = false;
                    _S1115 = _S1109;
                    _S1116 = _S1109;
                }
            }
            else
            {
                _runFlag_20 = false;
                _runFlag_21 = false;
                _S1115 = _S1109;
                _S1116 = _S1109;
            }
            float3  _S1133 = _S1114;
            _S1114 = _S1115;
            _S1115 = _S1116;
            _S1111 = _S1126;
            _S1116 = _S1133;
            _S1117 = _S1124;
        }
        else
        {
            _S1092 = int(0);
            points_11[int(0)] = _S1109;
            points_11[int(1)] = _S1109;
            points_11[int(2)] = _S1109;
            points_11[int(3)] = _S1109;
            _runFlag_19 = false;
            _runFlag_20 = false;
            _runFlag_21 = false;
            _S1114 = _S1109;
            _S1115 = _S1109;
            _S1111 = false;
            _S1116 = _S1109;
            _S1117 = _S1109;
        }
        if(_S1092 != int(1))
        {
            _runFlag_22 = false;
        }
        else
        {
            _runFlag_22 = _S1110;
        }
        if(_runFlag_22)
        {
            float3  dx_5 = points_11[int(1)] - points_11[int(0)];
            float3  _S1134 = - (points_11[int(3)] - points_11[int(2)]);
            float3  _S1135 = s_primal_ctx_cross_0(dx_5, _S1134);
            bool _S1136 = (s_primal_ctx_dot_0(_S1135, _S1135)) != 0.0f;
            if(_S1136)
            {
                float _S1137 = length_0(_S1135);
                float3  _S1138 = make_float3 (_S1137);
                _S1118 = make_float3 (_S1137 * _S1137);
                _S1119 = _S1138;
            }
            else
            {
                _S1118 = _S1109;
                _S1119 = _S1109;
            }
            float3  _S1139 = _S1119;
            _S1112 = _S1136;
            _S1119 = _S1135;
            _S1120 = _S1139;
            _S1121 = dx_5;
            _S1122 = _S1134;
        }
        else
        {
            _S1112 = false;
            _S1118 = _S1109;
            _S1119 = _S1109;
            _S1120 = _S1109;
            _S1121 = _S1109;
            _S1122 = _S1109;
        }
        bool _S1140 = _runFlag_19;
        bool _S1141 = _runFlag_20;
        bool _S1142 = _runFlag_21;
        float3  _S1143 = _S1114;
        float3  _S1144 = _S1115;
        bool _S1145 = _S1111;
        float3  _S1146 = _S1116;
        float3  _S1147 = _S1117;
        _runFlag_19 = _runFlag_22;
        _runFlag_20 = _S1112;
        _S1114 = _S1118;
        _S1115 = _S1119;
        _S1116 = _S1120;
        _S1117 = _S1121;
        _S1118 = _S1122;
        _runFlag_21 = _S1123;
        _S1111 = _S1140;
        _runFlag_22 = _S1141;
        _S1112 = _S1142;
        _S1119 = _S1143;
        _S1120 = _S1144;
        _S1113 = _S1145;
        _S1121 = _S1146;
        _S1122 = _S1147;
    }
    else
    {
        _runFlag_19 = false;
        _runFlag_20 = false;
        _S1114 = _S1109;
        _S1115 = _S1109;
        _S1116 = _S1109;
        _S1117 = _S1109;
        _S1118 = _S1109;
        _runFlag_21 = false;
        _S1111 = false;
        _runFlag_22 = false;
        _S1112 = false;
        _S1119 = _S1109;
        _S1120 = _S1109;
        _S1113 = false;
        _S1121 = _S1109;
        _S1122 = _S1109;
    }
    float4  _S1148 = make_float4 (0.0f);
    float4  _S1149;
    if(_S1110)
    {
        if(_runFlag_19)
        {
            if(_runFlag_20)
            {
                float3  _S1150 = v_normal_3 / _S1114;
                float3  _S1151 = _S1115 * - _S1150;
                float3  _S1152 = _S1116 * _S1150;
                float _S1153 = _S1151.x + _S1151.y + _S1151.z;
                DiffPair_vectorx3Cfloatx2C3x3E_0 _S1154;
                (&_S1154)->primal_0 = _S1115;
                (&_S1154)->differential_0 = _S1109;
                s_bwd_length_impl_0(&_S1154, _S1153);
                _S1114 = _S1152 + _S1154.differential_0;
            }
            else
            {
                _S1114 = v_normal_3;
            }
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1155;
            (&_S1155)->primal_0 = _S1115;
            (&_S1155)->differential_0 = _S1109;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1156;
            (&_S1156)->primal_0 = _S1115;
            (&_S1156)->differential_0 = _S1109;
            s_bwd_prop_dot_0(&_S1155, &_S1156, 0.0f);
            float3  _S1157 = _S1156.differential_0 + _S1155.differential_0 + _S1114;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1158;
            (&_S1158)->primal_0 = _S1117;
            (&_S1158)->differential_0 = _S1109;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1159;
            (&_S1159)->primal_0 = _S1118;
            (&_S1159)->differential_0 = _S1109;
            s_bwd_prop_cross_0(&_S1158, &_S1159, _S1157);
            float3  s_diff_dy_T_5 = - _S1159.differential_0;
            float3  _S1160 = - s_diff_dy_T_5;
            float3  _S1161 = - _S1158.differential_0;
            FixedArray<float3 , 4>  _S1162;
            _S1162[int(0)] = _S1109;
            _S1162[int(1)] = _S1109;
            _S1162[int(2)] = _S1109;
            _S1162[int(3)] = _S1109;
            _S1162[int(2)] = _S1160;
            _S1162[int(3)] = s_diff_dy_T_5;
            _S1162[int(0)] = _S1161;
            _S1162[int(1)] = _S1158.differential_0;
            points_11[int(0)] = _S1162[int(0)];
            points_11[int(1)] = _S1162[int(1)];
            points_11[int(2)] = _S1162[int(2)];
            points_11[int(3)] = _S1162[int(3)];
        }
        else
        {
            points_11[int(0)] = _S1109;
            points_11[int(1)] = _S1109;
            points_11[int(2)] = _S1109;
            points_11[int(3)] = _S1109;
        }
        if(_runFlag_21)
        {
            if(_S1111)
            {
                if(_runFlag_22)
                {
                    FixedArray<float3 , 4>  _S1163 = points_11;
                    FixedArray<float3 , 4>  _S1164 = points_11;
                    FixedArray<float3 , 4>  _S1165 = points_11;
                    FixedArray<float3 , 4>  _S1166 = points_11;
                    if(_S1112)
                    {
                        float3  _S1167 = _S1119 * _S1166[int(3)];
                        float _S1168 = _S1167.x + _S1167.y + _S1167.z;
                        float4  _S1169 = _S1148;
                        *&((&_S1169)->w) = _S1168;
                        points_11[int(0)] = _S1163[int(0)];
                        points_11[int(1)] = _S1164[int(1)];
                        points_11[int(2)] = _S1165[int(2)];
                        points_11[int(3)] = _S1109;
                        _S1149 = _S1169;
                    }
                    else
                    {
                        points_11[int(0)] = _S1163[int(0)];
                        points_11[int(1)] = _S1164[int(1)];
                        points_11[int(2)] = _S1165[int(2)];
                        points_11[int(3)] = _S1166[int(3)];
                        _S1149 = _S1148;
                    }
                    float3  _S1170 = _S1120 * points_11[int(2)];
                    float _S1171 = _S1170.x + _S1170.y + _S1170.z;
                    FixedArray<float3 , 4>  _S1172 = points_11;
                    FixedArray<float3 , 4>  _S1173 = points_11;
                    float4  _S1174 = _S1148;
                    *&((&_S1174)->z) = _S1171;
                    float4  _S1175 = _S1149 + _S1174;
                    points_11[int(0)] = points_11[int(0)];
                    points_11[int(1)] = _S1172[int(1)];
                    points_11[int(2)] = _S1109;
                    points_11[int(3)] = _S1173[int(3)];
                    _S1149 = _S1175;
                }
                else
                {
                    FixedArray<float3 , 4>  _S1176 = points_11;
                    FixedArray<float3 , 4>  _S1177 = points_11;
                    FixedArray<float3 , 4>  _S1178 = points_11;
                    points_11[int(0)] = points_11[int(0)];
                    points_11[int(1)] = _S1176[int(1)];
                    points_11[int(2)] = _S1177[int(2)];
                    points_11[int(3)] = _S1178[int(3)];
                    _S1149 = _S1148;
                }
            }
            else
            {
                FixedArray<float3 , 4>  _S1179 = points_11;
                FixedArray<float3 , 4>  _S1180 = points_11;
                FixedArray<float3 , 4>  _S1181 = points_11;
                points_11[int(0)] = points_11[int(0)];
                points_11[int(1)] = _S1179[int(1)];
                points_11[int(2)] = _S1180[int(2)];
                points_11[int(3)] = _S1181[int(3)];
                _S1149 = _S1148;
            }
            if(_S1113)
            {
                FixedArray<float3 , 4>  _S1182 = points_11;
                float3  _S1183 = _S1121 * points_11[int(1)];
                float _S1184 = _S1183.x + _S1183.y + _S1183.z;
                float4  _S1185 = _S1148;
                *&((&_S1185)->y) = _S1184;
                float4  _S1186 = _S1149 + _S1185;
                points_11[int(0)] = _S1109;
                points_11[int(1)] = _S1109;
                points_11[int(2)] = _S1109;
                points_11[int(3)] = _S1109;
                _S1114 = _S1182[int(0)];
                _S1149 = _S1186;
            }
            else
            {
                FixedArray<float3 , 4>  _S1187 = points_11;
                FixedArray<float3 , 4>  _S1188 = points_11;
                FixedArray<float3 , 4>  _S1189 = points_11;
                points_11[int(0)] = points_11[int(0)];
                points_11[int(1)] = _S1187[int(1)];
                points_11[int(2)] = _S1188[int(2)];
                points_11[int(3)] = _S1189[int(3)];
                _S1114 = _S1109;
            }
            float3  _S1190 = _S1122 * (points_11[int(0)] + _S1114);
            float _S1191 = _S1190.x + _S1190.y + _S1190.z;
            float4  _S1192 = _S1148;
            *&((&_S1192)->x) = _S1191;
            _S1149 = _S1149 + _S1192;
        }
        else
        {
            _S1149 = _S1148;
        }
    }
    else
    {
        _S1149 = _S1148;
    }
    *v_depths_4 = _S1149;
    return;
}

inline __device__ float ray_depth_to_linear_depth_factor_prism(float2  pix_center_12, float4  intrins_21, FixedArray<float, 8>  dist_coeffs_25, int camera_model_23)
{
    float _S1193;
    for(;;)
    {
        float2  uv_63 = (pix_center_12 - float2 {intrins_21.z, intrins_21.w}) / float2 {intrins_21.x, intrins_21.y};
        FixedArray<float, 8>  _S1194 = dist_coeffs_25;
        float2  uv_u_30;
        bool _S1195 = undistort_point_2(uv_63, &_S1194, int(12), &uv_u_30);
        if(!_S1195)
        {
            _S1193 = 0.0f;
            break;
        }
        float3  raydir_16 = unproject_raydir_0(uv_u_30, camera_model_23, false);
        _S1193 = float((F32_sign((raydir_16.z)))) / length_0(raydir_16);
        break;
    }
    return _S1193;
}

inline __device__ float depth_normal_loss_prism(float2  pix_center_13, float4  intrins_22, FixedArray<float, 8>  dist_coeffs_26, int camera_model_24, bool is_ray_depth_21, float4  depths_10, float3  gt_normal_4)
{
    float _S1196;
    for(;;)
    {
        float3  _S1197;
        float3  * _S1198;
        float3  * _S1199;
        float3  * _S1200;
        float3  * _S1201;
        int _S1202;
        FixedArray<float3 , 5>  points_12;
        for(;;)
        {
            float2  _S1203 = float2 {intrins_22.z, intrins_22.w};
            float2  _S1204 = float2 {intrins_22.x, intrins_22.y};
            float2  uv_64 = (pix_center_13 + make_float2 (-1.0f, -0.0f) - _S1203) / _S1204;
            FixedArray<float, 8>  _S1205 = dist_coeffs_26;
            float2  uv_u_31;
            bool _S1206 = undistort_point_2(uv_64, &_S1205, int(12), &uv_u_31);
            float3  _S1207 = make_float3 (0.0f);
            if(!_S1206)
            {
                _S1202 = int(0);
                _S1201 = nullptr;
                _S1200 = nullptr;
                _S1199 = nullptr;
                _S1198 = nullptr;
                _S1197 = _S1207;
                break;
            }
            float3  raydir_17 = unproject_raydir_0(uv_u_31, camera_model_24, is_ray_depth_21);
            points_12[int(0)] = make_float3 (depths_10.x) * raydir_17;
            float2  uv_65 = (pix_center_13 + make_float2 (1.0f, -0.0f) - _S1203) / _S1204;
            FixedArray<float, 8>  _S1208 = dist_coeffs_26;
            float2  uv_u_32;
            bool _S1209 = undistort_point_2(uv_65, &_S1208, int(12), &uv_u_32);
            if(!_S1209)
            {
                _S1202 = int(0);
                _S1201 = nullptr;
                _S1200 = &points_12[int(0)];
                _S1199 = nullptr;
                _S1198 = nullptr;
                _S1197 = _S1207;
                break;
            }
            float3  raydir_18 = unproject_raydir_0(uv_u_32, camera_model_24, is_ray_depth_21);
            points_12[int(1)] = make_float3 (depths_10.y) * raydir_18;
            float2  uv_66 = (pix_center_13 + make_float2 (0.0f, -1.0f) - _S1203) / _S1204;
            FixedArray<float, 8>  _S1210 = dist_coeffs_26;
            float2  uv_u_33;
            bool _S1211 = undistort_point_2(uv_66, &_S1210, int(12), &uv_u_33);
            if(!_S1211)
            {
                _S1202 = int(0);
                _S1201 = &points_12[int(1)];
                _S1200 = &points_12[int(0)];
                _S1199 = nullptr;
                _S1198 = nullptr;
                _S1197 = _S1207;
                break;
            }
            float3  raydir_19 = unproject_raydir_0(uv_u_33, camera_model_24, is_ray_depth_21);
            points_12[int(2)] = make_float3 (depths_10.z) * raydir_19;
            float2  uv_67 = (pix_center_13 + make_float2 (0.0f, 1.0f) - _S1203) / _S1204;
            FixedArray<float, 8>  _S1212 = dist_coeffs_26;
            float2  uv_u_34;
            bool _S1213 = undistort_point_2(uv_67, &_S1212, int(12), &uv_u_34);
            if(!_S1213)
            {
                _S1202 = int(0);
                _S1201 = &points_12[int(1)];
                _S1200 = &points_12[int(0)];
                _S1199 = nullptr;
                _S1198 = &points_12[int(2)];
                _S1197 = _S1207;
                break;
            }
            float3  raydir_20 = unproject_raydir_0(uv_u_34, camera_model_24, is_ray_depth_21);
            points_12[int(3)] = make_float3 (depths_10.w) * raydir_20;
            float2  uv_68 = (pix_center_13 + make_float2 (0.0f) * make_float2 (0.0f, 3.0f) - _S1203) / _S1204;
            FixedArray<float, 8>  _S1214 = dist_coeffs_26;
            float2  uv_u_35;
            bool _S1215 = undistort_point_2(uv_68, &_S1214, int(12), &uv_u_35);
            if(!_S1215)
            {
                _S1202 = int(0);
                _S1201 = &points_12[int(1)];
                _S1200 = &points_12[int(0)];
                _S1199 = &points_12[int(3)];
                _S1198 = &points_12[int(2)];
                _S1197 = _S1207;
                break;
            }
            float3  raydir_21 = unproject_raydir_0(uv_u_35, camera_model_24, is_ray_depth_21);
            _S1202 = int(1);
            _S1201 = &points_12[int(1)];
            _S1200 = &points_12[int(0)];
            _S1199 = &points_12[int(3)];
            _S1198 = &points_12[int(2)];
            _S1197 = raydir_21;
            break;
        }
        if(_S1202 != int(1))
        {
            _S1196 = 0.0f;
            break;
        }
        float3  normal_14 = cross_0(*_S1201 - *_S1200, - (*_S1199 - *_S1198));
        float3  normal_15;
        if((dot_0(normal_14, normal_14)) != 0.0f)
        {
            normal_15 = normalize_0(normal_14);
        }
        else
        {
            normal_15 = normal_14;
        }
        float3  _S1216;
        if((dot_0(gt_normal_4, gt_normal_4)) != 0.0f)
        {
            _S1216 = normalize_0(gt_normal_4);
        }
        else
        {
            _S1216 = gt_normal_4;
        }
        _S1196 = (1.0f - dot_0(normal_15, _S1216) + 0.00100000004749745f) / ((F32_max((dot_0(normal_15, - normalize_0(_S1197))), (0.0f))) + 0.00100000004749745f);
        break;
    }
    return _S1196;
}

struct s_bwd_prop_depth_normal_loss_Intermediates_2
{
    float2  _S1217;
    bool _S1218;
    float2  _S1219;
    bool _S1220;
    float2  _S1221;
    bool _S1222;
    float2  _S1223;
    bool _S1224;
    float2  _S1225;
    bool _S1226;
};

inline __device__ void depth_normal_loss_vjp_prism(float2  pix_center_14, float4  intrins_23, FixedArray<float, 8>  dist_coeffs_27, int camera_model_25, bool is_ray_depth_22, float4  depths_11, float3  gt_normal_5, float v_loss_2, float4  * v_depths_5, float3  * v_gt_normal_2)
{
    float2  _S1227 = make_float2 (0.0f);
    s_bwd_prop_depth_normal_loss_Intermediates_2 _S1228;
    (&_S1228)->_S1217 = _S1227;
    (&_S1228)->_S1218 = false;
    (&_S1228)->_S1219 = _S1227;
    (&_S1228)->_S1220 = false;
    (&_S1228)->_S1221 = _S1227;
    (&_S1228)->_S1222 = false;
    (&_S1228)->_S1223 = _S1227;
    (&_S1228)->_S1224 = false;
    (&_S1228)->_S1225 = _S1227;
    (&_S1228)->_S1226 = false;
    (&_S1228)->_S1219 = _S1227;
    (&_S1228)->_S1220 = false;
    (&_S1228)->_S1221 = _S1227;
    (&_S1228)->_S1222 = false;
    (&_S1228)->_S1223 = _S1227;
    (&_S1228)->_S1224 = false;
    (&_S1228)->_S1225 = _S1227;
    (&_S1228)->_S1226 = false;
    float2  _S1229 = float2 {intrins_23.z, intrins_23.w};
    float2  _S1230 = float2 {intrins_23.x, intrins_23.y};
    float2  uv_69 = (pix_center_14 + make_float2 (-1.0f, -0.0f) - _S1229) / _S1230;
    float2  _S1231 = _S1227;
    FixedArray<float, 8>  _S1232 = dist_coeffs_27;
    bool _S1233 = undistort_point_2(uv_69, &_S1232, int(12), &_S1231);
    (&_S1228)->_S1217 = _S1231;
    (&_S1228)->_S1218 = _S1233;
    bool _S1234 = !!_S1233;
    bool _runFlag_23;
    if(_S1234)
    {
        float2  uv_70 = (pix_center_14 + make_float2 (1.0f, -0.0f) - _S1229) / _S1230;
        float2  _S1235 = _S1227;
        FixedArray<float, 8>  _S1236 = dist_coeffs_27;
        bool _S1237 = undistort_point_2(uv_70, &_S1236, int(12), &_S1235);
        (&_S1228)->_S1219 = _S1235;
        (&_S1228)->_S1220 = _S1237;
        if(!_S1237)
        {
            _runFlag_23 = false;
        }
        else
        {
            _runFlag_23 = _S1234;
        }
        if(_runFlag_23)
        {
            float2  uv_71 = (pix_center_14 + make_float2 (0.0f, -1.0f) - _S1229) / _S1230;
            float2  _S1238 = _S1227;
            FixedArray<float, 8>  _S1239 = dist_coeffs_27;
            bool _S1240 = undistort_point_2(uv_71, &_S1239, int(12), &_S1238);
            (&_S1228)->_S1221 = _S1238;
            (&_S1228)->_S1222 = _S1240;
            if(!_S1240)
            {
                _runFlag_23 = false;
            }
            if(_runFlag_23)
            {
                float2  uv_72 = (pix_center_14 + make_float2 (0.0f, 1.0f) - _S1229) / _S1230;
                float2  _S1241 = _S1227;
                FixedArray<float, 8>  _S1242 = dist_coeffs_27;
                bool _S1243 = undistort_point_2(uv_72, &_S1242, int(12), &_S1241);
                (&_S1228)->_S1223 = _S1241;
                (&_S1228)->_S1224 = _S1243;
                if(!_S1243)
                {
                    _runFlag_23 = false;
                }
                if(_runFlag_23)
                {
                    float2  uv_73 = (pix_center_14 - _S1229) / _S1230;
                    float2  _S1244 = _S1227;
                    FixedArray<float, 8>  _S1245 = dist_coeffs_27;
                    bool _S1246 = undistort_point_2(uv_73, &_S1245, int(12), &_S1244);
                    (&_S1228)->_S1225 = _S1244;
                    (&_S1228)->_S1226 = _S1246;
                }
            }
        }
    }
    s_bwd_prop_depth_normal_loss_Intermediates_2 _S1247 = _S1228;
    float3  _S1248 = make_float3 (0.0f);
    bool _S1249 = !!_S1228._S1218;
    bool _runFlag_24;
    bool _runFlag_25;
    bool _runFlag_26;
    int _S1250;
    float3  raydir_22;
    float3  _S1251;
    float3  _S1252;
    float3  _S1253;
    float3  _S1254;
    FixedArray<float3 , 5>  points_13;
    if(_S1249)
    {
        float3  _S1255 = s_primal_ctx_unproject_raydir_0(_S1247._S1217, camera_model_25, is_ray_depth_22);
        float3  _S1256 = make_float3 (depths_11.x) * _S1255;
        if(!_S1247._S1220)
        {
            _runFlag_23 = false;
        }
        else
        {
            _runFlag_23 = _S1249;
        }
        if(_runFlag_23)
        {
            float3  _S1257 = s_primal_ctx_unproject_raydir_0(_S1247._S1219, camera_model_25, is_ray_depth_22);
            float3  _S1258 = make_float3 (depths_11.y) * _S1257;
            if(!_S1247._S1222)
            {
                _runFlag_24 = false;
            }
            else
            {
                _runFlag_24 = _runFlag_23;
            }
            if(_runFlag_24)
            {
                float3  _S1259 = s_primal_ctx_unproject_raydir_0(_S1247._S1221, camera_model_25, is_ray_depth_22);
                float3  _S1260 = make_float3 (depths_11.z) * _S1259;
                if(!_S1247._S1224)
                {
                    _runFlag_25 = false;
                }
                else
                {
                    _runFlag_25 = _runFlag_24;
                }
                if(_runFlag_25)
                {
                    float3  _S1261 = s_primal_ctx_unproject_raydir_0(_S1247._S1223, camera_model_25, is_ray_depth_22);
                    float3  _S1262 = make_float3 (depths_11.w) * _S1261;
                    if(!_S1247._S1226)
                    {
                        _runFlag_26 = false;
                    }
                    else
                    {
                        _runFlag_26 = _runFlag_25;
                    }
                    if(_runFlag_26)
                    {
                        float3  _S1263 = s_primal_ctx_unproject_raydir_0(_S1247._S1225, camera_model_25, is_ray_depth_22);
                        _S1250 = int(1);
                        raydir_22 = _S1263;
                    }
                    else
                    {
                        _S1250 = int(0);
                        raydir_22 = _S1261;
                    }
                    points_13[int(0)] = _S1256;
                    points_13[int(1)] = _S1258;
                    points_13[int(2)] = _S1260;
                    points_13[int(3)] = _S1262;
                    points_13[int(4)] = _S1248;
                    _S1251 = _S1261;
                }
                else
                {
                    _S1250 = int(0);
                    raydir_22 = _S1259;
                    points_13[int(0)] = _S1256;
                    points_13[int(1)] = _S1258;
                    points_13[int(2)] = _S1260;
                    points_13[int(3)] = _S1248;
                    points_13[int(4)] = _S1248;
                    _S1251 = _S1248;
                }
                _S1252 = _S1259;
            }
            else
            {
                _S1250 = int(0);
                raydir_22 = _S1257;
                points_13[int(0)] = _S1256;
                points_13[int(1)] = _S1258;
                points_13[int(2)] = _S1248;
                points_13[int(3)] = _S1248;
                points_13[int(4)] = _S1248;
                _runFlag_25 = false;
                _S1251 = _S1248;
                _S1252 = _S1248;
            }
            _S1253 = _S1257;
        }
        else
        {
            _S1250 = int(0);
            raydir_22 = _S1255;
            points_13[int(0)] = _S1256;
            points_13[int(1)] = _S1248;
            points_13[int(2)] = _S1248;
            points_13[int(3)] = _S1248;
            points_13[int(4)] = _S1248;
            _runFlag_24 = false;
            _runFlag_25 = false;
            _S1251 = _S1248;
            _S1252 = _S1248;
            _S1253 = _S1248;
        }
        _S1254 = _S1255;
    }
    else
    {
        _S1250 = int(0);
        points_13[int(0)] = _S1248;
        points_13[int(1)] = _S1248;
        points_13[int(2)] = _S1248;
        points_13[int(3)] = _S1248;
        points_13[int(4)] = _S1248;
        _runFlag_23 = false;
        _runFlag_24 = false;
        _runFlag_25 = false;
        _S1251 = _S1248;
        _S1252 = _S1248;
        _S1253 = _S1248;
        _S1254 = _S1248;
    }
    bool _S1264 = !(_S1250 != int(1));
    bool _S1265;
    float3  normal_16;
    float3  _S1266;
    float3  _S1267;
    float3  _S1268;
    float3  _S1269;
    float _S1270;
    float _S1271;
    float _S1272;
    float _S1273;
    if(_S1264)
    {
        float3  dx_6 = points_13[int(1)] - points_13[int(0)];
        float3  _S1274 = - (points_13[int(3)] - points_13[int(2)]);
        float3  _S1275 = s_primal_ctx_cross_0(dx_6, _S1274);
        bool _S1276 = (s_primal_ctx_dot_0(_S1275, _S1275)) != 0.0f;
        if(_S1276)
        {
            normal_16 = normalize_0(_S1275);
        }
        else
        {
            normal_16 = _S1275;
        }
        bool _S1277 = (s_primal_ctx_dot_0(gt_normal_5, gt_normal_5)) != 0.0f;
        if(_S1277)
        {
            _S1266 = normalize_0(gt_normal_5);
        }
        else
        {
            _S1266 = gt_normal_5;
        }
        float3  _S1278 = - normalize_0(raydir_22);
        float _S1279 = s_primal_ctx_dot_0(normal_16, _S1278);
        float _S1280 = 1.0f - s_primal_ctx_dot_0(normal_16, _S1266) + 0.00100000004749745f;
        float _S1281 = (F32_max((_S1279), (0.0f))) + 0.00100000004749745f;
        _S1270 = _S1281 * _S1281;
        _S1271 = _S1280;
        _S1272 = _S1281;
        _S1273 = _S1279;
        raydir_22 = normal_16;
        normal_16 = _S1278;
        _runFlag_26 = _S1277;
        _S1265 = _S1276;
        _S1267 = _S1275;
        _S1268 = dx_6;
        _S1269 = _S1274;
    }
    else
    {
        _S1270 = 0.0f;
        _S1271 = 0.0f;
        _S1272 = 0.0f;
        _S1273 = 0.0f;
        raydir_22 = _S1248;
        normal_16 = _S1248;
        _S1266 = _S1248;
        _runFlag_26 = false;
        _S1265 = false;
        _S1267 = _S1248;
        _S1268 = _S1248;
        _S1269 = _S1248;
    }
    float4  _S1282 = make_float4 (0.0f);
    if(_S1264)
    {
        float _S1283 = v_loss_2 / _S1270;
        float _S1284 = _S1271 * - _S1283;
        float s_diff_num_T_2 = _S1272 * _S1283;
        DiffPair_float_0 _S1285;
        (&_S1285)->primal_0 = _S1273;
        (&_S1285)->differential_0 = 0.0f;
        DiffPair_float_0 _S1286;
        (&_S1286)->primal_0 = 0.0f;
        (&_S1286)->differential_0 = 0.0f;
        _d_max_0(&_S1285, &_S1286, _S1284);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1287;
        (&_S1287)->primal_0 = raydir_22;
        (&_S1287)->differential_0 = _S1248;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1288;
        (&_S1288)->primal_0 = normal_16;
        (&_S1288)->differential_0 = _S1248;
        s_bwd_prop_dot_0(&_S1287, &_S1288, _S1285.differential_0);
        float _S1289 = - s_diff_num_T_2;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1290;
        (&_S1290)->primal_0 = raydir_22;
        (&_S1290)->differential_0 = _S1248;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1291;
        (&_S1291)->primal_0 = _S1266;
        (&_S1291)->differential_0 = _S1248;
        s_bwd_prop_dot_0(&_S1290, &_S1291, _S1289);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1292 = _S1291;
        float3  _S1293 = _S1287.differential_0 + _S1290.differential_0;
        if(_runFlag_26)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1294;
            (&_S1294)->primal_0 = gt_normal_5;
            (&_S1294)->differential_0 = _S1248;
            s_bwd_normalize_impl_0(&_S1294, _S1292.differential_0);
            raydir_22 = _S1294.differential_0;
        }
        else
        {
            raydir_22 = _S1292.differential_0;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1295;
        (&_S1295)->primal_0 = gt_normal_5;
        (&_S1295)->differential_0 = _S1248;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1296;
        (&_S1296)->primal_0 = gt_normal_5;
        (&_S1296)->differential_0 = _S1248;
        s_bwd_prop_dot_0(&_S1295, &_S1296, 0.0f);
        float3  _S1297 = _S1296.differential_0 + _S1295.differential_0 + raydir_22;
        if(_S1265)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1298;
            (&_S1298)->primal_0 = _S1267;
            (&_S1298)->differential_0 = _S1248;
            s_bwd_normalize_impl_0(&_S1298, _S1293);
            raydir_22 = _S1298.differential_0;
        }
        else
        {
            raydir_22 = _S1293;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1299;
        (&_S1299)->primal_0 = _S1267;
        (&_S1299)->differential_0 = _S1248;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1300;
        (&_S1300)->primal_0 = _S1267;
        (&_S1300)->differential_0 = _S1248;
        s_bwd_prop_dot_0(&_S1299, &_S1300, 0.0f);
        float3  _S1301 = _S1300.differential_0 + _S1299.differential_0 + raydir_22;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1302;
        (&_S1302)->primal_0 = _S1268;
        (&_S1302)->differential_0 = _S1248;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1303;
        (&_S1303)->primal_0 = _S1269;
        (&_S1303)->differential_0 = _S1248;
        s_bwd_prop_cross_0(&_S1302, &_S1303, _S1301);
        float3  s_diff_dy_T_6 = - _S1303.differential_0;
        float3  _S1304 = - s_diff_dy_T_6;
        float3  _S1305 = - _S1302.differential_0;
        FixedArray<float3 , 5>  _S1306;
        _S1306[int(0)] = _S1248;
        _S1306[int(1)] = _S1248;
        _S1306[int(2)] = _S1248;
        _S1306[int(3)] = _S1248;
        _S1306[int(4)] = _S1248;
        _S1306[int(2)] = _S1304;
        _S1306[int(3)] = s_diff_dy_T_6;
        _S1306[int(0)] = _S1305;
        _S1306[int(1)] = _S1302.differential_0;
        points_13[int(0)] = _S1306[int(0)];
        points_13[int(1)] = _S1306[int(1)];
        points_13[int(2)] = _S1306[int(2)];
        points_13[int(3)] = _S1306[int(3)];
        points_13[int(4)] = _S1306[int(4)];
        raydir_22 = _S1297;
    }
    else
    {
        points_13[int(0)] = _S1248;
        points_13[int(1)] = _S1248;
        points_13[int(2)] = _S1248;
        points_13[int(3)] = _S1248;
        points_13[int(4)] = _S1248;
        raydir_22 = _S1248;
    }
    float4  _S1307;
    if(_S1249)
    {
        if(_runFlag_23)
        {
            if(_runFlag_24)
            {
                if(_runFlag_25)
                {
                    FixedArray<float3 , 5>  _S1308 = points_13;
                    FixedArray<float3 , 5>  _S1309 = points_13;
                    FixedArray<float3 , 5>  _S1310 = points_13;
                    float3  _S1311 = _S1251 * points_13[int(3)];
                    float _S1312 = _S1311.x + _S1311.y + _S1311.z;
                    float4  _S1313 = _S1282;
                    *&((&_S1313)->w) = _S1312;
                    points_13[int(0)] = _S1248;
                    points_13[int(1)] = _S1248;
                    points_13[int(2)] = _S1248;
                    points_13[int(3)] = _S1248;
                    points_13[int(4)] = _S1248;
                    _S1251 = _S1310[int(2)];
                    normal_16 = _S1308[int(0)];
                    _S1266 = _S1309[int(1)];
                    _S1307 = _S1313;
                }
                else
                {
                    FixedArray<float3 , 5>  _S1314 = points_13;
                    FixedArray<float3 , 5>  _S1315 = points_13;
                    FixedArray<float3 , 5>  _S1316 = points_13;
                    FixedArray<float3 , 5>  _S1317 = points_13;
                    points_13[int(0)] = points_13[int(0)];
                    points_13[int(1)] = _S1314[int(1)];
                    points_13[int(2)] = _S1315[int(2)];
                    points_13[int(3)] = _S1316[int(3)];
                    points_13[int(4)] = _S1317[int(4)];
                    _S1251 = _S1248;
                    normal_16 = _S1248;
                    _S1266 = _S1248;
                    _S1307 = _S1282;
                }
                float3  _S1318 = _S1252 * (points_13[int(2)] + _S1251);
                float _S1319 = _S1318.x + _S1318.y + _S1318.z;
                float3  _S1320 = points_13[int(0)] + normal_16;
                float3  _S1321 = points_13[int(1)] + _S1266;
                float4  _S1322 = _S1282;
                *&((&_S1322)->z) = _S1319;
                float4  _S1323 = _S1307 + _S1322;
                points_13[int(0)] = _S1248;
                points_13[int(1)] = _S1248;
                points_13[int(2)] = _S1248;
                points_13[int(3)] = _S1248;
                points_13[int(4)] = _S1248;
                _S1251 = _S1321;
                _S1252 = _S1320;
                _S1307 = _S1323;
            }
            else
            {
                FixedArray<float3 , 5>  _S1324 = points_13;
                FixedArray<float3 , 5>  _S1325 = points_13;
                FixedArray<float3 , 5>  _S1326 = points_13;
                FixedArray<float3 , 5>  _S1327 = points_13;
                points_13[int(0)] = points_13[int(0)];
                points_13[int(1)] = _S1324[int(1)];
                points_13[int(2)] = _S1325[int(2)];
                points_13[int(3)] = _S1326[int(3)];
                points_13[int(4)] = _S1327[int(4)];
                _S1251 = _S1248;
                _S1252 = _S1248;
                _S1307 = _S1282;
            }
            float3  _S1328 = _S1253 * (points_13[int(1)] + _S1251);
            float _S1329 = _S1328.x + _S1328.y + _S1328.z;
            float3  _S1330 = points_13[int(0)] + _S1252;
            float4  _S1331 = _S1282;
            *&((&_S1331)->y) = _S1329;
            float4  _S1332 = _S1307 + _S1331;
            points_13[int(0)] = _S1248;
            points_13[int(1)] = _S1248;
            points_13[int(2)] = _S1248;
            points_13[int(3)] = _S1248;
            points_13[int(4)] = _S1248;
            _S1251 = _S1330;
            _S1307 = _S1332;
        }
        else
        {
            FixedArray<float3 , 5>  _S1333 = points_13;
            FixedArray<float3 , 5>  _S1334 = points_13;
            FixedArray<float3 , 5>  _S1335 = points_13;
            FixedArray<float3 , 5>  _S1336 = points_13;
            points_13[int(0)] = points_13[int(0)];
            points_13[int(1)] = _S1333[int(1)];
            points_13[int(2)] = _S1334[int(2)];
            points_13[int(3)] = _S1335[int(3)];
            points_13[int(4)] = _S1336[int(4)];
            _S1251 = _S1248;
            _S1307 = _S1282;
        }
        float3  _S1337 = _S1254 * (points_13[int(0)] + _S1251);
        float _S1338 = _S1337.x + _S1337.y + _S1337.z;
        float4  _S1339 = _S1282;
        *&((&_S1339)->x) = _S1338;
        _S1307 = _S1307 + _S1339;
    }
    else
    {
        _S1307 = _S1282;
    }
    *v_depths_5 = _S1307;
    *v_gt_normal_2 = raydir_22;
    return;
}

inline __device__ float3  generate_ray_d2n_rational(float2  pix_pos_9, float4  intrins_24, FixedArray<float, 8>  dist_coeffs_28, int camera_model_26, bool is_ray_depth_23)
{
    float3  _S1340;
    for(;;)
    {
        float2  uv_74 = (pix_pos_9 - float2 {intrins_24.z, intrins_24.w}) / float2 {intrins_24.x, intrins_24.y};
        FixedArray<float, 8>  _S1341 = dist_coeffs_28;
        float2  uv_u_36;
        bool _S1342 = undistort_point_3(uv_74, &_S1341, int(12), &uv_u_36);
        if(!_S1342)
        {
            int3  _S1343 = make_int3 (int(0));
            float3  _S1344 = make_float3 ((float)_S1343.x, (float)_S1343.y, (float)_S1343.z);
            _S1340 = _S1344;
            break;
        }
        _S1340 = unproject_raydir_0(uv_u_36, camera_model_26, is_ray_depth_23);
        break;
    }
    return _S1340;
}

inline __device__ float3  depth_to_point_rational(float2  pix_pos_10, float4  intrins_25, FixedArray<float, 8>  dist_coeffs_29, int camera_model_27, bool is_ray_depth_24, float depth_8)
{
    float3  _S1345;
    for(;;)
    {
        float2  uv_75 = (pix_pos_10 - float2 {intrins_25.z, intrins_25.w}) / float2 {intrins_25.x, intrins_25.y};
        FixedArray<float, 8>  _S1346 = dist_coeffs_29;
        float2  uv_u_37;
        bool _S1347 = undistort_point_3(uv_75, &_S1346, int(12), &uv_u_37);
        if(!_S1347)
        {
            _S1345 = make_float3 (0.0f);
            break;
        }
        _S1345 = make_float3 (depth_8) * unproject_raydir_0(uv_u_37, camera_model_27, is_ray_depth_24);
        break;
    }
    return _S1345;
}

struct s_bwd_prop_depth_to_point_Intermediates_3
{
    float2  _S1348;
    bool _S1349;
};

inline __device__ float depth_to_point_vjp_rational(float2  pix_pos_11, float4  intrins_26, FixedArray<float, 8>  dist_coeffs_30, int camera_model_28, bool is_ray_depth_25, float depth_9, float3  v_point_3)
{
    float2  _S1350 = make_float2 (0.0f);
    s_bwd_prop_depth_to_point_Intermediates_3 _S1351;
    (&_S1351)->_S1348 = _S1350;
    (&_S1351)->_S1349 = false;
    float2  uv_76 = (pix_pos_11 - float2 {intrins_26.z, intrins_26.w}) / float2 {intrins_26.x, intrins_26.y};
    float2  _S1352 = _S1350;
    FixedArray<float, 8>  _S1353 = dist_coeffs_30;
    bool _S1354 = undistort_point_3(uv_76, &_S1353, int(12), &_S1352);
    (&_S1351)->_S1348 = _S1352;
    (&_S1351)->_S1349 = _S1354;
    s_bwd_prop_depth_to_point_Intermediates_3 _S1355 = _S1351;
    float3  _S1356 = make_float3 (0.0f);
    bool _S1357 = !!_S1351._S1349;
    float3  _S1358;
    if(_S1357)
    {
        _S1358 = s_primal_ctx_unproject_raydir_0(_S1355._S1348, camera_model_28, is_ray_depth_25);
    }
    else
    {
        _S1358 = _S1356;
    }
    if(_S1357)
    {
        _S1358 = _S1358 * v_point_3;
    }
    else
    {
        _S1358 = _S1356;
    }
    return _S1358.x + _S1358.y + _S1358.z;
}

inline __device__ float3  depth_to_normal_rational(float2  pix_center_15, float4  intrins_27, FixedArray<float, 8>  dist_coeffs_31, int camera_model_29, bool is_ray_depth_26, float4  depths_12)
{
    float3  normal_17;
    for(;;)
    {
        bool _S1359;
        if((depths_12.x) == 0.0f)
        {
            _S1359 = true;
        }
        else
        {
            _S1359 = (depths_12.y) == 0.0f;
        }
        if(_S1359)
        {
            _S1359 = true;
        }
        else
        {
            _S1359 = (depths_12.z) == 0.0f;
        }
        if(_S1359)
        {
            _S1359 = true;
        }
        else
        {
            _S1359 = (depths_12.w) == 0.0f;
        }
        if(_S1359)
        {
            normal_17 = make_float3 (0.0f);
            break;
        }
        float3  * _S1360;
        float3  * _S1361;
        float3  * _S1362;
        float3  * _S1363;
        int _S1364;
        FixedArray<float3 , 4>  points_14;
        for(;;)
        {
            float2  _S1365 = float2 {intrins_27.z, intrins_27.w};
            float2  _S1366 = float2 {intrins_27.x, intrins_27.y};
            float2  uv_77 = (pix_center_15 + make_float2 (-1.0f, -0.0f) - _S1365) / _S1366;
            FixedArray<float, 8>  _S1367 = dist_coeffs_31;
            float2  uv_u_38;
            bool _S1368 = undistort_point_3(uv_77, &_S1367, int(12), &uv_u_38);
            if(!_S1368)
            {
                float3  _S1369 = make_float3 (0.0f);
                _S1364 = int(0);
                _S1363 = nullptr;
                _S1362 = nullptr;
                _S1361 = nullptr;
                _S1360 = nullptr;
                normal_17 = _S1369;
                break;
            }
            points_14[int(0)] = make_float3 (depths_12.x) * unproject_raydir_0(uv_u_38, camera_model_29, is_ray_depth_26);
            for(;;)
            {
                float2  uv_78 = (pix_center_15 + make_float2 (1.0f, -0.0f) - _S1365) / _S1366;
                FixedArray<float, 8>  _S1370 = dist_coeffs_31;
                float2  uv_u_39;
                bool _S1371 = undistort_point_3(uv_78, &_S1370, int(12), &uv_u_39);
                if(!_S1371)
                {
                    float3  _S1372 = make_float3 (0.0f);
                    _S1364 = int(0);
                    _S1363 = nullptr;
                    normal_17 = _S1372;
                    break;
                }
                points_14[int(1)] = make_float3 (depths_12.y) * unproject_raydir_0(uv_u_39, camera_model_29, is_ray_depth_26);
                _S1364 = int(2);
                _S1363 = &points_14[int(1)];
                break;
            }
            if(_S1364 != int(2))
            {
                _S1362 = &points_14[int(0)];
                _S1361 = nullptr;
                _S1360 = nullptr;
                break;
            }
            float2  uv_79 = (pix_center_15 + make_float2 (0.0f, -1.0f) - _S1365) / _S1366;
            FixedArray<float, 8>  _S1373 = dist_coeffs_31;
            float2  uv_u_40;
            bool _S1374 = undistort_point_3(uv_79, &_S1373, int(12), &uv_u_40);
            if(!_S1374)
            {
                float3  _S1375 = make_float3 (0.0f);
                _S1364 = int(0);
                _S1362 = &points_14[int(0)];
                _S1361 = nullptr;
                _S1360 = nullptr;
                normal_17 = _S1375;
                break;
            }
            points_14[int(2)] = make_float3 (depths_12.z) * unproject_raydir_0(uv_u_40, camera_model_29, is_ray_depth_26);
            for(;;)
            {
                float2  uv_80 = (pix_center_15 + make_float2 (0.0f, 1.0f) - _S1365) / _S1366;
                FixedArray<float, 8>  _S1376 = dist_coeffs_31;
                float2  uv_u_41;
                bool _S1377 = undistort_point_3(uv_80, &_S1376, int(12), &uv_u_41);
                if(!_S1377)
                {
                    float3  _S1378 = make_float3 (0.0f);
                    _S1364 = int(0);
                    _S1362 = nullptr;
                    normal_17 = _S1378;
                    break;
                }
                points_14[int(3)] = make_float3 (depths_12.w) * unproject_raydir_0(uv_u_41, camera_model_29, is_ray_depth_26);
                _S1364 = int(2);
                _S1362 = &points_14[int(3)];
                break;
            }
            if(_S1364 != int(2))
            {
                float3  * _S1379 = _S1362;
                _S1362 = &points_14[int(0)];
                _S1361 = _S1379;
                _S1360 = &points_14[int(2)];
                break;
            }
            float3  * _S1380 = _S1362;
            _S1364 = int(1);
            _S1362 = &points_14[int(0)];
            _S1361 = _S1380;
            _S1360 = &points_14[int(2)];
            break;
        }
        if(_S1364 != int(1))
        {
            break;
        }
        float3  normal_18 = cross_0(*_S1363 - *_S1362, - (*_S1361 - *_S1360));
        if((dot_0(normal_18, normal_18)) != 0.0f)
        {
            normal_17 = normal_18 / make_float3 (length_0(normal_18));
        }
        else
        {
            normal_17 = normal_18;
        }
        break;
    }
    return normal_17;
}

struct s_bwd_prop_depth_to_normal_Intermediates_3
{
    float2  _S1381;
    bool _S1382;
    float2  _S1383;
    bool _S1384;
    float2  _S1385;
    bool _S1386;
    float2  _S1387;
    bool _S1388;
};

inline __device__ void depth_to_normal_vjp_rational(float2  pix_center_16, float4  intrins_28, FixedArray<float, 8>  dist_coeffs_32, int camera_model_30, bool is_ray_depth_27, float4  depths_13, float3  v_normal_4, float4  * v_depths_6)
{
    float2  _S1389 = make_float2 (0.0f);
    s_bwd_prop_depth_to_normal_Intermediates_3 _S1390;
    (&_S1390)->_S1381 = _S1389;
    (&_S1390)->_S1382 = false;
    (&_S1390)->_S1383 = _S1389;
    (&_S1390)->_S1384 = false;
    (&_S1390)->_S1385 = _S1389;
    (&_S1390)->_S1386 = false;
    (&_S1390)->_S1387 = _S1389;
    (&_S1390)->_S1388 = false;
    (&_S1390)->_S1381 = _S1389;
    (&_S1390)->_S1382 = false;
    (&_S1390)->_S1383 = _S1389;
    (&_S1390)->_S1384 = false;
    (&_S1390)->_S1385 = _S1389;
    (&_S1390)->_S1386 = false;
    (&_S1390)->_S1387 = _S1389;
    (&_S1390)->_S1388 = false;
    bool _S1391 = (depths_13.x) == 0.0f;
    bool _runFlag_27;
    if(_S1391)
    {
        _runFlag_27 = true;
    }
    else
    {
        _runFlag_27 = (depths_13.y) == 0.0f;
    }
    if(_runFlag_27)
    {
        _runFlag_27 = true;
    }
    else
    {
        _runFlag_27 = (depths_13.z) == 0.0f;
    }
    if(_runFlag_27)
    {
        _runFlag_27 = true;
    }
    else
    {
        _runFlag_27 = (depths_13.w) == 0.0f;
    }
    int _S1392;
    if(!_runFlag_27)
    {
        float2  _S1393 = float2 {intrins_28.z, intrins_28.w};
        float2  _S1394 = float2 {intrins_28.x, intrins_28.y};
        float2  uv_81 = (pix_center_16 + make_float2 (-1.0f, -0.0f) - _S1393) / _S1394;
        float2  _S1395 = _S1389;
        FixedArray<float, 8>  _S1396 = dist_coeffs_32;
        bool _S1397 = undistort_point_3(uv_81, &_S1396, int(12), &_S1395);
        (&_S1390)->_S1381 = _S1395;
        (&_S1390)->_S1382 = _S1397;
        bool _S1398 = !!_S1397;
        if(_S1398)
        {
            float2  uv_82 = (pix_center_16 + make_float2 (1.0f, -0.0f) - _S1393) / _S1394;
            float2  _S1399 = _S1389;
            FixedArray<float, 8>  _S1400 = dist_coeffs_32;
            bool _S1401 = undistort_point_3(uv_82, &_S1400, int(12), &_S1399);
            (&_S1390)->_S1383 = _S1399;
            (&_S1390)->_S1384 = _S1401;
            if(!!_S1401)
            {
                _S1392 = int(2);
            }
            else
            {
                _S1392 = int(0);
            }
            if(_S1392 != int(2))
            {
                _runFlag_27 = false;
            }
            else
            {
                _runFlag_27 = _S1398;
            }
            if(_runFlag_27)
            {
                float2  uv_83 = (pix_center_16 + make_float2 (0.0f, -1.0f) - _S1393) / _S1394;
                float2  _S1402 = _S1389;
                FixedArray<float, 8>  _S1403 = dist_coeffs_32;
                bool _S1404 = undistort_point_3(uv_83, &_S1403, int(12), &_S1402);
                (&_S1390)->_S1385 = _S1402;
                (&_S1390)->_S1386 = _S1404;
                if(!_S1404)
                {
                    _runFlag_27 = false;
                }
                if(_runFlag_27)
                {
                    float2  uv_84 = (pix_center_16 + make_float2 (0.0f, 1.0f) - _S1393) / _S1394;
                    float2  _S1405 = _S1389;
                    FixedArray<float, 8>  _S1406 = dist_coeffs_32;
                    bool _S1407 = undistort_point_3(uv_84, &_S1406, int(12), &_S1405);
                    (&_S1390)->_S1387 = _S1405;
                    (&_S1390)->_S1388 = _S1407;
                }
            }
        }
    }
    s_bwd_prop_depth_to_normal_Intermediates_3 _S1408 = _S1390;
    float3  _S1409 = make_float3 (0.0f);
    if(_S1391)
    {
        _runFlag_27 = true;
    }
    else
    {
        _runFlag_27 = (depths_13.y) == 0.0f;
    }
    if(_runFlag_27)
    {
        _runFlag_27 = true;
    }
    else
    {
        _runFlag_27 = (depths_13.z) == 0.0f;
    }
    if(_runFlag_27)
    {
        _runFlag_27 = true;
    }
    else
    {
        _runFlag_27 = (depths_13.w) == 0.0f;
    }
    bool _S1410 = !_runFlag_27;
    bool _runFlag_28;
    bool _runFlag_29;
    bool _S1411;
    bool _runFlag_30;
    bool _S1412;
    bool _S1413;
    FixedArray<float3 , 4>  points_15;
    float3  _S1414;
    float3  _S1415;
    float3  _S1416;
    float3  _S1417;
    float3  _S1418;
    float3  _S1419;
    float3  _S1420;
    float3  _S1421;
    float3  _S1422;
    if(_S1410)
    {
        bool _S1423 = !!_S1408._S1382;
        if(_S1423)
        {
            float3  _S1424 = s_primal_ctx_unproject_raydir_0(_S1408._S1381, camera_model_30, is_ray_depth_27);
            float3  _S1425 = make_float3 (depths_13.x) * _S1424;
            bool _S1426 = !!_S1408._S1384;
            if(_S1426)
            {
                float3  _S1427 = s_primal_ctx_unproject_raydir_0(_S1408._S1383, camera_model_30, is_ray_depth_27);
                float3  _S1428 = make_float3 (depths_13.y) * _S1427;
                _S1392 = int(2);
                points_15[int(0)] = _S1425;
                points_15[int(1)] = _S1428;
                points_15[int(2)] = _S1409;
                points_15[int(3)] = _S1409;
                _S1414 = _S1427;
            }
            else
            {
                _S1392 = int(0);
                points_15[int(0)] = _S1425;
                points_15[int(1)] = _S1409;
                points_15[int(2)] = _S1409;
                points_15[int(3)] = _S1409;
                _S1414 = _S1409;
            }
            if(_S1392 != int(2))
            {
                _runFlag_27 = false;
            }
            else
            {
                _runFlag_27 = _S1423;
                _S1392 = int(0);
            }
            if(_runFlag_27)
            {
                if(!_S1408._S1386)
                {
                    _runFlag_28 = false;
                    _S1392 = int(0);
                }
                else
                {
                    _runFlag_28 = _runFlag_27;
                }
                if(_runFlag_28)
                {
                    float3  _S1429 = s_primal_ctx_unproject_raydir_0(_S1408._S1385, camera_model_30, is_ray_depth_27);
                    points_15[int(2)] = make_float3 (depths_13.z) * _S1429;
                    bool _S1430 = !!_S1408._S1388;
                    int _S1431;
                    if(_S1430)
                    {
                        float3  _S1432 = s_primal_ctx_unproject_raydir_0(_S1408._S1387, camera_model_30, is_ray_depth_27);
                        points_15[int(3)] = make_float3 (depths_13.w) * _S1432;
                        _S1431 = int(2);
                        _S1415 = _S1432;
                    }
                    else
                    {
                        _S1431 = int(0);
                        _S1415 = _S1409;
                    }
                    if(_S1431 != int(2))
                    {
                        _runFlag_29 = false;
                        _S1392 = _S1431;
                    }
                    else
                    {
                        _runFlag_29 = _runFlag_28;
                    }
                    if(_runFlag_29)
                    {
                        _S1392 = int(1);
                    }
                    _runFlag_29 = _S1430;
                    _S1416 = _S1429;
                }
                else
                {
                    _runFlag_29 = false;
                    _S1415 = _S1409;
                    _S1416 = _S1409;
                }
            }
            else
            {
                _runFlag_28 = false;
                _runFlag_29 = false;
                _S1415 = _S1409;
                _S1416 = _S1409;
            }
            float3  _S1433 = _S1414;
            _S1414 = _S1415;
            _S1415 = _S1416;
            _S1411 = _S1426;
            _S1416 = _S1433;
            _S1417 = _S1424;
        }
        else
        {
            _S1392 = int(0);
            points_15[int(0)] = _S1409;
            points_15[int(1)] = _S1409;
            points_15[int(2)] = _S1409;
            points_15[int(3)] = _S1409;
            _runFlag_27 = false;
            _runFlag_28 = false;
            _runFlag_29 = false;
            _S1414 = _S1409;
            _S1415 = _S1409;
            _S1411 = false;
            _S1416 = _S1409;
            _S1417 = _S1409;
        }
        if(_S1392 != int(1))
        {
            _runFlag_30 = false;
        }
        else
        {
            _runFlag_30 = _S1410;
        }
        if(_runFlag_30)
        {
            float3  dx_7 = points_15[int(1)] - points_15[int(0)];
            float3  _S1434 = - (points_15[int(3)] - points_15[int(2)]);
            float3  _S1435 = s_primal_ctx_cross_0(dx_7, _S1434);
            bool _S1436 = (s_primal_ctx_dot_0(_S1435, _S1435)) != 0.0f;
            if(_S1436)
            {
                float _S1437 = length_0(_S1435);
                float3  _S1438 = make_float3 (_S1437);
                _S1418 = make_float3 (_S1437 * _S1437);
                _S1419 = _S1438;
            }
            else
            {
                _S1418 = _S1409;
                _S1419 = _S1409;
            }
            float3  _S1439 = _S1419;
            _S1412 = _S1436;
            _S1419 = _S1435;
            _S1420 = _S1439;
            _S1421 = dx_7;
            _S1422 = _S1434;
        }
        else
        {
            _S1412 = false;
            _S1418 = _S1409;
            _S1419 = _S1409;
            _S1420 = _S1409;
            _S1421 = _S1409;
            _S1422 = _S1409;
        }
        bool _S1440 = _runFlag_27;
        bool _S1441 = _runFlag_28;
        bool _S1442 = _runFlag_29;
        float3  _S1443 = _S1414;
        float3  _S1444 = _S1415;
        bool _S1445 = _S1411;
        float3  _S1446 = _S1416;
        float3  _S1447 = _S1417;
        _runFlag_27 = _runFlag_30;
        _runFlag_28 = _S1412;
        _S1414 = _S1418;
        _S1415 = _S1419;
        _S1416 = _S1420;
        _S1417 = _S1421;
        _S1418 = _S1422;
        _runFlag_29 = _S1423;
        _S1411 = _S1440;
        _runFlag_30 = _S1441;
        _S1412 = _S1442;
        _S1419 = _S1443;
        _S1420 = _S1444;
        _S1413 = _S1445;
        _S1421 = _S1446;
        _S1422 = _S1447;
    }
    else
    {
        _runFlag_27 = false;
        _runFlag_28 = false;
        _S1414 = _S1409;
        _S1415 = _S1409;
        _S1416 = _S1409;
        _S1417 = _S1409;
        _S1418 = _S1409;
        _runFlag_29 = false;
        _S1411 = false;
        _runFlag_30 = false;
        _S1412 = false;
        _S1419 = _S1409;
        _S1420 = _S1409;
        _S1413 = false;
        _S1421 = _S1409;
        _S1422 = _S1409;
    }
    float4  _S1448 = make_float4 (0.0f);
    float4  _S1449;
    if(_S1410)
    {
        if(_runFlag_27)
        {
            if(_runFlag_28)
            {
                float3  _S1450 = v_normal_4 / _S1414;
                float3  _S1451 = _S1415 * - _S1450;
                float3  _S1452 = _S1416 * _S1450;
                float _S1453 = _S1451.x + _S1451.y + _S1451.z;
                DiffPair_vectorx3Cfloatx2C3x3E_0 _S1454;
                (&_S1454)->primal_0 = _S1415;
                (&_S1454)->differential_0 = _S1409;
                s_bwd_length_impl_0(&_S1454, _S1453);
                _S1414 = _S1452 + _S1454.differential_0;
            }
            else
            {
                _S1414 = v_normal_4;
            }
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1455;
            (&_S1455)->primal_0 = _S1415;
            (&_S1455)->differential_0 = _S1409;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1456;
            (&_S1456)->primal_0 = _S1415;
            (&_S1456)->differential_0 = _S1409;
            s_bwd_prop_dot_0(&_S1455, &_S1456, 0.0f);
            float3  _S1457 = _S1456.differential_0 + _S1455.differential_0 + _S1414;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1458;
            (&_S1458)->primal_0 = _S1417;
            (&_S1458)->differential_0 = _S1409;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1459;
            (&_S1459)->primal_0 = _S1418;
            (&_S1459)->differential_0 = _S1409;
            s_bwd_prop_cross_0(&_S1458, &_S1459, _S1457);
            float3  s_diff_dy_T_7 = - _S1459.differential_0;
            float3  _S1460 = - s_diff_dy_T_7;
            float3  _S1461 = - _S1458.differential_0;
            FixedArray<float3 , 4>  _S1462;
            _S1462[int(0)] = _S1409;
            _S1462[int(1)] = _S1409;
            _S1462[int(2)] = _S1409;
            _S1462[int(3)] = _S1409;
            _S1462[int(2)] = _S1460;
            _S1462[int(3)] = s_diff_dy_T_7;
            _S1462[int(0)] = _S1461;
            _S1462[int(1)] = _S1458.differential_0;
            points_15[int(0)] = _S1462[int(0)];
            points_15[int(1)] = _S1462[int(1)];
            points_15[int(2)] = _S1462[int(2)];
            points_15[int(3)] = _S1462[int(3)];
        }
        else
        {
            points_15[int(0)] = _S1409;
            points_15[int(1)] = _S1409;
            points_15[int(2)] = _S1409;
            points_15[int(3)] = _S1409;
        }
        if(_runFlag_29)
        {
            if(_S1411)
            {
                if(_runFlag_30)
                {
                    FixedArray<float3 , 4>  _S1463 = points_15;
                    FixedArray<float3 , 4>  _S1464 = points_15;
                    FixedArray<float3 , 4>  _S1465 = points_15;
                    FixedArray<float3 , 4>  _S1466 = points_15;
                    if(_S1412)
                    {
                        float3  _S1467 = _S1419 * _S1466[int(3)];
                        float _S1468 = _S1467.x + _S1467.y + _S1467.z;
                        float4  _S1469 = _S1448;
                        *&((&_S1469)->w) = _S1468;
                        points_15[int(0)] = _S1463[int(0)];
                        points_15[int(1)] = _S1464[int(1)];
                        points_15[int(2)] = _S1465[int(2)];
                        points_15[int(3)] = _S1409;
                        _S1449 = _S1469;
                    }
                    else
                    {
                        points_15[int(0)] = _S1463[int(0)];
                        points_15[int(1)] = _S1464[int(1)];
                        points_15[int(2)] = _S1465[int(2)];
                        points_15[int(3)] = _S1466[int(3)];
                        _S1449 = _S1448;
                    }
                    float3  _S1470 = _S1420 * points_15[int(2)];
                    float _S1471 = _S1470.x + _S1470.y + _S1470.z;
                    FixedArray<float3 , 4>  _S1472 = points_15;
                    FixedArray<float3 , 4>  _S1473 = points_15;
                    float4  _S1474 = _S1448;
                    *&((&_S1474)->z) = _S1471;
                    float4  _S1475 = _S1449 + _S1474;
                    points_15[int(0)] = points_15[int(0)];
                    points_15[int(1)] = _S1472[int(1)];
                    points_15[int(2)] = _S1409;
                    points_15[int(3)] = _S1473[int(3)];
                    _S1449 = _S1475;
                }
                else
                {
                    FixedArray<float3 , 4>  _S1476 = points_15;
                    FixedArray<float3 , 4>  _S1477 = points_15;
                    FixedArray<float3 , 4>  _S1478 = points_15;
                    points_15[int(0)] = points_15[int(0)];
                    points_15[int(1)] = _S1476[int(1)];
                    points_15[int(2)] = _S1477[int(2)];
                    points_15[int(3)] = _S1478[int(3)];
                    _S1449 = _S1448;
                }
            }
            else
            {
                FixedArray<float3 , 4>  _S1479 = points_15;
                FixedArray<float3 , 4>  _S1480 = points_15;
                FixedArray<float3 , 4>  _S1481 = points_15;
                points_15[int(0)] = points_15[int(0)];
                points_15[int(1)] = _S1479[int(1)];
                points_15[int(2)] = _S1480[int(2)];
                points_15[int(3)] = _S1481[int(3)];
                _S1449 = _S1448;
            }
            if(_S1413)
            {
                FixedArray<float3 , 4>  _S1482 = points_15;
                float3  _S1483 = _S1421 * points_15[int(1)];
                float _S1484 = _S1483.x + _S1483.y + _S1483.z;
                float4  _S1485 = _S1448;
                *&((&_S1485)->y) = _S1484;
                float4  _S1486 = _S1449 + _S1485;
                points_15[int(0)] = _S1409;
                points_15[int(1)] = _S1409;
                points_15[int(2)] = _S1409;
                points_15[int(3)] = _S1409;
                _S1414 = _S1482[int(0)];
                _S1449 = _S1486;
            }
            else
            {
                FixedArray<float3 , 4>  _S1487 = points_15;
                FixedArray<float3 , 4>  _S1488 = points_15;
                FixedArray<float3 , 4>  _S1489 = points_15;
                points_15[int(0)] = points_15[int(0)];
                points_15[int(1)] = _S1487[int(1)];
                points_15[int(2)] = _S1488[int(2)];
                points_15[int(3)] = _S1489[int(3)];
                _S1414 = _S1409;
            }
            float3  _S1490 = _S1422 * (points_15[int(0)] + _S1414);
            float _S1491 = _S1490.x + _S1490.y + _S1490.z;
            float4  _S1492 = _S1448;
            *&((&_S1492)->x) = _S1491;
            _S1449 = _S1449 + _S1492;
        }
        else
        {
            _S1449 = _S1448;
        }
    }
    else
    {
        _S1449 = _S1448;
    }
    *v_depths_6 = _S1449;
    return;
}

inline __device__ float ray_depth_to_linear_depth_factor_rational(float2  pix_center_17, float4  intrins_29, FixedArray<float, 8>  dist_coeffs_33, int camera_model_31)
{
    float _S1493;
    for(;;)
    {
        float2  uv_85 = (pix_center_17 - float2 {intrins_29.z, intrins_29.w}) / float2 {intrins_29.x, intrins_29.y};
        FixedArray<float, 8>  _S1494 = dist_coeffs_33;
        float2  uv_u_42;
        bool _S1495 = undistort_point_3(uv_85, &_S1494, int(12), &uv_u_42);
        if(!_S1495)
        {
            _S1493 = 0.0f;
            break;
        }
        float3  raydir_23 = unproject_raydir_0(uv_u_42, camera_model_31, false);
        _S1493 = float((F32_sign((raydir_23.z)))) / length_0(raydir_23);
        break;
    }
    return _S1493;
}

inline __device__ float depth_normal_loss_rational(float2  pix_center_18, float4  intrins_30, FixedArray<float, 8>  dist_coeffs_34, int camera_model_32, bool is_ray_depth_28, float4  depths_14, float3  gt_normal_6)
{
    float _S1496;
    for(;;)
    {
        float3  _S1497;
        float3  * _S1498;
        float3  * _S1499;
        float3  * _S1500;
        float3  * _S1501;
        int _S1502;
        FixedArray<float3 , 5>  points_16;
        for(;;)
        {
            float2  _S1503 = float2 {intrins_30.z, intrins_30.w};
            float2  _S1504 = float2 {intrins_30.x, intrins_30.y};
            float2  uv_86 = (pix_center_18 + make_float2 (-1.0f, -0.0f) - _S1503) / _S1504;
            FixedArray<float, 8>  _S1505 = dist_coeffs_34;
            float2  uv_u_43;
            bool _S1506 = undistort_point_3(uv_86, &_S1505, int(12), &uv_u_43);
            float3  _S1507 = make_float3 (0.0f);
            if(!_S1506)
            {
                _S1502 = int(0);
                _S1501 = nullptr;
                _S1500 = nullptr;
                _S1499 = nullptr;
                _S1498 = nullptr;
                _S1497 = _S1507;
                break;
            }
            float3  raydir_24 = unproject_raydir_0(uv_u_43, camera_model_32, is_ray_depth_28);
            points_16[int(0)] = make_float3 (depths_14.x) * raydir_24;
            float2  uv_87 = (pix_center_18 + make_float2 (1.0f, -0.0f) - _S1503) / _S1504;
            FixedArray<float, 8>  _S1508 = dist_coeffs_34;
            float2  uv_u_44;
            bool _S1509 = undistort_point_3(uv_87, &_S1508, int(12), &uv_u_44);
            if(!_S1509)
            {
                _S1502 = int(0);
                _S1501 = nullptr;
                _S1500 = &points_16[int(0)];
                _S1499 = nullptr;
                _S1498 = nullptr;
                _S1497 = _S1507;
                break;
            }
            float3  raydir_25 = unproject_raydir_0(uv_u_44, camera_model_32, is_ray_depth_28);
            points_16[int(1)] = make_float3 (depths_14.y) * raydir_25;
            float2  uv_88 = (pix_center_18 + make_float2 (0.0f, -1.0f) - _S1503) / _S1504;
            FixedArray<float, 8>  _S1510 = dist_coeffs_34;
            float2  uv_u_45;
            bool _S1511 = undistort_point_3(uv_88, &_S1510, int(12), &uv_u_45);
            if(!_S1511)
            {
                _S1502 = int(0);
                _S1501 = &points_16[int(1)];
                _S1500 = &points_16[int(0)];
                _S1499 = nullptr;
                _S1498 = nullptr;
                _S1497 = _S1507;
                break;
            }
            float3  raydir_26 = unproject_raydir_0(uv_u_45, camera_model_32, is_ray_depth_28);
            points_16[int(2)] = make_float3 (depths_14.z) * raydir_26;
            float2  uv_89 = (pix_center_18 + make_float2 (0.0f, 1.0f) - _S1503) / _S1504;
            FixedArray<float, 8>  _S1512 = dist_coeffs_34;
            float2  uv_u_46;
            bool _S1513 = undistort_point_3(uv_89, &_S1512, int(12), &uv_u_46);
            if(!_S1513)
            {
                _S1502 = int(0);
                _S1501 = &points_16[int(1)];
                _S1500 = &points_16[int(0)];
                _S1499 = nullptr;
                _S1498 = &points_16[int(2)];
                _S1497 = _S1507;
                break;
            }
            float3  raydir_27 = unproject_raydir_0(uv_u_46, camera_model_32, is_ray_depth_28);
            points_16[int(3)] = make_float3 (depths_14.w) * raydir_27;
            float2  uv_90 = (pix_center_18 + make_float2 (0.0f) * make_float2 (0.0f, 3.0f) - _S1503) / _S1504;
            FixedArray<float, 8>  _S1514 = dist_coeffs_34;
            float2  uv_u_47;
            bool _S1515 = undistort_point_3(uv_90, &_S1514, int(12), &uv_u_47);
            if(!_S1515)
            {
                _S1502 = int(0);
                _S1501 = &points_16[int(1)];
                _S1500 = &points_16[int(0)];
                _S1499 = &points_16[int(3)];
                _S1498 = &points_16[int(2)];
                _S1497 = _S1507;
                break;
            }
            float3  raydir_28 = unproject_raydir_0(uv_u_47, camera_model_32, is_ray_depth_28);
            _S1502 = int(1);
            _S1501 = &points_16[int(1)];
            _S1500 = &points_16[int(0)];
            _S1499 = &points_16[int(3)];
            _S1498 = &points_16[int(2)];
            _S1497 = raydir_28;
            break;
        }
        if(_S1502 != int(1))
        {
            _S1496 = 0.0f;
            break;
        }
        float3  normal_19 = cross_0(*_S1501 - *_S1500, - (*_S1499 - *_S1498));
        float3  normal_20;
        if((dot_0(normal_19, normal_19)) != 0.0f)
        {
            normal_20 = normalize_0(normal_19);
        }
        else
        {
            normal_20 = normal_19;
        }
        float3  _S1516;
        if((dot_0(gt_normal_6, gt_normal_6)) != 0.0f)
        {
            _S1516 = normalize_0(gt_normal_6);
        }
        else
        {
            _S1516 = gt_normal_6;
        }
        _S1496 = (1.0f - dot_0(normal_20, _S1516) + 0.00100000004749745f) / ((F32_max((dot_0(normal_20, - normalize_0(_S1497))), (0.0f))) + 0.00100000004749745f);
        break;
    }
    return _S1496;
}

struct s_bwd_prop_depth_normal_loss_Intermediates_3
{
    float2  _S1517;
    bool _S1518;
    float2  _S1519;
    bool _S1520;
    float2  _S1521;
    bool _S1522;
    float2  _S1523;
    bool _S1524;
    float2  _S1525;
    bool _S1526;
};

inline __device__ void depth_normal_loss_vjp_rational(float2  pix_center_19, float4  intrins_31, FixedArray<float, 8>  dist_coeffs_35, int camera_model_33, bool is_ray_depth_29, float4  depths_15, float3  gt_normal_7, float v_loss_3, float4  * v_depths_7, float3  * v_gt_normal_3)
{
    float2  _S1527 = make_float2 (0.0f);
    s_bwd_prop_depth_normal_loss_Intermediates_3 _S1528;
    (&_S1528)->_S1517 = _S1527;
    (&_S1528)->_S1518 = false;
    (&_S1528)->_S1519 = _S1527;
    (&_S1528)->_S1520 = false;
    (&_S1528)->_S1521 = _S1527;
    (&_S1528)->_S1522 = false;
    (&_S1528)->_S1523 = _S1527;
    (&_S1528)->_S1524 = false;
    (&_S1528)->_S1525 = _S1527;
    (&_S1528)->_S1526 = false;
    (&_S1528)->_S1519 = _S1527;
    (&_S1528)->_S1520 = false;
    (&_S1528)->_S1521 = _S1527;
    (&_S1528)->_S1522 = false;
    (&_S1528)->_S1523 = _S1527;
    (&_S1528)->_S1524 = false;
    (&_S1528)->_S1525 = _S1527;
    (&_S1528)->_S1526 = false;
    float2  _S1529 = float2 {intrins_31.z, intrins_31.w};
    float2  _S1530 = float2 {intrins_31.x, intrins_31.y};
    float2  uv_91 = (pix_center_19 + make_float2 (-1.0f, -0.0f) - _S1529) / _S1530;
    float2  _S1531 = _S1527;
    FixedArray<float, 8>  _S1532 = dist_coeffs_35;
    bool _S1533 = undistort_point_3(uv_91, &_S1532, int(12), &_S1531);
    (&_S1528)->_S1517 = _S1531;
    (&_S1528)->_S1518 = _S1533;
    bool _S1534 = !!_S1533;
    bool _runFlag_31;
    if(_S1534)
    {
        float2  uv_92 = (pix_center_19 + make_float2 (1.0f, -0.0f) - _S1529) / _S1530;
        float2  _S1535 = _S1527;
        FixedArray<float, 8>  _S1536 = dist_coeffs_35;
        bool _S1537 = undistort_point_3(uv_92, &_S1536, int(12), &_S1535);
        (&_S1528)->_S1519 = _S1535;
        (&_S1528)->_S1520 = _S1537;
        if(!_S1537)
        {
            _runFlag_31 = false;
        }
        else
        {
            _runFlag_31 = _S1534;
        }
        if(_runFlag_31)
        {
            float2  uv_93 = (pix_center_19 + make_float2 (0.0f, -1.0f) - _S1529) / _S1530;
            float2  _S1538 = _S1527;
            FixedArray<float, 8>  _S1539 = dist_coeffs_35;
            bool _S1540 = undistort_point_3(uv_93, &_S1539, int(12), &_S1538);
            (&_S1528)->_S1521 = _S1538;
            (&_S1528)->_S1522 = _S1540;
            if(!_S1540)
            {
                _runFlag_31 = false;
            }
            if(_runFlag_31)
            {
                float2  uv_94 = (pix_center_19 + make_float2 (0.0f, 1.0f) - _S1529) / _S1530;
                float2  _S1541 = _S1527;
                FixedArray<float, 8>  _S1542 = dist_coeffs_35;
                bool _S1543 = undistort_point_3(uv_94, &_S1542, int(12), &_S1541);
                (&_S1528)->_S1523 = _S1541;
                (&_S1528)->_S1524 = _S1543;
                if(!_S1543)
                {
                    _runFlag_31 = false;
                }
                if(_runFlag_31)
                {
                    float2  uv_95 = (pix_center_19 - _S1529) / _S1530;
                    float2  _S1544 = _S1527;
                    FixedArray<float, 8>  _S1545 = dist_coeffs_35;
                    bool _S1546 = undistort_point_3(uv_95, &_S1545, int(12), &_S1544);
                    (&_S1528)->_S1525 = _S1544;
                    (&_S1528)->_S1526 = _S1546;
                }
            }
        }
    }
    s_bwd_prop_depth_normal_loss_Intermediates_3 _S1547 = _S1528;
    float3  _S1548 = make_float3 (0.0f);
    bool _S1549 = !!_S1528._S1518;
    bool _runFlag_32;
    bool _runFlag_33;
    bool _runFlag_34;
    int _S1550;
    float3  raydir_29;
    float3  _S1551;
    float3  _S1552;
    float3  _S1553;
    float3  _S1554;
    FixedArray<float3 , 5>  points_17;
    if(_S1549)
    {
        float3  _S1555 = s_primal_ctx_unproject_raydir_0(_S1547._S1517, camera_model_33, is_ray_depth_29);
        float3  _S1556 = make_float3 (depths_15.x) * _S1555;
        if(!_S1547._S1520)
        {
            _runFlag_31 = false;
        }
        else
        {
            _runFlag_31 = _S1549;
        }
        if(_runFlag_31)
        {
            float3  _S1557 = s_primal_ctx_unproject_raydir_0(_S1547._S1519, camera_model_33, is_ray_depth_29);
            float3  _S1558 = make_float3 (depths_15.y) * _S1557;
            if(!_S1547._S1522)
            {
                _runFlag_32 = false;
            }
            else
            {
                _runFlag_32 = _runFlag_31;
            }
            if(_runFlag_32)
            {
                float3  _S1559 = s_primal_ctx_unproject_raydir_0(_S1547._S1521, camera_model_33, is_ray_depth_29);
                float3  _S1560 = make_float3 (depths_15.z) * _S1559;
                if(!_S1547._S1524)
                {
                    _runFlag_33 = false;
                }
                else
                {
                    _runFlag_33 = _runFlag_32;
                }
                if(_runFlag_33)
                {
                    float3  _S1561 = s_primal_ctx_unproject_raydir_0(_S1547._S1523, camera_model_33, is_ray_depth_29);
                    float3  _S1562 = make_float3 (depths_15.w) * _S1561;
                    if(!_S1547._S1526)
                    {
                        _runFlag_34 = false;
                    }
                    else
                    {
                        _runFlag_34 = _runFlag_33;
                    }
                    if(_runFlag_34)
                    {
                        float3  _S1563 = s_primal_ctx_unproject_raydir_0(_S1547._S1525, camera_model_33, is_ray_depth_29);
                        _S1550 = int(1);
                        raydir_29 = _S1563;
                    }
                    else
                    {
                        _S1550 = int(0);
                        raydir_29 = _S1561;
                    }
                    points_17[int(0)] = _S1556;
                    points_17[int(1)] = _S1558;
                    points_17[int(2)] = _S1560;
                    points_17[int(3)] = _S1562;
                    points_17[int(4)] = _S1548;
                    _S1551 = _S1561;
                }
                else
                {
                    _S1550 = int(0);
                    raydir_29 = _S1559;
                    points_17[int(0)] = _S1556;
                    points_17[int(1)] = _S1558;
                    points_17[int(2)] = _S1560;
                    points_17[int(3)] = _S1548;
                    points_17[int(4)] = _S1548;
                    _S1551 = _S1548;
                }
                _S1552 = _S1559;
            }
            else
            {
                _S1550 = int(0);
                raydir_29 = _S1557;
                points_17[int(0)] = _S1556;
                points_17[int(1)] = _S1558;
                points_17[int(2)] = _S1548;
                points_17[int(3)] = _S1548;
                points_17[int(4)] = _S1548;
                _runFlag_33 = false;
                _S1551 = _S1548;
                _S1552 = _S1548;
            }
            _S1553 = _S1557;
        }
        else
        {
            _S1550 = int(0);
            raydir_29 = _S1555;
            points_17[int(0)] = _S1556;
            points_17[int(1)] = _S1548;
            points_17[int(2)] = _S1548;
            points_17[int(3)] = _S1548;
            points_17[int(4)] = _S1548;
            _runFlag_32 = false;
            _runFlag_33 = false;
            _S1551 = _S1548;
            _S1552 = _S1548;
            _S1553 = _S1548;
        }
        _S1554 = _S1555;
    }
    else
    {
        _S1550 = int(0);
        points_17[int(0)] = _S1548;
        points_17[int(1)] = _S1548;
        points_17[int(2)] = _S1548;
        points_17[int(3)] = _S1548;
        points_17[int(4)] = _S1548;
        _runFlag_31 = false;
        _runFlag_32 = false;
        _runFlag_33 = false;
        _S1551 = _S1548;
        _S1552 = _S1548;
        _S1553 = _S1548;
        _S1554 = _S1548;
    }
    bool _S1564 = !(_S1550 != int(1));
    bool _S1565;
    float3  normal_21;
    float3  _S1566;
    float3  _S1567;
    float3  _S1568;
    float3  _S1569;
    float _S1570;
    float _S1571;
    float _S1572;
    float _S1573;
    if(_S1564)
    {
        float3  dx_8 = points_17[int(1)] - points_17[int(0)];
        float3  _S1574 = - (points_17[int(3)] - points_17[int(2)]);
        float3  _S1575 = s_primal_ctx_cross_0(dx_8, _S1574);
        bool _S1576 = (s_primal_ctx_dot_0(_S1575, _S1575)) != 0.0f;
        if(_S1576)
        {
            normal_21 = normalize_0(_S1575);
        }
        else
        {
            normal_21 = _S1575;
        }
        bool _S1577 = (s_primal_ctx_dot_0(gt_normal_7, gt_normal_7)) != 0.0f;
        if(_S1577)
        {
            _S1566 = normalize_0(gt_normal_7);
        }
        else
        {
            _S1566 = gt_normal_7;
        }
        float3  _S1578 = - normalize_0(raydir_29);
        float _S1579 = s_primal_ctx_dot_0(normal_21, _S1578);
        float _S1580 = 1.0f - s_primal_ctx_dot_0(normal_21, _S1566) + 0.00100000004749745f;
        float _S1581 = (F32_max((_S1579), (0.0f))) + 0.00100000004749745f;
        _S1570 = _S1581 * _S1581;
        _S1571 = _S1580;
        _S1572 = _S1581;
        _S1573 = _S1579;
        raydir_29 = normal_21;
        normal_21 = _S1578;
        _runFlag_34 = _S1577;
        _S1565 = _S1576;
        _S1567 = _S1575;
        _S1568 = dx_8;
        _S1569 = _S1574;
    }
    else
    {
        _S1570 = 0.0f;
        _S1571 = 0.0f;
        _S1572 = 0.0f;
        _S1573 = 0.0f;
        raydir_29 = _S1548;
        normal_21 = _S1548;
        _S1566 = _S1548;
        _runFlag_34 = false;
        _S1565 = false;
        _S1567 = _S1548;
        _S1568 = _S1548;
        _S1569 = _S1548;
    }
    float4  _S1582 = make_float4 (0.0f);
    if(_S1564)
    {
        float _S1583 = v_loss_3 / _S1570;
        float _S1584 = _S1571 * - _S1583;
        float s_diff_num_T_3 = _S1572 * _S1583;
        DiffPair_float_0 _S1585;
        (&_S1585)->primal_0 = _S1573;
        (&_S1585)->differential_0 = 0.0f;
        DiffPair_float_0 _S1586;
        (&_S1586)->primal_0 = 0.0f;
        (&_S1586)->differential_0 = 0.0f;
        _d_max_0(&_S1585, &_S1586, _S1584);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1587;
        (&_S1587)->primal_0 = raydir_29;
        (&_S1587)->differential_0 = _S1548;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1588;
        (&_S1588)->primal_0 = normal_21;
        (&_S1588)->differential_0 = _S1548;
        s_bwd_prop_dot_0(&_S1587, &_S1588, _S1585.differential_0);
        float _S1589 = - s_diff_num_T_3;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1590;
        (&_S1590)->primal_0 = raydir_29;
        (&_S1590)->differential_0 = _S1548;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1591;
        (&_S1591)->primal_0 = _S1566;
        (&_S1591)->differential_0 = _S1548;
        s_bwd_prop_dot_0(&_S1590, &_S1591, _S1589);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1592 = _S1591;
        float3  _S1593 = _S1587.differential_0 + _S1590.differential_0;
        if(_runFlag_34)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1594;
            (&_S1594)->primal_0 = gt_normal_7;
            (&_S1594)->differential_0 = _S1548;
            s_bwd_normalize_impl_0(&_S1594, _S1592.differential_0);
            raydir_29 = _S1594.differential_0;
        }
        else
        {
            raydir_29 = _S1592.differential_0;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1595;
        (&_S1595)->primal_0 = gt_normal_7;
        (&_S1595)->differential_0 = _S1548;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1596;
        (&_S1596)->primal_0 = gt_normal_7;
        (&_S1596)->differential_0 = _S1548;
        s_bwd_prop_dot_0(&_S1595, &_S1596, 0.0f);
        float3  _S1597 = _S1596.differential_0 + _S1595.differential_0 + raydir_29;
        if(_S1565)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1598;
            (&_S1598)->primal_0 = _S1567;
            (&_S1598)->differential_0 = _S1548;
            s_bwd_normalize_impl_0(&_S1598, _S1593);
            raydir_29 = _S1598.differential_0;
        }
        else
        {
            raydir_29 = _S1593;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1599;
        (&_S1599)->primal_0 = _S1567;
        (&_S1599)->differential_0 = _S1548;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1600;
        (&_S1600)->primal_0 = _S1567;
        (&_S1600)->differential_0 = _S1548;
        s_bwd_prop_dot_0(&_S1599, &_S1600, 0.0f);
        float3  _S1601 = _S1600.differential_0 + _S1599.differential_0 + raydir_29;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1602;
        (&_S1602)->primal_0 = _S1568;
        (&_S1602)->differential_0 = _S1548;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1603;
        (&_S1603)->primal_0 = _S1569;
        (&_S1603)->differential_0 = _S1548;
        s_bwd_prop_cross_0(&_S1602, &_S1603, _S1601);
        float3  s_diff_dy_T_8 = - _S1603.differential_0;
        float3  _S1604 = - s_diff_dy_T_8;
        float3  _S1605 = - _S1602.differential_0;
        FixedArray<float3 , 5>  _S1606;
        _S1606[int(0)] = _S1548;
        _S1606[int(1)] = _S1548;
        _S1606[int(2)] = _S1548;
        _S1606[int(3)] = _S1548;
        _S1606[int(4)] = _S1548;
        _S1606[int(2)] = _S1604;
        _S1606[int(3)] = s_diff_dy_T_8;
        _S1606[int(0)] = _S1605;
        _S1606[int(1)] = _S1602.differential_0;
        points_17[int(0)] = _S1606[int(0)];
        points_17[int(1)] = _S1606[int(1)];
        points_17[int(2)] = _S1606[int(2)];
        points_17[int(3)] = _S1606[int(3)];
        points_17[int(4)] = _S1606[int(4)];
        raydir_29 = _S1597;
    }
    else
    {
        points_17[int(0)] = _S1548;
        points_17[int(1)] = _S1548;
        points_17[int(2)] = _S1548;
        points_17[int(3)] = _S1548;
        points_17[int(4)] = _S1548;
        raydir_29 = _S1548;
    }
    float4  _S1607;
    if(_S1549)
    {
        if(_runFlag_31)
        {
            if(_runFlag_32)
            {
                if(_runFlag_33)
                {
                    FixedArray<float3 , 5>  _S1608 = points_17;
                    FixedArray<float3 , 5>  _S1609 = points_17;
                    FixedArray<float3 , 5>  _S1610 = points_17;
                    float3  _S1611 = _S1551 * points_17[int(3)];
                    float _S1612 = _S1611.x + _S1611.y + _S1611.z;
                    float4  _S1613 = _S1582;
                    *&((&_S1613)->w) = _S1612;
                    points_17[int(0)] = _S1548;
                    points_17[int(1)] = _S1548;
                    points_17[int(2)] = _S1548;
                    points_17[int(3)] = _S1548;
                    points_17[int(4)] = _S1548;
                    _S1551 = _S1610[int(2)];
                    normal_21 = _S1608[int(0)];
                    _S1566 = _S1609[int(1)];
                    _S1607 = _S1613;
                }
                else
                {
                    FixedArray<float3 , 5>  _S1614 = points_17;
                    FixedArray<float3 , 5>  _S1615 = points_17;
                    FixedArray<float3 , 5>  _S1616 = points_17;
                    FixedArray<float3 , 5>  _S1617 = points_17;
                    points_17[int(0)] = points_17[int(0)];
                    points_17[int(1)] = _S1614[int(1)];
                    points_17[int(2)] = _S1615[int(2)];
                    points_17[int(3)] = _S1616[int(3)];
                    points_17[int(4)] = _S1617[int(4)];
                    _S1551 = _S1548;
                    normal_21 = _S1548;
                    _S1566 = _S1548;
                    _S1607 = _S1582;
                }
                float3  _S1618 = _S1552 * (points_17[int(2)] + _S1551);
                float _S1619 = _S1618.x + _S1618.y + _S1618.z;
                float3  _S1620 = points_17[int(0)] + normal_21;
                float3  _S1621 = points_17[int(1)] + _S1566;
                float4  _S1622 = _S1582;
                *&((&_S1622)->z) = _S1619;
                float4  _S1623 = _S1607 + _S1622;
                points_17[int(0)] = _S1548;
                points_17[int(1)] = _S1548;
                points_17[int(2)] = _S1548;
                points_17[int(3)] = _S1548;
                points_17[int(4)] = _S1548;
                _S1551 = _S1621;
                _S1552 = _S1620;
                _S1607 = _S1623;
            }
            else
            {
                FixedArray<float3 , 5>  _S1624 = points_17;
                FixedArray<float3 , 5>  _S1625 = points_17;
                FixedArray<float3 , 5>  _S1626 = points_17;
                FixedArray<float3 , 5>  _S1627 = points_17;
                points_17[int(0)] = points_17[int(0)];
                points_17[int(1)] = _S1624[int(1)];
                points_17[int(2)] = _S1625[int(2)];
                points_17[int(3)] = _S1626[int(3)];
                points_17[int(4)] = _S1627[int(4)];
                _S1551 = _S1548;
                _S1552 = _S1548;
                _S1607 = _S1582;
            }
            float3  _S1628 = _S1553 * (points_17[int(1)] + _S1551);
            float _S1629 = _S1628.x + _S1628.y + _S1628.z;
            float3  _S1630 = points_17[int(0)] + _S1552;
            float4  _S1631 = _S1582;
            *&((&_S1631)->y) = _S1629;
            float4  _S1632 = _S1607 + _S1631;
            points_17[int(0)] = _S1548;
            points_17[int(1)] = _S1548;
            points_17[int(2)] = _S1548;
            points_17[int(3)] = _S1548;
            points_17[int(4)] = _S1548;
            _S1551 = _S1630;
            _S1607 = _S1632;
        }
        else
        {
            FixedArray<float3 , 5>  _S1633 = points_17;
            FixedArray<float3 , 5>  _S1634 = points_17;
            FixedArray<float3 , 5>  _S1635 = points_17;
            FixedArray<float3 , 5>  _S1636 = points_17;
            points_17[int(0)] = points_17[int(0)];
            points_17[int(1)] = _S1633[int(1)];
            points_17[int(2)] = _S1634[int(2)];
            points_17[int(3)] = _S1635[int(3)];
            points_17[int(4)] = _S1636[int(4)];
            _S1551 = _S1548;
            _S1607 = _S1582;
        }
        float3  _S1637 = _S1554 * (points_17[int(0)] + _S1551);
        float _S1638 = _S1637.x + _S1637.y + _S1637.z;
        float4  _S1639 = _S1582;
        *&((&_S1639)->x) = _S1638;
        _S1607 = _S1607 + _S1639;
    }
    else
    {
        _S1607 = _S1582;
    }
    *v_depths_7 = _S1607;
    *v_gt_normal_3 = raydir_29;
    return;
}

