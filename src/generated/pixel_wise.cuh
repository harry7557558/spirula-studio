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

inline __device__ float splat_dc_encode(float dc_0)
{
    return 2.0f * (F32_log(((F32_max((0.564189612865448f * dc_0 + 1.0f), (9.999999960041972e-13f))))));
}

inline __device__ float splat_dc_decode(float x_8)
{
    return ((F32_exp((0.5f * x_8))) - 1.0f) * 1.77245378494262695f;
}

struct DiffPair_matrixx3Cfloatx2C3x2C3x3E_0
{
    Matrix<float, 3, 3>  primal_0;
    Matrix<float, 3, 3>  differential_0;
};

inline __device__ void _d_mul_0(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * left_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * right_0, float3  dOut_3)
{
    float _S38 = (*left_0).primal_0.rows[int(0)].x * dOut_3.x;
    Matrix<float, 3, 3>  left_d_result_0;
    *&(((&left_d_result_0)->rows + (int(0)))->x) = (*right_0).primal_0.x * dOut_3.x;
    float sum_0 = _S38 + (*left_0).primal_0.rows[int(1)].x * dOut_3.y;
    *&(((&left_d_result_0)->rows + (int(1)))->x) = (*right_0).primal_0.x * dOut_3.y;
    float sum_1 = sum_0 + (*left_0).primal_0.rows[int(2)].x * dOut_3.z;
    *&(((&left_d_result_0)->rows + (int(2)))->x) = (*right_0).primal_0.x * dOut_3.z;
    float3  right_d_result_0;
    *&((&right_d_result_0)->x) = sum_1;
    float _S39 = (*left_0).primal_0.rows[int(0)].y * dOut_3.x;
    *&(((&left_d_result_0)->rows + (int(0)))->y) = (*right_0).primal_0.y * dOut_3.x;
    float sum_2 = _S39 + (*left_0).primal_0.rows[int(1)].y * dOut_3.y;
    *&(((&left_d_result_0)->rows + (int(1)))->y) = (*right_0).primal_0.y * dOut_3.y;
    float sum_3 = sum_2 + (*left_0).primal_0.rows[int(2)].y * dOut_3.z;
    *&(((&left_d_result_0)->rows + (int(2)))->y) = (*right_0).primal_0.y * dOut_3.z;
    *&((&right_d_result_0)->y) = sum_3;
    float _S40 = (*left_0).primal_0.rows[int(0)].z * dOut_3.x;
    *&(((&left_d_result_0)->rows + (int(0)))->z) = (*right_0).primal_0.z * dOut_3.x;
    float sum_4 = _S40 + (*left_0).primal_0.rows[int(1)].z * dOut_3.y;
    *&(((&left_d_result_0)->rows + (int(1)))->z) = (*right_0).primal_0.z * dOut_3.y;
    float sum_5 = sum_4 + (*left_0).primal_0.rows[int(2)].z * dOut_3.z;
    *&(((&left_d_result_0)->rows + (int(2)))->z) = (*right_0).primal_0.z * dOut_3.z;
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

inline __device__ void xfer_pass_grad_0(DiffPair_float_0 * dp_0, float v_out_1)
{
    dp_0->primal_0 = (*dp_0).primal_0;
    dp_0->differential_0 = v_out_1;
    return;
}

inline __device__ float xfer_max0_0(float x_9)
{
    return (F32_max((x_9), (0.0f)));
}

inline __device__ float xfer_filmic_0(float x_10)
{
    float t_0 = xfer_max0_0(x_10 - 0.00400000018998981f);
    float _S41 = 6.19999980926513672f * t_0;
    return t_0 * (_S41 + 0.5f) / (t_0 * (_S41 + 1.70000004768371582f) + 0.05999999865889549f);
}

inline __device__ float xfer_aces_0(float x_11)
{
    return x_11 * (2.50999999046325684f * x_11 + 0.02999999932944775f) / (x_11 * (2.43000006675720215f * x_11 + 0.5899999737739563f) + 0.14000000059604645f);
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
    float _S42 = 0.15000000596046448f * x_14;
    return (x_14 * (_S42 + 0.05000000074505806f) + 0.00400000018998981f) / (x_14 * (_S42 + 0.5f) + 0.06000000238418579f) - 0.06666666269302368f;
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
    float _S43;
    if(transfer_0 == int(2))
    {
        float _S44 = xfer_clamp01_0(xfer_aces_0(xfer_max0_0(x_16)));
        if(_S44 < 0.00313080009073019f)
        {
            _S43 = _S44 * 12.92000007629394531f;
        }
        else
        {
            _S43 = 1.0549999475479126f * (F32_pow((_S44), (0.4166666567325592f))) - 0.05499999970197678f;
        }
        return _S43;
    }
    if(transfer_0 == int(4))
    {
        float _S45 = xfer_clamp01_0(xfer_uncharted2_0(x_16));
        if(_S45 < 0.00313080009073019f)
        {
            _S43 = _S45 * 12.92000007629394531f;
        }
        else
        {
            _S43 = 1.0549999475479126f * (F32_pow((_S45), (0.4166666567325592f))) - 0.05499999970197678f;
        }
        return _S43;
    }
    if(transfer_0 == int(1))
    {
        float _S46 = xfer_clamp01_0(x_16);
        if(_S46 < 0.00313080009073019f)
        {
            _S43 = _S46 * 12.92000007629394531f;
        }
        else
        {
            _S43 = 1.0549999475479126f * (F32_pow((_S46), (0.4166666567325592f))) - 0.05499999970197678f;
        }
        return _S43;
    }
    float _S47 = xfer_max0_0(x_16);
    if(_S47 < 0.00313080009073019f)
    {
        _S43 = _S47 * 12.92000007629394531f;
    }
    else
    {
        _S43 = 1.0549999475479126f * (F32_pow((_S47), (0.4166666567325592f))) - 0.05499999970197678f;
    }
    return _S43;
}

inline __device__ float3  working_to_display(float3  rgb_2, Matrix<float, 3, 3>  color_matrix_0, int transfer_1, bool is_linear_0)
{
    float3  _S48;
    if(!is_linear_0)
    {
        float _S49 = rgb_2.x;
        float _S50;
        if(_S49 < 0.04044999927282333f)
        {
            _S50 = _S49 * 0.07739938050508499f;
        }
        else
        {
            _S50 = (F32_pow((0.94786733388900757f * (_S49 + 0.05499999970197678f)), (2.40000009536743164f)));
        }
        float _S51 = rgb_2.y;
        float _S52;
        if(_S51 < 0.04044999927282333f)
        {
            _S52 = _S51 * 0.07739938050508499f;
        }
        else
        {
            _S52 = (F32_pow((0.94786733388900757f * (_S51 + 0.05499999970197678f)), (2.40000009536743164f)));
        }
        float _S53 = rgb_2.z;
        float _S54;
        if(_S53 < 0.04044999927282333f)
        {
            _S54 = _S53 * 0.07739938050508499f;
        }
        else
        {
            _S54 = (F32_pow((0.94786733388900757f * (_S53 + 0.05499999970197678f)), (2.40000009536743164f)));
        }
        _S48 = make_float3 (_S50, _S52, _S54);
    }
    else
    {
        _S48 = rgb_2;
    }
    float3  _S55 = mul_0(color_matrix_0, _S48);
    return make_float3 (tone_encode_0(_S55.x, transfer_1), tone_encode_0(_S55.y, transfer_1), tone_encode_0(_S55.z, transfer_1));
}

inline __device__ float s_primal_ctx_pow_0(float _S56, float _S57)
{
    return (F32_pow((_S56), (_S57)));
}

inline __device__ float3  s_primal_ctx_mul_0(Matrix<float, 3, 3>  _S58, float3  _S59)
{
    return mul_0(_S58, _S59);
}

inline __device__ float s_primal_ctx_xfer_max0_0(float _S60)
{
    return xfer_max0_0(_S60);
}

inline __device__ float s_primal_ctx_xfer_aces_0(float dpx_4)
{
    return dpx_4 * (2.50999999046325684f * dpx_4 + 0.02999999932944775f) / (dpx_4 * (2.43000006675720215f * dpx_4 + 0.5899999737739563f) + 0.14000000059604645f);
}

inline __device__ float s_primal_ctx_xfer_clamp01_0(float _S61)
{
    return xfer_clamp01_0(_S61);
}

inline __device__ float s_primal_ctx_xfer_hable_0(float dpx_5)
{
    float _S62 = 0.15000000596046448f * dpx_5;
    return (dpx_5 * (_S62 + 0.05000000074505806f) + 0.00400000018998981f) / (dpx_5 * (_S62 + 0.5f) + 0.06000000238418579f) - 0.06666666269302368f;
}

inline __device__ float s_primal_ctx_xfer_uncharted2_0(float dpx_6)
{
    return s_primal_ctx_xfer_hable_0(s_primal_ctx_xfer_max0_0(dpx_6)) / s_primal_ctx_xfer_hable_0(11.19999980926513672f);
}

inline __device__ void s_bwd_prop_pow_0(DiffPair_float_0 * _S63, DiffPair_float_0 * _S64, float _S65)
{
    _d_pow_0(_S63, _S64, _S65);
    return;
}

inline __device__ void s_bwd_prop_xfer_max0_0(DiffPair_float_0 * _S66, float _S67)
{
    xfer_pass_grad_0(_S66, _S67);
    return;
}

inline __device__ void s_bwd_prop_xfer_clamp01_0(DiffPair_float_0 * _S68, float _S69)
{
    xfer_pass_grad_0(_S68, _S69);
    return;
}

inline __device__ void s_bwd_prop_xfer_hable_0(DiffPair_float_0 * dpx_7, float _s_dOut_1)
{
    float _S70 = 0.15000000596046448f * (*dpx_7).primal_0;
    float _S71 = _S70 + 0.05000000074505806f;
    float _S72 = _S70 + 0.5f;
    float _S73 = (*dpx_7).primal_0 * _S72 + 0.06000000238418579f;
    float _S74 = _s_dOut_1 / (_S73 * _S73);
    float _S75 = ((*dpx_7).primal_0 * _S71 + 0.00400000018998981f) * - _S74;
    float _S76 = _S73 * _S74;
    float _S77 = _S72 * _S75 + _S71 * _S76 + 0.15000000596046448f * ((*dpx_7).primal_0 * _S75 + (*dpx_7).primal_0 * _S76);
    dpx_7->primal_0 = (*dpx_7).primal_0;
    dpx_7->differential_0 = _S77;
    return;
}

inline __device__ void s_bwd_prop_xfer_uncharted2_0(DiffPair_float_0 * dpx_8, float _s_dOut_2)
{
    float _S78 = s_primal_ctx_xfer_hable_0(11.19999980926513672f);
    float _S79 = _S78 * (_s_dOut_2 / (_S78 * _S78));
    DiffPair_float_0 _S80;
    (&_S80)->primal_0 = s_primal_ctx_xfer_max0_0((*dpx_8).primal_0);
    (&_S80)->differential_0 = 0.0f;
    s_bwd_prop_xfer_hable_0(&_S80, _S79);
    DiffPair_float_0 _S81;
    (&_S81)->primal_0 = (*dpx_8).primal_0;
    (&_S81)->differential_0 = 0.0f;
    s_bwd_prop_xfer_max0_0(&_S81, _S80.differential_0);
    dpx_8->primal_0 = (*dpx_8).primal_0;
    dpx_8->differential_0 = _S81.differential_0;
    return;
}

inline __device__ void s_bwd_prop_xfer_aces_0(DiffPair_float_0 * dpx_9, float _s_dOut_3)
{
    float _S82 = 2.50999999046325684f * (*dpx_9).primal_0 + 0.02999999932944775f;
    float _S83 = 2.43000006675720215f * (*dpx_9).primal_0 + 0.5899999737739563f;
    float _S84 = (*dpx_9).primal_0 * _S83 + 0.14000000059604645f;
    float _S85 = _s_dOut_3 / (_S84 * _S84);
    float _S86 = (*dpx_9).primal_0 * _S82 * - _S85;
    float _S87 = _S84 * _S85;
    float _S88 = _S83 * _S86 + 2.43000006675720215f * ((*dpx_9).primal_0 * _S86) + _S82 * _S87 + 2.50999999046325684f * ((*dpx_9).primal_0 * _S87);
    dpx_9->primal_0 = (*dpx_9).primal_0;
    dpx_9->differential_0 = _S88;
    return;
}

inline __device__ void s_bwd_prop_xfer_filmic_0(DiffPair_float_0 * dpx_10, float _s_dOut_4)
{
    float _S89 = (*dpx_10).primal_0 - 0.00400000018998981f;
    float _S90 = s_primal_ctx_xfer_max0_0(_S89);
    float _S91 = 6.19999980926513672f * _S90;
    float _S92 = _S91 + 0.5f;
    float _S93 = _S91 + 1.70000004768371582f;
    float _S94 = _S90 * _S93 + 0.05999999865889549f;
    float _S95 = _s_dOut_4 / (_S94 * _S94);
    float _S96 = _S90 * _S92 * - _S95;
    float _S97 = _S94 * _S95;
    float _S98 = _S93 * _S96 + _S92 * _S97 + 6.19999980926513672f * (_S90 * _S96 + _S90 * _S97);
    DiffPair_float_0 _S99;
    (&_S99)->primal_0 = _S89;
    (&_S99)->differential_0 = 0.0f;
    s_bwd_prop_xfer_max0_0(&_S99, _S98);
    dpx_10->primal_0 = (*dpx_10).primal_0;
    dpx_10->differential_0 = _S99.differential_0;
    return;
}

inline __device__ void s_bwd_prop_tone_encode_0(DiffPair_float_0 * dpx_11, int transfer_2, float _s_dOut_5)
{
    DiffPair_float_0 _S100 = *dpx_11;
    bool _S101 = transfer_2 == int(3);
    bool _S102 = !_S101;
    bool _runFlag_0;
    bool _runFlag_1;
    bool _runFlag_2;
    bool _S103;
    bool _S104;
    bool _S105;
    float _S106;
    float _S107;
    float _S108;
    float _S109;
    float _S110;
    float _S111;
    float _S112;
    if(_S102)
    {
        bool _S113 = transfer_2 == int(2);
        if(_S113)
        {
            float _S114 = s_primal_ctx_xfer_max0_0(_S100.primal_0);
            float _S115 = s_primal_ctx_xfer_aces_0(_S114);
            float _S116 = s_primal_ctx_xfer_clamp01_0(_S115);
            _runFlag_0 = false;
            _S106 = _S116;
            _S107 = _S115;
            _S108 = _S114;
        }
        else
        {
            _runFlag_0 = _S102;
            _S106 = 0.0f;
            _S107 = 0.0f;
            _S108 = 0.0f;
        }
        if(_runFlag_0)
        {
            bool _S117 = transfer_2 == int(4);
            if(_S117)
            {
                float _S118 = s_primal_ctx_xfer_uncharted2_0(_S100.primal_0);
                float _S119 = s_primal_ctx_xfer_clamp01_0(_S118);
                _runFlag_1 = false;
                _S109 = _S119;
                _S110 = _S118;
            }
            else
            {
                _runFlag_1 = _runFlag_0;
                _S109 = 0.0f;
                _S110 = 0.0f;
            }
            if(_runFlag_1)
            {
                bool _S120 = transfer_2 == int(1);
                if(_S120)
                {
                    float _S121 = s_primal_ctx_xfer_clamp01_0(_S100.primal_0);
                    _runFlag_2 = false;
                    _S111 = _S121;
                }
                else
                {
                    _runFlag_2 = _runFlag_1;
                    _S111 = 0.0f;
                }
                if(_runFlag_2)
                {
                    _S112 = s_primal_ctx_xfer_max0_0(_S100.primal_0);
                }
                else
                {
                    _S112 = 0.0f;
                }
                float _S122 = _S111;
                _S111 = _S112;
                _S103 = _S120;
                _S112 = _S122;
            }
            else
            {
                _runFlag_2 = false;
                _S111 = 0.0f;
                _S103 = false;
                _S112 = 0.0f;
            }
            float _S123 = _S109;
            float _S124 = _S110;
            _S109 = _S111;
            _S110 = _S112;
            _S104 = _S117;
            _S111 = _S123;
            _S112 = _S124;
        }
        else
        {
            _runFlag_1 = false;
            _runFlag_2 = false;
            _S109 = 0.0f;
            _S103 = false;
            _S110 = 0.0f;
            _S104 = false;
            _S111 = 0.0f;
            _S112 = 0.0f;
        }
        float _S125 = _S106;
        float _S126 = _S107;
        float _S127 = _S108;
        _S106 = _S109;
        _S107 = _S110;
        _S108 = _S111;
        _S109 = _S112;
        _S105 = _S113;
        _S110 = _S125;
        _S111 = _S126;
        _S112 = _S127;
    }
    else
    {
        _runFlag_0 = false;
        _runFlag_1 = false;
        _runFlag_2 = false;
        _S106 = 0.0f;
        _S103 = false;
        _S107 = 0.0f;
        _S104 = false;
        _S108 = 0.0f;
        _S109 = 0.0f;
        _S105 = false;
        _S110 = 0.0f;
        _S111 = 0.0f;
        _S112 = 0.0f;
    }
    if(_S102)
    {
        if(_runFlag_0)
        {
            if(_runFlag_1)
            {
                float _S128;
                if(_runFlag_2)
                {
                    if(_S106 < 0.00313080009073019f)
                    {
                        _S106 = 12.92000007629394531f * _s_dOut_5;
                    }
                    else
                    {
                        float _S129 = 1.0549999475479126f * _s_dOut_5;
                        DiffPair_float_0 _S130;
                        (&_S130)->primal_0 = _S106;
                        (&_S130)->differential_0 = 0.0f;
                        DiffPair_float_0 _S131;
                        (&_S131)->primal_0 = 0.4166666567325592f;
                        (&_S131)->differential_0 = 0.0f;
                        s_bwd_prop_pow_0(&_S130, &_S131, _S129);
                        _S106 = _S130.differential_0;
                    }
                    DiffPair_float_0 _S132;
                    (&_S132)->primal_0 = _S100.primal_0;
                    (&_S132)->differential_0 = 0.0f;
                    s_bwd_prop_xfer_max0_0(&_S132, _S106);
                    _S106 = 0.0f;
                    _S128 = _S132.differential_0;
                }
                else
                {
                    _S106 = _s_dOut_5;
                    _S128 = 0.0f;
                }
                if(_S103)
                {
                    if(_S107 < 0.00313080009073019f)
                    {
                        _S106 = 12.92000007629394531f * _S106;
                    }
                    else
                    {
                        float _S133 = 1.0549999475479126f * _S106;
                        DiffPair_float_0 _S134;
                        (&_S134)->primal_0 = _S107;
                        (&_S134)->differential_0 = 0.0f;
                        DiffPair_float_0 _S135;
                        (&_S135)->primal_0 = 0.4166666567325592f;
                        (&_S135)->differential_0 = 0.0f;
                        s_bwd_prop_pow_0(&_S134, &_S135, _S133);
                        _S106 = _S134.differential_0;
                    }
                    DiffPair_float_0 _S136;
                    (&_S136)->primal_0 = _S100.primal_0;
                    (&_S136)->differential_0 = 0.0f;
                    s_bwd_prop_xfer_clamp01_0(&_S136, _S106);
                    float _S137 = _S136.differential_0 + _S128;
                    _S106 = 0.0f;
                    _S107 = _S137;
                }
                else
                {
                    _S107 = _S128;
                }
            }
            else
            {
                _S106 = _s_dOut_5;
                _S107 = 0.0f;
            }
            if(_S104)
            {
                if(_S108 < 0.00313080009073019f)
                {
                    _S106 = 12.92000007629394531f * _S106;
                }
                else
                {
                    float _S138 = 1.0549999475479126f * _S106;
                    DiffPair_float_0 _S139;
                    (&_S139)->primal_0 = _S108;
                    (&_S139)->differential_0 = 0.0f;
                    DiffPair_float_0 _S140;
                    (&_S140)->primal_0 = 0.4166666567325592f;
                    (&_S140)->differential_0 = 0.0f;
                    s_bwd_prop_pow_0(&_S139, &_S140, _S138);
                    _S106 = _S139.differential_0;
                }
                DiffPair_float_0 _S141;
                (&_S141)->primal_0 = _S109;
                (&_S141)->differential_0 = 0.0f;
                s_bwd_prop_xfer_clamp01_0(&_S141, _S106);
                DiffPair_float_0 _S142;
                (&_S142)->primal_0 = _S100.primal_0;
                (&_S142)->differential_0 = 0.0f;
                s_bwd_prop_xfer_uncharted2_0(&_S142, _S141.differential_0);
                float _S143 = _S142.differential_0 + _S107;
                _S106 = 0.0f;
                _S107 = _S143;
            }
        }
        else
        {
            _S106 = _s_dOut_5;
            _S107 = 0.0f;
        }
        if(_S105)
        {
            if(_S110 < 0.00313080009073019f)
            {
                _S106 = 12.92000007629394531f * _S106;
            }
            else
            {
                float _S144 = 1.0549999475479126f * _S106;
                DiffPair_float_0 _S145;
                (&_S145)->primal_0 = _S110;
                (&_S145)->differential_0 = 0.0f;
                DiffPair_float_0 _S146;
                (&_S146)->primal_0 = 0.4166666567325592f;
                (&_S146)->differential_0 = 0.0f;
                s_bwd_prop_pow_0(&_S145, &_S146, _S144);
                _S106 = _S145.differential_0;
            }
            DiffPair_float_0 _S147;
            (&_S147)->primal_0 = _S111;
            (&_S147)->differential_0 = 0.0f;
            s_bwd_prop_xfer_clamp01_0(&_S147, _S106);
            DiffPair_float_0 _S148;
            (&_S148)->primal_0 = _S112;
            (&_S148)->differential_0 = 0.0f;
            s_bwd_prop_xfer_aces_0(&_S148, _S147.differential_0);
            DiffPair_float_0 _S149;
            (&_S149)->primal_0 = _S100.primal_0;
            (&_S149)->differential_0 = 0.0f;
            s_bwd_prop_xfer_max0_0(&_S149, _S148.differential_0);
            float _S150 = _S149.differential_0 + _S107;
            _S106 = 0.0f;
            _S107 = _S150;
        }
    }
    else
    {
        _S106 = _s_dOut_5;
        _S107 = 0.0f;
    }
    if(_S101)
    {
        DiffPair_float_0 _S151;
        (&_S151)->primal_0 = _S100.primal_0;
        (&_S151)->differential_0 = 0.0f;
        s_bwd_prop_xfer_filmic_0(&_S151, _S106);
        _S106 = _S151.differential_0 + _S107;
    }
    else
    {
        _S106 = _S107;
    }
    dpx_11->primal_0 = (*dpx_11).primal_0;
    dpx_11->differential_0 = _S106;
    return;
}

inline __device__ void s_bwd_prop_mul_0(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S152, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S153, float3  _S154)
{
    _d_mul_0(_S152, _S153, _S154);
    return;
}

inline __device__ void s_bwd_prop_working_to_display_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dprgb_0, Matrix<float, 3, 3>  color_matrix_1, int transfer_3, bool is_linear_1, float3  _s_dOut_6)
{
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S155 = *dprgb_0;
    bool _S156 = !is_linear_1;
    float _S157;
    float _S158;
    float _S159;
    float3  _S160;
    if(_S156)
    {
        float _S161 = _S155.primal_0.x;
        if(_S161 < 0.04044999927282333f)
        {
            _S157 = _S161 * 0.07739938050508499f;
        }
        else
        {
            _S157 = s_primal_ctx_pow_0(0.94786733388900757f * (_S161 + 0.05499999970197678f), 2.40000009536743164f);
        }
        float _S162 = _S155.primal_0.y;
        if(_S162 < 0.04044999927282333f)
        {
            _S158 = _S162 * 0.07739938050508499f;
        }
        else
        {
            _S158 = s_primal_ctx_pow_0(0.94786733388900757f * (_S162 + 0.05499999970197678f), 2.40000009536743164f);
        }
        float _S163 = _S155.primal_0.z;
        if(_S163 < 0.04044999927282333f)
        {
            _S159 = _S163 * 0.07739938050508499f;
        }
        else
        {
            _S159 = s_primal_ctx_pow_0(0.94786733388900757f * (_S163 + 0.05499999970197678f), 2.40000009536743164f);
        }
        _S160 = make_float3 (_S157, _S158, _S159);
        _S157 = _S163;
        _S158 = _S162;
        _S159 = _S161;
    }
    else
    {
        _S160 = _S155.primal_0;
        _S157 = 0.0f;
        _S158 = 0.0f;
        _S159 = 0.0f;
    }
    float3  _S164 = s_primal_ctx_mul_0(color_matrix_1, _S160);
    float _S165 = _S164.x;
    float _S166 = _S164.y;
    float _S167 = _S164.z;
    DiffPair_float_0 _S168;
    (&_S168)->primal_0 = _S167;
    (&_S168)->differential_0 = 0.0f;
    s_bwd_prop_tone_encode_0(&_S168, transfer_3, _s_dOut_6.z);
    DiffPair_float_0 _S169;
    (&_S169)->primal_0 = _S166;
    (&_S169)->differential_0 = 0.0f;
    s_bwd_prop_tone_encode_0(&_S169, transfer_3, _s_dOut_6.y);
    DiffPair_float_0 _S170;
    (&_S170)->primal_0 = _S165;
    (&_S170)->differential_0 = 0.0f;
    s_bwd_prop_tone_encode_0(&_S170, transfer_3, _s_dOut_6.x);
    float3  _S171 = make_float3 (_S170.differential_0, _S169.differential_0, _S168.differential_0);
    Matrix<float, 3, 3>  _S172 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S173;
    (&_S173)->primal_0 = color_matrix_1;
    (&_S173)->differential_0 = _S172;
    float3  _S174 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S175;
    (&_S175)->primal_0 = _S160;
    (&_S175)->differential_0 = _S174;
    s_bwd_prop_mul_0(&_S173, &_S175, _S171);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S176 = _S175;
    if(_S156)
    {
        bool _S177 = _S157 < 0.04044999927282333f;
        if(_S177)
        {
            _S157 = 0.0f;
        }
        else
        {
            _S157 = 0.94786733388900757f * (_S157 + 0.05499999970197678f);
        }
        if(_S177)
        {
            _S157 = 0.07739938050508499f * _S176.differential_0.z;
        }
        else
        {
            DiffPair_float_0 _S178;
            (&_S178)->primal_0 = _S157;
            (&_S178)->differential_0 = 0.0f;
            DiffPair_float_0 _S179;
            (&_S179)->primal_0 = 2.40000009536743164f;
            (&_S179)->differential_0 = 0.0f;
            s_bwd_prop_pow_0(&_S178, &_S179, _S176.differential_0.z);
            _S157 = 0.94786733388900757f * _S178.differential_0;
        }
        bool _S180 = _S158 < 0.04044999927282333f;
        if(_S180)
        {
            _S158 = 0.0f;
        }
        else
        {
            _S158 = 0.94786733388900757f * (_S158 + 0.05499999970197678f);
        }
        if(_S180)
        {
            _S158 = 0.07739938050508499f * _S176.differential_0.y;
        }
        else
        {
            DiffPair_float_0 _S181;
            (&_S181)->primal_0 = _S158;
            (&_S181)->differential_0 = 0.0f;
            DiffPair_float_0 _S182;
            (&_S182)->primal_0 = 2.40000009536743164f;
            (&_S182)->differential_0 = 0.0f;
            s_bwd_prop_pow_0(&_S181, &_S182, _S176.differential_0.y);
            _S158 = 0.94786733388900757f * _S181.differential_0;
        }
        bool _S183 = _S159 < 0.04044999927282333f;
        if(_S183)
        {
            _S159 = 0.0f;
        }
        else
        {
            _S159 = 0.94786733388900757f * (_S159 + 0.05499999970197678f);
        }
        if(_S183)
        {
            _S159 = 0.07739938050508499f * _S176.differential_0.x;
        }
        else
        {
            DiffPair_float_0 _S184;
            (&_S184)->primal_0 = _S159;
            (&_S184)->differential_0 = 0.0f;
            DiffPair_float_0 _S185;
            (&_S185)->primal_0 = 2.40000009536743164f;
            (&_S185)->differential_0 = 0.0f;
            s_bwd_prop_pow_0(&_S184, &_S185, _S176.differential_0.x);
            _S159 = 0.94786733388900757f * _S184.differential_0;
        }
        _S160 = make_float3 (_S159, _S158, _S157);
    }
    else
    {
        _S160 = _S176.differential_0;
    }
    dprgb_0->primal_0 = (*dprgb_0).primal_0;
    dprgb_0->differential_0 = _S160;
    return;
}

