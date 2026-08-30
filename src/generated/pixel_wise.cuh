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

inline __device__ float3  linear_rgb_to_srgb(float3  rgb_2, Matrix<float, 3, 3>  color_matrix_0)
{
    float3  _S42 = mul_0(color_matrix_0, rgb_2);
    float _S43 = _S42.x;
    float _S44;
    if(_S43 < 0.00313080009073019f)
    {
        _S44 = _S43 * 12.92000007629394531f;
    }
    else
    {
        _S44 = 1.0549999475479126f * (F32_pow((_S43), (0.4166666567325592f))) - 0.05499999970197678f;
    }
    float _S45 = _S42.y;
    float _S46;
    if(_S45 < 0.00313080009073019f)
    {
        _S46 = _S45 * 12.92000007629394531f;
    }
    else
    {
        _S46 = 1.0549999475479126f * (F32_pow((_S45), (0.4166666567325592f))) - 0.05499999970197678f;
    }
    float _S47 = _S42.z;
    float _S48;
    if(_S47 < 0.00313080009073019f)
    {
        _S48 = _S47 * 12.92000007629394531f;
    }
    else
    {
        _S48 = 1.0549999475479126f * (F32_pow((_S47), (0.4166666567325592f))) - 0.05499999970197678f;
    }
    return make_float3 (_S44, _S46, _S48);
}

inline __device__ float3  s_primal_ctx_mul_0(Matrix<float, 3, 3>  _S49, float3  _S50)
{
    return mul_0(_S49, _S50);
}

inline __device__ void s_bwd_prop_pow_0(DiffPair_float_0 * _S51, DiffPair_float_0 * _S52, float _S53)
{
    _d_pow_0(_S51, _S52, _S53);
    return;
}

inline __device__ void s_bwd_prop_mul_0(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S54, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S55, float3  _S56)
{
    _d_mul_0(_S54, _S55, _S56);
    return;
}

inline __device__ void s_bwd_prop_linear_rgb_to_srgb_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dprgb_0, Matrix<float, 3, 3>  color_matrix_1, float3  _s_dOut_1)
{
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S57 = *dprgb_0;
    float3  _S58 = s_primal_ctx_mul_0(color_matrix_1, (*dprgb_0).primal_0);
    float _S59 = _S58.x;
    float _S60 = _S58.y;
    float _S61 = _S58.z;
    float _S62;
    if(_S61 < 0.00313080009073019f)
    {
        _S62 = 12.92000007629394531f * _s_dOut_1.z;
    }
    else
    {
        float _S63 = 1.0549999475479126f * _s_dOut_1.z;
        DiffPair_float_0 _S64;
        (&_S64)->primal_0 = _S61;
        (&_S64)->differential_0 = 0.0f;
        DiffPair_float_0 _S65;
        (&_S65)->primal_0 = 0.4166666567325592f;
        (&_S65)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S64, &_S65, _S63);
        _S62 = _S64.differential_0;
    }
    float _S66;
    if(_S60 < 0.00313080009073019f)
    {
        _S66 = 12.92000007629394531f * _s_dOut_1.y;
    }
    else
    {
        float _S67 = 1.0549999475479126f * _s_dOut_1.y;
        DiffPair_float_0 _S68;
        (&_S68)->primal_0 = _S60;
        (&_S68)->differential_0 = 0.0f;
        DiffPair_float_0 _S69;
        (&_S69)->primal_0 = 0.4166666567325592f;
        (&_S69)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S68, &_S69, _S67);
        _S66 = _S68.differential_0;
    }
    float _S70;
    if(_S59 < 0.00313080009073019f)
    {
        _S70 = 12.92000007629394531f * _s_dOut_1.x;
    }
    else
    {
        float _S71 = 1.0549999475479126f * _s_dOut_1.x;
        DiffPair_float_0 _S72;
        (&_S72)->primal_0 = _S59;
        (&_S72)->differential_0 = 0.0f;
        DiffPair_float_0 _S73;
        (&_S73)->primal_0 = 0.4166666567325592f;
        (&_S73)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S72, &_S73, _S71);
        _S70 = _S72.differential_0;
    }
    float3  _S74 = make_float3 (_S70, _S66, _S62);
    Matrix<float, 3, 3>  _S75 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S76;
    (&_S76)->primal_0 = color_matrix_1;
    (&_S76)->differential_0 = _S75;
    float3  _S77 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S78;
    (&_S78)->primal_0 = _S57.primal_0;
    (&_S78)->differential_0 = _S77;
    s_bwd_prop_mul_0(&_S76, &_S78, _S74);
    dprgb_0->primal_0 = (*dprgb_0).primal_0;
    dprgb_0->differential_0 = _S78.differential_0;
    return;
}

inline __device__ void s_bwd_linear_rgb_to_srgb_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S79, Matrix<float, 3, 3>  _S80, float3  _S81)
{
    s_bwd_prop_linear_rgb_to_srgb_0(_S79, _S80, _S81);
    return;
}

inline __device__ float3  linear_rgb_to_srgb_bwd(float3  rgb_3, Matrix<float, 3, 3>  color_matrix_2, float3  v_out_rgb_1)
{
    float3  _S82 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 p_rgb_1;
    (&p_rgb_1)->primal_0 = rgb_3;
    (&p_rgb_1)->differential_0 = _S82;
    s_bwd_linear_rgb_to_srgb_0(&p_rgb_1, color_matrix_2, v_out_rgb_1);
    return p_rgb_1.differential_0;
}

inline __device__ float3  rgb_to_srgb(float3  rgb_4, Matrix<float, 3, 3>  color_matrix_3)
{
    float _S83 = rgb_4.x;
    float _S84;
    if(_S83 < 0.04044999927282333f)
    {
        _S84 = _S83 * 0.07739938050508499f;
    }
    else
    {
        _S84 = (F32_pow((0.94786733388900757f * (_S83 + 0.05499999970197678f)), (2.40000009536743164f)));
    }
    float _S85 = rgb_4.y;
    float _S86;
    if(_S85 < 0.04044999927282333f)
    {
        _S86 = _S85 * 0.07739938050508499f;
    }
    else
    {
        _S86 = (F32_pow((0.94786733388900757f * (_S85 + 0.05499999970197678f)), (2.40000009536743164f)));
    }
    float _S87 = rgb_4.z;
    float _S88;
    if(_S87 < 0.04044999927282333f)
    {
        _S88 = _S87 * 0.07739938050508499f;
    }
    else
    {
        _S88 = (F32_pow((0.94786733388900757f * (_S87 + 0.05499999970197678f)), (2.40000009536743164f)));
    }
    float3  _S89 = mul_0(color_matrix_3, make_float3 (_S84, _S86, _S88));
    float _S90 = _S89.x;
    if(_S90 < 0.00313080009073019f)
    {
        _S84 = _S90 * 12.92000007629394531f;
    }
    else
    {
        _S84 = 1.0549999475479126f * (F32_pow((_S90), (0.4166666567325592f))) - 0.05499999970197678f;
    }
    float _S91 = _S89.y;
    if(_S91 < 0.00313080009073019f)
    {
        _S86 = _S91 * 12.92000007629394531f;
    }
    else
    {
        _S86 = 1.0549999475479126f * (F32_pow((_S91), (0.4166666567325592f))) - 0.05499999970197678f;
    }
    float _S92 = _S89.z;
    if(_S92 < 0.00313080009073019f)
    {
        _S88 = _S92 * 12.92000007629394531f;
    }
    else
    {
        _S88 = 1.0549999475479126f * (F32_pow((_S92), (0.4166666567325592f))) - 0.05499999970197678f;
    }
    return make_float3 (_S84, _S86, _S88);
}

inline __device__ float s_primal_ctx_pow_0(float _S93, float _S94)
{
    return (F32_pow((_S93), (_S94)));
}

inline __device__ void s_bwd_prop_rgb_to_srgb_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dprgb_1, Matrix<float, 3, 3>  color_matrix_4, float3  _s_dOut_2)
{
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S95 = *dprgb_1;
    float _S96 = (*dprgb_1).primal_0.x;
    bool _S97 = _S96 < 0.04044999927282333f;
    float _S98;
    if(_S97)
    {
        _S98 = _S96 * 0.07739938050508499f;
    }
    else
    {
        _S98 = s_primal_ctx_pow_0(0.94786733388900757f * (_S96 + 0.05499999970197678f), 2.40000009536743164f);
    }
    float _S99 = _S95.primal_0.y;
    bool _S100 = _S99 < 0.04044999927282333f;
    float _S101;
    if(_S100)
    {
        _S101 = _S99 * 0.07739938050508499f;
    }
    else
    {
        _S101 = s_primal_ctx_pow_0(0.94786733388900757f * (_S99 + 0.05499999970197678f), 2.40000009536743164f);
    }
    float _S102 = _S95.primal_0.z;
    bool _S103 = _S102 < 0.04044999927282333f;
    float _S104;
    if(_S103)
    {
        _S104 = _S102 * 0.07739938050508499f;
    }
    else
    {
        _S104 = s_primal_ctx_pow_0(0.94786733388900757f * (_S102 + 0.05499999970197678f), 2.40000009536743164f);
    }
    float3  _S105 = make_float3 (_S98, _S101, _S104);
    float3  _S106 = s_primal_ctx_mul_0(color_matrix_4, _S105);
    float _S107 = _S106.x;
    float _S108 = _S106.y;
    float _S109 = _S106.z;
    if(_S109 < 0.00313080009073019f)
    {
        _S98 = 12.92000007629394531f * _s_dOut_2.z;
    }
    else
    {
        float _S110 = 1.0549999475479126f * _s_dOut_2.z;
        DiffPair_float_0 _S111;
        (&_S111)->primal_0 = _S109;
        (&_S111)->differential_0 = 0.0f;
        DiffPair_float_0 _S112;
        (&_S112)->primal_0 = 0.4166666567325592f;
        (&_S112)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S111, &_S112, _S110);
        _S98 = _S111.differential_0;
    }
    if(_S108 < 0.00313080009073019f)
    {
        _S101 = 12.92000007629394531f * _s_dOut_2.y;
    }
    else
    {
        float _S113 = 1.0549999475479126f * _s_dOut_2.y;
        DiffPair_float_0 _S114;
        (&_S114)->primal_0 = _S108;
        (&_S114)->differential_0 = 0.0f;
        DiffPair_float_0 _S115;
        (&_S115)->primal_0 = 0.4166666567325592f;
        (&_S115)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S114, &_S115, _S113);
        _S101 = _S114.differential_0;
    }
    if(_S107 < 0.00313080009073019f)
    {
        _S104 = 12.92000007629394531f * _s_dOut_2.x;
    }
    else
    {
        float _S116 = 1.0549999475479126f * _s_dOut_2.x;
        DiffPair_float_0 _S117;
        (&_S117)->primal_0 = _S107;
        (&_S117)->differential_0 = 0.0f;
        DiffPair_float_0 _S118;
        (&_S118)->primal_0 = 0.4166666567325592f;
        (&_S118)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S117, &_S118, _S116);
        _S104 = _S117.differential_0;
    }
    float3  _S119 = make_float3 (_S104, _S101, _S98);
    Matrix<float, 3, 3>  _S120 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S121;
    (&_S121)->primal_0 = color_matrix_4;
    (&_S121)->differential_0 = _S120;
    float3  _S122 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S123;
    (&_S123)->primal_0 = _S105;
    (&_S123)->differential_0 = _S122;
    s_bwd_prop_mul_0(&_S121, &_S123, _S119);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S124 = _S123;
    if(_S103)
    {
        _S98 = 0.0f;
    }
    else
    {
        _S98 = 0.94786733388900757f * (_S102 + 0.05499999970197678f);
    }
    if(_S103)
    {
        _S98 = 0.07739938050508499f * _S124.differential_0.z;
    }
    else
    {
        DiffPair_float_0 _S125;
        (&_S125)->primal_0 = _S98;
        (&_S125)->differential_0 = 0.0f;
        DiffPair_float_0 _S126;
        (&_S126)->primal_0 = 2.40000009536743164f;
        (&_S126)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S125, &_S126, _S124.differential_0.z);
        _S98 = 0.94786733388900757f * _S125.differential_0;
    }
    if(_S100)
    {
        _S101 = 0.0f;
    }
    else
    {
        _S101 = 0.94786733388900757f * (_S99 + 0.05499999970197678f);
    }
    if(_S100)
    {
        _S101 = 0.07739938050508499f * _S124.differential_0.y;
    }
    else
    {
        DiffPair_float_0 _S127;
        (&_S127)->primal_0 = _S101;
        (&_S127)->differential_0 = 0.0f;
        DiffPair_float_0 _S128;
        (&_S128)->primal_0 = 2.40000009536743164f;
        (&_S128)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S127, &_S128, _S124.differential_0.y);
        _S101 = 0.94786733388900757f * _S127.differential_0;
    }
    if(_S97)
    {
        _S104 = 0.0f;
    }
    else
    {
        _S104 = 0.94786733388900757f * (_S96 + 0.05499999970197678f);
    }
    if(_S97)
    {
        _S104 = 0.07739938050508499f * _S124.differential_0.x;
    }
    else
    {
        DiffPair_float_0 _S129;
        (&_S129)->primal_0 = _S104;
        (&_S129)->differential_0 = 0.0f;
        DiffPair_float_0 _S130;
        (&_S130)->primal_0 = 2.40000009536743164f;
        (&_S130)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S129, &_S130, _S124.differential_0.x);
        _S104 = 0.94786733388900757f * _S129.differential_0;
    }
    float3  _S131 = make_float3 (_S104, _S101, _S98);
    dprgb_1->primal_0 = (*dprgb_1).primal_0;
    dprgb_1->differential_0 = _S131;
    return;
}

inline __device__ void s_bwd_rgb_to_srgb_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S132, Matrix<float, 3, 3>  _S133, float3  _S134)
{
    s_bwd_prop_rgb_to_srgb_0(_S132, _S133, _S134);
    return;
}

inline __device__ float3  rgb_to_srgb_bwd(float3  rgb_5, Matrix<float, 3, 3>  color_matrix_5, float3  v_out_rgb_2)
{
    float3  _S135 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 p_rgb_2;
    (&p_rgb_2)->primal_0 = rgb_5;
    (&p_rgb_2)->differential_0 = _S135;
    s_bwd_rgb_to_srgb_0(&p_rgb_2, color_matrix_5, v_out_rgb_2);
    return p_rgb_2.differential_0;
}

inline __device__ void _d_cross_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * a_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * b_0, float3  dOut_5)
{
    float _S136 = dOut_5.y;
    float _S137 = dOut_5.z;
    float _S138 = dOut_5.x;
    float _S139 = (*a_0).primal_0.z * _S136 + - (*a_0).primal_0.y * _S137;
    float _S140 = - (*a_0).primal_0.z * _S138 + (*a_0).primal_0.x * _S137;
    float _S141 = (*a_0).primal_0.y * _S138 + - (*a_0).primal_0.x * _S136;
    float3  _S142 = make_float3 (- (*b_0).primal_0.z * _S136 + (*b_0).primal_0.y * _S137, (*b_0).primal_0.z * _S138 + - (*b_0).primal_0.x * _S137, - (*b_0).primal_0.y * _S138 + (*b_0).primal_0.x * _S136);
    a_0->primal_0 = (*a_0).primal_0;
    a_0->differential_0 = _S142;
    float3  _S143 = make_float3 (_S139, _S140, _S141);
    b_0->primal_0 = (*b_0).primal_0;
    b_0->differential_0 = _S143;
    return;
}

inline __device__ float3  cross_0(float3  left_2, float3  right_2)
{
    float _S144 = left_2.y;
    float _S145 = right_2.z;
    float _S146 = left_2.z;
    float _S147 = right_2.y;
    float _S148 = right_2.x;
    float _S149 = left_2.x;
    return make_float3 (_S144 * _S145 - _S146 * _S147, _S146 * _S148 - _S149 * _S145, _S149 * _S147 - _S144 * _S148);
}

inline __device__ float length_0(float3  x_9)
{
    return (F32_sqrt((dot_0(x_9, x_9))));
}

inline __device__ float length_1(float2  x_10)
{
    return (F32_sqrt((dot_1(x_10, x_10))));
}

inline __device__ float3  points_to_normal(FixedArray<float3 , 4>  points_0)
{
    float3  _S150 = points_0[int(0)];
    bool _S151;
    if((dot_0(_S150, _S150)) == 0.0f)
    {
        _S151 = true;
    }
    else
    {
        float3  _S152 = points_0[int(1)];
        _S151 = (dot_0(_S152, _S152)) == 0.0f;
    }
    if(_S151)
    {
        _S151 = true;
    }
    else
    {
        float3  _S153 = points_0[int(2)];
        _S151 = (dot_0(_S153, _S153)) == 0.0f;
    }
    if(_S151)
    {
        _S151 = true;
    }
    else
    {
        float3  _S154 = points_0[int(3)];
        _S151 = (dot_0(_S154, _S154)) == 0.0f;
    }
    if(_S151)
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

inline __device__ float s_primal_ctx_dot_0(float3  _S155, float3  _S156)
{
    return dot_0(_S155, _S156);
}

inline __device__ float3  s_primal_ctx_cross_0(float3  _S157, float3  _S158)
{
    return cross_0(_S157, _S158);
}

inline __device__ void s_bwd_prop_sqrt_0(DiffPair_float_0 * _S159, float _S160)
{
    _d_sqrt_0(_S159, _S160);
    return;
}

inline __device__ void s_bwd_prop_length_impl_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_5, float _s_dOut_3)
{
    float _S161 = (*dpx_5).primal_0.x;
    float _S162 = (*dpx_5).primal_0.y;
    float _S163 = (*dpx_5).primal_0.z;
    DiffPair_float_0 _S164;
    (&_S164)->primal_0 = _S161 * _S161 + _S162 * _S162 + _S163 * _S163;
    (&_S164)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S164, _s_dOut_3);
    float _S165 = (*dpx_5).primal_0.z * _S164.differential_0;
    float _S166 = _S165 + _S165;
    float _S167 = (*dpx_5).primal_0.y * _S164.differential_0;
    float _S168 = _S167 + _S167;
    float _S169 = (*dpx_5).primal_0.x * _S164.differential_0;
    float _S170 = _S169 + _S169;
    float3  _S171 = make_float3 (0.0f);
    *&((&_S171)->z) = _S166;
    *&((&_S171)->y) = _S168;
    *&((&_S171)->x) = _S170;
    dpx_5->primal_0 = (*dpx_5).primal_0;
    dpx_5->differential_0 = _S171;
    return;
}

inline __device__ void s_bwd_length_impl_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S172, float _S173)
{
    s_bwd_prop_length_impl_0(_S172, _S173);
    return;
}

inline __device__ void s_bwd_prop_dot_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S174, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S175, float _S176)
{
    _d_dot_0(_S174, _S175, _S176);
    return;
}

inline __device__ void s_bwd_prop_cross_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S177, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S178, float3  _S179)
{
    _d_cross_0(_S177, _S178, _S179);
    return;
}

inline __device__ void s_bwd_prop_points_to_normal_0(DiffPair_arrayx3Cvectorx3Cfloatx2C3x3Ex2C4x3E_0 * dppoints_0, float3  _s_dOut_4)
{
    FixedArray<float3 , 4>  _S180 = dppoints_0->primal_0;
    float3  _S181 = make_float3 (0.0f);
    float3  _S182 = dppoints_0->primal_0[int(0)];
    bool _S183 = (s_primal_ctx_dot_0(_S182, _S182)) == 0.0f;
    bool _S184;
    float3  _S185;
    if(_S183)
    {
        _S184 = true;
        _S185 = _S181;
    }
    else
    {
        float3  _S186 = _S180[int(1)];
        _S184 = (s_primal_ctx_dot_0(_S186, _S186)) == 0.0f;
        _S185 = _S180[int(1)];
    }
    bool _S187;
    float3  _S188;
    if(_S184)
    {
        _S187 = true;
        _S188 = _S181;
    }
    else
    {
        float3  _S189 = _S180[int(2)];
        _S187 = (s_primal_ctx_dot_0(_S189, _S189)) == 0.0f;
        _S188 = _S180[int(2)];
    }
    bool _S190;
    float3  _S191;
    if(_S187)
    {
        _S190 = true;
        _S191 = _S181;
    }
    else
    {
        float3  _S192 = _S180[int(3)];
        _S190 = (s_primal_ctx_dot_0(_S192, _S192)) == 0.0f;
        _S191 = _S180[int(3)];
    }
    bool _S193 = !_S190;
    float3  _S194;
    float3  _S195;
    float3  _S196;
    float3  _S197;
    float3  _S198;
    if(_S193)
    {
        float3  dx_0 = _S180[int(1)] - _S180[int(0)];
        float3  _S199 = - (_S180[int(3)] - _S180[int(2)]);
        float3  _S200 = s_primal_ctx_cross_0(dx_0, _S199);
        bool _S201 = (s_primal_ctx_dot_0(_S200, _S200)) != 0.0f;
        if(_S201)
        {
            float _S202 = length_0(_S200);
            float3  _S203 = make_float3 (_S202);
            _S194 = make_float3 (_S202 * _S202);
            _S195 = _S203;
        }
        else
        {
            _S194 = _S181;
            _S195 = _S181;
        }
        float3  _S204 = _S195;
        _S190 = _S201;
        _S195 = _S200;
        _S196 = _S204;
        _S197 = dx_0;
        _S198 = _S199;
    }
    else
    {
        _S190 = false;
        _S194 = _S181;
        _S195 = _S181;
        _S196 = _S181;
        _S197 = _S181;
        _S198 = _S181;
    }
    FixedArray<float3 , 4>  _S205;
    if(_S193)
    {
        if(_S190)
        {
            float3  _S206 = _s_dOut_4 / _S194;
            float3  _S207 = _S195 * - _S206;
            float3  _S208 = _S196 * _S206;
            float _S209 = _S207.x + _S207.y + _S207.z;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S210;
            (&_S210)->primal_0 = _S195;
            (&_S210)->differential_0 = _S181;
            s_bwd_length_impl_0(&_S210, _S209);
            _S194 = _S208 + _S210.differential_0;
        }
        else
        {
            _S194 = _s_dOut_4;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S211;
        (&_S211)->primal_0 = _S195;
        (&_S211)->differential_0 = _S181;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S212;
        (&_S212)->primal_0 = _S195;
        (&_S212)->differential_0 = _S181;
        s_bwd_prop_dot_0(&_S211, &_S212, 0.0f);
        float3  _S213 = _S212.differential_0 + _S211.differential_0 + _S194;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S214;
        (&_S214)->primal_0 = _S197;
        (&_S214)->differential_0 = _S181;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S215;
        (&_S215)->primal_0 = _S198;
        (&_S215)->differential_0 = _S181;
        s_bwd_prop_cross_0(&_S214, &_S215, _S213);
        float3  s_diff_dy_T_0 = - _S215.differential_0;
        float3  _S216 = - s_diff_dy_T_0;
        float3  _S217 = - _S214.differential_0;
        FixedArray<float3 , 4>  _S218;
        _S218[int(0)] = _S181;
        _S218[int(1)] = _S181;
        _S218[int(2)] = _S181;
        _S218[int(3)] = _S181;
        _S218[int(2)] = _S216;
        _S218[int(3)] = s_diff_dy_T_0;
        _S218[int(1)] = _S214.differential_0;
        _S205[int(0)] = _S218[int(0)];
        _S205[int(1)] = _S218[int(1)];
        _S205[int(2)] = _S218[int(2)];
        _S205[int(3)] = _S218[int(3)];
        _S194 = _S217;
    }
    else
    {
        _S205[int(0)] = _S181;
        _S205[int(1)] = _S181;
        _S205[int(2)] = _S181;
        _S205[int(3)] = _S181;
        _S194 = _S181;
    }
    if(_S187)
    {
    }
    else
    {
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S219;
        (&_S219)->primal_0 = _S191;
        (&_S219)->differential_0 = _S181;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S220;
        (&_S220)->primal_0 = _S191;
        (&_S220)->differential_0 = _S181;
        s_bwd_prop_dot_0(&_S219, &_S220, 0.0f);
        float3  _S221 = _S220.differential_0 + _S219.differential_0;
        FixedArray<float3 , 4>  _S222;
        _S222[int(0)] = _S181;
        _S222[int(1)] = _S181;
        _S222[int(2)] = _S181;
        _S222[int(3)] = _S181;
        _S222[int(3)] = _S221;
        float3  _S223 = _S205[int(1)] + _S222[int(1)];
        float3  _S224 = _S205[int(2)] + _S222[int(2)];
        float3  _S225 = _S205[int(3)] + _S222[int(3)];
        _S205[int(0)] = _S205[int(0)] + _S222[int(0)];
        _S205[int(1)] = _S223;
        _S205[int(2)] = _S224;
        _S205[int(3)] = _S225;
    }
    if(_S184)
    {
    }
    else
    {
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S226;
        (&_S226)->primal_0 = _S188;
        (&_S226)->differential_0 = _S181;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S227;
        (&_S227)->primal_0 = _S188;
        (&_S227)->differential_0 = _S181;
        s_bwd_prop_dot_0(&_S226, &_S227, 0.0f);
        float3  _S228 = _S227.differential_0 + _S226.differential_0;
        FixedArray<float3 , 4>  _S229;
        _S229[int(0)] = _S181;
        _S229[int(1)] = _S181;
        _S229[int(2)] = _S181;
        _S229[int(3)] = _S181;
        _S229[int(2)] = _S228;
        float3  _S230 = _S205[int(1)] + _S229[int(1)];
        float3  _S231 = _S205[int(2)] + _S229[int(2)];
        float3  _S232 = _S205[int(3)] + _S229[int(3)];
        _S205[int(0)] = _S205[int(0)] + _S229[int(0)];
        _S205[int(1)] = _S230;
        _S205[int(2)] = _S231;
        _S205[int(3)] = _S232;
    }
    if(_S183)
    {
    }
    else
    {
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S233;
        (&_S233)->primal_0 = _S185;
        (&_S233)->differential_0 = _S181;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S234;
        (&_S234)->primal_0 = _S185;
        (&_S234)->differential_0 = _S181;
        s_bwd_prop_dot_0(&_S233, &_S234, 0.0f);
        float3  _S235 = _S234.differential_0 + _S233.differential_0;
        FixedArray<float3 , 4>  _S236;
        _S236[int(0)] = _S181;
        _S236[int(1)] = _S181;
        _S236[int(2)] = _S181;
        _S236[int(3)] = _S181;
        _S236[int(1)] = _S235;
        float3  _S237 = _S205[int(1)] + _S236[int(1)];
        float3  _S238 = _S205[int(2)] + _S236[int(2)];
        float3  _S239 = _S205[int(3)] + _S236[int(3)];
        _S205[int(0)] = _S205[int(0)] + _S236[int(0)];
        _S205[int(1)] = _S237;
        _S205[int(2)] = _S238;
        _S205[int(3)] = _S239;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S240;
    (&_S240)->primal_0 = _S180[int(0)];
    (&_S240)->differential_0 = _S181;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S241;
    (&_S241)->primal_0 = _S180[int(0)];
    (&_S241)->differential_0 = _S181;
    s_bwd_prop_dot_0(&_S240, &_S241, 0.0f);
    float3  _S242 = _S241.differential_0 + _S240.differential_0 + _S194;
    FixedArray<float3 , 4>  _S243;
    _S243[int(0)] = _S181;
    _S243[int(1)] = _S181;
    _S243[int(2)] = _S181;
    _S243[int(3)] = _S181;
    _S243[int(0)] = _S242;
    FixedArray<float3 , 4>  _S244 = {
        _S205[int(0)] + _S243[int(0)], _S205[int(1)] + _S243[int(1)], _S205[int(2)] + _S243[int(2)], _S205[int(3)] + _S243[int(3)]
    };
    dppoints_0->primal_0 = dppoints_0->primal_0;
    dppoints_0->differential_0 = _S244;
    return;
}

inline __device__ void s_bwd_points_to_normal_0(DiffPair_arrayx3Cvectorx3Cfloatx2C3x3Ex2C4x3E_0 * _S245, float3  _S246)
{
    s_bwd_prop_points_to_normal_0(_S245, _S246);
    return;
}

inline __device__ void points_to_normal_vjp(FixedArray<float3 , 4>  points_1, float3  v_normal_0, FixedArray<float3 , 4>  * v_points_0)
{
    FixedArray<float3 , 4>  _S247 = { make_float3 (0.0f), make_float3 (0.0f), make_float3 (0.0f), make_float3 (0.0f) };
    DiffPair_arrayx3Cvectorx3Cfloatx2C3x3Ex2C4x3E_0 dp_points_0;
    (&dp_points_0)->primal_0 = points_1;
    (&dp_points_0)->differential_0 = _S247;
    s_bwd_points_to_normal_0(&dp_points_0, v_normal_0);
    *v_points_0 = (&dp_points_0)->differential_0;
    return;
}

inline __device__ Matrix<float, 2, 2>  transpose_0(Matrix<float, 2, 2>  x_11)
{
    Matrix<float, 2, 2>  result_7;
    int r_0 = int(0);
    for(;;)
    {
        if(r_0 < int(2))
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
            *_slang_vector_get_element_ptr(((&result_7)->rows + (r_0)), c_1) = _slang_vector_get_element(x_11.rows[c_1], r_0);
            c_1 = c_1 + int(1);
        }
        r_0 = r_0 + int(1);
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
    float _S248 = s_diff_u_0 * u_3;
    float _S249 = s_diff_v_0 * v_1;
    float r2_1 = u_3 * u_3 + v_1 * v_1;
    float s_diff_r2_0 = _S248 + _S248 + (_S249 + _S249);
    float _S250 = (*coeffs_1)[int(0)] + r2_1 * (*coeffs_1)[int(1)];
    float radial_0 = 1.0f + r2_1 * _S250;
    float _S251 = 2.0f * (*coeffs_1)[int(2)];
    float _S252 = _S251 * u_3;
    float _S253 = 2.0f * u_3;
    float _S254 = 2.0f * (*coeffs_1)[int(3)];
    float _S255 = _S254 * u_3;
    float _S256 = 2.0f * v_1;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S257 = { dpuv_0->primal_0 * make_float2 (radial_0) + make_float2 (_S252 * v_1 + (*coeffs_1)[int(3)] * (r2_1 + _S253 * u_3), _S255 * v_1 + (*coeffs_1)[int(2)] * (r2_1 + _S256 * v_1)), dpuv_0->differential_0 * make_float2 (radial_0) + make_float2 (s_diff_r2_0 * _S250 + s_diff_r2_0 * (*coeffs_1)[int(1)] * r2_1) * dpuv_0->primal_0 + make_float2 (s_diff_u_0 * _S251 * v_1 + s_diff_v_0 * _S252 + (s_diff_r2_0 + (s_diff_u_0 * 2.0f * u_3 + s_diff_u_0 * _S253)) * (*coeffs_1)[int(3)], s_diff_u_0 * _S254 * v_1 + s_diff_v_0 * _S255 + (s_diff_r2_0 + (s_diff_v_0 * 2.0f * v_1 + s_diff_v_0 * _S256)) * (*coeffs_1)[int(2)]) };
    return _S257;
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
        float2  _S258 = DistOpenCV_distort_0(q_0, dist_coeffs_1);
        float2  r_1 = _S258 - uv_2;
        float2  _S259 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S260;
        (&_S260)->primal_0 = q_0;
        (&_S260)->differential_0 = _S259;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S261 = s_fwd_DistOpenCV_distort_0(&_S260, dist_coeffs_1);
        float2  _S262 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S263;
        (&_S263)->primal_0 = q_0;
        (&_S263)->differential_0 = _S262;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S264 = s_fwd_DistOpenCV_distort_0(&_S263, dist_coeffs_1);
        Matrix<float, 2, 2>  _S265 = transpose_0(makeMatrix<float, 2, 2> (_S261.differential_0, _S264.differential_0));
        float inv_det_0 = 1.0f / (_S265.rows[int(0)].x * _S265.rows[int(1)].y - _S265.rows[int(0)].y * _S265.rows[int(1)].x);
        float _S266 = r_1.x;
        float _S267 = r_1.y;
        float2  q_1 = q_0 - make_float2 ((_S266 * _S265.rows[int(1)].y - _S267 * _S265.rows[int(0)].y) * inv_det_0, (- _S266 * _S265.rows[int(1)].x + _S267 * _S265.rows[int(0)].x) * inv_det_0);
        i_5 = i_5 + int(1);
        q_0 = q_1;
    }
    *uv_undist_1 = q_0;
    float2  _S268 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S269;
    (&_S269)->primal_0 = q_0;
    (&_S269)->differential_0 = _S268;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S270 = s_fwd_DistOpenCV_distort_0(&_S269, dist_coeffs_1);
    float2  _S271 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S272;
    (&_S272)->primal_0 = q_0;
    (&_S272)->differential_0 = _S271;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S273 = s_fwd_DistOpenCV_distort_0(&_S272, dist_coeffs_1);
    Matrix<float, 2, 2>  _S274 = transpose_0(makeMatrix<float, 2, 2> (_S270.differential_0, _S273.differential_0));
    float _S275 = (F32_min((determinant_0(_S274)), ((F32_min((_S274.rows[int(0)].x), (_S274.rows[int(1)].y))))));
    bool _S276;
    if(_S275 > 0.25f)
    {
        _S276 = _S275 < 4.0f;
    }
    else
    {
        _S276 = false;
    }
    if(_S276)
    {
        float2  _S277 = DistOpenCV_distort_0(q_0, dist_coeffs_1);
        _S276 = (dot_1(q_0, _S277)) >= 0.0f;
    }
    else
    {
        _S276 = false;
    }
    if(_S276)
    {
        float2  _S278 = DistOpenCV_distort_0(*uv_undist_1, dist_coeffs_1);
        _S276 = (length_1(_S278 - uv_2)) < 0.00999999977648258f;
    }
    else
    {
        _S276 = false;
    }
    return _S276;
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
    float _S279 = s_diff_u_1 * u_5;
    float _S280 = s_diff_v_1 * v_3;
    float r2_3 = u_5 * u_5 + v_3 * v_3;
    float s_diff_r2_1 = _S279 + _S279 + (_S280 + _S280);
    float _S281 = (*coeffs_3)[int(2)] + r2_3 * (*coeffs_3)[int(3)];
    float _S282 = (*coeffs_3)[int(1)] + r2_3 * _S281;
    float _S283 = (*coeffs_3)[int(0)] + r2_3 * _S282;
    float radial_1 = 1.0f + r2_3 * _S283;
    float _S284 = 2.0f * (*coeffs_3)[int(4)];
    float _S285 = _S284 * u_5;
    float _S286 = 2.0f * u_5;
    float _S287 = 2.0f * (*coeffs_3)[int(5)];
    float _S288 = _S287 * u_5;
    float _S289 = 2.0f * v_3;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S290 = { dpuv_1->primal_0 * make_float2 (radial_1) + make_float2 (_S285 * v_3 + (*coeffs_3)[int(5)] * (r2_3 + _S286 * u_5) + (*coeffs_3)[int(6)] * r2_3, _S288 * v_3 + (*coeffs_3)[int(4)] * (r2_3 + _S289 * v_3) + (*coeffs_3)[int(7)] * r2_3), dpuv_1->differential_0 * make_float2 (radial_1) + make_float2 (s_diff_r2_1 * _S283 + (s_diff_r2_1 * _S282 + (s_diff_r2_1 * _S281 + s_diff_r2_1 * (*coeffs_3)[int(3)] * r2_3) * r2_3) * r2_3) * dpuv_1->primal_0 + make_float2 (s_diff_u_1 * _S284 * v_3 + s_diff_v_1 * _S285 + (s_diff_r2_1 + (s_diff_u_1 * 2.0f * u_5 + s_diff_u_1 * _S286)) * (*coeffs_3)[int(5)] + s_diff_r2_1 * (*coeffs_3)[int(6)], s_diff_u_1 * _S287 * v_3 + s_diff_v_1 * _S288 + (s_diff_r2_1 + (s_diff_v_1 * 2.0f * v_3 + s_diff_v_1 * _S289)) * (*coeffs_3)[int(4)] + s_diff_r2_1 * (*coeffs_3)[int(7)]) };
    return _S290;
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
        float2  _S291 = DistThinPrism_distort_0(q_2, dist_coeffs_2);
        float2  r_2 = _S291 - uv_4;
        float2  _S292 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S293;
        (&_S293)->primal_0 = q_2;
        (&_S293)->differential_0 = _S292;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S294 = s_fwd_DistThinPrism_distort_0(&_S293, dist_coeffs_2);
        float2  _S295 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S296;
        (&_S296)->primal_0 = q_2;
        (&_S296)->differential_0 = _S295;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S297 = s_fwd_DistThinPrism_distort_0(&_S296, dist_coeffs_2);
        Matrix<float, 2, 2>  _S298 = transpose_0(makeMatrix<float, 2, 2> (_S294.differential_0, _S297.differential_0));
        float inv_det_1 = 1.0f / (_S298.rows[int(0)].x * _S298.rows[int(1)].y - _S298.rows[int(0)].y * _S298.rows[int(1)].x);
        float _S299 = r_2.x;
        float _S300 = r_2.y;
        float2  q_3 = q_2 - make_float2 ((_S299 * _S298.rows[int(1)].y - _S300 * _S298.rows[int(0)].y) * inv_det_1, (- _S299 * _S298.rows[int(1)].x + _S300 * _S298.rows[int(0)].x) * inv_det_1);
        i_6 = i_6 + int(1);
        q_2 = q_3;
    }
    *uv_undist_2 = q_2;
    float2  _S301 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S302;
    (&_S302)->primal_0 = q_2;
    (&_S302)->differential_0 = _S301;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S303 = s_fwd_DistThinPrism_distort_0(&_S302, dist_coeffs_2);
    float2  _S304 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S305;
    (&_S305)->primal_0 = q_2;
    (&_S305)->differential_0 = _S304;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S306 = s_fwd_DistThinPrism_distort_0(&_S305, dist_coeffs_2);
    Matrix<float, 2, 2>  _S307 = transpose_0(makeMatrix<float, 2, 2> (_S303.differential_0, _S306.differential_0));
    float _S308 = (F32_min((determinant_0(_S307)), ((F32_min((_S307.rows[int(0)].x), (_S307.rows[int(1)].y))))));
    bool _S309;
    if(_S308 > 0.25f)
    {
        _S309 = _S308 < 4.0f;
    }
    else
    {
        _S309 = false;
    }
    if(_S309)
    {
        float2  _S310 = DistThinPrism_distort_0(q_2, dist_coeffs_2);
        _S309 = (dot_1(q_2, _S310)) >= 0.0f;
    }
    else
    {
        _S309 = false;
    }
    if(_S309)
    {
        float2  _S311 = DistThinPrism_distort_0(*uv_undist_2, dist_coeffs_2);
        _S309 = (length_1(_S311 - uv_4)) < 0.00999999977648258f;
    }
    else
    {
        _S309 = false;
    }
    return _S309;
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
    float _S312 = s_diff_u_2 * u_7;
    float _S313 = s_diff_v_2 * v_5;
    float r2_5 = u_7 * u_7 + v_5 * v_5;
    float s_diff_r2_2 = _S312 + _S312 + (_S313 + _S313);
    float _S314 = (*coeffs_5)[int(1)] + r2_5 * (*coeffs_5)[int(2)];
    float _S315 = (*coeffs_5)[int(0)] + r2_5 * _S314;
    float _S316 = 1.0f + r2_5 * _S315;
    float _S317 = (*coeffs_5)[int(4)] + r2_5 * (*coeffs_5)[int(5)];
    float _S318 = (*coeffs_5)[int(3)] + r2_5 * _S317;
    float _S319 = 1.0f + r2_5 * _S318;
    float radial_2 = _S316 / _S319;
    float _S320 = 2.0f * (*coeffs_5)[int(6)];
    float _S321 = _S320 * u_7;
    float _S322 = 2.0f * u_7;
    float _S323 = 2.0f * (*coeffs_5)[int(7)];
    float _S324 = _S323 * u_7;
    float _S325 = 2.0f * v_5;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S326 = { dpuv_2->primal_0 * make_float2 (radial_2) + make_float2 (_S321 * v_5 + (*coeffs_5)[int(7)] * (r2_5 + _S322 * u_7), _S324 * v_5 + (*coeffs_5)[int(6)] * (r2_5 + _S325 * v_5)), dpuv_2->differential_0 * make_float2 (radial_2) + make_float2 (((s_diff_r2_2 * _S315 + (s_diff_r2_2 * _S314 + s_diff_r2_2 * (*coeffs_5)[int(2)] * r2_5) * r2_5) * _S319 - _S316 * (s_diff_r2_2 * _S318 + (s_diff_r2_2 * _S317 + s_diff_r2_2 * (*coeffs_5)[int(5)] * r2_5) * r2_5)) / (_S319 * _S319)) * dpuv_2->primal_0 + make_float2 (s_diff_u_2 * _S320 * v_5 + s_diff_v_2 * _S321 + (s_diff_r2_2 + (s_diff_u_2 * 2.0f * u_7 + s_diff_u_2 * _S322)) * (*coeffs_5)[int(7)], s_diff_u_2 * _S323 * v_5 + s_diff_v_2 * _S324 + (s_diff_r2_2 + (s_diff_v_2 * 2.0f * v_5 + s_diff_v_2 * _S325)) * (*coeffs_5)[int(6)]) };
    return _S326;
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
        float2  _S327 = DistRational_distort_0(q_4, dist_coeffs_3);
        float2  r_3 = _S327 - uv_6;
        float2  _S328 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S329;
        (&_S329)->primal_0 = q_4;
        (&_S329)->differential_0 = _S328;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S330 = s_fwd_DistRational_distort_0(&_S329, dist_coeffs_3);
        float2  _S331 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S332;
        (&_S332)->primal_0 = q_4;
        (&_S332)->differential_0 = _S331;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S333 = s_fwd_DistRational_distort_0(&_S332, dist_coeffs_3);
        Matrix<float, 2, 2>  _S334 = transpose_0(makeMatrix<float, 2, 2> (_S330.differential_0, _S333.differential_0));
        float inv_det_2 = 1.0f / (_S334.rows[int(0)].x * _S334.rows[int(1)].y - _S334.rows[int(0)].y * _S334.rows[int(1)].x);
        float _S335 = r_3.x;
        float _S336 = r_3.y;
        float2  q_5 = q_4 - make_float2 ((_S335 * _S334.rows[int(1)].y - _S336 * _S334.rows[int(0)].y) * inv_det_2, (- _S335 * _S334.rows[int(1)].x + _S336 * _S334.rows[int(0)].x) * inv_det_2);
        i_7 = i_7 + int(1);
        q_4 = q_5;
    }
    *uv_undist_3 = q_4;
    float2  _S337 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S338;
    (&_S338)->primal_0 = q_4;
    (&_S338)->differential_0 = _S337;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S339 = s_fwd_DistRational_distort_0(&_S338, dist_coeffs_3);
    float2  _S340 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S341;
    (&_S341)->primal_0 = q_4;
    (&_S341)->differential_0 = _S340;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S342 = s_fwd_DistRational_distort_0(&_S341, dist_coeffs_3);
    Matrix<float, 2, 2>  _S343 = transpose_0(makeMatrix<float, 2, 2> (_S339.differential_0, _S342.differential_0));
    float _S344 = (F32_min((determinant_0(_S343)), ((F32_min((_S343.rows[int(0)].x), (_S343.rows[int(1)].y))))));
    bool _S345;
    if(_S344 > 0.25f)
    {
        _S345 = _S344 < 4.0f;
    }
    else
    {
        _S345 = false;
    }
    if(_S345)
    {
        float2  _S346 = DistRational_distort_0(q_4, dist_coeffs_3);
        _S345 = (dot_1(q_4, _S346)) >= 0.0f;
    }
    else
    {
        _S345 = false;
    }
    if(_S345)
    {
        float2  _S347 = DistRational_distort_0(*uv_undist_3, dist_coeffs_3);
        _S345 = (length_1(_S347 - uv_6)) < 0.00999999977648258f;
    }
    else
    {
        _S345 = false;
    }
    return _S345;
}