inline __device__ void s_bwd_working_to_display_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S186, Matrix<float, 3, 3>  _S187, int _S188, bool _S189, float3  _S190)
{
    s_bwd_prop_working_to_display_0(_S186, _S187, _S188, _S189, _S190);
    return;
}

inline __device__ float3  working_to_display_bwd(float3  rgb_3, Matrix<float, 3, 3>  color_matrix_2, int transfer_4, bool is_linear_2, float3  v_out_rgb_1)
{
    float3  _S191 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 p_rgb_1;
    (&p_rgb_1)->primal_0 = rgb_3;
    (&p_rgb_1)->differential_0 = _S191;
    s_bwd_working_to_display_0(&p_rgb_1, color_matrix_2, transfer_4, is_linear_2, v_out_rgb_1);
    return p_rgb_1.differential_0;
}

inline __device__ void _d_sqrt_0(DiffPair_float_0 * dpx_12, float dOut_4)
{
    float _S192 = 0.5f / (F32_sqrt(((F32_max((1.00000001168609742e-07f), ((*dpx_12).primal_0)))))) * dOut_4;
    dpx_12->primal_0 = (*dpx_12).primal_0;
    dpx_12->differential_0 = _S192;
    return;
}

inline __device__ float xfer_filmic_inv_0(float y_4)
{
    float _S193 = (F32_min((y_4), (xfer_filmic_0(11.19999980926513672f))));
    float a_0 = 6.19999980926513672f * (1.0f - _S193);
    float b_0 = 0.5f - 1.70000004768371582f * _S193;
    return (- b_0 + (F32_sqrt(((F32_max((b_0 * b_0 - 4.0f * a_0 * (-0.05999999865889549f * _S193)), (0.0f))))))) / (2.0f * a_0) + 0.00400000018998981f;
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
    float _S194;
    if(transfer_5 == int(2))
    {
        if(d_0 < 0.04044999927282333f)
        {
            _S194 = d_0 * 0.07739938050508499f;
        }
        else
        {
            _S194 = (F32_pow((0.94786733388900757f * (d_0 + 0.05499999970197678f)), (2.40000009536743164f)));
        }
        return xfer_aces_inv_0(_S194);
    }
    if(transfer_5 == int(4))
    {
        if(d_0 < 0.04044999927282333f)
        {
            _S194 = d_0 * 0.07739938050508499f;
        }
        else
        {
            _S194 = (F32_pow((0.94786733388900757f * (d_0 + 0.05499999970197678f)), (2.40000009536743164f)));
        }
        return xfer_uncharted2_inv_0(_S194);
    }
    if(d_0 < 0.04044999927282333f)
    {
        _S194 = d_0 * 0.07739938050508499f;
    }
    else
    {
        _S194 = (F32_pow((0.94786733388900757f * (d_0 + 0.05499999970197678f)), (2.40000009536743164f)));
    }
    return _S194;
}

inline __device__ float3  display_to_working3(float3  rgb_4, int transfer_6, bool is_linear_3)
{
    float _S195 = tone_decode_0(rgb_4.x, transfer_6);
    float _S196 = tone_decode_0(rgb_4.y, transfer_6);
    float _S197 = tone_decode_0(rgb_4.z, transfer_6);
    float3  lin_0 = make_float3 (_S195, _S196, _S197);
    if(is_linear_3)
    {
        return lin_0;
    }
    float _S198;
    if(_S195 < 0.00313080009073019f)
    {
        _S198 = _S195 * 12.92000007629394531f;
    }
    else
    {
        _S198 = 1.0549999475479126f * (F32_pow((_S195), (0.4166666567325592f))) - 0.05499999970197678f;
    }
    float _S199;
    if(_S196 < 0.00313080009073019f)
    {
        _S199 = _S196 * 12.92000007629394531f;
    }
    else
    {
        _S199 = 1.0549999475479126f * (F32_pow((_S196), (0.4166666567325592f))) - 0.05499999970197678f;
    }
    float _S200;
    if(_S197 < 0.00313080009073019f)
    {
        _S200 = _S197 * 12.92000007629394531f;
    }
    else
    {
        _S200 = 1.0549999475479126f * (F32_pow((_S197), (0.4166666567325592f))) - 0.05499999970197678f;
    }
    return make_float3 (_S198, _S199, _S200);
}

inline __device__ float3  background_apply_exponent(float3  display_0, float p_0)
{
    if(p_0 == 1.0f)
    {
        return display_0;
    }
    return make_float3 ((F32_pow(((F32_max((display_0.x), (0.0f)))), (p_0))), (F32_pow(((F32_max((display_0.y), (0.0f)))), (p_0))), (F32_pow(((F32_max((display_0.z), (0.0f)))), (p_0))));
}

inline __device__ void _d_cross_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * a_3, DiffPair_vectorx3Cfloatx2C3x3E_0 * b_3, float3  dOut_5)
{
    float _S201 = dOut_5.y;
    float _S202 = dOut_5.z;
    float _S203 = dOut_5.x;
    float _S204 = (*a_3).primal_0.z * _S201 + - (*a_3).primal_0.y * _S202;
    float _S205 = - (*a_3).primal_0.z * _S203 + (*a_3).primal_0.x * _S202;
    float _S206 = (*a_3).primal_0.y * _S203 + - (*a_3).primal_0.x * _S201;
    float3  _S207 = make_float3 (- (*b_3).primal_0.z * _S201 + (*b_3).primal_0.y * _S202, (*b_3).primal_0.z * _S203 + - (*b_3).primal_0.x * _S202, - (*b_3).primal_0.y * _S203 + (*b_3).primal_0.x * _S201);
    a_3->primal_0 = (*a_3).primal_0;
    a_3->differential_0 = _S207;
    float3  _S208 = make_float3 (_S204, _S205, _S206);
    b_3->primal_0 = (*b_3).primal_0;
    b_3->differential_0 = _S208;
    return;
}

inline __device__ float3  cross_0(float3  left_2, float3  right_2)
{
    float _S209 = left_2.y;
    float _S210 = right_2.z;
    float _S211 = left_2.z;
    float _S212 = right_2.y;
    float _S213 = right_2.x;
    float _S214 = left_2.x;
    return make_float3 (_S209 * _S210 - _S211 * _S212, _S211 * _S213 - _S214 * _S210, _S214 * _S212 - _S209 * _S213);
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
    float3  _S215 = points_0[int(0)];
    bool _S216;
    if((dot_0(_S215, _S215)) == 0.0f)
    {
        _S216 = true;
    }
    else
    {
        float3  _S217 = points_0[int(1)];
        _S216 = (dot_0(_S217, _S217)) == 0.0f;
    }
    if(_S216)
    {
        _S216 = true;
    }
    else
    {
        float3  _S218 = points_0[int(2)];
        _S216 = (dot_0(_S218, _S218)) == 0.0f;
    }
    if(_S216)
    {
        _S216 = true;
    }
    else
    {
        float3  _S219 = points_0[int(3)];
        _S216 = (dot_0(_S219, _S219)) == 0.0f;
    }
    if(_S216)
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

inline __device__ float s_primal_ctx_dot_0(float3  _S220, float3  _S221)
{
    return dot_0(_S220, _S221);
}

inline __device__ float3  s_primal_ctx_cross_0(float3  _S222, float3  _S223)
{
    return cross_0(_S222, _S223);
}

inline __device__ void s_bwd_prop_sqrt_0(DiffPair_float_0 * _S224, float _S225)
{
    _d_sqrt_0(_S224, _S225);
    return;
}

inline __device__ void s_bwd_prop_length_impl_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_13, float _s_dOut_7)
{
    float _S226 = (*dpx_13).primal_0.x;
    float _S227 = (*dpx_13).primal_0.y;
    float _S228 = (*dpx_13).primal_0.z;
    DiffPair_float_0 _S229;
    (&_S229)->primal_0 = _S226 * _S226 + _S227 * _S227 + _S228 * _S228;
    (&_S229)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S229, _s_dOut_7);
    float _S230 = (*dpx_13).primal_0.z * _S229.differential_0;
    float _S231 = _S230 + _S230;
    float _S232 = (*dpx_13).primal_0.y * _S229.differential_0;
    float _S233 = _S232 + _S232;
    float _S234 = (*dpx_13).primal_0.x * _S229.differential_0;
    float _S235 = _S234 + _S234;
    float3  _S236 = make_float3 (0.0f);
    *&((&_S236)->z) = _S231;
    *&((&_S236)->y) = _S233;
    *&((&_S236)->x) = _S235;
    dpx_13->primal_0 = (*dpx_13).primal_0;
    dpx_13->differential_0 = _S236;
    return;
}

inline __device__ void s_bwd_length_impl_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S237, float _S238)
{
    s_bwd_prop_length_impl_0(_S237, _S238);
    return;
}

inline __device__ void s_bwd_prop_dot_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S239, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S240, float _S241)
{
    _d_dot_0(_S239, _S240, _S241);
    return;
}

inline __device__ void s_bwd_prop_cross_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S242, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S243, float3  _S244)
{
    _d_cross_0(_S242, _S243, _S244);
    return;
}

inline __device__ void s_bwd_prop_points_to_normal_0(DiffPair_arrayx3Cvectorx3Cfloatx2C3x3Ex2C4x3E_0 * dppoints_0, float3  _s_dOut_8)
{
    FixedArray<float3 , 4>  _S245 = dppoints_0->primal_0;
    float3  _S246 = make_float3 (0.0f);
    float3  _S247 = dppoints_0->primal_0[int(0)];
    bool _S248 = (s_primal_ctx_dot_0(_S247, _S247)) == 0.0f;
    bool _S249;
    float3  _S250;
    if(_S248)
    {
        _S249 = true;
        _S250 = _S246;
    }
    else
    {
        float3  _S251 = _S245[int(1)];
        _S249 = (s_primal_ctx_dot_0(_S251, _S251)) == 0.0f;
        _S250 = _S245[int(1)];
    }
    bool _S252;
    float3  _S253;
    if(_S249)
    {
        _S252 = true;
        _S253 = _S246;
    }
    else
    {
        float3  _S254 = _S245[int(2)];
        _S252 = (s_primal_ctx_dot_0(_S254, _S254)) == 0.0f;
        _S253 = _S245[int(2)];
    }
    bool _S255;
    float3  _S256;
    if(_S252)
    {
        _S255 = true;
        _S256 = _S246;
    }
    else
    {
        float3  _S257 = _S245[int(3)];
        _S255 = (s_primal_ctx_dot_0(_S257, _S257)) == 0.0f;
        _S256 = _S245[int(3)];
    }
    bool _S258 = !_S255;
    float3  _S259;
    float3  _S260;
    float3  _S261;
    float3  _S262;
    float3  _S263;
    if(_S258)
    {
        float3  dx_0 = _S245[int(1)] - _S245[int(0)];
        float3  _S264 = - (_S245[int(3)] - _S245[int(2)]);
        float3  _S265 = s_primal_ctx_cross_0(dx_0, _S264);
        bool _S266 = (s_primal_ctx_dot_0(_S265, _S265)) != 0.0f;
        if(_S266)
        {
            float _S267 = length_0(_S265);
            float3  _S268 = make_float3 (_S267);
            _S259 = make_float3 (_S267 * _S267);
            _S260 = _S268;
        }
        else
        {
            _S259 = _S246;
            _S260 = _S246;
        }
        float3  _S269 = _S260;
        _S255 = _S266;
        _S260 = _S265;
        _S261 = _S269;
        _S262 = dx_0;
        _S263 = _S264;
    }
    else
    {
        _S255 = false;
        _S259 = _S246;
        _S260 = _S246;
        _S261 = _S246;
        _S262 = _S246;
        _S263 = _S246;
    }
    FixedArray<float3 , 4>  _S270;
    if(_S258)
    {
        if(_S255)
        {
            float3  _S271 = _s_dOut_8 / _S259;
            float3  _S272 = _S260 * - _S271;
            float3  _S273 = _S261 * _S271;
            float _S274 = _S272.x + _S272.y + _S272.z;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S275;
            (&_S275)->primal_0 = _S260;
            (&_S275)->differential_0 = _S246;
            s_bwd_length_impl_0(&_S275, _S274);
            _S259 = _S273 + _S275.differential_0;
        }
        else
        {
            _S259 = _s_dOut_8;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S276;
        (&_S276)->primal_0 = _S260;
        (&_S276)->differential_0 = _S246;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S277;
        (&_S277)->primal_0 = _S260;
        (&_S277)->differential_0 = _S246;
        s_bwd_prop_dot_0(&_S276, &_S277, 0.0f);
        float3  _S278 = _S277.differential_0 + _S276.differential_0 + _S259;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S279;
        (&_S279)->primal_0 = _S262;
        (&_S279)->differential_0 = _S246;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S280;
        (&_S280)->primal_0 = _S263;
        (&_S280)->differential_0 = _S246;
        s_bwd_prop_cross_0(&_S279, &_S280, _S278);
        float3  s_diff_dy_T_0 = - _S280.differential_0;
        float3  _S281 = - s_diff_dy_T_0;
        float3  _S282 = - _S279.differential_0;
        FixedArray<float3 , 4>  _S283;
        _S283[int(0)] = _S246;
        _S283[int(1)] = _S246;
        _S283[int(2)] = _S246;
        _S283[int(3)] = _S246;
        _S283[int(2)] = _S281;
        _S283[int(3)] = s_diff_dy_T_0;
        _S283[int(1)] = _S279.differential_0;
        _S270[int(0)] = _S283[int(0)];
        _S270[int(1)] = _S283[int(1)];
        _S270[int(2)] = _S283[int(2)];
        _S270[int(3)] = _S283[int(3)];
        _S259 = _S282;
    }
    else
    {
        _S270[int(0)] = _S246;
        _S270[int(1)] = _S246;
        _S270[int(2)] = _S246;
        _S270[int(3)] = _S246;
        _S259 = _S246;
    }
    if(_S252)
    {
    }
    else
    {
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S284;
        (&_S284)->primal_0 = _S256;
        (&_S284)->differential_0 = _S246;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S285;
        (&_S285)->primal_0 = _S256;
        (&_S285)->differential_0 = _S246;
        s_bwd_prop_dot_0(&_S284, &_S285, 0.0f);
        float3  _S286 = _S285.differential_0 + _S284.differential_0;
        FixedArray<float3 , 4>  _S287;
        _S287[int(0)] = _S246;
        _S287[int(1)] = _S246;
        _S287[int(2)] = _S246;
        _S287[int(3)] = _S246;
        _S287[int(3)] = _S286;
        float3  _S288 = _S270[int(1)] + _S287[int(1)];
        float3  _S289 = _S270[int(2)] + _S287[int(2)];
        float3  _S290 = _S270[int(3)] + _S287[int(3)];
        _S270[int(0)] = _S270[int(0)] + _S287[int(0)];
        _S270[int(1)] = _S288;
        _S270[int(2)] = _S289;
        _S270[int(3)] = _S290;
    }
    if(_S249)
    {
    }
    else
    {
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S291;
        (&_S291)->primal_0 = _S253;
        (&_S291)->differential_0 = _S246;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S292;
        (&_S292)->primal_0 = _S253;
        (&_S292)->differential_0 = _S246;
        s_bwd_prop_dot_0(&_S291, &_S292, 0.0f);
        float3  _S293 = _S292.differential_0 + _S291.differential_0;
        FixedArray<float3 , 4>  _S294;
        _S294[int(0)] = _S246;
        _S294[int(1)] = _S246;
        _S294[int(2)] = _S246;
        _S294[int(3)] = _S246;
        _S294[int(2)] = _S293;
        float3  _S295 = _S270[int(1)] + _S294[int(1)];
        float3  _S296 = _S270[int(2)] + _S294[int(2)];
        float3  _S297 = _S270[int(3)] + _S294[int(3)];
        _S270[int(0)] = _S270[int(0)] + _S294[int(0)];
        _S270[int(1)] = _S295;
        _S270[int(2)] = _S296;
        _S270[int(3)] = _S297;
    }
    if(_S248)
    {
    }
    else
    {
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S298;
        (&_S298)->primal_0 = _S250;
        (&_S298)->differential_0 = _S246;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S299;
        (&_S299)->primal_0 = _S250;
        (&_S299)->differential_0 = _S246;
        s_bwd_prop_dot_0(&_S298, &_S299, 0.0f);
        float3  _S300 = _S299.differential_0 + _S298.differential_0;
        FixedArray<float3 , 4>  _S301;
        _S301[int(0)] = _S246;
        _S301[int(1)] = _S246;
        _S301[int(2)] = _S246;
        _S301[int(3)] = _S246;
        _S301[int(1)] = _S300;
        float3  _S302 = _S270[int(1)] + _S301[int(1)];
        float3  _S303 = _S270[int(2)] + _S301[int(2)];
        float3  _S304 = _S270[int(3)] + _S301[int(3)];
        _S270[int(0)] = _S270[int(0)] + _S301[int(0)];
        _S270[int(1)] = _S302;
        _S270[int(2)] = _S303;
        _S270[int(3)] = _S304;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S305;
    (&_S305)->primal_0 = _S245[int(0)];
    (&_S305)->differential_0 = _S246;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S306;
    (&_S306)->primal_0 = _S245[int(0)];
    (&_S306)->differential_0 = _S246;
    s_bwd_prop_dot_0(&_S305, &_S306, 0.0f);
    float3  _S307 = _S306.differential_0 + _S305.differential_0 + _S259;
    FixedArray<float3 , 4>  _S308;
    _S308[int(0)] = _S246;
    _S308[int(1)] = _S246;
    _S308[int(2)] = _S246;
    _S308[int(3)] = _S246;
    _S308[int(0)] = _S307;
    FixedArray<float3 , 4>  _S309 = {
        _S270[int(0)] + _S308[int(0)], _S270[int(1)] + _S308[int(1)], _S270[int(2)] + _S308[int(2)], _S270[int(3)] + _S308[int(3)]
    };
    dppoints_0->primal_0 = dppoints_0->primal_0;
    dppoints_0->differential_0 = _S309;
    return;
}

inline __device__ void s_bwd_points_to_normal_0(DiffPair_arrayx3Cvectorx3Cfloatx2C3x3Ex2C4x3E_0 * _S310, float3  _S311)
{
    s_bwd_prop_points_to_normal_0(_S310, _S311);
    return;
}

inline __device__ void points_to_normal_vjp(FixedArray<float3 , 4>  points_1, float3  v_normal_0, FixedArray<float3 , 4>  * v_points_0)
{
    FixedArray<float3 , 4>  _S312 = { make_float3 (0.0f), make_float3 (0.0f), make_float3 (0.0f), make_float3 (0.0f) };
    DiffPair_arrayx3Cvectorx3Cfloatx2C3x3Ex2C4x3E_0 dp_points_0;
    (&dp_points_0)->primal_0 = points_1;
    (&dp_points_0)->differential_0 = _S312;
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
    float u_0 = uv_1.x;
    float v_0 = uv_1.y;
    float r2_0 = u_0 * u_0 + v_0 * v_0;
    return uv_1 * make_float2 (1.0f + r2_0 * ((*coeffs_0)[int(0)] + r2_0 * (*coeffs_0)[int(1)])) + make_float2 (2.0f * (*coeffs_0)[int(2)] * u_0 * v_0 + (*coeffs_0)[int(3)] * (r2_0 + 2.0f * u_0 * u_0), 2.0f * (*coeffs_0)[int(3)] * u_0 * v_0 + (*coeffs_0)[int(2)] * (r2_0 + 2.0f * v_0 * v_0));
}

struct DiffPair_vectorx3Cfloatx2C2x3E_0
{
    float2  primal_0;
    float2  differential_0;
};

inline __device__ DiffPair_vectorx3Cfloatx2C2x3E_0 s_fwd_DistOpenCV_distort_0(DiffPair_vectorx3Cfloatx2C2x3E_0 * dpuv_0, FixedArray<float, 4>  * coeffs_1)
{
    float u_1 = dpuv_0->primal_0.x;
    float s_diff_u_0 = dpuv_0->differential_0.x;
    float v_1 = dpuv_0->primal_0.y;
    float s_diff_v_0 = dpuv_0->differential_0.y;
    float _S313 = s_diff_u_0 * u_1;
    float _S314 = s_diff_v_0 * v_1;
    float r2_1 = u_1 * u_1 + v_1 * v_1;
    float s_diff_r2_0 = _S313 + _S313 + (_S314 + _S314);
    float _S315 = (*coeffs_1)[int(0)] + r2_1 * (*coeffs_1)[int(1)];
    float radial_0 = 1.0f + r2_1 * _S315;
    float _S316 = 2.0f * (*coeffs_1)[int(2)];
    float _S317 = _S316 * u_1;
    float _S318 = 2.0f * u_1;
    float _S319 = 2.0f * (*coeffs_1)[int(3)];
    float _S320 = _S319 * u_1;
    float _S321 = 2.0f * v_1;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S322 = { dpuv_0->primal_0 * make_float2 (radial_0) + make_float2 (_S317 * v_1 + (*coeffs_1)[int(3)] * (r2_1 + _S318 * u_1), _S320 * v_1 + (*coeffs_1)[int(2)] * (r2_1 + _S321 * v_1)), dpuv_0->differential_0 * make_float2 (radial_0) + make_float2 (s_diff_r2_0 * _S315 + s_diff_r2_0 * (*coeffs_1)[int(1)] * r2_1) * dpuv_0->primal_0 + make_float2 (s_diff_u_0 * _S316 * v_1 + s_diff_v_0 * _S317 + (s_diff_r2_0 + (s_diff_u_0 * 2.0f * u_1 + s_diff_u_0 * _S318)) * (*coeffs_1)[int(3)], s_diff_u_0 * _S319 * v_1 + s_diff_v_0 * _S320 + (s_diff_r2_0 + (s_diff_v_0 * 2.0f * v_1 + s_diff_v_0 * _S321)) * (*coeffs_1)[int(2)]) };
    return _S322;
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
        float2  _S323 = DistOpenCV_distort_0(q_0, dist_coeffs_1);
        float2  r_2 = _S323 - uv_2;
        float2  _S324 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S325;
        (&_S325)->primal_0 = q_0;
        (&_S325)->differential_0 = _S324;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S326 = s_fwd_DistOpenCV_distort_0(&_S325, dist_coeffs_1);
        float2  _S327 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S328;
        (&_S328)->primal_0 = q_0;
        (&_S328)->differential_0 = _S327;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S329 = s_fwd_DistOpenCV_distort_0(&_S328, dist_coeffs_1);
        Matrix<float, 2, 2>  _S330 = transpose_0(makeMatrix<float, 2, 2> (_S326.differential_0, _S329.differential_0));
        float inv_det_0 = 1.0f / (_S330.rows[int(0)].x * _S330.rows[int(1)].y - _S330.rows[int(0)].y * _S330.rows[int(1)].x);
        float _S331 = r_2.x;
        float _S332 = r_2.y;
        float2  q_1 = q_0 - make_float2 ((_S331 * _S330.rows[int(1)].y - _S332 * _S330.rows[int(0)].y) * inv_det_0, (- _S331 * _S330.rows[int(1)].x + _S332 * _S330.rows[int(0)].x) * inv_det_0);
        i_5 = i_5 + int(1);
        q_0 = q_1;
    }
    *uv_undist_1 = q_0;
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
    float _S340 = (F32_min((determinant_0(_S339)), ((F32_min((_S339.rows[int(0)].x), (_S339.rows[int(1)].y))))));
    bool _S341;
    if(_S340 > 0.25f)
    {
        _S341 = _S340 < 4.0f;
    }
    else
    {
        _S341 = false;
    }
    if(_S341)
    {
        float2  _S342 = DistOpenCV_distort_0(q_0, dist_coeffs_1);
        _S341 = (dot_1(q_0, _S342)) >= 0.0f;
    }
    else
    {
        _S341 = false;
    }
    if(_S341)
    {
        float2  _S343 = DistOpenCV_distort_0(*uv_undist_1, dist_coeffs_1);
        _S341 = (length_1(_S343 - uv_2)) < 0.00999999977648258f;
    }
    else
    {
        _S341 = false;
    }
    return _S341;
}

inline __device__ float2  DistThinPrism_distort_0(float2  uv_3, FixedArray<float, 8>  * coeffs_2)
{
    float u_2 = uv_3.x;
    float v_2 = uv_3.y;
    float r2_2 = u_2 * u_2 + v_2 * v_2;
    return uv_3 * make_float2 (1.0f + r2_2 * ((*coeffs_2)[int(0)] + r2_2 * ((*coeffs_2)[int(1)] + r2_2 * ((*coeffs_2)[int(2)] + r2_2 * (*coeffs_2)[int(3)])))) + make_float2 (2.0f * (*coeffs_2)[int(4)] * u_2 * v_2 + (*coeffs_2)[int(5)] * (r2_2 + 2.0f * u_2 * u_2) + (*coeffs_2)[int(6)] * r2_2, 2.0f * (*coeffs_2)[int(5)] * u_2 * v_2 + (*coeffs_2)[int(4)] * (r2_2 + 2.0f * v_2 * v_2) + (*coeffs_2)[int(7)] * r2_2);
}

inline __device__ DiffPair_vectorx3Cfloatx2C2x3E_0 s_fwd_DistThinPrism_distort_0(DiffPair_vectorx3Cfloatx2C2x3E_0 * dpuv_1, FixedArray<float, 8>  * coeffs_3)
{
    float u_3 = dpuv_1->primal_0.x;
    float s_diff_u_1 = dpuv_1->differential_0.x;
    float v_3 = dpuv_1->primal_0.y;
    float s_diff_v_1 = dpuv_1->differential_0.y;
    float _S344 = s_diff_u_1 * u_3;
    float _S345 = s_diff_v_1 * v_3;
    float r2_3 = u_3 * u_3 + v_3 * v_3;
    float s_diff_r2_1 = _S344 + _S344 + (_S345 + _S345);
    float _S346 = (*coeffs_3)[int(2)] + r2_3 * (*coeffs_3)[int(3)];
    float _S347 = (*coeffs_3)[int(1)] + r2_3 * _S346;
    float _S348 = (*coeffs_3)[int(0)] + r2_3 * _S347;
    float radial_1 = 1.0f + r2_3 * _S348;
    float _S349 = 2.0f * (*coeffs_3)[int(4)];
    float _S350 = _S349 * u_3;
    float _S351 = 2.0f * u_3;
    float _S352 = 2.0f * (*coeffs_3)[int(5)];
    float _S353 = _S352 * u_3;
    float _S354 = 2.0f * v_3;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S355 = { dpuv_1->primal_0 * make_float2 (radial_1) + make_float2 (_S350 * v_3 + (*coeffs_3)[int(5)] * (r2_3 + _S351 * u_3) + (*coeffs_3)[int(6)] * r2_3, _S353 * v_3 + (*coeffs_3)[int(4)] * (r2_3 + _S354 * v_3) + (*coeffs_3)[int(7)] * r2_3), dpuv_1->differential_0 * make_float2 (radial_1) + make_float2 (s_diff_r2_1 * _S348 + (s_diff_r2_1 * _S347 + (s_diff_r2_1 * _S346 + s_diff_r2_1 * (*coeffs_3)[int(3)] * r2_3) * r2_3) * r2_3) * dpuv_1->primal_0 + make_float2 (s_diff_u_1 * _S349 * v_3 + s_diff_v_1 * _S350 + (s_diff_r2_1 + (s_diff_u_1 * 2.0f * u_3 + s_diff_u_1 * _S351)) * (*coeffs_3)[int(5)] + s_diff_r2_1 * (*coeffs_3)[int(6)], s_diff_u_1 * _S352 * v_3 + s_diff_v_1 * _S353 + (s_diff_r2_1 + (s_diff_v_1 * 2.0f * v_3 + s_diff_v_1 * _S354)) * (*coeffs_3)[int(4)] + s_diff_r2_1 * (*coeffs_3)[int(7)]) };
    return _S355;
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
        float2  _S356 = DistThinPrism_distort_0(q_2, dist_coeffs_2);
        float2  r_3 = _S356 - uv_4;
        float2  _S357 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S358;
        (&_S358)->primal_0 = q_2;
        (&_S358)->differential_0 = _S357;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S359 = s_fwd_DistThinPrism_distort_0(&_S358, dist_coeffs_2);
        float2  _S360 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S361;
        (&_S361)->primal_0 = q_2;
        (&_S361)->differential_0 = _S360;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S362 = s_fwd_DistThinPrism_distort_0(&_S361, dist_coeffs_2);
        Matrix<float, 2, 2>  _S363 = transpose_0(makeMatrix<float, 2, 2> (_S359.differential_0, _S362.differential_0));
        float inv_det_1 = 1.0f / (_S363.rows[int(0)].x * _S363.rows[int(1)].y - _S363.rows[int(0)].y * _S363.rows[int(1)].x);
        float _S364 = r_3.x;
        float _S365 = r_3.y;
        float2  q_3 = q_2 - make_float2 ((_S364 * _S363.rows[int(1)].y - _S365 * _S363.rows[int(0)].y) * inv_det_1, (- _S364 * _S363.rows[int(1)].x + _S365 * _S363.rows[int(0)].x) * inv_det_1);
        i_6 = i_6 + int(1);
        q_2 = q_3;
    }
    *uv_undist_2 = q_2;
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
    float _S373 = (F32_min((determinant_0(_S372)), ((F32_min((_S372.rows[int(0)].x), (_S372.rows[int(1)].y))))));
    bool _S374;
    if(_S373 > 0.25f)
    {
        _S374 = _S373 < 4.0f;
    }
    else
    {
        _S374 = false;
    }
    if(_S374)
    {
        float2  _S375 = DistThinPrism_distort_0(q_2, dist_coeffs_2);
        _S374 = (dot_1(q_2, _S375)) >= 0.0f;
    }
    else
    {
        _S374 = false;
    }
    if(_S374)
    {
        float2  _S376 = DistThinPrism_distort_0(*uv_undist_2, dist_coeffs_2);
        _S374 = (length_1(_S376 - uv_4)) < 0.00999999977648258f;
    }
    else
    {
        _S374 = false;
    }
    return _S374;
}