inline __device__ float3  normalize_0(float3  x_12)
{
    return x_12 / make_float3 (length_0(x_12));
}

inline __device__ float3  unproject_raydir_0(float2  uv_7, int camera_model_0, bool is_ray_depth_0)
{
    float3  raydir_0;
    bool is_unit_0;
    if(camera_model_0 == int(1))
    {
        float theta_0 = length_1(uv_7);
        float3  _S348 = make_float3 ((uv_7 / make_float2 ((F32_max((theta_0), (1.00000001168609742e-07f)))) * make_float2 ((F32_sin((theta_0))))).x, (uv_7 / make_float2 ((F32_max((theta_0), (1.00000001168609742e-07f)))) * make_float2 ((F32_sin((theta_0))))).y, (F32_cos((theta_0))));
        is_unit_0 = true;
        raydir_0 = _S348;
    }
    else
    {
        bool _S349 = camera_model_0 == int(2);
        if(_S349)
        {
            float r_4 = length_1(uv_7);
            raydir_0 = make_float3 ((uv_7 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_4 * r_4)))))))).x, (uv_7 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_4 * r_4)))))))).y, 1.0f - 0.5f * r_4 * r_4);
        }
        else
        {
            raydir_0 = make_float3 (uv_7.x, uv_7.y, 1.0f);
        }
        is_unit_0 = _S349;
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
    float3  _S350;
    for(;;)
    {
        float2  uv_8 = (pix_pos_0 - float2 {intrins_0.z, intrins_0.w}) / float2 {intrins_0.x, intrins_0.y};
        FixedArray<float, 1>  _S351 = dist_coeffs_4;
        float2  uv_u_0;
        bool _S352 = undistort_point_0(uv_8, &_S351, int(12), &uv_u_0);
        if(!_S352)
        {
            int3  _S353 = make_int3 (int(0));
            float3  _S354 = make_float3 ((float)_S353.x, (float)_S353.y, (float)_S353.z);
            _S350 = _S354;
            break;
        }
        _S350 = unproject_raydir_0(uv_u_0, camera_model_1, is_ray_depth_1);
        break;
    }
    return _S350;
}

inline __device__ float3  depth_to_point_none(float2  pix_pos_1, float4  intrins_1, FixedArray<float, 1>  dist_coeffs_5, int camera_model_2, bool is_ray_depth_2, float depth_2)
{
    float3  _S355;
    for(;;)
    {
        float2  uv_9 = (pix_pos_1 - float2 {intrins_1.z, intrins_1.w}) / float2 {intrins_1.x, intrins_1.y};
        FixedArray<float, 1>  _S356 = dist_coeffs_5;
        float2  uv_u_1;
        bool _S357 = undistort_point_0(uv_9, &_S356, int(12), &uv_u_1);
        if(!_S357)
        {
            _S355 = make_float3 (0.0f);
            break;
        }
        _S355 = make_float3 (depth_2) * unproject_raydir_0(uv_u_1, camera_model_2, is_ray_depth_2);
        break;
    }
    return _S355;
}

struct s_bwd_prop_depth_to_point_Intermediates_0
{
    float2  _S358;
    bool _S359;
};

inline __device__ float s_primal_ctx_sin_0(float _S360)
{
    return (F32_sin((_S360)));
}

inline __device__ float s_primal_ctx_cos_0(float _S361)
{
    return (F32_cos((_S361)));
}

inline __device__ float s_primal_ctx_sqrt_0(float _S362)
{
    return (F32_sqrt((_S362)));
}

inline __device__ float3  s_primal_ctx_unproject_raydir_0(float2  dpuv_3, int camera_model_3, bool is_ray_depth_3)
{
    float3  raydir_1;
    bool is_unit_1;
    if(camera_model_3 == int(1))
    {
        float _S363 = length_1(dpuv_3);
        float3  _S364 = make_float3 ((dpuv_3 / make_float2 ((F32_max((_S363), (1.00000001168609742e-07f)))) * make_float2 (s_primal_ctx_sin_0(_S363))).x, (dpuv_3 / make_float2 ((F32_max((_S363), (1.00000001168609742e-07f)))) * make_float2 (s_primal_ctx_sin_0(_S363))).y, s_primal_ctx_cos_0(_S363));
        is_unit_1 = true;
        raydir_1 = _S364;
    }
    else
    {
        bool _S365 = camera_model_3 == int(2);
        if(_S365)
        {
            float _S366 = length_1(dpuv_3);
            raydir_1 = make_float3 ((dpuv_3 * make_float2 (s_primal_ctx_sqrt_0((F32_max((0.0f), (1.0f - 0.25f * _S366 * _S366)))))).x, (dpuv_3 * make_float2 (s_primal_ctx_sqrt_0((F32_max((0.0f), (1.0f - 0.25f * _S366 * _S366)))))).y, 1.0f - 0.5f * _S366 * _S366);
        }
        else
        {
            raydir_1 = make_float3 (dpuv_3.x, dpuv_3.y, 1.0f);
        }
        is_unit_1 = _S365;
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
    float2  _S367 = make_float2 (0.0f);
    s_bwd_prop_depth_to_point_Intermediates_0 _S368;
    (&_S368)->_S358 = _S367;
    (&_S368)->_S359 = false;
    float2  uv_10 = (pix_pos_2 - float2 {intrins_2.z, intrins_2.w}) / float2 {intrins_2.x, intrins_2.y};
    float2  _S369 = _S367;
    FixedArray<float, 1>  _S370 = dist_coeffs_6;
    bool _S371 = undistort_point_0(uv_10, &_S370, int(12), &_S369);
    (&_S368)->_S358 = _S369;
    (&_S368)->_S359 = _S371;
    s_bwd_prop_depth_to_point_Intermediates_0 _S372 = _S368;
    float3  _S373 = make_float3 (0.0f);
    bool _S374 = !!_S368._S359;
    float3  _S375;
    if(_S374)
    {
        _S375 = s_primal_ctx_unproject_raydir_0(_S372._S358, camera_model_4, is_ray_depth_4);
    }
    else
    {
        _S375 = _S373;
    }
    if(_S374)
    {
        _S375 = _S375 * v_point_0;
    }
    else
    {
        _S375 = _S373;
    }
    return _S375.x + _S375.y + _S375.z;
}

inline __device__ float3  depth_to_normal_none(float2  pix_center_0, float4  intrins_3, FixedArray<float, 1>  dist_coeffs_7, int camera_model_5, bool is_ray_depth_5, float4  depths_0)
{
    float3  normal_2;
    for(;;)
    {
        bool _S376;
        if((depths_0.x) == 0.0f)
        {
            _S376 = true;
        }
        else
        {
            _S376 = (depths_0.y) == 0.0f;
        }
        if(_S376)
        {
            _S376 = true;
        }
        else
        {
            _S376 = (depths_0.z) == 0.0f;
        }
        if(_S376)
        {
            _S376 = true;
        }
        else
        {
            _S376 = (depths_0.w) == 0.0f;
        }
        if(_S376)
        {
            normal_2 = make_float3 (0.0f);
            break;
        }
        float3  * _S377;
        float3  * _S378;
        float3  * _S379;
        float3  * _S380;
        int _S381;
        FixedArray<float3 , 4>  points_2;
        for(;;)
        {
            float2  _S382 = float2 {intrins_3.z, intrins_3.w};
            float2  _S383 = float2 {intrins_3.x, intrins_3.y};
            float2  uv_11 = (pix_center_0 + make_float2 (-1.0f, -0.0f) - _S382) / _S383;
            FixedArray<float, 1>  _S384 = dist_coeffs_7;
            float2  uv_u_2;
            bool _S385 = undistort_point_0(uv_11, &_S384, int(12), &uv_u_2);
            if(!_S385)
            {
                float3  _S386 = make_float3 (0.0f);
                _S381 = int(0);
                _S380 = nullptr;
                _S379 = nullptr;
                _S378 = nullptr;
                _S377 = nullptr;
                normal_2 = _S386;
                break;
            }
            points_2[int(0)] = make_float3 (depths_0.x) * unproject_raydir_0(uv_u_2, camera_model_5, is_ray_depth_5);
            for(;;)
            {
                float2  uv_12 = (pix_center_0 + make_float2 (1.0f, -0.0f) - _S382) / _S383;
                FixedArray<float, 1>  _S387 = dist_coeffs_7;
                float2  uv_u_3;
                bool _S388 = undistort_point_0(uv_12, &_S387, int(12), &uv_u_3);
                if(!_S388)
                {
                    float3  _S389 = make_float3 (0.0f);
                    _S381 = int(0);
                    _S380 = nullptr;
                    normal_2 = _S389;
                    break;
                }
                points_2[int(1)] = make_float3 (depths_0.y) * unproject_raydir_0(uv_u_3, camera_model_5, is_ray_depth_5);
                _S381 = int(2);
                _S380 = &points_2[int(1)];
                break;
            }
            if(_S381 != int(2))
            {
                _S379 = &points_2[int(0)];
                _S378 = nullptr;
                _S377 = nullptr;
                break;
            }
            float2  uv_13 = (pix_center_0 + make_float2 (0.0f, -1.0f) - _S382) / _S383;
            FixedArray<float, 1>  _S390 = dist_coeffs_7;
            float2  uv_u_4;
            bool _S391 = undistort_point_0(uv_13, &_S390, int(12), &uv_u_4);
            if(!_S391)
            {
                float3  _S392 = make_float3 (0.0f);
                _S381 = int(0);
                _S379 = &points_2[int(0)];
                _S378 = nullptr;
                _S377 = nullptr;
                normal_2 = _S392;
                break;
            }
            points_2[int(2)] = make_float3 (depths_0.z) * unproject_raydir_0(uv_u_4, camera_model_5, is_ray_depth_5);
            for(;;)
            {
                float2  uv_14 = (pix_center_0 + make_float2 (0.0f, 1.0f) - _S382) / _S383;
                FixedArray<float, 1>  _S393 = dist_coeffs_7;
                float2  uv_u_5;
                bool _S394 = undistort_point_0(uv_14, &_S393, int(12), &uv_u_5);
                if(!_S394)
                {
                    float3  _S395 = make_float3 (0.0f);
                    _S381 = int(0);
                    _S379 = nullptr;
                    normal_2 = _S395;
                    break;
                }
                points_2[int(3)] = make_float3 (depths_0.w) * unproject_raydir_0(uv_u_5, camera_model_5, is_ray_depth_5);
                _S381 = int(2);
                _S379 = &points_2[int(3)];
                break;
            }
            if(_S381 != int(2))
            {
                float3  * _S396 = _S379;
                _S379 = &points_2[int(0)];
                _S378 = _S396;
                _S377 = &points_2[int(2)];
                break;
            }
            float3  * _S397 = _S379;
            _S381 = int(1);
            _S379 = &points_2[int(0)];
            _S378 = _S397;
            _S377 = &points_2[int(2)];
            break;
        }
        if(_S381 != int(1))
        {
            break;
        }
        float3  normal_3 = cross_0(*_S380 - *_S379, - (*_S378 - *_S377));
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
    float2  _S398;
    bool _S399;
    float2  _S400;
    bool _S401;
    float2  _S402;
    bool _S403;
    float2  _S404;
    bool _S405;
};

inline __device__ void depth_to_normal_vjp_none(float2  pix_center_1, float4  intrins_4, FixedArray<float, 1>  dist_coeffs_8, int camera_model_6, bool is_ray_depth_6, float4  depths_1, float3  v_normal_1, float4  * v_depths_0)
{
    float2  _S406 = make_float2 (0.0f);
    s_bwd_prop_depth_to_normal_Intermediates_0 _S407;
    (&_S407)->_S398 = _S406;
    (&_S407)->_S399 = false;
    (&_S407)->_S400 = _S406;
    (&_S407)->_S401 = false;
    (&_S407)->_S402 = _S406;
    (&_S407)->_S403 = false;
    (&_S407)->_S404 = _S406;
    (&_S407)->_S405 = false;
    (&_S407)->_S398 = _S406;
    (&_S407)->_S399 = false;
    (&_S407)->_S400 = _S406;
    (&_S407)->_S401 = false;
    (&_S407)->_S402 = _S406;
    (&_S407)->_S403 = false;
    (&_S407)->_S404 = _S406;
    (&_S407)->_S405 = false;
    bool _S408 = (depths_1.x) == 0.0f;
    bool _runFlag_0;
    if(_S408)
    {
        _runFlag_0 = true;
    }
    else
    {
        _runFlag_0 = (depths_1.y) == 0.0f;
    }
    if(_runFlag_0)
    {
        _runFlag_0 = true;
    }
    else
    {
        _runFlag_0 = (depths_1.z) == 0.0f;
    }
    if(_runFlag_0)
    {
        _runFlag_0 = true;
    }
    else
    {
        _runFlag_0 = (depths_1.w) == 0.0f;
    }
    int _S409;
    if(!_runFlag_0)
    {
        float2  _S410 = float2 {intrins_4.z, intrins_4.w};
        float2  _S411 = float2 {intrins_4.x, intrins_4.y};
        float2  uv_15 = (pix_center_1 + make_float2 (-1.0f, -0.0f) - _S410) / _S411;
        float2  _S412 = _S406;
        FixedArray<float, 1>  _S413 = dist_coeffs_8;
        bool _S414 = undistort_point_0(uv_15, &_S413, int(12), &_S412);
        (&_S407)->_S398 = _S412;
        (&_S407)->_S399 = _S414;
        bool _S415 = !!_S414;
        if(_S415)
        {
            float2  uv_16 = (pix_center_1 + make_float2 (1.0f, -0.0f) - _S410) / _S411;
            float2  _S416 = _S406;
            FixedArray<float, 1>  _S417 = dist_coeffs_8;
            bool _S418 = undistort_point_0(uv_16, &_S417, int(12), &_S416);
            (&_S407)->_S400 = _S416;
            (&_S407)->_S401 = _S418;
            if(!!_S418)
            {
                _S409 = int(2);
            }
            else
            {
                _S409 = int(0);
            }
            if(_S409 != int(2))
            {
                _runFlag_0 = false;
            }
            else
            {
                _runFlag_0 = _S415;
            }
            if(_runFlag_0)
            {
                float2  uv_17 = (pix_center_1 + make_float2 (0.0f, -1.0f) - _S410) / _S411;
                float2  _S419 = _S406;
                FixedArray<float, 1>  _S420 = dist_coeffs_8;
                bool _S421 = undistort_point_0(uv_17, &_S420, int(12), &_S419);
                (&_S407)->_S402 = _S419;
                (&_S407)->_S403 = _S421;
                if(!_S421)
                {
                    _runFlag_0 = false;
                }
                if(_runFlag_0)
                {
                    float2  uv_18 = (pix_center_1 + make_float2 (0.0f, 1.0f) - _S410) / _S411;
                    float2  _S422 = _S406;
                    FixedArray<float, 1>  _S423 = dist_coeffs_8;
                    bool _S424 = undistort_point_0(uv_18, &_S423, int(12), &_S422);
                    (&_S407)->_S404 = _S422;
                    (&_S407)->_S405 = _S424;
                }
            }
        }
    }
    s_bwd_prop_depth_to_normal_Intermediates_0 _S425 = _S407;
    float3  _S426 = make_float3 (0.0f);
    if(_S408)
    {
        _runFlag_0 = true;
    }
    else
    {
        _runFlag_0 = (depths_1.y) == 0.0f;
    }
    if(_runFlag_0)
    {
        _runFlag_0 = true;
    }
    else
    {
        _runFlag_0 = (depths_1.z) == 0.0f;
    }
    if(_runFlag_0)
    {
        _runFlag_0 = true;
    }
    else
    {
        _runFlag_0 = (depths_1.w) == 0.0f;
    }
    bool _S427 = !_runFlag_0;
    bool _runFlag_1;
    bool _runFlag_2;
    bool _S428;
    bool _runFlag_3;
    bool _S429;
    bool _S430;
    FixedArray<float3 , 4>  points_3;
    float3  _S431;
    float3  _S432;
    float3  _S433;
    float3  _S434;
    float3  _S435;
    float3  _S436;
    float3  _S437;
    float3  _S438;
    float3  _S439;
    if(_S427)
    {
        bool _S440 = !!_S425._S399;
        if(_S440)
        {
            float3  _S441 = s_primal_ctx_unproject_raydir_0(_S425._S398, camera_model_6, is_ray_depth_6);
            float3  _S442 = make_float3 (depths_1.x) * _S441;
            bool _S443 = !!_S425._S401;
            if(_S443)
            {
                float3  _S444 = s_primal_ctx_unproject_raydir_0(_S425._S400, camera_model_6, is_ray_depth_6);
                float3  _S445 = make_float3 (depths_1.y) * _S444;
                _S409 = int(2);
                points_3[int(0)] = _S442;
                points_3[int(1)] = _S445;
                points_3[int(2)] = _S426;
                points_3[int(3)] = _S426;
                _S431 = _S444;
            }
            else
            {
                _S409 = int(0);
                points_3[int(0)] = _S442;
                points_3[int(1)] = _S426;
                points_3[int(2)] = _S426;
                points_3[int(3)] = _S426;
                _S431 = _S426;
            }
            if(_S409 != int(2))
            {
                _runFlag_0 = false;
            }
            else
            {
                _runFlag_0 = _S440;
                _S409 = int(0);
            }
            if(_runFlag_0)
            {
                if(!_S425._S403)
                {
                    _runFlag_1 = false;
                    _S409 = int(0);
                }
                else
                {
                    _runFlag_1 = _runFlag_0;
                }
                if(_runFlag_1)
                {
                    float3  _S446 = s_primal_ctx_unproject_raydir_0(_S425._S402, camera_model_6, is_ray_depth_6);
                    points_3[int(2)] = make_float3 (depths_1.z) * _S446;
                    bool _S447 = !!_S425._S405;
                    int _S448;
                    if(_S447)
                    {
                        float3  _S449 = s_primal_ctx_unproject_raydir_0(_S425._S404, camera_model_6, is_ray_depth_6);
                        points_3[int(3)] = make_float3 (depths_1.w) * _S449;
                        _S448 = int(2);
                        _S432 = _S449;
                    }
                    else
                    {
                        _S448 = int(0);
                        _S432 = _S426;
                    }
                    if(_S448 != int(2))
                    {
                        _runFlag_2 = false;
                        _S409 = _S448;
                    }
                    else
                    {
                        _runFlag_2 = _runFlag_1;
                    }
                    if(_runFlag_2)
                    {
                        _S409 = int(1);
                    }
                    _runFlag_2 = _S447;
                    _S433 = _S446;
                }
                else
                {
                    _runFlag_2 = false;
                    _S432 = _S426;
                    _S433 = _S426;
                }
            }
            else
            {
                _runFlag_1 = false;
                _runFlag_2 = false;
                _S432 = _S426;
                _S433 = _S426;
            }
            float3  _S450 = _S431;
            _S431 = _S432;
            _S432 = _S433;
            _S428 = _S443;
            _S433 = _S450;
            _S434 = _S441;
        }
        else
        {
            _S409 = int(0);
            points_3[int(0)] = _S426;
            points_3[int(1)] = _S426;
            points_3[int(2)] = _S426;
            points_3[int(3)] = _S426;
            _runFlag_0 = false;
            _runFlag_1 = false;
            _runFlag_2 = false;
            _S431 = _S426;
            _S432 = _S426;
            _S428 = false;
            _S433 = _S426;
            _S434 = _S426;
        }
        if(_S409 != int(1))
        {
            _runFlag_3 = false;
        }
        else
        {
            _runFlag_3 = _S427;
        }
        if(_runFlag_3)
        {
            float3  dx_1 = points_3[int(1)] - points_3[int(0)];
            float3  _S451 = - (points_3[int(3)] - points_3[int(2)]);
            float3  _S452 = s_primal_ctx_cross_0(dx_1, _S451);
            bool _S453 = (s_primal_ctx_dot_0(_S452, _S452)) != 0.0f;
            if(_S453)
            {
                float _S454 = length_0(_S452);
                float3  _S455 = make_float3 (_S454);
                _S435 = make_float3 (_S454 * _S454);
                _S436 = _S455;
            }
            else
            {
                _S435 = _S426;
                _S436 = _S426;
            }
            float3  _S456 = _S436;
            _S429 = _S453;
            _S436 = _S452;
            _S437 = _S456;
            _S438 = dx_1;
            _S439 = _S451;
        }
        else
        {
            _S429 = false;
            _S435 = _S426;
            _S436 = _S426;
            _S437 = _S426;
            _S438 = _S426;
            _S439 = _S426;
        }
        bool _S457 = _runFlag_0;
        bool _S458 = _runFlag_1;
        bool _S459 = _runFlag_2;
        float3  _S460 = _S431;
        float3  _S461 = _S432;
        bool _S462 = _S428;
        float3  _S463 = _S433;
        float3  _S464 = _S434;
        _runFlag_0 = _runFlag_3;
        _runFlag_1 = _S429;
        _S431 = _S435;
        _S432 = _S436;
        _S433 = _S437;
        _S434 = _S438;
        _S435 = _S439;
        _runFlag_2 = _S440;
        _S428 = _S457;
        _runFlag_3 = _S458;
        _S429 = _S459;
        _S436 = _S460;
        _S437 = _S461;
        _S430 = _S462;
        _S438 = _S463;
        _S439 = _S464;
    }
    else
    {
        _runFlag_0 = false;
        _runFlag_1 = false;
        _S431 = _S426;
        _S432 = _S426;
        _S433 = _S426;
        _S434 = _S426;
        _S435 = _S426;
        _runFlag_2 = false;
        _S428 = false;
        _runFlag_3 = false;
        _S429 = false;
        _S436 = _S426;
        _S437 = _S426;
        _S430 = false;
        _S438 = _S426;
        _S439 = _S426;
    }
    float4  _S465 = make_float4 (0.0f);
    float4  _S466;
    if(_S427)
    {
        if(_runFlag_0)
        {
            if(_runFlag_1)
            {
                float3  _S467 = v_normal_1 / _S431;
                float3  _S468 = _S432 * - _S467;
                float3  _S469 = _S433 * _S467;
                float _S470 = _S468.x + _S468.y + _S468.z;
                DiffPair_vectorx3Cfloatx2C3x3E_0 _S471;
                (&_S471)->primal_0 = _S432;
                (&_S471)->differential_0 = _S426;
                s_bwd_length_impl_0(&_S471, _S470);
                _S431 = _S469 + _S471.differential_0;
            }
            else
            {
                _S431 = v_normal_1;
            }
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S472;
            (&_S472)->primal_0 = _S432;
            (&_S472)->differential_0 = _S426;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S473;
            (&_S473)->primal_0 = _S432;
            (&_S473)->differential_0 = _S426;
            s_bwd_prop_dot_0(&_S472, &_S473, 0.0f);
            float3  _S474 = _S473.differential_0 + _S472.differential_0 + _S431;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S475;
            (&_S475)->primal_0 = _S434;
            (&_S475)->differential_0 = _S426;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S476;
            (&_S476)->primal_0 = _S435;
            (&_S476)->differential_0 = _S426;
            s_bwd_prop_cross_0(&_S475, &_S476, _S474);
            float3  s_diff_dy_T_1 = - _S476.differential_0;
            float3  _S477 = - s_diff_dy_T_1;
            float3  _S478 = - _S475.differential_0;
            FixedArray<float3 , 4>  _S479;
            _S479[int(0)] = _S426;
            _S479[int(1)] = _S426;
            _S479[int(2)] = _S426;
            _S479[int(3)] = _S426;
            _S479[int(2)] = _S477;
            _S479[int(3)] = s_diff_dy_T_1;
            _S479[int(0)] = _S478;
            _S479[int(1)] = _S475.differential_0;
            points_3[int(0)] = _S479[int(0)];
            points_3[int(1)] = _S479[int(1)];
            points_3[int(2)] = _S479[int(2)];
            points_3[int(3)] = _S479[int(3)];
        }
        else
        {
            points_3[int(0)] = _S426;
            points_3[int(1)] = _S426;
            points_3[int(2)] = _S426;
            points_3[int(3)] = _S426;
        }
        if(_runFlag_2)
        {
            if(_S428)
            {
                if(_runFlag_3)
                {
                    FixedArray<float3 , 4>  _S480 = points_3;
                    FixedArray<float3 , 4>  _S481 = points_3;
                    FixedArray<float3 , 4>  _S482 = points_3;
                    FixedArray<float3 , 4>  _S483 = points_3;
                    if(_S429)
                    {
                        float3  _S484 = _S436 * _S483[int(3)];
                        float _S485 = _S484.x + _S484.y + _S484.z;
                        float4  _S486 = _S465;
                        *&((&_S486)->w) = _S485;
                        points_3[int(0)] = _S480[int(0)];
                        points_3[int(1)] = _S481[int(1)];
                        points_3[int(2)] = _S482[int(2)];
                        points_3[int(3)] = _S426;
                        _S466 = _S486;
                    }
                    else
                    {
                        points_3[int(0)] = _S480[int(0)];
                        points_3[int(1)] = _S481[int(1)];
                        points_3[int(2)] = _S482[int(2)];
                        points_3[int(3)] = _S483[int(3)];
                        _S466 = _S465;
                    }
                    float3  _S487 = _S437 * points_3[int(2)];
                    float _S488 = _S487.x + _S487.y + _S487.z;
                    FixedArray<float3 , 4>  _S489 = points_3;
                    FixedArray<float3 , 4>  _S490 = points_3;
                    float4  _S491 = _S465;
                    *&((&_S491)->z) = _S488;
                    float4  _S492 = _S466 + _S491;
                    points_3[int(0)] = points_3[int(0)];
                    points_3[int(1)] = _S489[int(1)];
                    points_3[int(2)] = _S426;
                    points_3[int(3)] = _S490[int(3)];
                    _S466 = _S492;
                }
                else
                {
                    FixedArray<float3 , 4>  _S493 = points_3;
                    FixedArray<float3 , 4>  _S494 = points_3;
                    FixedArray<float3 , 4>  _S495 = points_3;
                    points_3[int(0)] = points_3[int(0)];
                    points_3[int(1)] = _S493[int(1)];
                    points_3[int(2)] = _S494[int(2)];
                    points_3[int(3)] = _S495[int(3)];
                    _S466 = _S465;
                }
            }
            else
            {
                FixedArray<float3 , 4>  _S496 = points_3;
                FixedArray<float3 , 4>  _S497 = points_3;
                FixedArray<float3 , 4>  _S498 = points_3;
                points_3[int(0)] = points_3[int(0)];
                points_3[int(1)] = _S496[int(1)];
                points_3[int(2)] = _S497[int(2)];
                points_3[int(3)] = _S498[int(3)];
                _S466 = _S465;
            }
            if(_S430)
            {
                FixedArray<float3 , 4>  _S499 = points_3;
                float3  _S500 = _S438 * points_3[int(1)];
                float _S501 = _S500.x + _S500.y + _S500.z;
                float4  _S502 = _S465;
                *&((&_S502)->y) = _S501;
                float4  _S503 = _S466 + _S502;
                points_3[int(0)] = _S426;
                points_3[int(1)] = _S426;
                points_3[int(2)] = _S426;
                points_3[int(3)] = _S426;
                _S431 = _S499[int(0)];
                _S466 = _S503;
            }
            else
            {
                FixedArray<float3 , 4>  _S504 = points_3;
                FixedArray<float3 , 4>  _S505 = points_3;
                FixedArray<float3 , 4>  _S506 = points_3;
                points_3[int(0)] = points_3[int(0)];
                points_3[int(1)] = _S504[int(1)];
                points_3[int(2)] = _S505[int(2)];
                points_3[int(3)] = _S506[int(3)];
                _S431 = _S426;
            }
            float3  _S507 = _S439 * (points_3[int(0)] + _S431);
            float _S508 = _S507.x + _S507.y + _S507.z;
            float4  _S509 = _S465;
            *&((&_S509)->x) = _S508;
            _S466 = _S466 + _S509;
        }
        else
        {
            _S466 = _S465;
        }
    }
    else
    {
        _S466 = _S465;
    }
    *v_depths_0 = _S466;
    return;
}

inline __device__ float ray_depth_to_linear_depth_factor_none(float2  pix_center_2, float4  intrins_5, FixedArray<float, 1>  dist_coeffs_9, int camera_model_7)
{
    float _S510;
    for(;;)
    {
        float2  uv_19 = (pix_center_2 - float2 {intrins_5.z, intrins_5.w}) / float2 {intrins_5.x, intrins_5.y};
        FixedArray<float, 1>  _S511 = dist_coeffs_9;
        float2  uv_u_6;
        bool _S512 = undistort_point_0(uv_19, &_S511, int(12), &uv_u_6);
        if(!_S512)
        {
            _S510 = 0.0f;
            break;
        }
        float3  raydir_2 = unproject_raydir_0(uv_u_6, camera_model_7, false);
        _S510 = float((F32_sign((raydir_2.z)))) / length_0(raydir_2);
        break;
    }
    return _S510;
}

inline __device__ float depth_normal_loss_none(float2  pix_center_3, float4  intrins_6, FixedArray<float, 1>  dist_coeffs_10, int camera_model_8, bool is_ray_depth_7, float4  depths_2, float3  gt_normal_0)
{
    float _S513;
    for(;;)
    {
        float3  _S514;
        float3  * _S515;
        float3  * _S516;
        float3  * _S517;
        float3  * _S518;
        int _S519;
        FixedArray<float3 , 5>  points_4;
        for(;;)
        {
            float2  _S520 = float2 {intrins_6.z, intrins_6.w};
            float2  _S521 = float2 {intrins_6.x, intrins_6.y};
            float2  uv_20 = (pix_center_3 + make_float2 (-1.0f, -0.0f) - _S520) / _S521;
            FixedArray<float, 1>  _S522 = dist_coeffs_10;
            float2  uv_u_7;
            bool _S523 = undistort_point_0(uv_20, &_S522, int(12), &uv_u_7);
            float3  _S524 = make_float3 (0.0f);
            if(!_S523)
            {
                _S519 = int(0);
                _S518 = nullptr;
                _S517 = nullptr;
                _S516 = nullptr;
                _S515 = nullptr;
                _S514 = _S524;
                break;
            }
            float3  raydir_3 = unproject_raydir_0(uv_u_7, camera_model_8, is_ray_depth_7);
            points_4[int(0)] = make_float3 (depths_2.x) * raydir_3;
            float2  uv_21 = (pix_center_3 + make_float2 (1.0f, -0.0f) - _S520) / _S521;
            FixedArray<float, 1>  _S525 = dist_coeffs_10;
            float2  uv_u_8;
            bool _S526 = undistort_point_0(uv_21, &_S525, int(12), &uv_u_8);
            if(!_S526)
            {
                _S519 = int(0);
                _S518 = nullptr;
                _S517 = &points_4[int(0)];
                _S516 = nullptr;
                _S515 = nullptr;
                _S514 = _S524;
                break;
            }
            float3  raydir_4 = unproject_raydir_0(uv_u_8, camera_model_8, is_ray_depth_7);
            points_4[int(1)] = make_float3 (depths_2.y) * raydir_4;
            float2  uv_22 = (pix_center_3 + make_float2 (0.0f, -1.0f) - _S520) / _S521;
            FixedArray<float, 1>  _S527 = dist_coeffs_10;
            float2  uv_u_9;
            bool _S528 = undistort_point_0(uv_22, &_S527, int(12), &uv_u_9);
            if(!_S528)
            {
                _S519 = int(0);
                _S518 = &points_4[int(1)];
                _S517 = &points_4[int(0)];
                _S516 = nullptr;
                _S515 = nullptr;
                _S514 = _S524;
                break;
            }
            float3  raydir_5 = unproject_raydir_0(uv_u_9, camera_model_8, is_ray_depth_7);
            points_4[int(2)] = make_float3 (depths_2.z) * raydir_5;
            float2  uv_23 = (pix_center_3 + make_float2 (0.0f, 1.0f) - _S520) / _S521;
            FixedArray<float, 1>  _S529 = dist_coeffs_10;
            float2  uv_u_10;
            bool _S530 = undistort_point_0(uv_23, &_S529, int(12), &uv_u_10);
            if(!_S530)
            {
                _S519 = int(0);
                _S518 = &points_4[int(1)];
                _S517 = &points_4[int(0)];
                _S516 = nullptr;
                _S515 = &points_4[int(2)];
                _S514 = _S524;
                break;
            }
            float3  raydir_6 = unproject_raydir_0(uv_u_10, camera_model_8, is_ray_depth_7);
            points_4[int(3)] = make_float3 (depths_2.w) * raydir_6;
            float2  uv_24 = (pix_center_3 + make_float2 (0.0f) * make_float2 (0.0f, 3.0f) - _S520) / _S521;
            FixedArray<float, 1>  _S531 = dist_coeffs_10;
            float2  uv_u_11;
            bool _S532 = undistort_point_0(uv_24, &_S531, int(12), &uv_u_11);
            if(!_S532)
            {
                _S519 = int(0);
                _S518 = &points_4[int(1)];
                _S517 = &points_4[int(0)];
                _S516 = &points_4[int(3)];
                _S515 = &points_4[int(2)];
                _S514 = _S524;
                break;
            }
            float3  raydir_7 = unproject_raydir_0(uv_u_11, camera_model_8, is_ray_depth_7);
            _S519 = int(1);
            _S518 = &points_4[int(1)];
            _S517 = &points_4[int(0)];
            _S516 = &points_4[int(3)];
            _S515 = &points_4[int(2)];
            _S514 = raydir_7;
            break;
        }
        if(_S519 != int(1))
        {
            _S513 = 0.0f;
            break;
        }
        float3  normal_4 = cross_0(*_S518 - *_S517, - (*_S516 - *_S515));
        float3  normal_5;
        if((dot_0(normal_4, normal_4)) != 0.0f)
        {
            normal_5 = normalize_0(normal_4);
        }
        else
        {
            normal_5 = normal_4;
        }
        float3  _S533;
        if((dot_0(gt_normal_0, gt_normal_0)) != 0.0f)
        {
            _S533 = normalize_0(gt_normal_0);
        }
        else
        {
            _S533 = gt_normal_0;
        }
        _S513 = (1.0f - dot_0(normal_5, _S533) + 0.00100000004749745f) / ((F32_max((dot_0(normal_5, - normalize_0(_S514))), (0.0f))) + 0.00100000004749745f);
        break;
    }
    return _S513;
}

struct s_bwd_prop_depth_normal_loss_Intermediates_0
{
    float2  _S534;
    bool _S535;
    float2  _S536;
    bool _S537;
    float2  _S538;
    bool _S539;
    float2  _S540;
    bool _S541;
    float2  _S542;
    bool _S543;
};

inline __device__ void s_bwd_prop_normalize_impl_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_6, float3  _s_dOut_5)
{
    float _S544 = length_0((*dpx_6).primal_0);
    float3  _S545 = (*dpx_6).primal_0 * _s_dOut_5;
    float3  _S546 = make_float3 (1.0f / _S544) * _s_dOut_5;
    float _S547 = - ((_S545.x + _S545.y + _S545.z) / (_S544 * _S544));
    float3  _S548 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S549;
    (&_S549)->primal_0 = (*dpx_6).primal_0;
    (&_S549)->differential_0 = _S548;
    s_bwd_length_impl_0(&_S549, _S547);
    float3  _S550 = _S546 + _S549.differential_0;
    dpx_6->primal_0 = (*dpx_6).primal_0;
    dpx_6->differential_0 = _S550;
    return;
}

inline __device__ void s_bwd_normalize_impl_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S551, float3  _S552)
{
    s_bwd_prop_normalize_impl_0(_S551, _S552);
    return;
}