inline __device__ float3  normalize_0(float3  x_20)
{
    return x_20 / make_float3 (length_0(x_20));
}

inline __device__ float3  unproject_raydir_0(float2  uv_5, int camera_model_0, bool is_ray_depth_0)
{
    if(camera_model_0 == int(4))
    {
        return make_float3 (0.0f, 0.0f, 1.0f);
    }
    float3  raydir_0;
    bool is_unit_0;
    if(camera_model_0 == int(1))
    {
        float theta_0 = length_1(uv_5);
        float3  _S377 = make_float3 ((uv_5 / make_float2 ((F32_max((theta_0), (1.00000001168609742e-07f)))) * make_float2 ((F32_sin((theta_0))))).x, (uv_5 / make_float2 ((F32_max((theta_0), (1.00000001168609742e-07f)))) * make_float2 ((F32_sin((theta_0))))).y, (F32_cos((theta_0))));
        is_unit_0 = true;
        raydir_0 = _S377;
    }
    else
    {
        if(camera_model_0 == int(2))
        {
            float r_4 = length_1(uv_5);
            float3  _S378 = make_float3 ((uv_5 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_4 * r_4)))))))).x, (uv_5 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_4 * r_4)))))))).y, 1.0f - 0.5f * r_4 * r_4);
            is_unit_0 = true;
            raydir_0 = _S378;
        }
        else
        {
            bool _S379 = camera_model_0 == int(3);
            if(_S379)
            {
                float lon_0 = uv_5.x;
                float lat_0 = uv_5.y;
                float cl_0 = (F32_cos((lat_0)));
                raydir_0 = make_float3 (cl_0 * (F32_sin((lon_0))), (F32_sin((lat_0))), cl_0 * (F32_cos((lon_0))));
            }
            else
            {
                raydir_0 = make_float3 (uv_5.x, uv_5.y, 1.0f);
            }
            is_unit_0 = _S379;
        }
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

inline __device__ float3  generate_ray_d2n_none(float2  pix_pos_0, float4  intrins_0, FixedArray<float, 1>  dist_coeffs_3, int camera_model_1, bool is_ray_depth_1)
{
    float3  _S380;
    for(;;)
    {
        float2  uv_6 = (pix_pos_0 - float2 {intrins_0.z, intrins_0.w}) / float2 {intrins_0.x, intrins_0.y};
        FixedArray<float, 1>  _S381 = dist_coeffs_3;
        float2  uv_u_0;
        bool _S382 = undistort_point_0(uv_6, &_S381, int(12), &uv_u_0);
        if(!_S382)
        {
            int3  _S383 = make_int3 (int(0));
            float3  _S384 = make_float3 ((float)_S383.x, (float)_S383.y, (float)_S383.z);
            _S380 = _S384;
            break;
        }
        _S380 = unproject_raydir_0(uv_u_0, camera_model_1, is_ray_depth_1);
        break;
    }
    return _S380;
}

inline __device__ float3  depth_to_point_none(float2  pix_pos_1, float4  intrins_1, FixedArray<float, 1>  dist_coeffs_4, int camera_model_2, bool is_ray_depth_2, float depth_2)
{
    float3  _S385;
    for(;;)
    {
        float2  uv_7 = (pix_pos_1 - float2 {intrins_1.z, intrins_1.w}) / float2 {intrins_1.x, intrins_1.y};
        FixedArray<float, 1>  _S386 = dist_coeffs_4;
        float2  uv_u_1;
        bool _S387 = undistort_point_0(uv_7, &_S386, int(12), &uv_u_1);
        if(!_S387)
        {
            _S385 = make_float3 (0.0f);
            break;
        }
        if(camera_model_2 == int(4))
        {
            _S385 = make_float3 (uv_7.x, uv_7.y, depth_2);
            break;
        }
        _S385 = make_float3 (depth_2) * unproject_raydir_0(uv_u_1, camera_model_2, is_ray_depth_2);
        break;
    }
    return _S385;
}

struct s_bwd_prop_depth_to_point_Intermediates_0
{
    float2  _S388;
    bool _S389;
};

inline __device__ float s_primal_ctx_sin_0(float _S390)
{
    return (F32_sin((_S390)));
}

inline __device__ float s_primal_ctx_cos_0(float _S391)
{
    return (F32_cos((_S391)));
}

inline __device__ float s_primal_ctx_sqrt_0(float _S392)
{
    return (F32_sqrt((_S392)));
}

inline __device__ float3  s_primal_ctx_unproject_raydir_0(float2  dpuv_2, int camera_model_3, bool is_ray_depth_3)
{
    bool _S393 = camera_model_3 == int(4);
    float3  raydir_1;
    if(_S393)
    {
        raydir_1 = make_float3 (0.0f, 0.0f, 1.0f);
    }
    bool _S394 = !_S393;
    if(_S394)
    {
        bool is_unit_1;
        if(camera_model_3 == int(1))
        {
            float _S395 = length_1(dpuv_2);
            float3  _S396 = make_float3 ((dpuv_2 / make_float2 ((F32_max((_S395), (1.00000001168609742e-07f)))) * make_float2 (s_primal_ctx_sin_0(_S395))).x, (dpuv_2 / make_float2 ((F32_max((_S395), (1.00000001168609742e-07f)))) * make_float2 (s_primal_ctx_sin_0(_S395))).y, s_primal_ctx_cos_0(_S395));
            is_unit_1 = true;
            raydir_1 = _S396;
        }
        else
        {
            if(camera_model_3 == int(2))
            {
                float _S397 = length_1(dpuv_2);
                float3  _S398 = make_float3 ((dpuv_2 * make_float2 (s_primal_ctx_sqrt_0((F32_max((0.0f), (1.0f - 0.25f * _S397 * _S397)))))).x, (dpuv_2 * make_float2 (s_primal_ctx_sqrt_0((F32_max((0.0f), (1.0f - 0.25f * _S397 * _S397)))))).y, 1.0f - 0.5f * _S397 * _S397);
                is_unit_1 = true;
                raydir_1 = _S398;
            }
            else
            {
                bool _S399 = camera_model_3 == int(3);
                if(_S399)
                {
                    float lon_1 = dpuv_2.x;
                    float lat_1 = dpuv_2.y;
                    float _S400 = s_primal_ctx_cos_0(lat_1);
                    raydir_1 = make_float3 (_S400 * s_primal_ctx_sin_0(lon_1), s_primal_ctx_sin_0(lat_1), _S400 * s_primal_ctx_cos_0(lon_1));
                }
                else
                {
                    raydir_1 = make_float3 (dpuv_2.x, dpuv_2.y, 1.0f);
                }
                is_unit_1 = _S399;
            }
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
    }
    return raydir_1;
}

inline __device__ float depth_to_point_vjp_none(float2  pix_pos_2, float4  intrins_2, FixedArray<float, 1>  dist_coeffs_5, int camera_model_4, bool is_ray_depth_4, float depth_3, float3  v_point_0)
{
    float2  _S401 = make_float2 (0.0f);
    s_bwd_prop_depth_to_point_Intermediates_0 _S402;
    (&_S402)->_S388 = _S401;
    (&_S402)->_S389 = false;
    float2  uv_8 = (pix_pos_2 - float2 {intrins_2.z, intrins_2.w}) / float2 {intrins_2.x, intrins_2.y};
    float2  _S403 = _S401;
    FixedArray<float, 1>  _S404 = dist_coeffs_5;
    bool _S405 = undistort_point_0(uv_8, &_S404, int(12), &_S403);
    (&_S402)->_S388 = _S403;
    (&_S402)->_S389 = _S405;
    s_bwd_prop_depth_to_point_Intermediates_0 _S406 = _S402;
    float3  _S407 = make_float3 (0.0f);
    bool _S408 = !!_S402._S389;
    bool _runFlag_3;
    bool _S409;
    float3  _S410;
    if(_S408)
    {
        bool _S411 = camera_model_4 == int(4);
        if(_S411)
        {
            _runFlag_3 = false;
        }
        else
        {
            _runFlag_3 = _S408;
        }
        if(_runFlag_3)
        {
            _S410 = s_primal_ctx_unproject_raydir_0(_S406._S388, camera_model_4, is_ray_depth_4);
        }
        else
        {
            _S410 = _S407;
        }
        _S409 = _S411;
    }
    else
    {
        _runFlag_3 = false;
        _S410 = _S407;
        _S409 = false;
    }
    float _S412;
    if(_S408)
    {
        float3  _S413;
        if(_runFlag_3)
        {
            float3  _S414 = _S410 * v_point_0;
            _S410 = _S407;
            _S413 = _S414;
        }
        else
        {
            _S410 = v_point_0;
            _S413 = _S407;
        }
        if(_S409)
        {
            _S412 = _S410.z;
        }
        else
        {
            _S412 = 0.0f;
        }
        _S410 = _S413;
    }
    else
    {
        _S410 = _S407;
        _S412 = 0.0f;
    }
    return _S410.x + _S410.y + _S410.z + _S412;
}

inline __device__ float3  depth_to_normal_none(float2  pix_center_0, float4  intrins_3, FixedArray<float, 1>  dist_coeffs_6, int camera_model_5, bool is_ray_depth_5, float4  depths_0)
{
    float3  normal_2;
    for(;;)
    {
        bool _S415;
        if((depths_0.x) == 0.0f)
        {
            _S415 = true;
        }
        else
        {
            _S415 = (depths_0.y) == 0.0f;
        }
        if(_S415)
        {
            _S415 = true;
        }
        else
        {
            _S415 = (depths_0.z) == 0.0f;
        }
        if(_S415)
        {
            _S415 = true;
        }
        else
        {
            _S415 = (depths_0.w) == 0.0f;
        }
        if(_S415)
        {
            normal_2 = make_float3 (0.0f);
            break;
        }
        float3  * _S416;
        float3  * _S417;
        float3  * _S418;
        float3  * _S419;
        int _S420;
        FixedArray<float3 , 4>  points_2;
        for(;;)
        {
            float2  _S421 = float2 {intrins_3.z, intrins_3.w};
            float2  _S422 = float2 {intrins_3.x, intrins_3.y};
            float2  uv_9 = (pix_center_0 + make_float2 (-1.0f, -0.0f) - _S421) / _S422;
            FixedArray<float, 1>  _S423 = dist_coeffs_6;
            float2  uv_u_2;
            bool _S424 = undistort_point_0(uv_9, &_S423, int(12), &uv_u_2);
            if(!_S424)
            {
                float3  _S425 = make_float3 (0.0f);
                _S420 = int(0);
                _S419 = nullptr;
                _S418 = nullptr;
                _S417 = nullptr;
                _S416 = nullptr;
                normal_2 = _S425;
                break;
            }
            points_2[int(0)] = make_float3 (depths_0.x) * unproject_raydir_0(uv_u_2, camera_model_5, is_ray_depth_5);
            for(;;)
            {
                float2  uv_10 = (pix_center_0 + make_float2 (1.0f, -0.0f) - _S421) / _S422;
                FixedArray<float, 1>  _S426 = dist_coeffs_6;
                float2  uv_u_3;
                bool _S427 = undistort_point_0(uv_10, &_S426, int(12), &uv_u_3);
                if(!_S427)
                {
                    float3  _S428 = make_float3 (0.0f);
                    _S420 = int(0);
                    _S419 = nullptr;
                    normal_2 = _S428;
                    break;
                }
                points_2[int(1)] = make_float3 (depths_0.y) * unproject_raydir_0(uv_u_3, camera_model_5, is_ray_depth_5);
                _S420 = int(2);
                _S419 = &points_2[int(1)];
                break;
            }
            if(_S420 != int(2))
            {
                _S418 = &points_2[int(0)];
                _S417 = nullptr;
                _S416 = nullptr;
                break;
            }
            float2  uv_11 = (pix_center_0 + make_float2 (0.0f, -1.0f) - _S421) / _S422;
            FixedArray<float, 1>  _S429 = dist_coeffs_6;
            float2  uv_u_4;
            bool _S430 = undistort_point_0(uv_11, &_S429, int(12), &uv_u_4);
            if(!_S430)
            {
                float3  _S431 = make_float3 (0.0f);
                _S420 = int(0);
                _S418 = &points_2[int(0)];
                _S417 = nullptr;
                _S416 = nullptr;
                normal_2 = _S431;
                break;
            }
            points_2[int(2)] = make_float3 (depths_0.z) * unproject_raydir_0(uv_u_4, camera_model_5, is_ray_depth_5);
            for(;;)
            {
                float2  uv_12 = (pix_center_0 + make_float2 (0.0f, 1.0f) - _S421) / _S422;
                FixedArray<float, 1>  _S432 = dist_coeffs_6;
                float2  uv_u_5;
                bool _S433 = undistort_point_0(uv_12, &_S432, int(12), &uv_u_5);
                if(!_S433)
                {
                    float3  _S434 = make_float3 (0.0f);
                    _S420 = int(0);
                    _S418 = nullptr;
                    normal_2 = _S434;
                    break;
                }
                points_2[int(3)] = make_float3 (depths_0.w) * unproject_raydir_0(uv_u_5, camera_model_5, is_ray_depth_5);
                _S420 = int(2);
                _S418 = &points_2[int(3)];
                break;
            }
            if(_S420 != int(2))
            {
                float3  * _S435 = _S418;
                _S418 = &points_2[int(0)];
                _S417 = _S435;
                _S416 = &points_2[int(2)];
                break;
            }
            float3  * _S436 = _S418;
            _S420 = int(1);
            _S418 = &points_2[int(0)];
            _S417 = _S436;
            _S416 = &points_2[int(2)];
            break;
        }
        if(_S420 != int(1))
        {
            break;
        }
        float3  normal_3 = cross_0(*_S419 - *_S418, - (*_S417 - *_S416));
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
    float2  _S437;
    bool _S438;
    float2  _S439;
    bool _S440;
    float2  _S441;
    bool _S442;
    float2  _S443;
    bool _S444;
};

inline __device__ void depth_to_normal_vjp_none(float2  pix_center_1, float4  intrins_4, FixedArray<float, 1>  dist_coeffs_7, int camera_model_6, bool is_ray_depth_6, float4  depths_1, float3  v_normal_1, float4  * v_depths_0)
{
    float2  _S445 = make_float2 (0.0f);
    s_bwd_prop_depth_to_normal_Intermediates_0 _S446;
    (&_S446)->_S437 = _S445;
    (&_S446)->_S438 = false;
    (&_S446)->_S439 = _S445;
    (&_S446)->_S440 = false;
    (&_S446)->_S441 = _S445;
    (&_S446)->_S442 = false;
    (&_S446)->_S443 = _S445;
    (&_S446)->_S444 = false;
    (&_S446)->_S437 = _S445;
    (&_S446)->_S438 = false;
    (&_S446)->_S439 = _S445;
    (&_S446)->_S440 = false;
    (&_S446)->_S441 = _S445;
    (&_S446)->_S442 = false;
    (&_S446)->_S443 = _S445;
    (&_S446)->_S444 = false;
    bool _S447 = (depths_1.x) == 0.0f;
    bool _runFlag_4;
    if(_S447)
    {
        _runFlag_4 = true;
    }
    else
    {
        _runFlag_4 = (depths_1.y) == 0.0f;
    }
    if(_runFlag_4)
    {
        _runFlag_4 = true;
    }
    else
    {
        _runFlag_4 = (depths_1.z) == 0.0f;
    }
    if(_runFlag_4)
    {
        _runFlag_4 = true;
    }
    else
    {
        _runFlag_4 = (depths_1.w) == 0.0f;
    }
    int _S448;
    if(!_runFlag_4)
    {
        float2  _S449 = float2 {intrins_4.z, intrins_4.w};
        float2  _S450 = float2 {intrins_4.x, intrins_4.y};
        float2  uv_13 = (pix_center_1 + make_float2 (-1.0f, -0.0f) - _S449) / _S450;
        float2  _S451 = _S445;
        FixedArray<float, 1>  _S452 = dist_coeffs_7;
        bool _S453 = undistort_point_0(uv_13, &_S452, int(12), &_S451);
        (&_S446)->_S437 = _S451;
        (&_S446)->_S438 = _S453;
        bool _S454 = !!_S453;
        if(_S454)
        {
            float2  uv_14 = (pix_center_1 + make_float2 (1.0f, -0.0f) - _S449) / _S450;
            float2  _S455 = _S445;
            FixedArray<float, 1>  _S456 = dist_coeffs_7;
            bool _S457 = undistort_point_0(uv_14, &_S456, int(12), &_S455);
            (&_S446)->_S439 = _S455;
            (&_S446)->_S440 = _S457;
            if(!!_S457)
            {
                _S448 = int(2);
            }
            else
            {
                _S448 = int(0);
            }
            if(_S448 != int(2))
            {
                _runFlag_4 = false;
            }
            else
            {
                _runFlag_4 = _S454;
            }
            if(_runFlag_4)
            {
                float2  uv_15 = (pix_center_1 + make_float2 (0.0f, -1.0f) - _S449) / _S450;
                float2  _S458 = _S445;
                FixedArray<float, 1>  _S459 = dist_coeffs_7;
                bool _S460 = undistort_point_0(uv_15, &_S459, int(12), &_S458);
                (&_S446)->_S441 = _S458;
                (&_S446)->_S442 = _S460;
                if(!_S460)
                {
                    _runFlag_4 = false;
                }
                if(_runFlag_4)
                {
                    float2  uv_16 = (pix_center_1 + make_float2 (0.0f, 1.0f) - _S449) / _S450;
                    float2  _S461 = _S445;
                    FixedArray<float, 1>  _S462 = dist_coeffs_7;
                    bool _S463 = undistort_point_0(uv_16, &_S462, int(12), &_S461);
                    (&_S446)->_S443 = _S461;
                    (&_S446)->_S444 = _S463;
                }
            }
        }
    }
    s_bwd_prop_depth_to_normal_Intermediates_0 _S464 = _S446;
    float3  _S465 = make_float3 (0.0f);
    if(_S447)
    {
        _runFlag_4 = true;
    }
    else
    {
        _runFlag_4 = (depths_1.y) == 0.0f;
    }
    if(_runFlag_4)
    {
        _runFlag_4 = true;
    }
    else
    {
        _runFlag_4 = (depths_1.z) == 0.0f;
    }
    if(_runFlag_4)
    {
        _runFlag_4 = true;
    }
    else
    {
        _runFlag_4 = (depths_1.w) == 0.0f;
    }
    bool _S466 = !_runFlag_4;
    bool _runFlag_5;
    bool _runFlag_6;
    bool _S467;
    bool _runFlag_7;
    bool _S468;
    bool _S469;
    FixedArray<float3 , 4>  points_3;
    float3  _S470;
    float3  _S471;
    float3  _S472;
    float3  _S473;
    float3  _S474;
    float3  _S475;
    float3  _S476;
    float3  _S477;
    float3  _S478;
    if(_S466)
    {
        bool _S479 = !!_S464._S438;
        if(_S479)
        {
            float3  _S480 = s_primal_ctx_unproject_raydir_0(_S464._S437, camera_model_6, is_ray_depth_6);
            float3  _S481 = make_float3 (depths_1.x) * _S480;
            bool _S482 = !!_S464._S440;
            if(_S482)
            {
                float3  _S483 = s_primal_ctx_unproject_raydir_0(_S464._S439, camera_model_6, is_ray_depth_6);
                float3  _S484 = make_float3 (depths_1.y) * _S483;
                _S448 = int(2);
                points_3[int(0)] = _S481;
                points_3[int(1)] = _S484;
                points_3[int(2)] = _S465;
                points_3[int(3)] = _S465;
                _S470 = _S483;
            }
            else
            {
                _S448 = int(0);
                points_3[int(0)] = _S481;
                points_3[int(1)] = _S465;
                points_3[int(2)] = _S465;
                points_3[int(3)] = _S465;
                _S470 = _S465;
            }
            if(_S448 != int(2))
            {
                _runFlag_4 = false;
            }
            else
            {
                _runFlag_4 = _S479;
                _S448 = int(0);
            }
            if(_runFlag_4)
            {
                if(!_S464._S442)
                {
                    _runFlag_5 = false;
                    _S448 = int(0);
                }
                else
                {
                    _runFlag_5 = _runFlag_4;
                }
                if(_runFlag_5)
                {
                    float3  _S485 = s_primal_ctx_unproject_raydir_0(_S464._S441, camera_model_6, is_ray_depth_6);
                    points_3[int(2)] = make_float3 (depths_1.z) * _S485;
                    bool _S486 = !!_S464._S444;
                    int _S487;
                    if(_S486)
                    {
                        float3  _S488 = s_primal_ctx_unproject_raydir_0(_S464._S443, camera_model_6, is_ray_depth_6);
                        points_3[int(3)] = make_float3 (depths_1.w) * _S488;
                        _S487 = int(2);
                        _S471 = _S488;
                    }
                    else
                    {
                        _S487 = int(0);
                        _S471 = _S465;
                    }
                    if(_S487 != int(2))
                    {
                        _runFlag_6 = false;
                        _S448 = _S487;
                    }
                    else
                    {
                        _runFlag_6 = _runFlag_5;
                    }
                    if(_runFlag_6)
                    {
                        _S448 = int(1);
                    }
                    _runFlag_6 = _S486;
                    _S472 = _S485;
                }
                else
                {
                    _runFlag_6 = false;
                    _S471 = _S465;
                    _S472 = _S465;
                }
            }
            else
            {
                _runFlag_5 = false;
                _runFlag_6 = false;
                _S471 = _S465;
                _S472 = _S465;
            }
            float3  _S489 = _S470;
            _S470 = _S471;
            _S471 = _S472;
            _S467 = _S482;
            _S472 = _S489;
            _S473 = _S480;
        }
        else
        {
            _S448 = int(0);
            points_3[int(0)] = _S465;
            points_3[int(1)] = _S465;
            points_3[int(2)] = _S465;
            points_3[int(3)] = _S465;
            _runFlag_4 = false;
            _runFlag_5 = false;
            _runFlag_6 = false;
            _S470 = _S465;
            _S471 = _S465;
            _S467 = false;
            _S472 = _S465;
            _S473 = _S465;
        }
        if(_S448 != int(1))
        {
            _runFlag_7 = false;
        }
        else
        {
            _runFlag_7 = _S466;
        }
        if(_runFlag_7)
        {
            float3  dx_1 = points_3[int(1)] - points_3[int(0)];
            float3  _S490 = - (points_3[int(3)] - points_3[int(2)]);
            float3  _S491 = s_primal_ctx_cross_0(dx_1, _S490);
            bool _S492 = (s_primal_ctx_dot_0(_S491, _S491)) != 0.0f;
            if(_S492)
            {
                float _S493 = length_0(_S491);
                float3  _S494 = make_float3 (_S493);
                _S474 = make_float3 (_S493 * _S493);
                _S475 = _S494;
            }
            else
            {
                _S474 = _S465;
                _S475 = _S465;
            }
            float3  _S495 = _S475;
            _S468 = _S492;
            _S475 = _S491;
            _S476 = _S495;
            _S477 = dx_1;
            _S478 = _S490;
        }
        else
        {
            _S468 = false;
            _S474 = _S465;
            _S475 = _S465;
            _S476 = _S465;
            _S477 = _S465;
            _S478 = _S465;
        }
        bool _S496 = _runFlag_4;
        bool _S497 = _runFlag_5;
        bool _S498 = _runFlag_6;
        float3  _S499 = _S470;
        float3  _S500 = _S471;
        bool _S501 = _S467;
        float3  _S502 = _S472;
        float3  _S503 = _S473;
        _runFlag_4 = _runFlag_7;
        _runFlag_5 = _S468;
        _S470 = _S474;
        _S471 = _S475;
        _S472 = _S476;
        _S473 = _S477;
        _S474 = _S478;
        _runFlag_6 = _S479;
        _S467 = _S496;
        _runFlag_7 = _S497;
        _S468 = _S498;
        _S475 = _S499;
        _S476 = _S500;
        _S469 = _S501;
        _S477 = _S502;
        _S478 = _S503;
    }
    else
    {
        _runFlag_4 = false;
        _runFlag_5 = false;
        _S470 = _S465;
        _S471 = _S465;
        _S472 = _S465;
        _S473 = _S465;
        _S474 = _S465;
        _runFlag_6 = false;
        _S467 = false;
        _runFlag_7 = false;
        _S468 = false;
        _S475 = _S465;
        _S476 = _S465;
        _S469 = false;
        _S477 = _S465;
        _S478 = _S465;
    }
    float4  _S504 = make_float4 (0.0f);
    float4  _S505;
    if(_S466)
    {
        if(_runFlag_4)
        {
            if(_runFlag_5)
            {
                float3  _S506 = v_normal_1 / _S470;
                float3  _S507 = _S471 * - _S506;
                float3  _S508 = _S472 * _S506;
                float _S509 = _S507.x + _S507.y + _S507.z;
                DiffPair_vectorx3Cfloatx2C3x3E_0 _S510;
                (&_S510)->primal_0 = _S471;
                (&_S510)->differential_0 = _S465;
                s_bwd_length_impl_0(&_S510, _S509);
                _S470 = _S508 + _S510.differential_0;
            }
            else
            {
                _S470 = v_normal_1;
            }
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S511;
            (&_S511)->primal_0 = _S471;
            (&_S511)->differential_0 = _S465;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S512;
            (&_S512)->primal_0 = _S471;
            (&_S512)->differential_0 = _S465;
            s_bwd_prop_dot_0(&_S511, &_S512, 0.0f);
            float3  _S513 = _S512.differential_0 + _S511.differential_0 + _S470;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S514;
            (&_S514)->primal_0 = _S473;
            (&_S514)->differential_0 = _S465;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S515;
            (&_S515)->primal_0 = _S474;
            (&_S515)->differential_0 = _S465;
            s_bwd_prop_cross_0(&_S514, &_S515, _S513);
            float3  s_diff_dy_T_1 = - _S515.differential_0;
            float3  _S516 = - s_diff_dy_T_1;
            float3  _S517 = - _S514.differential_0;
            FixedArray<float3 , 4>  _S518;
            _S518[int(0)] = _S465;
            _S518[int(1)] = _S465;
            _S518[int(2)] = _S465;
            _S518[int(3)] = _S465;
            _S518[int(2)] = _S516;
            _S518[int(3)] = s_diff_dy_T_1;
            _S518[int(0)] = _S517;
            _S518[int(1)] = _S514.differential_0;
            points_3[int(0)] = _S518[int(0)];
            points_3[int(1)] = _S518[int(1)];
            points_3[int(2)] = _S518[int(2)];
            points_3[int(3)] = _S518[int(3)];
        }
        else
        {
            points_3[int(0)] = _S465;
            points_3[int(1)] = _S465;
            points_3[int(2)] = _S465;
            points_3[int(3)] = _S465;
        }
        if(_runFlag_6)
        {
            if(_S467)
            {
                if(_runFlag_7)
                {
                    FixedArray<float3 , 4>  _S519 = points_3;
                    FixedArray<float3 , 4>  _S520 = points_3;
                    FixedArray<float3 , 4>  _S521 = points_3;
                    FixedArray<float3 , 4>  _S522 = points_3;
                    if(_S468)
                    {
                        float3  _S523 = _S475 * _S522[int(3)];
                        float _S524 = _S523.x + _S523.y + _S523.z;
                        float4  _S525 = _S504;
                        *&((&_S525)->w) = _S524;
                        points_3[int(0)] = _S519[int(0)];
                        points_3[int(1)] = _S520[int(1)];
                        points_3[int(2)] = _S521[int(2)];
                        points_3[int(3)] = _S465;
                        _S505 = _S525;
                    }
                    else
                    {
                        points_3[int(0)] = _S519[int(0)];
                        points_3[int(1)] = _S520[int(1)];
                        points_3[int(2)] = _S521[int(2)];
                        points_3[int(3)] = _S522[int(3)];
                        _S505 = _S504;
                    }
                    float3  _S526 = _S476 * points_3[int(2)];
                    float _S527 = _S526.x + _S526.y + _S526.z;
                    FixedArray<float3 , 4>  _S528 = points_3;
                    FixedArray<float3 , 4>  _S529 = points_3;
                    float4  _S530 = _S504;
                    *&((&_S530)->z) = _S527;
                    float4  _S531 = _S505 + _S530;
                    points_3[int(0)] = points_3[int(0)];
                    points_3[int(1)] = _S528[int(1)];
                    points_3[int(2)] = _S465;
                    points_3[int(3)] = _S529[int(3)];
                    _S505 = _S531;
                }
                else
                {
                    FixedArray<float3 , 4>  _S532 = points_3;
                    FixedArray<float3 , 4>  _S533 = points_3;
                    FixedArray<float3 , 4>  _S534 = points_3;
                    points_3[int(0)] = points_3[int(0)];
                    points_3[int(1)] = _S532[int(1)];
                    points_3[int(2)] = _S533[int(2)];
                    points_3[int(3)] = _S534[int(3)];
                    _S505 = _S504;
                }
            }
            else
            {
                FixedArray<float3 , 4>  _S535 = points_3;
                FixedArray<float3 , 4>  _S536 = points_3;
                FixedArray<float3 , 4>  _S537 = points_3;
                points_3[int(0)] = points_3[int(0)];
                points_3[int(1)] = _S535[int(1)];
                points_3[int(2)] = _S536[int(2)];
                points_3[int(3)] = _S537[int(3)];
                _S505 = _S504;
            }
            if(_S469)
            {
                FixedArray<float3 , 4>  _S538 = points_3;
                float3  _S539 = _S477 * points_3[int(1)];
                float _S540 = _S539.x + _S539.y + _S539.z;
                float4  _S541 = _S504;
                *&((&_S541)->y) = _S540;
                float4  _S542 = _S505 + _S541;
                points_3[int(0)] = _S465;
                points_3[int(1)] = _S465;
                points_3[int(2)] = _S465;
                points_3[int(3)] = _S465;
                _S470 = _S538[int(0)];
                _S505 = _S542;
            }
            else
            {
                FixedArray<float3 , 4>  _S543 = points_3;
                FixedArray<float3 , 4>  _S544 = points_3;
                FixedArray<float3 , 4>  _S545 = points_3;
                points_3[int(0)] = points_3[int(0)];
                points_3[int(1)] = _S543[int(1)];
                points_3[int(2)] = _S544[int(2)];
                points_3[int(3)] = _S545[int(3)];
                _S470 = _S465;
            }
            float3  _S546 = _S478 * (points_3[int(0)] + _S470);
            float _S547 = _S546.x + _S546.y + _S546.z;
            float4  _S548 = _S504;
            *&((&_S548)->x) = _S547;
            _S505 = _S505 + _S548;
        }
        else
        {
            _S505 = _S504;
        }
    }
    else
    {
        _S505 = _S504;
    }
    *v_depths_0 = _S505;
    return;
}

inline __device__ float ray_depth_to_linear_depth_factor_none(float2  pix_center_2, float4  intrins_5, FixedArray<float, 1>  dist_coeffs_8, int camera_model_7)
{
    float _S549;
    for(;;)
    {
        float2  uv_17 = (pix_center_2 - float2 {intrins_5.z, intrins_5.w}) / float2 {intrins_5.x, intrins_5.y};
        FixedArray<float, 1>  _S550 = dist_coeffs_8;
        float2  uv_u_6;
        bool _S551 = undistort_point_0(uv_17, &_S550, int(12), &uv_u_6);
        if(!_S551)
        {
            _S549 = 0.0f;
            break;
        }
        float3  raydir_2 = unproject_raydir_0(uv_u_6, camera_model_7, false);
        _S549 = float((F32_sign((raydir_2.z)))) / length_0(raydir_2);
        break;
    }
    return _S549;
}

inline __device__ float depth_normal_loss_none(float2  pix_center_3, float4  intrins_6, FixedArray<float, 1>  dist_coeffs_9, int camera_model_8, bool is_ray_depth_7, float4  depths_2, float3  gt_normal_0)
{
    float _S552;
    for(;;)
    {
        float3  _S553;
        float3  * _S554;
        float3  * _S555;
        float3  * _S556;
        float3  * _S557;
        int _S558;
        FixedArray<float3 , 5>  points_4;
        for(;;)
        {
            float2  _S559 = float2 {intrins_6.z, intrins_6.w};
            float2  _S560 = float2 {intrins_6.x, intrins_6.y};
            float2  uv_18 = (pix_center_3 + make_float2 (-1.0f, -0.0f) - _S559) / _S560;
            FixedArray<float, 1>  _S561 = dist_coeffs_9;
            float2  uv_u_7;
            bool _S562 = undistort_point_0(uv_18, &_S561, int(12), &uv_u_7);
            float3  _S563 = make_float3 (0.0f);
            if(!_S562)
            {
                _S558 = int(0);
                _S557 = nullptr;
                _S556 = nullptr;
                _S555 = nullptr;
                _S554 = nullptr;
                _S553 = _S563;
                break;
            }
            float3  raydir_3 = unproject_raydir_0(uv_u_7, camera_model_8, is_ray_depth_7);
            points_4[int(0)] = make_float3 (depths_2.x) * raydir_3;
            float2  uv_19 = (pix_center_3 + make_float2 (1.0f, -0.0f) - _S559) / _S560;
            FixedArray<float, 1>  _S564 = dist_coeffs_9;
            float2  uv_u_8;
            bool _S565 = undistort_point_0(uv_19, &_S564, int(12), &uv_u_8);
            if(!_S565)
            {
                _S558 = int(0);
                _S557 = nullptr;
                _S556 = &points_4[int(0)];
                _S555 = nullptr;
                _S554 = nullptr;
                _S553 = _S563;
                break;
            }
            float3  raydir_4 = unproject_raydir_0(uv_u_8, camera_model_8, is_ray_depth_7);
            points_4[int(1)] = make_float3 (depths_2.y) * raydir_4;
            float2  uv_20 = (pix_center_3 + make_float2 (0.0f, -1.0f) - _S559) / _S560;
            FixedArray<float, 1>  _S566 = dist_coeffs_9;
            float2  uv_u_9;
            bool _S567 = undistort_point_0(uv_20, &_S566, int(12), &uv_u_9);
            if(!_S567)
            {
                _S558 = int(0);
                _S557 = &points_4[int(1)];
                _S556 = &points_4[int(0)];
                _S555 = nullptr;
                _S554 = nullptr;
                _S553 = _S563;
                break;
            }
            float3  raydir_5 = unproject_raydir_0(uv_u_9, camera_model_8, is_ray_depth_7);
            points_4[int(2)] = make_float3 (depths_2.z) * raydir_5;
            float2  uv_21 = (pix_center_3 + make_float2 (0.0f, 1.0f) - _S559) / _S560;
            FixedArray<float, 1>  _S568 = dist_coeffs_9;
            float2  uv_u_10;
            bool _S569 = undistort_point_0(uv_21, &_S568, int(12), &uv_u_10);
            if(!_S569)
            {
                _S558 = int(0);
                _S557 = &points_4[int(1)];
                _S556 = &points_4[int(0)];
                _S555 = nullptr;
                _S554 = &points_4[int(2)];
                _S553 = _S563;
                break;
            }
            float3  raydir_6 = unproject_raydir_0(uv_u_10, camera_model_8, is_ray_depth_7);
            points_4[int(3)] = make_float3 (depths_2.w) * raydir_6;
            float2  uv_22 = (pix_center_3 + make_float2 (0.0f) * make_float2 (0.0f, 3.0f) - _S559) / _S560;
            FixedArray<float, 1>  _S570 = dist_coeffs_9;
            float2  uv_u_11;
            bool _S571 = undistort_point_0(uv_22, &_S570, int(12), &uv_u_11);
            if(!_S571)
            {
                _S558 = int(0);
                _S557 = &points_4[int(1)];
                _S556 = &points_4[int(0)];
                _S555 = &points_4[int(3)];
                _S554 = &points_4[int(2)];
                _S553 = _S563;
                break;
            }
            float3  raydir_7 = unproject_raydir_0(uv_u_11, camera_model_8, is_ray_depth_7);
            _S558 = int(1);
            _S557 = &points_4[int(1)];
            _S556 = &points_4[int(0)];
            _S555 = &points_4[int(3)];
            _S554 = &points_4[int(2)];
            _S553 = raydir_7;
            break;
        }
        if(_S558 != int(1))
        {
            _S552 = 0.0f;
            break;
        }
        float3  normal_4 = cross_0(*_S557 - *_S556, - (*_S555 - *_S554));
        float3  normal_5;
        if((dot_0(normal_4, normal_4)) != 0.0f)
        {
            normal_5 = normalize_0(normal_4);
        }
        else
        {
            normal_5 = normal_4;
        }
        float3  _S572;
        if((dot_0(gt_normal_0, gt_normal_0)) != 0.0f)
        {
            _S572 = normalize_0(gt_normal_0);
        }
        else
        {
            _S572 = gt_normal_0;
        }
        _S552 = (1.0f - dot_0(normal_5, _S572) + 0.00100000004749745f) / ((F32_max((dot_0(normal_5, - normalize_0(_S553))), (0.0f))) + 0.00100000004749745f);
        break;
    }
    return _S552;
}

struct s_bwd_prop_depth_normal_loss_Intermediates_0
{
    float2  _S573;
    bool _S574;
    float2  _S575;
    bool _S576;
    float2  _S577;
    bool _S578;
    float2  _S579;
    bool _S580;
    float2  _S581;
    bool _S582;
};

inline __device__ void s_bwd_prop_normalize_impl_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_14, float3  _s_dOut_9)
{
    float _S583 = length_0((*dpx_14).primal_0);
    float3  _S584 = (*dpx_14).primal_0 * _s_dOut_9;
    float3  _S585 = make_float3 (1.0f / _S583) * _s_dOut_9;
    float _S586 = - ((_S584.x + _S584.y + _S584.z) / (_S583 * _S583));
    float3  _S587 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S588;
    (&_S588)->primal_0 = (*dpx_14).primal_0;
    (&_S588)->differential_0 = _S587;
    s_bwd_length_impl_0(&_S588, _S586);
    float3  _S589 = _S585 + _S588.differential_0;
    dpx_14->primal_0 = (*dpx_14).primal_0;
    dpx_14->differential_0 = _S589;
    return;
}

inline __device__ void s_bwd_normalize_impl_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S590, float3  _S591)
{
    s_bwd_prop_normalize_impl_0(_S590, _S591);
    return;
}