inline __device__ void depth_normal_loss_vjp_none(float2  pix_center_4, float4  intrins_7, FixedArray<float, 1>  dist_coeffs_11, int camera_model_9, bool is_ray_depth_8, float4  depths_3, float3  gt_normal_1, float v_loss_0, float4  * v_depths_1, float3  * v_gt_normal_0)
{
    float2  _S553 = make_float2 (0.0f);
    s_bwd_prop_depth_normal_loss_Intermediates_0 _S554;
    (&_S554)->_S534 = _S553;
    (&_S554)->_S535 = false;
    (&_S554)->_S536 = _S553;
    (&_S554)->_S537 = false;
    (&_S554)->_S538 = _S553;
    (&_S554)->_S539 = false;
    (&_S554)->_S540 = _S553;
    (&_S554)->_S541 = false;
    (&_S554)->_S542 = _S553;
    (&_S554)->_S543 = false;
    (&_S554)->_S536 = _S553;
    (&_S554)->_S537 = false;
    (&_S554)->_S538 = _S553;
    (&_S554)->_S539 = false;
    (&_S554)->_S540 = _S553;
    (&_S554)->_S541 = false;
    (&_S554)->_S542 = _S553;
    (&_S554)->_S543 = false;
    float2  _S555 = float2 {intrins_7.z, intrins_7.w};
    float2  _S556 = float2 {intrins_7.x, intrins_7.y};
    float2  uv_25 = (pix_center_4 + make_float2 (-1.0f, -0.0f) - _S555) / _S556;
    float2  _S557 = _S553;
    FixedArray<float, 1>  _S558 = dist_coeffs_11;
    bool _S559 = undistort_point_0(uv_25, &_S558, int(12), &_S557);
    (&_S554)->_S534 = _S557;
    (&_S554)->_S535 = _S559;
    bool _S560 = !!_S559;
    bool _runFlag_4;
    if(_S560)
    {
        float2  uv_26 = (pix_center_4 + make_float2 (1.0f, -0.0f) - _S555) / _S556;
        float2  _S561 = _S553;
        FixedArray<float, 1>  _S562 = dist_coeffs_11;
        bool _S563 = undistort_point_0(uv_26, &_S562, int(12), &_S561);
        (&_S554)->_S536 = _S561;
        (&_S554)->_S537 = _S563;
        if(!_S563)
        {
            _runFlag_4 = false;
        }
        else
        {
            _runFlag_4 = _S560;
        }
        if(_runFlag_4)
        {
            float2  uv_27 = (pix_center_4 + make_float2 (0.0f, -1.0f) - _S555) / _S556;
            float2  _S564 = _S553;
            FixedArray<float, 1>  _S565 = dist_coeffs_11;
            bool _S566 = undistort_point_0(uv_27, &_S565, int(12), &_S564);
            (&_S554)->_S538 = _S564;
            (&_S554)->_S539 = _S566;
            if(!_S566)
            {
                _runFlag_4 = false;
            }
            if(_runFlag_4)
            {
                float2  uv_28 = (pix_center_4 + make_float2 (0.0f, 1.0f) - _S555) / _S556;
                float2  _S567 = _S553;
                FixedArray<float, 1>  _S568 = dist_coeffs_11;
                bool _S569 = undistort_point_0(uv_28, &_S568, int(12), &_S567);
                (&_S554)->_S540 = _S567;
                (&_S554)->_S541 = _S569;
                if(!_S569)
                {
                    _runFlag_4 = false;
                }
                if(_runFlag_4)
                {
                    float2  uv_29 = (pix_center_4 - _S555) / _S556;
                    float2  _S570 = _S553;
                    FixedArray<float, 1>  _S571 = dist_coeffs_11;
                    bool _S572 = undistort_point_0(uv_29, &_S571, int(12), &_S570);
                    (&_S554)->_S542 = _S570;
                    (&_S554)->_S543 = _S572;
                }
            }
        }
    }
    s_bwd_prop_depth_normal_loss_Intermediates_0 _S573 = _S554;
    float3  _S574 = make_float3 (0.0f);
    bool _S575 = !!_S554._S535;
    bool _runFlag_5;
    bool _runFlag_6;
    bool _runFlag_7;
    int _S576;
    float3  raydir_8;
    float3  _S577;
    float3  _S578;
    float3  _S579;
    float3  _S580;
    FixedArray<float3 , 5>  points_5;
    if(_S575)
    {
        float3  _S581 = s_primal_ctx_unproject_raydir_0(_S573._S534, camera_model_9, is_ray_depth_8);
        float3  _S582 = make_float3 (depths_3.x) * _S581;
        if(!_S573._S537)
        {
            _runFlag_4 = false;
        }
        else
        {
            _runFlag_4 = _S575;
        }
        if(_runFlag_4)
        {
            float3  _S583 = s_primal_ctx_unproject_raydir_0(_S573._S536, camera_model_9, is_ray_depth_8);
            float3  _S584 = make_float3 (depths_3.y) * _S583;
            if(!_S573._S539)
            {
                _runFlag_5 = false;
            }
            else
            {
                _runFlag_5 = _runFlag_4;
            }
            if(_runFlag_5)
            {
                float3  _S585 = s_primal_ctx_unproject_raydir_0(_S573._S538, camera_model_9, is_ray_depth_8);
                float3  _S586 = make_float3 (depths_3.z) * _S585;
                if(!_S573._S541)
                {
                    _runFlag_6 = false;
                }
                else
                {
                    _runFlag_6 = _runFlag_5;
                }
                if(_runFlag_6)
                {
                    float3  _S587 = s_primal_ctx_unproject_raydir_0(_S573._S540, camera_model_9, is_ray_depth_8);
                    float3  _S588 = make_float3 (depths_3.w) * _S587;
                    if(!_S573._S543)
                    {
                        _runFlag_7 = false;
                    }
                    else
                    {
                        _runFlag_7 = _runFlag_6;
                    }
                    if(_runFlag_7)
                    {
                        float3  _S589 = s_primal_ctx_unproject_raydir_0(_S573._S542, camera_model_9, is_ray_depth_8);
                        _S576 = int(1);
                        raydir_8 = _S589;
                    }
                    else
                    {
                        _S576 = int(0);
                        raydir_8 = _S587;
                    }
                    points_5[int(0)] = _S582;
                    points_5[int(1)] = _S584;
                    points_5[int(2)] = _S586;
                    points_5[int(3)] = _S588;
                    points_5[int(4)] = _S574;
                    _S577 = _S587;
                }
                else
                {
                    _S576 = int(0);
                    raydir_8 = _S585;
                    points_5[int(0)] = _S582;
                    points_5[int(1)] = _S584;
                    points_5[int(2)] = _S586;
                    points_5[int(3)] = _S574;
                    points_5[int(4)] = _S574;
                    _S577 = _S574;
                }
                _S578 = _S585;
            }
            else
            {
                _S576 = int(0);
                raydir_8 = _S583;
                points_5[int(0)] = _S582;
                points_5[int(1)] = _S584;
                points_5[int(2)] = _S574;
                points_5[int(3)] = _S574;
                points_5[int(4)] = _S574;
                _runFlag_6 = false;
                _S577 = _S574;
                _S578 = _S574;
            }
            _S579 = _S583;
        }
        else
        {
            _S576 = int(0);
            raydir_8 = _S581;
            points_5[int(0)] = _S582;
            points_5[int(1)] = _S574;
            points_5[int(2)] = _S574;
            points_5[int(3)] = _S574;
            points_5[int(4)] = _S574;
            _runFlag_5 = false;
            _runFlag_6 = false;
            _S577 = _S574;
            _S578 = _S574;
            _S579 = _S574;
        }
        _S580 = _S581;
    }
    else
    {
        _S576 = int(0);
        points_5[int(0)] = _S574;
        points_5[int(1)] = _S574;
        points_5[int(2)] = _S574;
        points_5[int(3)] = _S574;
        points_5[int(4)] = _S574;
        _runFlag_4 = false;
        _runFlag_5 = false;
        _runFlag_6 = false;
        _S577 = _S574;
        _S578 = _S574;
        _S579 = _S574;
        _S580 = _S574;
    }
    bool _S590 = !(_S576 != int(1));
    bool _S591;
    float3  normal_6;
    float3  _S592;
    float3  _S593;
    float3  _S594;
    float3  _S595;
    float _S596;
    float _S597;
    float _S598;
    float _S599;
    if(_S590)
    {
        float3  dx_2 = points_5[int(1)] - points_5[int(0)];
        float3  _S600 = - (points_5[int(3)] - points_5[int(2)]);
        float3  _S601 = s_primal_ctx_cross_0(dx_2, _S600);
        bool _S602 = (s_primal_ctx_dot_0(_S601, _S601)) != 0.0f;
        if(_S602)
        {
            normal_6 = normalize_0(_S601);
        }
        else
        {
            normal_6 = _S601;
        }
        bool _S603 = (s_primal_ctx_dot_0(gt_normal_1, gt_normal_1)) != 0.0f;
        if(_S603)
        {
            _S592 = normalize_0(gt_normal_1);
        }
        else
        {
            _S592 = gt_normal_1;
        }
        float3  _S604 = - normalize_0(raydir_8);
        float _S605 = s_primal_ctx_dot_0(normal_6, _S604);
        float _S606 = 1.0f - s_primal_ctx_dot_0(normal_6, _S592) + 0.00100000004749745f;
        float _S607 = (F32_max((_S605), (0.0f))) + 0.00100000004749745f;
        _S596 = _S607 * _S607;
        _S597 = _S606;
        _S598 = _S607;
        _S599 = _S605;
        raydir_8 = normal_6;
        normal_6 = _S604;
        _runFlag_7 = _S603;
        _S591 = _S602;
        _S593 = _S601;
        _S594 = dx_2;
        _S595 = _S600;
    }
    else
    {
        _S596 = 0.0f;
        _S597 = 0.0f;
        _S598 = 0.0f;
        _S599 = 0.0f;
        raydir_8 = _S574;
        normal_6 = _S574;
        _S592 = _S574;
        _runFlag_7 = false;
        _S591 = false;
        _S593 = _S574;
        _S594 = _S574;
        _S595 = _S574;
    }
    float4  _S608 = make_float4 (0.0f);
    if(_S590)
    {
        float _S609 = v_loss_0 / _S596;
        float _S610 = _S597 * - _S609;
        float s_diff_num_T_0 = _S598 * _S609;
        DiffPair_float_0 _S611;
        (&_S611)->primal_0 = _S599;
        (&_S611)->differential_0 = 0.0f;
        DiffPair_float_0 _S612;
        (&_S612)->primal_0 = 0.0f;
        (&_S612)->differential_0 = 0.0f;
        _d_max_0(&_S611, &_S612, _S610);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S613;
        (&_S613)->primal_0 = raydir_8;
        (&_S613)->differential_0 = _S574;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S614;
        (&_S614)->primal_0 = normal_6;
        (&_S614)->differential_0 = _S574;
        s_bwd_prop_dot_0(&_S613, &_S614, _S611.differential_0);
        float _S615 = - s_diff_num_T_0;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S616;
        (&_S616)->primal_0 = raydir_8;
        (&_S616)->differential_0 = _S574;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S617;
        (&_S617)->primal_0 = _S592;
        (&_S617)->differential_0 = _S574;
        s_bwd_prop_dot_0(&_S616, &_S617, _S615);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S618 = _S617;
        float3  _S619 = _S613.differential_0 + _S616.differential_0;
        if(_runFlag_7)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S620;
            (&_S620)->primal_0 = gt_normal_1;
            (&_S620)->differential_0 = _S574;
            s_bwd_normalize_impl_0(&_S620, _S618.differential_0);
            raydir_8 = _S620.differential_0;
        }
        else
        {
            raydir_8 = _S618.differential_0;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S621;
        (&_S621)->primal_0 = gt_normal_1;
        (&_S621)->differential_0 = _S574;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S622;
        (&_S622)->primal_0 = gt_normal_1;
        (&_S622)->differential_0 = _S574;
        s_bwd_prop_dot_0(&_S621, &_S622, 0.0f);
        float3  _S623 = _S622.differential_0 + _S621.differential_0 + raydir_8;
        if(_S591)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S624;
            (&_S624)->primal_0 = _S593;
            (&_S624)->differential_0 = _S574;
            s_bwd_normalize_impl_0(&_S624, _S619);
            raydir_8 = _S624.differential_0;
        }
        else
        {
            raydir_8 = _S619;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S625;
        (&_S625)->primal_0 = _S593;
        (&_S625)->differential_0 = _S574;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S626;
        (&_S626)->primal_0 = _S593;
        (&_S626)->differential_0 = _S574;
        s_bwd_prop_dot_0(&_S625, &_S626, 0.0f);
        float3  _S627 = _S626.differential_0 + _S625.differential_0 + raydir_8;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S628;
        (&_S628)->primal_0 = _S594;
        (&_S628)->differential_0 = _S574;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S629;
        (&_S629)->primal_0 = _S595;
        (&_S629)->differential_0 = _S574;
        s_bwd_prop_cross_0(&_S628, &_S629, _S627);
        float3  s_diff_dy_T_2 = - _S629.differential_0;
        float3  _S630 = - s_diff_dy_T_2;
        float3  _S631 = - _S628.differential_0;
        FixedArray<float3 , 5>  _S632;
        _S632[int(0)] = _S574;
        _S632[int(1)] = _S574;
        _S632[int(2)] = _S574;
        _S632[int(3)] = _S574;
        _S632[int(4)] = _S574;
        _S632[int(2)] = _S630;
        _S632[int(3)] = s_diff_dy_T_2;
        _S632[int(0)] = _S631;
        _S632[int(1)] = _S628.differential_0;
        points_5[int(0)] = _S632[int(0)];
        points_5[int(1)] = _S632[int(1)];
        points_5[int(2)] = _S632[int(2)];
        points_5[int(3)] = _S632[int(3)];
        points_5[int(4)] = _S632[int(4)];
        raydir_8 = _S623;
    }
    else
    {
        points_5[int(0)] = _S574;
        points_5[int(1)] = _S574;
        points_5[int(2)] = _S574;
        points_5[int(3)] = _S574;
        points_5[int(4)] = _S574;
        raydir_8 = _S574;
    }
    float4  _S633;
    if(_S575)
    {
        if(_runFlag_4)
        {
            if(_runFlag_5)
            {
                if(_runFlag_6)
                {
                    FixedArray<float3 , 5>  _S634 = points_5;
                    FixedArray<float3 , 5>  _S635 = points_5;
                    FixedArray<float3 , 5>  _S636 = points_5;
                    float3  _S637 = _S577 * points_5[int(3)];
                    float _S638 = _S637.x + _S637.y + _S637.z;
                    float4  _S639 = _S608;
                    *&((&_S639)->w) = _S638;
                    points_5[int(0)] = _S574;
                    points_5[int(1)] = _S574;
                    points_5[int(2)] = _S574;
                    points_5[int(3)] = _S574;
                    points_5[int(4)] = _S574;
                    _S577 = _S636[int(2)];
                    normal_6 = _S634[int(0)];
                    _S592 = _S635[int(1)];
                    _S633 = _S639;
                }
                else
                {
                    FixedArray<float3 , 5>  _S640 = points_5;
                    FixedArray<float3 , 5>  _S641 = points_5;
                    FixedArray<float3 , 5>  _S642 = points_5;
                    FixedArray<float3 , 5>  _S643 = points_5;
                    points_5[int(0)] = points_5[int(0)];
                    points_5[int(1)] = _S640[int(1)];
                    points_5[int(2)] = _S641[int(2)];
                    points_5[int(3)] = _S642[int(3)];
                    points_5[int(4)] = _S643[int(4)];
                    _S577 = _S574;
                    normal_6 = _S574;
                    _S592 = _S574;
                    _S633 = _S608;
                }
                float3  _S644 = _S578 * (points_5[int(2)] + _S577);
                float _S645 = _S644.x + _S644.y + _S644.z;
                float3  _S646 = points_5[int(0)] + normal_6;
                float3  _S647 = points_5[int(1)] + _S592;
                float4  _S648 = _S608;
                *&((&_S648)->z) = _S645;
                float4  _S649 = _S633 + _S648;
                points_5[int(0)] = _S574;
                points_5[int(1)] = _S574;
                points_5[int(2)] = _S574;
                points_5[int(3)] = _S574;
                points_5[int(4)] = _S574;
                _S577 = _S647;
                _S578 = _S646;
                _S633 = _S649;
            }
            else
            {
                FixedArray<float3 , 5>  _S650 = points_5;
                FixedArray<float3 , 5>  _S651 = points_5;
                FixedArray<float3 , 5>  _S652 = points_5;
                FixedArray<float3 , 5>  _S653 = points_5;
                points_5[int(0)] = points_5[int(0)];
                points_5[int(1)] = _S650[int(1)];
                points_5[int(2)] = _S651[int(2)];
                points_5[int(3)] = _S652[int(3)];
                points_5[int(4)] = _S653[int(4)];
                _S577 = _S574;
                _S578 = _S574;
                _S633 = _S608;
            }
            float3  _S654 = _S579 * (points_5[int(1)] + _S577);
            float _S655 = _S654.x + _S654.y + _S654.z;
            float3  _S656 = points_5[int(0)] + _S578;
            float4  _S657 = _S608;
            *&((&_S657)->y) = _S655;
            float4  _S658 = _S633 + _S657;
            points_5[int(0)] = _S574;
            points_5[int(1)] = _S574;
            points_5[int(2)] = _S574;
            points_5[int(3)] = _S574;
            points_5[int(4)] = _S574;
            _S577 = _S656;
            _S633 = _S658;
        }
        else
        {
            FixedArray<float3 , 5>  _S659 = points_5;
            FixedArray<float3 , 5>  _S660 = points_5;
            FixedArray<float3 , 5>  _S661 = points_5;
            FixedArray<float3 , 5>  _S662 = points_5;
            points_5[int(0)] = points_5[int(0)];
            points_5[int(1)] = _S659[int(1)];
            points_5[int(2)] = _S660[int(2)];
            points_5[int(3)] = _S661[int(3)];
            points_5[int(4)] = _S662[int(4)];
            _S577 = _S574;
            _S633 = _S608;
        }
        float3  _S663 = _S580 * (points_5[int(0)] + _S577);
        float _S664 = _S663.x + _S663.y + _S663.z;
        float4  _S665 = _S608;
        *&((&_S665)->x) = _S664;
        _S633 = _S633 + _S665;
    }
    else
    {
        _S633 = _S608;
    }
    *v_depths_1 = _S633;
    *v_gt_normal_0 = raydir_8;
    return;
}