inline __device__ void depth_normal_loss_vjp_none(float2  pix_center_4, float4  intrins_7, FixedArray<float, 1>  dist_coeffs_10, int camera_model_9, bool is_ray_depth_8, float4  depths_3, float3  gt_normal_1, float v_loss_0, float4  * v_depths_1, float3  * v_gt_normal_0)
{
    float2  _S592 = make_float2 (0.0f);
    s_bwd_prop_depth_normal_loss_Intermediates_0 _S593;
    (&_S593)->_S573 = _S592;
    (&_S593)->_S574 = false;
    (&_S593)->_S575 = _S592;
    (&_S593)->_S576 = false;
    (&_S593)->_S577 = _S592;
    (&_S593)->_S578 = false;
    (&_S593)->_S579 = _S592;
    (&_S593)->_S580 = false;
    (&_S593)->_S581 = _S592;
    (&_S593)->_S582 = false;
    (&_S593)->_S575 = _S592;
    (&_S593)->_S576 = false;
    (&_S593)->_S577 = _S592;
    (&_S593)->_S578 = false;
    (&_S593)->_S579 = _S592;
    (&_S593)->_S580 = false;
    (&_S593)->_S581 = _S592;
    (&_S593)->_S582 = false;
    float2  _S594 = float2 {intrins_7.z, intrins_7.w};
    float2  _S595 = float2 {intrins_7.x, intrins_7.y};
    float2  uv_23 = (pix_center_4 + make_float2 (-1.0f, -0.0f) - _S594) / _S595;
    float2  _S596 = _S592;
    FixedArray<float, 1>  _S597 = dist_coeffs_10;
    bool _S598 = undistort_point_0(uv_23, &_S597, int(12), &_S596);
    (&_S593)->_S573 = _S596;
    (&_S593)->_S574 = _S598;
    bool _S599 = !!_S598;
    bool _runFlag_8;
    if(_S599)
    {
        float2  uv_24 = (pix_center_4 + make_float2 (1.0f, -0.0f) - _S594) / _S595;
        float2  _S600 = _S592;
        FixedArray<float, 1>  _S601 = dist_coeffs_10;
        bool _S602 = undistort_point_0(uv_24, &_S601, int(12), &_S600);
        (&_S593)->_S575 = _S600;
        (&_S593)->_S576 = _S602;
        if(!_S602)
        {
            _runFlag_8 = false;
        }
        else
        {
            _runFlag_8 = _S599;
        }
        if(_runFlag_8)
        {
            float2  uv_25 = (pix_center_4 + make_float2 (0.0f, -1.0f) - _S594) / _S595;
            float2  _S603 = _S592;
            FixedArray<float, 1>  _S604 = dist_coeffs_10;
            bool _S605 = undistort_point_0(uv_25, &_S604, int(12), &_S603);
            (&_S593)->_S577 = _S603;
            (&_S593)->_S578 = _S605;
            if(!_S605)
            {
                _runFlag_8 = false;
            }
            if(_runFlag_8)
            {
                float2  uv_26 = (pix_center_4 + make_float2 (0.0f, 1.0f) - _S594) / _S595;
                float2  _S606 = _S592;
                FixedArray<float, 1>  _S607 = dist_coeffs_10;
                bool _S608 = undistort_point_0(uv_26, &_S607, int(12), &_S606);
                (&_S593)->_S579 = _S606;
                (&_S593)->_S580 = _S608;
                if(!_S608)
                {
                    _runFlag_8 = false;
                }
                if(_runFlag_8)
                {
                    float2  uv_27 = (pix_center_4 - _S594) / _S595;
                    float2  _S609 = _S592;
                    FixedArray<float, 1>  _S610 = dist_coeffs_10;
                    bool _S611 = undistort_point_0(uv_27, &_S610, int(12), &_S609);
                    (&_S593)->_S581 = _S609;
                    (&_S593)->_S582 = _S611;
                }
            }
        }
    }
    s_bwd_prop_depth_normal_loss_Intermediates_0 _S612 = _S593;
    float3  _S613 = make_float3 (0.0f);
    bool _S614 = !!_S593._S574;
    bool _runFlag_9;
    bool _runFlag_10;
    bool _runFlag_11;
    int _S615;
    float3  raydir_8;
    float3  _S616;
    float3  _S617;
    float3  _S618;
    float3  _S619;
    FixedArray<float3 , 5>  points_5;
    if(_S614)
    {
        float3  _S620 = s_primal_ctx_unproject_raydir_0(_S612._S573, camera_model_9, is_ray_depth_8);
        float3  _S621 = make_float3 (depths_3.x) * _S620;
        if(!_S612._S576)
        {
            _runFlag_8 = false;
        }
        else
        {
            _runFlag_8 = _S614;
        }
        if(_runFlag_8)
        {
            float3  _S622 = s_primal_ctx_unproject_raydir_0(_S612._S575, camera_model_9, is_ray_depth_8);
            float3  _S623 = make_float3 (depths_3.y) * _S622;
            if(!_S612._S578)
            {
                _runFlag_9 = false;
            }
            else
            {
                _runFlag_9 = _runFlag_8;
            }
            if(_runFlag_9)
            {
                float3  _S624 = s_primal_ctx_unproject_raydir_0(_S612._S577, camera_model_9, is_ray_depth_8);
                float3  _S625 = make_float3 (depths_3.z) * _S624;
                if(!_S612._S580)
                {
                    _runFlag_10 = false;
                }
                else
                {
                    _runFlag_10 = _runFlag_9;
                }
                if(_runFlag_10)
                {
                    float3  _S626 = s_primal_ctx_unproject_raydir_0(_S612._S579, camera_model_9, is_ray_depth_8);
                    float3  _S627 = make_float3 (depths_3.w) * _S626;
                    if(!_S612._S582)
                    {
                        _runFlag_11 = false;
                    }
                    else
                    {
                        _runFlag_11 = _runFlag_10;
                    }
                    if(_runFlag_11)
                    {
                        float3  _S628 = s_primal_ctx_unproject_raydir_0(_S612._S581, camera_model_9, is_ray_depth_8);
                        _S615 = int(1);
                        raydir_8 = _S628;
                    }
                    else
                    {
                        _S615 = int(0);
                        raydir_8 = _S626;
                    }
                    points_5[int(0)] = _S621;
                    points_5[int(1)] = _S623;
                    points_5[int(2)] = _S625;
                    points_5[int(3)] = _S627;
                    points_5[int(4)] = _S613;
                    _S616 = _S626;
                }
                else
                {
                    _S615 = int(0);
                    raydir_8 = _S624;
                    points_5[int(0)] = _S621;
                    points_5[int(1)] = _S623;
                    points_5[int(2)] = _S625;
                    points_5[int(3)] = _S613;
                    points_5[int(4)] = _S613;
                    _S616 = _S613;
                }
                _S617 = _S624;
            }
            else
            {
                _S615 = int(0);
                raydir_8 = _S622;
                points_5[int(0)] = _S621;
                points_5[int(1)] = _S623;
                points_5[int(2)] = _S613;
                points_5[int(3)] = _S613;
                points_5[int(4)] = _S613;
                _runFlag_10 = false;
                _S616 = _S613;
                _S617 = _S613;
            }
            _S618 = _S622;
        }
        else
        {
            _S615 = int(0);
            raydir_8 = _S620;
            points_5[int(0)] = _S621;
            points_5[int(1)] = _S613;
            points_5[int(2)] = _S613;
            points_5[int(3)] = _S613;
            points_5[int(4)] = _S613;
            _runFlag_9 = false;
            _runFlag_10 = false;
            _S616 = _S613;
            _S617 = _S613;
            _S618 = _S613;
        }
        _S619 = _S620;
    }
    else
    {
        _S615 = int(0);
        points_5[int(0)] = _S613;
        points_5[int(1)] = _S613;
        points_5[int(2)] = _S613;
        points_5[int(3)] = _S613;
        points_5[int(4)] = _S613;
        _runFlag_8 = false;
        _runFlag_9 = false;
        _runFlag_10 = false;
        _S616 = _S613;
        _S617 = _S613;
        _S618 = _S613;
        _S619 = _S613;
    }
    bool _S629 = !(_S615 != int(1));
    bool _S630;
    float3  normal_6;
    float3  _S631;
    float3  _S632;
    float3  _S633;
    float3  _S634;
    float _S635;
    float _S636;
    float _S637;
    float _S638;
    if(_S629)
    {
        float3  dx_2 = points_5[int(1)] - points_5[int(0)];
        float3  _S639 = - (points_5[int(3)] - points_5[int(2)]);
        float3  _S640 = s_primal_ctx_cross_0(dx_2, _S639);
        bool _S641 = (s_primal_ctx_dot_0(_S640, _S640)) != 0.0f;
        if(_S641)
        {
            normal_6 = normalize_0(_S640);
        }
        else
        {
            normal_6 = _S640;
        }
        bool _S642 = (s_primal_ctx_dot_0(gt_normal_1, gt_normal_1)) != 0.0f;
        if(_S642)
        {
            _S631 = normalize_0(gt_normal_1);
        }
        else
        {
            _S631 = gt_normal_1;
        }
        float3  _S643 = - normalize_0(raydir_8);
        float _S644 = s_primal_ctx_dot_0(normal_6, _S643);
        float _S645 = 1.0f - s_primal_ctx_dot_0(normal_6, _S631) + 0.00100000004749745f;
        float _S646 = (F32_max((_S644), (0.0f))) + 0.00100000004749745f;
        _S635 = _S646 * _S646;
        _S636 = _S645;
        _S637 = _S646;
        _S638 = _S644;
        raydir_8 = normal_6;
        normal_6 = _S643;
        _runFlag_11 = _S642;
        _S630 = _S641;
        _S632 = _S640;
        _S633 = dx_2;
        _S634 = _S639;
    }
    else
    {
        _S635 = 0.0f;
        _S636 = 0.0f;
        _S637 = 0.0f;
        _S638 = 0.0f;
        raydir_8 = _S613;
        normal_6 = _S613;
        _S631 = _S613;
        _runFlag_11 = false;
        _S630 = false;
        _S632 = _S613;
        _S633 = _S613;
        _S634 = _S613;
    }
    float4  _S647 = make_float4 (0.0f);
    if(_S629)
    {
        float _S648 = v_loss_0 / _S635;
        float _S649 = _S636 * - _S648;
        float s_diff_num_T_0 = _S637 * _S648;
        DiffPair_float_0 _S650;
        (&_S650)->primal_0 = _S638;
        (&_S650)->differential_0 = 0.0f;
        DiffPair_float_0 _S651;
        (&_S651)->primal_0 = 0.0f;
        (&_S651)->differential_0 = 0.0f;
        _d_max_0(&_S650, &_S651, _S649);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S652;
        (&_S652)->primal_0 = raydir_8;
        (&_S652)->differential_0 = _S613;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S653;
        (&_S653)->primal_0 = normal_6;
        (&_S653)->differential_0 = _S613;
        s_bwd_prop_dot_0(&_S652, &_S653, _S650.differential_0);
        float _S654 = - s_diff_num_T_0;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S655;
        (&_S655)->primal_0 = raydir_8;
        (&_S655)->differential_0 = _S613;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S656;
        (&_S656)->primal_0 = _S631;
        (&_S656)->differential_0 = _S613;
        s_bwd_prop_dot_0(&_S655, &_S656, _S654);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S657 = _S656;
        float3  _S658 = _S652.differential_0 + _S655.differential_0;
        if(_runFlag_11)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S659;
            (&_S659)->primal_0 = gt_normal_1;
            (&_S659)->differential_0 = _S613;
            s_bwd_normalize_impl_0(&_S659, _S657.differential_0);
            raydir_8 = _S659.differential_0;
        }
        else
        {
            raydir_8 = _S657.differential_0;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S660;
        (&_S660)->primal_0 = gt_normal_1;
        (&_S660)->differential_0 = _S613;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S661;
        (&_S661)->primal_0 = gt_normal_1;
        (&_S661)->differential_0 = _S613;
        s_bwd_prop_dot_0(&_S660, &_S661, 0.0f);
        float3  _S662 = _S661.differential_0 + _S660.differential_0 + raydir_8;
        if(_S630)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S663;
            (&_S663)->primal_0 = _S632;
            (&_S663)->differential_0 = _S613;
            s_bwd_normalize_impl_0(&_S663, _S658);
            raydir_8 = _S663.differential_0;
        }
        else
        {
            raydir_8 = _S658;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S664;
        (&_S664)->primal_0 = _S632;
        (&_S664)->differential_0 = _S613;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S665;
        (&_S665)->primal_0 = _S632;
        (&_S665)->differential_0 = _S613;
        s_bwd_prop_dot_0(&_S664, &_S665, 0.0f);
        float3  _S666 = _S665.differential_0 + _S664.differential_0 + raydir_8;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S667;
        (&_S667)->primal_0 = _S633;
        (&_S667)->differential_0 = _S613;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S668;
        (&_S668)->primal_0 = _S634;
        (&_S668)->differential_0 = _S613;
        s_bwd_prop_cross_0(&_S667, &_S668, _S666);
        float3  s_diff_dy_T_2 = - _S668.differential_0;
        float3  _S669 = - s_diff_dy_T_2;
        float3  _S670 = - _S667.differential_0;
        FixedArray<float3 , 5>  _S671;
        _S671[int(0)] = _S613;
        _S671[int(1)] = _S613;
        _S671[int(2)] = _S613;
        _S671[int(3)] = _S613;
        _S671[int(4)] = _S613;
        _S671[int(2)] = _S669;
        _S671[int(3)] = s_diff_dy_T_2;
        _S671[int(0)] = _S670;
        _S671[int(1)] = _S667.differential_0;
        points_5[int(0)] = _S671[int(0)];
        points_5[int(1)] = _S671[int(1)];
        points_5[int(2)] = _S671[int(2)];
        points_5[int(3)] = _S671[int(3)];
        points_5[int(4)] = _S671[int(4)];
        raydir_8 = _S662;
    }
    else
    {
        points_5[int(0)] = _S613;
        points_5[int(1)] = _S613;
        points_5[int(2)] = _S613;
        points_5[int(3)] = _S613;
        points_5[int(4)] = _S613;
        raydir_8 = _S613;
    }
    float4  _S672;
    if(_S614)
    {
        if(_runFlag_8)
        {
            if(_runFlag_9)
            {
                if(_runFlag_10)
                {
                    FixedArray<float3 , 5>  _S673 = points_5;
                    FixedArray<float3 , 5>  _S674 = points_5;
                    FixedArray<float3 , 5>  _S675 = points_5;
                    float3  _S676 = _S616 * points_5[int(3)];
                    float _S677 = _S676.x + _S676.y + _S676.z;
                    float4  _S678 = _S647;
                    *&((&_S678)->w) = _S677;
                    points_5[int(0)] = _S613;
                    points_5[int(1)] = _S613;
                    points_5[int(2)] = _S613;
                    points_5[int(3)] = _S613;
                    points_5[int(4)] = _S613;
                    _S616 = _S675[int(2)];
                    normal_6 = _S673[int(0)];
                    _S631 = _S674[int(1)];
                    _S672 = _S678;
                }
                else
                {
                    FixedArray<float3 , 5>  _S679 = points_5;
                    FixedArray<float3 , 5>  _S680 = points_5;
                    FixedArray<float3 , 5>  _S681 = points_5;
                    FixedArray<float3 , 5>  _S682 = points_5;
                    points_5[int(0)] = points_5[int(0)];
                    points_5[int(1)] = _S679[int(1)];
                    points_5[int(2)] = _S680[int(2)];
                    points_5[int(3)] = _S681[int(3)];
                    points_5[int(4)] = _S682[int(4)];
                    _S616 = _S613;
                    normal_6 = _S613;
                    _S631 = _S613;
                    _S672 = _S647;
                }
                float3  _S683 = _S617 * (points_5[int(2)] + _S616);
                float _S684 = _S683.x + _S683.y + _S683.z;
                float3  _S685 = points_5[int(0)] + normal_6;
                float3  _S686 = points_5[int(1)] + _S631;
                float4  _S687 = _S647;
                *&((&_S687)->z) = _S684;
                float4  _S688 = _S672 + _S687;
                points_5[int(0)] = _S613;
                points_5[int(1)] = _S613;
                points_5[int(2)] = _S613;
                points_5[int(3)] = _S613;
                points_5[int(4)] = _S613;
                _S616 = _S686;
                _S617 = _S685;
                _S672 = _S688;
            }
            else
            {
                FixedArray<float3 , 5>  _S689 = points_5;
                FixedArray<float3 , 5>  _S690 = points_5;
                FixedArray<float3 , 5>  _S691 = points_5;
                FixedArray<float3 , 5>  _S692 = points_5;
                points_5[int(0)] = points_5[int(0)];
                points_5[int(1)] = _S689[int(1)];
                points_5[int(2)] = _S690[int(2)];
                points_5[int(3)] = _S691[int(3)];
                points_5[int(4)] = _S692[int(4)];
                _S616 = _S613;
                _S617 = _S613;
                _S672 = _S647;
            }
            float3  _S693 = _S618 * (points_5[int(1)] + _S616);
            float _S694 = _S693.x + _S693.y + _S693.z;
            float3  _S695 = points_5[int(0)] + _S617;
            float4  _S696 = _S647;
            *&((&_S696)->y) = _S694;
            float4  _S697 = _S672 + _S696;
            points_5[int(0)] = _S613;
            points_5[int(1)] = _S613;
            points_5[int(2)] = _S613;
            points_5[int(3)] = _S613;
            points_5[int(4)] = _S613;
            _S616 = _S695;
            _S672 = _S697;
        }
        else
        {
            FixedArray<float3 , 5>  _S698 = points_5;
            FixedArray<float3 , 5>  _S699 = points_5;
            FixedArray<float3 , 5>  _S700 = points_5;
            FixedArray<float3 , 5>  _S701 = points_5;
            points_5[int(0)] = points_5[int(0)];
            points_5[int(1)] = _S698[int(1)];
            points_5[int(2)] = _S699[int(2)];
            points_5[int(3)] = _S700[int(3)];
            points_5[int(4)] = _S701[int(4)];
            _S616 = _S613;
            _S672 = _S647;
        }
        float3  _S702 = _S619 * (points_5[int(0)] + _S616);
        float _S703 = _S702.x + _S702.y + _S702.z;
        float4  _S704 = _S647;
        *&((&_S704)->x) = _S703;
        _S672 = _S672 + _S704;
    }
    else
    {
        _S672 = _S647;
    }
    *v_depths_1 = _S672;
    *v_gt_normal_0 = raydir_8;
    return;
}

inline __device__ float3  generate_ray_d2n_opencv(float2  pix_pos_3, float4  intrins_8, FixedArray<float, 4>  dist_coeffs_11, int camera_model_10, bool is_ray_depth_9)
{
    float3  _S705;
    for(;;)
    {
        float2  uv_28 = (pix_pos_3 - float2 {intrins_8.z, intrins_8.w}) / float2 {intrins_8.x, intrins_8.y};
        FixedArray<float, 4>  _S706 = dist_coeffs_11;
        float2  uv_u_12;
        bool _S707 = undistort_point_1(uv_28, &_S706, int(12), &uv_u_12);
        if(!_S707)
        {
            int3  _S708 = make_int3 (int(0));
            float3  _S709 = make_float3 ((float)_S708.x, (float)_S708.y, (float)_S708.z);
            _S705 = _S709;
            break;
        }
        _S705 = unproject_raydir_0(uv_u_12, camera_model_10, is_ray_depth_9);
        break;
    }
    return _S705;
}

inline __device__ float3  depth_to_point_opencv(float2  pix_pos_4, float4  intrins_9, FixedArray<float, 4>  dist_coeffs_12, int camera_model_11, bool is_ray_depth_10, float depth_4)
{
    float3  _S710;
    for(;;)
    {
        float2  uv_29 = (pix_pos_4 - float2 {intrins_9.z, intrins_9.w}) / float2 {intrins_9.x, intrins_9.y};
        FixedArray<float, 4>  _S711 = dist_coeffs_12;
        float2  uv_u_13;
        bool _S712 = undistort_point_1(uv_29, &_S711, int(12), &uv_u_13);
        if(!_S712)
        {
            _S710 = make_float3 (0.0f);
            break;
        }
        if(camera_model_11 == int(4))
        {
            _S710 = make_float3 (uv_29.x, uv_29.y, depth_4);
            break;
        }
        _S710 = make_float3 (depth_4) * unproject_raydir_0(uv_u_13, camera_model_11, is_ray_depth_10);
        break;
    }
    return _S710;
}

struct s_bwd_prop_depth_to_point_Intermediates_1
{
    float2  _S713;
    bool _S714;
};

inline __device__ float depth_to_point_vjp_opencv(float2  pix_pos_5, float4  intrins_10, FixedArray<float, 4>  dist_coeffs_13, int camera_model_12, bool is_ray_depth_11, float depth_5, float3  v_point_1)
{
    float2  _S715 = make_float2 (0.0f);
    s_bwd_prop_depth_to_point_Intermediates_1 _S716;
    (&_S716)->_S713 = _S715;
    (&_S716)->_S714 = false;
    float2  uv_30 = (pix_pos_5 - float2 {intrins_10.z, intrins_10.w}) / float2 {intrins_10.x, intrins_10.y};
    float2  _S717 = _S715;
    FixedArray<float, 4>  _S718 = dist_coeffs_13;
    bool _S719 = undistort_point_1(uv_30, &_S718, int(12), &_S717);
    (&_S716)->_S713 = _S717;
    (&_S716)->_S714 = _S719;
    s_bwd_prop_depth_to_point_Intermediates_1 _S720 = _S716;
    float3  _S721 = make_float3 (0.0f);
    bool _S722 = !!_S716._S714;
    bool _runFlag_12;
    bool _S723;
    float3  _S724;
    if(_S722)
    {
        bool _S725 = camera_model_12 == int(4);
        if(_S725)
        {
            _runFlag_12 = false;
        }
        else
        {
            _runFlag_12 = _S722;
        }
        if(_runFlag_12)
        {
            _S724 = s_primal_ctx_unproject_raydir_0(_S720._S713, camera_model_12, is_ray_depth_11);
        }
        else
        {
            _S724 = _S721;
        }
        _S723 = _S725;
    }
    else
    {
        _runFlag_12 = false;
        _S724 = _S721;
        _S723 = false;
    }
    float _S726;
    if(_S722)
    {
        float3  _S727;
        if(_runFlag_12)
        {
            float3  _S728 = _S724 * v_point_1;
            _S724 = _S721;
            _S727 = _S728;
        }
        else
        {
            _S724 = v_point_1;
            _S727 = _S721;
        }
        if(_S723)
        {
            _S726 = _S724.z;
        }
        else
        {
            _S726 = 0.0f;
        }
        _S724 = _S727;
    }
    else
    {
        _S724 = _S721;
        _S726 = 0.0f;
    }
    return _S724.x + _S724.y + _S724.z + _S726;
}