inline __device__ float3  generate_ray_d2n_opencv(float2  pix_pos_3, float4  intrins_8, FixedArray<float, 4>  dist_coeffs_12, int camera_model_10, bool is_ray_depth_9)
{
    float3  _S666;
    for(;;)
    {
        float2  uv_30 = (pix_pos_3 - float2 {intrins_8.z, intrins_8.w}) / float2 {intrins_8.x, intrins_8.y};
        FixedArray<float, 4>  _S667 = dist_coeffs_12;
        float2  uv_u_12;
        bool _S668 = undistort_point_1(uv_30, &_S667, int(12), &uv_u_12);
        if(!_S668)
        {
            int3  _S669 = make_int3 (int(0));
            float3  _S670 = make_float3 ((float)_S669.x, (float)_S669.y, (float)_S669.z);
            _S666 = _S670;
            break;
        }
        _S666 = unproject_raydir_0(uv_u_12, camera_model_10, is_ray_depth_9);
        break;
    }
    return _S666;
}

inline __device__ float3  depth_to_point_opencv(float2  pix_pos_4, float4  intrins_9, FixedArray<float, 4>  dist_coeffs_13, int camera_model_11, bool is_ray_depth_10, float depth_4)
{
    float3  _S671;
    for(;;)
    {
        float2  uv_31 = (pix_pos_4 - float2 {intrins_9.z, intrins_9.w}) / float2 {intrins_9.x, intrins_9.y};
        FixedArray<float, 4>  _S672 = dist_coeffs_13;
        float2  uv_u_13;
        bool _S673 = undistort_point_1(uv_31, &_S672, int(12), &uv_u_13);
        if(!_S673)
        {
            _S671 = make_float3 (0.0f);
            break;
        }
        _S671 = make_float3 (depth_4) * unproject_raydir_0(uv_u_13, camera_model_11, is_ray_depth_10);
        break;
    }
    return _S671;
}

struct s_bwd_prop_depth_to_point_Intermediates_1
{
    float2  _S674;
    bool _S675;
};

inline __device__ float depth_to_point_vjp_opencv(float2  pix_pos_5, float4  intrins_10, FixedArray<float, 4>  dist_coeffs_14, int camera_model_12, bool is_ray_depth_11, float depth_5, float3  v_point_1)
{
    float2  _S676 = make_float2 (0.0f);
    s_bwd_prop_depth_to_point_Intermediates_1 _S677;
    (&_S677)->_S674 = _S676;
    (&_S677)->_S675 = false;
    float2  uv_32 = (pix_pos_5 - float2 {intrins_10.z, intrins_10.w}) / float2 {intrins_10.x, intrins_10.y};
    float2  _S678 = _S676;
    FixedArray<float, 4>  _S679 = dist_coeffs_14;
    bool _S680 = undistort_point_1(uv_32, &_S679, int(12), &_S678);
    (&_S677)->_S674 = _S678;
    (&_S677)->_S675 = _S680;
    s_bwd_prop_depth_to_point_Intermediates_1 _S681 = _S677;
    float3  _S682 = make_float3 (0.0f);
    bool _S683 = !!_S677._S675;
    float3  _S684;
    if(_S683)
    {
        _S684 = s_primal_ctx_unproject_raydir_0(_S681._S674, camera_model_12, is_ray_depth_11);
    }
    else
    {
        _S684 = _S682;
    }
    if(_S683)
    {
        _S684 = _S684 * v_point_1;
    }
    else
    {
        _S684 = _S682;
    }
    return _S684.x + _S684.y + _S684.z;
}