inline __device__ float3  depth_to_normal_opencv(float2  pix_center_5, float4  intrins_11, FixedArray<float, 4>  dist_coeffs_14, int camera_model_13, bool is_ray_depth_12, float4  depths_4)
{
    float3  normal_7;
    for(;;)
    {
        bool _S729;
        if((depths_4.x) == 0.0f)
        {
            _S729 = true;
        }
        else
        {
            _S729 = (depths_4.y) == 0.0f;
        }
        if(_S729)
        {
            _S729 = true;
        }
        else
        {
            _S729 = (depths_4.z) == 0.0f;
        }
        if(_S729)
        {
            _S729 = true;
        }
        else
        {
            _S729 = (depths_4.w) == 0.0f;
        }
        if(_S729)
        {
            normal_7 = make_float3 (0.0f);
            break;
        }
        float3  * _S730;
        float3  * _S731;
        float3  * _S732;
        float3  * _S733;
        int _S734;
        FixedArray<float3 , 4>  points_6;
        for(;;)
        {
            float2  _S735 = float2 {intrins_11.z, intrins_11.w};
            float2  _S736 = float2 {intrins_11.x, intrins_11.y};
            float2  uv_31 = (pix_center_5 + make_float2 (-1.0f, -0.0f) - _S735) / _S736;
            FixedArray<float, 4>  _S737 = dist_coeffs_14;
            float2  uv_u_14;
            bool _S738 = undistort_point_1(uv_31, &_S737, int(12), &uv_u_14);
            if(!_S738)
            {
                float3  _S739 = make_float3 (0.0f);
                _S734 = int(0);
                _S733 = nullptr;
                _S732 = nullptr;
                _S731 = nullptr;
                _S730 = nullptr;
                normal_7 = _S739;
                break;
            }
            points_6[int(0)] = make_float3 (depths_4.x) * unproject_raydir_0(uv_u_14, camera_model_13, is_ray_depth_12);
            for(;;)
            {
                float2  uv_32 = (pix_center_5 + make_float2 (1.0f, -0.0f) - _S735) / _S736;
                FixedArray<float, 4>  _S740 = dist_coeffs_14;
                float2  uv_u_15;
                bool _S741 = undistort_point_1(uv_32, &_S740, int(12), &uv_u_15);
                if(!_S741)
                {
                    float3  _S742 = make_float3 (0.0f);
                    _S734 = int(0);
                    _S733 = nullptr;
                    normal_7 = _S742;
                    break;
                }
                points_6[int(1)] = make_float3 (depths_4.y) * unproject_raydir_0(uv_u_15, camera_model_13, is_ray_depth_12);
                _S734 = int(2);
                _S733 = &points_6[int(1)];
                break;
            }
            if(_S734 != int(2))
            {
                _S732 = &points_6[int(0)];
                _S731 = nullptr;
                _S730 = nullptr;
                break;
            }
            float2  uv_33 = (pix_center_5 + make_float2 (0.0f, -1.0f) - _S735) / _S736;
            FixedArray<float, 4>  _S743 = dist_coeffs_14;
            float2  uv_u_16;
            bool _S744 = undistort_point_1(uv_33, &_S743, int(12), &uv_u_16);
            if(!_S744)
            {
                float3  _S745 = make_float3 (0.0f);
                _S734 = int(0);
                _S732 = &points_6[int(0)];
                _S731 = nullptr;
                _S730 = nullptr;
                normal_7 = _S745;
                break;
            }
            points_6[int(2)] = make_float3 (depths_4.z) * unproject_raydir_0(uv_u_16, camera_model_13, is_ray_depth_12);
            for(;;)
            {
                float2  uv_34 = (pix_center_5 + make_float2 (0.0f, 1.0f) - _S735) / _S736;
                FixedArray<float, 4>  _S746 = dist_coeffs_14;
                float2  uv_u_17;
                bool _S747 = undistort_point_1(uv_34, &_S746, int(12), &uv_u_17);
                if(!_S747)
                {
                    float3  _S748 = make_float3 (0.0f);
                    _S734 = int(0);
                    _S732 = nullptr;
                    normal_7 = _S748;
                    break;
                }
                points_6[int(3)] = make_float3 (depths_4.w) * unproject_raydir_0(uv_u_17, camera_model_13, is_ray_depth_12);
                _S734 = int(2);
                _S732 = &points_6[int(3)];
                break;
            }
            if(_S734 != int(2))
            {
                float3  * _S749 = _S732;
                _S732 = &points_6[int(0)];
                _S731 = _S749;
                _S730 = &points_6[int(2)];
                break;
            }
            float3  * _S750 = _S732;
            _S734 = int(1);
            _S732 = &points_6[int(0)];
            _S731 = _S750;
            _S730 = &points_6[int(2)];
            break;
        }
        if(_S734 != int(1))
        {
            break;
        }
        float3  normal_8 = cross_0(*_S733 - *_S732, - (*_S731 - *_S730));
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
    float2  _S751;
    bool _S752;
    float2  _S753;
    bool _S754;
    float2  _S755;
    bool _S756;
    float2  _S757;
    bool _S758;
};

inline __device__ void depth_to_normal_vjp_opencv(float2  pix_center_6, float4  intrins_12, FixedArray<float, 4>  dist_coeffs_15, int camera_model_14, bool is_ray_depth_13, float4  depths_5, float3  v_normal_2, float4  * v_depths_2)
{
    float2  _S759 = make_float2 (0.0f);
    s_bwd_prop_depth_to_normal_Intermediates_1 _S760;
    (&_S760)->_S751 = _S759;
    (&_S760)->_S752 = false;
    (&_S760)->_S753 = _S759;
    (&_S760)->_S754 = false;
    (&_S760)->_S755 = _S759;
    (&_S760)->_S756 = false;
    (&_S760)->_S757 = _S759;
    (&_S760)->_S758 = false;
    (&_S760)->_S751 = _S759;
    (&_S760)->_S752 = false;
    (&_S760)->_S753 = _S759;
    (&_S760)->_S754 = false;
    (&_S760)->_S755 = _S759;
    (&_S760)->_S756 = false;
    (&_S760)->_S757 = _S759;
    (&_S760)->_S758 = false;
    bool _S761 = (depths_5.x) == 0.0f;
    bool _runFlag_13;
    if(_S761)
    {
        _runFlag_13 = true;
    }
    else
    {
        _runFlag_13 = (depths_5.y) == 0.0f;
    }
    if(_runFlag_13)
    {
        _runFlag_13 = true;
    }
    else
    {
        _runFlag_13 = (depths_5.z) == 0.0f;
    }
    if(_runFlag_13)
    {
        _runFlag_13 = true;
    }
    else
    {
        _runFlag_13 = (depths_5.w) == 0.0f;
    }
    int _S762;
    if(!_runFlag_13)
    {
        float2  _S763 = float2 {intrins_12.z, intrins_12.w};
        float2  _S764 = float2 {intrins_12.x, intrins_12.y};
        float2  uv_35 = (pix_center_6 + make_float2 (-1.0f, -0.0f) - _S763) / _S764;
        float2  _S765 = _S759;
        FixedArray<float, 4>  _S766 = dist_coeffs_15;
        bool _S767 = undistort_point_1(uv_35, &_S766, int(12), &_S765);
        (&_S760)->_S751 = _S765;
        (&_S760)->_S752 = _S767;
        bool _S768 = !!_S767;
        if(_S768)
        {
            float2  uv_36 = (pix_center_6 + make_float2 (1.0f, -0.0f) - _S763) / _S764;
            float2  _S769 = _S759;
            FixedArray<float, 4>  _S770 = dist_coeffs_15;
            bool _S771 = undistort_point_1(uv_36, &_S770, int(12), &_S769);
            (&_S760)->_S753 = _S769;
            (&_S760)->_S754 = _S771;
            if(!!_S771)
            {
                _S762 = int(2);
            }
            else
            {
                _S762 = int(0);
            }
            if(_S762 != int(2))
            {
                _runFlag_13 = false;
            }
            else
            {
                _runFlag_13 = _S768;
            }
            if(_runFlag_13)
            {
                float2  uv_37 = (pix_center_6 + make_float2 (0.0f, -1.0f) - _S763) / _S764;
                float2  _S772 = _S759;
                FixedArray<float, 4>  _S773 = dist_coeffs_15;
                bool _S774 = undistort_point_1(uv_37, &_S773, int(12), &_S772);
                (&_S760)->_S755 = _S772;
                (&_S760)->_S756 = _S774;
                if(!_S774)
                {
                    _runFlag_13 = false;
                }
                if(_runFlag_13)
                {
                    float2  uv_38 = (pix_center_6 + make_float2 (0.0f, 1.0f) - _S763) / _S764;
                    float2  _S775 = _S759;
                    FixedArray<float, 4>  _S776 = dist_coeffs_15;
                    bool _S777 = undistort_point_1(uv_38, &_S776, int(12), &_S775);
                    (&_S760)->_S757 = _S775;
                    (&_S760)->_S758 = _S777;
                }
            }
        }
    }
    s_bwd_prop_depth_to_normal_Intermediates_1 _S778 = _S760;
    float3  _S779 = make_float3 (0.0f);
    if(_S761)
    {
        _runFlag_13 = true;
    }
    else
    {
        _runFlag_13 = (depths_5.y) == 0.0f;
    }
    if(_runFlag_13)
    {
        _runFlag_13 = true;
    }
    else
    {
        _runFlag_13 = (depths_5.z) == 0.0f;
    }
    if(_runFlag_13)
    {
        _runFlag_13 = true;
    }
    else
    {
        _runFlag_13 = (depths_5.w) == 0.0f;
    }
    bool _S780 = !_runFlag_13;
    bool _runFlag_14;
    bool _runFlag_15;
    bool _S781;
    bool _runFlag_16;
    bool _S782;
    bool _S783;
    FixedArray<float3 , 4>  points_7;
    float3  _S784;
    float3  _S785;
    float3  _S786;
    float3  _S787;
    float3  _S788;
    float3  _S789;
    float3  _S790;
    float3  _S791;
    float3  _S792;
    if(_S780)
    {
        bool _S793 = !!_S778._S752;
        if(_S793)
        {
            float3  _S794 = s_primal_ctx_unproject_raydir_0(_S778._S751, camera_model_14, is_ray_depth_13);
            float3  _S795 = make_float3 (depths_5.x) * _S794;
            bool _S796 = !!_S778._S754;
            if(_S796)
            {
                float3  _S797 = s_primal_ctx_unproject_raydir_0(_S778._S753, camera_model_14, is_ray_depth_13);
                float3  _S798 = make_float3 (depths_5.y) * _S797;
                _S762 = int(2);
                points_7[int(0)] = _S795;
                points_7[int(1)] = _S798;
                points_7[int(2)] = _S779;
                points_7[int(3)] = _S779;
                _S784 = _S797;
            }
            else
            {
                _S762 = int(0);
                points_7[int(0)] = _S795;
                points_7[int(1)] = _S779;
                points_7[int(2)] = _S779;
                points_7[int(3)] = _S779;
                _S784 = _S779;
            }
            if(_S762 != int(2))
            {
                _runFlag_13 = false;
            }
            else
            {
                _runFlag_13 = _S793;
                _S762 = int(0);
            }
            if(_runFlag_13)
            {
                if(!_S778._S756)
                {
                    _runFlag_14 = false;
                    _S762 = int(0);
                }
                else
                {
                    _runFlag_14 = _runFlag_13;
                }
                if(_runFlag_14)
                {
                    float3  _S799 = s_primal_ctx_unproject_raydir_0(_S778._S755, camera_model_14, is_ray_depth_13);
                    points_7[int(2)] = make_float3 (depths_5.z) * _S799;
                    bool _S800 = !!_S778._S758;
                    int _S801;
                    if(_S800)
                    {
                        float3  _S802 = s_primal_ctx_unproject_raydir_0(_S778._S757, camera_model_14, is_ray_depth_13);
                        points_7[int(3)] = make_float3 (depths_5.w) * _S802;
                        _S801 = int(2);
                        _S785 = _S802;
                    }
                    else
                    {
                        _S801 = int(0);
                        _S785 = _S779;
                    }
                    if(_S801 != int(2))
                    {
                        _runFlag_15 = false;
                        _S762 = _S801;
                    }
                    else
                    {
                        _runFlag_15 = _runFlag_14;
                    }
                    if(_runFlag_15)
                    {
                        _S762 = int(1);
                    }
                    _runFlag_15 = _S800;
                    _S786 = _S799;
                }
                else
                {
                    _runFlag_15 = false;
                    _S785 = _S779;
                    _S786 = _S779;
                }
            }
            else
            {
                _runFlag_14 = false;
                _runFlag_15 = false;
                _S785 = _S779;
                _S786 = _S779;
            }
            float3  _S803 = _S784;
            _S784 = _S785;
            _S785 = _S786;
            _S781 = _S796;
            _S786 = _S803;
            _S787 = _S794;
        }
        else
        {
            _S762 = int(0);
            points_7[int(0)] = _S779;
            points_7[int(1)] = _S779;
            points_7[int(2)] = _S779;
            points_7[int(3)] = _S779;
            _runFlag_13 = false;
            _runFlag_14 = false;
            _runFlag_15 = false;
            _S784 = _S779;
            _S785 = _S779;
            _S781 = false;
            _S786 = _S779;
            _S787 = _S779;
        }
        if(_S762 != int(1))
        {
            _runFlag_16 = false;
        }
        else
        {
            _runFlag_16 = _S780;
        }
        if(_runFlag_16)
        {
            float3  dx_3 = points_7[int(1)] - points_7[int(0)];
            float3  _S804 = - (points_7[int(3)] - points_7[int(2)]);
            float3  _S805 = s_primal_ctx_cross_0(dx_3, _S804);
            bool _S806 = (s_primal_ctx_dot_0(_S805, _S805)) != 0.0f;
            if(_S806)
            {
                float _S807 = length_0(_S805);
                float3  _S808 = make_float3 (_S807);
                _S788 = make_float3 (_S807 * _S807);
                _S789 = _S808;
            }
            else
            {
                _S788 = _S779;
                _S789 = _S779;
            }
            float3  _S809 = _S789;
            _S782 = _S806;
            _S789 = _S805;
            _S790 = _S809;
            _S791 = dx_3;
            _S792 = _S804;
        }
        else
        {
            _S782 = false;
            _S788 = _S779;
            _S789 = _S779;
            _S790 = _S779;
            _S791 = _S779;
            _S792 = _S779;
        }
        bool _S810 = _runFlag_13;
        bool _S811 = _runFlag_14;
        bool _S812 = _runFlag_15;
        float3  _S813 = _S784;
        float3  _S814 = _S785;
        bool _S815 = _S781;
        float3  _S816 = _S786;
        float3  _S817 = _S787;
        _runFlag_13 = _runFlag_16;
        _runFlag_14 = _S782;
        _S784 = _S788;
        _S785 = _S789;
        _S786 = _S790;
        _S787 = _S791;
        _S788 = _S792;
        _runFlag_15 = _S793;
        _S781 = _S810;
        _runFlag_16 = _S811;
        _S782 = _S812;
        _S789 = _S813;
        _S790 = _S814;
        _S783 = _S815;
        _S791 = _S816;
        _S792 = _S817;
    }
    else
    {
        _runFlag_13 = false;
        _runFlag_14 = false;
        _S784 = _S779;
        _S785 = _S779;
        _S786 = _S779;
        _S787 = _S779;
        _S788 = _S779;
        _runFlag_15 = false;
        _S781 = false;
        _runFlag_16 = false;
        _S782 = false;
        _S789 = _S779;
        _S790 = _S779;
        _S783 = false;
        _S791 = _S779;
        _S792 = _S779;
    }
    float4  _S818 = make_float4 (0.0f);
    float4  _S819;
    if(_S780)
    {
        if(_runFlag_13)
        {
            if(_runFlag_14)
            {
                float3  _S820 = v_normal_2 / _S784;
                float3  _S821 = _S785 * - _S820;
                float3  _S822 = _S786 * _S820;
                float _S823 = _S821.x + _S821.y + _S821.z;
                DiffPair_vectorx3Cfloatx2C3x3E_0 _S824;
                (&_S824)->primal_0 = _S785;
                (&_S824)->differential_0 = _S779;
                s_bwd_length_impl_0(&_S824, _S823);
                _S784 = _S822 + _S824.differential_0;
            }
            else
            {
                _S784 = v_normal_2;
            }
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S825;
            (&_S825)->primal_0 = _S785;
            (&_S825)->differential_0 = _S779;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S826;
            (&_S826)->primal_0 = _S785;
            (&_S826)->differential_0 = _S779;
            s_bwd_prop_dot_0(&_S825, &_S826, 0.0f);
            float3  _S827 = _S826.differential_0 + _S825.differential_0 + _S784;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S828;
            (&_S828)->primal_0 = _S787;
            (&_S828)->differential_0 = _S779;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S829;
            (&_S829)->primal_0 = _S788;
            (&_S829)->differential_0 = _S779;
            s_bwd_prop_cross_0(&_S828, &_S829, _S827);
            float3  s_diff_dy_T_3 = - _S829.differential_0;
            float3  _S830 = - s_diff_dy_T_3;
            float3  _S831 = - _S828.differential_0;
            FixedArray<float3 , 4>  _S832;
            _S832[int(0)] = _S779;
            _S832[int(1)] = _S779;
            _S832[int(2)] = _S779;
            _S832[int(3)] = _S779;
            _S832[int(2)] = _S830;
            _S832[int(3)] = s_diff_dy_T_3;
            _S832[int(0)] = _S831;
            _S832[int(1)] = _S828.differential_0;
            points_7[int(0)] = _S832[int(0)];
            points_7[int(1)] = _S832[int(1)];
            points_7[int(2)] = _S832[int(2)];
            points_7[int(3)] = _S832[int(3)];
        }
        else
        {
            points_7[int(0)] = _S779;
            points_7[int(1)] = _S779;
            points_7[int(2)] = _S779;
            points_7[int(3)] = _S779;
        }
        if(_runFlag_15)
        {
            if(_S781)
            {
                if(_runFlag_16)
                {
                    FixedArray<float3 , 4>  _S833 = points_7;
                    FixedArray<float3 , 4>  _S834 = points_7;
                    FixedArray<float3 , 4>  _S835 = points_7;
                    FixedArray<float3 , 4>  _S836 = points_7;
                    if(_S782)
                    {
                        float3  _S837 = _S789 * _S836[int(3)];
                        float _S838 = _S837.x + _S837.y + _S837.z;
                        float4  _S839 = _S818;
                        *&((&_S839)->w) = _S838;
                        points_7[int(0)] = _S833[int(0)];
                        points_7[int(1)] = _S834[int(1)];
                        points_7[int(2)] = _S835[int(2)];
                        points_7[int(3)] = _S779;
                        _S819 = _S839;
                    }
                    else
                    {
                        points_7[int(0)] = _S833[int(0)];
                        points_7[int(1)] = _S834[int(1)];
                        points_7[int(2)] = _S835[int(2)];
                        points_7[int(3)] = _S836[int(3)];
                        _S819 = _S818;
                    }
                    float3  _S840 = _S790 * points_7[int(2)];
                    float _S841 = _S840.x + _S840.y + _S840.z;
                    FixedArray<float3 , 4>  _S842 = points_7;
                    FixedArray<float3 , 4>  _S843 = points_7;
                    float4  _S844 = _S818;
                    *&((&_S844)->z) = _S841;
                    float4  _S845 = _S819 + _S844;
                    points_7[int(0)] = points_7[int(0)];
                    points_7[int(1)] = _S842[int(1)];
                    points_7[int(2)] = _S779;
                    points_7[int(3)] = _S843[int(3)];
                    _S819 = _S845;
                }
                else
                {
                    FixedArray<float3 , 4>  _S846 = points_7;
                    FixedArray<float3 , 4>  _S847 = points_7;
                    FixedArray<float3 , 4>  _S848 = points_7;
                    points_7[int(0)] = points_7[int(0)];
                    points_7[int(1)] = _S846[int(1)];
                    points_7[int(2)] = _S847[int(2)];
                    points_7[int(3)] = _S848[int(3)];
                    _S819 = _S818;
                }
            }
            else
            {
                FixedArray<float3 , 4>  _S849 = points_7;
                FixedArray<float3 , 4>  _S850 = points_7;
                FixedArray<float3 , 4>  _S851 = points_7;
                points_7[int(0)] = points_7[int(0)];
                points_7[int(1)] = _S849[int(1)];
                points_7[int(2)] = _S850[int(2)];
                points_7[int(3)] = _S851[int(3)];
                _S819 = _S818;
            }
            if(_S783)
            {
                FixedArray<float3 , 4>  _S852 = points_7;
                float3  _S853 = _S791 * points_7[int(1)];
                float _S854 = _S853.x + _S853.y + _S853.z;
                float4  _S855 = _S818;
                *&((&_S855)->y) = _S854;
                float4  _S856 = _S819 + _S855;
                points_7[int(0)] = _S779;
                points_7[int(1)] = _S779;
                points_7[int(2)] = _S779;
                points_7[int(3)] = _S779;
                _S784 = _S852[int(0)];
                _S819 = _S856;
            }
            else
            {
                FixedArray<float3 , 4>  _S857 = points_7;
                FixedArray<float3 , 4>  _S858 = points_7;
                FixedArray<float3 , 4>  _S859 = points_7;
                points_7[int(0)] = points_7[int(0)];
                points_7[int(1)] = _S857[int(1)];
                points_7[int(2)] = _S858[int(2)];
                points_7[int(3)] = _S859[int(3)];
                _S784 = _S779;
            }
            float3  _S860 = _S792 * (points_7[int(0)] + _S784);
            float _S861 = _S860.x + _S860.y + _S860.z;
            float4  _S862 = _S818;
            *&((&_S862)->x) = _S861;
            _S819 = _S819 + _S862;
        }
        else
        {
            _S819 = _S818;
        }
    }
    else
    {
        _S819 = _S818;
    }
    *v_depths_2 = _S819;
    return;
}

inline __device__ float ray_depth_to_linear_depth_factor_opencv(float2  pix_center_7, float4  intrins_13, FixedArray<float, 4>  dist_coeffs_16, int camera_model_15)
{
    float _S863;
    for(;;)
    {
        float2  uv_39 = (pix_center_7 - float2 {intrins_13.z, intrins_13.w}) / float2 {intrins_13.x, intrins_13.y};
        FixedArray<float, 4>  _S864 = dist_coeffs_16;
        float2  uv_u_18;
        bool _S865 = undistort_point_1(uv_39, &_S864, int(12), &uv_u_18);
        if(!_S865)
        {
            _S863 = 0.0f;
            break;
        }
        float3  raydir_9 = unproject_raydir_0(uv_u_18, camera_model_15, false);
        _S863 = float((F32_sign((raydir_9.z)))) / length_0(raydir_9);
        break;
    }
    return _S863;
}

inline __device__ float depth_normal_loss_opencv(float2  pix_center_8, float4  intrins_14, FixedArray<float, 4>  dist_coeffs_17, int camera_model_16, bool is_ray_depth_14, float4  depths_6, float3  gt_normal_2)
{
    float _S866;
    for(;;)
    {
        float3  _S867;
        float3  * _S868;
        float3  * _S869;
        float3  * _S870;
        float3  * _S871;
        int _S872;
        FixedArray<float3 , 5>  points_8;
        for(;;)
        {
            float2  _S873 = float2 {intrins_14.z, intrins_14.w};
            float2  _S874 = float2 {intrins_14.x, intrins_14.y};
            float2  uv_40 = (pix_center_8 + make_float2 (-1.0f, -0.0f) - _S873) / _S874;
            FixedArray<float, 4>  _S875 = dist_coeffs_17;
            float2  uv_u_19;
            bool _S876 = undistort_point_1(uv_40, &_S875, int(12), &uv_u_19);
            float3  _S877 = make_float3 (0.0f);
            if(!_S876)
            {
                _S872 = int(0);
                _S871 = nullptr;
                _S870 = nullptr;
                _S869 = nullptr;
                _S868 = nullptr;
                _S867 = _S877;
                break;
            }
            float3  raydir_10 = unproject_raydir_0(uv_u_19, camera_model_16, is_ray_depth_14);
            points_8[int(0)] = make_float3 (depths_6.x) * raydir_10;
            float2  uv_41 = (pix_center_8 + make_float2 (1.0f, -0.0f) - _S873) / _S874;
            FixedArray<float, 4>  _S878 = dist_coeffs_17;
            float2  uv_u_20;
            bool _S879 = undistort_point_1(uv_41, &_S878, int(12), &uv_u_20);
            if(!_S879)
            {
                _S872 = int(0);
                _S871 = nullptr;
                _S870 = &points_8[int(0)];
                _S869 = nullptr;
                _S868 = nullptr;
                _S867 = _S877;
                break;
            }
            float3  raydir_11 = unproject_raydir_0(uv_u_20, camera_model_16, is_ray_depth_14);
            points_8[int(1)] = make_float3 (depths_6.y) * raydir_11;
            float2  uv_42 = (pix_center_8 + make_float2 (0.0f, -1.0f) - _S873) / _S874;
            FixedArray<float, 4>  _S880 = dist_coeffs_17;
            float2  uv_u_21;
            bool _S881 = undistort_point_1(uv_42, &_S880, int(12), &uv_u_21);
            if(!_S881)
            {
                _S872 = int(0);
                _S871 = &points_8[int(1)];
                _S870 = &points_8[int(0)];
                _S869 = nullptr;
                _S868 = nullptr;
                _S867 = _S877;
                break;
            }
            float3  raydir_12 = unproject_raydir_0(uv_u_21, camera_model_16, is_ray_depth_14);
            points_8[int(2)] = make_float3 (depths_6.z) * raydir_12;
            float2  uv_43 = (pix_center_8 + make_float2 (0.0f, 1.0f) - _S873) / _S874;
            FixedArray<float, 4>  _S882 = dist_coeffs_17;
            float2  uv_u_22;
            bool _S883 = undistort_point_1(uv_43, &_S882, int(12), &uv_u_22);
            if(!_S883)
            {
                _S872 = int(0);
                _S871 = &points_8[int(1)];
                _S870 = &points_8[int(0)];
                _S869 = nullptr;
                _S868 = &points_8[int(2)];
                _S867 = _S877;
                break;
            }
            float3  raydir_13 = unproject_raydir_0(uv_u_22, camera_model_16, is_ray_depth_14);
            points_8[int(3)] = make_float3 (depths_6.w) * raydir_13;
            float2  uv_44 = (pix_center_8 + make_float2 (0.0f) * make_float2 (0.0f, 3.0f) - _S873) / _S874;
            FixedArray<float, 4>  _S884 = dist_coeffs_17;
            float2  uv_u_23;
            bool _S885 = undistort_point_1(uv_44, &_S884, int(12), &uv_u_23);
            if(!_S885)
            {
                _S872 = int(0);
                _S871 = &points_8[int(1)];
                _S870 = &points_8[int(0)];
                _S869 = &points_8[int(3)];
                _S868 = &points_8[int(2)];
                _S867 = _S877;
                break;
            }
            float3  raydir_14 = unproject_raydir_0(uv_u_23, camera_model_16, is_ray_depth_14);
            _S872 = int(1);
            _S871 = &points_8[int(1)];
            _S870 = &points_8[int(0)];
            _S869 = &points_8[int(3)];
            _S868 = &points_8[int(2)];
            _S867 = raydir_14;
            break;
        }
        if(_S872 != int(1))
        {
            _S866 = 0.0f;
            break;
        }
        float3  normal_9 = cross_0(*_S871 - *_S870, - (*_S869 - *_S868));
        float3  normal_10;
        if((dot_0(normal_9, normal_9)) != 0.0f)
        {
            normal_10 = normalize_0(normal_9);
        }
        else
        {
            normal_10 = normal_9;
        }
        float3  _S886;
        if((dot_0(gt_normal_2, gt_normal_2)) != 0.0f)
        {
            _S886 = normalize_0(gt_normal_2);
        }
        else
        {
            _S886 = gt_normal_2;
        }
        _S866 = (1.0f - dot_0(normal_10, _S886) + 0.00100000004749745f) / ((F32_max((dot_0(normal_10, - normalize_0(_S867))), (0.0f))) + 0.00100000004749745f);
        break;
    }
    return _S866;
}

struct s_bwd_prop_depth_normal_loss_Intermediates_1
{
    float2  _S887;
    bool _S888;
    float2  _S889;
    bool _S890;
    float2  _S891;
    bool _S892;
    float2  _S893;
    bool _S894;
    float2  _S895;
    bool _S896;
};

inline __device__ void depth_normal_loss_vjp_opencv(float2  pix_center_9, float4  intrins_15, FixedArray<float, 4>  dist_coeffs_18, int camera_model_17, bool is_ray_depth_15, float4  depths_7, float3  gt_normal_3, float v_loss_1, float4  * v_depths_3, float3  * v_gt_normal_1)
{
    float2  _S897 = make_float2 (0.0f);
    s_bwd_prop_depth_normal_loss_Intermediates_1 _S898;
    (&_S898)->_S887 = _S897;
    (&_S898)->_S888 = false;
    (&_S898)->_S889 = _S897;
    (&_S898)->_S890 = false;
    (&_S898)->_S891 = _S897;
    (&_S898)->_S892 = false;
    (&_S898)->_S893 = _S897;
    (&_S898)->_S894 = false;
    (&_S898)->_S895 = _S897;
    (&_S898)->_S896 = false;
    (&_S898)->_S889 = _S897;
    (&_S898)->_S890 = false;
    (&_S898)->_S891 = _S897;
    (&_S898)->_S892 = false;
    (&_S898)->_S893 = _S897;
    (&_S898)->_S894 = false;
    (&_S898)->_S895 = _S897;
    (&_S898)->_S896 = false;
    float2  _S899 = float2 {intrins_15.z, intrins_15.w};
    float2  _S900 = float2 {intrins_15.x, intrins_15.y};
    float2  uv_45 = (pix_center_9 + make_float2 (-1.0f, -0.0f) - _S899) / _S900;
    float2  _S901 = _S897;
    FixedArray<float, 4>  _S902 = dist_coeffs_18;
    bool _S903 = undistort_point_1(uv_45, &_S902, int(12), &_S901);
    (&_S898)->_S887 = _S901;
    (&_S898)->_S888 = _S903;
    bool _S904 = !!_S903;
    bool _runFlag_17;
    if(_S904)
    {
        float2  uv_46 = (pix_center_9 + make_float2 (1.0f, -0.0f) - _S899) / _S900;
        float2  _S905 = _S897;
        FixedArray<float, 4>  _S906 = dist_coeffs_18;
        bool _S907 = undistort_point_1(uv_46, &_S906, int(12), &_S905);
        (&_S898)->_S889 = _S905;
        (&_S898)->_S890 = _S907;
        if(!_S907)
        {
            _runFlag_17 = false;
        }
        else
        {
            _runFlag_17 = _S904;
        }
        if(_runFlag_17)
        {
            float2  uv_47 = (pix_center_9 + make_float2 (0.0f, -1.0f) - _S899) / _S900;
            float2  _S908 = _S897;
            FixedArray<float, 4>  _S909 = dist_coeffs_18;
            bool _S910 = undistort_point_1(uv_47, &_S909, int(12), &_S908);
            (&_S898)->_S891 = _S908;
            (&_S898)->_S892 = _S910;
            if(!_S910)
            {
                _runFlag_17 = false;
            }
            if(_runFlag_17)
            {
                float2  uv_48 = (pix_center_9 + make_float2 (0.0f, 1.0f) - _S899) / _S900;
                float2  _S911 = _S897;
                FixedArray<float, 4>  _S912 = dist_coeffs_18;
                bool _S913 = undistort_point_1(uv_48, &_S912, int(12), &_S911);
                (&_S898)->_S893 = _S911;
                (&_S898)->_S894 = _S913;
                if(!_S913)
                {
                    _runFlag_17 = false;
                }
                if(_runFlag_17)
                {
                    float2  uv_49 = (pix_center_9 - _S899) / _S900;
                    float2  _S914 = _S897;
                    FixedArray<float, 4>  _S915 = dist_coeffs_18;
                    bool _S916 = undistort_point_1(uv_49, &_S915, int(12), &_S914);
                    (&_S898)->_S895 = _S914;
                    (&_S898)->_S896 = _S916;
                }
            }
        }
    }
    s_bwd_prop_depth_normal_loss_Intermediates_1 _S917 = _S898;
    float3  _S918 = make_float3 (0.0f);
    bool _S919 = !!_S898._S888;
    bool _runFlag_18;
    bool _runFlag_19;
    bool _runFlag_20;
    int _S920;
    float3  raydir_15;
    float3  _S921;
    float3  _S922;
    float3  _S923;
    float3  _S924;
    FixedArray<float3 , 5>  points_9;
    if(_S919)
    {
        float3  _S925 = s_primal_ctx_unproject_raydir_0(_S917._S887, camera_model_17, is_ray_depth_15);
        float3  _S926 = make_float3 (depths_7.x) * _S925;
        if(!_S917._S890)
        {
            _runFlag_17 = false;
        }
        else
        {
            _runFlag_17 = _S919;
        }
        if(_runFlag_17)
        {
            float3  _S927 = s_primal_ctx_unproject_raydir_0(_S917._S889, camera_model_17, is_ray_depth_15);
            float3  _S928 = make_float3 (depths_7.y) * _S927;
            if(!_S917._S892)
            {
                _runFlag_18 = false;
            }
            else
            {
                _runFlag_18 = _runFlag_17;
            }
            if(_runFlag_18)
            {
                float3  _S929 = s_primal_ctx_unproject_raydir_0(_S917._S891, camera_model_17, is_ray_depth_15);
                float3  _S930 = make_float3 (depths_7.z) * _S929;
                if(!_S917._S894)
                {
                    _runFlag_19 = false;
                }
                else
                {
                    _runFlag_19 = _runFlag_18;
                }
                if(_runFlag_19)
                {
                    float3  _S931 = s_primal_ctx_unproject_raydir_0(_S917._S893, camera_model_17, is_ray_depth_15);
                    float3  _S932 = make_float3 (depths_7.w) * _S931;
                    if(!_S917._S896)
                    {
                        _runFlag_20 = false;
                    }
                    else
                    {
                        _runFlag_20 = _runFlag_19;
                    }
                    if(_runFlag_20)
                    {
                        float3  _S933 = s_primal_ctx_unproject_raydir_0(_S917._S895, camera_model_17, is_ray_depth_15);
                        _S920 = int(1);
                        raydir_15 = _S933;
                    }
                    else
                    {
                        _S920 = int(0);
                        raydir_15 = _S931;
                    }
                    points_9[int(0)] = _S926;
                    points_9[int(1)] = _S928;
                    points_9[int(2)] = _S930;
                    points_9[int(3)] = _S932;
                    points_9[int(4)] = _S918;
                    _S921 = _S931;
                }
                else
                {
                    _S920 = int(0);
                    raydir_15 = _S929;
                    points_9[int(0)] = _S926;
                    points_9[int(1)] = _S928;
                    points_9[int(2)] = _S930;
                    points_9[int(3)] = _S918;
                    points_9[int(4)] = _S918;
                    _S921 = _S918;
                }
                _S922 = _S929;
            }
            else
            {
                _S920 = int(0);
                raydir_15 = _S927;
                points_9[int(0)] = _S926;
                points_9[int(1)] = _S928;
                points_9[int(2)] = _S918;
                points_9[int(3)] = _S918;
                points_9[int(4)] = _S918;
                _runFlag_19 = false;
                _S921 = _S918;
                _S922 = _S918;
            }
            _S923 = _S927;
        }
        else
        {
            _S920 = int(0);
            raydir_15 = _S925;
            points_9[int(0)] = _S926;
            points_9[int(1)] = _S918;
            points_9[int(2)] = _S918;
            points_9[int(3)] = _S918;
            points_9[int(4)] = _S918;
            _runFlag_18 = false;
            _runFlag_19 = false;
            _S921 = _S918;
            _S922 = _S918;
            _S923 = _S918;
        }
        _S924 = _S925;
    }
    else
    {
        _S920 = int(0);
        points_9[int(0)] = _S918;
        points_9[int(1)] = _S918;
        points_9[int(2)] = _S918;
        points_9[int(3)] = _S918;
        points_9[int(4)] = _S918;
        _runFlag_17 = false;
        _runFlag_18 = false;
        _runFlag_19 = false;
        _S921 = _S918;
        _S922 = _S918;
        _S923 = _S918;
        _S924 = _S918;
    }
    bool _S934 = !(_S920 != int(1));
    bool _S935;
    float3  normal_11;
    float3  _S936;
    float3  _S937;
    float3  _S938;
    float3  _S939;
    float _S940;
    float _S941;
    float _S942;
    float _S943;
    if(_S934)
    {
        float3  dx_4 = points_9[int(1)] - points_9[int(0)];
        float3  _S944 = - (points_9[int(3)] - points_9[int(2)]);
        float3  _S945 = s_primal_ctx_cross_0(dx_4, _S944);
        bool _S946 = (s_primal_ctx_dot_0(_S945, _S945)) != 0.0f;
        if(_S946)
        {
            normal_11 = normalize_0(_S945);
        }
        else
        {
            normal_11 = _S945;
        }
        bool _S947 = (s_primal_ctx_dot_0(gt_normal_3, gt_normal_3)) != 0.0f;
        if(_S947)
        {
            _S936 = normalize_0(gt_normal_3);
        }
        else
        {
            _S936 = gt_normal_3;
        }
        float3  _S948 = - normalize_0(raydir_15);
        float _S949 = s_primal_ctx_dot_0(normal_11, _S948);
        float _S950 = 1.0f - s_primal_ctx_dot_0(normal_11, _S936) + 0.00100000004749745f;
        float _S951 = (F32_max((_S949), (0.0f))) + 0.00100000004749745f;
        _S940 = _S951 * _S951;
        _S941 = _S950;
        _S942 = _S951;
        _S943 = _S949;
        raydir_15 = normal_11;
        normal_11 = _S948;
        _runFlag_20 = _S947;
        _S935 = _S946;
        _S937 = _S945;
        _S938 = dx_4;
        _S939 = _S944;
    }
    else
    {
        _S940 = 0.0f;
        _S941 = 0.0f;
        _S942 = 0.0f;
        _S943 = 0.0f;
        raydir_15 = _S918;
        normal_11 = _S918;
        _S936 = _S918;
        _runFlag_20 = false;
        _S935 = false;
        _S937 = _S918;
        _S938 = _S918;
        _S939 = _S918;
    }
    float4  _S952 = make_float4 (0.0f);
    if(_S934)
    {
        float _S953 = v_loss_1 / _S940;
        float _S954 = _S941 * - _S953;
        float s_diff_num_T_1 = _S942 * _S953;
        DiffPair_float_0 _S955;
        (&_S955)->primal_0 = _S943;
        (&_S955)->differential_0 = 0.0f;
        DiffPair_float_0 _S956;
        (&_S956)->primal_0 = 0.0f;
        (&_S956)->differential_0 = 0.0f;
        _d_max_0(&_S955, &_S956, _S954);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S957;
        (&_S957)->primal_0 = raydir_15;
        (&_S957)->differential_0 = _S918;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S958;
        (&_S958)->primal_0 = normal_11;
        (&_S958)->differential_0 = _S918;
        s_bwd_prop_dot_0(&_S957, &_S958, _S955.differential_0);
        float _S959 = - s_diff_num_T_1;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S960;
        (&_S960)->primal_0 = raydir_15;
        (&_S960)->differential_0 = _S918;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S961;
        (&_S961)->primal_0 = _S936;
        (&_S961)->differential_0 = _S918;
        s_bwd_prop_dot_0(&_S960, &_S961, _S959);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S962 = _S961;
        float3  _S963 = _S957.differential_0 + _S960.differential_0;
        if(_runFlag_20)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S964;
            (&_S964)->primal_0 = gt_normal_3;
            (&_S964)->differential_0 = _S918;
            s_bwd_normalize_impl_0(&_S964, _S962.differential_0);
            raydir_15 = _S964.differential_0;
        }
        else
        {
            raydir_15 = _S962.differential_0;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S965;
        (&_S965)->primal_0 = gt_normal_3;
        (&_S965)->differential_0 = _S918;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S966;
        (&_S966)->primal_0 = gt_normal_3;
        (&_S966)->differential_0 = _S918;
        s_bwd_prop_dot_0(&_S965, &_S966, 0.0f);
        float3  _S967 = _S966.differential_0 + _S965.differential_0 + raydir_15;
        if(_S935)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S968;
            (&_S968)->primal_0 = _S937;
            (&_S968)->differential_0 = _S918;
            s_bwd_normalize_impl_0(&_S968, _S963);
            raydir_15 = _S968.differential_0;
        }
        else
        {
            raydir_15 = _S963;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S969;
        (&_S969)->primal_0 = _S937;
        (&_S969)->differential_0 = _S918;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S970;
        (&_S970)->primal_0 = _S937;
        (&_S970)->differential_0 = _S918;
        s_bwd_prop_dot_0(&_S969, &_S970, 0.0f);
        float3  _S971 = _S970.differential_0 + _S969.differential_0 + raydir_15;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S972;
        (&_S972)->primal_0 = _S938;
        (&_S972)->differential_0 = _S918;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S973;
        (&_S973)->primal_0 = _S939;
        (&_S973)->differential_0 = _S918;
        s_bwd_prop_cross_0(&_S972, &_S973, _S971);
        float3  s_diff_dy_T_4 = - _S973.differential_0;
        float3  _S974 = - s_diff_dy_T_4;
        float3  _S975 = - _S972.differential_0;
        FixedArray<float3 , 5>  _S976;
        _S976[int(0)] = _S918;
        _S976[int(1)] = _S918;
        _S976[int(2)] = _S918;
        _S976[int(3)] = _S918;
        _S976[int(4)] = _S918;
        _S976[int(2)] = _S974;
        _S976[int(3)] = s_diff_dy_T_4;
        _S976[int(0)] = _S975;
        _S976[int(1)] = _S972.differential_0;
        points_9[int(0)] = _S976[int(0)];
        points_9[int(1)] = _S976[int(1)];
        points_9[int(2)] = _S976[int(2)];
        points_9[int(3)] = _S976[int(3)];
        points_9[int(4)] = _S976[int(4)];
        raydir_15 = _S967;
    }
    else
    {
        points_9[int(0)] = _S918;
        points_9[int(1)] = _S918;
        points_9[int(2)] = _S918;
        points_9[int(3)] = _S918;
        points_9[int(4)] = _S918;
        raydir_15 = _S918;
    }
    float4  _S977;
    if(_S919)
    {
        if(_runFlag_17)
        {
            if(_runFlag_18)
            {
                if(_runFlag_19)
                {
                    FixedArray<float3 , 5>  _S978 = points_9;
                    FixedArray<float3 , 5>  _S979 = points_9;
                    FixedArray<float3 , 5>  _S980 = points_9;
                    float3  _S981 = _S921 * points_9[int(3)];
                    float _S982 = _S981.x + _S981.y + _S981.z;
                    float4  _S983 = _S952;
                    *&((&_S983)->w) = _S982;
                    points_9[int(0)] = _S918;
                    points_9[int(1)] = _S918;
                    points_9[int(2)] = _S918;
                    points_9[int(3)] = _S918;
                    points_9[int(4)] = _S918;
                    _S921 = _S980[int(2)];
                    normal_11 = _S978[int(0)];
                    _S936 = _S979[int(1)];
                    _S977 = _S983;
                }
                else
                {
                    FixedArray<float3 , 5>  _S984 = points_9;
                    FixedArray<float3 , 5>  _S985 = points_9;
                    FixedArray<float3 , 5>  _S986 = points_9;
                    FixedArray<float3 , 5>  _S987 = points_9;
                    points_9[int(0)] = points_9[int(0)];
                    points_9[int(1)] = _S984[int(1)];
                    points_9[int(2)] = _S985[int(2)];
                    points_9[int(3)] = _S986[int(3)];
                    points_9[int(4)] = _S987[int(4)];
                    _S921 = _S918;
                    normal_11 = _S918;
                    _S936 = _S918;
                    _S977 = _S952;
                }
                float3  _S988 = _S922 * (points_9[int(2)] + _S921);
                float _S989 = _S988.x + _S988.y + _S988.z;
                float3  _S990 = points_9[int(0)] + normal_11;
                float3  _S991 = points_9[int(1)] + _S936;
                float4  _S992 = _S952;
                *&((&_S992)->z) = _S989;
                float4  _S993 = _S977 + _S992;
                points_9[int(0)] = _S918;
                points_9[int(1)] = _S918;
                points_9[int(2)] = _S918;
                points_9[int(3)] = _S918;
                points_9[int(4)] = _S918;
                _S921 = _S991;
                _S922 = _S990;
                _S977 = _S993;
            }
            else
            {
                FixedArray<float3 , 5>  _S994 = points_9;
                FixedArray<float3 , 5>  _S995 = points_9;
                FixedArray<float3 , 5>  _S996 = points_9;
                FixedArray<float3 , 5>  _S997 = points_9;
                points_9[int(0)] = points_9[int(0)];
                points_9[int(1)] = _S994[int(1)];
                points_9[int(2)] = _S995[int(2)];
                points_9[int(3)] = _S996[int(3)];
                points_9[int(4)] = _S997[int(4)];
                _S921 = _S918;
                _S922 = _S918;
                _S977 = _S952;
            }
            float3  _S998 = _S923 * (points_9[int(1)] + _S921);
            float _S999 = _S998.x + _S998.y + _S998.z;
            float3  _S1000 = points_9[int(0)] + _S922;
            float4  _S1001 = _S952;
            *&((&_S1001)->y) = _S999;
            float4  _S1002 = _S977 + _S1001;
            points_9[int(0)] = _S918;
            points_9[int(1)] = _S918;
            points_9[int(2)] = _S918;
            points_9[int(3)] = _S918;
            points_9[int(4)] = _S918;
            _S921 = _S1000;
            _S977 = _S1002;
        }
        else
        {
            FixedArray<float3 , 5>  _S1003 = points_9;
            FixedArray<float3 , 5>  _S1004 = points_9;
            FixedArray<float3 , 5>  _S1005 = points_9;
            FixedArray<float3 , 5>  _S1006 = points_9;
            points_9[int(0)] = points_9[int(0)];
            points_9[int(1)] = _S1003[int(1)];
            points_9[int(2)] = _S1004[int(2)];
            points_9[int(3)] = _S1005[int(3)];
            points_9[int(4)] = _S1006[int(4)];
            _S921 = _S918;
            _S977 = _S952;
        }
        float3  _S1007 = _S924 * (points_9[int(0)] + _S921);
        float _S1008 = _S1007.x + _S1007.y + _S1007.z;
        float4  _S1009 = _S952;
        *&((&_S1009)->x) = _S1008;
        _S977 = _S977 + _S1009;
    }
    else
    {
        _S977 = _S952;
    }
    *v_depths_3 = _S977;
    *v_gt_normal_1 = raydir_15;
    return;
}

inline __device__ float3  generate_ray_d2n_prism(float2  pix_pos_6, float4  intrins_16, FixedArray<float, 8>  dist_coeffs_19, int camera_model_18, bool is_ray_depth_16)
{
    float3  _S1010;
    for(;;)
    {
        float2  uv_50 = (pix_pos_6 - float2 {intrins_16.z, intrins_16.w}) / float2 {intrins_16.x, intrins_16.y};
        FixedArray<float, 8>  _S1011 = dist_coeffs_19;
        float2  uv_u_24;
        bool _S1012 = undistort_point_2(uv_50, &_S1011, int(12), &uv_u_24);
        if(!_S1012)
        {
            int3  _S1013 = make_int3 (int(0));
            float3  _S1014 = make_float3 ((float)_S1013.x, (float)_S1013.y, (float)_S1013.z);
            _S1010 = _S1014;
            break;
        }
        _S1010 = unproject_raydir_0(uv_u_24, camera_model_18, is_ray_depth_16);
        break;
    }
    return _S1010;
}

inline __device__ float3  depth_to_point_prism(float2  pix_pos_7, float4  intrins_17, FixedArray<float, 8>  dist_coeffs_20, int camera_model_19, bool is_ray_depth_17, float depth_6)
{
    float3  _S1015;
    for(;;)
    {
        float2  uv_51 = (pix_pos_7 - float2 {intrins_17.z, intrins_17.w}) / float2 {intrins_17.x, intrins_17.y};
        FixedArray<float, 8>  _S1016 = dist_coeffs_20;
        float2  uv_u_25;
        bool _S1017 = undistort_point_2(uv_51, &_S1016, int(12), &uv_u_25);
        if(!_S1017)
        {
            _S1015 = make_float3 (0.0f);
            break;
        }
        if(camera_model_19 == int(4))
        {
            _S1015 = make_float3 (uv_51.x, uv_51.y, depth_6);
            break;
        }
        _S1015 = make_float3 (depth_6) * unproject_raydir_0(uv_u_25, camera_model_19, is_ray_depth_17);
        break;
    }
    return _S1015;
}

struct s_bwd_prop_depth_to_point_Intermediates_2
{
    float2  _S1018;
    bool _S1019;
};

inline __device__ float depth_to_point_vjp_prism(float2  pix_pos_8, float4  intrins_18, FixedArray<float, 8>  dist_coeffs_21, int camera_model_20, bool is_ray_depth_18, float depth_7, float3  v_point_2)
{
    float2  _S1020 = make_float2 (0.0f);
    s_bwd_prop_depth_to_point_Intermediates_2 _S1021;
    (&_S1021)->_S1018 = _S1020;
    (&_S1021)->_S1019 = false;
    float2  uv_52 = (pix_pos_8 - float2 {intrins_18.z, intrins_18.w}) / float2 {intrins_18.x, intrins_18.y};
    float2  _S1022 = _S1020;
    FixedArray<float, 8>  _S1023 = dist_coeffs_21;
    bool _S1024 = undistort_point_2(uv_52, &_S1023, int(12), &_S1022);
    (&_S1021)->_S1018 = _S1022;
    (&_S1021)->_S1019 = _S1024;
    s_bwd_prop_depth_to_point_Intermediates_2 _S1025 = _S1021;
    float3  _S1026 = make_float3 (0.0f);
    bool _S1027 = !!_S1021._S1019;
    bool _runFlag_21;
    bool _S1028;
    float3  _S1029;
    if(_S1027)
    {
        bool _S1030 = camera_model_20 == int(4);
        if(_S1030)
        {
            _runFlag_21 = false;
        }
        else
        {
            _runFlag_21 = _S1027;
        }
        if(_runFlag_21)
        {
            _S1029 = s_primal_ctx_unproject_raydir_0(_S1025._S1018, camera_model_20, is_ray_depth_18);
        }
        else
        {
            _S1029 = _S1026;
        }
        _S1028 = _S1030;
    }
    else
    {
        _runFlag_21 = false;
        _S1029 = _S1026;
        _S1028 = false;
    }
    float _S1031;
    if(_S1027)
    {
        float3  _S1032;
        if(_runFlag_21)
        {
            float3  _S1033 = _S1029 * v_point_2;
            _S1029 = _S1026;
            _S1032 = _S1033;
        }
        else
        {
            _S1029 = v_point_2;
            _S1032 = _S1026;
        }
        if(_S1028)
        {
            _S1031 = _S1029.z;
        }
        else
        {
            _S1031 = 0.0f;
        }
        _S1029 = _S1032;
    }
    else
    {
        _S1029 = _S1026;
        _S1031 = 0.0f;
    }
    return _S1029.x + _S1029.y + _S1029.z + _S1031;
}

inline __device__ float3  depth_to_normal_prism(float2  pix_center_10, float4  intrins_19, FixedArray<float, 8>  dist_coeffs_22, int camera_model_21, bool is_ray_depth_19, float4  depths_8)
{
    float3  normal_12;
    for(;;)
    {
        bool _S1034;
        if((depths_8.x) == 0.0f)
        {
            _S1034 = true;
        }
        else
        {
            _S1034 = (depths_8.y) == 0.0f;
        }
        if(_S1034)
        {
            _S1034 = true;
        }
        else
        {
            _S1034 = (depths_8.z) == 0.0f;
        }
        if(_S1034)
        {
            _S1034 = true;
        }
        else
        {
            _S1034 = (depths_8.w) == 0.0f;
        }
        if(_S1034)
        {
            normal_12 = make_float3 (0.0f);
            break;
        }
        float3  * _S1035;
        float3  * _S1036;
        float3  * _S1037;
        float3  * _S1038;
        int _S1039;
        FixedArray<float3 , 4>  points_10;
        for(;;)
        {
            float2  _S1040 = float2 {intrins_19.z, intrins_19.w};
            float2  _S1041 = float2 {intrins_19.x, intrins_19.y};
            float2  uv_53 = (pix_center_10 + make_float2 (-1.0f, -0.0f) - _S1040) / _S1041;
            FixedArray<float, 8>  _S1042 = dist_coeffs_22;
            float2  uv_u_26;
            bool _S1043 = undistort_point_2(uv_53, &_S1042, int(12), &uv_u_26);
            if(!_S1043)
            {
                float3  _S1044 = make_float3 (0.0f);
                _S1039 = int(0);
                _S1038 = nullptr;
                _S1037 = nullptr;
                _S1036 = nullptr;
                _S1035 = nullptr;
                normal_12 = _S1044;
                break;
            }
            points_10[int(0)] = make_float3 (depths_8.x) * unproject_raydir_0(uv_u_26, camera_model_21, is_ray_depth_19);
            for(;;)
            {
                float2  uv_54 = (pix_center_10 + make_float2 (1.0f, -0.0f) - _S1040) / _S1041;
                FixedArray<float, 8>  _S1045 = dist_coeffs_22;
                float2  uv_u_27;
                bool _S1046 = undistort_point_2(uv_54, &_S1045, int(12), &uv_u_27);
                if(!_S1046)
                {
                    float3  _S1047 = make_float3 (0.0f);
                    _S1039 = int(0);
                    _S1038 = nullptr;
                    normal_12 = _S1047;
                    break;
                }
                points_10[int(1)] = make_float3 (depths_8.y) * unproject_raydir_0(uv_u_27, camera_model_21, is_ray_depth_19);
                _S1039 = int(2);
                _S1038 = &points_10[int(1)];
                break;
            }
            if(_S1039 != int(2))
            {
                _S1037 = &points_10[int(0)];
                _S1036 = nullptr;
                _S1035 = nullptr;
                break;
            }
            float2  uv_55 = (pix_center_10 + make_float2 (0.0f, -1.0f) - _S1040) / _S1041;
            FixedArray<float, 8>  _S1048 = dist_coeffs_22;
            float2  uv_u_28;
            bool _S1049 = undistort_point_2(uv_55, &_S1048, int(12), &uv_u_28);
            if(!_S1049)
            {
                float3  _S1050 = make_float3 (0.0f);
                _S1039 = int(0);
                _S1037 = &points_10[int(0)];
                _S1036 = nullptr;
                _S1035 = nullptr;
                normal_12 = _S1050;
                break;
            }
            points_10[int(2)] = make_float3 (depths_8.z) * unproject_raydir_0(uv_u_28, camera_model_21, is_ray_depth_19);
            for(;;)
            {
                float2  uv_56 = (pix_center_10 + make_float2 (0.0f, 1.0f) - _S1040) / _S1041;
                FixedArray<float, 8>  _S1051 = dist_coeffs_22;
                float2  uv_u_29;
                bool _S1052 = undistort_point_2(uv_56, &_S1051, int(12), &uv_u_29);
                if(!_S1052)
                {
                    float3  _S1053 = make_float3 (0.0f);
                    _S1039 = int(0);
                    _S1037 = nullptr;
                    normal_12 = _S1053;
                    break;
                }
                points_10[int(3)] = make_float3 (depths_8.w) * unproject_raydir_0(uv_u_29, camera_model_21, is_ray_depth_19);
                _S1039 = int(2);
                _S1037 = &points_10[int(3)];
                break;
            }
            if(_S1039 != int(2))
            {
                float3  * _S1054 = _S1037;
                _S1037 = &points_10[int(0)];
                _S1036 = _S1054;
                _S1035 = &points_10[int(2)];
                break;
            }
            float3  * _S1055 = _S1037;
            _S1039 = int(1);
            _S1037 = &points_10[int(0)];
            _S1036 = _S1055;
            _S1035 = &points_10[int(2)];
            break;
        }
        if(_S1039 != int(1))
        {
            break;
        }
        float3  normal_13 = cross_0(*_S1038 - *_S1037, - (*_S1036 - *_S1035));
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
    float2  _S1056;
    bool _S1057;
    float2  _S1058;
    bool _S1059;
    float2  _S1060;
    bool _S1061;
    float2  _S1062;
    bool _S1063;
};

inline __device__ void depth_to_normal_vjp_prism(float2  pix_center_11, float4  intrins_20, FixedArray<float, 8>  dist_coeffs_23, int camera_model_22, bool is_ray_depth_20, float4  depths_9, float3  v_normal_3, float4  * v_depths_4)
{
    float2  _S1064 = make_float2 (0.0f);
    s_bwd_prop_depth_to_normal_Intermediates_2 _S1065;
    (&_S1065)->_S1056 = _S1064;
    (&_S1065)->_S1057 = false;
    (&_S1065)->_S1058 = _S1064;
    (&_S1065)->_S1059 = false;
    (&_S1065)->_S1060 = _S1064;
    (&_S1065)->_S1061 = false;
    (&_S1065)->_S1062 = _S1064;
    (&_S1065)->_S1063 = false;
    (&_S1065)->_S1056 = _S1064;
    (&_S1065)->_S1057 = false;
    (&_S1065)->_S1058 = _S1064;
    (&_S1065)->_S1059 = false;
    (&_S1065)->_S1060 = _S1064;
    (&_S1065)->_S1061 = false;
    (&_S1065)->_S1062 = _S1064;
    (&_S1065)->_S1063 = false;
    bool _S1066 = (depths_9.x) == 0.0f;
    bool _runFlag_22;
    if(_S1066)
    {
        _runFlag_22 = true;
    }
    else
    {
        _runFlag_22 = (depths_9.y) == 0.0f;
    }
    if(_runFlag_22)
    {
        _runFlag_22 = true;
    }
    else
    {
        _runFlag_22 = (depths_9.z) == 0.0f;
    }
    if(_runFlag_22)
    {
        _runFlag_22 = true;
    }
    else
    {
        _runFlag_22 = (depths_9.w) == 0.0f;
    }
    int _S1067;
    if(!_runFlag_22)
    {
        float2  _S1068 = float2 {intrins_20.z, intrins_20.w};
        float2  _S1069 = float2 {intrins_20.x, intrins_20.y};
        float2  uv_57 = (pix_center_11 + make_float2 (-1.0f, -0.0f) - _S1068) / _S1069;
        float2  _S1070 = _S1064;
        FixedArray<float, 8>  _S1071 = dist_coeffs_23;
        bool _S1072 = undistort_point_2(uv_57, &_S1071, int(12), &_S1070);
        (&_S1065)->_S1056 = _S1070;
        (&_S1065)->_S1057 = _S1072;
        bool _S1073 = !!_S1072;
        if(_S1073)
        {
            float2  uv_58 = (pix_center_11 + make_float2 (1.0f, -0.0f) - _S1068) / _S1069;
            float2  _S1074 = _S1064;
            FixedArray<float, 8>  _S1075 = dist_coeffs_23;
            bool _S1076 = undistort_point_2(uv_58, &_S1075, int(12), &_S1074);
            (&_S1065)->_S1058 = _S1074;
            (&_S1065)->_S1059 = _S1076;
            if(!!_S1076)
            {
                _S1067 = int(2);
            }
            else
            {
                _S1067 = int(0);
            }
            if(_S1067 != int(2))
            {
                _runFlag_22 = false;
            }
            else
            {
                _runFlag_22 = _S1073;
            }
            if(_runFlag_22)
            {
                float2  uv_59 = (pix_center_11 + make_float2 (0.0f, -1.0f) - _S1068) / _S1069;
                float2  _S1077 = _S1064;
                FixedArray<float, 8>  _S1078 = dist_coeffs_23;
                bool _S1079 = undistort_point_2(uv_59, &_S1078, int(12), &_S1077);
                (&_S1065)->_S1060 = _S1077;
                (&_S1065)->_S1061 = _S1079;
                if(!_S1079)
                {
                    _runFlag_22 = false;
                }
                if(_runFlag_22)
                {
                    float2  uv_60 = (pix_center_11 + make_float2 (0.0f, 1.0f) - _S1068) / _S1069;
                    float2  _S1080 = _S1064;
                    FixedArray<float, 8>  _S1081 = dist_coeffs_23;
                    bool _S1082 = undistort_point_2(uv_60, &_S1081, int(12), &_S1080);
                    (&_S1065)->_S1062 = _S1080;
                    (&_S1065)->_S1063 = _S1082;
                }
            }
        }
    }
    s_bwd_prop_depth_to_normal_Intermediates_2 _S1083 = _S1065;
    float3  _S1084 = make_float3 (0.0f);
    if(_S1066)
    {
        _runFlag_22 = true;
    }
    else
    {
        _runFlag_22 = (depths_9.y) == 0.0f;
    }
    if(_runFlag_22)
    {
        _runFlag_22 = true;
    }
    else
    {
        _runFlag_22 = (depths_9.z) == 0.0f;
    }
    if(_runFlag_22)
    {
        _runFlag_22 = true;
    }
    else
    {
        _runFlag_22 = (depths_9.w) == 0.0f;
    }
    bool _S1085 = !_runFlag_22;
    bool _runFlag_23;
    bool _runFlag_24;
    bool _S1086;
    bool _runFlag_25;
    bool _S1087;
    bool _S1088;
    FixedArray<float3 , 4>  points_11;
    float3  _S1089;
    float3  _S1090;
    float3  _S1091;
    float3  _S1092;
    float3  _S1093;
    float3  _S1094;
    float3  _S1095;
    float3  _S1096;
    float3  _S1097;
    if(_S1085)
    {
        bool _S1098 = !!_S1083._S1057;
        if(_S1098)
        {
            float3  _S1099 = s_primal_ctx_unproject_raydir_0(_S1083._S1056, camera_model_22, is_ray_depth_20);
            float3  _S1100 = make_float3 (depths_9.x) * _S1099;
            bool _S1101 = !!_S1083._S1059;
            if(_S1101)
            {
                float3  _S1102 = s_primal_ctx_unproject_raydir_0(_S1083._S1058, camera_model_22, is_ray_depth_20);
                float3  _S1103 = make_float3 (depths_9.y) * _S1102;
                _S1067 = int(2);
                points_11[int(0)] = _S1100;
                points_11[int(1)] = _S1103;
                points_11[int(2)] = _S1084;
                points_11[int(3)] = _S1084;
                _S1089 = _S1102;
            }
            else
            {
                _S1067 = int(0);
                points_11[int(0)] = _S1100;
                points_11[int(1)] = _S1084;
                points_11[int(2)] = _S1084;
                points_11[int(3)] = _S1084;
                _S1089 = _S1084;
            }
            if(_S1067 != int(2))
            {
                _runFlag_22 = false;
            }
            else
            {
                _runFlag_22 = _S1098;
                _S1067 = int(0);
            }
            if(_runFlag_22)
            {
                if(!_S1083._S1061)
                {
                    _runFlag_23 = false;
                    _S1067 = int(0);
                }
                else
                {
                    _runFlag_23 = _runFlag_22;
                }
                if(_runFlag_23)
                {
                    float3  _S1104 = s_primal_ctx_unproject_raydir_0(_S1083._S1060, camera_model_22, is_ray_depth_20);
                    points_11[int(2)] = make_float3 (depths_9.z) * _S1104;
                    bool _S1105 = !!_S1083._S1063;
                    int _S1106;
                    if(_S1105)
                    {
                        float3  _S1107 = s_primal_ctx_unproject_raydir_0(_S1083._S1062, camera_model_22, is_ray_depth_20);
                        points_11[int(3)] = make_float3 (depths_9.w) * _S1107;
                        _S1106 = int(2);
                        _S1090 = _S1107;
                    }
                    else
                    {
                        _S1106 = int(0);
                        _S1090 = _S1084;
                    }
                    if(_S1106 != int(2))
                    {
                        _runFlag_24 = false;
                        _S1067 = _S1106;
                    }
                    else
                    {
                        _runFlag_24 = _runFlag_23;
                    }
                    if(_runFlag_24)
                    {
                        _S1067 = int(1);
                    }
                    _runFlag_24 = _S1105;
                    _S1091 = _S1104;
                }
                else
                {
                    _runFlag_24 = false;
                    _S1090 = _S1084;
                    _S1091 = _S1084;
                }
            }
            else
            {
                _runFlag_23 = false;
                _runFlag_24 = false;
                _S1090 = _S1084;
                _S1091 = _S1084;
            }
            float3  _S1108 = _S1089;
            _S1089 = _S1090;
            _S1090 = _S1091;
            _S1086 = _S1101;
            _S1091 = _S1108;
            _S1092 = _S1099;
        }
        else
        {
            _S1067 = int(0);
            points_11[int(0)] = _S1084;
            points_11[int(1)] = _S1084;
            points_11[int(2)] = _S1084;
            points_11[int(3)] = _S1084;
            _runFlag_22 = false;
            _runFlag_23 = false;
            _runFlag_24 = false;
            _S1089 = _S1084;
            _S1090 = _S1084;
            _S1086 = false;
            _S1091 = _S1084;
            _S1092 = _S1084;
        }
        if(_S1067 != int(1))
        {
            _runFlag_25 = false;
        }
        else
        {
            _runFlag_25 = _S1085;
        }
        if(_runFlag_25)
        {
            float3  dx_5 = points_11[int(1)] - points_11[int(0)];
            float3  _S1109 = - (points_11[int(3)] - points_11[int(2)]);
            float3  _S1110 = s_primal_ctx_cross_0(dx_5, _S1109);
            bool _S1111 = (s_primal_ctx_dot_0(_S1110, _S1110)) != 0.0f;
            if(_S1111)
            {
                float _S1112 = length_0(_S1110);
                float3  _S1113 = make_float3 (_S1112);
                _S1093 = make_float3 (_S1112 * _S1112);
                _S1094 = _S1113;
            }
            else
            {
                _S1093 = _S1084;
                _S1094 = _S1084;
            }
            float3  _S1114 = _S1094;
            _S1087 = _S1111;
            _S1094 = _S1110;
            _S1095 = _S1114;
            _S1096 = dx_5;
            _S1097 = _S1109;
        }
        else
        {
            _S1087 = false;
            _S1093 = _S1084;
            _S1094 = _S1084;
            _S1095 = _S1084;
            _S1096 = _S1084;
            _S1097 = _S1084;
        }
        bool _S1115 = _runFlag_22;
        bool _S1116 = _runFlag_23;
        bool _S1117 = _runFlag_24;
        float3  _S1118 = _S1089;
        float3  _S1119 = _S1090;
        bool _S1120 = _S1086;
        float3  _S1121 = _S1091;
        float3  _S1122 = _S1092;
        _runFlag_22 = _runFlag_25;
        _runFlag_23 = _S1087;
        _S1089 = _S1093;
        _S1090 = _S1094;
        _S1091 = _S1095;
        _S1092 = _S1096;
        _S1093 = _S1097;
        _runFlag_24 = _S1098;
        _S1086 = _S1115;
        _runFlag_25 = _S1116;
        _S1087 = _S1117;
        _S1094 = _S1118;
        _S1095 = _S1119;
        _S1088 = _S1120;
        _S1096 = _S1121;
        _S1097 = _S1122;
    }
    else
    {
        _runFlag_22 = false;
        _runFlag_23 = false;
        _S1089 = _S1084;
        _S1090 = _S1084;
        _S1091 = _S1084;
        _S1092 = _S1084;
        _S1093 = _S1084;
        _runFlag_24 = false;
        _S1086 = false;
        _runFlag_25 = false;
        _S1087 = false;
        _S1094 = _S1084;
        _S1095 = _S1084;
        _S1088 = false;
        _S1096 = _S1084;
        _S1097 = _S1084;
    }
    float4  _S1123 = make_float4 (0.0f);
    float4  _S1124;
    if(_S1085)
    {
        if(_runFlag_22)
        {
            if(_runFlag_23)
            {
                float3  _S1125 = v_normal_3 / _S1089;
                float3  _S1126 = _S1090 * - _S1125;
                float3  _S1127 = _S1091 * _S1125;
                float _S1128 = _S1126.x + _S1126.y + _S1126.z;
                DiffPair_vectorx3Cfloatx2C3x3E_0 _S1129;
                (&_S1129)->primal_0 = _S1090;
                (&_S1129)->differential_0 = _S1084;
                s_bwd_length_impl_0(&_S1129, _S1128);
                _S1089 = _S1127 + _S1129.differential_0;
            }
            else
            {
                _S1089 = v_normal_3;
            }
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1130;
            (&_S1130)->primal_0 = _S1090;
            (&_S1130)->differential_0 = _S1084;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1131;
            (&_S1131)->primal_0 = _S1090;
            (&_S1131)->differential_0 = _S1084;
            s_bwd_prop_dot_0(&_S1130, &_S1131, 0.0f);
            float3  _S1132 = _S1131.differential_0 + _S1130.differential_0 + _S1089;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1133;
            (&_S1133)->primal_0 = _S1092;
            (&_S1133)->differential_0 = _S1084;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1134;
            (&_S1134)->primal_0 = _S1093;
            (&_S1134)->differential_0 = _S1084;
            s_bwd_prop_cross_0(&_S1133, &_S1134, _S1132);
            float3  s_diff_dy_T_5 = - _S1134.differential_0;
            float3  _S1135 = - s_diff_dy_T_5;
            float3  _S1136 = - _S1133.differential_0;
            FixedArray<float3 , 4>  _S1137;
            _S1137[int(0)] = _S1084;
            _S1137[int(1)] = _S1084;
            _S1137[int(2)] = _S1084;
            _S1137[int(3)] = _S1084;
            _S1137[int(2)] = _S1135;
            _S1137[int(3)] = s_diff_dy_T_5;
            _S1137[int(0)] = _S1136;
            _S1137[int(1)] = _S1133.differential_0;
            points_11[int(0)] = _S1137[int(0)];
            points_11[int(1)] = _S1137[int(1)];
            points_11[int(2)] = _S1137[int(2)];
            points_11[int(3)] = _S1137[int(3)];
        }
        else
        {
            points_11[int(0)] = _S1084;
            points_11[int(1)] = _S1084;
            points_11[int(2)] = _S1084;
            points_11[int(3)] = _S1084;
        }
        if(_runFlag_24)
        {
            if(_S1086)
            {
                if(_runFlag_25)
                {
                    FixedArray<float3 , 4>  _S1138 = points_11;
                    FixedArray<float3 , 4>  _S1139 = points_11;
                    FixedArray<float3 , 4>  _S1140 = points_11;
                    FixedArray<float3 , 4>  _S1141 = points_11;
                    if(_S1087)
                    {
                        float3  _S1142 = _S1094 * _S1141[int(3)];
                        float _S1143 = _S1142.x + _S1142.y + _S1142.z;
                        float4  _S1144 = _S1123;
                        *&((&_S1144)->w) = _S1143;
                        points_11[int(0)] = _S1138[int(0)];
                        points_11[int(1)] = _S1139[int(1)];
                        points_11[int(2)] = _S1140[int(2)];
                        points_11[int(3)] = _S1084;
                        _S1124 = _S1144;
                    }
                    else
                    {
                        points_11[int(0)] = _S1138[int(0)];
                        points_11[int(1)] = _S1139[int(1)];
                        points_11[int(2)] = _S1140[int(2)];
                        points_11[int(3)] = _S1141[int(3)];
                        _S1124 = _S1123;
                    }
                    float3  _S1145 = _S1095 * points_11[int(2)];
                    float _S1146 = _S1145.x + _S1145.y + _S1145.z;
                    FixedArray<float3 , 4>  _S1147 = points_11;
                    FixedArray<float3 , 4>  _S1148 = points_11;
                    float4  _S1149 = _S1123;
                    *&((&_S1149)->z) = _S1146;
                    float4  _S1150 = _S1124 + _S1149;
                    points_11[int(0)] = points_11[int(0)];
                    points_11[int(1)] = _S1147[int(1)];
                    points_11[int(2)] = _S1084;
                    points_11[int(3)] = _S1148[int(3)];
                    _S1124 = _S1150;
                }
                else
                {
                    FixedArray<float3 , 4>  _S1151 = points_11;
                    FixedArray<float3 , 4>  _S1152 = points_11;
                    FixedArray<float3 , 4>  _S1153 = points_11;
                    points_11[int(0)] = points_11[int(0)];
                    points_11[int(1)] = _S1151[int(1)];
                    points_11[int(2)] = _S1152[int(2)];
                    points_11[int(3)] = _S1153[int(3)];
                    _S1124 = _S1123;
                }
            }
            else
            {
                FixedArray<float3 , 4>  _S1154 = points_11;
                FixedArray<float3 , 4>  _S1155 = points_11;
                FixedArray<float3 , 4>  _S1156 = points_11;
                points_11[int(0)] = points_11[int(0)];
                points_11[int(1)] = _S1154[int(1)];
                points_11[int(2)] = _S1155[int(2)];
                points_11[int(3)] = _S1156[int(3)];
                _S1124 = _S1123;
            }
            if(_S1088)
            {
                FixedArray<float3 , 4>  _S1157 = points_11;
                float3  _S1158 = _S1096 * points_11[int(1)];
                float _S1159 = _S1158.x + _S1158.y + _S1158.z;
                float4  _S1160 = _S1123;
                *&((&_S1160)->y) = _S1159;
                float4  _S1161 = _S1124 + _S1160;
                points_11[int(0)] = _S1084;
                points_11[int(1)] = _S1084;
                points_11[int(2)] = _S1084;
                points_11[int(3)] = _S1084;
                _S1089 = _S1157[int(0)];
                _S1124 = _S1161;
            }
            else
            {
                FixedArray<float3 , 4>  _S1162 = points_11;
                FixedArray<float3 , 4>  _S1163 = points_11;
                FixedArray<float3 , 4>  _S1164 = points_11;
                points_11[int(0)] = points_11[int(0)];
                points_11[int(1)] = _S1162[int(1)];
                points_11[int(2)] = _S1163[int(2)];
                points_11[int(3)] = _S1164[int(3)];
                _S1089 = _S1084;
            }
            float3  _S1165 = _S1097 * (points_11[int(0)] + _S1089);
            float _S1166 = _S1165.x + _S1165.y + _S1165.z;
            float4  _S1167 = _S1123;
            *&((&_S1167)->x) = _S1166;
            _S1124 = _S1124 + _S1167;
        }
        else
        {
            _S1124 = _S1123;
        }
    }
    else
    {
        _S1124 = _S1123;
    }
    *v_depths_4 = _S1124;
    return;
}

inline __device__ float ray_depth_to_linear_depth_factor_prism(float2  pix_center_12, float4  intrins_21, FixedArray<float, 8>  dist_coeffs_24, int camera_model_23)
{
    float _S1168;
    for(;;)
    {
        float2  uv_61 = (pix_center_12 - float2 {intrins_21.z, intrins_21.w}) / float2 {intrins_21.x, intrins_21.y};
        FixedArray<float, 8>  _S1169 = dist_coeffs_24;
        float2  uv_u_30;
        bool _S1170 = undistort_point_2(uv_61, &_S1169, int(12), &uv_u_30);
        if(!_S1170)
        {
            _S1168 = 0.0f;
            break;
        }
        float3  raydir_16 = unproject_raydir_0(uv_u_30, camera_model_23, false);
        _S1168 = float((F32_sign((raydir_16.z)))) / length_0(raydir_16);
        break;
    }
    return _S1168;
}

inline __device__ float depth_normal_loss_prism(float2  pix_center_13, float4  intrins_22, FixedArray<float, 8>  dist_coeffs_25, int camera_model_24, bool is_ray_depth_21, float4  depths_10, float3  gt_normal_4)
{
    float _S1171;
    for(;;)
    {
        float3  _S1172;
        float3  * _S1173;
        float3  * _S1174;
        float3  * _S1175;
        float3  * _S1176;
        int _S1177;
        FixedArray<float3 , 5>  points_12;
        for(;;)
        {
            float2  _S1178 = float2 {intrins_22.z, intrins_22.w};
            float2  _S1179 = float2 {intrins_22.x, intrins_22.y};
            float2  uv_62 = (pix_center_13 + make_float2 (-1.0f, -0.0f) - _S1178) / _S1179;
            FixedArray<float, 8>  _S1180 = dist_coeffs_25;
            float2  uv_u_31;
            bool _S1181 = undistort_point_2(uv_62, &_S1180, int(12), &uv_u_31);
            float3  _S1182 = make_float3 (0.0f);
            if(!_S1181)
            {
                _S1177 = int(0);
                _S1176 = nullptr;
                _S1175 = nullptr;
                _S1174 = nullptr;
                _S1173 = nullptr;
                _S1172 = _S1182;
                break;
            }
            float3  raydir_17 = unproject_raydir_0(uv_u_31, camera_model_24, is_ray_depth_21);
            points_12[int(0)] = make_float3 (depths_10.x) * raydir_17;
            float2  uv_63 = (pix_center_13 + make_float2 (1.0f, -0.0f) - _S1178) / _S1179;
            FixedArray<float, 8>  _S1183 = dist_coeffs_25;
            float2  uv_u_32;
            bool _S1184 = undistort_point_2(uv_63, &_S1183, int(12), &uv_u_32);
            if(!_S1184)
            {
                _S1177 = int(0);
                _S1176 = nullptr;
                _S1175 = &points_12[int(0)];
                _S1174 = nullptr;
                _S1173 = nullptr;
                _S1172 = _S1182;
                break;
            }
            float3  raydir_18 = unproject_raydir_0(uv_u_32, camera_model_24, is_ray_depth_21);
            points_12[int(1)] = make_float3 (depths_10.y) * raydir_18;
            float2  uv_64 = (pix_center_13 + make_float2 (0.0f, -1.0f) - _S1178) / _S1179;
            FixedArray<float, 8>  _S1185 = dist_coeffs_25;
            float2  uv_u_33;
            bool _S1186 = undistort_point_2(uv_64, &_S1185, int(12), &uv_u_33);
            if(!_S1186)
            {
                _S1177 = int(0);
                _S1176 = &points_12[int(1)];
                _S1175 = &points_12[int(0)];
                _S1174 = nullptr;
                _S1173 = nullptr;
                _S1172 = _S1182;
                break;
            }
            float3  raydir_19 = unproject_raydir_0(uv_u_33, camera_model_24, is_ray_depth_21);
            points_12[int(2)] = make_float3 (depths_10.z) * raydir_19;
            float2  uv_65 = (pix_center_13 + make_float2 (0.0f, 1.0f) - _S1178) / _S1179;
            FixedArray<float, 8>  _S1187 = dist_coeffs_25;
            float2  uv_u_34;
            bool _S1188 = undistort_point_2(uv_65, &_S1187, int(12), &uv_u_34);
            if(!_S1188)
            {
                _S1177 = int(0);
                _S1176 = &points_12[int(1)];
                _S1175 = &points_12[int(0)];
                _S1174 = nullptr;
                _S1173 = &points_12[int(2)];
                _S1172 = _S1182;
                break;
            }
            float3  raydir_20 = unproject_raydir_0(uv_u_34, camera_model_24, is_ray_depth_21);
            points_12[int(3)] = make_float3 (depths_10.w) * raydir_20;
            float2  uv_66 = (pix_center_13 + make_float2 (0.0f) * make_float2 (0.0f, 3.0f) - _S1178) / _S1179;
            FixedArray<float, 8>  _S1189 = dist_coeffs_25;
            float2  uv_u_35;
            bool _S1190 = undistort_point_2(uv_66, &_S1189, int(12), &uv_u_35);
            if(!_S1190)
            {
                _S1177 = int(0);
                _S1176 = &points_12[int(1)];
                _S1175 = &points_12[int(0)];
                _S1174 = &points_12[int(3)];
                _S1173 = &points_12[int(2)];
                _S1172 = _S1182;
                break;
            }
            float3  raydir_21 = unproject_raydir_0(uv_u_35, camera_model_24, is_ray_depth_21);
            _S1177 = int(1);
            _S1176 = &points_12[int(1)];
            _S1175 = &points_12[int(0)];
            _S1174 = &points_12[int(3)];
            _S1173 = &points_12[int(2)];
            _S1172 = raydir_21;
            break;
        }
        if(_S1177 != int(1))
        {
            _S1171 = 0.0f;
            break;
        }
        float3  normal_14 = cross_0(*_S1176 - *_S1175, - (*_S1174 - *_S1173));
        float3  normal_15;
        if((dot_0(normal_14, normal_14)) != 0.0f)
        {
            normal_15 = normalize_0(normal_14);
        }
        else
        {
            normal_15 = normal_14;
        }
        float3  _S1191;
        if((dot_0(gt_normal_4, gt_normal_4)) != 0.0f)
        {
            _S1191 = normalize_0(gt_normal_4);
        }
        else
        {
            _S1191 = gt_normal_4;
        }
        _S1171 = (1.0f - dot_0(normal_15, _S1191) + 0.00100000004749745f) / ((F32_max((dot_0(normal_15, - normalize_0(_S1172))), (0.0f))) + 0.00100000004749745f);
        break;
    }
    return _S1171;
}

struct s_bwd_prop_depth_normal_loss_Intermediates_2
{
    float2  _S1192;
    bool _S1193;
    float2  _S1194;
    bool _S1195;
    float2  _S1196;
    bool _S1197;
    float2  _S1198;
    bool _S1199;
    float2  _S1200;
    bool _S1201;
};

inline __device__ void depth_normal_loss_vjp_prism(float2  pix_center_14, float4  intrins_23, FixedArray<float, 8>  dist_coeffs_26, int camera_model_25, bool is_ray_depth_22, float4  depths_11, float3  gt_normal_5, float v_loss_2, float4  * v_depths_5, float3  * v_gt_normal_2)
{
    float2  _S1202 = make_float2 (0.0f);
    s_bwd_prop_depth_normal_loss_Intermediates_2 _S1203;
    (&_S1203)->_S1192 = _S1202;
    (&_S1203)->_S1193 = false;
    (&_S1203)->_S1194 = _S1202;
    (&_S1203)->_S1195 = false;
    (&_S1203)->_S1196 = _S1202;
    (&_S1203)->_S1197 = false;
    (&_S1203)->_S1198 = _S1202;
    (&_S1203)->_S1199 = false;
    (&_S1203)->_S1200 = _S1202;
    (&_S1203)->_S1201 = false;
    (&_S1203)->_S1194 = _S1202;
    (&_S1203)->_S1195 = false;
    (&_S1203)->_S1196 = _S1202;
    (&_S1203)->_S1197 = false;
    (&_S1203)->_S1198 = _S1202;
    (&_S1203)->_S1199 = false;
    (&_S1203)->_S1200 = _S1202;
    (&_S1203)->_S1201 = false;
    float2  _S1204 = float2 {intrins_23.z, intrins_23.w};
    float2  _S1205 = float2 {intrins_23.x, intrins_23.y};
    float2  uv_67 = (pix_center_14 + make_float2 (-1.0f, -0.0f) - _S1204) / _S1205;
    float2  _S1206 = _S1202;
    FixedArray<float, 8>  _S1207 = dist_coeffs_26;
    bool _S1208 = undistort_point_2(uv_67, &_S1207, int(12), &_S1206);
    (&_S1203)->_S1192 = _S1206;
    (&_S1203)->_S1193 = _S1208;
    bool _S1209 = !!_S1208;
    bool _runFlag_26;
    if(_S1209)
    {
        float2  uv_68 = (pix_center_14 + make_float2 (1.0f, -0.0f) - _S1204) / _S1205;
        float2  _S1210 = _S1202;
        FixedArray<float, 8>  _S1211 = dist_coeffs_26;
        bool _S1212 = undistort_point_2(uv_68, &_S1211, int(12), &_S1210);
        (&_S1203)->_S1194 = _S1210;
        (&_S1203)->_S1195 = _S1212;
        if(!_S1212)
        {
            _runFlag_26 = false;
        }
        else
        {
            _runFlag_26 = _S1209;
        }
        if(_runFlag_26)
        {
            float2  uv_69 = (pix_center_14 + make_float2 (0.0f, -1.0f) - _S1204) / _S1205;
            float2  _S1213 = _S1202;
            FixedArray<float, 8>  _S1214 = dist_coeffs_26;
            bool _S1215 = undistort_point_2(uv_69, &_S1214, int(12), &_S1213);
            (&_S1203)->_S1196 = _S1213;
            (&_S1203)->_S1197 = _S1215;
            if(!_S1215)
            {
                _runFlag_26 = false;
            }
            if(_runFlag_26)
            {
                float2  uv_70 = (pix_center_14 + make_float2 (0.0f, 1.0f) - _S1204) / _S1205;
                float2  _S1216 = _S1202;
                FixedArray<float, 8>  _S1217 = dist_coeffs_26;
                bool _S1218 = undistort_point_2(uv_70, &_S1217, int(12), &_S1216);
                (&_S1203)->_S1198 = _S1216;
                (&_S1203)->_S1199 = _S1218;
                if(!_S1218)
                {
                    _runFlag_26 = false;
                }
                if(_runFlag_26)
                {
                    float2  uv_71 = (pix_center_14 - _S1204) / _S1205;
                    float2  _S1219 = _S1202;
                    FixedArray<float, 8>  _S1220 = dist_coeffs_26;
                    bool _S1221 = undistort_point_2(uv_71, &_S1220, int(12), &_S1219);
                    (&_S1203)->_S1200 = _S1219;
                    (&_S1203)->_S1201 = _S1221;
                }
            }
        }
    }
    s_bwd_prop_depth_normal_loss_Intermediates_2 _S1222 = _S1203;
    float3  _S1223 = make_float3 (0.0f);
    bool _S1224 = !!_S1203._S1193;
    bool _runFlag_27;
    bool _runFlag_28;
    bool _runFlag_29;
    int _S1225;
    float3  raydir_22;
    float3  _S1226;
    float3  _S1227;
    float3  _S1228;
    float3  _S1229;
    FixedArray<float3 , 5>  points_13;
    if(_S1224)
    {
        float3  _S1230 = s_primal_ctx_unproject_raydir_0(_S1222._S1192, camera_model_25, is_ray_depth_22);
        float3  _S1231 = make_float3 (depths_11.x) * _S1230;
        if(!_S1222._S1195)
        {
            _runFlag_26 = false;
        }
        else
        {
            _runFlag_26 = _S1224;
        }
        if(_runFlag_26)
        {
            float3  _S1232 = s_primal_ctx_unproject_raydir_0(_S1222._S1194, camera_model_25, is_ray_depth_22);
            float3  _S1233 = make_float3 (depths_11.y) * _S1232;
            if(!_S1222._S1197)
            {
                _runFlag_27 = false;
            }
            else
            {
                _runFlag_27 = _runFlag_26;
            }
            if(_runFlag_27)
            {
                float3  _S1234 = s_primal_ctx_unproject_raydir_0(_S1222._S1196, camera_model_25, is_ray_depth_22);
                float3  _S1235 = make_float3 (depths_11.z) * _S1234;
                if(!_S1222._S1199)
                {
                    _runFlag_28 = false;
                }
                else
                {
                    _runFlag_28 = _runFlag_27;
                }
                if(_runFlag_28)
                {
                    float3  _S1236 = s_primal_ctx_unproject_raydir_0(_S1222._S1198, camera_model_25, is_ray_depth_22);
                    float3  _S1237 = make_float3 (depths_11.w) * _S1236;
                    if(!_S1222._S1201)
                    {
                        _runFlag_29 = false;
                    }
                    else
                    {
                        _runFlag_29 = _runFlag_28;
                    }
                    if(_runFlag_29)
                    {
                        float3  _S1238 = s_primal_ctx_unproject_raydir_0(_S1222._S1200, camera_model_25, is_ray_depth_22);
                        _S1225 = int(1);
                        raydir_22 = _S1238;
                    }
                    else
                    {
                        _S1225 = int(0);
                        raydir_22 = _S1236;
                    }
                    points_13[int(0)] = _S1231;
                    points_13[int(1)] = _S1233;
                    points_13[int(2)] = _S1235;
                    points_13[int(3)] = _S1237;
                    points_13[int(4)] = _S1223;
                    _S1226 = _S1236;
                }
                else
                {
                    _S1225 = int(0);
                    raydir_22 = _S1234;
                    points_13[int(0)] = _S1231;
                    points_13[int(1)] = _S1233;
                    points_13[int(2)] = _S1235;
                    points_13[int(3)] = _S1223;
                    points_13[int(4)] = _S1223;
                    _S1226 = _S1223;
                }
                _S1227 = _S1234;
            }
            else
            {
                _S1225 = int(0);
                raydir_22 = _S1232;
                points_13[int(0)] = _S1231;
                points_13[int(1)] = _S1233;
                points_13[int(2)] = _S1223;
                points_13[int(3)] = _S1223;
                points_13[int(4)] = _S1223;
                _runFlag_28 = false;
                _S1226 = _S1223;
                _S1227 = _S1223;
            }
            _S1228 = _S1232;
        }
        else
        {
            _S1225 = int(0);
            raydir_22 = _S1230;
            points_13[int(0)] = _S1231;
            points_13[int(1)] = _S1223;
            points_13[int(2)] = _S1223;
            points_13[int(3)] = _S1223;
            points_13[int(4)] = _S1223;
            _runFlag_27 = false;
            _runFlag_28 = false;
            _S1226 = _S1223;
            _S1227 = _S1223;
            _S1228 = _S1223;
        }
        _S1229 = _S1230;
    }
    else
    {
        _S1225 = int(0);
        points_13[int(0)] = _S1223;
        points_13[int(1)] = _S1223;
        points_13[int(2)] = _S1223;
        points_13[int(3)] = _S1223;
        points_13[int(4)] = _S1223;
        _runFlag_26 = false;
        _runFlag_27 = false;
        _runFlag_28 = false;
        _S1226 = _S1223;
        _S1227 = _S1223;
        _S1228 = _S1223;
        _S1229 = _S1223;
    }
    bool _S1239 = !(_S1225 != int(1));
    bool _S1240;
    float3  normal_16;
    float3  _S1241;
    float3  _S1242;
    float3  _S1243;
    float3  _S1244;
    float _S1245;
    float _S1246;
    float _S1247;
    float _S1248;
    if(_S1239)
    {
        float3  dx_6 = points_13[int(1)] - points_13[int(0)];
        float3  _S1249 = - (points_13[int(3)] - points_13[int(2)]);
        float3  _S1250 = s_primal_ctx_cross_0(dx_6, _S1249);
        bool _S1251 = (s_primal_ctx_dot_0(_S1250, _S1250)) != 0.0f;
        if(_S1251)
        {
            normal_16 = normalize_0(_S1250);
        }
        else
        {
            normal_16 = _S1250;
        }
        bool _S1252 = (s_primal_ctx_dot_0(gt_normal_5, gt_normal_5)) != 0.0f;
        if(_S1252)
        {
            _S1241 = normalize_0(gt_normal_5);
        }
        else
        {
            _S1241 = gt_normal_5;
        }
        float3  _S1253 = - normalize_0(raydir_22);
        float _S1254 = s_primal_ctx_dot_0(normal_16, _S1253);
        float _S1255 = 1.0f - s_primal_ctx_dot_0(normal_16, _S1241) + 0.00100000004749745f;
        float _S1256 = (F32_max((_S1254), (0.0f))) + 0.00100000004749745f;
        _S1245 = _S1256 * _S1256;
        _S1246 = _S1255;
        _S1247 = _S1256;
        _S1248 = _S1254;
        raydir_22 = normal_16;
        normal_16 = _S1253;
        _runFlag_29 = _S1252;
        _S1240 = _S1251;
        _S1242 = _S1250;
        _S1243 = dx_6;
        _S1244 = _S1249;
    }
    else
    {
        _S1245 = 0.0f;
        _S1246 = 0.0f;
        _S1247 = 0.0f;
        _S1248 = 0.0f;
        raydir_22 = _S1223;
        normal_16 = _S1223;
        _S1241 = _S1223;
        _runFlag_29 = false;
        _S1240 = false;
        _S1242 = _S1223;
        _S1243 = _S1223;
        _S1244 = _S1223;
    }
    float4  _S1257 = make_float4 (0.0f);
    if(_S1239)
    {
        float _S1258 = v_loss_2 / _S1245;
        float _S1259 = _S1246 * - _S1258;
        float s_diff_num_T_2 = _S1247 * _S1258;
        DiffPair_float_0 _S1260;
        (&_S1260)->primal_0 = _S1248;
        (&_S1260)->differential_0 = 0.0f;
        DiffPair_float_0 _S1261;
        (&_S1261)->primal_0 = 0.0f;
        (&_S1261)->differential_0 = 0.0f;
        _d_max_0(&_S1260, &_S1261, _S1259);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1262;
        (&_S1262)->primal_0 = raydir_22;
        (&_S1262)->differential_0 = _S1223;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1263;
        (&_S1263)->primal_0 = normal_16;
        (&_S1263)->differential_0 = _S1223;
        s_bwd_prop_dot_0(&_S1262, &_S1263, _S1260.differential_0);
        float _S1264 = - s_diff_num_T_2;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1265;
        (&_S1265)->primal_0 = raydir_22;
        (&_S1265)->differential_0 = _S1223;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1266;
        (&_S1266)->primal_0 = _S1241;
        (&_S1266)->differential_0 = _S1223;
        s_bwd_prop_dot_0(&_S1265, &_S1266, _S1264);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1267 = _S1266;
        float3  _S1268 = _S1262.differential_0 + _S1265.differential_0;
        if(_runFlag_29)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1269;
            (&_S1269)->primal_0 = gt_normal_5;
            (&_S1269)->differential_0 = _S1223;
            s_bwd_normalize_impl_0(&_S1269, _S1267.differential_0);
            raydir_22 = _S1269.differential_0;
        }
        else
        {
            raydir_22 = _S1267.differential_0;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1270;
        (&_S1270)->primal_0 = gt_normal_5;
        (&_S1270)->differential_0 = _S1223;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1271;
        (&_S1271)->primal_0 = gt_normal_5;
        (&_S1271)->differential_0 = _S1223;
        s_bwd_prop_dot_0(&_S1270, &_S1271, 0.0f);
        float3  _S1272 = _S1271.differential_0 + _S1270.differential_0 + raydir_22;
        if(_S1240)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1273;
            (&_S1273)->primal_0 = _S1242;
            (&_S1273)->differential_0 = _S1223;
            s_bwd_normalize_impl_0(&_S1273, _S1268);
            raydir_22 = _S1273.differential_0;
        }
        else
        {
            raydir_22 = _S1268;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1274;
        (&_S1274)->primal_0 = _S1242;
        (&_S1274)->differential_0 = _S1223;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1275;
        (&_S1275)->primal_0 = _S1242;
        (&_S1275)->differential_0 = _S1223;
        s_bwd_prop_dot_0(&_S1274, &_S1275, 0.0f);
        float3  _S1276 = _S1275.differential_0 + _S1274.differential_0 + raydir_22;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1277;
        (&_S1277)->primal_0 = _S1243;
        (&_S1277)->differential_0 = _S1223;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1278;
        (&_S1278)->primal_0 = _S1244;
        (&_S1278)->differential_0 = _S1223;
        s_bwd_prop_cross_0(&_S1277, &_S1278, _S1276);
        float3  s_diff_dy_T_6 = - _S1278.differential_0;
        float3  _S1279 = - s_diff_dy_T_6;
        float3  _S1280 = - _S1277.differential_0;
        FixedArray<float3 , 5>  _S1281;
        _S1281[int(0)] = _S1223;
        _S1281[int(1)] = _S1223;
        _S1281[int(2)] = _S1223;
        _S1281[int(3)] = _S1223;
        _S1281[int(4)] = _S1223;
        _S1281[int(2)] = _S1279;
        _S1281[int(3)] = s_diff_dy_T_6;
        _S1281[int(0)] = _S1280;
        _S1281[int(1)] = _S1277.differential_0;
        points_13[int(0)] = _S1281[int(0)];
        points_13[int(1)] = _S1281[int(1)];
        points_13[int(2)] = _S1281[int(2)];
        points_13[int(3)] = _S1281[int(3)];
        points_13[int(4)] = _S1281[int(4)];
        raydir_22 = _S1272;
    }
    else
    {
        points_13[int(0)] = _S1223;
        points_13[int(1)] = _S1223;
        points_13[int(2)] = _S1223;
        points_13[int(3)] = _S1223;
        points_13[int(4)] = _S1223;
        raydir_22 = _S1223;
    }
    float4  _S1282;
    if(_S1224)
    {
        if(_runFlag_26)
        {
            if(_runFlag_27)
            {
                if(_runFlag_28)
                {
                    FixedArray<float3 , 5>  _S1283 = points_13;
                    FixedArray<float3 , 5>  _S1284 = points_13;
                    FixedArray<float3 , 5>  _S1285 = points_13;
                    float3  _S1286 = _S1226 * points_13[int(3)];
                    float _S1287 = _S1286.x + _S1286.y + _S1286.z;
                    float4  _S1288 = _S1257;
                    *&((&_S1288)->w) = _S1287;
                    points_13[int(0)] = _S1223;
                    points_13[int(1)] = _S1223;
                    points_13[int(2)] = _S1223;
                    points_13[int(3)] = _S1223;
                    points_13[int(4)] = _S1223;
                    _S1226 = _S1285[int(2)];
                    normal_16 = _S1283[int(0)];
                    _S1241 = _S1284[int(1)];
                    _S1282 = _S1288;
                }
                else
                {
                    FixedArray<float3 , 5>  _S1289 = points_13;
                    FixedArray<float3 , 5>  _S1290 = points_13;
                    FixedArray<float3 , 5>  _S1291 = points_13;
                    FixedArray<float3 , 5>  _S1292 = points_13;
                    points_13[int(0)] = points_13[int(0)];
                    points_13[int(1)] = _S1289[int(1)];
                    points_13[int(2)] = _S1290[int(2)];
                    points_13[int(3)] = _S1291[int(3)];
                    points_13[int(4)] = _S1292[int(4)];
                    _S1226 = _S1223;
                    normal_16 = _S1223;
                    _S1241 = _S1223;
                    _S1282 = _S1257;
                }
                float3  _S1293 = _S1227 * (points_13[int(2)] + _S1226);
                float _S1294 = _S1293.x + _S1293.y + _S1293.z;
                float3  _S1295 = points_13[int(0)] + normal_16;
                float3  _S1296 = points_13[int(1)] + _S1241;
                float4  _S1297 = _S1257;
                *&((&_S1297)->z) = _S1294;
                float4  _S1298 = _S1282 + _S1297;
                points_13[int(0)] = _S1223;
                points_13[int(1)] = _S1223;
                points_13[int(2)] = _S1223;
                points_13[int(3)] = _S1223;
                points_13[int(4)] = _S1223;
                _S1226 = _S1296;
                _S1227 = _S1295;
                _S1282 = _S1298;
            }
            else
            {
                FixedArray<float3 , 5>  _S1299 = points_13;
                FixedArray<float3 , 5>  _S1300 = points_13;
                FixedArray<float3 , 5>  _S1301 = points_13;
                FixedArray<float3 , 5>  _S1302 = points_13;
                points_13[int(0)] = points_13[int(0)];
                points_13[int(1)] = _S1299[int(1)];
                points_13[int(2)] = _S1300[int(2)];
                points_13[int(3)] = _S1301[int(3)];
                points_13[int(4)] = _S1302[int(4)];
                _S1226 = _S1223;
                _S1227 = _S1223;
                _S1282 = _S1257;
            }
            float3  _S1303 = _S1228 * (points_13[int(1)] + _S1226);
            float _S1304 = _S1303.x + _S1303.y + _S1303.z;
            float3  _S1305 = points_13[int(0)] + _S1227;
            float4  _S1306 = _S1257;
            *&((&_S1306)->y) = _S1304;
            float4  _S1307 = _S1282 + _S1306;
            points_13[int(0)] = _S1223;
            points_13[int(1)] = _S1223;
            points_13[int(2)] = _S1223;
            points_13[int(3)] = _S1223;
            points_13[int(4)] = _S1223;
            _S1226 = _S1305;
            _S1282 = _S1307;
        }
        else
        {
            FixedArray<float3 , 5>  _S1308 = points_13;
            FixedArray<float3 , 5>  _S1309 = points_13;
            FixedArray<float3 , 5>  _S1310 = points_13;
            FixedArray<float3 , 5>  _S1311 = points_13;
            points_13[int(0)] = points_13[int(0)];
            points_13[int(1)] = _S1308[int(1)];
            points_13[int(2)] = _S1309[int(2)];
            points_13[int(3)] = _S1310[int(3)];
            points_13[int(4)] = _S1311[int(4)];
            _S1226 = _S1223;
            _S1282 = _S1257;
        }
        float3  _S1312 = _S1229 * (points_13[int(0)] + _S1226);
        float _S1313 = _S1312.x + _S1312.y + _S1312.z;
        float4  _S1314 = _S1257;
        *&((&_S1314)->x) = _S1313;
        _S1282 = _S1282 + _S1314;
    }
    else
    {
        _S1282 = _S1257;
    }
    *v_depths_5 = _S1282;
    *v_gt_normal_2 = raydir_22;
    return;
}