inline __device__ float3  depth_to_normal_opencv(float2  pix_center_5, float4  intrins_11, FixedArray<float, 4>  dist_coeffs_15, int camera_model_13, bool is_ray_depth_12, float4  depths_4)
{
    float3  normal_7;
    for(;;)
    {
        bool _S685;
        if((depths_4.x) == 0.0f)
        {
            _S685 = true;
        }
        else
        {
            _S685 = (depths_4.y) == 0.0f;
        }
        if(_S685)
        {
            _S685 = true;
        }
        else
        {
            _S685 = (depths_4.z) == 0.0f;
        }
        if(_S685)
        {
            _S685 = true;
        }
        else
        {
            _S685 = (depths_4.w) == 0.0f;
        }
        if(_S685)
        {
            normal_7 = make_float3 (0.0f);
            break;
        }
        float3  * _S686;
        float3  * _S687;
        float3  * _S688;
        float3  * _S689;
        int _S690;
        FixedArray<float3 , 4>  points_6;
        for(;;)
        {
            float2  _S691 = float2 {intrins_11.z, intrins_11.w};
            float2  _S692 = float2 {intrins_11.x, intrins_11.y};
            float2  uv_33 = (pix_center_5 + make_float2 (-1.0f, -0.0f) - _S691) / _S692;
            FixedArray<float, 4>  _S693 = dist_coeffs_15;
            float2  uv_u_14;
            bool _S694 = undistort_point_1(uv_33, &_S693, int(12), &uv_u_14);
            if(!_S694)
            {
                float3  _S695 = make_float3 (0.0f);
                _S690 = int(0);
                _S689 = nullptr;
                _S688 = nullptr;
                _S687 = nullptr;
                _S686 = nullptr;
                normal_7 = _S695;
                break;
            }
            points_6[int(0)] = make_float3 (depths_4.x) * unproject_raydir_0(uv_u_14, camera_model_13, is_ray_depth_12);
            for(;;)
            {
                float2  uv_34 = (pix_center_5 + make_float2 (1.0f, -0.0f) - _S691) / _S692;
                FixedArray<float, 4>  _S696 = dist_coeffs_15;
                float2  uv_u_15;
                bool _S697 = undistort_point_1(uv_34, &_S696, int(12), &uv_u_15);
                if(!_S697)
                {
                    float3  _S698 = make_float3 (0.0f);
                    _S690 = int(0);
                    _S689 = nullptr;
                    normal_7 = _S698;
                    break;
                }
                points_6[int(1)] = make_float3 (depths_4.y) * unproject_raydir_0(uv_u_15, camera_model_13, is_ray_depth_12);
                _S690 = int(2);
                _S689 = &points_6[int(1)];
                break;
            }
            if(_S690 != int(2))
            {
                _S688 = &points_6[int(0)];
                _S687 = nullptr;
                _S686 = nullptr;
                break;
            }
            float2  uv_35 = (pix_center_5 + make_float2 (0.0f, -1.0f) - _S691) / _S692;
            FixedArray<float, 4>  _S699 = dist_coeffs_15;
            float2  uv_u_16;
            bool _S700 = undistort_point_1(uv_35, &_S699, int(12), &uv_u_16);
            if(!_S700)
            {
                float3  _S701 = make_float3 (0.0f);
                _S690 = int(0);
                _S688 = &points_6[int(0)];
                _S687 = nullptr;
                _S686 = nullptr;
                normal_7 = _S701;
                break;
            }
            points_6[int(2)] = make_float3 (depths_4.z) * unproject_raydir_0(uv_u_16, camera_model_13, is_ray_depth_12);
            for(;;)
            {
                float2  uv_36 = (pix_center_5 + make_float2 (0.0f, 1.0f) - _S691) / _S692;
                FixedArray<float, 4>  _S702 = dist_coeffs_15;
                float2  uv_u_17;
                bool _S703 = undistort_point_1(uv_36, &_S702, int(12), &uv_u_17);
                if(!_S703)
                {
                    float3  _S704 = make_float3 (0.0f);
                    _S690 = int(0);
                    _S688 = nullptr;
                    normal_7 = _S704;
                    break;
                }
                points_6[int(3)] = make_float3 (depths_4.w) * unproject_raydir_0(uv_u_17, camera_model_13, is_ray_depth_12);
                _S690 = int(2);
                _S688 = &points_6[int(3)];
                break;
            }
            if(_S690 != int(2))
            {
                float3  * _S705 = _S688;
                _S688 = &points_6[int(0)];
                _S687 = _S705;
                _S686 = &points_6[int(2)];
                break;
            }
            float3  * _S706 = _S688;
            _S690 = int(1);
            _S688 = &points_6[int(0)];
            _S687 = _S706;
            _S686 = &points_6[int(2)];
            break;
        }
        if(_S690 != int(1))
        {
            break;
        }
        float3  normal_8 = cross_0(*_S689 - *_S688, - (*_S687 - *_S686));
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
    float2  _S707;
    bool _S708;
    float2  _S709;
    bool _S710;
    float2  _S711;
    bool _S712;
    float2  _S713;
    bool _S714;
};

inline __device__ void depth_to_normal_vjp_opencv(float2  pix_center_6, float4  intrins_12, FixedArray<float, 4>  dist_coeffs_16, int camera_model_14, bool is_ray_depth_13, float4  depths_5, float3  v_normal_2, float4  * v_depths_2)
{
    float2  _S715 = make_float2 (0.0f);
    s_bwd_prop_depth_to_normal_Intermediates_1 _S716;
    (&_S716)->_S707 = _S715;
    (&_S716)->_S708 = false;
    (&_S716)->_S709 = _S715;
    (&_S716)->_S710 = false;
    (&_S716)->_S711 = _S715;
    (&_S716)->_S712 = false;
    (&_S716)->_S713 = _S715;
    (&_S716)->_S714 = false;
    (&_S716)->_S707 = _S715;
    (&_S716)->_S708 = false;
    (&_S716)->_S709 = _S715;
    (&_S716)->_S710 = false;
    (&_S716)->_S711 = _S715;
    (&_S716)->_S712 = false;
    (&_S716)->_S713 = _S715;
    (&_S716)->_S714 = false;
    bool _S717 = (depths_5.x) == 0.0f;
    bool _runFlag_8;
    if(_S717)
    {
        _runFlag_8 = true;
    }
    else
    {
        _runFlag_8 = (depths_5.y) == 0.0f;
    }
    if(_runFlag_8)
    {
        _runFlag_8 = true;
    }
    else
    {
        _runFlag_8 = (depths_5.z) == 0.0f;
    }
    if(_runFlag_8)
    {
        _runFlag_8 = true;
    }
    else
    {
        _runFlag_8 = (depths_5.w) == 0.0f;
    }
    int _S718;
    if(!_runFlag_8)
    {
        float2  _S719 = float2 {intrins_12.z, intrins_12.w};
        float2  _S720 = float2 {intrins_12.x, intrins_12.y};
        float2  uv_37 = (pix_center_6 + make_float2 (-1.0f, -0.0f) - _S719) / _S720;
        float2  _S721 = _S715;
        FixedArray<float, 4>  _S722 = dist_coeffs_16;
        bool _S723 = undistort_point_1(uv_37, &_S722, int(12), &_S721);
        (&_S716)->_S707 = _S721;
        (&_S716)->_S708 = _S723;
        bool _S724 = !!_S723;
        if(_S724)
        {
            float2  uv_38 = (pix_center_6 + make_float2 (1.0f, -0.0f) - _S719) / _S720;
            float2  _S725 = _S715;
            FixedArray<float, 4>  _S726 = dist_coeffs_16;
            bool _S727 = undistort_point_1(uv_38, &_S726, int(12), &_S725);
            (&_S716)->_S709 = _S725;
            (&_S716)->_S710 = _S727;
            if(!!_S727)
            {
                _S718 = int(2);
            }
            else
            {
                _S718 = int(0);
            }
            if(_S718 != int(2))
            {
                _runFlag_8 = false;
            }
            else
            {
                _runFlag_8 = _S724;
            }
            if(_runFlag_8)
            {
                float2  uv_39 = (pix_center_6 + make_float2 (0.0f, -1.0f) - _S719) / _S720;
                float2  _S728 = _S715;
                FixedArray<float, 4>  _S729 = dist_coeffs_16;
                bool _S730 = undistort_point_1(uv_39, &_S729, int(12), &_S728);
                (&_S716)->_S711 = _S728;
                (&_S716)->_S712 = _S730;
                if(!_S730)
                {
                    _runFlag_8 = false;
                }
                if(_runFlag_8)
                {
                    float2  uv_40 = (pix_center_6 + make_float2 (0.0f, 1.0f) - _S719) / _S720;
                    float2  _S731 = _S715;
                    FixedArray<float, 4>  _S732 = dist_coeffs_16;
                    bool _S733 = undistort_point_1(uv_40, &_S732, int(12), &_S731);
                    (&_S716)->_S713 = _S731;
                    (&_S716)->_S714 = _S733;
                }
            }
        }
    }
    s_bwd_prop_depth_to_normal_Intermediates_1 _S734 = _S716;
    float3  _S735 = make_float3 (0.0f);
    if(_S717)
    {
        _runFlag_8 = true;
    }
    else
    {
        _runFlag_8 = (depths_5.y) == 0.0f;
    }
    if(_runFlag_8)
    {
        _runFlag_8 = true;
    }
    else
    {
        _runFlag_8 = (depths_5.z) == 0.0f;
    }
    if(_runFlag_8)
    {
        _runFlag_8 = true;
    }
    else
    {
        _runFlag_8 = (depths_5.w) == 0.0f;
    }
    bool _S736 = !_runFlag_8;
    bool _runFlag_9;
    bool _runFlag_10;
    bool _S737;
    bool _runFlag_11;
    bool _S738;
    bool _S739;
    FixedArray<float3 , 4>  points_7;
    float3  _S740;
    float3  _S741;
    float3  _S742;
    float3  _S743;
    float3  _S744;
    float3  _S745;
    float3  _S746;
    float3  _S747;
    float3  _S748;
    if(_S736)
    {
        bool _S749 = !!_S734._S708;
        if(_S749)
        {
            float3  _S750 = s_primal_ctx_unproject_raydir_0(_S734._S707, camera_model_14, is_ray_depth_13);
            float3  _S751 = make_float3 (depths_5.x) * _S750;
            bool _S752 = !!_S734._S710;
            if(_S752)
            {
                float3  _S753 = s_primal_ctx_unproject_raydir_0(_S734._S709, camera_model_14, is_ray_depth_13);
                float3  _S754 = make_float3 (depths_5.y) * _S753;
                _S718 = int(2);
                points_7[int(0)] = _S751;
                points_7[int(1)] = _S754;
                points_7[int(2)] = _S735;
                points_7[int(3)] = _S735;
                _S740 = _S753;
            }
            else
            {
                _S718 = int(0);
                points_7[int(0)] = _S751;
                points_7[int(1)] = _S735;
                points_7[int(2)] = _S735;
                points_7[int(3)] = _S735;
                _S740 = _S735;
            }
            if(_S718 != int(2))
            {
                _runFlag_8 = false;
            }
            else
            {
                _runFlag_8 = _S749;
                _S718 = int(0);
            }
            if(_runFlag_8)
            {
                if(!_S734._S712)
                {
                    _runFlag_9 = false;
                    _S718 = int(0);
                }
                else
                {
                    _runFlag_9 = _runFlag_8;
                }
                if(_runFlag_9)
                {
                    float3  _S755 = s_primal_ctx_unproject_raydir_0(_S734._S711, camera_model_14, is_ray_depth_13);
                    points_7[int(2)] = make_float3 (depths_5.z) * _S755;
                    bool _S756 = !!_S734._S714;
                    int _S757;
                    if(_S756)
                    {
                        float3  _S758 = s_primal_ctx_unproject_raydir_0(_S734._S713, camera_model_14, is_ray_depth_13);
                        points_7[int(3)] = make_float3 (depths_5.w) * _S758;
                        _S757 = int(2);
                        _S741 = _S758;
                    }
                    else
                    {
                        _S757 = int(0);
                        _S741 = _S735;
                    }
                    if(_S757 != int(2))
                    {
                        _runFlag_10 = false;
                        _S718 = _S757;
                    }
                    else
                    {
                        _runFlag_10 = _runFlag_9;
                    }
                    if(_runFlag_10)
                    {
                        _S718 = int(1);
                    }
                    _runFlag_10 = _S756;
                    _S742 = _S755;
                }
                else
                {
                    _runFlag_10 = false;
                    _S741 = _S735;
                    _S742 = _S735;
                }
            }
            else
            {
                _runFlag_9 = false;
                _runFlag_10 = false;
                _S741 = _S735;
                _S742 = _S735;
            }
            float3  _S759 = _S740;
            _S740 = _S741;
            _S741 = _S742;
            _S737 = _S752;
            _S742 = _S759;
            _S743 = _S750;
        }
        else
        {
            _S718 = int(0);
            points_7[int(0)] = _S735;
            points_7[int(1)] = _S735;
            points_7[int(2)] = _S735;
            points_7[int(3)] = _S735;
            _runFlag_8 = false;
            _runFlag_9 = false;
            _runFlag_10 = false;
            _S740 = _S735;
            _S741 = _S735;
            _S737 = false;
            _S742 = _S735;
            _S743 = _S735;
        }
        if(_S718 != int(1))
        {
            _runFlag_11 = false;
        }
        else
        {
            _runFlag_11 = _S736;
        }
        if(_runFlag_11)
        {
            float3  dx_3 = points_7[int(1)] - points_7[int(0)];
            float3  _S760 = - (points_7[int(3)] - points_7[int(2)]);
            float3  _S761 = s_primal_ctx_cross_0(dx_3, _S760);
            bool _S762 = (s_primal_ctx_dot_0(_S761, _S761)) != 0.0f;
            if(_S762)
            {
                float _S763 = length_0(_S761);
                float3  _S764 = make_float3 (_S763);
                _S744 = make_float3 (_S763 * _S763);
                _S745 = _S764;
            }
            else
            {
                _S744 = _S735;
                _S745 = _S735;
            }
            float3  _S765 = _S745;
            _S738 = _S762;
            _S745 = _S761;
            _S746 = _S765;
            _S747 = dx_3;
            _S748 = _S760;
        }
        else
        {
            _S738 = false;
            _S744 = _S735;
            _S745 = _S735;
            _S746 = _S735;
            _S747 = _S735;
            _S748 = _S735;
        }
        bool _S766 = _runFlag_8;
        bool _S767 = _runFlag_9;
        bool _S768 = _runFlag_10;
        float3  _S769 = _S740;
        float3  _S770 = _S741;
        bool _S771 = _S737;
        float3  _S772 = _S742;
        float3  _S773 = _S743;
        _runFlag_8 = _runFlag_11;
        _runFlag_9 = _S738;
        _S740 = _S744;
        _S741 = _S745;
        _S742 = _S746;
        _S743 = _S747;
        _S744 = _S748;
        _runFlag_10 = _S749;
        _S737 = _S766;
        _runFlag_11 = _S767;
        _S738 = _S768;
        _S745 = _S769;
        _S746 = _S770;
        _S739 = _S771;
        _S747 = _S772;
        _S748 = _S773;
    }
    else
    {
        _runFlag_8 = false;
        _runFlag_9 = false;
        _S740 = _S735;
        _S741 = _S735;
        _S742 = _S735;
        _S743 = _S735;
        _S744 = _S735;
        _runFlag_10 = false;
        _S737 = false;
        _runFlag_11 = false;
        _S738 = false;
        _S745 = _S735;
        _S746 = _S735;
        _S739 = false;
        _S747 = _S735;
        _S748 = _S735;
    }
    float4  _S774 = make_float4 (0.0f);
    float4  _S775;
    if(_S736)
    {
        if(_runFlag_8)
        {
            if(_runFlag_9)
            {
                float3  _S776 = v_normal_2 / _S740;
                float3  _S777 = _S741 * - _S776;
                float3  _S778 = _S742 * _S776;
                float _S779 = _S777.x + _S777.y + _S777.z;
                DiffPair_vectorx3Cfloatx2C3x3E_0 _S780;
                (&_S780)->primal_0 = _S741;
                (&_S780)->differential_0 = _S735;
                s_bwd_length_impl_0(&_S780, _S779);
                _S740 = _S778 + _S780.differential_0;
            }
            else
            {
                _S740 = v_normal_2;
            }
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S781;
            (&_S781)->primal_0 = _S741;
            (&_S781)->differential_0 = _S735;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S782;
            (&_S782)->primal_0 = _S741;
            (&_S782)->differential_0 = _S735;
            s_bwd_prop_dot_0(&_S781, &_S782, 0.0f);
            float3  _S783 = _S782.differential_0 + _S781.differential_0 + _S740;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S784;
            (&_S784)->primal_0 = _S743;
            (&_S784)->differential_0 = _S735;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S785;
            (&_S785)->primal_0 = _S744;
            (&_S785)->differential_0 = _S735;
            s_bwd_prop_cross_0(&_S784, &_S785, _S783);
            float3  s_diff_dy_T_3 = - _S785.differential_0;
            float3  _S786 = - s_diff_dy_T_3;
            float3  _S787 = - _S784.differential_0;
            FixedArray<float3 , 4>  _S788;
            _S788[int(0)] = _S735;
            _S788[int(1)] = _S735;
            _S788[int(2)] = _S735;
            _S788[int(3)] = _S735;
            _S788[int(2)] = _S786;
            _S788[int(3)] = s_diff_dy_T_3;
            _S788[int(0)] = _S787;
            _S788[int(1)] = _S784.differential_0;
            points_7[int(0)] = _S788[int(0)];
            points_7[int(1)] = _S788[int(1)];
            points_7[int(2)] = _S788[int(2)];
            points_7[int(3)] = _S788[int(3)];
        }
        else
        {
            points_7[int(0)] = _S735;
            points_7[int(1)] = _S735;
            points_7[int(2)] = _S735;
            points_7[int(3)] = _S735;
        }
        if(_runFlag_10)
        {
            if(_S737)
            {
                if(_runFlag_11)
                {
                    FixedArray<float3 , 4>  _S789 = points_7;
                    FixedArray<float3 , 4>  _S790 = points_7;
                    FixedArray<float3 , 4>  _S791 = points_7;
                    FixedArray<float3 , 4>  _S792 = points_7;
                    if(_S738)
                    {
                        float3  _S793 = _S745 * _S792[int(3)];
                        float _S794 = _S793.x + _S793.y + _S793.z;
                        float4  _S795 = _S774;
                        *&((&_S795)->w) = _S794;
                        points_7[int(0)] = _S789[int(0)];
                        points_7[int(1)] = _S790[int(1)];
                        points_7[int(2)] = _S791[int(2)];
                        points_7[int(3)] = _S735;
                        _S775 = _S795;
                    }
                    else
                    {
                        points_7[int(0)] = _S789[int(0)];
                        points_7[int(1)] = _S790[int(1)];
                        points_7[int(2)] = _S791[int(2)];
                        points_7[int(3)] = _S792[int(3)];
                        _S775 = _S774;
                    }
                    float3  _S796 = _S746 * points_7[int(2)];
                    float _S797 = _S796.x + _S796.y + _S796.z;
                    FixedArray<float3 , 4>  _S798 = points_7;
                    FixedArray<float3 , 4>  _S799 = points_7;
                    float4  _S800 = _S774;
                    *&((&_S800)->z) = _S797;
                    float4  _S801 = _S775 + _S800;
                    points_7[int(0)] = points_7[int(0)];
                    points_7[int(1)] = _S798[int(1)];
                    points_7[int(2)] = _S735;
                    points_7[int(3)] = _S799[int(3)];
                    _S775 = _S801;
                }
                else
                {
                    FixedArray<float3 , 4>  _S802 = points_7;
                    FixedArray<float3 , 4>  _S803 = points_7;
                    FixedArray<float3 , 4>  _S804 = points_7;
                    points_7[int(0)] = points_7[int(0)];
                    points_7[int(1)] = _S802[int(1)];
                    points_7[int(2)] = _S803[int(2)];
                    points_7[int(3)] = _S804[int(3)];
                    _S775 = _S774;
                }
            }
            else
            {
                FixedArray<float3 , 4>  _S805 = points_7;
                FixedArray<float3 , 4>  _S806 = points_7;
                FixedArray<float3 , 4>  _S807 = points_7;
                points_7[int(0)] = points_7[int(0)];
                points_7[int(1)] = _S805[int(1)];
                points_7[int(2)] = _S806[int(2)];
                points_7[int(3)] = _S807[int(3)];
                _S775 = _S774;
            }
            if(_S739)
            {
                FixedArray<float3 , 4>  _S808 = points_7;
                float3  _S809 = _S747 * points_7[int(1)];
                float _S810 = _S809.x + _S809.y + _S809.z;
                float4  _S811 = _S774;
                *&((&_S811)->y) = _S810;
                float4  _S812 = _S775 + _S811;
                points_7[int(0)] = _S735;
                points_7[int(1)] = _S735;
                points_7[int(2)] = _S735;
                points_7[int(3)] = _S735;
                _S740 = _S808[int(0)];
                _S775 = _S812;
            }
            else
            {
                FixedArray<float3 , 4>  _S813 = points_7;
                FixedArray<float3 , 4>  _S814 = points_7;
                FixedArray<float3 , 4>  _S815 = points_7;
                points_7[int(0)] = points_7[int(0)];
                points_7[int(1)] = _S813[int(1)];
                points_7[int(2)] = _S814[int(2)];
                points_7[int(3)] = _S815[int(3)];
                _S740 = _S735;
            }
            float3  _S816 = _S748 * (points_7[int(0)] + _S740);
            float _S817 = _S816.x + _S816.y + _S816.z;
            float4  _S818 = _S774;
            *&((&_S818)->x) = _S817;
            _S775 = _S775 + _S818;
        }
        else
        {
            _S775 = _S774;
        }
    }
    else
    {
        _S775 = _S774;
    }
    *v_depths_2 = _S775;
    return;
}

inline __device__ float ray_depth_to_linear_depth_factor_opencv(float2  pix_center_7, float4  intrins_13, FixedArray<float, 4>  dist_coeffs_17, int camera_model_15)
{
    float _S819;
    for(;;)
    {
        float2  uv_41 = (pix_center_7 - float2 {intrins_13.z, intrins_13.w}) / float2 {intrins_13.x, intrins_13.y};
        FixedArray<float, 4>  _S820 = dist_coeffs_17;
        float2  uv_u_18;
        bool _S821 = undistort_point_1(uv_41, &_S820, int(12), &uv_u_18);
        if(!_S821)
        {
            _S819 = 0.0f;
            break;
        }
        float3  raydir_9 = unproject_raydir_0(uv_u_18, camera_model_15, false);
        _S819 = float((F32_sign((raydir_9.z)))) / length_0(raydir_9);
        break;
    }
    return _S819;
}

inline __device__ float depth_normal_loss_opencv(float2  pix_center_8, float4  intrins_14, FixedArray<float, 4>  dist_coeffs_18, int camera_model_16, bool is_ray_depth_14, float4  depths_6, float3  gt_normal_2)
{
    float _S822;
    for(;;)
    {
        float3  _S823;
        float3  * _S824;
        float3  * _S825;
        float3  * _S826;
        float3  * _S827;
        int _S828;
        FixedArray<float3 , 5>  points_8;
        for(;;)
        {
            float2  _S829 = float2 {intrins_14.z, intrins_14.w};
            float2  _S830 = float2 {intrins_14.x, intrins_14.y};
            float2  uv_42 = (pix_center_8 + make_float2 (-1.0f, -0.0f) - _S829) / _S830;
            FixedArray<float, 4>  _S831 = dist_coeffs_18;
            float2  uv_u_19;
            bool _S832 = undistort_point_1(uv_42, &_S831, int(12), &uv_u_19);
            float3  _S833 = make_float3 (0.0f);
            if(!_S832)
            {
                _S828 = int(0);
                _S827 = nullptr;
                _S826 = nullptr;
                _S825 = nullptr;
                _S824 = nullptr;
                _S823 = _S833;
                break;
            }
            float3  raydir_10 = unproject_raydir_0(uv_u_19, camera_model_16, is_ray_depth_14);
            points_8[int(0)] = make_float3 (depths_6.x) * raydir_10;
            float2  uv_43 = (pix_center_8 + make_float2 (1.0f, -0.0f) - _S829) / _S830;
            FixedArray<float, 4>  _S834 = dist_coeffs_18;
            float2  uv_u_20;
            bool _S835 = undistort_point_1(uv_43, &_S834, int(12), &uv_u_20);
            if(!_S835)
            {
                _S828 = int(0);
                _S827 = nullptr;
                _S826 = &points_8[int(0)];
                _S825 = nullptr;
                _S824 = nullptr;
                _S823 = _S833;
                break;
            }
            float3  raydir_11 = unproject_raydir_0(uv_u_20, camera_model_16, is_ray_depth_14);
            points_8[int(1)] = make_float3 (depths_6.y) * raydir_11;
            float2  uv_44 = (pix_center_8 + make_float2 (0.0f, -1.0f) - _S829) / _S830;
            FixedArray<float, 4>  _S836 = dist_coeffs_18;
            float2  uv_u_21;
            bool _S837 = undistort_point_1(uv_44, &_S836, int(12), &uv_u_21);
            if(!_S837)
            {
                _S828 = int(0);
                _S827 = &points_8[int(1)];
                _S826 = &points_8[int(0)];
                _S825 = nullptr;
                _S824 = nullptr;
                _S823 = _S833;
                break;
            }
            float3  raydir_12 = unproject_raydir_0(uv_u_21, camera_model_16, is_ray_depth_14);
            points_8[int(2)] = make_float3 (depths_6.z) * raydir_12;
            float2  uv_45 = (pix_center_8 + make_float2 (0.0f, 1.0f) - _S829) / _S830;
            FixedArray<float, 4>  _S838 = dist_coeffs_18;
            float2  uv_u_22;
            bool _S839 = undistort_point_1(uv_45, &_S838, int(12), &uv_u_22);
            if(!_S839)
            {
                _S828 = int(0);
                _S827 = &points_8[int(1)];
                _S826 = &points_8[int(0)];
                _S825 = nullptr;
                _S824 = &points_8[int(2)];
                _S823 = _S833;
                break;
            }
            float3  raydir_13 = unproject_raydir_0(uv_u_22, camera_model_16, is_ray_depth_14);
            points_8[int(3)] = make_float3 (depths_6.w) * raydir_13;
            float2  uv_46 = (pix_center_8 + make_float2 (0.0f) * make_float2 (0.0f, 3.0f) - _S829) / _S830;
            FixedArray<float, 4>  _S840 = dist_coeffs_18;
            float2  uv_u_23;
            bool _S841 = undistort_point_1(uv_46, &_S840, int(12), &uv_u_23);
            if(!_S841)
            {
                _S828 = int(0);
                _S827 = &points_8[int(1)];
                _S826 = &points_8[int(0)];
                _S825 = &points_8[int(3)];
                _S824 = &points_8[int(2)];
                _S823 = _S833;
                break;
            }
            float3  raydir_14 = unproject_raydir_0(uv_u_23, camera_model_16, is_ray_depth_14);
            _S828 = int(1);
            _S827 = &points_8[int(1)];
            _S826 = &points_8[int(0)];
            _S825 = &points_8[int(3)];
            _S824 = &points_8[int(2)];
            _S823 = raydir_14;
            break;
        }
        if(_S828 != int(1))
        {
            _S822 = 0.0f;
            break;
        }
        float3  normal_9 = cross_0(*_S827 - *_S826, - (*_S825 - *_S824));
        float3  normal_10;
        if((dot_0(normal_9, normal_9)) != 0.0f)
        {
            normal_10 = normalize_0(normal_9);
        }
        else
        {
            normal_10 = normal_9;
        }
        float3  _S842;
        if((dot_0(gt_normal_2, gt_normal_2)) != 0.0f)
        {
            _S842 = normalize_0(gt_normal_2);
        }
        else
        {
            _S842 = gt_normal_2;
        }
        _S822 = (1.0f - dot_0(normal_10, _S842) + 0.00100000004749745f) / ((F32_max((dot_0(normal_10, - normalize_0(_S823))), (0.0f))) + 0.00100000004749745f);
        break;
    }
    return _S822;
}

struct s_bwd_prop_depth_normal_loss_Intermediates_1
{
    float2  _S843;
    bool _S844;
    float2  _S845;
    bool _S846;
    float2  _S847;
    bool _S848;
    float2  _S849;
    bool _S850;
    float2  _S851;
    bool _S852;
};

inline __device__ void depth_normal_loss_vjp_opencv(float2  pix_center_9, float4  intrins_15, FixedArray<float, 4>  dist_coeffs_19, int camera_model_17, bool is_ray_depth_15, float4  depths_7, float3  gt_normal_3, float v_loss_1, float4  * v_depths_3, float3  * v_gt_normal_1)
{
    float2  _S853 = make_float2 (0.0f);
    s_bwd_prop_depth_normal_loss_Intermediates_1 _S854;
    (&_S854)->_S843 = _S853;
    (&_S854)->_S844 = false;
    (&_S854)->_S845 = _S853;
    (&_S854)->_S846 = false;
    (&_S854)->_S847 = _S853;
    (&_S854)->_S848 = false;
    (&_S854)->_S849 = _S853;
    (&_S854)->_S850 = false;
    (&_S854)->_S851 = _S853;
    (&_S854)->_S852 = false;
    (&_S854)->_S845 = _S853;
    (&_S854)->_S846 = false;
    (&_S854)->_S847 = _S853;
    (&_S854)->_S848 = false;
    (&_S854)->_S849 = _S853;
    (&_S854)->_S850 = false;
    (&_S854)->_S851 = _S853;
    (&_S854)->_S852 = false;
    float2  _S855 = float2 {intrins_15.z, intrins_15.w};
    float2  _S856 = float2 {intrins_15.x, intrins_15.y};
    float2  uv_47 = (pix_center_9 + make_float2 (-1.0f, -0.0f) - _S855) / _S856;
    float2  _S857 = _S853;
    FixedArray<float, 4>  _S858 = dist_coeffs_19;
    bool _S859 = undistort_point_1(uv_47, &_S858, int(12), &_S857);
    (&_S854)->_S843 = _S857;
    (&_S854)->_S844 = _S859;
    bool _S860 = !!_S859;
    bool _runFlag_12;
    if(_S860)
    {
        float2  uv_48 = (pix_center_9 + make_float2 (1.0f, -0.0f) - _S855) / _S856;
        float2  _S861 = _S853;
        FixedArray<float, 4>  _S862 = dist_coeffs_19;
        bool _S863 = undistort_point_1(uv_48, &_S862, int(12), &_S861);
        (&_S854)->_S845 = _S861;
        (&_S854)->_S846 = _S863;
        if(!_S863)
        {
            _runFlag_12 = false;
        }
        else
        {
            _runFlag_12 = _S860;
        }
        if(_runFlag_12)
        {
            float2  uv_49 = (pix_center_9 + make_float2 (0.0f, -1.0f) - _S855) / _S856;
            float2  _S864 = _S853;
            FixedArray<float, 4>  _S865 = dist_coeffs_19;
            bool _S866 = undistort_point_1(uv_49, &_S865, int(12), &_S864);
            (&_S854)->_S847 = _S864;
            (&_S854)->_S848 = _S866;
            if(!_S866)
            {
                _runFlag_12 = false;
            }
            if(_runFlag_12)
            {
                float2  uv_50 = (pix_center_9 + make_float2 (0.0f, 1.0f) - _S855) / _S856;
                float2  _S867 = _S853;
                FixedArray<float, 4>  _S868 = dist_coeffs_19;
                bool _S869 = undistort_point_1(uv_50, &_S868, int(12), &_S867);
                (&_S854)->_S849 = _S867;
                (&_S854)->_S850 = _S869;
                if(!_S869)
                {
                    _runFlag_12 = false;
                }
                if(_runFlag_12)
                {
                    float2  uv_51 = (pix_center_9 - _S855) / _S856;
                    float2  _S870 = _S853;
                    FixedArray<float, 4>  _S871 = dist_coeffs_19;
                    bool _S872 = undistort_point_1(uv_51, &_S871, int(12), &_S870);
                    (&_S854)->_S851 = _S870;
                    (&_S854)->_S852 = _S872;
                }
            }
        }
    }
    s_bwd_prop_depth_normal_loss_Intermediates_1 _S873 = _S854;
    float3  _S874 = make_float3 (0.0f);
    bool _S875 = !!_S854._S844;
    bool _runFlag_13;
    bool _runFlag_14;
    bool _runFlag_15;
    int _S876;
    float3  raydir_15;
    float3  _S877;
    float3  _S878;
    float3  _S879;
    float3  _S880;
    FixedArray<float3 , 5>  points_9;
    if(_S875)
    {
        float3  _S881 = s_primal_ctx_unproject_raydir_0(_S873._S843, camera_model_17, is_ray_depth_15);
        float3  _S882 = make_float3 (depths_7.x) * _S881;
        if(!_S873._S846)
        {
            _runFlag_12 = false;
        }
        else
        {
            _runFlag_12 = _S875;
        }
        if(_runFlag_12)
        {
            float3  _S883 = s_primal_ctx_unproject_raydir_0(_S873._S845, camera_model_17, is_ray_depth_15);
            float3  _S884 = make_float3 (depths_7.y) * _S883;
            if(!_S873._S848)
            {
                _runFlag_13 = false;
            }
            else
            {
                _runFlag_13 = _runFlag_12;
            }
            if(_runFlag_13)
            {
                float3  _S885 = s_primal_ctx_unproject_raydir_0(_S873._S847, camera_model_17, is_ray_depth_15);
                float3  _S886 = make_float3 (depths_7.z) * _S885;
                if(!_S873._S850)
                {
                    _runFlag_14 = false;
                }
                else
                {
                    _runFlag_14 = _runFlag_13;
                }
                if(_runFlag_14)
                {
                    float3  _S887 = s_primal_ctx_unproject_raydir_0(_S873._S849, camera_model_17, is_ray_depth_15);
                    float3  _S888 = make_float3 (depths_7.w) * _S887;
                    if(!_S873._S852)
                    {
                        _runFlag_15 = false;
                    }
                    else
                    {
                        _runFlag_15 = _runFlag_14;
                    }
                    if(_runFlag_15)
                    {
                        float3  _S889 = s_primal_ctx_unproject_raydir_0(_S873._S851, camera_model_17, is_ray_depth_15);
                        _S876 = int(1);
                        raydir_15 = _S889;
                    }
                    else
                    {
                        _S876 = int(0);
                        raydir_15 = _S887;
                    }
                    points_9[int(0)] = _S882;
                    points_9[int(1)] = _S884;
                    points_9[int(2)] = _S886;
                    points_9[int(3)] = _S888;
                    points_9[int(4)] = _S874;
                    _S877 = _S887;
                }
                else
                {
                    _S876 = int(0);
                    raydir_15 = _S885;
                    points_9[int(0)] = _S882;
                    points_9[int(1)] = _S884;
                    points_9[int(2)] = _S886;
                    points_9[int(3)] = _S874;
                    points_9[int(4)] = _S874;
                    _S877 = _S874;
                }
                _S878 = _S885;
            }
            else
            {
                _S876 = int(0);
                raydir_15 = _S883;
                points_9[int(0)] = _S882;
                points_9[int(1)] = _S884;
                points_9[int(2)] = _S874;
                points_9[int(3)] = _S874;
                points_9[int(4)] = _S874;
                _runFlag_14 = false;
                _S877 = _S874;
                _S878 = _S874;
            }
            _S879 = _S883;
        }
        else
        {
            _S876 = int(0);
            raydir_15 = _S881;
            points_9[int(0)] = _S882;
            points_9[int(1)] = _S874;
            points_9[int(2)] = _S874;
            points_9[int(3)] = _S874;
            points_9[int(4)] = _S874;
            _runFlag_13 = false;
            _runFlag_14 = false;
            _S877 = _S874;
            _S878 = _S874;
            _S879 = _S874;
        }
        _S880 = _S881;
    }
    else
    {
        _S876 = int(0);
        points_9[int(0)] = _S874;
        points_9[int(1)] = _S874;
        points_9[int(2)] = _S874;
        points_9[int(3)] = _S874;
        points_9[int(4)] = _S874;
        _runFlag_12 = false;
        _runFlag_13 = false;
        _runFlag_14 = false;
        _S877 = _S874;
        _S878 = _S874;
        _S879 = _S874;
        _S880 = _S874;
    }
    bool _S890 = !(_S876 != int(1));
    bool _S891;
    float3  normal_11;
    float3  _S892;
    float3  _S893;
    float3  _S894;
    float3  _S895;
    float _S896;
    float _S897;
    float _S898;
    float _S899;
    if(_S890)
    {
        float3  dx_4 = points_9[int(1)] - points_9[int(0)];
        float3  _S900 = - (points_9[int(3)] - points_9[int(2)]);
        float3  _S901 = s_primal_ctx_cross_0(dx_4, _S900);
        bool _S902 = (s_primal_ctx_dot_0(_S901, _S901)) != 0.0f;
        if(_S902)
        {
            normal_11 = normalize_0(_S901);
        }
        else
        {
            normal_11 = _S901;
        }
        bool _S903 = (s_primal_ctx_dot_0(gt_normal_3, gt_normal_3)) != 0.0f;
        if(_S903)
        {
            _S892 = normalize_0(gt_normal_3);
        }
        else
        {
            _S892 = gt_normal_3;
        }
        float3  _S904 = - normalize_0(raydir_15);
        float _S905 = s_primal_ctx_dot_0(normal_11, _S904);
        float _S906 = 1.0f - s_primal_ctx_dot_0(normal_11, _S892) + 0.00100000004749745f;
        float _S907 = (F32_max((_S905), (0.0f))) + 0.00100000004749745f;
        _S896 = _S907 * _S907;
        _S897 = _S906;
        _S898 = _S907;
        _S899 = _S905;
        raydir_15 = normal_11;
        normal_11 = _S904;
        _runFlag_15 = _S903;
        _S891 = _S902;
        _S893 = _S901;
        _S894 = dx_4;
        _S895 = _S900;
    }
    else
    {
        _S896 = 0.0f;
        _S897 = 0.0f;
        _S898 = 0.0f;
        _S899 = 0.0f;
        raydir_15 = _S874;
        normal_11 = _S874;
        _S892 = _S874;
        _runFlag_15 = false;
        _S891 = false;
        _S893 = _S874;
        _S894 = _S874;
        _S895 = _S874;
    }
    float4  _S908 = make_float4 (0.0f);
    if(_S890)
    {
        float _S909 = v_loss_1 / _S896;
        float _S910 = _S897 * - _S909;
        float s_diff_num_T_1 = _S898 * _S909;
        DiffPair_float_0 _S911;
        (&_S911)->primal_0 = _S899;
        (&_S911)->differential_0 = 0.0f;
        DiffPair_float_0 _S912;
        (&_S912)->primal_0 = 0.0f;
        (&_S912)->differential_0 = 0.0f;
        _d_max_0(&_S911, &_S912, _S910);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S913;
        (&_S913)->primal_0 = raydir_15;
        (&_S913)->differential_0 = _S874;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S914;
        (&_S914)->primal_0 = normal_11;
        (&_S914)->differential_0 = _S874;
        s_bwd_prop_dot_0(&_S913, &_S914, _S911.differential_0);
        float _S915 = - s_diff_num_T_1;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S916;
        (&_S916)->primal_0 = raydir_15;
        (&_S916)->differential_0 = _S874;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S917;
        (&_S917)->primal_0 = _S892;
        (&_S917)->differential_0 = _S874;
        s_bwd_prop_dot_0(&_S916, &_S917, _S915);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S918 = _S917;
        float3  _S919 = _S913.differential_0 + _S916.differential_0;
        if(_runFlag_15)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S920;
            (&_S920)->primal_0 = gt_normal_3;
            (&_S920)->differential_0 = _S874;
            s_bwd_normalize_impl_0(&_S920, _S918.differential_0);
            raydir_15 = _S920.differential_0;
        }
        else
        {
            raydir_15 = _S918.differential_0;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S921;
        (&_S921)->primal_0 = gt_normal_3;
        (&_S921)->differential_0 = _S874;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S922;
        (&_S922)->primal_0 = gt_normal_3;
        (&_S922)->differential_0 = _S874;
        s_bwd_prop_dot_0(&_S921, &_S922, 0.0f);
        float3  _S923 = _S922.differential_0 + _S921.differential_0 + raydir_15;
        if(_S891)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S924;
            (&_S924)->primal_0 = _S893;
            (&_S924)->differential_0 = _S874;
            s_bwd_normalize_impl_0(&_S924, _S919);
            raydir_15 = _S924.differential_0;
        }
        else
        {
            raydir_15 = _S919;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S925;
        (&_S925)->primal_0 = _S893;
        (&_S925)->differential_0 = _S874;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S926;
        (&_S926)->primal_0 = _S893;
        (&_S926)->differential_0 = _S874;
        s_bwd_prop_dot_0(&_S925, &_S926, 0.0f);
        float3  _S927 = _S926.differential_0 + _S925.differential_0 + raydir_15;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S928;
        (&_S928)->primal_0 = _S894;
        (&_S928)->differential_0 = _S874;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S929;
        (&_S929)->primal_0 = _S895;
        (&_S929)->differential_0 = _S874;
        s_bwd_prop_cross_0(&_S928, &_S929, _S927);
        float3  s_diff_dy_T_4 = - _S929.differential_0;
        float3  _S930 = - s_diff_dy_T_4;
        float3  _S931 = - _S928.differential_0;
        FixedArray<float3 , 5>  _S932;
        _S932[int(0)] = _S874;
        _S932[int(1)] = _S874;
        _S932[int(2)] = _S874;
        _S932[int(3)] = _S874;
        _S932[int(4)] = _S874;
        _S932[int(2)] = _S930;
        _S932[int(3)] = s_diff_dy_T_4;
        _S932[int(0)] = _S931;
        _S932[int(1)] = _S928.differential_0;
        points_9[int(0)] = _S932[int(0)];
        points_9[int(1)] = _S932[int(1)];
        points_9[int(2)] = _S932[int(2)];
        points_9[int(3)] = _S932[int(3)];
        points_9[int(4)] = _S932[int(4)];
        raydir_15 = _S923;
    }
    else
    {
        points_9[int(0)] = _S874;
        points_9[int(1)] = _S874;
        points_9[int(2)] = _S874;
        points_9[int(3)] = _S874;
        points_9[int(4)] = _S874;
        raydir_15 = _S874;
    }
    float4  _S933;
    if(_S875)
    {
        if(_runFlag_12)
        {
            if(_runFlag_13)
            {
                if(_runFlag_14)
                {
                    FixedArray<float3 , 5>  _S934 = points_9;
                    FixedArray<float3 , 5>  _S935 = points_9;
                    FixedArray<float3 , 5>  _S936 = points_9;
                    float3  _S937 = _S877 * points_9[int(3)];
                    float _S938 = _S937.x + _S937.y + _S937.z;
                    float4  _S939 = _S908;
                    *&((&_S939)->w) = _S938;
                    points_9[int(0)] = _S874;
                    points_9[int(1)] = _S874;
                    points_9[int(2)] = _S874;
                    points_9[int(3)] = _S874;
                    points_9[int(4)] = _S874;
                    _S877 = _S936[int(2)];
                    normal_11 = _S934[int(0)];
                    _S892 = _S935[int(1)];
                    _S933 = _S939;
                }
                else
                {
                    FixedArray<float3 , 5>  _S940 = points_9;
                    FixedArray<float3 , 5>  _S941 = points_9;
                    FixedArray<float3 , 5>  _S942 = points_9;
                    FixedArray<float3 , 5>  _S943 = points_9;
                    points_9[int(0)] = points_9[int(0)];
                    points_9[int(1)] = _S940[int(1)];
                    points_9[int(2)] = _S941[int(2)];
                    points_9[int(3)] = _S942[int(3)];
                    points_9[int(4)] = _S943[int(4)];
                    _S877 = _S874;
                    normal_11 = _S874;
                    _S892 = _S874;
                    _S933 = _S908;
                }
                float3  _S944 = _S878 * (points_9[int(2)] + _S877);
                float _S945 = _S944.x + _S944.y + _S944.z;
                float3  _S946 = points_9[int(0)] + normal_11;
                float3  _S947 = points_9[int(1)] + _S892;
                float4  _S948 = _S908;
                *&((&_S948)->z) = _S945;
                float4  _S949 = _S933 + _S948;
                points_9[int(0)] = _S874;
                points_9[int(1)] = _S874;
                points_9[int(2)] = _S874;
                points_9[int(3)] = _S874;
                points_9[int(4)] = _S874;
                _S877 = _S947;
                _S878 = _S946;
                _S933 = _S949;
            }
            else
            {
                FixedArray<float3 , 5>  _S950 = points_9;
                FixedArray<float3 , 5>  _S951 = points_9;
                FixedArray<float3 , 5>  _S952 = points_9;
                FixedArray<float3 , 5>  _S953 = points_9;
                points_9[int(0)] = points_9[int(0)];
                points_9[int(1)] = _S950[int(1)];
                points_9[int(2)] = _S951[int(2)];
                points_9[int(3)] = _S952[int(3)];
                points_9[int(4)] = _S953[int(4)];
                _S877 = _S874;
                _S878 = _S874;
                _S933 = _S908;
            }
            float3  _S954 = _S879 * (points_9[int(1)] + _S877);
            float _S955 = _S954.x + _S954.y + _S954.z;
            float3  _S956 = points_9[int(0)] + _S878;
            float4  _S957 = _S908;
            *&((&_S957)->y) = _S955;
            float4  _S958 = _S933 + _S957;
            points_9[int(0)] = _S874;
            points_9[int(1)] = _S874;
            points_9[int(2)] = _S874;
            points_9[int(3)] = _S874;
            points_9[int(4)] = _S874;
            _S877 = _S956;
            _S933 = _S958;
        }
        else
        {
            FixedArray<float3 , 5>  _S959 = points_9;
            FixedArray<float3 , 5>  _S960 = points_9;
            FixedArray<float3 , 5>  _S961 = points_9;
            FixedArray<float3 , 5>  _S962 = points_9;
            points_9[int(0)] = points_9[int(0)];
            points_9[int(1)] = _S959[int(1)];
            points_9[int(2)] = _S960[int(2)];
            points_9[int(3)] = _S961[int(3)];
            points_9[int(4)] = _S962[int(4)];
            _S877 = _S874;
            _S933 = _S908;
        }
        float3  _S963 = _S880 * (points_9[int(0)] + _S877);
        float _S964 = _S963.x + _S963.y + _S963.z;
        float4  _S965 = _S908;
        *&((&_S965)->x) = _S964;
        _S933 = _S933 + _S965;
    }
    else
    {
        _S933 = _S908;
    }
    *v_depths_3 = _S933;
    *v_gt_normal_1 = raydir_15;
    return;
}

inline __device__ float3  generate_ray_d2n_prism(float2  pix_pos_6, float4  intrins_16, FixedArray<float, 8>  dist_coeffs_20, int camera_model_18, bool is_ray_depth_16)
{
    float3  _S966;
    for(;;)
    {
        float2  uv_52 = (pix_pos_6 - float2 {intrins_16.z, intrins_16.w}) / float2 {intrins_16.x, intrins_16.y};
        FixedArray<float, 8>  _S967 = dist_coeffs_20;
        float2  uv_u_24;
        bool _S968 = undistort_point_2(uv_52, &_S967, int(12), &uv_u_24);
        if(!_S968)
        {
            int3  _S969 = make_int3 (int(0));
            float3  _S970 = make_float3 ((float)_S969.x, (float)_S969.y, (float)_S969.z);
            _S966 = _S970;
            break;
        }
        _S966 = unproject_raydir_0(uv_u_24, camera_model_18, is_ray_depth_16);
        break;
    }
    return _S966;
}

inline __device__ float3  depth_to_point_prism(float2  pix_pos_7, float4  intrins_17, FixedArray<float, 8>  dist_coeffs_21, int camera_model_19, bool is_ray_depth_17, float depth_6)
{
    float3  _S971;
    for(;;)
    {
        float2  uv_53 = (pix_pos_7 - float2 {intrins_17.z, intrins_17.w}) / float2 {intrins_17.x, intrins_17.y};
        FixedArray<float, 8>  _S972 = dist_coeffs_21;
        float2  uv_u_25;
        bool _S973 = undistort_point_2(uv_53, &_S972, int(12), &uv_u_25);
        if(!_S973)
        {
            _S971 = make_float3 (0.0f);
            break;
        }
        _S971 = make_float3 (depth_6) * unproject_raydir_0(uv_u_25, camera_model_19, is_ray_depth_17);
        break;
    }
    return _S971;
}

struct s_bwd_prop_depth_to_point_Intermediates_2
{
    float2  _S974;
    bool _S975;
};

inline __device__ float depth_to_point_vjp_prism(float2  pix_pos_8, float4  intrins_18, FixedArray<float, 8>  dist_coeffs_22, int camera_model_20, bool is_ray_depth_18, float depth_7, float3  v_point_2)
{
    float2  _S976 = make_float2 (0.0f);
    s_bwd_prop_depth_to_point_Intermediates_2 _S977;
    (&_S977)->_S974 = _S976;
    (&_S977)->_S975 = false;
    float2  uv_54 = (pix_pos_8 - float2 {intrins_18.z, intrins_18.w}) / float2 {intrins_18.x, intrins_18.y};
    float2  _S978 = _S976;
    FixedArray<float, 8>  _S979 = dist_coeffs_22;
    bool _S980 = undistort_point_2(uv_54, &_S979, int(12), &_S978);
    (&_S977)->_S974 = _S978;
    (&_S977)->_S975 = _S980;
    s_bwd_prop_depth_to_point_Intermediates_2 _S981 = _S977;
    float3  _S982 = make_float3 (0.0f);
    bool _S983 = !!_S977._S975;
    float3  _S984;
    if(_S983)
    {
        _S984 = s_primal_ctx_unproject_raydir_0(_S981._S974, camera_model_20, is_ray_depth_18);
    }
    else
    {
        _S984 = _S982;
    }
    if(_S983)
    {
        _S984 = _S984 * v_point_2;
    }
    else
    {
        _S984 = _S982;
    }
    return _S984.x + _S984.y + _S984.z;
}

inline __device__ float3  depth_to_normal_prism(float2  pix_center_10, float4  intrins_19, FixedArray<float, 8>  dist_coeffs_23, int camera_model_21, bool is_ray_depth_19, float4  depths_8)
{
    float3  normal_12;
    for(;;)
    {
        bool _S985;
        if((depths_8.x) == 0.0f)
        {
            _S985 = true;
        }
        else
        {
            _S985 = (depths_8.y) == 0.0f;
        }
        if(_S985)
        {
            _S985 = true;
        }
        else
        {
            _S985 = (depths_8.z) == 0.0f;
        }
        if(_S985)
        {
            _S985 = true;
        }
        else
        {
            _S985 = (depths_8.w) == 0.0f;
        }
        if(_S985)
        {
            normal_12 = make_float3 (0.0f);
            break;
        }
        float3  * _S986;
        float3  * _S987;
        float3  * _S988;
        float3  * _S989;
        int _S990;
        FixedArray<float3 , 4>  points_10;
        for(;;)
        {
            float2  _S991 = float2 {intrins_19.z, intrins_19.w};
            float2  _S992 = float2 {intrins_19.x, intrins_19.y};
            float2  uv_55 = (pix_center_10 + make_float2 (-1.0f, -0.0f) - _S991) / _S992;
            FixedArray<float, 8>  _S993 = dist_coeffs_23;
            float2  uv_u_26;
            bool _S994 = undistort_point_2(uv_55, &_S993, int(12), &uv_u_26);
            if(!_S994)
            {
                float3  _S995 = make_float3 (0.0f);
                _S990 = int(0);
                _S989 = nullptr;
                _S988 = nullptr;
                _S987 = nullptr;
                _S986 = nullptr;
                normal_12 = _S995;
                break;
            }
            points_10[int(0)] = make_float3 (depths_8.x) * unproject_raydir_0(uv_u_26, camera_model_21, is_ray_depth_19);
            for(;;)
            {
                float2  uv_56 = (pix_center_10 + make_float2 (1.0f, -0.0f) - _S991) / _S992;
                FixedArray<float, 8>  _S996 = dist_coeffs_23;
                float2  uv_u_27;
                bool _S997 = undistort_point_2(uv_56, &_S996, int(12), &uv_u_27);
                if(!_S997)
                {
                    float3  _S998 = make_float3 (0.0f);
                    _S990 = int(0);
                    _S989 = nullptr;
                    normal_12 = _S998;
                    break;
                }
                points_10[int(1)] = make_float3 (depths_8.y) * unproject_raydir_0(uv_u_27, camera_model_21, is_ray_depth_19);
                _S990 = int(2);
                _S989 = &points_10[int(1)];
                break;
            }
            if(_S990 != int(2))
            {
                _S988 = &points_10[int(0)];
                _S987 = nullptr;
                _S986 = nullptr;
                break;
            }
            float2  uv_57 = (pix_center_10 + make_float2 (0.0f, -1.0f) - _S991) / _S992;
            FixedArray<float, 8>  _S999 = dist_coeffs_23;
            float2  uv_u_28;
            bool _S1000 = undistort_point_2(uv_57, &_S999, int(12), &uv_u_28);
            if(!_S1000)
            {
                float3  _S1001 = make_float3 (0.0f);
                _S990 = int(0);
                _S988 = &points_10[int(0)];
                _S987 = nullptr;
                _S986 = nullptr;
                normal_12 = _S1001;
                break;
            }
            points_10[int(2)] = make_float3 (depths_8.z) * unproject_raydir_0(uv_u_28, camera_model_21, is_ray_depth_19);
            for(;;)
            {
                float2  uv_58 = (pix_center_10 + make_float2 (0.0f, 1.0f) - _S991) / _S992;
                FixedArray<float, 8>  _S1002 = dist_coeffs_23;
                float2  uv_u_29;
                bool _S1003 = undistort_point_2(uv_58, &_S1002, int(12), &uv_u_29);
                if(!_S1003)
                {
                    float3  _S1004 = make_float3 (0.0f);
                    _S990 = int(0);
                    _S988 = nullptr;
                    normal_12 = _S1004;
                    break;
                }
                points_10[int(3)] = make_float3 (depths_8.w) * unproject_raydir_0(uv_u_29, camera_model_21, is_ray_depth_19);
                _S990 = int(2);
                _S988 = &points_10[int(3)];
                break;
            }
            if(_S990 != int(2))
            {
                float3  * _S1005 = _S988;
                _S988 = &points_10[int(0)];
                _S987 = _S1005;
                _S986 = &points_10[int(2)];
                break;
            }
            float3  * _S1006 = _S988;
            _S990 = int(1);
            _S988 = &points_10[int(0)];
            _S987 = _S1006;
            _S986 = &points_10[int(2)];
            break;
        }
        if(_S990 != int(1))
        {
            break;
        }
        float3  normal_13 = cross_0(*_S989 - *_S988, - (*_S987 - *_S986));
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
    float2  _S1007;
    bool _S1008;
    float2  _S1009;
    bool _S1010;
    float2  _S1011;
    bool _S1012;
    float2  _S1013;
    bool _S1014;
};

inline __device__ void depth_to_normal_vjp_prism(float2  pix_center_11, float4  intrins_20, FixedArray<float, 8>  dist_coeffs_24, int camera_model_22, bool is_ray_depth_20, float4  depths_9, float3  v_normal_3, float4  * v_depths_4)
{
    float2  _S1015 = make_float2 (0.0f);
    s_bwd_prop_depth_to_normal_Intermediates_2 _S1016;
    (&_S1016)->_S1007 = _S1015;
    (&_S1016)->_S1008 = false;
    (&_S1016)->_S1009 = _S1015;
    (&_S1016)->_S1010 = false;
    (&_S1016)->_S1011 = _S1015;
    (&_S1016)->_S1012 = false;
    (&_S1016)->_S1013 = _S1015;
    (&_S1016)->_S1014 = false;
    (&_S1016)->_S1007 = _S1015;
    (&_S1016)->_S1008 = false;
    (&_S1016)->_S1009 = _S1015;
    (&_S1016)->_S1010 = false;
    (&_S1016)->_S1011 = _S1015;
    (&_S1016)->_S1012 = false;
    (&_S1016)->_S1013 = _S1015;
    (&_S1016)->_S1014 = false;
    bool _S1017 = (depths_9.x) == 0.0f;
    bool _runFlag_16;
    if(_S1017)
    {
        _runFlag_16 = true;
    }
    else
    {
        _runFlag_16 = (depths_9.y) == 0.0f;
    }
    if(_runFlag_16)
    {
        _runFlag_16 = true;
    }
    else
    {
        _runFlag_16 = (depths_9.z) == 0.0f;
    }
    if(_runFlag_16)
    {
        _runFlag_16 = true;
    }
    else
    {
        _runFlag_16 = (depths_9.w) == 0.0f;
    }
    int _S1018;
    if(!_runFlag_16)
    {
        float2  _S1019 = float2 {intrins_20.z, intrins_20.w};
        float2  _S1020 = float2 {intrins_20.x, intrins_20.y};
        float2  uv_59 = (pix_center_11 + make_float2 (-1.0f, -0.0f) - _S1019) / _S1020;
        float2  _S1021 = _S1015;
        FixedArray<float, 8>  _S1022 = dist_coeffs_24;
        bool _S1023 = undistort_point_2(uv_59, &_S1022, int(12), &_S1021);
        (&_S1016)->_S1007 = _S1021;
        (&_S1016)->_S1008 = _S1023;
        bool _S1024 = !!_S1023;
        if(_S1024)
        {
            float2  uv_60 = (pix_center_11 + make_float2 (1.0f, -0.0f) - _S1019) / _S1020;
            float2  _S1025 = _S1015;
            FixedArray<float, 8>  _S1026 = dist_coeffs_24;
            bool _S1027 = undistort_point_2(uv_60, &_S1026, int(12), &_S1025);
            (&_S1016)->_S1009 = _S1025;
            (&_S1016)->_S1010 = _S1027;
            if(!!_S1027)
            {
                _S1018 = int(2);
            }
            else
            {
                _S1018 = int(0);
            }
            if(_S1018 != int(2))
            {
                _runFlag_16 = false;
            }
            else
            {
                _runFlag_16 = _S1024;
            }
            if(_runFlag_16)
            {
                float2  uv_61 = (pix_center_11 + make_float2 (0.0f, -1.0f) - _S1019) / _S1020;
                float2  _S1028 = _S1015;
                FixedArray<float, 8>  _S1029 = dist_coeffs_24;
                bool _S1030 = undistort_point_2(uv_61, &_S1029, int(12), &_S1028);
                (&_S1016)->_S1011 = _S1028;
                (&_S1016)->_S1012 = _S1030;
                if(!_S1030)
                {
                    _runFlag_16 = false;
                }
                if(_runFlag_16)
                {
                    float2  uv_62 = (pix_center_11 + make_float2 (0.0f, 1.0f) - _S1019) / _S1020;
                    float2  _S1031 = _S1015;
                    FixedArray<float, 8>  _S1032 = dist_coeffs_24;
                    bool _S1033 = undistort_point_2(uv_62, &_S1032, int(12), &_S1031);
                    (&_S1016)->_S1013 = _S1031;
                    (&_S1016)->_S1014 = _S1033;
                }
            }
        }
    }
    s_bwd_prop_depth_to_normal_Intermediates_2 _S1034 = _S1016;
    float3  _S1035 = make_float3 (0.0f);
    if(_S1017)
    {
        _runFlag_16 = true;
    }
    else
    {
        _runFlag_16 = (depths_9.y) == 0.0f;
    }
    if(_runFlag_16)
    {
        _runFlag_16 = true;
    }
    else
    {
        _runFlag_16 = (depths_9.z) == 0.0f;
    }
    if(_runFlag_16)
    {
        _runFlag_16 = true;
    }
    else
    {
        _runFlag_16 = (depths_9.w) == 0.0f;
    }
    bool _S1036 = !_runFlag_16;
    bool _runFlag_17;
    bool _runFlag_18;
    bool _S1037;
    bool _runFlag_19;
    bool _S1038;
    bool _S1039;
    FixedArray<float3 , 4>  points_11;
    float3  _S1040;
    float3  _S1041;
    float3  _S1042;
    float3  _S1043;
    float3  _S1044;
    float3  _S1045;
    float3  _S1046;
    float3  _S1047;
    float3  _S1048;
    if(_S1036)
    {
        bool _S1049 = !!_S1034._S1008;
        if(_S1049)
        {
            float3  _S1050 = s_primal_ctx_unproject_raydir_0(_S1034._S1007, camera_model_22, is_ray_depth_20);
            float3  _S1051 = make_float3 (depths_9.x) * _S1050;
            bool _S1052 = !!_S1034._S1010;
            if(_S1052)
            {
                float3  _S1053 = s_primal_ctx_unproject_raydir_0(_S1034._S1009, camera_model_22, is_ray_depth_20);
                float3  _S1054 = make_float3 (depths_9.y) * _S1053;
                _S1018 = int(2);
                points_11[int(0)] = _S1051;
                points_11[int(1)] = _S1054;
                points_11[int(2)] = _S1035;
                points_11[int(3)] = _S1035;
                _S1040 = _S1053;
            }
            else
            {
                _S1018 = int(0);
                points_11[int(0)] = _S1051;
                points_11[int(1)] = _S1035;
                points_11[int(2)] = _S1035;
                points_11[int(3)] = _S1035;
                _S1040 = _S1035;
            }
            if(_S1018 != int(2))
            {
                _runFlag_16 = false;
            }
            else
            {
                _runFlag_16 = _S1049;
                _S1018 = int(0);
            }
            if(_runFlag_16)
            {
                if(!_S1034._S1012)
                {
                    _runFlag_17 = false;
                    _S1018 = int(0);
                }
                else
                {
                    _runFlag_17 = _runFlag_16;
                }
                if(_runFlag_17)
                {
                    float3  _S1055 = s_primal_ctx_unproject_raydir_0(_S1034._S1011, camera_model_22, is_ray_depth_20);
                    points_11[int(2)] = make_float3 (depths_9.z) * _S1055;
                    bool _S1056 = !!_S1034._S1014;
                    int _S1057;
                    if(_S1056)
                    {
                        float3  _S1058 = s_primal_ctx_unproject_raydir_0(_S1034._S1013, camera_model_22, is_ray_depth_20);
                        points_11[int(3)] = make_float3 (depths_9.w) * _S1058;
                        _S1057 = int(2);
                        _S1041 = _S1058;
                    }
                    else
                    {
                        _S1057 = int(0);
                        _S1041 = _S1035;
                    }
                    if(_S1057 != int(2))
                    {
                        _runFlag_18 = false;
                        _S1018 = _S1057;
                    }
                    else
                    {
                        _runFlag_18 = _runFlag_17;
                    }
                    if(_runFlag_18)
                    {
                        _S1018 = int(1);
                    }
                    _runFlag_18 = _S1056;
                    _S1042 = _S1055;
                }
                else
                {
                    _runFlag_18 = false;
                    _S1041 = _S1035;
                    _S1042 = _S1035;
                }
            }
            else
            {
                _runFlag_17 = false;
                _runFlag_18 = false;
                _S1041 = _S1035;
                _S1042 = _S1035;
            }
            float3  _S1059 = _S1040;
            _S1040 = _S1041;
            _S1041 = _S1042;
            _S1037 = _S1052;
            _S1042 = _S1059;
            _S1043 = _S1050;
        }
        else
        {
            _S1018 = int(0);
            points_11[int(0)] = _S1035;
            points_11[int(1)] = _S1035;
            points_11[int(2)] = _S1035;
            points_11[int(3)] = _S1035;
            _runFlag_16 = false;
            _runFlag_17 = false;
            _runFlag_18 = false;
            _S1040 = _S1035;
            _S1041 = _S1035;
            _S1037 = false;
            _S1042 = _S1035;
            _S1043 = _S1035;
        }
        if(_S1018 != int(1))
        {
            _runFlag_19 = false;
        }
        else
        {
            _runFlag_19 = _S1036;
        }
        if(_runFlag_19)
        {
            float3  dx_5 = points_11[int(1)] - points_11[int(0)];
            float3  _S1060 = - (points_11[int(3)] - points_11[int(2)]);
            float3  _S1061 = s_primal_ctx_cross_0(dx_5, _S1060);
            bool _S1062 = (s_primal_ctx_dot_0(_S1061, _S1061)) != 0.0f;
            if(_S1062)
            {
                float _S1063 = length_0(_S1061);
                float3  _S1064 = make_float3 (_S1063);
                _S1044 = make_float3 (_S1063 * _S1063);
                _S1045 = _S1064;
            }
            else
            {
                _S1044 = _S1035;
                _S1045 = _S1035;
            }
            float3  _S1065 = _S1045;
            _S1038 = _S1062;
            _S1045 = _S1061;
            _S1046 = _S1065;
            _S1047 = dx_5;
            _S1048 = _S1060;
        }
        else
        {
            _S1038 = false;
            _S1044 = _S1035;
            _S1045 = _S1035;
            _S1046 = _S1035;
            _S1047 = _S1035;
            _S1048 = _S1035;
        }
        bool _S1066 = _runFlag_16;
        bool _S1067 = _runFlag_17;
        bool _S1068 = _runFlag_18;
        float3  _S1069 = _S1040;
        float3  _S1070 = _S1041;
        bool _S1071 = _S1037;
        float3  _S1072 = _S1042;
        float3  _S1073 = _S1043;
        _runFlag_16 = _runFlag_19;
        _runFlag_17 = _S1038;
        _S1040 = _S1044;
        _S1041 = _S1045;
        _S1042 = _S1046;
        _S1043 = _S1047;
        _S1044 = _S1048;
        _runFlag_18 = _S1049;
        _S1037 = _S1066;
        _runFlag_19 = _S1067;
        _S1038 = _S1068;
        _S1045 = _S1069;
        _S1046 = _S1070;
        _S1039 = _S1071;
        _S1047 = _S1072;
        _S1048 = _S1073;
    }
    else
    {
        _runFlag_16 = false;
        _runFlag_17 = false;
        _S1040 = _S1035;
        _S1041 = _S1035;
        _S1042 = _S1035;
        _S1043 = _S1035;
        _S1044 = _S1035;
        _runFlag_18 = false;
        _S1037 = false;
        _runFlag_19 = false;
        _S1038 = false;
        _S1045 = _S1035;
        _S1046 = _S1035;
        _S1039 = false;
        _S1047 = _S1035;
        _S1048 = _S1035;
    }
    float4  _S1074 = make_float4 (0.0f);
    float4  _S1075;
    if(_S1036)
    {
        if(_runFlag_16)
        {
            if(_runFlag_17)
            {
                float3  _S1076 = v_normal_3 / _S1040;
                float3  _S1077 = _S1041 * - _S1076;
                float3  _S1078 = _S1042 * _S1076;
                float _S1079 = _S1077.x + _S1077.y + _S1077.z;
                DiffPair_vectorx3Cfloatx2C3x3E_0 _S1080;
                (&_S1080)->primal_0 = _S1041;
                (&_S1080)->differential_0 = _S1035;
                s_bwd_length_impl_0(&_S1080, _S1079);
                _S1040 = _S1078 + _S1080.differential_0;
            }
            else
            {
                _S1040 = v_normal_3;
            }
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1081;
            (&_S1081)->primal_0 = _S1041;
            (&_S1081)->differential_0 = _S1035;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1082;
            (&_S1082)->primal_0 = _S1041;
            (&_S1082)->differential_0 = _S1035;
            s_bwd_prop_dot_0(&_S1081, &_S1082, 0.0f);
            float3  _S1083 = _S1082.differential_0 + _S1081.differential_0 + _S1040;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1084;
            (&_S1084)->primal_0 = _S1043;
            (&_S1084)->differential_0 = _S1035;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1085;
            (&_S1085)->primal_0 = _S1044;
            (&_S1085)->differential_0 = _S1035;
            s_bwd_prop_cross_0(&_S1084, &_S1085, _S1083);
            float3  s_diff_dy_T_5 = - _S1085.differential_0;
            float3  _S1086 = - s_diff_dy_T_5;
            float3  _S1087 = - _S1084.differential_0;
            FixedArray<float3 , 4>  _S1088;
            _S1088[int(0)] = _S1035;
            _S1088[int(1)] = _S1035;
            _S1088[int(2)] = _S1035;
            _S1088[int(3)] = _S1035;
            _S1088[int(2)] = _S1086;
            _S1088[int(3)] = s_diff_dy_T_5;
            _S1088[int(0)] = _S1087;
            _S1088[int(1)] = _S1084.differential_0;
            points_11[int(0)] = _S1088[int(0)];
            points_11[int(1)] = _S1088[int(1)];
            points_11[int(2)] = _S1088[int(2)];
            points_11[int(3)] = _S1088[int(3)];
        }
        else
        {
            points_11[int(0)] = _S1035;
            points_11[int(1)] = _S1035;
            points_11[int(2)] = _S1035;
            points_11[int(3)] = _S1035;
        }
        if(_runFlag_18)
        {
            if(_S1037)
            {
                if(_runFlag_19)
                {
                    FixedArray<float3 , 4>  _S1089 = points_11;
                    FixedArray<float3 , 4>  _S1090 = points_11;
                    FixedArray<float3 , 4>  _S1091 = points_11;
                    FixedArray<float3 , 4>  _S1092 = points_11;
                    if(_S1038)
                    {
                        float3  _S1093 = _S1045 * _S1092[int(3)];
                        float _S1094 = _S1093.x + _S1093.y + _S1093.z;
                        float4  _S1095 = _S1074;
                        *&((&_S1095)->w) = _S1094;
                        points_11[int(0)] = _S1089[int(0)];
                        points_11[int(1)] = _S1090[int(1)];
                        points_11[int(2)] = _S1091[int(2)];
                        points_11[int(3)] = _S1035;
                        _S1075 = _S1095;
                    }
                    else
                    {
                        points_11[int(0)] = _S1089[int(0)];
                        points_11[int(1)] = _S1090[int(1)];
                        points_11[int(2)] = _S1091[int(2)];
                        points_11[int(3)] = _S1092[int(3)];
                        _S1075 = _S1074;
                    }
                    float3  _S1096 = _S1046 * points_11[int(2)];
                    float _S1097 = _S1096.x + _S1096.y + _S1096.z;
                    FixedArray<float3 , 4>  _S1098 = points_11;
                    FixedArray<float3 , 4>  _S1099 = points_11;
                    float4  _S1100 = _S1074;
                    *&((&_S1100)->z) = _S1097;
                    float4  _S1101 = _S1075 + _S1100;
                    points_11[int(0)] = points_11[int(0)];
                    points_11[int(1)] = _S1098[int(1)];
                    points_11[int(2)] = _S1035;
                    points_11[int(3)] = _S1099[int(3)];
                    _S1075 = _S1101;
                }
                else
                {
                    FixedArray<float3 , 4>  _S1102 = points_11;
                    FixedArray<float3 , 4>  _S1103 = points_11;
                    FixedArray<float3 , 4>  _S1104 = points_11;
                    points_11[int(0)] = points_11[int(0)];
                    points_11[int(1)] = _S1102[int(1)];
                    points_11[int(2)] = _S1103[int(2)];
                    points_11[int(3)] = _S1104[int(3)];
                    _S1075 = _S1074;
                }
            }
            else
            {
                FixedArray<float3 , 4>  _S1105 = points_11;
                FixedArray<float3 , 4>  _S1106 = points_11;
                FixedArray<float3 , 4>  _S1107 = points_11;
                points_11[int(0)] = points_11[int(0)];
                points_11[int(1)] = _S1105[int(1)];
                points_11[int(2)] = _S1106[int(2)];
                points_11[int(3)] = _S1107[int(3)];
                _S1075 = _S1074;
            }
            if(_S1039)
            {
                FixedArray<float3 , 4>  _S1108 = points_11;
                float3  _S1109 = _S1047 * points_11[int(1)];
                float _S1110 = _S1109.x + _S1109.y + _S1109.z;
                float4  _S1111 = _S1074;
                *&((&_S1111)->y) = _S1110;
                float4  _S1112 = _S1075 + _S1111;
                points_11[int(0)] = _S1035;
                points_11[int(1)] = _S1035;
                points_11[int(2)] = _S1035;
                points_11[int(3)] = _S1035;
                _S1040 = _S1108[int(0)];
                _S1075 = _S1112;
            }
            else
            {
                FixedArray<float3 , 4>  _S1113 = points_11;
                FixedArray<float3 , 4>  _S1114 = points_11;
                FixedArray<float3 , 4>  _S1115 = points_11;
                points_11[int(0)] = points_11[int(0)];
                points_11[int(1)] = _S1113[int(1)];
                points_11[int(2)] = _S1114[int(2)];
                points_11[int(3)] = _S1115[int(3)];
                _S1040 = _S1035;
            }
            float3  _S1116 = _S1048 * (points_11[int(0)] + _S1040);
            float _S1117 = _S1116.x + _S1116.y + _S1116.z;
            float4  _S1118 = _S1074;
            *&((&_S1118)->x) = _S1117;
            _S1075 = _S1075 + _S1118;
        }
        else
        {
            _S1075 = _S1074;
        }
    }
    else
    {
        _S1075 = _S1074;
    }
    *v_depths_4 = _S1075;
    return;
}

inline __device__ float ray_depth_to_linear_depth_factor_prism(float2  pix_center_12, float4  intrins_21, FixedArray<float, 8>  dist_coeffs_25, int camera_model_23)
{
    float _S1119;
    for(;;)
    {
        float2  uv_63 = (pix_center_12 - float2 {intrins_21.z, intrins_21.w}) / float2 {intrins_21.x, intrins_21.y};
        FixedArray<float, 8>  _S1120 = dist_coeffs_25;
        float2  uv_u_30;
        bool _S1121 = undistort_point_2(uv_63, &_S1120, int(12), &uv_u_30);
        if(!_S1121)
        {
            _S1119 = 0.0f;
            break;
        }
        float3  raydir_16 = unproject_raydir_0(uv_u_30, camera_model_23, false);
        _S1119 = float((F32_sign((raydir_16.z)))) / length_0(raydir_16);
        break;
    }
    return _S1119;
}

inline __device__ float depth_normal_loss_prism(float2  pix_center_13, float4  intrins_22, FixedArray<float, 8>  dist_coeffs_26, int camera_model_24, bool is_ray_depth_21, float4  depths_10, float3  gt_normal_4)
{
    float _S1122;
    for(;;)
    {
        float3  _S1123;
        float3  * _S1124;
        float3  * _S1125;
        float3  * _S1126;
        float3  * _S1127;
        int _S1128;
        FixedArray<float3 , 5>  points_12;
        for(;;)
        {
            float2  _S1129 = float2 {intrins_22.z, intrins_22.w};
            float2  _S1130 = float2 {intrins_22.x, intrins_22.y};
            float2  uv_64 = (pix_center_13 + make_float2 (-1.0f, -0.0f) - _S1129) / _S1130;
            FixedArray<float, 8>  _S1131 = dist_coeffs_26;
            float2  uv_u_31;
            bool _S1132 = undistort_point_2(uv_64, &_S1131, int(12), &uv_u_31);
            float3  _S1133 = make_float3 (0.0f);
            if(!_S1132)
            {
                _S1128 = int(0);
                _S1127 = nullptr;
                _S1126 = nullptr;
                _S1125 = nullptr;
                _S1124 = nullptr;
                _S1123 = _S1133;
                break;
            }
            float3  raydir_17 = unproject_raydir_0(uv_u_31, camera_model_24, is_ray_depth_21);
            points_12[int(0)] = make_float3 (depths_10.x) * raydir_17;
            float2  uv_65 = (pix_center_13 + make_float2 (1.0f, -0.0f) - _S1129) / _S1130;
            FixedArray<float, 8>  _S1134 = dist_coeffs_26;
            float2  uv_u_32;
            bool _S1135 = undistort_point_2(uv_65, &_S1134, int(12), &uv_u_32);
            if(!_S1135)
            {
                _S1128 = int(0);
                _S1127 = nullptr;
                _S1126 = &points_12[int(0)];
                _S1125 = nullptr;
                _S1124 = nullptr;
                _S1123 = _S1133;
                break;
            }
            float3  raydir_18 = unproject_raydir_0(uv_u_32, camera_model_24, is_ray_depth_21);
            points_12[int(1)] = make_float3 (depths_10.y) * raydir_18;
            float2  uv_66 = (pix_center_13 + make_float2 (0.0f, -1.0f) - _S1129) / _S1130;
            FixedArray<float, 8>  _S1136 = dist_coeffs_26;
            float2  uv_u_33;
            bool _S1137 = undistort_point_2(uv_66, &_S1136, int(12), &uv_u_33);
            if(!_S1137)
            {
                _S1128 = int(0);
                _S1127 = &points_12[int(1)];
                _S1126 = &points_12[int(0)];
                _S1125 = nullptr;
                _S1124 = nullptr;
                _S1123 = _S1133;
                break;
            }
            float3  raydir_19 = unproject_raydir_0(uv_u_33, camera_model_24, is_ray_depth_21);
            points_12[int(2)] = make_float3 (depths_10.z) * raydir_19;
            float2  uv_67 = (pix_center_13 + make_float2 (0.0f, 1.0f) - _S1129) / _S1130;
            FixedArray<float, 8>  _S1138 = dist_coeffs_26;
            float2  uv_u_34;
            bool _S1139 = undistort_point_2(uv_67, &_S1138, int(12), &uv_u_34);
            if(!_S1139)
            {
                _S1128 = int(0);
                _S1127 = &points_12[int(1)];
                _S1126 = &points_12[int(0)];
                _S1125 = nullptr;
                _S1124 = &points_12[int(2)];
                _S1123 = _S1133;
                break;
            }
            float3  raydir_20 = unproject_raydir_0(uv_u_34, camera_model_24, is_ray_depth_21);
            points_12[int(3)] = make_float3 (depths_10.w) * raydir_20;
            float2  uv_68 = (pix_center_13 + make_float2 (0.0f) * make_float2 (0.0f, 3.0f) - _S1129) / _S1130;
            FixedArray<float, 8>  _S1140 = dist_coeffs_26;
            float2  uv_u_35;
            bool _S1141 = undistort_point_2(uv_68, &_S1140, int(12), &uv_u_35);
            if(!_S1141)
            {
                _S1128 = int(0);
                _S1127 = &points_12[int(1)];
                _S1126 = &points_12[int(0)];
                _S1125 = &points_12[int(3)];
                _S1124 = &points_12[int(2)];
                _S1123 = _S1133;
                break;
            }
            float3  raydir_21 = unproject_raydir_0(uv_u_35, camera_model_24, is_ray_depth_21);
            _S1128 = int(1);
            _S1127 = &points_12[int(1)];
            _S1126 = &points_12[int(0)];
            _S1125 = &points_12[int(3)];
            _S1124 = &points_12[int(2)];
            _S1123 = raydir_21;
            break;
        }
        if(_S1128 != int(1))
        {
            _S1122 = 0.0f;
            break;
        }
        float3  normal_14 = cross_0(*_S1127 - *_S1126, - (*_S1125 - *_S1124));
        float3  normal_15;
        if((dot_0(normal_14, normal_14)) != 0.0f)
        {
            normal_15 = normalize_0(normal_14);
        }
        else
        {
            normal_15 = normal_14;
        }
        float3  _S1142;
        if((dot_0(gt_normal_4, gt_normal_4)) != 0.0f)
        {
            _S1142 = normalize_0(gt_normal_4);
        }
        else
        {
            _S1142 = gt_normal_4;
        }
        _S1122 = (1.0f - dot_0(normal_15, _S1142) + 0.00100000004749745f) / ((F32_max((dot_0(normal_15, - normalize_0(_S1123))), (0.0f))) + 0.00100000004749745f);
        break;
    }
    return _S1122;
}

struct s_bwd_prop_depth_normal_loss_Intermediates_2
{
    float2  _S1143;
    bool _S1144;
    float2  _S1145;
    bool _S1146;
    float2  _S1147;
    bool _S1148;
    float2  _S1149;
    bool _S1150;
    float2  _S1151;
    bool _S1152;
};

inline __device__ void depth_normal_loss_vjp_prism(float2  pix_center_14, float4  intrins_23, FixedArray<float, 8>  dist_coeffs_27, int camera_model_25, bool is_ray_depth_22, float4  depths_11, float3  gt_normal_5, float v_loss_2, float4  * v_depths_5, float3  * v_gt_normal_2)
{
    float2  _S1153 = make_float2 (0.0f);
    s_bwd_prop_depth_normal_loss_Intermediates_2 _S1154;
    (&_S1154)->_S1143 = _S1153;
    (&_S1154)->_S1144 = false;
    (&_S1154)->_S1145 = _S1153;
    (&_S1154)->_S1146 = false;
    (&_S1154)->_S1147 = _S1153;
    (&_S1154)->_S1148 = false;
    (&_S1154)->_S1149 = _S1153;
    (&_S1154)->_S1150 = false;
    (&_S1154)->_S1151 = _S1153;
    (&_S1154)->_S1152 = false;
    (&_S1154)->_S1145 = _S1153;
    (&_S1154)->_S1146 = false;
    (&_S1154)->_S1147 = _S1153;
    (&_S1154)->_S1148 = false;
    (&_S1154)->_S1149 = _S1153;
    (&_S1154)->_S1150 = false;
    (&_S1154)->_S1151 = _S1153;
    (&_S1154)->_S1152 = false;
    float2  _S1155 = float2 {intrins_23.z, intrins_23.w};
    float2  _S1156 = float2 {intrins_23.x, intrins_23.y};
    float2  uv_69 = (pix_center_14 + make_float2 (-1.0f, -0.0f) - _S1155) / _S1156;
    float2  _S1157 = _S1153;
    FixedArray<float, 8>  _S1158 = dist_coeffs_27;
    bool _S1159 = undistort_point_2(uv_69, &_S1158, int(12), &_S1157);
    (&_S1154)->_S1143 = _S1157;
    (&_S1154)->_S1144 = _S1159;
    bool _S1160 = !!_S1159;
    bool _runFlag_20;
    if(_S1160)
    {
        float2  uv_70 = (pix_center_14 + make_float2 (1.0f, -0.0f) - _S1155) / _S1156;
        float2  _S1161 = _S1153;
        FixedArray<float, 8>  _S1162 = dist_coeffs_27;
        bool _S1163 = undistort_point_2(uv_70, &_S1162, int(12), &_S1161);
        (&_S1154)->_S1145 = _S1161;
        (&_S1154)->_S1146 = _S1163;
        if(!_S1163)
        {
            _runFlag_20 = false;
        }
        else
        {
            _runFlag_20 = _S1160;
        }
        if(_runFlag_20)
        {
            float2  uv_71 = (pix_center_14 + make_float2 (0.0f, -1.0f) - _S1155) / _S1156;
            float2  _S1164 = _S1153;
            FixedArray<float, 8>  _S1165 = dist_coeffs_27;
            bool _S1166 = undistort_point_2(uv_71, &_S1165, int(12), &_S1164);
            (&_S1154)->_S1147 = _S1164;
            (&_S1154)->_S1148 = _S1166;
            if(!_S1166)
            {
                _runFlag_20 = false;
            }
            if(_runFlag_20)
            {
                float2  uv_72 = (pix_center_14 + make_float2 (0.0f, 1.0f) - _S1155) / _S1156;
                float2  _S1167 = _S1153;
                FixedArray<float, 8>  _S1168 = dist_coeffs_27;
                bool _S1169 = undistort_point_2(uv_72, &_S1168, int(12), &_S1167);
                (&_S1154)->_S1149 = _S1167;
                (&_S1154)->_S1150 = _S1169;
                if(!_S1169)
                {
                    _runFlag_20 = false;
                }
                if(_runFlag_20)
                {
                    float2  uv_73 = (pix_center_14 - _S1155) / _S1156;
                    float2  _S1170 = _S1153;
                    FixedArray<float, 8>  _S1171 = dist_coeffs_27;
                    bool _S1172 = undistort_point_2(uv_73, &_S1171, int(12), &_S1170);
                    (&_S1154)->_S1151 = _S1170;
                    (&_S1154)->_S1152 = _S1172;
                }
            }
        }
    }
    s_bwd_prop_depth_normal_loss_Intermediates_2 _S1173 = _S1154;
    float3  _S1174 = make_float3 (0.0f);
    bool _S1175 = !!_S1154._S1144;
    bool _runFlag_21;
    bool _runFlag_22;
    bool _runFlag_23;
    int _S1176;
    float3  raydir_22;
    float3  _S1177;
    float3  _S1178;
    float3  _S1179;
    float3  _S1180;
    FixedArray<float3 , 5>  points_13;
    if(_S1175)
    {
        float3  _S1181 = s_primal_ctx_unproject_raydir_0(_S1173._S1143, camera_model_25, is_ray_depth_22);
        float3  _S1182 = make_float3 (depths_11.x) * _S1181;
        if(!_S1173._S1146)
        {
            _runFlag_20 = false;
        }
        else
        {
            _runFlag_20 = _S1175;
        }
        if(_runFlag_20)
        {
            float3  _S1183 = s_primal_ctx_unproject_raydir_0(_S1173._S1145, camera_model_25, is_ray_depth_22);
            float3  _S1184 = make_float3 (depths_11.y) * _S1183;
            if(!_S1173._S1148)
            {
                _runFlag_21 = false;
            }
            else
            {
                _runFlag_21 = _runFlag_20;
            }
            if(_runFlag_21)
            {
                float3  _S1185 = s_primal_ctx_unproject_raydir_0(_S1173._S1147, camera_model_25, is_ray_depth_22);
                float3  _S1186 = make_float3 (depths_11.z) * _S1185;
                if(!_S1173._S1150)
                {
                    _runFlag_22 = false;
                }
                else
                {
                    _runFlag_22 = _runFlag_21;
                }
                if(_runFlag_22)
                {
                    float3  _S1187 = s_primal_ctx_unproject_raydir_0(_S1173._S1149, camera_model_25, is_ray_depth_22);
                    float3  _S1188 = make_float3 (depths_11.w) * _S1187;
                    if(!_S1173._S1152)
                    {
                        _runFlag_23 = false;
                    }
                    else
                    {
                        _runFlag_23 = _runFlag_22;
                    }
                    if(_runFlag_23)
                    {
                        float3  _S1189 = s_primal_ctx_unproject_raydir_0(_S1173._S1151, camera_model_25, is_ray_depth_22);
                        _S1176 = int(1);
                        raydir_22 = _S1189;
                    }
                    else
                    {
                        _S1176 = int(0);
                        raydir_22 = _S1187;
                    }
                    points_13[int(0)] = _S1182;
                    points_13[int(1)] = _S1184;
                    points_13[int(2)] = _S1186;
                    points_13[int(3)] = _S1188;
                    points_13[int(4)] = _S1174;
                    _S1177 = _S1187;
                }
                else
                {
                    _S1176 = int(0);
                    raydir_22 = _S1185;
                    points_13[int(0)] = _S1182;
                    points_13[int(1)] = _S1184;
                    points_13[int(2)] = _S1186;
                    points_13[int(3)] = _S1174;
                    points_13[int(4)] = _S1174;
                    _S1177 = _S1174;
                }
                _S1178 = _S1185;
            }
            else
            {
                _S1176 = int(0);
                raydir_22 = _S1183;
                points_13[int(0)] = _S1182;
                points_13[int(1)] = _S1184;
                points_13[int(2)] = _S1174;
                points_13[int(3)] = _S1174;
                points_13[int(4)] = _S1174;
                _runFlag_22 = false;
                _S1177 = _S1174;
                _S1178 = _S1174;
            }
            _S1179 = _S1183;
        }
        else
        {
            _S1176 = int(0);
            raydir_22 = _S1181;
            points_13[int(0)] = _S1182;
            points_13[int(1)] = _S1174;
            points_13[int(2)] = _S1174;
            points_13[int(3)] = _S1174;
            points_13[int(4)] = _S1174;
            _runFlag_21 = false;
            _runFlag_22 = false;
            _S1177 = _S1174;
            _S1178 = _S1174;
            _S1179 = _S1174;
        }
        _S1180 = _S1181;
    }
    else
    {
        _S1176 = int(0);
        points_13[int(0)] = _S1174;
        points_13[int(1)] = _S1174;
        points_13[int(2)] = _S1174;
        points_13[int(3)] = _S1174;
        points_13[int(4)] = _S1174;
        _runFlag_20 = false;
        _runFlag_21 = false;
        _runFlag_22 = false;
        _S1177 = _S1174;
        _S1178 = _S1174;
        _S1179 = _S1174;
        _S1180 = _S1174;
    }
    bool _S1190 = !(_S1176 != int(1));
    bool _S1191;
    float3  normal_16;
    float3  _S1192;
    float3  _S1193;
    float3  _S1194;
    float3  _S1195;
    float _S1196;
    float _S1197;
    float _S1198;
    float _S1199;
    if(_S1190)
    {
        float3  dx_6 = points_13[int(1)] - points_13[int(0)];
        float3  _S1200 = - (points_13[int(3)] - points_13[int(2)]);
        float3  _S1201 = s_primal_ctx_cross_0(dx_6, _S1200);
        bool _S1202 = (s_primal_ctx_dot_0(_S1201, _S1201)) != 0.0f;
        if(_S1202)
        {
            normal_16 = normalize_0(_S1201);
        }
        else
        {
            normal_16 = _S1201;
        }
        bool _S1203 = (s_primal_ctx_dot_0(gt_normal_5, gt_normal_5)) != 0.0f;
        if(_S1203)
        {
            _S1192 = normalize_0(gt_normal_5);
        }
        else
        {
            _S1192 = gt_normal_5;
        }
        float3  _S1204 = - normalize_0(raydir_22);
        float _S1205 = s_primal_ctx_dot_0(normal_16, _S1204);
        float _S1206 = 1.0f - s_primal_ctx_dot_0(normal_16, _S1192) + 0.00100000004749745f;
        float _S1207 = (F32_max((_S1205), (0.0f))) + 0.00100000004749745f;
        _S1196 = _S1207 * _S1207;
        _S1197 = _S1206;
        _S1198 = _S1207;
        _S1199 = _S1205;
        raydir_22 = normal_16;
        normal_16 = _S1204;
        _runFlag_23 = _S1203;
        _S1191 = _S1202;
        _S1193 = _S1201;
        _S1194 = dx_6;
        _S1195 = _S1200;
    }
    else
    {
        _S1196 = 0.0f;
        _S1197 = 0.0f;
        _S1198 = 0.0f;
        _S1199 = 0.0f;
        raydir_22 = _S1174;
        normal_16 = _S1174;
        _S1192 = _S1174;
        _runFlag_23 = false;
        _S1191 = false;
        _S1193 = _S1174;
        _S1194 = _S1174;
        _S1195 = _S1174;
    }
    float4  _S1208 = make_float4 (0.0f);
    if(_S1190)
    {
        float _S1209 = v_loss_2 / _S1196;
        float _S1210 = _S1197 * - _S1209;
        float s_diff_num_T_2 = _S1198 * _S1209;
        DiffPair_float_0 _S1211;
        (&_S1211)->primal_0 = _S1199;
        (&_S1211)->differential_0 = 0.0f;
        DiffPair_float_0 _S1212;
        (&_S1212)->primal_0 = 0.0f;
        (&_S1212)->differential_0 = 0.0f;
        _d_max_0(&_S1211, &_S1212, _S1210);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1213;
        (&_S1213)->primal_0 = raydir_22;
        (&_S1213)->differential_0 = _S1174;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1214;
        (&_S1214)->primal_0 = normal_16;
        (&_S1214)->differential_0 = _S1174;
        s_bwd_prop_dot_0(&_S1213, &_S1214, _S1211.differential_0);
        float _S1215 = - s_diff_num_T_2;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1216;
        (&_S1216)->primal_0 = raydir_22;
        (&_S1216)->differential_0 = _S1174;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1217;
        (&_S1217)->primal_0 = _S1192;
        (&_S1217)->differential_0 = _S1174;
        s_bwd_prop_dot_0(&_S1216, &_S1217, _S1215);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1218 = _S1217;
        float3  _S1219 = _S1213.differential_0 + _S1216.differential_0;
        if(_runFlag_23)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1220;
            (&_S1220)->primal_0 = gt_normal_5;
            (&_S1220)->differential_0 = _S1174;
            s_bwd_normalize_impl_0(&_S1220, _S1218.differential_0);
            raydir_22 = _S1220.differential_0;
        }
        else
        {
            raydir_22 = _S1218.differential_0;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1221;
        (&_S1221)->primal_0 = gt_normal_5;
        (&_S1221)->differential_0 = _S1174;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1222;
        (&_S1222)->primal_0 = gt_normal_5;
        (&_S1222)->differential_0 = _S1174;
        s_bwd_prop_dot_0(&_S1221, &_S1222, 0.0f);
        float3  _S1223 = _S1222.differential_0 + _S1221.differential_0 + raydir_22;
        if(_S1191)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1224;
            (&_S1224)->primal_0 = _S1193;
            (&_S1224)->differential_0 = _S1174;
            s_bwd_normalize_impl_0(&_S1224, _S1219);
            raydir_22 = _S1224.differential_0;
        }
        else
        {
            raydir_22 = _S1219;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1225;
        (&_S1225)->primal_0 = _S1193;
        (&_S1225)->differential_0 = _S1174;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1226;
        (&_S1226)->primal_0 = _S1193;
        (&_S1226)->differential_0 = _S1174;
        s_bwd_prop_dot_0(&_S1225, &_S1226, 0.0f);
        float3  _S1227 = _S1226.differential_0 + _S1225.differential_0 + raydir_22;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1228;
        (&_S1228)->primal_0 = _S1194;
        (&_S1228)->differential_0 = _S1174;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1229;
        (&_S1229)->primal_0 = _S1195;
        (&_S1229)->differential_0 = _S1174;
        s_bwd_prop_cross_0(&_S1228, &_S1229, _S1227);
        float3  s_diff_dy_T_6 = - _S1229.differential_0;
        float3  _S1230 = - s_diff_dy_T_6;
        float3  _S1231 = - _S1228.differential_0;
        FixedArray<float3 , 5>  _S1232;
        _S1232[int(0)] = _S1174;
        _S1232[int(1)] = _S1174;
        _S1232[int(2)] = _S1174;
        _S1232[int(3)] = _S1174;
        _S1232[int(4)] = _S1174;
        _S1232[int(2)] = _S1230;
        _S1232[int(3)] = s_diff_dy_T_6;
        _S1232[int(0)] = _S1231;
        _S1232[int(1)] = _S1228.differential_0;
        points_13[int(0)] = _S1232[int(0)];
        points_13[int(1)] = _S1232[int(1)];
        points_13[int(2)] = _S1232[int(2)];
        points_13[int(3)] = _S1232[int(3)];
        points_13[int(4)] = _S1232[int(4)];
        raydir_22 = _S1223;
    }
    else
    {
        points_13[int(0)] = _S1174;
        points_13[int(1)] = _S1174;
        points_13[int(2)] = _S1174;
        points_13[int(3)] = _S1174;
        points_13[int(4)] = _S1174;
        raydir_22 = _S1174;
    }
    float4  _S1233;
    if(_S1175)
    {
        if(_runFlag_20)
        {
            if(_runFlag_21)
            {
                if(_runFlag_22)
                {
                    FixedArray<float3 , 5>  _S1234 = points_13;
                    FixedArray<float3 , 5>  _S1235 = points_13;
                    FixedArray<float3 , 5>  _S1236 = points_13;
                    float3  _S1237 = _S1177 * points_13[int(3)];
                    float _S1238 = _S1237.x + _S1237.y + _S1237.z;
                    float4  _S1239 = _S1208;
                    *&((&_S1239)->w) = _S1238;
                    points_13[int(0)] = _S1174;
                    points_13[int(1)] = _S1174;
                    points_13[int(2)] = _S1174;
                    points_13[int(3)] = _S1174;
                    points_13[int(4)] = _S1174;
                    _S1177 = _S1236[int(2)];
                    normal_16 = _S1234[int(0)];
                    _S1192 = _S1235[int(1)];
                    _S1233 = _S1239;
                }
                else
                {
                    FixedArray<float3 , 5>  _S1240 = points_13;
                    FixedArray<float3 , 5>  _S1241 = points_13;
                    FixedArray<float3 , 5>  _S1242 = points_13;
                    FixedArray<float3 , 5>  _S1243 = points_13;
                    points_13[int(0)] = points_13[int(0)];
                    points_13[int(1)] = _S1240[int(1)];
                    points_13[int(2)] = _S1241[int(2)];
                    points_13[int(3)] = _S1242[int(3)];
                    points_13[int(4)] = _S1243[int(4)];
                    _S1177 = _S1174;
                    normal_16 = _S1174;
                    _S1192 = _S1174;
                    _S1233 = _S1208;
                }
                float3  _S1244 = _S1178 * (points_13[int(2)] + _S1177);
                float _S1245 = _S1244.x + _S1244.y + _S1244.z;
                float3  _S1246 = points_13[int(0)] + normal_16;
                float3  _S1247 = points_13[int(1)] + _S1192;
                float4  _S1248 = _S1208;
                *&((&_S1248)->z) = _S1245;
                float4  _S1249 = _S1233 + _S1248;
                points_13[int(0)] = _S1174;
                points_13[int(1)] = _S1174;
                points_13[int(2)] = _S1174;
                points_13[int(3)] = _S1174;
                points_13[int(4)] = _S1174;
                _S1177 = _S1247;
                _S1178 = _S1246;
                _S1233 = _S1249;
            }
            else
            {
                FixedArray<float3 , 5>  _S1250 = points_13;
                FixedArray<float3 , 5>  _S1251 = points_13;
                FixedArray<float3 , 5>  _S1252 = points_13;
                FixedArray<float3 , 5>  _S1253 = points_13;
                points_13[int(0)] = points_13[int(0)];
                points_13[int(1)] = _S1250[int(1)];
                points_13[int(2)] = _S1251[int(2)];
                points_13[int(3)] = _S1252[int(3)];
                points_13[int(4)] = _S1253[int(4)];
                _S1177 = _S1174;
                _S1178 = _S1174;
                _S1233 = _S1208;
            }
            float3  _S1254 = _S1179 * (points_13[int(1)] + _S1177);
            float _S1255 = _S1254.x + _S1254.y + _S1254.z;
            float3  _S1256 = points_13[int(0)] + _S1178;
            float4  _S1257 = _S1208;
            *&((&_S1257)->y) = _S1255;
            float4  _S1258 = _S1233 + _S1257;
            points_13[int(0)] = _S1174;
            points_13[int(1)] = _S1174;
            points_13[int(2)] = _S1174;
            points_13[int(3)] = _S1174;
            points_13[int(4)] = _S1174;
            _S1177 = _S1256;
            _S1233 = _S1258;
        }
        else
        {
            FixedArray<float3 , 5>  _S1259 = points_13;
            FixedArray<float3 , 5>  _S1260 = points_13;
            FixedArray<float3 , 5>  _S1261 = points_13;
            FixedArray<float3 , 5>  _S1262 = points_13;
            points_13[int(0)] = points_13[int(0)];
            points_13[int(1)] = _S1259[int(1)];
            points_13[int(2)] = _S1260[int(2)];
            points_13[int(3)] = _S1261[int(3)];
            points_13[int(4)] = _S1262[int(4)];
            _S1177 = _S1174;
            _S1233 = _S1208;
        }
        float3  _S1263 = _S1180 * (points_13[int(0)] + _S1177);
        float _S1264 = _S1263.x + _S1263.y + _S1263.z;
        float4  _S1265 = _S1208;
        *&((&_S1265)->x) = _S1264;
        _S1233 = _S1233 + _S1265;
    }
    else
    {
        _S1233 = _S1208;
    }
    *v_depths_5 = _S1233;
    *v_gt_normal_2 = raydir_22;
    return;
}

inline __device__ float3  generate_ray_d2n_rational(float2  pix_pos_9, float4  intrins_24, FixedArray<float, 8>  dist_coeffs_28, int camera_model_26, bool is_ray_depth_23)
{
    float3  _S1266;
    for(;;)
    {
        float2  uv_74 = (pix_pos_9 - float2 {intrins_24.z, intrins_24.w}) / float2 {intrins_24.x, intrins_24.y};
        FixedArray<float, 8>  _S1267 = dist_coeffs_28;
        float2  uv_u_36;
        bool _S1268 = undistort_point_3(uv_74, &_S1267, int(12), &uv_u_36);
        if(!_S1268)
        {
            int3  _S1269 = make_int3 (int(0));
            float3  _S1270 = make_float3 ((float)_S1269.x, (float)_S1269.y, (float)_S1269.z);
            _S1266 = _S1270;
            break;
        }
        _S1266 = unproject_raydir_0(uv_u_36, camera_model_26, is_ray_depth_23);
        break;
    }
    return _S1266;
}

inline __device__ float3  depth_to_point_rational(float2  pix_pos_10, float4  intrins_25, FixedArray<float, 8>  dist_coeffs_29, int camera_model_27, bool is_ray_depth_24, float depth_8)
{
    float3  _S1271;
    for(;;)
    {
        float2  uv_75 = (pix_pos_10 - float2 {intrins_25.z, intrins_25.w}) / float2 {intrins_25.x, intrins_25.y};
        FixedArray<float, 8>  _S1272 = dist_coeffs_29;
        float2  uv_u_37;
        bool _S1273 = undistort_point_3(uv_75, &_S1272, int(12), &uv_u_37);
        if(!_S1273)
        {
            _S1271 = make_float3 (0.0f);
            break;
        }
        _S1271 = make_float3 (depth_8) * unproject_raydir_0(uv_u_37, camera_model_27, is_ray_depth_24);
        break;
    }
    return _S1271;
}

struct s_bwd_prop_depth_to_point_Intermediates_3
{
    float2  _S1274;
    bool _S1275;
};

inline __device__ float depth_to_point_vjp_rational(float2  pix_pos_11, float4  intrins_26, FixedArray<float, 8>  dist_coeffs_30, int camera_model_28, bool is_ray_depth_25, float depth_9, float3  v_point_3)
{
    float2  _S1276 = make_float2 (0.0f);
    s_bwd_prop_depth_to_point_Intermediates_3 _S1277;
    (&_S1277)->_S1274 = _S1276;
    (&_S1277)->_S1275 = false;
    float2  uv_76 = (pix_pos_11 - float2 {intrins_26.z, intrins_26.w}) / float2 {intrins_26.x, intrins_26.y};
    float2  _S1278 = _S1276;
    FixedArray<float, 8>  _S1279 = dist_coeffs_30;
    bool _S1280 = undistort_point_3(uv_76, &_S1279, int(12), &_S1278);
    (&_S1277)->_S1274 = _S1278;
    (&_S1277)->_S1275 = _S1280;
    s_bwd_prop_depth_to_point_Intermediates_3 _S1281 = _S1277;
    float3  _S1282 = make_float3 (0.0f);
    bool _S1283 = !!_S1277._S1275;
    float3  _S1284;
    if(_S1283)
    {
        _S1284 = s_primal_ctx_unproject_raydir_0(_S1281._S1274, camera_model_28, is_ray_depth_25);
    }
    else
    {
        _S1284 = _S1282;
    }
    if(_S1283)
    {
        _S1284 = _S1284 * v_point_3;
    }
    else
    {
        _S1284 = _S1282;
    }
    return _S1284.x + _S1284.y + _S1284.z;
}

inline __device__ float3  depth_to_normal_rational(float2  pix_center_15, float4  intrins_27, FixedArray<float, 8>  dist_coeffs_31, int camera_model_29, bool is_ray_depth_26, float4  depths_12)
{
    float3  normal_17;
    for(;;)
    {
        bool _S1285;
        if((depths_12.x) == 0.0f)
        {
            _S1285 = true;
        }
        else
        {
            _S1285 = (depths_12.y) == 0.0f;
        }
        if(_S1285)
        {
            _S1285 = true;
        }
        else
        {
            _S1285 = (depths_12.z) == 0.0f;
        }
        if(_S1285)
        {
            _S1285 = true;
        }
        else
        {
            _S1285 = (depths_12.w) == 0.0f;
        }
        if(_S1285)
        {
            normal_17 = make_float3 (0.0f);
            break;
        }
        float3  * _S1286;
        float3  * _S1287;
        float3  * _S1288;
        float3  * _S1289;
        int _S1290;
        FixedArray<float3 , 4>  points_14;
        for(;;)
        {
            float2  _S1291 = float2 {intrins_27.z, intrins_27.w};
            float2  _S1292 = float2 {intrins_27.x, intrins_27.y};
            float2  uv_77 = (pix_center_15 + make_float2 (-1.0f, -0.0f) - _S1291) / _S1292;
            FixedArray<float, 8>  _S1293 = dist_coeffs_31;
            float2  uv_u_38;
            bool _S1294 = undistort_point_3(uv_77, &_S1293, int(12), &uv_u_38);
            if(!_S1294)
            {
                float3  _S1295 = make_float3 (0.0f);
                _S1290 = int(0);
                _S1289 = nullptr;
                _S1288 = nullptr;
                _S1287 = nullptr;
                _S1286 = nullptr;
                normal_17 = _S1295;
                break;
            }
            points_14[int(0)] = make_float3 (depths_12.x) * unproject_raydir_0(uv_u_38, camera_model_29, is_ray_depth_26);
            for(;;)
            {
                float2  uv_78 = (pix_center_15 + make_float2 (1.0f, -0.0f) - _S1291) / _S1292;
                FixedArray<float, 8>  _S1296 = dist_coeffs_31;
                float2  uv_u_39;
                bool _S1297 = undistort_point_3(uv_78, &_S1296, int(12), &uv_u_39);
                if(!_S1297)
                {
                    float3  _S1298 = make_float3 (0.0f);
                    _S1290 = int(0);
                    _S1289 = nullptr;
                    normal_17 = _S1298;
                    break;
                }
                points_14[int(1)] = make_float3 (depths_12.y) * unproject_raydir_0(uv_u_39, camera_model_29, is_ray_depth_26);
                _S1290 = int(2);
                _S1289 = &points_14[int(1)];
                break;
            }
            if(_S1290 != int(2))
            {
                _S1288 = &points_14[int(0)];
                _S1287 = nullptr;
                _S1286 = nullptr;
                break;
            }
            float2  uv_79 = (pix_center_15 + make_float2 (0.0f, -1.0f) - _S1291) / _S1292;
            FixedArray<float, 8>  _S1299 = dist_coeffs_31;
            float2  uv_u_40;
            bool _S1300 = undistort_point_3(uv_79, &_S1299, int(12), &uv_u_40);
            if(!_S1300)
            {
                float3  _S1301 = make_float3 (0.0f);
                _S1290 = int(0);
                _S1288 = &points_14[int(0)];
                _S1287 = nullptr;
                _S1286 = nullptr;
                normal_17 = _S1301;
                break;
            }
            points_14[int(2)] = make_float3 (depths_12.z) * unproject_raydir_0(uv_u_40, camera_model_29, is_ray_depth_26);
            for(;;)
            {
                float2  uv_80 = (pix_center_15 + make_float2 (0.0f, 1.0f) - _S1291) / _S1292;
                FixedArray<float, 8>  _S1302 = dist_coeffs_31;
                float2  uv_u_41;
                bool _S1303 = undistort_point_3(uv_80, &_S1302, int(12), &uv_u_41);
                if(!_S1303)
                {
                    float3  _S1304 = make_float3 (0.0f);
                    _S1290 = int(0);
                    _S1288 = nullptr;
                    normal_17 = _S1304;
                    break;
                }
                points_14[int(3)] = make_float3 (depths_12.w) * unproject_raydir_0(uv_u_41, camera_model_29, is_ray_depth_26);
                _S1290 = int(2);
                _S1288 = &points_14[int(3)];
                break;
            }
            if(_S1290 != int(2))
            {
                float3  * _S1305 = _S1288;
                _S1288 = &points_14[int(0)];
                _S1287 = _S1305;
                _S1286 = &points_14[int(2)];
                break;
            }
            float3  * _S1306 = _S1288;
            _S1290 = int(1);
            _S1288 = &points_14[int(0)];
            _S1287 = _S1306;
            _S1286 = &points_14[int(2)];
            break;
        }
        if(_S1290 != int(1))
        {
            break;
        }
        float3  normal_18 = cross_0(*_S1289 - *_S1288, - (*_S1287 - *_S1286));
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
    float2  _S1307;
    bool _S1308;
    float2  _S1309;
    bool _S1310;
    float2  _S1311;
    bool _S1312;
    float2  _S1313;
    bool _S1314;
};

inline __device__ void depth_to_normal_vjp_rational(float2  pix_center_16, float4  intrins_28, FixedArray<float, 8>  dist_coeffs_32, int camera_model_30, bool is_ray_depth_27, float4  depths_13, float3  v_normal_4, float4  * v_depths_6)
{
    float2  _S1315 = make_float2 (0.0f);
    s_bwd_prop_depth_to_normal_Intermediates_3 _S1316;
    (&_S1316)->_S1307 = _S1315;
    (&_S1316)->_S1308 = false;
    (&_S1316)->_S1309 = _S1315;
    (&_S1316)->_S1310 = false;
    (&_S1316)->_S1311 = _S1315;
    (&_S1316)->_S1312 = false;
    (&_S1316)->_S1313 = _S1315;
    (&_S1316)->_S1314 = false;
    (&_S1316)->_S1307 = _S1315;
    (&_S1316)->_S1308 = false;
    (&_S1316)->_S1309 = _S1315;
    (&_S1316)->_S1310 = false;
    (&_S1316)->_S1311 = _S1315;
    (&_S1316)->_S1312 = false;
    (&_S1316)->_S1313 = _S1315;
    (&_S1316)->_S1314 = false;
    bool _S1317 = (depths_13.x) == 0.0f;
    bool _runFlag_24;
    if(_S1317)
    {
        _runFlag_24 = true;
    }
    else
    {
        _runFlag_24 = (depths_13.y) == 0.0f;
    }
    if(_runFlag_24)
    {
        _runFlag_24 = true;
    }
    else
    {
        _runFlag_24 = (depths_13.z) == 0.0f;
    }
    if(_runFlag_24)
    {
        _runFlag_24 = true;
    }
    else
    {
        _runFlag_24 = (depths_13.w) == 0.0f;
    }
    int _S1318;
    if(!_runFlag_24)
    {
        float2  _S1319 = float2 {intrins_28.z, intrins_28.w};
        float2  _S1320 = float2 {intrins_28.x, intrins_28.y};
        float2  uv_81 = (pix_center_16 + make_float2 (-1.0f, -0.0f) - _S1319) / _S1320;
        float2  _S1321 = _S1315;
        FixedArray<float, 8>  _S1322 = dist_coeffs_32;
        bool _S1323 = undistort_point_3(uv_81, &_S1322, int(12), &_S1321);
        (&_S1316)->_S1307 = _S1321;
        (&_S1316)->_S1308 = _S1323;
        bool _S1324 = !!_S1323;
        if(_S1324)
        {
            float2  uv_82 = (pix_center_16 + make_float2 (1.0f, -0.0f) - _S1319) / _S1320;
            float2  _S1325 = _S1315;
            FixedArray<float, 8>  _S1326 = dist_coeffs_32;
            bool _S1327 = undistort_point_3(uv_82, &_S1326, int(12), &_S1325);
            (&_S1316)->_S1309 = _S1325;
            (&_S1316)->_S1310 = _S1327;
            if(!!_S1327)
            {
                _S1318 = int(2);
            }
            else
            {
                _S1318 = int(0);
            }
            if(_S1318 != int(2))
            {
                _runFlag_24 = false;
            }
            else
            {
                _runFlag_24 = _S1324;
            }
            if(_runFlag_24)
            {
                float2  uv_83 = (pix_center_16 + make_float2 (0.0f, -1.0f) - _S1319) / _S1320;
                float2  _S1328 = _S1315;
                FixedArray<float, 8>  _S1329 = dist_coeffs_32;
                bool _S1330 = undistort_point_3(uv_83, &_S1329, int(12), &_S1328);
                (&_S1316)->_S1311 = _S1328;
                (&_S1316)->_S1312 = _S1330;
                if(!_S1330)
                {
                    _runFlag_24 = false;
                }
                if(_runFlag_24)
                {
                    float2  uv_84 = (pix_center_16 + make_float2 (0.0f, 1.0f) - _S1319) / _S1320;
                    float2  _S1331 = _S1315;
                    FixedArray<float, 8>  _S1332 = dist_coeffs_32;
                    bool _S1333 = undistort_point_3(uv_84, &_S1332, int(12), &_S1331);
                    (&_S1316)->_S1313 = _S1331;
                    (&_S1316)->_S1314 = _S1333;
                }
            }
        }
    }
    s_bwd_prop_depth_to_normal_Intermediates_3 _S1334 = _S1316;
    float3  _S1335 = make_float3 (0.0f);
    if(_S1317)
    {
        _runFlag_24 = true;
    }
    else
    {
        _runFlag_24 = (depths_13.y) == 0.0f;
    }
    if(_runFlag_24)
    {
        _runFlag_24 = true;
    }
    else
    {
        _runFlag_24 = (depths_13.z) == 0.0f;
    }
    if(_runFlag_24)
    {
        _runFlag_24 = true;
    }
    else
    {
        _runFlag_24 = (depths_13.w) == 0.0f;
    }
    bool _S1336 = !_runFlag_24;
    bool _runFlag_25;
    bool _runFlag_26;
    bool _S1337;
    bool _runFlag_27;
    bool _S1338;
    bool _S1339;
    FixedArray<float3 , 4>  points_15;
    float3  _S1340;
    float3  _S1341;
    float3  _S1342;
    float3  _S1343;
    float3  _S1344;
    float3  _S1345;
    float3  _S1346;
    float3  _S1347;
    float3  _S1348;
    if(_S1336)
    {
        bool _S1349 = !!_S1334._S1308;
        if(_S1349)
        {
            float3  _S1350 = s_primal_ctx_unproject_raydir_0(_S1334._S1307, camera_model_30, is_ray_depth_27);
            float3  _S1351 = make_float3 (depths_13.x) * _S1350;
            bool _S1352 = !!_S1334._S1310;
            if(_S1352)
            {
                float3  _S1353 = s_primal_ctx_unproject_raydir_0(_S1334._S1309, camera_model_30, is_ray_depth_27);
                float3  _S1354 = make_float3 (depths_13.y) * _S1353;
                _S1318 = int(2);
                points_15[int(0)] = _S1351;
                points_15[int(1)] = _S1354;
                points_15[int(2)] = _S1335;
                points_15[int(3)] = _S1335;
                _S1340 = _S1353;
            }
            else
            {
                _S1318 = int(0);
                points_15[int(0)] = _S1351;
                points_15[int(1)] = _S1335;
                points_15[int(2)] = _S1335;
                points_15[int(3)] = _S1335;
                _S1340 = _S1335;
            }
            if(_S1318 != int(2))
            {
                _runFlag_24 = false;
            }
            else
            {
                _runFlag_24 = _S1349;
                _S1318 = int(0);
            }
            if(_runFlag_24)
            {
                if(!_S1334._S1312)
                {
                    _runFlag_25 = false;
                    _S1318 = int(0);
                }
                else
                {
                    _runFlag_25 = _runFlag_24;
                }
                if(_runFlag_25)
                {
                    float3  _S1355 = s_primal_ctx_unproject_raydir_0(_S1334._S1311, camera_model_30, is_ray_depth_27);
                    points_15[int(2)] = make_float3 (depths_13.z) * _S1355;
                    bool _S1356 = !!_S1334._S1314;
                    int _S1357;
                    if(_S1356)
                    {
                        float3  _S1358 = s_primal_ctx_unproject_raydir_0(_S1334._S1313, camera_model_30, is_ray_depth_27);
                        points_15[int(3)] = make_float3 (depths_13.w) * _S1358;
                        _S1357 = int(2);
                        _S1341 = _S1358;
                    }
                    else
                    {
                        _S1357 = int(0);
                        _S1341 = _S1335;
                    }
                    if(_S1357 != int(2))
                    {
                        _runFlag_26 = false;
                        _S1318 = _S1357;
                    }
                    else
                    {
                        _runFlag_26 = _runFlag_25;
                    }
                    if(_runFlag_26)
                    {
                        _S1318 = int(1);
                    }
                    _runFlag_26 = _S1356;
                    _S1342 = _S1355;
                }
                else
                {
                    _runFlag_26 = false;
                    _S1341 = _S1335;
                    _S1342 = _S1335;
                }
            }
            else
            {
                _runFlag_25 = false;
                _runFlag_26 = false;
                _S1341 = _S1335;
                _S1342 = _S1335;
            }
            float3  _S1359 = _S1340;
            _S1340 = _S1341;
            _S1341 = _S1342;
            _S1337 = _S1352;
            _S1342 = _S1359;
            _S1343 = _S1350;
        }
        else
        {
            _S1318 = int(0);
            points_15[int(0)] = _S1335;
            points_15[int(1)] = _S1335;
            points_15[int(2)] = _S1335;
            points_15[int(3)] = _S1335;
            _runFlag_24 = false;
            _runFlag_25 = false;
            _runFlag_26 = false;
            _S1340 = _S1335;
            _S1341 = _S1335;
            _S1337 = false;
            _S1342 = _S1335;
            _S1343 = _S1335;
        }
        if(_S1318 != int(1))
        {
            _runFlag_27 = false;
        }
        else
        {
            _runFlag_27 = _S1336;
        }
        if(_runFlag_27)
        {
            float3  dx_7 = points_15[int(1)] - points_15[int(0)];
            float3  _S1360 = - (points_15[int(3)] - points_15[int(2)]);
            float3  _S1361 = s_primal_ctx_cross_0(dx_7, _S1360);
            bool _S1362 = (s_primal_ctx_dot_0(_S1361, _S1361)) != 0.0f;
            if(_S1362)
            {
                float _S1363 = length_0(_S1361);
                float3  _S1364 = make_float3 (_S1363);
                _S1344 = make_float3 (_S1363 * _S1363);
                _S1345 = _S1364;
            }
            else
            {
                _S1344 = _S1335;
                _S1345 = _S1335;
            }
            float3  _S1365 = _S1345;
            _S1338 = _S1362;
            _S1345 = _S1361;
            _S1346 = _S1365;
            _S1347 = dx_7;
            _S1348 = _S1360;
        }
        else
        {
            _S1338 = false;
            _S1344 = _S1335;
            _S1345 = _S1335;
            _S1346 = _S1335;
            _S1347 = _S1335;
            _S1348 = _S1335;
        }
        bool _S1366 = _runFlag_24;
        bool _S1367 = _runFlag_25;
        bool _S1368 = _runFlag_26;
        float3  _S1369 = _S1340;
        float3  _S1370 = _S1341;
        bool _S1371 = _S1337;
        float3  _S1372 = _S1342;
        float3  _S1373 = _S1343;
        _runFlag_24 = _runFlag_27;
        _runFlag_25 = _S1338;
        _S1340 = _S1344;
        _S1341 = _S1345;
        _S1342 = _S1346;
        _S1343 = _S1347;
        _S1344 = _S1348;
        _runFlag_26 = _S1349;
        _S1337 = _S1366;
        _runFlag_27 = _S1367;
        _S1338 = _S1368;
        _S1345 = _S1369;
        _S1346 = _S1370;
        _S1339 = _S1371;
        _S1347 = _S1372;
        _S1348 = _S1373;
    }
    else
    {
        _runFlag_24 = false;
        _runFlag_25 = false;
        _S1340 = _S1335;
        _S1341 = _S1335;
        _S1342 = _S1335;
        _S1343 = _S1335;
        _S1344 = _S1335;
        _runFlag_26 = false;
        _S1337 = false;
        _runFlag_27 = false;
        _S1338 = false;
        _S1345 = _S1335;
        _S1346 = _S1335;
        _S1339 = false;
        _S1347 = _S1335;
        _S1348 = _S1335;
    }
    float4  _S1374 = make_float4 (0.0f);
    float4  _S1375;
    if(_S1336)
    {
        if(_runFlag_24)
        {
            if(_runFlag_25)
            {
                float3  _S1376 = v_normal_4 / _S1340;
                float3  _S1377 = _S1341 * - _S1376;
                float3  _S1378 = _S1342 * _S1376;
                float _S1379 = _S1377.x + _S1377.y + _S1377.z;
                DiffPair_vectorx3Cfloatx2C3x3E_0 _S1380;
                (&_S1380)->primal_0 = _S1341;
                (&_S1380)->differential_0 = _S1335;
                s_bwd_length_impl_0(&_S1380, _S1379);
                _S1340 = _S1378 + _S1380.differential_0;
            }
            else
            {
                _S1340 = v_normal_4;
            }
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1381;
            (&_S1381)->primal_0 = _S1341;
            (&_S1381)->differential_0 = _S1335;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1382;
            (&_S1382)->primal_0 = _S1341;
            (&_S1382)->differential_0 = _S1335;
            s_bwd_prop_dot_0(&_S1381, &_S1382, 0.0f);
            float3  _S1383 = _S1382.differential_0 + _S1381.differential_0 + _S1340;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1384;
            (&_S1384)->primal_0 = _S1343;
            (&_S1384)->differential_0 = _S1335;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1385;
            (&_S1385)->primal_0 = _S1344;
            (&_S1385)->differential_0 = _S1335;
            s_bwd_prop_cross_0(&_S1384, &_S1385, _S1383);
            float3  s_diff_dy_T_7 = - _S1385.differential_0;
            float3  _S1386 = - s_diff_dy_T_7;
            float3  _S1387 = - _S1384.differential_0;
            FixedArray<float3 , 4>  _S1388;
            _S1388[int(0)] = _S1335;
            _S1388[int(1)] = _S1335;
            _S1388[int(2)] = _S1335;
            _S1388[int(3)] = _S1335;
            _S1388[int(2)] = _S1386;
            _S1388[int(3)] = s_diff_dy_T_7;
            _S1388[int(0)] = _S1387;
            _S1388[int(1)] = _S1384.differential_0;
            points_15[int(0)] = _S1388[int(0)];
            points_15[int(1)] = _S1388[int(1)];
            points_15[int(2)] = _S1388[int(2)];
            points_15[int(3)] = _S1388[int(3)];
        }
        else
        {
            points_15[int(0)] = _S1335;
            points_15[int(1)] = _S1335;
            points_15[int(2)] = _S1335;
            points_15[int(3)] = _S1335;
        }
        if(_runFlag_26)
        {
            if(_S1337)
            {
                if(_runFlag_27)
                {
                    FixedArray<float3 , 4>  _S1389 = points_15;
                    FixedArray<float3 , 4>  _S1390 = points_15;
                    FixedArray<float3 , 4>  _S1391 = points_15;
                    FixedArray<float3 , 4>  _S1392 = points_15;
                    if(_S1338)
                    {
                        float3  _S1393 = _S1345 * _S1392[int(3)];
                        float _S1394 = _S1393.x + _S1393.y + _S1393.z;
                        float4  _S1395 = _S1374;
                        *&((&_S1395)->w) = _S1394;
                        points_15[int(0)] = _S1389[int(0)];
                        points_15[int(1)] = _S1390[int(1)];
                        points_15[int(2)] = _S1391[int(2)];
                        points_15[int(3)] = _S1335;
                        _S1375 = _S1395;
                    }
                    else
                    {
                        points_15[int(0)] = _S1389[int(0)];
                        points_15[int(1)] = _S1390[int(1)];
                        points_15[int(2)] = _S1391[int(2)];
                        points_15[int(3)] = _S1392[int(3)];
                        _S1375 = _S1374;
                    }
                    float3  _S1396 = _S1346 * points_15[int(2)];
                    float _S1397 = _S1396.x + _S1396.y + _S1396.z;
                    FixedArray<float3 , 4>  _S1398 = points_15;
                    FixedArray<float3 , 4>  _S1399 = points_15;
                    float4  _S1400 = _S1374;
                    *&((&_S1400)->z) = _S1397;
                    float4  _S1401 = _S1375 + _S1400;
                    points_15[int(0)] = points_15[int(0)];
                    points_15[int(1)] = _S1398[int(1)];
                    points_15[int(2)] = _S1335;
                    points_15[int(3)] = _S1399[int(3)];
                    _S1375 = _S1401;
                }
                else
                {
                    FixedArray<float3 , 4>  _S1402 = points_15;
                    FixedArray<float3 , 4>  _S1403 = points_15;
                    FixedArray<float3 , 4>  _S1404 = points_15;
                    points_15[int(0)] = points_15[int(0)];
                    points_15[int(1)] = _S1402[int(1)];
                    points_15[int(2)] = _S1403[int(2)];
                    points_15[int(3)] = _S1404[int(3)];
                    _S1375 = _S1374;
                }
            }
            else
            {
                FixedArray<float3 , 4>  _S1405 = points_15;
                FixedArray<float3 , 4>  _S1406 = points_15;
                FixedArray<float3 , 4>  _S1407 = points_15;
                points_15[int(0)] = points_15[int(0)];
                points_15[int(1)] = _S1405[int(1)];
                points_15[int(2)] = _S1406[int(2)];
                points_15[int(3)] = _S1407[int(3)];
                _S1375 = _S1374;
            }
            if(_S1339)
            {
                FixedArray<float3 , 4>  _S1408 = points_15;
                float3  _S1409 = _S1347 * points_15[int(1)];
                float _S1410 = _S1409.x + _S1409.y + _S1409.z;
                float4  _S1411 = _S1374;
                *&((&_S1411)->y) = _S1410;
                float4  _S1412 = _S1375 + _S1411;
                points_15[int(0)] = _S1335;
                points_15[int(1)] = _S1335;
                points_15[int(2)] = _S1335;
                points_15[int(3)] = _S1335;
                _S1340 = _S1408[int(0)];
                _S1375 = _S1412;
            }
            else
            {
                FixedArray<float3 , 4>  _S1413 = points_15;
                FixedArray<float3 , 4>  _S1414 = points_15;
                FixedArray<float3 , 4>  _S1415 = points_15;
                points_15[int(0)] = points_15[int(0)];
                points_15[int(1)] = _S1413[int(1)];
                points_15[int(2)] = _S1414[int(2)];
                points_15[int(3)] = _S1415[int(3)];
                _S1340 = _S1335;
            }
            float3  _S1416 = _S1348 * (points_15[int(0)] + _S1340);
            float _S1417 = _S1416.x + _S1416.y + _S1416.z;
            float4  _S1418 = _S1374;
            *&((&_S1418)->x) = _S1417;
            _S1375 = _S1375 + _S1418;
        }
        else
        {
            _S1375 = _S1374;
        }
    }
    else
    {
        _S1375 = _S1374;
    }
    *v_depths_6 = _S1375;
    return;
}

inline __device__ float ray_depth_to_linear_depth_factor_rational(float2  pix_center_17, float4  intrins_29, FixedArray<float, 8>  dist_coeffs_33, int camera_model_31)
{
    float _S1419;
    for(;;)
    {
        float2  uv_85 = (pix_center_17 - float2 {intrins_29.z, intrins_29.w}) / float2 {intrins_29.x, intrins_29.y};
        FixedArray<float, 8>  _S1420 = dist_coeffs_33;
        float2  uv_u_42;
        bool _S1421 = undistort_point_3(uv_85, &_S1420, int(12), &uv_u_42);
        if(!_S1421)
        {
            _S1419 = 0.0f;
            break;
        }
        float3  raydir_23 = unproject_raydir_0(uv_u_42, camera_model_31, false);
        _S1419 = float((F32_sign((raydir_23.z)))) / length_0(raydir_23);
        break;
    }
    return _S1419;
}

inline __device__ float depth_normal_loss_rational(float2  pix_center_18, float4  intrins_30, FixedArray<float, 8>  dist_coeffs_34, int camera_model_32, bool is_ray_depth_28, float4  depths_14, float3  gt_normal_6)
{
    float _S1422;
    for(;;)
    {
        float3  _S1423;
        float3  * _S1424;
        float3  * _S1425;
        float3  * _S1426;
        float3  * _S1427;
        int _S1428;
        FixedArray<float3 , 5>  points_16;
        for(;;)
        {
            float2  _S1429 = float2 {intrins_30.z, intrins_30.w};
            float2  _S1430 = float2 {intrins_30.x, intrins_30.y};
            float2  uv_86 = (pix_center_18 + make_float2 (-1.0f, -0.0f) - _S1429) / _S1430;
            FixedArray<float, 8>  _S1431 = dist_coeffs_34;
            float2  uv_u_43;
            bool _S1432 = undistort_point_3(uv_86, &_S1431, int(12), &uv_u_43);
            float3  _S1433 = make_float3 (0.0f);
            if(!_S1432)
            {
                _S1428 = int(0);
                _S1427 = nullptr;
                _S1426 = nullptr;
                _S1425 = nullptr;
                _S1424 = nullptr;
                _S1423 = _S1433;
                break;
            }
            float3  raydir_24 = unproject_raydir_0(uv_u_43, camera_model_32, is_ray_depth_28);
            points_16[int(0)] = make_float3 (depths_14.x) * raydir_24;
            float2  uv_87 = (pix_center_18 + make_float2 (1.0f, -0.0f) - _S1429) / _S1430;
            FixedArray<float, 8>  _S1434 = dist_coeffs_34;
            float2  uv_u_44;
            bool _S1435 = undistort_point_3(uv_87, &_S1434, int(12), &uv_u_44);
            if(!_S1435)
            {
                _S1428 = int(0);
                _S1427 = nullptr;
                _S1426 = &points_16[int(0)];
                _S1425 = nullptr;
                _S1424 = nullptr;
                _S1423 = _S1433;
                break;
            }
            float3  raydir_25 = unproject_raydir_0(uv_u_44, camera_model_32, is_ray_depth_28);
            points_16[int(1)] = make_float3 (depths_14.y) * raydir_25;
            float2  uv_88 = (pix_center_18 + make_float2 (0.0f, -1.0f) - _S1429) / _S1430;
            FixedArray<float, 8>  _S1436 = dist_coeffs_34;
            float2  uv_u_45;
            bool _S1437 = undistort_point_3(uv_88, &_S1436, int(12), &uv_u_45);
            if(!_S1437)
            {
                _S1428 = int(0);
                _S1427 = &points_16[int(1)];
                _S1426 = &points_16[int(0)];
                _S1425 = nullptr;
                _S1424 = nullptr;
                _S1423 = _S1433;
                break;
            }
            float3  raydir_26 = unproject_raydir_0(uv_u_45, camera_model_32, is_ray_depth_28);
            points_16[int(2)] = make_float3 (depths_14.z) * raydir_26;
            float2  uv_89 = (pix_center_18 + make_float2 (0.0f, 1.0f) - _S1429) / _S1430;
            FixedArray<float, 8>  _S1438 = dist_coeffs_34;
            float2  uv_u_46;
            bool _S1439 = undistort_point_3(uv_89, &_S1438, int(12), &uv_u_46);
            if(!_S1439)
            {
                _S1428 = int(0);
                _S1427 = &points_16[int(1)];
                _S1426 = &points_16[int(0)];
                _S1425 = nullptr;
                _S1424 = &points_16[int(2)];
                _S1423 = _S1433;
                break;
            }
            float3  raydir_27 = unproject_raydir_0(uv_u_46, camera_model_32, is_ray_depth_28);
            points_16[int(3)] = make_float3 (depths_14.w) * raydir_27;
            float2  uv_90 = (pix_center_18 + make_float2 (0.0f) * make_float2 (0.0f, 3.0f) - _S1429) / _S1430;
            FixedArray<float, 8>  _S1440 = dist_coeffs_34;
            float2  uv_u_47;
            bool _S1441 = undistort_point_3(uv_90, &_S1440, int(12), &uv_u_47);
            if(!_S1441)
            {
                _S1428 = int(0);
                _S1427 = &points_16[int(1)];
                _S1426 = &points_16[int(0)];
                _S1425 = &points_16[int(3)];
                _S1424 = &points_16[int(2)];
                _S1423 = _S1433;
                break;
            }
            float3  raydir_28 = unproject_raydir_0(uv_u_47, camera_model_32, is_ray_depth_28);
            _S1428 = int(1);
            _S1427 = &points_16[int(1)];
            _S1426 = &points_16[int(0)];
            _S1425 = &points_16[int(3)];
            _S1424 = &points_16[int(2)];
            _S1423 = raydir_28;
            break;
        }
        if(_S1428 != int(1))
        {
            _S1422 = 0.0f;
            break;
        }
        float3  normal_19 = cross_0(*_S1427 - *_S1426, - (*_S1425 - *_S1424));
        float3  normal_20;
        if((dot_0(normal_19, normal_19)) != 0.0f)
        {
            normal_20 = normalize_0(normal_19);
        }
        else
        {
            normal_20 = normal_19;
        }
        float3  _S1442;
        if((dot_0(gt_normal_6, gt_normal_6)) != 0.0f)
        {
            _S1442 = normalize_0(gt_normal_6);
        }
        else
        {
            _S1442 = gt_normal_6;
        }
        _S1422 = (1.0f - dot_0(normal_20, _S1442) + 0.00100000004749745f) / ((F32_max((dot_0(normal_20, - normalize_0(_S1423))), (0.0f))) + 0.00100000004749745f);
        break;
    }
    return _S1422;
}

struct s_bwd_prop_depth_normal_loss_Intermediates_3
{
    float2  _S1443;
    bool _S1444;
    float2  _S1445;
    bool _S1446;
    float2  _S1447;
    bool _S1448;
    float2  _S1449;
    bool _S1450;
    float2  _S1451;
    bool _S1452;
};

inline __device__ void depth_normal_loss_vjp_rational(float2  pix_center_19, float4  intrins_31, FixedArray<float, 8>  dist_coeffs_35, int camera_model_33, bool is_ray_depth_29, float4  depths_15, float3  gt_normal_7, float v_loss_3, float4  * v_depths_7, float3  * v_gt_normal_3)
{
    float2  _S1453 = make_float2 (0.0f);
    s_bwd_prop_depth_normal_loss_Intermediates_3 _S1454;
    (&_S1454)->_S1443 = _S1453;
    (&_S1454)->_S1444 = false;
    (&_S1454)->_S1445 = _S1453;
    (&_S1454)->_S1446 = false;
    (&_S1454)->_S1447 = _S1453;
    (&_S1454)->_S1448 = false;
    (&_S1454)->_S1449 = _S1453;
    (&_S1454)->_S1450 = false;
    (&_S1454)->_S1451 = _S1453;
    (&_S1454)->_S1452 = false;
    (&_S1454)->_S1445 = _S1453;
    (&_S1454)->_S1446 = false;
    (&_S1454)->_S1447 = _S1453;
    (&_S1454)->_S1448 = false;
    (&_S1454)->_S1449 = _S1453;
    (&_S1454)->_S1450 = false;
    (&_S1454)->_S1451 = _S1453;
    (&_S1454)->_S1452 = false;
    float2  _S1455 = float2 {intrins_31.z, intrins_31.w};
    float2  _S1456 = float2 {intrins_31.x, intrins_31.y};
    float2  uv_91 = (pix_center_19 + make_float2 (-1.0f, -0.0f) - _S1455) / _S1456;
    float2  _S1457 = _S1453;
    FixedArray<float, 8>  _S1458 = dist_coeffs_35;
    bool _S1459 = undistort_point_3(uv_91, &_S1458, int(12), &_S1457);
    (&_S1454)->_S1443 = _S1457;
    (&_S1454)->_S1444 = _S1459;
    bool _S1460 = !!_S1459;
    bool _runFlag_28;
    if(_S1460)
    {
        float2  uv_92 = (pix_center_19 + make_float2 (1.0f, -0.0f) - _S1455) / _S1456;
        float2  _S1461 = _S1453;
        FixedArray<float, 8>  _S1462 = dist_coeffs_35;
        bool _S1463 = undistort_point_3(uv_92, &_S1462, int(12), &_S1461);
        (&_S1454)->_S1445 = _S1461;
        (&_S1454)->_S1446 = _S1463;
        if(!_S1463)
        {
            _runFlag_28 = false;
        }
        else
        {
            _runFlag_28 = _S1460;
        }
        if(_runFlag_28)
        {
            float2  uv_93 = (pix_center_19 + make_float2 (0.0f, -1.0f) - _S1455) / _S1456;
            float2  _S1464 = _S1453;
            FixedArray<float, 8>  _S1465 = dist_coeffs_35;
            bool _S1466 = undistort_point_3(uv_93, &_S1465, int(12), &_S1464);
            (&_S1454)->_S1447 = _S1464;
            (&_S1454)->_S1448 = _S1466;
            if(!_S1466)
            {
                _runFlag_28 = false;
            }
            if(_runFlag_28)
            {
                float2  uv_94 = (pix_center_19 + make_float2 (0.0f, 1.0f) - _S1455) / _S1456;
                float2  _S1467 = _S1453;
                FixedArray<float, 8>  _S1468 = dist_coeffs_35;
                bool _S1469 = undistort_point_3(uv_94, &_S1468, int(12), &_S1467);
                (&_S1454)->_S1449 = _S1467;
                (&_S1454)->_S1450 = _S1469;
                if(!_S1469)
                {
                    _runFlag_28 = false;
                }
                if(_runFlag_28)
                {
                    float2  uv_95 = (pix_center_19 - _S1455) / _S1456;
                    float2  _S1470 = _S1453;
                    FixedArray<float, 8>  _S1471 = dist_coeffs_35;
                    bool _S1472 = undistort_point_3(uv_95, &_S1471, int(12), &_S1470);
                    (&_S1454)->_S1451 = _S1470;
                    (&_S1454)->_S1452 = _S1472;
                }
            }
        }
    }
    s_bwd_prop_depth_normal_loss_Intermediates_3 _S1473 = _S1454;
    float3  _S1474 = make_float3 (0.0f);
    bool _S1475 = !!_S1454._S1444;
    bool _runFlag_29;
    bool _runFlag_30;
    bool _runFlag_31;
    int _S1476;
    float3  raydir_29;
    float3  _S1477;
    float3  _S1478;
    float3  _S1479;
    float3  _S1480;
    FixedArray<float3 , 5>  points_17;
    if(_S1475)
    {
        float3  _S1481 = s_primal_ctx_unproject_raydir_0(_S1473._S1443, camera_model_33, is_ray_depth_29);
        float3  _S1482 = make_float3 (depths_15.x) * _S1481;
        if(!_S1473._S1446)
        {
            _runFlag_28 = false;
        }
        else
        {
            _runFlag_28 = _S1475;
        }
        if(_runFlag_28)
        {
            float3  _S1483 = s_primal_ctx_unproject_raydir_0(_S1473._S1445, camera_model_33, is_ray_depth_29);
            float3  _S1484 = make_float3 (depths_15.y) * _S1483;
            if(!_S1473._S1448)
            {
                _runFlag_29 = false;
            }
            else
            {
                _runFlag_29 = _runFlag_28;
            }
            if(_runFlag_29)
            {
                float3  _S1485 = s_primal_ctx_unproject_raydir_0(_S1473._S1447, camera_model_33, is_ray_depth_29);
                float3  _S1486 = make_float3 (depths_15.z) * _S1485;
                if(!_S1473._S1450)
                {
                    _runFlag_30 = false;
                }
                else
                {
                    _runFlag_30 = _runFlag_29;
                }
                if(_runFlag_30)
                {
                    float3  _S1487 = s_primal_ctx_unproject_raydir_0(_S1473._S1449, camera_model_33, is_ray_depth_29);
                    float3  _S1488 = make_float3 (depths_15.w) * _S1487;
                    if(!_S1473._S1452)
                    {
                        _runFlag_31 = false;
                    }
                    else
                    {
                        _runFlag_31 = _runFlag_30;
                    }
                    if(_runFlag_31)
                    {
                        float3  _S1489 = s_primal_ctx_unproject_raydir_0(_S1473._S1451, camera_model_33, is_ray_depth_29);
                        _S1476 = int(1);
                        raydir_29 = _S1489;
                    }
                    else
                    {
                        _S1476 = int(0);
                        raydir_29 = _S1487;
                    }
                    points_17[int(0)] = _S1482;
                    points_17[int(1)] = _S1484;
                    points_17[int(2)] = _S1486;
                    points_17[int(3)] = _S1488;
                    points_17[int(4)] = _S1474;
                    _S1477 = _S1487;
                }
                else
                {
                    _S1476 = int(0);
                    raydir_29 = _S1485;
                    points_17[int(0)] = _S1482;
                    points_17[int(1)] = _S1484;
                    points_17[int(2)] = _S1486;
                    points_17[int(3)] = _S1474;
                    points_17[int(4)] = _S1474;
                    _S1477 = _S1474;
                }
                _S1478 = _S1485;
            }
            else
            {
                _S1476 = int(0);
                raydir_29 = _S1483;
                points_17[int(0)] = _S1482;
                points_17[int(1)] = _S1484;
                points_17[int(2)] = _S1474;
                points_17[int(3)] = _S1474;
                points_17[int(4)] = _S1474;
                _runFlag_30 = false;
                _S1477 = _S1474;
                _S1478 = _S1474;
            }
            _S1479 = _S1483;
        }
        else
        {
            _S1476 = int(0);
            raydir_29 = _S1481;
            points_17[int(0)] = _S1482;
            points_17[int(1)] = _S1474;
            points_17[int(2)] = _S1474;
            points_17[int(3)] = _S1474;
            points_17[int(4)] = _S1474;
            _runFlag_29 = false;
            _runFlag_30 = false;
            _S1477 = _S1474;
            _S1478 = _S1474;
            _S1479 = _S1474;
        }
        _S1480 = _S1481;
    }
    else
    {
        _S1476 = int(0);
        points_17[int(0)] = _S1474;
        points_17[int(1)] = _S1474;
        points_17[int(2)] = _S1474;
        points_17[int(3)] = _S1474;
        points_17[int(4)] = _S1474;
        _runFlag_28 = false;
        _runFlag_29 = false;
        _runFlag_30 = false;
        _S1477 = _S1474;
        _S1478 = _S1474;
        _S1479 = _S1474;
        _S1480 = _S1474;
    }
    bool _S1490 = !(_S1476 != int(1));
    bool _S1491;
    float3  normal_21;
    float3  _S1492;
    float3  _S1493;
    float3  _S1494;
    float3  _S1495;
    float _S1496;
    float _S1497;
    float _S1498;
    float _S1499;
    if(_S1490)
    {
        float3  dx_8 = points_17[int(1)] - points_17[int(0)];
        float3  _S1500 = - (points_17[int(3)] - points_17[int(2)]);
        float3  _S1501 = s_primal_ctx_cross_0(dx_8, _S1500);
        bool _S1502 = (s_primal_ctx_dot_0(_S1501, _S1501)) != 0.0f;
        if(_S1502)
        {
            normal_21 = normalize_0(_S1501);
        }
        else
        {
            normal_21 = _S1501;
        }
        bool _S1503 = (s_primal_ctx_dot_0(gt_normal_7, gt_normal_7)) != 0.0f;
        if(_S1503)
        {
            _S1492 = normalize_0(gt_normal_7);
        }
        else
        {
            _S1492 = gt_normal_7;
        }
        float3  _S1504 = - normalize_0(raydir_29);
        float _S1505 = s_primal_ctx_dot_0(normal_21, _S1504);
        float _S1506 = 1.0f - s_primal_ctx_dot_0(normal_21, _S1492) + 0.00100000004749745f;
        float _S1507 = (F32_max((_S1505), (0.0f))) + 0.00100000004749745f;
        _S1496 = _S1507 * _S1507;
        _S1497 = _S1506;
        _S1498 = _S1507;
        _S1499 = _S1505;
        raydir_29 = normal_21;
        normal_21 = _S1504;
        _runFlag_31 = _S1503;
        _S1491 = _S1502;
        _S1493 = _S1501;
        _S1494 = dx_8;
        _S1495 = _S1500;
    }
    else
    {
        _S1496 = 0.0f;
        _S1497 = 0.0f;
        _S1498 = 0.0f;
        _S1499 = 0.0f;
        raydir_29 = _S1474;
        normal_21 = _S1474;
        _S1492 = _S1474;
        _runFlag_31 = false;
        _S1491 = false;
        _S1493 = _S1474;
        _S1494 = _S1474;
        _S1495 = _S1474;
    }
    float4  _S1508 = make_float4 (0.0f);
    if(_S1490)
    {
        float _S1509 = v_loss_3 / _S1496;
        float _S1510 = _S1497 * - _S1509;
        float s_diff_num_T_3 = _S1498 * _S1509;
        DiffPair_float_0 _S1511;
        (&_S1511)->primal_0 = _S1499;
        (&_S1511)->differential_0 = 0.0f;
        DiffPair_float_0 _S1512;
        (&_S1512)->primal_0 = 0.0f;
        (&_S1512)->differential_0 = 0.0f;
        _d_max_0(&_S1511, &_S1512, _S1510);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1513;
        (&_S1513)->primal_0 = raydir_29;
        (&_S1513)->differential_0 = _S1474;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1514;
        (&_S1514)->primal_0 = normal_21;
        (&_S1514)->differential_0 = _S1474;
        s_bwd_prop_dot_0(&_S1513, &_S1514, _S1511.differential_0);
        float _S1515 = - s_diff_num_T_3;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1516;
        (&_S1516)->primal_0 = raydir_29;
        (&_S1516)->differential_0 = _S1474;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1517;
        (&_S1517)->primal_0 = _S1492;
        (&_S1517)->differential_0 = _S1474;
        s_bwd_prop_dot_0(&_S1516, &_S1517, _S1515);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1518 = _S1517;
        float3  _S1519 = _S1513.differential_0 + _S1516.differential_0;
        if(_runFlag_31)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1520;
            (&_S1520)->primal_0 = gt_normal_7;
            (&_S1520)->differential_0 = _S1474;
            s_bwd_normalize_impl_0(&_S1520, _S1518.differential_0);
            raydir_29 = _S1520.differential_0;
        }
        else
        {
            raydir_29 = _S1518.differential_0;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1521;
        (&_S1521)->primal_0 = gt_normal_7;
        (&_S1521)->differential_0 = _S1474;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1522;
        (&_S1522)->primal_0 = gt_normal_7;
        (&_S1522)->differential_0 = _S1474;
        s_bwd_prop_dot_0(&_S1521, &_S1522, 0.0f);
        float3  _S1523 = _S1522.differential_0 + _S1521.differential_0 + raydir_29;
        if(_S1491)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1524;
            (&_S1524)->primal_0 = _S1493;
            (&_S1524)->differential_0 = _S1474;
            s_bwd_normalize_impl_0(&_S1524, _S1519);
            raydir_29 = _S1524.differential_0;
        }
        else
        {
            raydir_29 = _S1519;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1525;
        (&_S1525)->primal_0 = _S1493;
        (&_S1525)->differential_0 = _S1474;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1526;
        (&_S1526)->primal_0 = _S1493;
        (&_S1526)->differential_0 = _S1474;
        s_bwd_prop_dot_0(&_S1525, &_S1526, 0.0f);
        float3  _S1527 = _S1526.differential_0 + _S1525.differential_0 + raydir_29;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1528;
        (&_S1528)->primal_0 = _S1494;
        (&_S1528)->differential_0 = _S1474;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1529;
        (&_S1529)->primal_0 = _S1495;
        (&_S1529)->differential_0 = _S1474;
        s_bwd_prop_cross_0(&_S1528, &_S1529, _S1527);
        float3  s_diff_dy_T_8 = - _S1529.differential_0;
        float3  _S1530 = - s_diff_dy_T_8;
        float3  _S1531 = - _S1528.differential_0;
        FixedArray<float3 , 5>  _S1532;
        _S1532[int(0)] = _S1474;
        _S1532[int(1)] = _S1474;
        _S1532[int(2)] = _S1474;
        _S1532[int(3)] = _S1474;
        _S1532[int(4)] = _S1474;
        _S1532[int(2)] = _S1530;
        _S1532[int(3)] = s_diff_dy_T_8;
        _S1532[int(0)] = _S1531;
        _S1532[int(1)] = _S1528.differential_0;
        points_17[int(0)] = _S1532[int(0)];
        points_17[int(1)] = _S1532[int(1)];
        points_17[int(2)] = _S1532[int(2)];
        points_17[int(3)] = _S1532[int(3)];
        points_17[int(4)] = _S1532[int(4)];
        raydir_29 = _S1523;
    }
    else
    {
        points_17[int(0)] = _S1474;
        points_17[int(1)] = _S1474;
        points_17[int(2)] = _S1474;
        points_17[int(3)] = _S1474;
        points_17[int(4)] = _S1474;
        raydir_29 = _S1474;
    }
    float4  _S1533;
    if(_S1475)
    {
        if(_runFlag_28)
        {
            if(_runFlag_29)
            {
                if(_runFlag_30)
                {
                    FixedArray<float3 , 5>  _S1534 = points_17;
                    FixedArray<float3 , 5>  _S1535 = points_17;
                    FixedArray<float3 , 5>  _S1536 = points_17;
                    float3  _S1537 = _S1477 * points_17[int(3)];
                    float _S1538 = _S1537.x + _S1537.y + _S1537.z;
                    float4  _S1539 = _S1508;
                    *&((&_S1539)->w) = _S1538;
                    points_17[int(0)] = _S1474;
                    points_17[int(1)] = _S1474;
                    points_17[int(2)] = _S1474;
                    points_17[int(3)] = _S1474;
                    points_17[int(4)] = _S1474;
                    _S1477 = _S1536[int(2)];
                    normal_21 = _S1534[int(0)];
                    _S1492 = _S1535[int(1)];
                    _S1533 = _S1539;
                }
                else
                {
                    FixedArray<float3 , 5>  _S1540 = points_17;
                    FixedArray<float3 , 5>  _S1541 = points_17;
                    FixedArray<float3 , 5>  _S1542 = points_17;
                    FixedArray<float3 , 5>  _S1543 = points_17;
                    points_17[int(0)] = points_17[int(0)];
                    points_17[int(1)] = _S1540[int(1)];
                    points_17[int(2)] = _S1541[int(2)];
                    points_17[int(3)] = _S1542[int(3)];
                    points_17[int(4)] = _S1543[int(4)];
                    _S1477 = _S1474;
                    normal_21 = _S1474;
                    _S1492 = _S1474;
                    _S1533 = _S1508;
                }
                float3  _S1544 = _S1478 * (points_17[int(2)] + _S1477);
                float _S1545 = _S1544.x + _S1544.y + _S1544.z;
                float3  _S1546 = points_17[int(0)] + normal_21;
                float3  _S1547 = points_17[int(1)] + _S1492;
                float4  _S1548 = _S1508;
                *&((&_S1548)->z) = _S1545;
                float4  _S1549 = _S1533 + _S1548;
                points_17[int(0)] = _S1474;
                points_17[int(1)] = _S1474;
                points_17[int(2)] = _S1474;
                points_17[int(3)] = _S1474;
                points_17[int(4)] = _S1474;
                _S1477 = _S1547;
                _S1478 = _S1546;
                _S1533 = _S1549;
            }
            else
            {
                FixedArray<float3 , 5>  _S1550 = points_17;
                FixedArray<float3 , 5>  _S1551 = points_17;
                FixedArray<float3 , 5>  _S1552 = points_17;
                FixedArray<float3 , 5>  _S1553 = points_17;
                points_17[int(0)] = points_17[int(0)];
                points_17[int(1)] = _S1550[int(1)];
                points_17[int(2)] = _S1551[int(2)];
                points_17[int(3)] = _S1552[int(3)];
                points_17[int(4)] = _S1553[int(4)];
                _S1477 = _S1474;
                _S1478 = _S1474;
                _S1533 = _S1508;
            }
            float3  _S1554 = _S1479 * (points_17[int(1)] + _S1477);
            float _S1555 = _S1554.x + _S1554.y + _S1554.z;
            float3  _S1556 = points_17[int(0)] + _S1478;
            float4  _S1557 = _S1508;
            *&((&_S1557)->y) = _S1555;
            float4  _S1558 = _S1533 + _S1557;
            points_17[int(0)] = _S1474;
            points_17[int(1)] = _S1474;
            points_17[int(2)] = _S1474;
            points_17[int(3)] = _S1474;
            points_17[int(4)] = _S1474;
            _S1477 = _S1556;
            _S1533 = _S1558;
        }
        else
        {
            FixedArray<float3 , 5>  _S1559 = points_17;
            FixedArray<float3 , 5>  _S1560 = points_17;
            FixedArray<float3 , 5>  _S1561 = points_17;
            FixedArray<float3 , 5>  _S1562 = points_17;
            points_17[int(0)] = points_17[int(0)];
            points_17[int(1)] = _S1559[int(1)];
            points_17[int(2)] = _S1560[int(2)];
            points_17[int(3)] = _S1561[int(3)];
            points_17[int(4)] = _S1562[int(4)];
            _S1477 = _S1474;
            _S1533 = _S1508;
        }
        float3  _S1563 = _S1480 * (points_17[int(0)] + _S1477);
        float _S1564 = _S1563.x + _S1563.y + _S1563.z;
        float4  _S1565 = _S1508;
        *&((&_S1565)->x) = _S1564;
        _S1533 = _S1533 + _S1565;
    }
    else
    {
        _S1533 = _S1508;
    }
    *v_depths_7 = _S1533;
    *v_gt_normal_3 = raydir_29;
    return;
}

