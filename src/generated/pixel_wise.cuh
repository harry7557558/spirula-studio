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

inline __device__ float3  min_0(float3  x_0, float3  y_0)
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
        *_slang_vector_get_element_ptr(&result_0, i_0) = (F32_min((_slang_vector_get_element(x_0, i_0)), (_slang_vector_get_element(y_0, i_0))));
        i_0 = i_0 + int(1);
    }
    return result_0;
}

inline __device__ float3  max_0(float3  x_1, float3  y_1)
{
    float3  result_1;
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
        *_slang_vector_get_element_ptr(&result_1, i_1) = (F32_max((_slang_vector_get_element(x_1, i_1)), (_slang_vector_get_element(y_1, i_1))));
        i_1 = i_1 + int(1);
    }
    return result_1;
}

inline __device__ float3  clamp_0(float3  x_2, float3  minBound_0, float3  maxBound_0)
{
    return min_0(max_0(x_2, minBound_0), maxBound_0);
}

inline __device__ float3  blend_background(float3  rgb_0, float transmittance_2, float3  background_0)
{
    return clamp_0(rgb_0 + make_float3 (transmittance_2, transmittance_2, transmittance_2) * background_0, make_float3 (0.0f), make_float3 (1.0f));
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

inline __device__ float dot_0(float3  x_3, float3  y_2)
{
    int i_2 = int(0);
    float result_2 = 0.0f;
    for(;;)
    {
        if(i_2 < int(3))
        {
        }
        else
        {
            break;
        }
        float result_3 = result_2 + _slang_vector_get_element(x_3, i_2) * _slang_vector_get_element(y_2, i_2);
        i_2 = i_2 + int(1);
        result_2 = result_3;
    }
    return result_2;
}

inline __device__ float dot_1(float2  x_4, float2  y_3)
{
    int i_3 = int(0);
    float result_4 = 0.0f;
    for(;;)
    {
        if(i_3 < int(2))
        {
        }
        else
        {
            break;
        }
        float result_5 = result_4 + _slang_vector_get_element(x_4, i_3) * _slang_vector_get_element(y_3, i_3);
        i_3 = i_3 + int(1);
        result_4 = result_5;
    }
    return result_4;
}

inline __device__ void blend_background_bwd(float3  rgb_1, float transmittance_3, float3  background_1, float3  v_out_rgb_0, float3  * v_rgb_0, float * v_transmittance_1, float3  * v_background_0)
{
    *v_rgb_0 = v_out_rgb_0;
    *v_transmittance_1 = dot_0(v_out_rgb_0, background_1);
    *v_background_0 = make_float3 (transmittance_3, transmittance_3, transmittance_3) * v_out_rgb_0;
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
        DiffPair_float_0 _S15 = *dpx_2;
        float _S16 = val_0 * (*dpy_2).primal_0 / (*dpx_2).primal_0 * dOut_2;
        dpx_2->primal_0 = (*dpx_2).primal_0;
        dpx_2->differential_0 = _S16;
        float _S17 = val_0 * (F32_log((_S15.primal_0))) * dOut_2;
        dpy_2->primal_0 = (*dpy_2).primal_0;
        dpy_2->differential_0 = _S17;
    }
    return;
}

inline __device__ DiffPair_float_0 _d_pow_1(DiffPair_float_0 * dpx_3, DiffPair_float_0 * dpy_3)
{
    float _S18 = dpx_3->primal_0;
    if((dpx_3->primal_0) < 9.99999997475242708e-07f)
    {
        DiffPair_float_0 _S19 = { 0.0f, 0.0f };
        return _S19;
    }
    float val_1 = (F32_pow((_S18), (dpy_3->primal_0)));
    DiffPair_float_0 _S20 = { val_1, val_1 * (F32_log((_S18))) * dpy_3->differential_0 + val_1 * dpy_3->primal_0 / _S18 * dpx_3->differential_0 };
    return _S20;
}

inline __device__ float linear_rgb_to_srgb(float x_5)
{
    float _S21;
    if(x_5 < 0.00313080009073019f)
    {
        _S21 = x_5 * 12.92000007629394531f;
    }
    else
    {
        _S21 = 1.0549999475479126f * (F32_pow((x_5), (0.4166666567325592f))) - 0.05499999970197678f;
    }
    return _S21;
}

inline __device__ float linear_rgb_to_srgb_grad(float x_6)
{
    float _S22;
    if(x_6 < 0.00313080009073019f)
    {
        _S22 = 12.92000007629394531f;
    }
    else
    {
        DiffPair_float_0 _S23;
        (&_S23)->primal_0 = x_6;
        (&_S23)->differential_0 = 1.0f;
        DiffPair_float_0 _S24;
        (&_S24)->primal_0 = 0.4166666567325592f;
        (&_S24)->differential_0 = 0.0f;
        DiffPair_float_0 _S25 = _d_pow_1(&_S23, &_S24);
        _S22 = _S25.differential_0 * 1.0549999475479126f;
    }
    return _S22;
}

inline __device__ float srgb_to_linear_rgb(float x_7)
{
    float _S26;
    if(x_7 < 0.04044999927282333f)
    {
        _S26 = x_7 * 0.07739938050508499f;
    }
    else
    {
        _S26 = (F32_pow((0.94786733388900757f * (x_7 + 0.05499999970197678f)), (2.40000009536743164f)));
    }
    return _S26;
}

inline __device__ float srgb_to_linear_rgb_grad(float x_8)
{
    float _S27;
    if(x_8 < 0.04044999927282333f)
    {
        _S27 = 0.07739938050508499f;
    }
    else
    {
        DiffPair_float_0 _S28;
        (&_S28)->primal_0 = 0.94786733388900757f * (x_8 + 0.05499999970197678f);
        (&_S28)->differential_0 = 0.94786733388900757f;
        DiffPair_float_0 _S29;
        (&_S29)->primal_0 = 2.40000009536743164f;
        (&_S29)->differential_0 = 0.0f;
        DiffPair_float_0 _S30 = _d_pow_1(&_S28, &_S29);
        _S27 = _S30.differential_0;
    }
    return _S27;
}

inline __device__ void _d_sqrt_0(DiffPair_float_0 * dpx_4, float dOut_3)
{
    float _S31 = 0.5f / (F32_sqrt(((F32_max((1.00000001168609742e-07f), ((*dpx_4).primal_0)))))) * dOut_3;
    dpx_4->primal_0 = (*dpx_4).primal_0;
    dpx_4->differential_0 = _S31;
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

inline __device__ float splat_dc_decode(float x_9)
{
    float s_1 = (F32_sqrt((0.50001537799835205f)));
    float w_1 = x_9 * 0.282094806432724f / s_1 + 2.0f * s_1;
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
    float _S32 = (*left_0).primal_0.rows[int(0)].x * dOut_4.x;
    Matrix<float, 3, 3>  left_d_result_0;
    *&(((&left_d_result_0)->rows + (int(0)))->x) = (*right_0).primal_0.x * dOut_4.x;
    float sum_0 = _S32 + (*left_0).primal_0.rows[int(1)].x * dOut_4.y;
    *&(((&left_d_result_0)->rows + (int(1)))->x) = (*right_0).primal_0.x * dOut_4.y;
    float sum_1 = sum_0 + (*left_0).primal_0.rows[int(2)].x * dOut_4.z;
    *&(((&left_d_result_0)->rows + (int(2)))->x) = (*right_0).primal_0.x * dOut_4.z;
    float3  right_d_result_0;
    *&((&right_d_result_0)->x) = sum_1;
    float _S33 = (*left_0).primal_0.rows[int(0)].y * dOut_4.x;
    *&(((&left_d_result_0)->rows + (int(0)))->y) = (*right_0).primal_0.y * dOut_4.x;
    float sum_2 = _S33 + (*left_0).primal_0.rows[int(1)].y * dOut_4.y;
    *&(((&left_d_result_0)->rows + (int(1)))->y) = (*right_0).primal_0.y * dOut_4.y;
    float sum_3 = sum_2 + (*left_0).primal_0.rows[int(2)].y * dOut_4.z;
    *&(((&left_d_result_0)->rows + (int(2)))->y) = (*right_0).primal_0.y * dOut_4.z;
    *&((&right_d_result_0)->y) = sum_3;
    float _S34 = (*left_0).primal_0.rows[int(0)].z * dOut_4.x;
    *&(((&left_d_result_0)->rows + (int(0)))->z) = (*right_0).primal_0.z * dOut_4.x;
    float sum_4 = _S34 + (*left_0).primal_0.rows[int(1)].z * dOut_4.y;
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
    float3  _S35 = mul_0(color_matrix_0, rgb_2);
    float _S36 = _S35.x;
    float _S37;
    if(_S36 < 0.00313080009073019f)
    {
        _S37 = _S36 * 12.92000007629394531f;
    }
    else
    {
        _S37 = 1.0549999475479126f * (F32_pow((_S36), (0.4166666567325592f))) - 0.05499999970197678f;
    }
    float _S38 = _S35.y;
    float _S39;
    if(_S38 < 0.00313080009073019f)
    {
        _S39 = _S38 * 12.92000007629394531f;
    }
    else
    {
        _S39 = 1.0549999475479126f * (F32_pow((_S38), (0.4166666567325592f))) - 0.05499999970197678f;
    }
    float _S40 = _S35.z;
    float _S41;
    if(_S40 < 0.00313080009073019f)
    {
        _S41 = _S40 * 12.92000007629394531f;
    }
    else
    {
        _S41 = 1.0549999475479126f * (F32_pow((_S40), (0.4166666567325592f))) - 0.05499999970197678f;
    }
    return make_float3 (_S37, _S39, _S41);
}

inline __device__ float3  s_primal_ctx_mul_0(Matrix<float, 3, 3>  _S42, float3  _S43)
{
    return mul_0(_S42, _S43);
}

inline __device__ void s_bwd_prop_pow_0(DiffPair_float_0 * _S44, DiffPair_float_0 * _S45, float _S46)
{
    _d_pow_0(_S44, _S45, _S46);
    return;
}

inline __device__ void s_bwd_prop_mul_0(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S47, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S48, float3  _S49)
{
    _d_mul_0(_S47, _S48, _S49);
    return;
}

inline __device__ void s_bwd_prop_linear_rgb_to_srgb_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dprgb_0, Matrix<float, 3, 3>  color_matrix_1, float3  _s_dOut_1)
{
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S50 = *dprgb_0;
    float3  _S51 = s_primal_ctx_mul_0(color_matrix_1, (*dprgb_0).primal_0);
    float _S52 = _S51.x;
    float _S53 = _S51.y;
    float _S54 = _S51.z;
    float _S55;
    if(_S54 < 0.00313080009073019f)
    {
        _S55 = 12.92000007629394531f * _s_dOut_1.z;
    }
    else
    {
        float _S56 = 1.0549999475479126f * _s_dOut_1.z;
        DiffPair_float_0 _S57;
        (&_S57)->primal_0 = _S54;
        (&_S57)->differential_0 = 0.0f;
        DiffPair_float_0 _S58;
        (&_S58)->primal_0 = 0.4166666567325592f;
        (&_S58)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S57, &_S58, _S56);
        _S55 = _S57.differential_0;
    }
    float _S59;
    if(_S53 < 0.00313080009073019f)
    {
        _S59 = 12.92000007629394531f * _s_dOut_1.y;
    }
    else
    {
        float _S60 = 1.0549999475479126f * _s_dOut_1.y;
        DiffPair_float_0 _S61;
        (&_S61)->primal_0 = _S53;
        (&_S61)->differential_0 = 0.0f;
        DiffPair_float_0 _S62;
        (&_S62)->primal_0 = 0.4166666567325592f;
        (&_S62)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S61, &_S62, _S60);
        _S59 = _S61.differential_0;
    }
    float _S63;
    if(_S52 < 0.00313080009073019f)
    {
        _S63 = 12.92000007629394531f * _s_dOut_1.x;
    }
    else
    {
        float _S64 = 1.0549999475479126f * _s_dOut_1.x;
        DiffPair_float_0 _S65;
        (&_S65)->primal_0 = _S52;
        (&_S65)->differential_0 = 0.0f;
        DiffPair_float_0 _S66;
        (&_S66)->primal_0 = 0.4166666567325592f;
        (&_S66)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S65, &_S66, _S64);
        _S63 = _S65.differential_0;
    }
    float3  _S67 = make_float3 (_S63, _S59, _S55);
    Matrix<float, 3, 3>  _S68 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S69;
    (&_S69)->primal_0 = color_matrix_1;
    (&_S69)->differential_0 = _S68;
    float3  _S70 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S71;
    (&_S71)->primal_0 = _S50.primal_0;
    (&_S71)->differential_0 = _S70;
    s_bwd_prop_mul_0(&_S69, &_S71, _S67);
    dprgb_0->primal_0 = (*dprgb_0).primal_0;
    dprgb_0->differential_0 = _S71.differential_0;
    return;
}

inline __device__ void s_bwd_linear_rgb_to_srgb_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S72, Matrix<float, 3, 3>  _S73, float3  _S74)
{
    s_bwd_prop_linear_rgb_to_srgb_0(_S72, _S73, _S74);
    return;
}

inline __device__ float3  linear_rgb_to_srgb_bwd(float3  rgb_3, Matrix<float, 3, 3>  color_matrix_2, float3  v_out_rgb_1)
{
    float3  _S75 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 p_rgb_0;
    (&p_rgb_0)->primal_0 = rgb_3;
    (&p_rgb_0)->differential_0 = _S75;
    s_bwd_linear_rgb_to_srgb_0(&p_rgb_0, color_matrix_2, v_out_rgb_1);
    return p_rgb_0.differential_0;
}

inline __device__ float3  rgb_to_srgb(float3  rgb_4, Matrix<float, 3, 3>  color_matrix_3)
{
    float _S76 = rgb_4.x;
    float _S77;
    if(_S76 < 0.04044999927282333f)
    {
        _S77 = _S76 * 0.07739938050508499f;
    }
    else
    {
        _S77 = (F32_pow((0.94786733388900757f * (_S76 + 0.05499999970197678f)), (2.40000009536743164f)));
    }
    float _S78 = rgb_4.y;
    float _S79;
    if(_S78 < 0.04044999927282333f)
    {
        _S79 = _S78 * 0.07739938050508499f;
    }
    else
    {
        _S79 = (F32_pow((0.94786733388900757f * (_S78 + 0.05499999970197678f)), (2.40000009536743164f)));
    }
    float _S80 = rgb_4.z;
    float _S81;
    if(_S80 < 0.04044999927282333f)
    {
        _S81 = _S80 * 0.07739938050508499f;
    }
    else
    {
        _S81 = (F32_pow((0.94786733388900757f * (_S80 + 0.05499999970197678f)), (2.40000009536743164f)));
    }
    float3  _S82 = mul_0(color_matrix_3, make_float3 (_S77, _S79, _S81));
    float _S83 = _S82.x;
    if(_S83 < 0.00313080009073019f)
    {
        _S77 = _S83 * 12.92000007629394531f;
    }
    else
    {
        _S77 = 1.0549999475479126f * (F32_pow((_S83), (0.4166666567325592f))) - 0.05499999970197678f;
    }
    float _S84 = _S82.y;
    if(_S84 < 0.00313080009073019f)
    {
        _S79 = _S84 * 12.92000007629394531f;
    }
    else
    {
        _S79 = 1.0549999475479126f * (F32_pow((_S84), (0.4166666567325592f))) - 0.05499999970197678f;
    }
    float _S85 = _S82.z;
    if(_S85 < 0.00313080009073019f)
    {
        _S81 = _S85 * 12.92000007629394531f;
    }
    else
    {
        _S81 = 1.0549999475479126f * (F32_pow((_S85), (0.4166666567325592f))) - 0.05499999970197678f;
    }
    return make_float3 (_S77, _S79, _S81);
}

inline __device__ float s_primal_ctx_pow_0(float _S86, float _S87)
{
    return (F32_pow((_S86), (_S87)));
}

inline __device__ void s_bwd_prop_rgb_to_srgb_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dprgb_1, Matrix<float, 3, 3>  color_matrix_4, float3  _s_dOut_2)
{
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S88 = *dprgb_1;
    float _S89 = (*dprgb_1).primal_0.x;
    bool _S90 = _S89 < 0.04044999927282333f;
    float _S91;
    if(_S90)
    {
        _S91 = _S89 * 0.07739938050508499f;
    }
    else
    {
        _S91 = s_primal_ctx_pow_0(0.94786733388900757f * (_S89 + 0.05499999970197678f), 2.40000009536743164f);
    }
    float _S92 = _S88.primal_0.y;
    bool _S93 = _S92 < 0.04044999927282333f;
    float _S94;
    if(_S93)
    {
        _S94 = _S92 * 0.07739938050508499f;
    }
    else
    {
        _S94 = s_primal_ctx_pow_0(0.94786733388900757f * (_S92 + 0.05499999970197678f), 2.40000009536743164f);
    }
    float _S95 = _S88.primal_0.z;
    bool _S96 = _S95 < 0.04044999927282333f;
    float _S97;
    if(_S96)
    {
        _S97 = _S95 * 0.07739938050508499f;
    }
    else
    {
        _S97 = s_primal_ctx_pow_0(0.94786733388900757f * (_S95 + 0.05499999970197678f), 2.40000009536743164f);
    }
    float3  _S98 = make_float3 (_S91, _S94, _S97);
    float3  _S99 = s_primal_ctx_mul_0(color_matrix_4, _S98);
    float _S100 = _S99.x;
    float _S101 = _S99.y;
    float _S102 = _S99.z;
    if(_S102 < 0.00313080009073019f)
    {
        _S91 = 12.92000007629394531f * _s_dOut_2.z;
    }
    else
    {
        float _S103 = 1.0549999475479126f * _s_dOut_2.z;
        DiffPair_float_0 _S104;
        (&_S104)->primal_0 = _S102;
        (&_S104)->differential_0 = 0.0f;
        DiffPair_float_0 _S105;
        (&_S105)->primal_0 = 0.4166666567325592f;
        (&_S105)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S104, &_S105, _S103);
        _S91 = _S104.differential_0;
    }
    if(_S101 < 0.00313080009073019f)
    {
        _S94 = 12.92000007629394531f * _s_dOut_2.y;
    }
    else
    {
        float _S106 = 1.0549999475479126f * _s_dOut_2.y;
        DiffPair_float_0 _S107;
        (&_S107)->primal_0 = _S101;
        (&_S107)->differential_0 = 0.0f;
        DiffPair_float_0 _S108;
        (&_S108)->primal_0 = 0.4166666567325592f;
        (&_S108)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S107, &_S108, _S106);
        _S94 = _S107.differential_0;
    }
    if(_S100 < 0.00313080009073019f)
    {
        _S97 = 12.92000007629394531f * _s_dOut_2.x;
    }
    else
    {
        float _S109 = 1.0549999475479126f * _s_dOut_2.x;
        DiffPair_float_0 _S110;
        (&_S110)->primal_0 = _S100;
        (&_S110)->differential_0 = 0.0f;
        DiffPair_float_0 _S111;
        (&_S111)->primal_0 = 0.4166666567325592f;
        (&_S111)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S110, &_S111, _S109);
        _S97 = _S110.differential_0;
    }
    float3  _S112 = make_float3 (_S97, _S94, _S91);
    Matrix<float, 3, 3>  _S113 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S114;
    (&_S114)->primal_0 = color_matrix_4;
    (&_S114)->differential_0 = _S113;
    float3  _S115 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S116;
    (&_S116)->primal_0 = _S98;
    (&_S116)->differential_0 = _S115;
    s_bwd_prop_mul_0(&_S114, &_S116, _S112);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S117 = _S116;
    if(_S96)
    {
        _S91 = 0.0f;
    }
    else
    {
        _S91 = 0.94786733388900757f * (_S95 + 0.05499999970197678f);
    }
    if(_S96)
    {
        _S91 = 0.07739938050508499f * _S117.differential_0.z;
    }
    else
    {
        DiffPair_float_0 _S118;
        (&_S118)->primal_0 = _S91;
        (&_S118)->differential_0 = 0.0f;
        DiffPair_float_0 _S119;
        (&_S119)->primal_0 = 2.40000009536743164f;
        (&_S119)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S118, &_S119, _S117.differential_0.z);
        _S91 = 0.94786733388900757f * _S118.differential_0;
    }
    if(_S93)
    {
        _S94 = 0.0f;
    }
    else
    {
        _S94 = 0.94786733388900757f * (_S92 + 0.05499999970197678f);
    }
    if(_S93)
    {
        _S94 = 0.07739938050508499f * _S117.differential_0.y;
    }
    else
    {
        DiffPair_float_0 _S120;
        (&_S120)->primal_0 = _S94;
        (&_S120)->differential_0 = 0.0f;
        DiffPair_float_0 _S121;
        (&_S121)->primal_0 = 2.40000009536743164f;
        (&_S121)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S120, &_S121, _S117.differential_0.y);
        _S94 = 0.94786733388900757f * _S120.differential_0;
    }
    if(_S90)
    {
        _S97 = 0.0f;
    }
    else
    {
        _S97 = 0.94786733388900757f * (_S89 + 0.05499999970197678f);
    }
    if(_S90)
    {
        _S97 = 0.07739938050508499f * _S117.differential_0.x;
    }
    else
    {
        DiffPair_float_0 _S122;
        (&_S122)->primal_0 = _S97;
        (&_S122)->differential_0 = 0.0f;
        DiffPair_float_0 _S123;
        (&_S123)->primal_0 = 2.40000009536743164f;
        (&_S123)->differential_0 = 0.0f;
        s_bwd_prop_pow_0(&_S122, &_S123, _S117.differential_0.x);
        _S97 = 0.94786733388900757f * _S122.differential_0;
    }
    float3  _S124 = make_float3 (_S97, _S94, _S91);
    dprgb_1->primal_0 = (*dprgb_1).primal_0;
    dprgb_1->differential_0 = _S124;
    return;
}

inline __device__ void s_bwd_rgb_to_srgb_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S125, Matrix<float, 3, 3>  _S126, float3  _S127)
{
    s_bwd_prop_rgb_to_srgb_0(_S125, _S126, _S127);
    return;
}

inline __device__ float3  rgb_to_srgb_bwd(float3  rgb_5, Matrix<float, 3, 3>  color_matrix_5, float3  v_out_rgb_2)
{
    float3  _S128 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 p_rgb_1;
    (&p_rgb_1)->primal_0 = rgb_5;
    (&p_rgb_1)->differential_0 = _S128;
    s_bwd_rgb_to_srgb_0(&p_rgb_1, color_matrix_5, v_out_rgb_2);
    return p_rgb_1.differential_0;
}

inline __device__ void _d_cross_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * a_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * b_0, float3  dOut_5)
{
    float _S129 = dOut_5.y;
    float _S130 = dOut_5.z;
    float _S131 = dOut_5.x;
    float _S132 = (*a_0).primal_0.z * _S129 + - (*a_0).primal_0.y * _S130;
    float _S133 = - (*a_0).primal_0.z * _S131 + (*a_0).primal_0.x * _S130;
    float _S134 = (*a_0).primal_0.y * _S131 + - (*a_0).primal_0.x * _S129;
    float3  _S135 = make_float3 (- (*b_0).primal_0.z * _S129 + (*b_0).primal_0.y * _S130, (*b_0).primal_0.z * _S131 + - (*b_0).primal_0.x * _S130, - (*b_0).primal_0.y * _S131 + (*b_0).primal_0.x * _S129);
    a_0->primal_0 = (*a_0).primal_0;
    a_0->differential_0 = _S135;
    float3  _S136 = make_float3 (_S132, _S133, _S134);
    b_0->primal_0 = (*b_0).primal_0;
    b_0->differential_0 = _S136;
    return;
}

inline __device__ float3  cross_0(float3  left_2, float3  right_2)
{
    float _S137 = left_2.y;
    float _S138 = right_2.z;
    float _S139 = left_2.z;
    float _S140 = right_2.y;
    float _S141 = right_2.x;
    float _S142 = left_2.x;
    return make_float3 (_S137 * _S138 - _S139 * _S140, _S139 * _S141 - _S142 * _S138, _S142 * _S140 - _S137 * _S141);
}

inline __device__ float length_0(float3  x_10)
{
    return (F32_sqrt((dot_0(x_10, x_10))));
}

inline __device__ float length_1(float2  x_11)
{
    return (F32_sqrt((dot_1(x_11, x_11))));
}

inline __device__ float3  points_to_normal(FixedArray<float3 , 4>  points_0)
{
    float3  _S143 = points_0[int(0)];
    bool _S144;
    if((dot_0(_S143, _S143)) == 0.0f)
    {
        _S144 = true;
    }
    else
    {
        float3  _S145 = points_0[int(1)];
        _S144 = (dot_0(_S145, _S145)) == 0.0f;
    }
    if(_S144)
    {
        _S144 = true;
    }
    else
    {
        float3  _S146 = points_0[int(2)];
        _S144 = (dot_0(_S146, _S146)) == 0.0f;
    }
    if(_S144)
    {
        _S144 = true;
    }
    else
    {
        float3  _S147 = points_0[int(3)];
        _S144 = (dot_0(_S147, _S147)) == 0.0f;
    }
    if(_S144)
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

inline __device__ float s_primal_ctx_dot_0(float3  _S148, float3  _S149)
{
    return dot_0(_S148, _S149);
}

inline __device__ float3  s_primal_ctx_cross_0(float3  _S150, float3  _S151)
{
    return cross_0(_S150, _S151);
}

inline __device__ void s_bwd_prop_sqrt_0(DiffPair_float_0 * _S152, float _S153)
{
    _d_sqrt_0(_S152, _S153);
    return;
}

inline __device__ void s_bwd_prop_length_impl_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_5, float _s_dOut_3)
{
    float _S154 = (*dpx_5).primal_0.x;
    float _S155 = (*dpx_5).primal_0.y;
    float _S156 = (*dpx_5).primal_0.z;
    DiffPair_float_0 _S157;
    (&_S157)->primal_0 = _S154 * _S154 + _S155 * _S155 + _S156 * _S156;
    (&_S157)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S157, _s_dOut_3);
    float _S158 = (*dpx_5).primal_0.z * _S157.differential_0;
    float _S159 = _S158 + _S158;
    float _S160 = (*dpx_5).primal_0.y * _S157.differential_0;
    float _S161 = _S160 + _S160;
    float _S162 = (*dpx_5).primal_0.x * _S157.differential_0;
    float _S163 = _S162 + _S162;
    float3  _S164 = make_float3 (0.0f);
    *&((&_S164)->z) = _S159;
    *&((&_S164)->y) = _S161;
    *&((&_S164)->x) = _S163;
    dpx_5->primal_0 = (*dpx_5).primal_0;
    dpx_5->differential_0 = _S164;
    return;
}

inline __device__ void s_bwd_length_impl_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S165, float _S166)
{
    s_bwd_prop_length_impl_0(_S165, _S166);
    return;
}

inline __device__ void s_bwd_prop_dot_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S167, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S168, float _S169)
{
    _d_dot_0(_S167, _S168, _S169);
    return;
}

inline __device__ void s_bwd_prop_cross_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S170, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S171, float3  _S172)
{
    _d_cross_0(_S170, _S171, _S172);
    return;
}

inline __device__ void s_bwd_prop_points_to_normal_0(DiffPair_arrayx3Cvectorx3Cfloatx2C3x3Ex2C4x3E_0 * dppoints_0, float3  _s_dOut_4)
{
    FixedArray<float3 , 4>  _S173 = dppoints_0->primal_0;
    float3  _S174 = make_float3 (0.0f);
    float3  _S175 = dppoints_0->primal_0[int(0)];
    bool _S176 = (s_primal_ctx_dot_0(_S175, _S175)) == 0.0f;
    bool _S177;
    float3  _S178;
    if(_S176)
    {
        _S177 = true;
        _S178 = _S174;
    }
    else
    {
        float3  _S179 = _S173[int(1)];
        _S177 = (s_primal_ctx_dot_0(_S179, _S179)) == 0.0f;
        _S178 = _S173[int(1)];
    }
    bool _S180;
    float3  _S181;
    if(_S177)
    {
        _S180 = true;
        _S181 = _S174;
    }
    else
    {
        float3  _S182 = _S173[int(2)];
        _S180 = (s_primal_ctx_dot_0(_S182, _S182)) == 0.0f;
        _S181 = _S173[int(2)];
    }
    bool _S183;
    float3  _S184;
    if(_S180)
    {
        _S183 = true;
        _S184 = _S174;
    }
    else
    {
        float3  _S185 = _S173[int(3)];
        _S183 = (s_primal_ctx_dot_0(_S185, _S185)) == 0.0f;
        _S184 = _S173[int(3)];
    }
    bool _S186 = !_S183;
    float3  _S187;
    float3  _S188;
    float3  _S189;
    float3  _S190;
    float3  _S191;
    if(_S186)
    {
        float3  dx_0 = _S173[int(1)] - _S173[int(0)];
        float3  _S192 = - (_S173[int(3)] - _S173[int(2)]);
        float3  _S193 = s_primal_ctx_cross_0(dx_0, _S192);
        bool _S194 = (s_primal_ctx_dot_0(_S193, _S193)) != 0.0f;
        if(_S194)
        {
            float _S195 = length_0(_S193);
            float3  _S196 = make_float3 (_S195);
            _S187 = make_float3 (_S195 * _S195);
            _S188 = _S196;
        }
        else
        {
            _S187 = _S174;
            _S188 = _S174;
        }
        float3  _S197 = _S188;
        _S183 = _S194;
        _S188 = _S193;
        _S189 = _S197;
        _S190 = dx_0;
        _S191 = _S192;
    }
    else
    {
        _S183 = false;
        _S187 = _S174;
        _S188 = _S174;
        _S189 = _S174;
        _S190 = _S174;
        _S191 = _S174;
    }
    FixedArray<float3 , 4>  _S198;
    if(_S186)
    {
        if(_S183)
        {
            float3  _S199 = _s_dOut_4 / _S187;
            float3  _S200 = _S188 * - _S199;
            float3  _S201 = _S189 * _S199;
            float _S202 = _S200.x + _S200.y + _S200.z;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S203;
            (&_S203)->primal_0 = _S188;
            (&_S203)->differential_0 = _S174;
            s_bwd_length_impl_0(&_S203, _S202);
            _S187 = _S201 + _S203.differential_0;
        }
        else
        {
            _S187 = _s_dOut_4;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S204;
        (&_S204)->primal_0 = _S188;
        (&_S204)->differential_0 = _S174;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S205;
        (&_S205)->primal_0 = _S188;
        (&_S205)->differential_0 = _S174;
        s_bwd_prop_dot_0(&_S204, &_S205, 0.0f);
        float3  _S206 = _S205.differential_0 + _S204.differential_0 + _S187;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S207;
        (&_S207)->primal_0 = _S190;
        (&_S207)->differential_0 = _S174;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S208;
        (&_S208)->primal_0 = _S191;
        (&_S208)->differential_0 = _S174;
        s_bwd_prop_cross_0(&_S207, &_S208, _S206);
        float3  s_diff_dy_T_0 = - _S208.differential_0;
        float3  _S209 = - s_diff_dy_T_0;
        float3  _S210 = - _S207.differential_0;
        FixedArray<float3 , 4>  _S211;
        _S211[int(0)] = _S174;
        _S211[int(1)] = _S174;
        _S211[int(2)] = _S174;
        _S211[int(3)] = _S174;
        _S211[int(2)] = _S209;
        _S211[int(3)] = s_diff_dy_T_0;
        _S211[int(1)] = _S207.differential_0;
        _S198[int(0)] = _S211[int(0)];
        _S198[int(1)] = _S211[int(1)];
        _S198[int(2)] = _S211[int(2)];
        _S198[int(3)] = _S211[int(3)];
        _S187 = _S210;
    }
    else
    {
        _S198[int(0)] = _S174;
        _S198[int(1)] = _S174;
        _S198[int(2)] = _S174;
        _S198[int(3)] = _S174;
        _S187 = _S174;
    }
    if(_S180)
    {
    }
    else
    {
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S212;
        (&_S212)->primal_0 = _S184;
        (&_S212)->differential_0 = _S174;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S213;
        (&_S213)->primal_0 = _S184;
        (&_S213)->differential_0 = _S174;
        s_bwd_prop_dot_0(&_S212, &_S213, 0.0f);
        float3  _S214 = _S213.differential_0 + _S212.differential_0;
        FixedArray<float3 , 4>  _S215;
        _S215[int(0)] = _S174;
        _S215[int(1)] = _S174;
        _S215[int(2)] = _S174;
        _S215[int(3)] = _S174;
        _S215[int(3)] = _S214;
        float3  _S216 = _S198[int(1)] + _S215[int(1)];
        float3  _S217 = _S198[int(2)] + _S215[int(2)];
        float3  _S218 = _S198[int(3)] + _S215[int(3)];
        _S198[int(0)] = _S198[int(0)] + _S215[int(0)];
        _S198[int(1)] = _S216;
        _S198[int(2)] = _S217;
        _S198[int(3)] = _S218;
    }
    if(_S177)
    {
    }
    else
    {
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S219;
        (&_S219)->primal_0 = _S181;
        (&_S219)->differential_0 = _S174;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S220;
        (&_S220)->primal_0 = _S181;
        (&_S220)->differential_0 = _S174;
        s_bwd_prop_dot_0(&_S219, &_S220, 0.0f);
        float3  _S221 = _S220.differential_0 + _S219.differential_0;
        FixedArray<float3 , 4>  _S222;
        _S222[int(0)] = _S174;
        _S222[int(1)] = _S174;
        _S222[int(2)] = _S174;
        _S222[int(3)] = _S174;
        _S222[int(2)] = _S221;
        float3  _S223 = _S198[int(1)] + _S222[int(1)];
        float3  _S224 = _S198[int(2)] + _S222[int(2)];
        float3  _S225 = _S198[int(3)] + _S222[int(3)];
        _S198[int(0)] = _S198[int(0)] + _S222[int(0)];
        _S198[int(1)] = _S223;
        _S198[int(2)] = _S224;
        _S198[int(3)] = _S225;
    }
    if(_S176)
    {
    }
    else
    {
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S226;
        (&_S226)->primal_0 = _S178;
        (&_S226)->differential_0 = _S174;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S227;
        (&_S227)->primal_0 = _S178;
        (&_S227)->differential_0 = _S174;
        s_bwd_prop_dot_0(&_S226, &_S227, 0.0f);
        float3  _S228 = _S227.differential_0 + _S226.differential_0;
        FixedArray<float3 , 4>  _S229;
        _S229[int(0)] = _S174;
        _S229[int(1)] = _S174;
        _S229[int(2)] = _S174;
        _S229[int(3)] = _S174;
        _S229[int(1)] = _S228;
        float3  _S230 = _S198[int(1)] + _S229[int(1)];
        float3  _S231 = _S198[int(2)] + _S229[int(2)];
        float3  _S232 = _S198[int(3)] + _S229[int(3)];
        _S198[int(0)] = _S198[int(0)] + _S229[int(0)];
        _S198[int(1)] = _S230;
        _S198[int(2)] = _S231;
        _S198[int(3)] = _S232;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S233;
    (&_S233)->primal_0 = _S173[int(0)];
    (&_S233)->differential_0 = _S174;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S234;
    (&_S234)->primal_0 = _S173[int(0)];
    (&_S234)->differential_0 = _S174;
    s_bwd_prop_dot_0(&_S233, &_S234, 0.0f);
    float3  _S235 = _S234.differential_0 + _S233.differential_0 + _S187;
    FixedArray<float3 , 4>  _S236;
    _S236[int(0)] = _S174;
    _S236[int(1)] = _S174;
    _S236[int(2)] = _S174;
    _S236[int(3)] = _S174;
    _S236[int(0)] = _S235;
    FixedArray<float3 , 4>  _S237 = {
        _S198[int(0)] + _S236[int(0)], _S198[int(1)] + _S236[int(1)], _S198[int(2)] + _S236[int(2)], _S198[int(3)] + _S236[int(3)]
    };
    dppoints_0->primal_0 = dppoints_0->primal_0;
    dppoints_0->differential_0 = _S237;
    return;
}

inline __device__ void s_bwd_points_to_normal_0(DiffPair_arrayx3Cvectorx3Cfloatx2C3x3Ex2C4x3E_0 * _S238, float3  _S239)
{
    s_bwd_prop_points_to_normal_0(_S238, _S239);
    return;
}

inline __device__ void points_to_normal_vjp(FixedArray<float3 , 4>  points_1, float3  v_normal_0, FixedArray<float3 , 4>  * v_points_0)
{
    FixedArray<float3 , 4>  _S240 = { make_float3 (0.0f), make_float3 (0.0f), make_float3 (0.0f), make_float3 (0.0f) };
    DiffPair_arrayx3Cvectorx3Cfloatx2C3x3Ex2C4x3E_0 dp_points_0;
    (&dp_points_0)->primal_0 = points_1;
    (&dp_points_0)->differential_0 = _S240;
    s_bwd_points_to_normal_0(&dp_points_0, v_normal_0);
    *v_points_0 = (&dp_points_0)->differential_0;
    return;
}

inline __device__ Matrix<float, 2, 2>  transpose_0(Matrix<float, 2, 2>  x_12)
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
        int c_0 = int(0);
        for(;;)
        {
            if(c_0 < int(2))
            {
            }
            else
            {
                break;
            }
            *_slang_vector_get_element_ptr(((&result_7)->rows + (r_0)), c_0) = _slang_vector_get_element(x_12.rows[c_0], r_0);
            c_0 = c_0 + int(1);
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
    float _S241 = s_diff_u_0 * u_3;
    float _S242 = s_diff_v_0 * v_1;
    float r2_1 = u_3 * u_3 + v_1 * v_1;
    float s_diff_r2_0 = _S241 + _S241 + (_S242 + _S242);
    float _S243 = (*coeffs_1)[int(0)] + r2_1 * (*coeffs_1)[int(1)];
    float radial_0 = 1.0f + r2_1 * _S243;
    float _S244 = 2.0f * (*coeffs_1)[int(2)];
    float _S245 = _S244 * u_3;
    float _S246 = 2.0f * u_3;
    float _S247 = 2.0f * (*coeffs_1)[int(3)];
    float _S248 = _S247 * u_3;
    float _S249 = 2.0f * v_1;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S250 = { dpuv_0->primal_0 * make_float2 (radial_0) + make_float2 (_S245 * v_1 + (*coeffs_1)[int(3)] * (r2_1 + _S246 * u_3), _S248 * v_1 + (*coeffs_1)[int(2)] * (r2_1 + _S249 * v_1)), dpuv_0->differential_0 * make_float2 (radial_0) + make_float2 (s_diff_r2_0 * _S243 + s_diff_r2_0 * (*coeffs_1)[int(1)] * r2_1) * dpuv_0->primal_0 + make_float2 (s_diff_u_0 * _S244 * v_1 + s_diff_v_0 * _S245 + (s_diff_r2_0 + (s_diff_u_0 * 2.0f * u_3 + s_diff_u_0 * _S246)) * (*coeffs_1)[int(3)], s_diff_u_0 * _S247 * v_1 + s_diff_v_0 * _S248 + (s_diff_r2_0 + (s_diff_v_0 * 2.0f * v_1 + s_diff_v_0 * _S249)) * (*coeffs_1)[int(2)]) };
    return _S250;
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
        float2  _S251 = DistOpenCV_distort_0(q_0, dist_coeffs_1);
        float2  r_1 = _S251 - uv_2;
        float2  _S252 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S253;
        (&_S253)->primal_0 = q_0;
        (&_S253)->differential_0 = _S252;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S254 = s_fwd_DistOpenCV_distort_0(&_S253, dist_coeffs_1);
        float2  _S255 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S256;
        (&_S256)->primal_0 = q_0;
        (&_S256)->differential_0 = _S255;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S257 = s_fwd_DistOpenCV_distort_0(&_S256, dist_coeffs_1);
        Matrix<float, 2, 2>  _S258 = transpose_0(makeMatrix<float, 2, 2> (_S254.differential_0, _S257.differential_0));
        float inv_det_0 = 1.0f / (_S258.rows[int(0)].x * _S258.rows[int(1)].y - _S258.rows[int(0)].y * _S258.rows[int(1)].x);
        float _S259 = r_1.x;
        float _S260 = r_1.y;
        float2  q_1 = q_0 - make_float2 ((_S259 * _S258.rows[int(1)].y - _S260 * _S258.rows[int(0)].y) * inv_det_0, (- _S259 * _S258.rows[int(1)].x + _S260 * _S258.rows[int(0)].x) * inv_det_0);
        i_5 = i_5 + int(1);
        q_0 = q_1;
    }
    *uv_undist_1 = q_0;
    float2  _S261 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S262;
    (&_S262)->primal_0 = q_0;
    (&_S262)->differential_0 = _S261;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S263 = s_fwd_DistOpenCV_distort_0(&_S262, dist_coeffs_1);
    float2  _S264 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S265;
    (&_S265)->primal_0 = q_0;
    (&_S265)->differential_0 = _S264;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S266 = s_fwd_DistOpenCV_distort_0(&_S265, dist_coeffs_1);
    Matrix<float, 2, 2>  _S267 = transpose_0(makeMatrix<float, 2, 2> (_S263.differential_0, _S266.differential_0));
    float _S268 = (F32_min((determinant_0(_S267)), ((F32_min((_S267.rows[int(0)].x), (_S267.rows[int(1)].y))))));
    bool _S269;
    if(_S268 > 0.25f)
    {
        _S269 = _S268 < 4.0f;
    }
    else
    {
        _S269 = false;
    }
    if(_S269)
    {
        float2  _S270 = DistOpenCV_distort_0(q_0, dist_coeffs_1);
        _S269 = (dot_1(q_0, _S270)) >= 0.0f;
    }
    else
    {
        _S269 = false;
    }
    if(_S269)
    {
        float2  _S271 = DistOpenCV_distort_0(*uv_undist_1, dist_coeffs_1);
        _S269 = (length_1(_S271 - uv_2)) < 0.00999999977648258f;
    }
    else
    {
        _S269 = false;
    }
    return _S269;
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
    float _S272 = s_diff_u_1 * u_5;
    float _S273 = s_diff_v_1 * v_3;
    float r2_3 = u_5 * u_5 + v_3 * v_3;
    float s_diff_r2_1 = _S272 + _S272 + (_S273 + _S273);
    float _S274 = (*coeffs_3)[int(2)] + r2_3 * (*coeffs_3)[int(3)];
    float _S275 = (*coeffs_3)[int(1)] + r2_3 * _S274;
    float _S276 = (*coeffs_3)[int(0)] + r2_3 * _S275;
    float radial_1 = 1.0f + r2_3 * _S276;
    float _S277 = 2.0f * (*coeffs_3)[int(4)];
    float _S278 = _S277 * u_5;
    float _S279 = 2.0f * u_5;
    float _S280 = 2.0f * (*coeffs_3)[int(5)];
    float _S281 = _S280 * u_5;
    float _S282 = 2.0f * v_3;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S283 = { dpuv_1->primal_0 * make_float2 (radial_1) + make_float2 (_S278 * v_3 + (*coeffs_3)[int(5)] * (r2_3 + _S279 * u_5) + (*coeffs_3)[int(6)] * r2_3, _S281 * v_3 + (*coeffs_3)[int(4)] * (r2_3 + _S282 * v_3) + (*coeffs_3)[int(7)] * r2_3), dpuv_1->differential_0 * make_float2 (radial_1) + make_float2 (s_diff_r2_1 * _S276 + (s_diff_r2_1 * _S275 + (s_diff_r2_1 * _S274 + s_diff_r2_1 * (*coeffs_3)[int(3)] * r2_3) * r2_3) * r2_3) * dpuv_1->primal_0 + make_float2 (s_diff_u_1 * _S277 * v_3 + s_diff_v_1 * _S278 + (s_diff_r2_1 + (s_diff_u_1 * 2.0f * u_5 + s_diff_u_1 * _S279)) * (*coeffs_3)[int(5)] + s_diff_r2_1 * (*coeffs_3)[int(6)], s_diff_u_1 * _S280 * v_3 + s_diff_v_1 * _S281 + (s_diff_r2_1 + (s_diff_v_1 * 2.0f * v_3 + s_diff_v_1 * _S282)) * (*coeffs_3)[int(4)] + s_diff_r2_1 * (*coeffs_3)[int(7)]) };
    return _S283;
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
        float2  _S284 = DistThinPrism_distort_0(q_2, dist_coeffs_2);
        float2  r_2 = _S284 - uv_4;
        float2  _S285 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S286;
        (&_S286)->primal_0 = q_2;
        (&_S286)->differential_0 = _S285;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S287 = s_fwd_DistThinPrism_distort_0(&_S286, dist_coeffs_2);
        float2  _S288 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S289;
        (&_S289)->primal_0 = q_2;
        (&_S289)->differential_0 = _S288;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S290 = s_fwd_DistThinPrism_distort_0(&_S289, dist_coeffs_2);
        Matrix<float, 2, 2>  _S291 = transpose_0(makeMatrix<float, 2, 2> (_S287.differential_0, _S290.differential_0));
        float inv_det_1 = 1.0f / (_S291.rows[int(0)].x * _S291.rows[int(1)].y - _S291.rows[int(0)].y * _S291.rows[int(1)].x);
        float _S292 = r_2.x;
        float _S293 = r_2.y;
        float2  q_3 = q_2 - make_float2 ((_S292 * _S291.rows[int(1)].y - _S293 * _S291.rows[int(0)].y) * inv_det_1, (- _S292 * _S291.rows[int(1)].x + _S293 * _S291.rows[int(0)].x) * inv_det_1);
        i_6 = i_6 + int(1);
        q_2 = q_3;
    }
    *uv_undist_2 = q_2;
    float2  _S294 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S295;
    (&_S295)->primal_0 = q_2;
    (&_S295)->differential_0 = _S294;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S296 = s_fwd_DistThinPrism_distort_0(&_S295, dist_coeffs_2);
    float2  _S297 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S298;
    (&_S298)->primal_0 = q_2;
    (&_S298)->differential_0 = _S297;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S299 = s_fwd_DistThinPrism_distort_0(&_S298, dist_coeffs_2);
    Matrix<float, 2, 2>  _S300 = transpose_0(makeMatrix<float, 2, 2> (_S296.differential_0, _S299.differential_0));
    float _S301 = (F32_min((determinant_0(_S300)), ((F32_min((_S300.rows[int(0)].x), (_S300.rows[int(1)].y))))));
    bool _S302;
    if(_S301 > 0.25f)
    {
        _S302 = _S301 < 4.0f;
    }
    else
    {
        _S302 = false;
    }
    if(_S302)
    {
        float2  _S303 = DistThinPrism_distort_0(q_2, dist_coeffs_2);
        _S302 = (dot_1(q_2, _S303)) >= 0.0f;
    }
    else
    {
        _S302 = false;
    }
    if(_S302)
    {
        float2  _S304 = DistThinPrism_distort_0(*uv_undist_2, dist_coeffs_2);
        _S302 = (length_1(_S304 - uv_4)) < 0.00999999977648258f;
    }
    else
    {
        _S302 = false;
    }
    return _S302;
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
    float _S305 = s_diff_u_2 * u_7;
    float _S306 = s_diff_v_2 * v_5;
    float r2_5 = u_7 * u_7 + v_5 * v_5;
    float s_diff_r2_2 = _S305 + _S305 + (_S306 + _S306);
    float _S307 = (*coeffs_5)[int(1)] + r2_5 * (*coeffs_5)[int(2)];
    float _S308 = (*coeffs_5)[int(0)] + r2_5 * _S307;
    float _S309 = 1.0f + r2_5 * _S308;
    float _S310 = (*coeffs_5)[int(4)] + r2_5 * (*coeffs_5)[int(5)];
    float _S311 = (*coeffs_5)[int(3)] + r2_5 * _S310;
    float _S312 = 1.0f + r2_5 * _S311;
    float radial_2 = _S309 / _S312;
    float _S313 = 2.0f * (*coeffs_5)[int(6)];
    float _S314 = _S313 * u_7;
    float _S315 = 2.0f * u_7;
    float _S316 = 2.0f * (*coeffs_5)[int(7)];
    float _S317 = _S316 * u_7;
    float _S318 = 2.0f * v_5;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S319 = { dpuv_2->primal_0 * make_float2 (radial_2) + make_float2 (_S314 * v_5 + (*coeffs_5)[int(7)] * (r2_5 + _S315 * u_7), _S317 * v_5 + (*coeffs_5)[int(6)] * (r2_5 + _S318 * v_5)), dpuv_2->differential_0 * make_float2 (radial_2) + make_float2 (((s_diff_r2_2 * _S308 + (s_diff_r2_2 * _S307 + s_diff_r2_2 * (*coeffs_5)[int(2)] * r2_5) * r2_5) * _S312 - _S309 * (s_diff_r2_2 * _S311 + (s_diff_r2_2 * _S310 + s_diff_r2_2 * (*coeffs_5)[int(5)] * r2_5) * r2_5)) / (_S312 * _S312)) * dpuv_2->primal_0 + make_float2 (s_diff_u_2 * _S313 * v_5 + s_diff_v_2 * _S314 + (s_diff_r2_2 + (s_diff_u_2 * 2.0f * u_7 + s_diff_u_2 * _S315)) * (*coeffs_5)[int(7)], s_diff_u_2 * _S316 * v_5 + s_diff_v_2 * _S317 + (s_diff_r2_2 + (s_diff_v_2 * 2.0f * v_5 + s_diff_v_2 * _S318)) * (*coeffs_5)[int(6)]) };
    return _S319;
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
        float2  _S320 = DistRational_distort_0(q_4, dist_coeffs_3);
        float2  r_3 = _S320 - uv_6;
        float2  _S321 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S322;
        (&_S322)->primal_0 = q_4;
        (&_S322)->differential_0 = _S321;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S323 = s_fwd_DistRational_distort_0(&_S322, dist_coeffs_3);
        float2  _S324 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S325;
        (&_S325)->primal_0 = q_4;
        (&_S325)->differential_0 = _S324;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S326 = s_fwd_DistRational_distort_0(&_S325, dist_coeffs_3);
        Matrix<float, 2, 2>  _S327 = transpose_0(makeMatrix<float, 2, 2> (_S323.differential_0, _S326.differential_0));
        float inv_det_2 = 1.0f / (_S327.rows[int(0)].x * _S327.rows[int(1)].y - _S327.rows[int(0)].y * _S327.rows[int(1)].x);
        float _S328 = r_3.x;
        float _S329 = r_3.y;
        float2  q_5 = q_4 - make_float2 ((_S328 * _S327.rows[int(1)].y - _S329 * _S327.rows[int(0)].y) * inv_det_2, (- _S328 * _S327.rows[int(1)].x + _S329 * _S327.rows[int(0)].x) * inv_det_2);
        i_7 = i_7 + int(1);
        q_4 = q_5;
    }
    *uv_undist_3 = q_4;
    float2  _S330 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S331;
    (&_S331)->primal_0 = q_4;
    (&_S331)->differential_0 = _S330;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S332 = s_fwd_DistRational_distort_0(&_S331, dist_coeffs_3);
    float2  _S333 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S334;
    (&_S334)->primal_0 = q_4;
    (&_S334)->differential_0 = _S333;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S335 = s_fwd_DistRational_distort_0(&_S334, dist_coeffs_3);
    Matrix<float, 2, 2>  _S336 = transpose_0(makeMatrix<float, 2, 2> (_S332.differential_0, _S335.differential_0));
    float _S337 = (F32_min((determinant_0(_S336)), ((F32_min((_S336.rows[int(0)].x), (_S336.rows[int(1)].y))))));
    bool _S338;
    if(_S337 > 0.25f)
    {
        _S338 = _S337 < 4.0f;
    }
    else
    {
        _S338 = false;
    }
    if(_S338)
    {
        float2  _S339 = DistRational_distort_0(q_4, dist_coeffs_3);
        _S338 = (dot_1(q_4, _S339)) >= 0.0f;
    }
    else
    {
        _S338 = false;
    }
    if(_S338)
    {
        float2  _S340 = DistRational_distort_0(*uv_undist_3, dist_coeffs_3);
        _S338 = (length_1(_S340 - uv_6)) < 0.00999999977648258f;
    }
    else
    {
        _S338 = false;
    }
    return _S338;
}

inline __device__ float3  normalize_0(float3  x_13)
{
    return x_13 / make_float3 (length_0(x_13));
}

inline __device__ float3  unproject_raydir_0(float2  uv_7, int camera_model_0, bool is_ray_depth_0)
{
    float3  raydir_0;
    bool is_unit_0;
    if(camera_model_0 == int(1))
    {
        float theta_0 = length_1(uv_7);
        float3  _S341 = make_float3 ((uv_7 / make_float2 ((F32_max((theta_0), (1.00000001168609742e-07f)))) * make_float2 ((F32_sin((theta_0))))).x, (uv_7 / make_float2 ((F32_max((theta_0), (1.00000001168609742e-07f)))) * make_float2 ((F32_sin((theta_0))))).y, (F32_cos((theta_0))));
        is_unit_0 = true;
        raydir_0 = _S341;
    }
    else
    {
        bool _S342 = camera_model_0 == int(2);
        if(_S342)
        {
            float r_4 = length_1(uv_7);
            raydir_0 = make_float3 ((uv_7 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_4 * r_4)))))))).x, (uv_7 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_4 * r_4)))))))).y, 1.0f - 0.5f * r_4 * r_4);
        }
        else
        {
            raydir_0 = make_float3 (uv_7.x, uv_7.y, 1.0f);
        }
        is_unit_0 = _S342;
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
    float3  _S343;
    for(;;)
    {
        float2  uv_8 = (pix_pos_0 - float2 {intrins_0.z, intrins_0.w}) / float2 {intrins_0.x, intrins_0.y};
        FixedArray<float, 1>  _S344 = dist_coeffs_4;
        float2  uv_u_0;
        bool _S345 = undistort_point_0(uv_8, &_S344, int(12), &uv_u_0);
        if(!_S345)
        {
            int3  _S346 = make_int3 (int(0));
            float3  _S347 = make_float3 ((float)_S346.x, (float)_S346.y, (float)_S346.z);
            _S343 = _S347;
            break;
        }
        _S343 = unproject_raydir_0(uv_u_0, camera_model_1, is_ray_depth_1);
        break;
    }
    return _S343;
}

inline __device__ float3  depth_to_point_none(float2  pix_pos_1, float4  intrins_1, FixedArray<float, 1>  dist_coeffs_5, int camera_model_2, bool is_ray_depth_2, float depth_2)
{
    float3  _S348;
    for(;;)
    {
        float2  uv_9 = (pix_pos_1 - float2 {intrins_1.z, intrins_1.w}) / float2 {intrins_1.x, intrins_1.y};
        FixedArray<float, 1>  _S349 = dist_coeffs_5;
        float2  uv_u_1;
        bool _S350 = undistort_point_0(uv_9, &_S349, int(12), &uv_u_1);
        if(!_S350)
        {
            _S348 = make_float3 (0.0f);
            break;
        }
        _S348 = make_float3 (depth_2) * unproject_raydir_0(uv_u_1, camera_model_2, is_ray_depth_2);
        break;
    }
    return _S348;
}

struct s_bwd_prop_depth_to_point_Intermediates_0
{
    float2  _S351;
    bool _S352;
};

inline __device__ float s_primal_ctx_sin_0(float _S353)
{
    return (F32_sin((_S353)));
}

inline __device__ float s_primal_ctx_cos_0(float _S354)
{
    return (F32_cos((_S354)));
}

inline __device__ float s_primal_ctx_sqrt_0(float _S355)
{
    return (F32_sqrt((_S355)));
}

inline __device__ float3  s_primal_ctx_unproject_raydir_0(float2  dpuv_3, int camera_model_3, bool is_ray_depth_3)
{
    float3  raydir_1;
    bool is_unit_1;
    if(camera_model_3 == int(1))
    {
        float _S356 = length_1(dpuv_3);
        float3  _S357 = make_float3 ((dpuv_3 / make_float2 ((F32_max((_S356), (1.00000001168609742e-07f)))) * make_float2 (s_primal_ctx_sin_0(_S356))).x, (dpuv_3 / make_float2 ((F32_max((_S356), (1.00000001168609742e-07f)))) * make_float2 (s_primal_ctx_sin_0(_S356))).y, s_primal_ctx_cos_0(_S356));
        is_unit_1 = true;
        raydir_1 = _S357;
    }
    else
    {
        bool _S358 = camera_model_3 == int(2);
        if(_S358)
        {
            float _S359 = length_1(dpuv_3);
            raydir_1 = make_float3 ((dpuv_3 * make_float2 (s_primal_ctx_sqrt_0((F32_max((0.0f), (1.0f - 0.25f * _S359 * _S359)))))).x, (dpuv_3 * make_float2 (s_primal_ctx_sqrt_0((F32_max((0.0f), (1.0f - 0.25f * _S359 * _S359)))))).y, 1.0f - 0.5f * _S359 * _S359);
        }
        else
        {
            raydir_1 = make_float3 (dpuv_3.x, dpuv_3.y, 1.0f);
        }
        is_unit_1 = _S358;
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
    float2  _S360 = make_float2 (0.0f);
    s_bwd_prop_depth_to_point_Intermediates_0 _S361;
    (&_S361)->_S351 = _S360;
    (&_S361)->_S352 = false;
    float2  uv_10 = (pix_pos_2 - float2 {intrins_2.z, intrins_2.w}) / float2 {intrins_2.x, intrins_2.y};
    float2  _S362 = _S360;
    FixedArray<float, 1>  _S363 = dist_coeffs_6;
    bool _S364 = undistort_point_0(uv_10, &_S363, int(12), &_S362);
    (&_S361)->_S351 = _S362;
    (&_S361)->_S352 = _S364;
    s_bwd_prop_depth_to_point_Intermediates_0 _S365 = _S361;
    float3  _S366 = make_float3 (0.0f);
    bool _S367 = !!_S361._S352;
    float3  _S368;
    if(_S367)
    {
        _S368 = s_primal_ctx_unproject_raydir_0(_S365._S351, camera_model_4, is_ray_depth_4);
    }
    else
    {
        _S368 = _S366;
    }
    if(_S367)
    {
        _S368 = _S368 * v_point_0;
    }
    else
    {
        _S368 = _S366;
    }
    return _S368.x + _S368.y + _S368.z;
}

inline __device__ float3  depth_to_normal_none(float2  pix_center_0, float4  intrins_3, FixedArray<float, 1>  dist_coeffs_7, int camera_model_5, bool is_ray_depth_5, float4  depths_0)
{
    float3  normal_2;
    for(;;)
    {
        bool _S369;
        if((depths_0.x) == 0.0f)
        {
            _S369 = true;
        }
        else
        {
            _S369 = (depths_0.y) == 0.0f;
        }
        if(_S369)
        {
            _S369 = true;
        }
        else
        {
            _S369 = (depths_0.z) == 0.0f;
        }
        if(_S369)
        {
            _S369 = true;
        }
        else
        {
            _S369 = (depths_0.w) == 0.0f;
        }
        if(_S369)
        {
            normal_2 = make_float3 (0.0f);
            break;
        }
        float3  * _S370;
        float3  * _S371;
        float3  * _S372;
        float3  * _S373;
        int _S374;
        FixedArray<float3 , 4>  points_2;
        for(;;)
        {
            float2  _S375 = float2 {intrins_3.z, intrins_3.w};
            float2  _S376 = float2 {intrins_3.x, intrins_3.y};
            float2  uv_11 = (pix_center_0 + make_float2 (-1.0f, -0.0f) - _S375) / _S376;
            FixedArray<float, 1>  _S377 = dist_coeffs_7;
            float2  uv_u_2;
            bool _S378 = undistort_point_0(uv_11, &_S377, int(12), &uv_u_2);
            if(!_S378)
            {
                float3  _S379 = make_float3 (0.0f);
                _S374 = int(0);
                _S373 = nullptr;
                _S372 = nullptr;
                _S371 = nullptr;
                _S370 = nullptr;
                normal_2 = _S379;
                break;
            }
            points_2[int(0)] = make_float3 (depths_0.x) * unproject_raydir_0(uv_u_2, camera_model_5, is_ray_depth_5);
            for(;;)
            {
                float2  uv_12 = (pix_center_0 + make_float2 (1.0f, -0.0f) - _S375) / _S376;
                FixedArray<float, 1>  _S380 = dist_coeffs_7;
                float2  uv_u_3;
                bool _S381 = undistort_point_0(uv_12, &_S380, int(12), &uv_u_3);
                if(!_S381)
                {
                    float3  _S382 = make_float3 (0.0f);
                    _S374 = int(0);
                    _S373 = nullptr;
                    normal_2 = _S382;
                    break;
                }
                points_2[int(1)] = make_float3 (depths_0.y) * unproject_raydir_0(uv_u_3, camera_model_5, is_ray_depth_5);
                _S374 = int(2);
                _S373 = &points_2[int(1)];
                break;
            }
            if(_S374 != int(2))
            {
                _S372 = &points_2[int(0)];
                _S371 = nullptr;
                _S370 = nullptr;
                break;
            }
            float2  uv_13 = (pix_center_0 + make_float2 (0.0f, -1.0f) - _S375) / _S376;
            FixedArray<float, 1>  _S383 = dist_coeffs_7;
            float2  uv_u_4;
            bool _S384 = undistort_point_0(uv_13, &_S383, int(12), &uv_u_4);
            if(!_S384)
            {
                float3  _S385 = make_float3 (0.0f);
                _S374 = int(0);
                _S372 = &points_2[int(0)];
                _S371 = nullptr;
                _S370 = nullptr;
                normal_2 = _S385;
                break;
            }
            points_2[int(2)] = make_float3 (depths_0.z) * unproject_raydir_0(uv_u_4, camera_model_5, is_ray_depth_5);
            for(;;)
            {
                float2  uv_14 = (pix_center_0 + make_float2 (0.0f, 1.0f) - _S375) / _S376;
                FixedArray<float, 1>  _S386 = dist_coeffs_7;
                float2  uv_u_5;
                bool _S387 = undistort_point_0(uv_14, &_S386, int(12), &uv_u_5);
                if(!_S387)
                {
                    float3  _S388 = make_float3 (0.0f);
                    _S374 = int(0);
                    _S372 = nullptr;
                    normal_2 = _S388;
                    break;
                }
                points_2[int(3)] = make_float3 (depths_0.w) * unproject_raydir_0(uv_u_5, camera_model_5, is_ray_depth_5);
                _S374 = int(2);
                _S372 = &points_2[int(3)];
                break;
            }
            if(_S374 != int(2))
            {
                float3  * _S389 = _S372;
                _S372 = &points_2[int(0)];
                _S371 = _S389;
                _S370 = &points_2[int(2)];
                break;
            }
            float3  * _S390 = _S372;
            _S374 = int(1);
            _S372 = &points_2[int(0)];
            _S371 = _S390;
            _S370 = &points_2[int(2)];
            break;
        }
        if(_S374 != int(1))
        {
            break;
        }
        float3  normal_3 = cross_0(*_S373 - *_S372, - (*_S371 - *_S370));
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
    float2  _S391;
    bool _S392;
    float2  _S393;
    bool _S394;
    float2  _S395;
    bool _S396;
    float2  _S397;
    bool _S398;
};

inline __device__ void depth_to_normal_vjp_none(float2  pix_center_1, float4  intrins_4, FixedArray<float, 1>  dist_coeffs_8, int camera_model_6, bool is_ray_depth_6, float4  depths_1, float3  v_normal_1, float4  * v_depths_0)
{
    float2  _S399 = make_float2 (0.0f);
    s_bwd_prop_depth_to_normal_Intermediates_0 _S400;
    (&_S400)->_S391 = _S399;
    (&_S400)->_S392 = false;
    (&_S400)->_S393 = _S399;
    (&_S400)->_S394 = false;
    (&_S400)->_S395 = _S399;
    (&_S400)->_S396 = false;
    (&_S400)->_S397 = _S399;
    (&_S400)->_S398 = false;
    (&_S400)->_S391 = _S399;
    (&_S400)->_S392 = false;
    (&_S400)->_S393 = _S399;
    (&_S400)->_S394 = false;
    (&_S400)->_S395 = _S399;
    (&_S400)->_S396 = false;
    (&_S400)->_S397 = _S399;
    (&_S400)->_S398 = false;
    bool _S401 = (depths_1.x) == 0.0f;
    bool _runFlag_0;
    if(_S401)
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
    int _S402;
    if(!_runFlag_0)
    {
        float2  _S403 = float2 {intrins_4.z, intrins_4.w};
        float2  _S404 = float2 {intrins_4.x, intrins_4.y};
        float2  uv_15 = (pix_center_1 + make_float2 (-1.0f, -0.0f) - _S403) / _S404;
        float2  _S405 = _S399;
        FixedArray<float, 1>  _S406 = dist_coeffs_8;
        bool _S407 = undistort_point_0(uv_15, &_S406, int(12), &_S405);
        (&_S400)->_S391 = _S405;
        (&_S400)->_S392 = _S407;
        bool _S408 = !!_S407;
        if(_S408)
        {
            float2  uv_16 = (pix_center_1 + make_float2 (1.0f, -0.0f) - _S403) / _S404;
            float2  _S409 = _S399;
            FixedArray<float, 1>  _S410 = dist_coeffs_8;
            bool _S411 = undistort_point_0(uv_16, &_S410, int(12), &_S409);
            (&_S400)->_S393 = _S409;
            (&_S400)->_S394 = _S411;
            if(!!_S411)
            {
                _S402 = int(2);
            }
            else
            {
                _S402 = int(0);
            }
            if(_S402 != int(2))
            {
                _runFlag_0 = false;
            }
            else
            {
                _runFlag_0 = _S408;
            }
            if(_runFlag_0)
            {
                float2  uv_17 = (pix_center_1 + make_float2 (0.0f, -1.0f) - _S403) / _S404;
                float2  _S412 = _S399;
                FixedArray<float, 1>  _S413 = dist_coeffs_8;
                bool _S414 = undistort_point_0(uv_17, &_S413, int(12), &_S412);
                (&_S400)->_S395 = _S412;
                (&_S400)->_S396 = _S414;
                if(!_S414)
                {
                    _runFlag_0 = false;
                }
                if(_runFlag_0)
                {
                    float2  uv_18 = (pix_center_1 + make_float2 (0.0f, 1.0f) - _S403) / _S404;
                    float2  _S415 = _S399;
                    FixedArray<float, 1>  _S416 = dist_coeffs_8;
                    bool _S417 = undistort_point_0(uv_18, &_S416, int(12), &_S415);
                    (&_S400)->_S397 = _S415;
                    (&_S400)->_S398 = _S417;
                }
            }
        }
    }
    s_bwd_prop_depth_to_normal_Intermediates_0 _S418 = _S400;
    float3  _S419 = make_float3 (0.0f);
    if(_S401)
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
    bool _S420 = !_runFlag_0;
    bool _runFlag_1;
    bool _runFlag_2;
    bool _S421;
    bool _runFlag_3;
    bool _S422;
    bool _S423;
    FixedArray<float3 , 4>  points_3;
    float3  _S424;
    float3  _S425;
    float3  _S426;
    float3  _S427;
    float3  _S428;
    float3  _S429;
    float3  _S430;
    float3  _S431;
    float3  _S432;
    if(_S420)
    {
        bool _S433 = !!_S418._S392;
        if(_S433)
        {
            float3  _S434 = s_primal_ctx_unproject_raydir_0(_S418._S391, camera_model_6, is_ray_depth_6);
            float3  _S435 = make_float3 (depths_1.x) * _S434;
            bool _S436 = !!_S418._S394;
            if(_S436)
            {
                float3  _S437 = s_primal_ctx_unproject_raydir_0(_S418._S393, camera_model_6, is_ray_depth_6);
                float3  _S438 = make_float3 (depths_1.y) * _S437;
                _S402 = int(2);
                points_3[int(0)] = _S435;
                points_3[int(1)] = _S438;
                points_3[int(2)] = _S419;
                points_3[int(3)] = _S419;
                _S424 = _S437;
            }
            else
            {
                _S402 = int(0);
                points_3[int(0)] = _S435;
                points_3[int(1)] = _S419;
                points_3[int(2)] = _S419;
                points_3[int(3)] = _S419;
                _S424 = _S419;
            }
            if(_S402 != int(2))
            {
                _runFlag_0 = false;
            }
            else
            {
                _runFlag_0 = _S433;
                _S402 = int(0);
            }
            if(_runFlag_0)
            {
                if(!_S418._S396)
                {
                    _runFlag_1 = false;
                    _S402 = int(0);
                }
                else
                {
                    _runFlag_1 = _runFlag_0;
                }
                if(_runFlag_1)
                {
                    float3  _S439 = s_primal_ctx_unproject_raydir_0(_S418._S395, camera_model_6, is_ray_depth_6);
                    points_3[int(2)] = make_float3 (depths_1.z) * _S439;
                    bool _S440 = !!_S418._S398;
                    int _S441;
                    if(_S440)
                    {
                        float3  _S442 = s_primal_ctx_unproject_raydir_0(_S418._S397, camera_model_6, is_ray_depth_6);
                        points_3[int(3)] = make_float3 (depths_1.w) * _S442;
                        _S441 = int(2);
                        _S425 = _S442;
                    }
                    else
                    {
                        _S441 = int(0);
                        _S425 = _S419;
                    }
                    if(_S441 != int(2))
                    {
                        _runFlag_2 = false;
                        _S402 = _S441;
                    }
                    else
                    {
                        _runFlag_2 = _runFlag_1;
                    }
                    if(_runFlag_2)
                    {
                        _S402 = int(1);
                    }
                    _runFlag_2 = _S440;
                    _S426 = _S439;
                }
                else
                {
                    _runFlag_2 = false;
                    _S425 = _S419;
                    _S426 = _S419;
                }
            }
            else
            {
                _runFlag_1 = false;
                _runFlag_2 = false;
                _S425 = _S419;
                _S426 = _S419;
            }
            float3  _S443 = _S424;
            _S424 = _S425;
            _S425 = _S426;
            _S421 = _S436;
            _S426 = _S443;
            _S427 = _S434;
        }
        else
        {
            _S402 = int(0);
            points_3[int(0)] = _S419;
            points_3[int(1)] = _S419;
            points_3[int(2)] = _S419;
            points_3[int(3)] = _S419;
            _runFlag_0 = false;
            _runFlag_1 = false;
            _runFlag_2 = false;
            _S424 = _S419;
            _S425 = _S419;
            _S421 = false;
            _S426 = _S419;
            _S427 = _S419;
        }
        if(_S402 != int(1))
        {
            _runFlag_3 = false;
        }
        else
        {
            _runFlag_3 = _S420;
        }
        if(_runFlag_3)
        {
            float3  dx_1 = points_3[int(1)] - points_3[int(0)];
            float3  _S444 = - (points_3[int(3)] - points_3[int(2)]);
            float3  _S445 = s_primal_ctx_cross_0(dx_1, _S444);
            bool _S446 = (s_primal_ctx_dot_0(_S445, _S445)) != 0.0f;
            if(_S446)
            {
                float _S447 = length_0(_S445);
                float3  _S448 = make_float3 (_S447);
                _S428 = make_float3 (_S447 * _S447);
                _S429 = _S448;
            }
            else
            {
                _S428 = _S419;
                _S429 = _S419;
            }
            float3  _S449 = _S429;
            _S422 = _S446;
            _S429 = _S445;
            _S430 = _S449;
            _S431 = dx_1;
            _S432 = _S444;
        }
        else
        {
            _S422 = false;
            _S428 = _S419;
            _S429 = _S419;
            _S430 = _S419;
            _S431 = _S419;
            _S432 = _S419;
        }
        bool _S450 = _runFlag_0;
        bool _S451 = _runFlag_1;
        bool _S452 = _runFlag_2;
        float3  _S453 = _S424;
        float3  _S454 = _S425;
        bool _S455 = _S421;
        float3  _S456 = _S426;
        float3  _S457 = _S427;
        _runFlag_0 = _runFlag_3;
        _runFlag_1 = _S422;
        _S424 = _S428;
        _S425 = _S429;
        _S426 = _S430;
        _S427 = _S431;
        _S428 = _S432;
        _runFlag_2 = _S433;
        _S421 = _S450;
        _runFlag_3 = _S451;
        _S422 = _S452;
        _S429 = _S453;
        _S430 = _S454;
        _S423 = _S455;
        _S431 = _S456;
        _S432 = _S457;
    }
    else
    {
        _runFlag_0 = false;
        _runFlag_1 = false;
        _S424 = _S419;
        _S425 = _S419;
        _S426 = _S419;
        _S427 = _S419;
        _S428 = _S419;
        _runFlag_2 = false;
        _S421 = false;
        _runFlag_3 = false;
        _S422 = false;
        _S429 = _S419;
        _S430 = _S419;
        _S423 = false;
        _S431 = _S419;
        _S432 = _S419;
    }
    float4  _S458 = make_float4 (0.0f);
    float4  _S459;
    if(_S420)
    {
        if(_runFlag_0)
        {
            if(_runFlag_1)
            {
                float3  _S460 = v_normal_1 / _S424;
                float3  _S461 = _S425 * - _S460;
                float3  _S462 = _S426 * _S460;
                float _S463 = _S461.x + _S461.y + _S461.z;
                DiffPair_vectorx3Cfloatx2C3x3E_0 _S464;
                (&_S464)->primal_0 = _S425;
                (&_S464)->differential_0 = _S419;
                s_bwd_length_impl_0(&_S464, _S463);
                _S424 = _S462 + _S464.differential_0;
            }
            else
            {
                _S424 = v_normal_1;
            }
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S465;
            (&_S465)->primal_0 = _S425;
            (&_S465)->differential_0 = _S419;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S466;
            (&_S466)->primal_0 = _S425;
            (&_S466)->differential_0 = _S419;
            s_bwd_prop_dot_0(&_S465, &_S466, 0.0f);
            float3  _S467 = _S466.differential_0 + _S465.differential_0 + _S424;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S468;
            (&_S468)->primal_0 = _S427;
            (&_S468)->differential_0 = _S419;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S469;
            (&_S469)->primal_0 = _S428;
            (&_S469)->differential_0 = _S419;
            s_bwd_prop_cross_0(&_S468, &_S469, _S467);
            float3  s_diff_dy_T_1 = - _S469.differential_0;
            float3  _S470 = - s_diff_dy_T_1;
            float3  _S471 = - _S468.differential_0;
            FixedArray<float3 , 4>  _S472;
            _S472[int(0)] = _S419;
            _S472[int(1)] = _S419;
            _S472[int(2)] = _S419;
            _S472[int(3)] = _S419;
            _S472[int(2)] = _S470;
            _S472[int(3)] = s_diff_dy_T_1;
            _S472[int(0)] = _S471;
            _S472[int(1)] = _S468.differential_0;
            points_3[int(0)] = _S472[int(0)];
            points_3[int(1)] = _S472[int(1)];
            points_3[int(2)] = _S472[int(2)];
            points_3[int(3)] = _S472[int(3)];
        }
        else
        {
            points_3[int(0)] = _S419;
            points_3[int(1)] = _S419;
            points_3[int(2)] = _S419;
            points_3[int(3)] = _S419;
        }
        if(_runFlag_2)
        {
            if(_S421)
            {
                if(_runFlag_3)
                {
                    FixedArray<float3 , 4>  _S473 = points_3;
                    FixedArray<float3 , 4>  _S474 = points_3;
                    FixedArray<float3 , 4>  _S475 = points_3;
                    FixedArray<float3 , 4>  _S476 = points_3;
                    if(_S422)
                    {
                        float3  _S477 = _S429 * _S476[int(3)];
                        float _S478 = _S477.x + _S477.y + _S477.z;
                        float4  _S479 = _S458;
                        *&((&_S479)->w) = _S478;
                        points_3[int(0)] = _S473[int(0)];
                        points_3[int(1)] = _S474[int(1)];
                        points_3[int(2)] = _S475[int(2)];
                        points_3[int(3)] = _S419;
                        _S459 = _S479;
                    }
                    else
                    {
                        points_3[int(0)] = _S473[int(0)];
                        points_3[int(1)] = _S474[int(1)];
                        points_3[int(2)] = _S475[int(2)];
                        points_3[int(3)] = _S476[int(3)];
                        _S459 = _S458;
                    }
                    float3  _S480 = _S430 * points_3[int(2)];
                    float _S481 = _S480.x + _S480.y + _S480.z;
                    FixedArray<float3 , 4>  _S482 = points_3;
                    FixedArray<float3 , 4>  _S483 = points_3;
                    float4  _S484 = _S458;
                    *&((&_S484)->z) = _S481;
                    float4  _S485 = _S459 + _S484;
                    points_3[int(0)] = points_3[int(0)];
                    points_3[int(1)] = _S482[int(1)];
                    points_3[int(2)] = _S419;
                    points_3[int(3)] = _S483[int(3)];
                    _S459 = _S485;
                }
                else
                {
                    FixedArray<float3 , 4>  _S486 = points_3;
                    FixedArray<float3 , 4>  _S487 = points_3;
                    FixedArray<float3 , 4>  _S488 = points_3;
                    points_3[int(0)] = points_3[int(0)];
                    points_3[int(1)] = _S486[int(1)];
                    points_3[int(2)] = _S487[int(2)];
                    points_3[int(3)] = _S488[int(3)];
                    _S459 = _S458;
                }
            }
            else
            {
                FixedArray<float3 , 4>  _S489 = points_3;
                FixedArray<float3 , 4>  _S490 = points_3;
                FixedArray<float3 , 4>  _S491 = points_3;
                points_3[int(0)] = points_3[int(0)];
                points_3[int(1)] = _S489[int(1)];
                points_3[int(2)] = _S490[int(2)];
                points_3[int(3)] = _S491[int(3)];
                _S459 = _S458;
            }
            if(_S423)
            {
                FixedArray<float3 , 4>  _S492 = points_3;
                float3  _S493 = _S431 * points_3[int(1)];
                float _S494 = _S493.x + _S493.y + _S493.z;
                float4  _S495 = _S458;
                *&((&_S495)->y) = _S494;
                float4  _S496 = _S459 + _S495;
                points_3[int(0)] = _S419;
                points_3[int(1)] = _S419;
                points_3[int(2)] = _S419;
                points_3[int(3)] = _S419;
                _S424 = _S492[int(0)];
                _S459 = _S496;
            }
            else
            {
                FixedArray<float3 , 4>  _S497 = points_3;
                FixedArray<float3 , 4>  _S498 = points_3;
                FixedArray<float3 , 4>  _S499 = points_3;
                points_3[int(0)] = points_3[int(0)];
                points_3[int(1)] = _S497[int(1)];
                points_3[int(2)] = _S498[int(2)];
                points_3[int(3)] = _S499[int(3)];
                _S424 = _S419;
            }
            float3  _S500 = _S432 * (points_3[int(0)] + _S424);
            float _S501 = _S500.x + _S500.y + _S500.z;
            float4  _S502 = _S458;
            *&((&_S502)->x) = _S501;
            _S459 = _S459 + _S502;
        }
        else
        {
            _S459 = _S458;
        }
    }
    else
    {
        _S459 = _S458;
    }
    *v_depths_0 = _S459;
    return;
}

inline __device__ float ray_depth_to_linear_depth_factor_none(float2  pix_center_2, float4  intrins_5, FixedArray<float, 1>  dist_coeffs_9, int camera_model_7)
{
    float _S503;
    for(;;)
    {
        float2  uv_19 = (pix_center_2 - float2 {intrins_5.z, intrins_5.w}) / float2 {intrins_5.x, intrins_5.y};
        FixedArray<float, 1>  _S504 = dist_coeffs_9;
        float2  uv_u_6;
        bool _S505 = undistort_point_0(uv_19, &_S504, int(12), &uv_u_6);
        if(!_S505)
        {
            _S503 = 0.0f;
            break;
        }
        float3  raydir_2 = unproject_raydir_0(uv_u_6, camera_model_7, false);
        _S503 = float((F32_sign((raydir_2.z)))) / length_0(raydir_2);
        break;
    }
    return _S503;
}

inline __device__ float depth_normal_loss_none(float2  pix_center_3, float4  intrins_6, FixedArray<float, 1>  dist_coeffs_10, int camera_model_8, bool is_ray_depth_7, float4  depths_2, float3  gt_normal_0)
{
    float _S506;
    for(;;)
    {
        float3  _S507;
        float3  * _S508;
        float3  * _S509;
        float3  * _S510;
        float3  * _S511;
        int _S512;
        FixedArray<float3 , 5>  points_4;
        for(;;)
        {
            float2  _S513 = float2 {intrins_6.z, intrins_6.w};
            float2  _S514 = float2 {intrins_6.x, intrins_6.y};
            float2  uv_20 = (pix_center_3 + make_float2 (-1.0f, -0.0f) - _S513) / _S514;
            FixedArray<float, 1>  _S515 = dist_coeffs_10;
            float2  uv_u_7;
            bool _S516 = undistort_point_0(uv_20, &_S515, int(12), &uv_u_7);
            float3  _S517 = make_float3 (0.0f);
            if(!_S516)
            {
                _S512 = int(0);
                _S511 = nullptr;
                _S510 = nullptr;
                _S509 = nullptr;
                _S508 = nullptr;
                _S507 = _S517;
                break;
            }
            float3  raydir_3 = unproject_raydir_0(uv_u_7, camera_model_8, is_ray_depth_7);
            points_4[int(0)] = make_float3 (depths_2.x) * raydir_3;
            float2  uv_21 = (pix_center_3 + make_float2 (1.0f, -0.0f) - _S513) / _S514;
            FixedArray<float, 1>  _S518 = dist_coeffs_10;
            float2  uv_u_8;
            bool _S519 = undistort_point_0(uv_21, &_S518, int(12), &uv_u_8);
            if(!_S519)
            {
                _S512 = int(0);
                _S511 = nullptr;
                _S510 = &points_4[int(0)];
                _S509 = nullptr;
                _S508 = nullptr;
                _S507 = _S517;
                break;
            }
            float3  raydir_4 = unproject_raydir_0(uv_u_8, camera_model_8, is_ray_depth_7);
            points_4[int(1)] = make_float3 (depths_2.y) * raydir_4;
            float2  uv_22 = (pix_center_3 + make_float2 (0.0f, -1.0f) - _S513) / _S514;
            FixedArray<float, 1>  _S520 = dist_coeffs_10;
            float2  uv_u_9;
            bool _S521 = undistort_point_0(uv_22, &_S520, int(12), &uv_u_9);
            if(!_S521)
            {
                _S512 = int(0);
                _S511 = &points_4[int(1)];
                _S510 = &points_4[int(0)];
                _S509 = nullptr;
                _S508 = nullptr;
                _S507 = _S517;
                break;
            }
            float3  raydir_5 = unproject_raydir_0(uv_u_9, camera_model_8, is_ray_depth_7);
            points_4[int(2)] = make_float3 (depths_2.z) * raydir_5;
            float2  uv_23 = (pix_center_3 + make_float2 (0.0f, 1.0f) - _S513) / _S514;
            FixedArray<float, 1>  _S522 = dist_coeffs_10;
            float2  uv_u_10;
            bool _S523 = undistort_point_0(uv_23, &_S522, int(12), &uv_u_10);
            if(!_S523)
            {
                _S512 = int(0);
                _S511 = &points_4[int(1)];
                _S510 = &points_4[int(0)];
                _S509 = nullptr;
                _S508 = &points_4[int(2)];
                _S507 = _S517;
                break;
            }
            float3  raydir_6 = unproject_raydir_0(uv_u_10, camera_model_8, is_ray_depth_7);
            points_4[int(3)] = make_float3 (depths_2.w) * raydir_6;
            float2  uv_24 = (pix_center_3 + make_float2 (0.0f) * make_float2 (0.0f, 3.0f) - _S513) / _S514;
            FixedArray<float, 1>  _S524 = dist_coeffs_10;
            float2  uv_u_11;
            bool _S525 = undistort_point_0(uv_24, &_S524, int(12), &uv_u_11);
            if(!_S525)
            {
                _S512 = int(0);
                _S511 = &points_4[int(1)];
                _S510 = &points_4[int(0)];
                _S509 = &points_4[int(3)];
                _S508 = &points_4[int(2)];
                _S507 = _S517;
                break;
            }
            float3  raydir_7 = unproject_raydir_0(uv_u_11, camera_model_8, is_ray_depth_7);
            _S512 = int(1);
            _S511 = &points_4[int(1)];
            _S510 = &points_4[int(0)];
            _S509 = &points_4[int(3)];
            _S508 = &points_4[int(2)];
            _S507 = raydir_7;
            break;
        }
        if(_S512 != int(1))
        {
            _S506 = 0.0f;
            break;
        }
        float3  normal_4 = cross_0(*_S511 - *_S510, - (*_S509 - *_S508));
        float3  normal_5;
        if((dot_0(normal_4, normal_4)) != 0.0f)
        {
            normal_5 = normalize_0(normal_4);
        }
        else
        {
            normal_5 = normal_4;
        }
        float3  _S526;
        if((dot_0(gt_normal_0, gt_normal_0)) != 0.0f)
        {
            _S526 = normalize_0(gt_normal_0);
        }
        else
        {
            _S526 = gt_normal_0;
        }
        _S506 = (1.0f - dot_0(normal_5, _S526) + 0.00100000004749745f) / ((F32_max((dot_0(normal_5, - normalize_0(_S507))), (0.0f))) + 0.00100000004749745f);
        break;
    }
    return _S506;
}

struct s_bwd_prop_depth_normal_loss_Intermediates_0
{
    float2  _S527;
    bool _S528;
    float2  _S529;
    bool _S530;
    float2  _S531;
    bool _S532;
    float2  _S533;
    bool _S534;
    float2  _S535;
    bool _S536;
};

inline __device__ void s_bwd_prop_normalize_impl_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_6, float3  _s_dOut_5)
{
    float _S537 = length_0((*dpx_6).primal_0);
    float3  _S538 = (*dpx_6).primal_0 * _s_dOut_5;
    float3  _S539 = make_float3 (1.0f / _S537) * _s_dOut_5;
    float _S540 = - ((_S538.x + _S538.y + _S538.z) / (_S537 * _S537));
    float3  _S541 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S542;
    (&_S542)->primal_0 = (*dpx_6).primal_0;
    (&_S542)->differential_0 = _S541;
    s_bwd_length_impl_0(&_S542, _S540);
    float3  _S543 = _S539 + _S542.differential_0;
    dpx_6->primal_0 = (*dpx_6).primal_0;
    dpx_6->differential_0 = _S543;
    return;
}

inline __device__ void s_bwd_normalize_impl_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S544, float3  _S545)
{
    s_bwd_prop_normalize_impl_0(_S544, _S545);
    return;
}

inline __device__ void depth_normal_loss_vjp_none(float2  pix_center_4, float4  intrins_7, FixedArray<float, 1>  dist_coeffs_11, int camera_model_9, bool is_ray_depth_8, float4  depths_3, float3  gt_normal_1, float v_loss_0, float4  * v_depths_1, float3  * v_gt_normal_0)
{
    float2  _S546 = make_float2 (0.0f);
    s_bwd_prop_depth_normal_loss_Intermediates_0 _S547;
    (&_S547)->_S527 = _S546;
    (&_S547)->_S528 = false;
    (&_S547)->_S529 = _S546;
    (&_S547)->_S530 = false;
    (&_S547)->_S531 = _S546;
    (&_S547)->_S532 = false;
    (&_S547)->_S533 = _S546;
    (&_S547)->_S534 = false;
    (&_S547)->_S535 = _S546;
    (&_S547)->_S536 = false;
    (&_S547)->_S529 = _S546;
    (&_S547)->_S530 = false;
    (&_S547)->_S531 = _S546;
    (&_S547)->_S532 = false;
    (&_S547)->_S533 = _S546;
    (&_S547)->_S534 = false;
    (&_S547)->_S535 = _S546;
    (&_S547)->_S536 = false;
    float2  _S548 = float2 {intrins_7.z, intrins_7.w};
    float2  _S549 = float2 {intrins_7.x, intrins_7.y};
    float2  uv_25 = (pix_center_4 + make_float2 (-1.0f, -0.0f) - _S548) / _S549;
    float2  _S550 = _S546;
    FixedArray<float, 1>  _S551 = dist_coeffs_11;
    bool _S552 = undistort_point_0(uv_25, &_S551, int(12), &_S550);
    (&_S547)->_S527 = _S550;
    (&_S547)->_S528 = _S552;
    bool _S553 = !!_S552;
    bool _runFlag_4;
    if(_S553)
    {
        float2  uv_26 = (pix_center_4 + make_float2 (1.0f, -0.0f) - _S548) / _S549;
        float2  _S554 = _S546;
        FixedArray<float, 1>  _S555 = dist_coeffs_11;
        bool _S556 = undistort_point_0(uv_26, &_S555, int(12), &_S554);
        (&_S547)->_S529 = _S554;
        (&_S547)->_S530 = _S556;
        if(!_S556)
        {
            _runFlag_4 = false;
        }
        else
        {
            _runFlag_4 = _S553;
        }
        if(_runFlag_4)
        {
            float2  uv_27 = (pix_center_4 + make_float2 (0.0f, -1.0f) - _S548) / _S549;
            float2  _S557 = _S546;
            FixedArray<float, 1>  _S558 = dist_coeffs_11;
            bool _S559 = undistort_point_0(uv_27, &_S558, int(12), &_S557);
            (&_S547)->_S531 = _S557;
            (&_S547)->_S532 = _S559;
            if(!_S559)
            {
                _runFlag_4 = false;
            }
            if(_runFlag_4)
            {
                float2  uv_28 = (pix_center_4 + make_float2 (0.0f, 1.0f) - _S548) / _S549;
                float2  _S560 = _S546;
                FixedArray<float, 1>  _S561 = dist_coeffs_11;
                bool _S562 = undistort_point_0(uv_28, &_S561, int(12), &_S560);
                (&_S547)->_S533 = _S560;
                (&_S547)->_S534 = _S562;
                if(!_S562)
                {
                    _runFlag_4 = false;
                }
                if(_runFlag_4)
                {
                    float2  uv_29 = (pix_center_4 - _S548) / _S549;
                    float2  _S563 = _S546;
                    FixedArray<float, 1>  _S564 = dist_coeffs_11;
                    bool _S565 = undistort_point_0(uv_29, &_S564, int(12), &_S563);
                    (&_S547)->_S535 = _S563;
                    (&_S547)->_S536 = _S565;
                }
            }
        }
    }
    s_bwd_prop_depth_normal_loss_Intermediates_0 _S566 = _S547;
    float3  _S567 = make_float3 (0.0f);
    bool _S568 = !!_S547._S528;
    bool _runFlag_5;
    bool _runFlag_6;
    bool _runFlag_7;
    int _S569;
    float3  raydir_8;
    float3  _S570;
    float3  _S571;
    float3  _S572;
    float3  _S573;
    FixedArray<float3 , 5>  points_5;
    if(_S568)
    {
        float3  _S574 = s_primal_ctx_unproject_raydir_0(_S566._S527, camera_model_9, is_ray_depth_8);
        float3  _S575 = make_float3 (depths_3.x) * _S574;
        if(!_S566._S530)
        {
            _runFlag_4 = false;
        }
        else
        {
            _runFlag_4 = _S568;
        }
        if(_runFlag_4)
        {
            float3  _S576 = s_primal_ctx_unproject_raydir_0(_S566._S529, camera_model_9, is_ray_depth_8);
            float3  _S577 = make_float3 (depths_3.y) * _S576;
            if(!_S566._S532)
            {
                _runFlag_5 = false;
            }
            else
            {
                _runFlag_5 = _runFlag_4;
            }
            if(_runFlag_5)
            {
                float3  _S578 = s_primal_ctx_unproject_raydir_0(_S566._S531, camera_model_9, is_ray_depth_8);
                float3  _S579 = make_float3 (depths_3.z) * _S578;
                if(!_S566._S534)
                {
                    _runFlag_6 = false;
                }
                else
                {
                    _runFlag_6 = _runFlag_5;
                }
                if(_runFlag_6)
                {
                    float3  _S580 = s_primal_ctx_unproject_raydir_0(_S566._S533, camera_model_9, is_ray_depth_8);
                    float3  _S581 = make_float3 (depths_3.w) * _S580;
                    if(!_S566._S536)
                    {
                        _runFlag_7 = false;
                    }
                    else
                    {
                        _runFlag_7 = _runFlag_6;
                    }
                    if(_runFlag_7)
                    {
                        float3  _S582 = s_primal_ctx_unproject_raydir_0(_S566._S535, camera_model_9, is_ray_depth_8);
                        _S569 = int(1);
                        raydir_8 = _S582;
                    }
                    else
                    {
                        _S569 = int(0);
                        raydir_8 = _S580;
                    }
                    points_5[int(0)] = _S575;
                    points_5[int(1)] = _S577;
                    points_5[int(2)] = _S579;
                    points_5[int(3)] = _S581;
                    points_5[int(4)] = _S567;
                    _S570 = _S580;
                }
                else
                {
                    _S569 = int(0);
                    raydir_8 = _S578;
                    points_5[int(0)] = _S575;
                    points_5[int(1)] = _S577;
                    points_5[int(2)] = _S579;
                    points_5[int(3)] = _S567;
                    points_5[int(4)] = _S567;
                    _S570 = _S567;
                }
                _S571 = _S578;
            }
            else
            {
                _S569 = int(0);
                raydir_8 = _S576;
                points_5[int(0)] = _S575;
                points_5[int(1)] = _S577;
                points_5[int(2)] = _S567;
                points_5[int(3)] = _S567;
                points_5[int(4)] = _S567;
                _runFlag_6 = false;
                _S570 = _S567;
                _S571 = _S567;
            }
            _S572 = _S576;
        }
        else
        {
            _S569 = int(0);
            raydir_8 = _S574;
            points_5[int(0)] = _S575;
            points_5[int(1)] = _S567;
            points_5[int(2)] = _S567;
            points_5[int(3)] = _S567;
            points_5[int(4)] = _S567;
            _runFlag_5 = false;
            _runFlag_6 = false;
            _S570 = _S567;
            _S571 = _S567;
            _S572 = _S567;
        }
        _S573 = _S574;
    }
    else
    {
        _S569 = int(0);
        points_5[int(0)] = _S567;
        points_5[int(1)] = _S567;
        points_5[int(2)] = _S567;
        points_5[int(3)] = _S567;
        points_5[int(4)] = _S567;
        _runFlag_4 = false;
        _runFlag_5 = false;
        _runFlag_6 = false;
        _S570 = _S567;
        _S571 = _S567;
        _S572 = _S567;
        _S573 = _S567;
    }
    bool _S583 = !(_S569 != int(1));
    bool _S584;
    float3  normal_6;
    float3  _S585;
    float3  _S586;
    float3  _S587;
    float3  _S588;
    float _S589;
    float _S590;
    float _S591;
    float _S592;
    if(_S583)
    {
        float3  dx_2 = points_5[int(1)] - points_5[int(0)];
        float3  _S593 = - (points_5[int(3)] - points_5[int(2)]);
        float3  _S594 = s_primal_ctx_cross_0(dx_2, _S593);
        bool _S595 = (s_primal_ctx_dot_0(_S594, _S594)) != 0.0f;
        if(_S595)
        {
            normal_6 = normalize_0(_S594);
        }
        else
        {
            normal_6 = _S594;
        }
        bool _S596 = (s_primal_ctx_dot_0(gt_normal_1, gt_normal_1)) != 0.0f;
        if(_S596)
        {
            _S585 = normalize_0(gt_normal_1);
        }
        else
        {
            _S585 = gt_normal_1;
        }
        float3  _S597 = - normalize_0(raydir_8);
        float _S598 = s_primal_ctx_dot_0(normal_6, _S597);
        float _S599 = 1.0f - s_primal_ctx_dot_0(normal_6, _S585) + 0.00100000004749745f;
        float _S600 = (F32_max((_S598), (0.0f))) + 0.00100000004749745f;
        _S589 = _S600 * _S600;
        _S590 = _S599;
        _S591 = _S600;
        _S592 = _S598;
        raydir_8 = normal_6;
        normal_6 = _S597;
        _runFlag_7 = _S596;
        _S584 = _S595;
        _S586 = _S594;
        _S587 = dx_2;
        _S588 = _S593;
    }
    else
    {
        _S589 = 0.0f;
        _S590 = 0.0f;
        _S591 = 0.0f;
        _S592 = 0.0f;
        raydir_8 = _S567;
        normal_6 = _S567;
        _S585 = _S567;
        _runFlag_7 = false;
        _S584 = false;
        _S586 = _S567;
        _S587 = _S567;
        _S588 = _S567;
    }
    float4  _S601 = make_float4 (0.0f);
    if(_S583)
    {
        float _S602 = v_loss_0 / _S589;
        float _S603 = _S590 * - _S602;
        float s_diff_num_T_0 = _S591 * _S602;
        DiffPair_float_0 _S604;
        (&_S604)->primal_0 = _S592;
        (&_S604)->differential_0 = 0.0f;
        DiffPair_float_0 _S605;
        (&_S605)->primal_0 = 0.0f;
        (&_S605)->differential_0 = 0.0f;
        _d_max_0(&_S604, &_S605, _S603);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S606;
        (&_S606)->primal_0 = raydir_8;
        (&_S606)->differential_0 = _S567;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S607;
        (&_S607)->primal_0 = normal_6;
        (&_S607)->differential_0 = _S567;
        s_bwd_prop_dot_0(&_S606, &_S607, _S604.differential_0);
        float _S608 = - s_diff_num_T_0;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S609;
        (&_S609)->primal_0 = raydir_8;
        (&_S609)->differential_0 = _S567;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S610;
        (&_S610)->primal_0 = _S585;
        (&_S610)->differential_0 = _S567;
        s_bwd_prop_dot_0(&_S609, &_S610, _S608);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S611 = _S610;
        float3  _S612 = _S606.differential_0 + _S609.differential_0;
        if(_runFlag_7)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S613;
            (&_S613)->primal_0 = gt_normal_1;
            (&_S613)->differential_0 = _S567;
            s_bwd_normalize_impl_0(&_S613, _S611.differential_0);
            raydir_8 = _S613.differential_0;
        }
        else
        {
            raydir_8 = _S611.differential_0;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S614;
        (&_S614)->primal_0 = gt_normal_1;
        (&_S614)->differential_0 = _S567;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S615;
        (&_S615)->primal_0 = gt_normal_1;
        (&_S615)->differential_0 = _S567;
        s_bwd_prop_dot_0(&_S614, &_S615, 0.0f);
        float3  _S616 = _S615.differential_0 + _S614.differential_0 + raydir_8;
        if(_S584)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S617;
            (&_S617)->primal_0 = _S586;
            (&_S617)->differential_0 = _S567;
            s_bwd_normalize_impl_0(&_S617, _S612);
            raydir_8 = _S617.differential_0;
        }
        else
        {
            raydir_8 = _S612;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S618;
        (&_S618)->primal_0 = _S586;
        (&_S618)->differential_0 = _S567;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S619;
        (&_S619)->primal_0 = _S586;
        (&_S619)->differential_0 = _S567;
        s_bwd_prop_dot_0(&_S618, &_S619, 0.0f);
        float3  _S620 = _S619.differential_0 + _S618.differential_0 + raydir_8;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S621;
        (&_S621)->primal_0 = _S587;
        (&_S621)->differential_0 = _S567;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S622;
        (&_S622)->primal_0 = _S588;
        (&_S622)->differential_0 = _S567;
        s_bwd_prop_cross_0(&_S621, &_S622, _S620);
        float3  s_diff_dy_T_2 = - _S622.differential_0;
        float3  _S623 = - s_diff_dy_T_2;
        float3  _S624 = - _S621.differential_0;
        FixedArray<float3 , 5>  _S625;
        _S625[int(0)] = _S567;
        _S625[int(1)] = _S567;
        _S625[int(2)] = _S567;
        _S625[int(3)] = _S567;
        _S625[int(4)] = _S567;
        _S625[int(2)] = _S623;
        _S625[int(3)] = s_diff_dy_T_2;
        _S625[int(0)] = _S624;
        _S625[int(1)] = _S621.differential_0;
        points_5[int(0)] = _S625[int(0)];
        points_5[int(1)] = _S625[int(1)];
        points_5[int(2)] = _S625[int(2)];
        points_5[int(3)] = _S625[int(3)];
        points_5[int(4)] = _S625[int(4)];
        raydir_8 = _S616;
    }
    else
    {
        points_5[int(0)] = _S567;
        points_5[int(1)] = _S567;
        points_5[int(2)] = _S567;
        points_5[int(3)] = _S567;
        points_5[int(4)] = _S567;
        raydir_8 = _S567;
    }
    float4  _S626;
    if(_S568)
    {
        if(_runFlag_4)
        {
            if(_runFlag_5)
            {
                if(_runFlag_6)
                {
                    FixedArray<float3 , 5>  _S627 = points_5;
                    FixedArray<float3 , 5>  _S628 = points_5;
                    FixedArray<float3 , 5>  _S629 = points_5;
                    float3  _S630 = _S570 * points_5[int(3)];
                    float _S631 = _S630.x + _S630.y + _S630.z;
                    float4  _S632 = _S601;
                    *&((&_S632)->w) = _S631;
                    points_5[int(0)] = _S567;
                    points_5[int(1)] = _S567;
                    points_5[int(2)] = _S567;
                    points_5[int(3)] = _S567;
                    points_5[int(4)] = _S567;
                    _S570 = _S629[int(2)];
                    normal_6 = _S627[int(0)];
                    _S585 = _S628[int(1)];
                    _S626 = _S632;
                }
                else
                {
                    FixedArray<float3 , 5>  _S633 = points_5;
                    FixedArray<float3 , 5>  _S634 = points_5;
                    FixedArray<float3 , 5>  _S635 = points_5;
                    FixedArray<float3 , 5>  _S636 = points_5;
                    points_5[int(0)] = points_5[int(0)];
                    points_5[int(1)] = _S633[int(1)];
                    points_5[int(2)] = _S634[int(2)];
                    points_5[int(3)] = _S635[int(3)];
                    points_5[int(4)] = _S636[int(4)];
                    _S570 = _S567;
                    normal_6 = _S567;
                    _S585 = _S567;
                    _S626 = _S601;
                }
                float3  _S637 = _S571 * (points_5[int(2)] + _S570);
                float _S638 = _S637.x + _S637.y + _S637.z;
                float3  _S639 = points_5[int(0)] + normal_6;
                float3  _S640 = points_5[int(1)] + _S585;
                float4  _S641 = _S601;
                *&((&_S641)->z) = _S638;
                float4  _S642 = _S626 + _S641;
                points_5[int(0)] = _S567;
                points_5[int(1)] = _S567;
                points_5[int(2)] = _S567;
                points_5[int(3)] = _S567;
                points_5[int(4)] = _S567;
                _S570 = _S640;
                _S571 = _S639;
                _S626 = _S642;
            }
            else
            {
                FixedArray<float3 , 5>  _S643 = points_5;
                FixedArray<float3 , 5>  _S644 = points_5;
                FixedArray<float3 , 5>  _S645 = points_5;
                FixedArray<float3 , 5>  _S646 = points_5;
                points_5[int(0)] = points_5[int(0)];
                points_5[int(1)] = _S643[int(1)];
                points_5[int(2)] = _S644[int(2)];
                points_5[int(3)] = _S645[int(3)];
                points_5[int(4)] = _S646[int(4)];
                _S570 = _S567;
                _S571 = _S567;
                _S626 = _S601;
            }
            float3  _S647 = _S572 * (points_5[int(1)] + _S570);
            float _S648 = _S647.x + _S647.y + _S647.z;
            float3  _S649 = points_5[int(0)] + _S571;
            float4  _S650 = _S601;
            *&((&_S650)->y) = _S648;
            float4  _S651 = _S626 + _S650;
            points_5[int(0)] = _S567;
            points_5[int(1)] = _S567;
            points_5[int(2)] = _S567;
            points_5[int(3)] = _S567;
            points_5[int(4)] = _S567;
            _S570 = _S649;
            _S626 = _S651;
        }
        else
        {
            FixedArray<float3 , 5>  _S652 = points_5;
            FixedArray<float3 , 5>  _S653 = points_5;
            FixedArray<float3 , 5>  _S654 = points_5;
            FixedArray<float3 , 5>  _S655 = points_5;
            points_5[int(0)] = points_5[int(0)];
            points_5[int(1)] = _S652[int(1)];
            points_5[int(2)] = _S653[int(2)];
            points_5[int(3)] = _S654[int(3)];
            points_5[int(4)] = _S655[int(4)];
            _S570 = _S567;
            _S626 = _S601;
        }
        float3  _S656 = _S573 * (points_5[int(0)] + _S570);
        float _S657 = _S656.x + _S656.y + _S656.z;
        float4  _S658 = _S601;
        *&((&_S658)->x) = _S657;
        _S626 = _S626 + _S658;
    }
    else
    {
        _S626 = _S601;
    }
    *v_depths_1 = _S626;
    *v_gt_normal_0 = raydir_8;
    return;
}

inline __device__ float3  generate_ray_d2n_opencv(float2  pix_pos_3, float4  intrins_8, FixedArray<float, 4>  dist_coeffs_12, int camera_model_10, bool is_ray_depth_9)
{
    float3  _S659;
    for(;;)
    {
        float2  uv_30 = (pix_pos_3 - float2 {intrins_8.z, intrins_8.w}) / float2 {intrins_8.x, intrins_8.y};
        FixedArray<float, 4>  _S660 = dist_coeffs_12;
        float2  uv_u_12;
        bool _S661 = undistort_point_1(uv_30, &_S660, int(12), &uv_u_12);
        if(!_S661)
        {
            int3  _S662 = make_int3 (int(0));
            float3  _S663 = make_float3 ((float)_S662.x, (float)_S662.y, (float)_S662.z);
            _S659 = _S663;
            break;
        }
        _S659 = unproject_raydir_0(uv_u_12, camera_model_10, is_ray_depth_9);
        break;
    }
    return _S659;
}

inline __device__ float3  depth_to_point_opencv(float2  pix_pos_4, float4  intrins_9, FixedArray<float, 4>  dist_coeffs_13, int camera_model_11, bool is_ray_depth_10, float depth_4)
{
    float3  _S664;
    for(;;)
    {
        float2  uv_31 = (pix_pos_4 - float2 {intrins_9.z, intrins_9.w}) / float2 {intrins_9.x, intrins_9.y};
        FixedArray<float, 4>  _S665 = dist_coeffs_13;
        float2  uv_u_13;
        bool _S666 = undistort_point_1(uv_31, &_S665, int(12), &uv_u_13);
        if(!_S666)
        {
            _S664 = make_float3 (0.0f);
            break;
        }
        _S664 = make_float3 (depth_4) * unproject_raydir_0(uv_u_13, camera_model_11, is_ray_depth_10);
        break;
    }
    return _S664;
}

struct s_bwd_prop_depth_to_point_Intermediates_1
{
    float2  _S667;
    bool _S668;
};

inline __device__ float depth_to_point_vjp_opencv(float2  pix_pos_5, float4  intrins_10, FixedArray<float, 4>  dist_coeffs_14, int camera_model_12, bool is_ray_depth_11, float depth_5, float3  v_point_1)
{
    float2  _S669 = make_float2 (0.0f);
    s_bwd_prop_depth_to_point_Intermediates_1 _S670;
    (&_S670)->_S667 = _S669;
    (&_S670)->_S668 = false;
    float2  uv_32 = (pix_pos_5 - float2 {intrins_10.z, intrins_10.w}) / float2 {intrins_10.x, intrins_10.y};
    float2  _S671 = _S669;
    FixedArray<float, 4>  _S672 = dist_coeffs_14;
    bool _S673 = undistort_point_1(uv_32, &_S672, int(12), &_S671);
    (&_S670)->_S667 = _S671;
    (&_S670)->_S668 = _S673;
    s_bwd_prop_depth_to_point_Intermediates_1 _S674 = _S670;
    float3  _S675 = make_float3 (0.0f);
    bool _S676 = !!_S670._S668;
    float3  _S677;
    if(_S676)
    {
        _S677 = s_primal_ctx_unproject_raydir_0(_S674._S667, camera_model_12, is_ray_depth_11);
    }
    else
    {
        _S677 = _S675;
    }
    if(_S676)
    {
        _S677 = _S677 * v_point_1;
    }
    else
    {
        _S677 = _S675;
    }
    return _S677.x + _S677.y + _S677.z;
}

inline __device__ float3  depth_to_normal_opencv(float2  pix_center_5, float4  intrins_11, FixedArray<float, 4>  dist_coeffs_15, int camera_model_13, bool is_ray_depth_12, float4  depths_4)
{
    float3  normal_7;
    for(;;)
    {
        bool _S678;
        if((depths_4.x) == 0.0f)
        {
            _S678 = true;
        }
        else
        {
            _S678 = (depths_4.y) == 0.0f;
        }
        if(_S678)
        {
            _S678 = true;
        }
        else
        {
            _S678 = (depths_4.z) == 0.0f;
        }
        if(_S678)
        {
            _S678 = true;
        }
        else
        {
            _S678 = (depths_4.w) == 0.0f;
        }
        if(_S678)
        {
            normal_7 = make_float3 (0.0f);
            break;
        }
        float3  * _S679;
        float3  * _S680;
        float3  * _S681;
        float3  * _S682;
        int _S683;
        FixedArray<float3 , 4>  points_6;
        for(;;)
        {
            float2  _S684 = float2 {intrins_11.z, intrins_11.w};
            float2  _S685 = float2 {intrins_11.x, intrins_11.y};
            float2  uv_33 = (pix_center_5 + make_float2 (-1.0f, -0.0f) - _S684) / _S685;
            FixedArray<float, 4>  _S686 = dist_coeffs_15;
            float2  uv_u_14;
            bool _S687 = undistort_point_1(uv_33, &_S686, int(12), &uv_u_14);
            if(!_S687)
            {
                float3  _S688 = make_float3 (0.0f);
                _S683 = int(0);
                _S682 = nullptr;
                _S681 = nullptr;
                _S680 = nullptr;
                _S679 = nullptr;
                normal_7 = _S688;
                break;
            }
            points_6[int(0)] = make_float3 (depths_4.x) * unproject_raydir_0(uv_u_14, camera_model_13, is_ray_depth_12);
            for(;;)
            {
                float2  uv_34 = (pix_center_5 + make_float2 (1.0f, -0.0f) - _S684) / _S685;
                FixedArray<float, 4>  _S689 = dist_coeffs_15;
                float2  uv_u_15;
                bool _S690 = undistort_point_1(uv_34, &_S689, int(12), &uv_u_15);
                if(!_S690)
                {
                    float3  _S691 = make_float3 (0.0f);
                    _S683 = int(0);
                    _S682 = nullptr;
                    normal_7 = _S691;
                    break;
                }
                points_6[int(1)] = make_float3 (depths_4.y) * unproject_raydir_0(uv_u_15, camera_model_13, is_ray_depth_12);
                _S683 = int(2);
                _S682 = &points_6[int(1)];
                break;
            }
            if(_S683 != int(2))
            {
                _S681 = &points_6[int(0)];
                _S680 = nullptr;
                _S679 = nullptr;
                break;
            }
            float2  uv_35 = (pix_center_5 + make_float2 (0.0f, -1.0f) - _S684) / _S685;
            FixedArray<float, 4>  _S692 = dist_coeffs_15;
            float2  uv_u_16;
            bool _S693 = undistort_point_1(uv_35, &_S692, int(12), &uv_u_16);
            if(!_S693)
            {
                float3  _S694 = make_float3 (0.0f);
                _S683 = int(0);
                _S681 = &points_6[int(0)];
                _S680 = nullptr;
                _S679 = nullptr;
                normal_7 = _S694;
                break;
            }
            points_6[int(2)] = make_float3 (depths_4.z) * unproject_raydir_0(uv_u_16, camera_model_13, is_ray_depth_12);
            for(;;)
            {
                float2  uv_36 = (pix_center_5 + make_float2 (0.0f, 1.0f) - _S684) / _S685;
                FixedArray<float, 4>  _S695 = dist_coeffs_15;
                float2  uv_u_17;
                bool _S696 = undistort_point_1(uv_36, &_S695, int(12), &uv_u_17);
                if(!_S696)
                {
                    float3  _S697 = make_float3 (0.0f);
                    _S683 = int(0);
                    _S681 = nullptr;
                    normal_7 = _S697;
                    break;
                }
                points_6[int(3)] = make_float3 (depths_4.w) * unproject_raydir_0(uv_u_17, camera_model_13, is_ray_depth_12);
                _S683 = int(2);
                _S681 = &points_6[int(3)];
                break;
            }
            if(_S683 != int(2))
            {
                float3  * _S698 = _S681;
                _S681 = &points_6[int(0)];
                _S680 = _S698;
                _S679 = &points_6[int(2)];
                break;
            }
            float3  * _S699 = _S681;
            _S683 = int(1);
            _S681 = &points_6[int(0)];
            _S680 = _S699;
            _S679 = &points_6[int(2)];
            break;
        }
        if(_S683 != int(1))
        {
            break;
        }
        float3  normal_8 = cross_0(*_S682 - *_S681, - (*_S680 - *_S679));
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
    float2  _S700;
    bool _S701;
    float2  _S702;
    bool _S703;
    float2  _S704;
    bool _S705;
    float2  _S706;
    bool _S707;
};

inline __device__ void depth_to_normal_vjp_opencv(float2  pix_center_6, float4  intrins_12, FixedArray<float, 4>  dist_coeffs_16, int camera_model_14, bool is_ray_depth_13, float4  depths_5, float3  v_normal_2, float4  * v_depths_2)
{
    float2  _S708 = make_float2 (0.0f);
    s_bwd_prop_depth_to_normal_Intermediates_1 _S709;
    (&_S709)->_S700 = _S708;
    (&_S709)->_S701 = false;
    (&_S709)->_S702 = _S708;
    (&_S709)->_S703 = false;
    (&_S709)->_S704 = _S708;
    (&_S709)->_S705 = false;
    (&_S709)->_S706 = _S708;
    (&_S709)->_S707 = false;
    (&_S709)->_S700 = _S708;
    (&_S709)->_S701 = false;
    (&_S709)->_S702 = _S708;
    (&_S709)->_S703 = false;
    (&_S709)->_S704 = _S708;
    (&_S709)->_S705 = false;
    (&_S709)->_S706 = _S708;
    (&_S709)->_S707 = false;
    bool _S710 = (depths_5.x) == 0.0f;
    bool _runFlag_8;
    if(_S710)
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
    int _S711;
    if(!_runFlag_8)
    {
        float2  _S712 = float2 {intrins_12.z, intrins_12.w};
        float2  _S713 = float2 {intrins_12.x, intrins_12.y};
        float2  uv_37 = (pix_center_6 + make_float2 (-1.0f, -0.0f) - _S712) / _S713;
        float2  _S714 = _S708;
        FixedArray<float, 4>  _S715 = dist_coeffs_16;
        bool _S716 = undistort_point_1(uv_37, &_S715, int(12), &_S714);
        (&_S709)->_S700 = _S714;
        (&_S709)->_S701 = _S716;
        bool _S717 = !!_S716;
        if(_S717)
        {
            float2  uv_38 = (pix_center_6 + make_float2 (1.0f, -0.0f) - _S712) / _S713;
            float2  _S718 = _S708;
            FixedArray<float, 4>  _S719 = dist_coeffs_16;
            bool _S720 = undistort_point_1(uv_38, &_S719, int(12), &_S718);
            (&_S709)->_S702 = _S718;
            (&_S709)->_S703 = _S720;
            if(!!_S720)
            {
                _S711 = int(2);
            }
            else
            {
                _S711 = int(0);
            }
            if(_S711 != int(2))
            {
                _runFlag_8 = false;
            }
            else
            {
                _runFlag_8 = _S717;
            }
            if(_runFlag_8)
            {
                float2  uv_39 = (pix_center_6 + make_float2 (0.0f, -1.0f) - _S712) / _S713;
                float2  _S721 = _S708;
                FixedArray<float, 4>  _S722 = dist_coeffs_16;
                bool _S723 = undistort_point_1(uv_39, &_S722, int(12), &_S721);
                (&_S709)->_S704 = _S721;
                (&_S709)->_S705 = _S723;
                if(!_S723)
                {
                    _runFlag_8 = false;
                }
                if(_runFlag_8)
                {
                    float2  uv_40 = (pix_center_6 + make_float2 (0.0f, 1.0f) - _S712) / _S713;
                    float2  _S724 = _S708;
                    FixedArray<float, 4>  _S725 = dist_coeffs_16;
                    bool _S726 = undistort_point_1(uv_40, &_S725, int(12), &_S724);
                    (&_S709)->_S706 = _S724;
                    (&_S709)->_S707 = _S726;
                }
            }
        }
    }
    s_bwd_prop_depth_to_normal_Intermediates_1 _S727 = _S709;
    float3  _S728 = make_float3 (0.0f);
    if(_S710)
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
    bool _S729 = !_runFlag_8;
    bool _runFlag_9;
    bool _runFlag_10;
    bool _S730;
    bool _runFlag_11;
    bool _S731;
    bool _S732;
    FixedArray<float3 , 4>  points_7;
    float3  _S733;
    float3  _S734;
    float3  _S735;
    float3  _S736;
    float3  _S737;
    float3  _S738;
    float3  _S739;
    float3  _S740;
    float3  _S741;
    if(_S729)
    {
        bool _S742 = !!_S727._S701;
        if(_S742)
        {
            float3  _S743 = s_primal_ctx_unproject_raydir_0(_S727._S700, camera_model_14, is_ray_depth_13);
            float3  _S744 = make_float3 (depths_5.x) * _S743;
            bool _S745 = !!_S727._S703;
            if(_S745)
            {
                float3  _S746 = s_primal_ctx_unproject_raydir_0(_S727._S702, camera_model_14, is_ray_depth_13);
                float3  _S747 = make_float3 (depths_5.y) * _S746;
                _S711 = int(2);
                points_7[int(0)] = _S744;
                points_7[int(1)] = _S747;
                points_7[int(2)] = _S728;
                points_7[int(3)] = _S728;
                _S733 = _S746;
            }
            else
            {
                _S711 = int(0);
                points_7[int(0)] = _S744;
                points_7[int(1)] = _S728;
                points_7[int(2)] = _S728;
                points_7[int(3)] = _S728;
                _S733 = _S728;
            }
            if(_S711 != int(2))
            {
                _runFlag_8 = false;
            }
            else
            {
                _runFlag_8 = _S742;
                _S711 = int(0);
            }
            if(_runFlag_8)
            {
                if(!_S727._S705)
                {
                    _runFlag_9 = false;
                    _S711 = int(0);
                }
                else
                {
                    _runFlag_9 = _runFlag_8;
                }
                if(_runFlag_9)
                {
                    float3  _S748 = s_primal_ctx_unproject_raydir_0(_S727._S704, camera_model_14, is_ray_depth_13);
                    points_7[int(2)] = make_float3 (depths_5.z) * _S748;
                    bool _S749 = !!_S727._S707;
                    int _S750;
                    if(_S749)
                    {
                        float3  _S751 = s_primal_ctx_unproject_raydir_0(_S727._S706, camera_model_14, is_ray_depth_13);
                        points_7[int(3)] = make_float3 (depths_5.w) * _S751;
                        _S750 = int(2);
                        _S734 = _S751;
                    }
                    else
                    {
                        _S750 = int(0);
                        _S734 = _S728;
                    }
                    if(_S750 != int(2))
                    {
                        _runFlag_10 = false;
                        _S711 = _S750;
                    }
                    else
                    {
                        _runFlag_10 = _runFlag_9;
                    }
                    if(_runFlag_10)
                    {
                        _S711 = int(1);
                    }
                    _runFlag_10 = _S749;
                    _S735 = _S748;
                }
                else
                {
                    _runFlag_10 = false;
                    _S734 = _S728;
                    _S735 = _S728;
                }
            }
            else
            {
                _runFlag_9 = false;
                _runFlag_10 = false;
                _S734 = _S728;
                _S735 = _S728;
            }
            float3  _S752 = _S733;
            _S733 = _S734;
            _S734 = _S735;
            _S730 = _S745;
            _S735 = _S752;
            _S736 = _S743;
        }
        else
        {
            _S711 = int(0);
            points_7[int(0)] = _S728;
            points_7[int(1)] = _S728;
            points_7[int(2)] = _S728;
            points_7[int(3)] = _S728;
            _runFlag_8 = false;
            _runFlag_9 = false;
            _runFlag_10 = false;
            _S733 = _S728;
            _S734 = _S728;
            _S730 = false;
            _S735 = _S728;
            _S736 = _S728;
        }
        if(_S711 != int(1))
        {
            _runFlag_11 = false;
        }
        else
        {
            _runFlag_11 = _S729;
        }
        if(_runFlag_11)
        {
            float3  dx_3 = points_7[int(1)] - points_7[int(0)];
            float3  _S753 = - (points_7[int(3)] - points_7[int(2)]);
            float3  _S754 = s_primal_ctx_cross_0(dx_3, _S753);
            bool _S755 = (s_primal_ctx_dot_0(_S754, _S754)) != 0.0f;
            if(_S755)
            {
                float _S756 = length_0(_S754);
                float3  _S757 = make_float3 (_S756);
                _S737 = make_float3 (_S756 * _S756);
                _S738 = _S757;
            }
            else
            {
                _S737 = _S728;
                _S738 = _S728;
            }
            float3  _S758 = _S738;
            _S731 = _S755;
            _S738 = _S754;
            _S739 = _S758;
            _S740 = dx_3;
            _S741 = _S753;
        }
        else
        {
            _S731 = false;
            _S737 = _S728;
            _S738 = _S728;
            _S739 = _S728;
            _S740 = _S728;
            _S741 = _S728;
        }
        bool _S759 = _runFlag_8;
        bool _S760 = _runFlag_9;
        bool _S761 = _runFlag_10;
        float3  _S762 = _S733;
        float3  _S763 = _S734;
        bool _S764 = _S730;
        float3  _S765 = _S735;
        float3  _S766 = _S736;
        _runFlag_8 = _runFlag_11;
        _runFlag_9 = _S731;
        _S733 = _S737;
        _S734 = _S738;
        _S735 = _S739;
        _S736 = _S740;
        _S737 = _S741;
        _runFlag_10 = _S742;
        _S730 = _S759;
        _runFlag_11 = _S760;
        _S731 = _S761;
        _S738 = _S762;
        _S739 = _S763;
        _S732 = _S764;
        _S740 = _S765;
        _S741 = _S766;
    }
    else
    {
        _runFlag_8 = false;
        _runFlag_9 = false;
        _S733 = _S728;
        _S734 = _S728;
        _S735 = _S728;
        _S736 = _S728;
        _S737 = _S728;
        _runFlag_10 = false;
        _S730 = false;
        _runFlag_11 = false;
        _S731 = false;
        _S738 = _S728;
        _S739 = _S728;
        _S732 = false;
        _S740 = _S728;
        _S741 = _S728;
    }
    float4  _S767 = make_float4 (0.0f);
    float4  _S768;
    if(_S729)
    {
        if(_runFlag_8)
        {
            if(_runFlag_9)
            {
                float3  _S769 = v_normal_2 / _S733;
                float3  _S770 = _S734 * - _S769;
                float3  _S771 = _S735 * _S769;
                float _S772 = _S770.x + _S770.y + _S770.z;
                DiffPair_vectorx3Cfloatx2C3x3E_0 _S773;
                (&_S773)->primal_0 = _S734;
                (&_S773)->differential_0 = _S728;
                s_bwd_length_impl_0(&_S773, _S772);
                _S733 = _S771 + _S773.differential_0;
            }
            else
            {
                _S733 = v_normal_2;
            }
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S774;
            (&_S774)->primal_0 = _S734;
            (&_S774)->differential_0 = _S728;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S775;
            (&_S775)->primal_0 = _S734;
            (&_S775)->differential_0 = _S728;
            s_bwd_prop_dot_0(&_S774, &_S775, 0.0f);
            float3  _S776 = _S775.differential_0 + _S774.differential_0 + _S733;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S777;
            (&_S777)->primal_0 = _S736;
            (&_S777)->differential_0 = _S728;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S778;
            (&_S778)->primal_0 = _S737;
            (&_S778)->differential_0 = _S728;
            s_bwd_prop_cross_0(&_S777, &_S778, _S776);
            float3  s_diff_dy_T_3 = - _S778.differential_0;
            float3  _S779 = - s_diff_dy_T_3;
            float3  _S780 = - _S777.differential_0;
            FixedArray<float3 , 4>  _S781;
            _S781[int(0)] = _S728;
            _S781[int(1)] = _S728;
            _S781[int(2)] = _S728;
            _S781[int(3)] = _S728;
            _S781[int(2)] = _S779;
            _S781[int(3)] = s_diff_dy_T_3;
            _S781[int(0)] = _S780;
            _S781[int(1)] = _S777.differential_0;
            points_7[int(0)] = _S781[int(0)];
            points_7[int(1)] = _S781[int(1)];
            points_7[int(2)] = _S781[int(2)];
            points_7[int(3)] = _S781[int(3)];
        }
        else
        {
            points_7[int(0)] = _S728;
            points_7[int(1)] = _S728;
            points_7[int(2)] = _S728;
            points_7[int(3)] = _S728;
        }
        if(_runFlag_10)
        {
            if(_S730)
            {
                if(_runFlag_11)
                {
                    FixedArray<float3 , 4>  _S782 = points_7;
                    FixedArray<float3 , 4>  _S783 = points_7;
                    FixedArray<float3 , 4>  _S784 = points_7;
                    FixedArray<float3 , 4>  _S785 = points_7;
                    if(_S731)
                    {
                        float3  _S786 = _S738 * _S785[int(3)];
                        float _S787 = _S786.x + _S786.y + _S786.z;
                        float4  _S788 = _S767;
                        *&((&_S788)->w) = _S787;
                        points_7[int(0)] = _S782[int(0)];
                        points_7[int(1)] = _S783[int(1)];
                        points_7[int(2)] = _S784[int(2)];
                        points_7[int(3)] = _S728;
                        _S768 = _S788;
                    }
                    else
                    {
                        points_7[int(0)] = _S782[int(0)];
                        points_7[int(1)] = _S783[int(1)];
                        points_7[int(2)] = _S784[int(2)];
                        points_7[int(3)] = _S785[int(3)];
                        _S768 = _S767;
                    }
                    float3  _S789 = _S739 * points_7[int(2)];
                    float _S790 = _S789.x + _S789.y + _S789.z;
                    FixedArray<float3 , 4>  _S791 = points_7;
                    FixedArray<float3 , 4>  _S792 = points_7;
                    float4  _S793 = _S767;
                    *&((&_S793)->z) = _S790;
                    float4  _S794 = _S768 + _S793;
                    points_7[int(0)] = points_7[int(0)];
                    points_7[int(1)] = _S791[int(1)];
                    points_7[int(2)] = _S728;
                    points_7[int(3)] = _S792[int(3)];
                    _S768 = _S794;
                }
                else
                {
                    FixedArray<float3 , 4>  _S795 = points_7;
                    FixedArray<float3 , 4>  _S796 = points_7;
                    FixedArray<float3 , 4>  _S797 = points_7;
                    points_7[int(0)] = points_7[int(0)];
                    points_7[int(1)] = _S795[int(1)];
                    points_7[int(2)] = _S796[int(2)];
                    points_7[int(3)] = _S797[int(3)];
                    _S768 = _S767;
                }
            }
            else
            {
                FixedArray<float3 , 4>  _S798 = points_7;
                FixedArray<float3 , 4>  _S799 = points_7;
                FixedArray<float3 , 4>  _S800 = points_7;
                points_7[int(0)] = points_7[int(0)];
                points_7[int(1)] = _S798[int(1)];
                points_7[int(2)] = _S799[int(2)];
                points_7[int(3)] = _S800[int(3)];
                _S768 = _S767;
            }
            if(_S732)
            {
                FixedArray<float3 , 4>  _S801 = points_7;
                float3  _S802 = _S740 * points_7[int(1)];
                float _S803 = _S802.x + _S802.y + _S802.z;
                float4  _S804 = _S767;
                *&((&_S804)->y) = _S803;
                float4  _S805 = _S768 + _S804;
                points_7[int(0)] = _S728;
                points_7[int(1)] = _S728;
                points_7[int(2)] = _S728;
                points_7[int(3)] = _S728;
                _S733 = _S801[int(0)];
                _S768 = _S805;
            }
            else
            {
                FixedArray<float3 , 4>  _S806 = points_7;
                FixedArray<float3 , 4>  _S807 = points_7;
                FixedArray<float3 , 4>  _S808 = points_7;
                points_7[int(0)] = points_7[int(0)];
                points_7[int(1)] = _S806[int(1)];
                points_7[int(2)] = _S807[int(2)];
                points_7[int(3)] = _S808[int(3)];
                _S733 = _S728;
            }
            float3  _S809 = _S741 * (points_7[int(0)] + _S733);
            float _S810 = _S809.x + _S809.y + _S809.z;
            float4  _S811 = _S767;
            *&((&_S811)->x) = _S810;
            _S768 = _S768 + _S811;
        }
        else
        {
            _S768 = _S767;
        }
    }
    else
    {
        _S768 = _S767;
    }
    *v_depths_2 = _S768;
    return;
}

inline __device__ float ray_depth_to_linear_depth_factor_opencv(float2  pix_center_7, float4  intrins_13, FixedArray<float, 4>  dist_coeffs_17, int camera_model_15)
{
    float _S812;
    for(;;)
    {
        float2  uv_41 = (pix_center_7 - float2 {intrins_13.z, intrins_13.w}) / float2 {intrins_13.x, intrins_13.y};
        FixedArray<float, 4>  _S813 = dist_coeffs_17;
        float2  uv_u_18;
        bool _S814 = undistort_point_1(uv_41, &_S813, int(12), &uv_u_18);
        if(!_S814)
        {
            _S812 = 0.0f;
            break;
        }
        float3  raydir_9 = unproject_raydir_0(uv_u_18, camera_model_15, false);
        _S812 = float((F32_sign((raydir_9.z)))) / length_0(raydir_9);
        break;
    }
    return _S812;
}

inline __device__ float depth_normal_loss_opencv(float2  pix_center_8, float4  intrins_14, FixedArray<float, 4>  dist_coeffs_18, int camera_model_16, bool is_ray_depth_14, float4  depths_6, float3  gt_normal_2)
{
    float _S815;
    for(;;)
    {
        float3  _S816;
        float3  * _S817;
        float3  * _S818;
        float3  * _S819;
        float3  * _S820;
        int _S821;
        FixedArray<float3 , 5>  points_8;
        for(;;)
        {
            float2  _S822 = float2 {intrins_14.z, intrins_14.w};
            float2  _S823 = float2 {intrins_14.x, intrins_14.y};
            float2  uv_42 = (pix_center_8 + make_float2 (-1.0f, -0.0f) - _S822) / _S823;
            FixedArray<float, 4>  _S824 = dist_coeffs_18;
            float2  uv_u_19;
            bool _S825 = undistort_point_1(uv_42, &_S824, int(12), &uv_u_19);
            float3  _S826 = make_float3 (0.0f);
            if(!_S825)
            {
                _S821 = int(0);
                _S820 = nullptr;
                _S819 = nullptr;
                _S818 = nullptr;
                _S817 = nullptr;
                _S816 = _S826;
                break;
            }
            float3  raydir_10 = unproject_raydir_0(uv_u_19, camera_model_16, is_ray_depth_14);
            points_8[int(0)] = make_float3 (depths_6.x) * raydir_10;
            float2  uv_43 = (pix_center_8 + make_float2 (1.0f, -0.0f) - _S822) / _S823;
            FixedArray<float, 4>  _S827 = dist_coeffs_18;
            float2  uv_u_20;
            bool _S828 = undistort_point_1(uv_43, &_S827, int(12), &uv_u_20);
            if(!_S828)
            {
                _S821 = int(0);
                _S820 = nullptr;
                _S819 = &points_8[int(0)];
                _S818 = nullptr;
                _S817 = nullptr;
                _S816 = _S826;
                break;
            }
            float3  raydir_11 = unproject_raydir_0(uv_u_20, camera_model_16, is_ray_depth_14);
            points_8[int(1)] = make_float3 (depths_6.y) * raydir_11;
            float2  uv_44 = (pix_center_8 + make_float2 (0.0f, -1.0f) - _S822) / _S823;
            FixedArray<float, 4>  _S829 = dist_coeffs_18;
            float2  uv_u_21;
            bool _S830 = undistort_point_1(uv_44, &_S829, int(12), &uv_u_21);
            if(!_S830)
            {
                _S821 = int(0);
                _S820 = &points_8[int(1)];
                _S819 = &points_8[int(0)];
                _S818 = nullptr;
                _S817 = nullptr;
                _S816 = _S826;
                break;
            }
            float3  raydir_12 = unproject_raydir_0(uv_u_21, camera_model_16, is_ray_depth_14);
            points_8[int(2)] = make_float3 (depths_6.z) * raydir_12;
            float2  uv_45 = (pix_center_8 + make_float2 (0.0f, 1.0f) - _S822) / _S823;
            FixedArray<float, 4>  _S831 = dist_coeffs_18;
            float2  uv_u_22;
            bool _S832 = undistort_point_1(uv_45, &_S831, int(12), &uv_u_22);
            if(!_S832)
            {
                _S821 = int(0);
                _S820 = &points_8[int(1)];
                _S819 = &points_8[int(0)];
                _S818 = nullptr;
                _S817 = &points_8[int(2)];
                _S816 = _S826;
                break;
            }
            float3  raydir_13 = unproject_raydir_0(uv_u_22, camera_model_16, is_ray_depth_14);
            points_8[int(3)] = make_float3 (depths_6.w) * raydir_13;
            float2  uv_46 = (pix_center_8 + make_float2 (0.0f) * make_float2 (0.0f, 3.0f) - _S822) / _S823;
            FixedArray<float, 4>  _S833 = dist_coeffs_18;
            float2  uv_u_23;
            bool _S834 = undistort_point_1(uv_46, &_S833, int(12), &uv_u_23);
            if(!_S834)
            {
                _S821 = int(0);
                _S820 = &points_8[int(1)];
                _S819 = &points_8[int(0)];
                _S818 = &points_8[int(3)];
                _S817 = &points_8[int(2)];
                _S816 = _S826;
                break;
            }
            float3  raydir_14 = unproject_raydir_0(uv_u_23, camera_model_16, is_ray_depth_14);
            _S821 = int(1);
            _S820 = &points_8[int(1)];
            _S819 = &points_8[int(0)];
            _S818 = &points_8[int(3)];
            _S817 = &points_8[int(2)];
            _S816 = raydir_14;
            break;
        }
        if(_S821 != int(1))
        {
            _S815 = 0.0f;
            break;
        }
        float3  normal_9 = cross_0(*_S820 - *_S819, - (*_S818 - *_S817));
        float3  normal_10;
        if((dot_0(normal_9, normal_9)) != 0.0f)
        {
            normal_10 = normalize_0(normal_9);
        }
        else
        {
            normal_10 = normal_9;
        }
        float3  _S835;
        if((dot_0(gt_normal_2, gt_normal_2)) != 0.0f)
        {
            _S835 = normalize_0(gt_normal_2);
        }
        else
        {
            _S835 = gt_normal_2;
        }
        _S815 = (1.0f - dot_0(normal_10, _S835) + 0.00100000004749745f) / ((F32_max((dot_0(normal_10, - normalize_0(_S816))), (0.0f))) + 0.00100000004749745f);
        break;
    }
    return _S815;
}

struct s_bwd_prop_depth_normal_loss_Intermediates_1
{
    float2  _S836;
    bool _S837;
    float2  _S838;
    bool _S839;
    float2  _S840;
    bool _S841;
    float2  _S842;
    bool _S843;
    float2  _S844;
    bool _S845;
};

inline __device__ void depth_normal_loss_vjp_opencv(float2  pix_center_9, float4  intrins_15, FixedArray<float, 4>  dist_coeffs_19, int camera_model_17, bool is_ray_depth_15, float4  depths_7, float3  gt_normal_3, float v_loss_1, float4  * v_depths_3, float3  * v_gt_normal_1)
{
    float2  _S846 = make_float2 (0.0f);
    s_bwd_prop_depth_normal_loss_Intermediates_1 _S847;
    (&_S847)->_S836 = _S846;
    (&_S847)->_S837 = false;
    (&_S847)->_S838 = _S846;
    (&_S847)->_S839 = false;
    (&_S847)->_S840 = _S846;
    (&_S847)->_S841 = false;
    (&_S847)->_S842 = _S846;
    (&_S847)->_S843 = false;
    (&_S847)->_S844 = _S846;
    (&_S847)->_S845 = false;
    (&_S847)->_S838 = _S846;
    (&_S847)->_S839 = false;
    (&_S847)->_S840 = _S846;
    (&_S847)->_S841 = false;
    (&_S847)->_S842 = _S846;
    (&_S847)->_S843 = false;
    (&_S847)->_S844 = _S846;
    (&_S847)->_S845 = false;
    float2  _S848 = float2 {intrins_15.z, intrins_15.w};
    float2  _S849 = float2 {intrins_15.x, intrins_15.y};
    float2  uv_47 = (pix_center_9 + make_float2 (-1.0f, -0.0f) - _S848) / _S849;
    float2  _S850 = _S846;
    FixedArray<float, 4>  _S851 = dist_coeffs_19;
    bool _S852 = undistort_point_1(uv_47, &_S851, int(12), &_S850);
    (&_S847)->_S836 = _S850;
    (&_S847)->_S837 = _S852;
    bool _S853 = !!_S852;
    bool _runFlag_12;
    if(_S853)
    {
        float2  uv_48 = (pix_center_9 + make_float2 (1.0f, -0.0f) - _S848) / _S849;
        float2  _S854 = _S846;
        FixedArray<float, 4>  _S855 = dist_coeffs_19;
        bool _S856 = undistort_point_1(uv_48, &_S855, int(12), &_S854);
        (&_S847)->_S838 = _S854;
        (&_S847)->_S839 = _S856;
        if(!_S856)
        {
            _runFlag_12 = false;
        }
        else
        {
            _runFlag_12 = _S853;
        }
        if(_runFlag_12)
        {
            float2  uv_49 = (pix_center_9 + make_float2 (0.0f, -1.0f) - _S848) / _S849;
            float2  _S857 = _S846;
            FixedArray<float, 4>  _S858 = dist_coeffs_19;
            bool _S859 = undistort_point_1(uv_49, &_S858, int(12), &_S857);
            (&_S847)->_S840 = _S857;
            (&_S847)->_S841 = _S859;
            if(!_S859)
            {
                _runFlag_12 = false;
            }
            if(_runFlag_12)
            {
                float2  uv_50 = (pix_center_9 + make_float2 (0.0f, 1.0f) - _S848) / _S849;
                float2  _S860 = _S846;
                FixedArray<float, 4>  _S861 = dist_coeffs_19;
                bool _S862 = undistort_point_1(uv_50, &_S861, int(12), &_S860);
                (&_S847)->_S842 = _S860;
                (&_S847)->_S843 = _S862;
                if(!_S862)
                {
                    _runFlag_12 = false;
                }
                if(_runFlag_12)
                {
                    float2  uv_51 = (pix_center_9 - _S848) / _S849;
                    float2  _S863 = _S846;
                    FixedArray<float, 4>  _S864 = dist_coeffs_19;
                    bool _S865 = undistort_point_1(uv_51, &_S864, int(12), &_S863);
                    (&_S847)->_S844 = _S863;
                    (&_S847)->_S845 = _S865;
                }
            }
        }
    }
    s_bwd_prop_depth_normal_loss_Intermediates_1 _S866 = _S847;
    float3  _S867 = make_float3 (0.0f);
    bool _S868 = !!_S847._S837;
    bool _runFlag_13;
    bool _runFlag_14;
    bool _runFlag_15;
    int _S869;
    float3  raydir_15;
    float3  _S870;
    float3  _S871;
    float3  _S872;
    float3  _S873;
    FixedArray<float3 , 5>  points_9;
    if(_S868)
    {
        float3  _S874 = s_primal_ctx_unproject_raydir_0(_S866._S836, camera_model_17, is_ray_depth_15);
        float3  _S875 = make_float3 (depths_7.x) * _S874;
        if(!_S866._S839)
        {
            _runFlag_12 = false;
        }
        else
        {
            _runFlag_12 = _S868;
        }
        if(_runFlag_12)
        {
            float3  _S876 = s_primal_ctx_unproject_raydir_0(_S866._S838, camera_model_17, is_ray_depth_15);
            float3  _S877 = make_float3 (depths_7.y) * _S876;
            if(!_S866._S841)
            {
                _runFlag_13 = false;
            }
            else
            {
                _runFlag_13 = _runFlag_12;
            }
            if(_runFlag_13)
            {
                float3  _S878 = s_primal_ctx_unproject_raydir_0(_S866._S840, camera_model_17, is_ray_depth_15);
                float3  _S879 = make_float3 (depths_7.z) * _S878;
                if(!_S866._S843)
                {
                    _runFlag_14 = false;
                }
                else
                {
                    _runFlag_14 = _runFlag_13;
                }
                if(_runFlag_14)
                {
                    float3  _S880 = s_primal_ctx_unproject_raydir_0(_S866._S842, camera_model_17, is_ray_depth_15);
                    float3  _S881 = make_float3 (depths_7.w) * _S880;
                    if(!_S866._S845)
                    {
                        _runFlag_15 = false;
                    }
                    else
                    {
                        _runFlag_15 = _runFlag_14;
                    }
                    if(_runFlag_15)
                    {
                        float3  _S882 = s_primal_ctx_unproject_raydir_0(_S866._S844, camera_model_17, is_ray_depth_15);
                        _S869 = int(1);
                        raydir_15 = _S882;
                    }
                    else
                    {
                        _S869 = int(0);
                        raydir_15 = _S880;
                    }
                    points_9[int(0)] = _S875;
                    points_9[int(1)] = _S877;
                    points_9[int(2)] = _S879;
                    points_9[int(3)] = _S881;
                    points_9[int(4)] = _S867;
                    _S870 = _S880;
                }
                else
                {
                    _S869 = int(0);
                    raydir_15 = _S878;
                    points_9[int(0)] = _S875;
                    points_9[int(1)] = _S877;
                    points_9[int(2)] = _S879;
                    points_9[int(3)] = _S867;
                    points_9[int(4)] = _S867;
                    _S870 = _S867;
                }
                _S871 = _S878;
            }
            else
            {
                _S869 = int(0);
                raydir_15 = _S876;
                points_9[int(0)] = _S875;
                points_9[int(1)] = _S877;
                points_9[int(2)] = _S867;
                points_9[int(3)] = _S867;
                points_9[int(4)] = _S867;
                _runFlag_14 = false;
                _S870 = _S867;
                _S871 = _S867;
            }
            _S872 = _S876;
        }
        else
        {
            _S869 = int(0);
            raydir_15 = _S874;
            points_9[int(0)] = _S875;
            points_9[int(1)] = _S867;
            points_9[int(2)] = _S867;
            points_9[int(3)] = _S867;
            points_9[int(4)] = _S867;
            _runFlag_13 = false;
            _runFlag_14 = false;
            _S870 = _S867;
            _S871 = _S867;
            _S872 = _S867;
        }
        _S873 = _S874;
    }
    else
    {
        _S869 = int(0);
        points_9[int(0)] = _S867;
        points_9[int(1)] = _S867;
        points_9[int(2)] = _S867;
        points_9[int(3)] = _S867;
        points_9[int(4)] = _S867;
        _runFlag_12 = false;
        _runFlag_13 = false;
        _runFlag_14 = false;
        _S870 = _S867;
        _S871 = _S867;
        _S872 = _S867;
        _S873 = _S867;
    }
    bool _S883 = !(_S869 != int(1));
    bool _S884;
    float3  normal_11;
    float3  _S885;
    float3  _S886;
    float3  _S887;
    float3  _S888;
    float _S889;
    float _S890;
    float _S891;
    float _S892;
    if(_S883)
    {
        float3  dx_4 = points_9[int(1)] - points_9[int(0)];
        float3  _S893 = - (points_9[int(3)] - points_9[int(2)]);
        float3  _S894 = s_primal_ctx_cross_0(dx_4, _S893);
        bool _S895 = (s_primal_ctx_dot_0(_S894, _S894)) != 0.0f;
        if(_S895)
        {
            normal_11 = normalize_0(_S894);
        }
        else
        {
            normal_11 = _S894;
        }
        bool _S896 = (s_primal_ctx_dot_0(gt_normal_3, gt_normal_3)) != 0.0f;
        if(_S896)
        {
            _S885 = normalize_0(gt_normal_3);
        }
        else
        {
            _S885 = gt_normal_3;
        }
        float3  _S897 = - normalize_0(raydir_15);
        float _S898 = s_primal_ctx_dot_0(normal_11, _S897);
        float _S899 = 1.0f - s_primal_ctx_dot_0(normal_11, _S885) + 0.00100000004749745f;
        float _S900 = (F32_max((_S898), (0.0f))) + 0.00100000004749745f;
        _S889 = _S900 * _S900;
        _S890 = _S899;
        _S891 = _S900;
        _S892 = _S898;
        raydir_15 = normal_11;
        normal_11 = _S897;
        _runFlag_15 = _S896;
        _S884 = _S895;
        _S886 = _S894;
        _S887 = dx_4;
        _S888 = _S893;
    }
    else
    {
        _S889 = 0.0f;
        _S890 = 0.0f;
        _S891 = 0.0f;
        _S892 = 0.0f;
        raydir_15 = _S867;
        normal_11 = _S867;
        _S885 = _S867;
        _runFlag_15 = false;
        _S884 = false;
        _S886 = _S867;
        _S887 = _S867;
        _S888 = _S867;
    }
    float4  _S901 = make_float4 (0.0f);
    if(_S883)
    {
        float _S902 = v_loss_1 / _S889;
        float _S903 = _S890 * - _S902;
        float s_diff_num_T_1 = _S891 * _S902;
        DiffPair_float_0 _S904;
        (&_S904)->primal_0 = _S892;
        (&_S904)->differential_0 = 0.0f;
        DiffPair_float_0 _S905;
        (&_S905)->primal_0 = 0.0f;
        (&_S905)->differential_0 = 0.0f;
        _d_max_0(&_S904, &_S905, _S903);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S906;
        (&_S906)->primal_0 = raydir_15;
        (&_S906)->differential_0 = _S867;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S907;
        (&_S907)->primal_0 = normal_11;
        (&_S907)->differential_0 = _S867;
        s_bwd_prop_dot_0(&_S906, &_S907, _S904.differential_0);
        float _S908 = - s_diff_num_T_1;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S909;
        (&_S909)->primal_0 = raydir_15;
        (&_S909)->differential_0 = _S867;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S910;
        (&_S910)->primal_0 = _S885;
        (&_S910)->differential_0 = _S867;
        s_bwd_prop_dot_0(&_S909, &_S910, _S908);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S911 = _S910;
        float3  _S912 = _S906.differential_0 + _S909.differential_0;
        if(_runFlag_15)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S913;
            (&_S913)->primal_0 = gt_normal_3;
            (&_S913)->differential_0 = _S867;
            s_bwd_normalize_impl_0(&_S913, _S911.differential_0);
            raydir_15 = _S913.differential_0;
        }
        else
        {
            raydir_15 = _S911.differential_0;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S914;
        (&_S914)->primal_0 = gt_normal_3;
        (&_S914)->differential_0 = _S867;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S915;
        (&_S915)->primal_0 = gt_normal_3;
        (&_S915)->differential_0 = _S867;
        s_bwd_prop_dot_0(&_S914, &_S915, 0.0f);
        float3  _S916 = _S915.differential_0 + _S914.differential_0 + raydir_15;
        if(_S884)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S917;
            (&_S917)->primal_0 = _S886;
            (&_S917)->differential_0 = _S867;
            s_bwd_normalize_impl_0(&_S917, _S912);
            raydir_15 = _S917.differential_0;
        }
        else
        {
            raydir_15 = _S912;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S918;
        (&_S918)->primal_0 = _S886;
        (&_S918)->differential_0 = _S867;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S919;
        (&_S919)->primal_0 = _S886;
        (&_S919)->differential_0 = _S867;
        s_bwd_prop_dot_0(&_S918, &_S919, 0.0f);
        float3  _S920 = _S919.differential_0 + _S918.differential_0 + raydir_15;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S921;
        (&_S921)->primal_0 = _S887;
        (&_S921)->differential_0 = _S867;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S922;
        (&_S922)->primal_0 = _S888;
        (&_S922)->differential_0 = _S867;
        s_bwd_prop_cross_0(&_S921, &_S922, _S920);
        float3  s_diff_dy_T_4 = - _S922.differential_0;
        float3  _S923 = - s_diff_dy_T_4;
        float3  _S924 = - _S921.differential_0;
        FixedArray<float3 , 5>  _S925;
        _S925[int(0)] = _S867;
        _S925[int(1)] = _S867;
        _S925[int(2)] = _S867;
        _S925[int(3)] = _S867;
        _S925[int(4)] = _S867;
        _S925[int(2)] = _S923;
        _S925[int(3)] = s_diff_dy_T_4;
        _S925[int(0)] = _S924;
        _S925[int(1)] = _S921.differential_0;
        points_9[int(0)] = _S925[int(0)];
        points_9[int(1)] = _S925[int(1)];
        points_9[int(2)] = _S925[int(2)];
        points_9[int(3)] = _S925[int(3)];
        points_9[int(4)] = _S925[int(4)];
        raydir_15 = _S916;
    }
    else
    {
        points_9[int(0)] = _S867;
        points_9[int(1)] = _S867;
        points_9[int(2)] = _S867;
        points_9[int(3)] = _S867;
        points_9[int(4)] = _S867;
        raydir_15 = _S867;
    }
    float4  _S926;
    if(_S868)
    {
        if(_runFlag_12)
        {
            if(_runFlag_13)
            {
                if(_runFlag_14)
                {
                    FixedArray<float3 , 5>  _S927 = points_9;
                    FixedArray<float3 , 5>  _S928 = points_9;
                    FixedArray<float3 , 5>  _S929 = points_9;
                    float3  _S930 = _S870 * points_9[int(3)];
                    float _S931 = _S930.x + _S930.y + _S930.z;
                    float4  _S932 = _S901;
                    *&((&_S932)->w) = _S931;
                    points_9[int(0)] = _S867;
                    points_9[int(1)] = _S867;
                    points_9[int(2)] = _S867;
                    points_9[int(3)] = _S867;
                    points_9[int(4)] = _S867;
                    _S870 = _S929[int(2)];
                    normal_11 = _S927[int(0)];
                    _S885 = _S928[int(1)];
                    _S926 = _S932;
                }
                else
                {
                    FixedArray<float3 , 5>  _S933 = points_9;
                    FixedArray<float3 , 5>  _S934 = points_9;
                    FixedArray<float3 , 5>  _S935 = points_9;
                    FixedArray<float3 , 5>  _S936 = points_9;
                    points_9[int(0)] = points_9[int(0)];
                    points_9[int(1)] = _S933[int(1)];
                    points_9[int(2)] = _S934[int(2)];
                    points_9[int(3)] = _S935[int(3)];
                    points_9[int(4)] = _S936[int(4)];
                    _S870 = _S867;
                    normal_11 = _S867;
                    _S885 = _S867;
                    _S926 = _S901;
                }
                float3  _S937 = _S871 * (points_9[int(2)] + _S870);
                float _S938 = _S937.x + _S937.y + _S937.z;
                float3  _S939 = points_9[int(0)] + normal_11;
                float3  _S940 = points_9[int(1)] + _S885;
                float4  _S941 = _S901;
                *&((&_S941)->z) = _S938;
                float4  _S942 = _S926 + _S941;
                points_9[int(0)] = _S867;
                points_9[int(1)] = _S867;
                points_9[int(2)] = _S867;
                points_9[int(3)] = _S867;
                points_9[int(4)] = _S867;
                _S870 = _S940;
                _S871 = _S939;
                _S926 = _S942;
            }
            else
            {
                FixedArray<float3 , 5>  _S943 = points_9;
                FixedArray<float3 , 5>  _S944 = points_9;
                FixedArray<float3 , 5>  _S945 = points_9;
                FixedArray<float3 , 5>  _S946 = points_9;
                points_9[int(0)] = points_9[int(0)];
                points_9[int(1)] = _S943[int(1)];
                points_9[int(2)] = _S944[int(2)];
                points_9[int(3)] = _S945[int(3)];
                points_9[int(4)] = _S946[int(4)];
                _S870 = _S867;
                _S871 = _S867;
                _S926 = _S901;
            }
            float3  _S947 = _S872 * (points_9[int(1)] + _S870);
            float _S948 = _S947.x + _S947.y + _S947.z;
            float3  _S949 = points_9[int(0)] + _S871;
            float4  _S950 = _S901;
            *&((&_S950)->y) = _S948;
            float4  _S951 = _S926 + _S950;
            points_9[int(0)] = _S867;
            points_9[int(1)] = _S867;
            points_9[int(2)] = _S867;
            points_9[int(3)] = _S867;
            points_9[int(4)] = _S867;
            _S870 = _S949;
            _S926 = _S951;
        }
        else
        {
            FixedArray<float3 , 5>  _S952 = points_9;
            FixedArray<float3 , 5>  _S953 = points_9;
            FixedArray<float3 , 5>  _S954 = points_9;
            FixedArray<float3 , 5>  _S955 = points_9;
            points_9[int(0)] = points_9[int(0)];
            points_9[int(1)] = _S952[int(1)];
            points_9[int(2)] = _S953[int(2)];
            points_9[int(3)] = _S954[int(3)];
            points_9[int(4)] = _S955[int(4)];
            _S870 = _S867;
            _S926 = _S901;
        }
        float3  _S956 = _S873 * (points_9[int(0)] + _S870);
        float _S957 = _S956.x + _S956.y + _S956.z;
        float4  _S958 = _S901;
        *&((&_S958)->x) = _S957;
        _S926 = _S926 + _S958;
    }
    else
    {
        _S926 = _S901;
    }
    *v_depths_3 = _S926;
    *v_gt_normal_1 = raydir_15;
    return;
}

inline __device__ float3  generate_ray_d2n_prism(float2  pix_pos_6, float4  intrins_16, FixedArray<float, 8>  dist_coeffs_20, int camera_model_18, bool is_ray_depth_16)
{
    float3  _S959;
    for(;;)
    {
        float2  uv_52 = (pix_pos_6 - float2 {intrins_16.z, intrins_16.w}) / float2 {intrins_16.x, intrins_16.y};
        FixedArray<float, 8>  _S960 = dist_coeffs_20;
        float2  uv_u_24;
        bool _S961 = undistort_point_2(uv_52, &_S960, int(12), &uv_u_24);
        if(!_S961)
        {
            int3  _S962 = make_int3 (int(0));
            float3  _S963 = make_float3 ((float)_S962.x, (float)_S962.y, (float)_S962.z);
            _S959 = _S963;
            break;
        }
        _S959 = unproject_raydir_0(uv_u_24, camera_model_18, is_ray_depth_16);
        break;
    }
    return _S959;
}

inline __device__ float3  depth_to_point_prism(float2  pix_pos_7, float4  intrins_17, FixedArray<float, 8>  dist_coeffs_21, int camera_model_19, bool is_ray_depth_17, float depth_6)
{
    float3  _S964;
    for(;;)
    {
        float2  uv_53 = (pix_pos_7 - float2 {intrins_17.z, intrins_17.w}) / float2 {intrins_17.x, intrins_17.y};
        FixedArray<float, 8>  _S965 = dist_coeffs_21;
        float2  uv_u_25;
        bool _S966 = undistort_point_2(uv_53, &_S965, int(12), &uv_u_25);
        if(!_S966)
        {
            _S964 = make_float3 (0.0f);
            break;
        }
        _S964 = make_float3 (depth_6) * unproject_raydir_0(uv_u_25, camera_model_19, is_ray_depth_17);
        break;
    }
    return _S964;
}

struct s_bwd_prop_depth_to_point_Intermediates_2
{
    float2  _S967;
    bool _S968;
};

inline __device__ float depth_to_point_vjp_prism(float2  pix_pos_8, float4  intrins_18, FixedArray<float, 8>  dist_coeffs_22, int camera_model_20, bool is_ray_depth_18, float depth_7, float3  v_point_2)
{
    float2  _S969 = make_float2 (0.0f);
    s_bwd_prop_depth_to_point_Intermediates_2 _S970;
    (&_S970)->_S967 = _S969;
    (&_S970)->_S968 = false;
    float2  uv_54 = (pix_pos_8 - float2 {intrins_18.z, intrins_18.w}) / float2 {intrins_18.x, intrins_18.y};
    float2  _S971 = _S969;
    FixedArray<float, 8>  _S972 = dist_coeffs_22;
    bool _S973 = undistort_point_2(uv_54, &_S972, int(12), &_S971);
    (&_S970)->_S967 = _S971;
    (&_S970)->_S968 = _S973;
    s_bwd_prop_depth_to_point_Intermediates_2 _S974 = _S970;
    float3  _S975 = make_float3 (0.0f);
    bool _S976 = !!_S970._S968;
    float3  _S977;
    if(_S976)
    {
        _S977 = s_primal_ctx_unproject_raydir_0(_S974._S967, camera_model_20, is_ray_depth_18);
    }
    else
    {
        _S977 = _S975;
    }
    if(_S976)
    {
        _S977 = _S977 * v_point_2;
    }
    else
    {
        _S977 = _S975;
    }
    return _S977.x + _S977.y + _S977.z;
}

inline __device__ float3  depth_to_normal_prism(float2  pix_center_10, float4  intrins_19, FixedArray<float, 8>  dist_coeffs_23, int camera_model_21, bool is_ray_depth_19, float4  depths_8)
{
    float3  normal_12;
    for(;;)
    {
        bool _S978;
        if((depths_8.x) == 0.0f)
        {
            _S978 = true;
        }
        else
        {
            _S978 = (depths_8.y) == 0.0f;
        }
        if(_S978)
        {
            _S978 = true;
        }
        else
        {
            _S978 = (depths_8.z) == 0.0f;
        }
        if(_S978)
        {
            _S978 = true;
        }
        else
        {
            _S978 = (depths_8.w) == 0.0f;
        }
        if(_S978)
        {
            normal_12 = make_float3 (0.0f);
            break;
        }
        float3  * _S979;
        float3  * _S980;
        float3  * _S981;
        float3  * _S982;
        int _S983;
        FixedArray<float3 , 4>  points_10;
        for(;;)
        {
            float2  _S984 = float2 {intrins_19.z, intrins_19.w};
            float2  _S985 = float2 {intrins_19.x, intrins_19.y};
            float2  uv_55 = (pix_center_10 + make_float2 (-1.0f, -0.0f) - _S984) / _S985;
            FixedArray<float, 8>  _S986 = dist_coeffs_23;
            float2  uv_u_26;
            bool _S987 = undistort_point_2(uv_55, &_S986, int(12), &uv_u_26);
            if(!_S987)
            {
                float3  _S988 = make_float3 (0.0f);
                _S983 = int(0);
                _S982 = nullptr;
                _S981 = nullptr;
                _S980 = nullptr;
                _S979 = nullptr;
                normal_12 = _S988;
                break;
            }
            points_10[int(0)] = make_float3 (depths_8.x) * unproject_raydir_0(uv_u_26, camera_model_21, is_ray_depth_19);
            for(;;)
            {
                float2  uv_56 = (pix_center_10 + make_float2 (1.0f, -0.0f) - _S984) / _S985;
                FixedArray<float, 8>  _S989 = dist_coeffs_23;
                float2  uv_u_27;
                bool _S990 = undistort_point_2(uv_56, &_S989, int(12), &uv_u_27);
                if(!_S990)
                {
                    float3  _S991 = make_float3 (0.0f);
                    _S983 = int(0);
                    _S982 = nullptr;
                    normal_12 = _S991;
                    break;
                }
                points_10[int(1)] = make_float3 (depths_8.y) * unproject_raydir_0(uv_u_27, camera_model_21, is_ray_depth_19);
                _S983 = int(2);
                _S982 = &points_10[int(1)];
                break;
            }
            if(_S983 != int(2))
            {
                _S981 = &points_10[int(0)];
                _S980 = nullptr;
                _S979 = nullptr;
                break;
            }
            float2  uv_57 = (pix_center_10 + make_float2 (0.0f, -1.0f) - _S984) / _S985;
            FixedArray<float, 8>  _S992 = dist_coeffs_23;
            float2  uv_u_28;
            bool _S993 = undistort_point_2(uv_57, &_S992, int(12), &uv_u_28);
            if(!_S993)
            {
                float3  _S994 = make_float3 (0.0f);
                _S983 = int(0);
                _S981 = &points_10[int(0)];
                _S980 = nullptr;
                _S979 = nullptr;
                normal_12 = _S994;
                break;
            }
            points_10[int(2)] = make_float3 (depths_8.z) * unproject_raydir_0(uv_u_28, camera_model_21, is_ray_depth_19);
            for(;;)
            {
                float2  uv_58 = (pix_center_10 + make_float2 (0.0f, 1.0f) - _S984) / _S985;
                FixedArray<float, 8>  _S995 = dist_coeffs_23;
                float2  uv_u_29;
                bool _S996 = undistort_point_2(uv_58, &_S995, int(12), &uv_u_29);
                if(!_S996)
                {
                    float3  _S997 = make_float3 (0.0f);
                    _S983 = int(0);
                    _S981 = nullptr;
                    normal_12 = _S997;
                    break;
                }
                points_10[int(3)] = make_float3 (depths_8.w) * unproject_raydir_0(uv_u_29, camera_model_21, is_ray_depth_19);
                _S983 = int(2);
                _S981 = &points_10[int(3)];
                break;
            }
            if(_S983 != int(2))
            {
                float3  * _S998 = _S981;
                _S981 = &points_10[int(0)];
                _S980 = _S998;
                _S979 = &points_10[int(2)];
                break;
            }
            float3  * _S999 = _S981;
            _S983 = int(1);
            _S981 = &points_10[int(0)];
            _S980 = _S999;
            _S979 = &points_10[int(2)];
            break;
        }
        if(_S983 != int(1))
        {
            break;
        }
        float3  normal_13 = cross_0(*_S982 - *_S981, - (*_S980 - *_S979));
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
    float2  _S1000;
    bool _S1001;
    float2  _S1002;
    bool _S1003;
    float2  _S1004;
    bool _S1005;
    float2  _S1006;
    bool _S1007;
};

inline __device__ void depth_to_normal_vjp_prism(float2  pix_center_11, float4  intrins_20, FixedArray<float, 8>  dist_coeffs_24, int camera_model_22, bool is_ray_depth_20, float4  depths_9, float3  v_normal_3, float4  * v_depths_4)
{
    float2  _S1008 = make_float2 (0.0f);
    s_bwd_prop_depth_to_normal_Intermediates_2 _S1009;
    (&_S1009)->_S1000 = _S1008;
    (&_S1009)->_S1001 = false;
    (&_S1009)->_S1002 = _S1008;
    (&_S1009)->_S1003 = false;
    (&_S1009)->_S1004 = _S1008;
    (&_S1009)->_S1005 = false;
    (&_S1009)->_S1006 = _S1008;
    (&_S1009)->_S1007 = false;
    (&_S1009)->_S1000 = _S1008;
    (&_S1009)->_S1001 = false;
    (&_S1009)->_S1002 = _S1008;
    (&_S1009)->_S1003 = false;
    (&_S1009)->_S1004 = _S1008;
    (&_S1009)->_S1005 = false;
    (&_S1009)->_S1006 = _S1008;
    (&_S1009)->_S1007 = false;
    bool _S1010 = (depths_9.x) == 0.0f;
    bool _runFlag_16;
    if(_S1010)
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
    int _S1011;
    if(!_runFlag_16)
    {
        float2  _S1012 = float2 {intrins_20.z, intrins_20.w};
        float2  _S1013 = float2 {intrins_20.x, intrins_20.y};
        float2  uv_59 = (pix_center_11 + make_float2 (-1.0f, -0.0f) - _S1012) / _S1013;
        float2  _S1014 = _S1008;
        FixedArray<float, 8>  _S1015 = dist_coeffs_24;
        bool _S1016 = undistort_point_2(uv_59, &_S1015, int(12), &_S1014);
        (&_S1009)->_S1000 = _S1014;
        (&_S1009)->_S1001 = _S1016;
        bool _S1017 = !!_S1016;
        if(_S1017)
        {
            float2  uv_60 = (pix_center_11 + make_float2 (1.0f, -0.0f) - _S1012) / _S1013;
            float2  _S1018 = _S1008;
            FixedArray<float, 8>  _S1019 = dist_coeffs_24;
            bool _S1020 = undistort_point_2(uv_60, &_S1019, int(12), &_S1018);
            (&_S1009)->_S1002 = _S1018;
            (&_S1009)->_S1003 = _S1020;
            if(!!_S1020)
            {
                _S1011 = int(2);
            }
            else
            {
                _S1011 = int(0);
            }
            if(_S1011 != int(2))
            {
                _runFlag_16 = false;
            }
            else
            {
                _runFlag_16 = _S1017;
            }
            if(_runFlag_16)
            {
                float2  uv_61 = (pix_center_11 + make_float2 (0.0f, -1.0f) - _S1012) / _S1013;
                float2  _S1021 = _S1008;
                FixedArray<float, 8>  _S1022 = dist_coeffs_24;
                bool _S1023 = undistort_point_2(uv_61, &_S1022, int(12), &_S1021);
                (&_S1009)->_S1004 = _S1021;
                (&_S1009)->_S1005 = _S1023;
                if(!_S1023)
                {
                    _runFlag_16 = false;
                }
                if(_runFlag_16)
                {
                    float2  uv_62 = (pix_center_11 + make_float2 (0.0f, 1.0f) - _S1012) / _S1013;
                    float2  _S1024 = _S1008;
                    FixedArray<float, 8>  _S1025 = dist_coeffs_24;
                    bool _S1026 = undistort_point_2(uv_62, &_S1025, int(12), &_S1024);
                    (&_S1009)->_S1006 = _S1024;
                    (&_S1009)->_S1007 = _S1026;
                }
            }
        }
    }
    s_bwd_prop_depth_to_normal_Intermediates_2 _S1027 = _S1009;
    float3  _S1028 = make_float3 (0.0f);
    if(_S1010)
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
    bool _S1029 = !_runFlag_16;
    bool _runFlag_17;
    bool _runFlag_18;
    bool _S1030;
    bool _runFlag_19;
    bool _S1031;
    bool _S1032;
    FixedArray<float3 , 4>  points_11;
    float3  _S1033;
    float3  _S1034;
    float3  _S1035;
    float3  _S1036;
    float3  _S1037;
    float3  _S1038;
    float3  _S1039;
    float3  _S1040;
    float3  _S1041;
    if(_S1029)
    {
        bool _S1042 = !!_S1027._S1001;
        if(_S1042)
        {
            float3  _S1043 = s_primal_ctx_unproject_raydir_0(_S1027._S1000, camera_model_22, is_ray_depth_20);
            float3  _S1044 = make_float3 (depths_9.x) * _S1043;
            bool _S1045 = !!_S1027._S1003;
            if(_S1045)
            {
                float3  _S1046 = s_primal_ctx_unproject_raydir_0(_S1027._S1002, camera_model_22, is_ray_depth_20);
                float3  _S1047 = make_float3 (depths_9.y) * _S1046;
                _S1011 = int(2);
                points_11[int(0)] = _S1044;
                points_11[int(1)] = _S1047;
                points_11[int(2)] = _S1028;
                points_11[int(3)] = _S1028;
                _S1033 = _S1046;
            }
            else
            {
                _S1011 = int(0);
                points_11[int(0)] = _S1044;
                points_11[int(1)] = _S1028;
                points_11[int(2)] = _S1028;
                points_11[int(3)] = _S1028;
                _S1033 = _S1028;
            }
            if(_S1011 != int(2))
            {
                _runFlag_16 = false;
            }
            else
            {
                _runFlag_16 = _S1042;
                _S1011 = int(0);
            }
            if(_runFlag_16)
            {
                if(!_S1027._S1005)
                {
                    _runFlag_17 = false;
                    _S1011 = int(0);
                }
                else
                {
                    _runFlag_17 = _runFlag_16;
                }
                if(_runFlag_17)
                {
                    float3  _S1048 = s_primal_ctx_unproject_raydir_0(_S1027._S1004, camera_model_22, is_ray_depth_20);
                    points_11[int(2)] = make_float3 (depths_9.z) * _S1048;
                    bool _S1049 = !!_S1027._S1007;
                    int _S1050;
                    if(_S1049)
                    {
                        float3  _S1051 = s_primal_ctx_unproject_raydir_0(_S1027._S1006, camera_model_22, is_ray_depth_20);
                        points_11[int(3)] = make_float3 (depths_9.w) * _S1051;
                        _S1050 = int(2);
                        _S1034 = _S1051;
                    }
                    else
                    {
                        _S1050 = int(0);
                        _S1034 = _S1028;
                    }
                    if(_S1050 != int(2))
                    {
                        _runFlag_18 = false;
                        _S1011 = _S1050;
                    }
                    else
                    {
                        _runFlag_18 = _runFlag_17;
                    }
                    if(_runFlag_18)
                    {
                        _S1011 = int(1);
                    }
                    _runFlag_18 = _S1049;
                    _S1035 = _S1048;
                }
                else
                {
                    _runFlag_18 = false;
                    _S1034 = _S1028;
                    _S1035 = _S1028;
                }
            }
            else
            {
                _runFlag_17 = false;
                _runFlag_18 = false;
                _S1034 = _S1028;
                _S1035 = _S1028;
            }
            float3  _S1052 = _S1033;
            _S1033 = _S1034;
            _S1034 = _S1035;
            _S1030 = _S1045;
            _S1035 = _S1052;
            _S1036 = _S1043;
        }
        else
        {
            _S1011 = int(0);
            points_11[int(0)] = _S1028;
            points_11[int(1)] = _S1028;
            points_11[int(2)] = _S1028;
            points_11[int(3)] = _S1028;
            _runFlag_16 = false;
            _runFlag_17 = false;
            _runFlag_18 = false;
            _S1033 = _S1028;
            _S1034 = _S1028;
            _S1030 = false;
            _S1035 = _S1028;
            _S1036 = _S1028;
        }
        if(_S1011 != int(1))
        {
            _runFlag_19 = false;
        }
        else
        {
            _runFlag_19 = _S1029;
        }
        if(_runFlag_19)
        {
            float3  dx_5 = points_11[int(1)] - points_11[int(0)];
            float3  _S1053 = - (points_11[int(3)] - points_11[int(2)]);
            float3  _S1054 = s_primal_ctx_cross_0(dx_5, _S1053);
            bool _S1055 = (s_primal_ctx_dot_0(_S1054, _S1054)) != 0.0f;
            if(_S1055)
            {
                float _S1056 = length_0(_S1054);
                float3  _S1057 = make_float3 (_S1056);
                _S1037 = make_float3 (_S1056 * _S1056);
                _S1038 = _S1057;
            }
            else
            {
                _S1037 = _S1028;
                _S1038 = _S1028;
            }
            float3  _S1058 = _S1038;
            _S1031 = _S1055;
            _S1038 = _S1054;
            _S1039 = _S1058;
            _S1040 = dx_5;
            _S1041 = _S1053;
        }
        else
        {
            _S1031 = false;
            _S1037 = _S1028;
            _S1038 = _S1028;
            _S1039 = _S1028;
            _S1040 = _S1028;
            _S1041 = _S1028;
        }
        bool _S1059 = _runFlag_16;
        bool _S1060 = _runFlag_17;
        bool _S1061 = _runFlag_18;
        float3  _S1062 = _S1033;
        float3  _S1063 = _S1034;
        bool _S1064 = _S1030;
        float3  _S1065 = _S1035;
        float3  _S1066 = _S1036;
        _runFlag_16 = _runFlag_19;
        _runFlag_17 = _S1031;
        _S1033 = _S1037;
        _S1034 = _S1038;
        _S1035 = _S1039;
        _S1036 = _S1040;
        _S1037 = _S1041;
        _runFlag_18 = _S1042;
        _S1030 = _S1059;
        _runFlag_19 = _S1060;
        _S1031 = _S1061;
        _S1038 = _S1062;
        _S1039 = _S1063;
        _S1032 = _S1064;
        _S1040 = _S1065;
        _S1041 = _S1066;
    }
    else
    {
        _runFlag_16 = false;
        _runFlag_17 = false;
        _S1033 = _S1028;
        _S1034 = _S1028;
        _S1035 = _S1028;
        _S1036 = _S1028;
        _S1037 = _S1028;
        _runFlag_18 = false;
        _S1030 = false;
        _runFlag_19 = false;
        _S1031 = false;
        _S1038 = _S1028;
        _S1039 = _S1028;
        _S1032 = false;
        _S1040 = _S1028;
        _S1041 = _S1028;
    }
    float4  _S1067 = make_float4 (0.0f);
    float4  _S1068;
    if(_S1029)
    {
        if(_runFlag_16)
        {
            if(_runFlag_17)
            {
                float3  _S1069 = v_normal_3 / _S1033;
                float3  _S1070 = _S1034 * - _S1069;
                float3  _S1071 = _S1035 * _S1069;
                float _S1072 = _S1070.x + _S1070.y + _S1070.z;
                DiffPair_vectorx3Cfloatx2C3x3E_0 _S1073;
                (&_S1073)->primal_0 = _S1034;
                (&_S1073)->differential_0 = _S1028;
                s_bwd_length_impl_0(&_S1073, _S1072);
                _S1033 = _S1071 + _S1073.differential_0;
            }
            else
            {
                _S1033 = v_normal_3;
            }
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1074;
            (&_S1074)->primal_0 = _S1034;
            (&_S1074)->differential_0 = _S1028;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1075;
            (&_S1075)->primal_0 = _S1034;
            (&_S1075)->differential_0 = _S1028;
            s_bwd_prop_dot_0(&_S1074, &_S1075, 0.0f);
            float3  _S1076 = _S1075.differential_0 + _S1074.differential_0 + _S1033;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1077;
            (&_S1077)->primal_0 = _S1036;
            (&_S1077)->differential_0 = _S1028;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1078;
            (&_S1078)->primal_0 = _S1037;
            (&_S1078)->differential_0 = _S1028;
            s_bwd_prop_cross_0(&_S1077, &_S1078, _S1076);
            float3  s_diff_dy_T_5 = - _S1078.differential_0;
            float3  _S1079 = - s_diff_dy_T_5;
            float3  _S1080 = - _S1077.differential_0;
            FixedArray<float3 , 4>  _S1081;
            _S1081[int(0)] = _S1028;
            _S1081[int(1)] = _S1028;
            _S1081[int(2)] = _S1028;
            _S1081[int(3)] = _S1028;
            _S1081[int(2)] = _S1079;
            _S1081[int(3)] = s_diff_dy_T_5;
            _S1081[int(0)] = _S1080;
            _S1081[int(1)] = _S1077.differential_0;
            points_11[int(0)] = _S1081[int(0)];
            points_11[int(1)] = _S1081[int(1)];
            points_11[int(2)] = _S1081[int(2)];
            points_11[int(3)] = _S1081[int(3)];
        }
        else
        {
            points_11[int(0)] = _S1028;
            points_11[int(1)] = _S1028;
            points_11[int(2)] = _S1028;
            points_11[int(3)] = _S1028;
        }
        if(_runFlag_18)
        {
            if(_S1030)
            {
                if(_runFlag_19)
                {
                    FixedArray<float3 , 4>  _S1082 = points_11;
                    FixedArray<float3 , 4>  _S1083 = points_11;
                    FixedArray<float3 , 4>  _S1084 = points_11;
                    FixedArray<float3 , 4>  _S1085 = points_11;
                    if(_S1031)
                    {
                        float3  _S1086 = _S1038 * _S1085[int(3)];
                        float _S1087 = _S1086.x + _S1086.y + _S1086.z;
                        float4  _S1088 = _S1067;
                        *&((&_S1088)->w) = _S1087;
                        points_11[int(0)] = _S1082[int(0)];
                        points_11[int(1)] = _S1083[int(1)];
                        points_11[int(2)] = _S1084[int(2)];
                        points_11[int(3)] = _S1028;
                        _S1068 = _S1088;
                    }
                    else
                    {
                        points_11[int(0)] = _S1082[int(0)];
                        points_11[int(1)] = _S1083[int(1)];
                        points_11[int(2)] = _S1084[int(2)];
                        points_11[int(3)] = _S1085[int(3)];
                        _S1068 = _S1067;
                    }
                    float3  _S1089 = _S1039 * points_11[int(2)];
                    float _S1090 = _S1089.x + _S1089.y + _S1089.z;
                    FixedArray<float3 , 4>  _S1091 = points_11;
                    FixedArray<float3 , 4>  _S1092 = points_11;
                    float4  _S1093 = _S1067;
                    *&((&_S1093)->z) = _S1090;
                    float4  _S1094 = _S1068 + _S1093;
                    points_11[int(0)] = points_11[int(0)];
                    points_11[int(1)] = _S1091[int(1)];
                    points_11[int(2)] = _S1028;
                    points_11[int(3)] = _S1092[int(3)];
                    _S1068 = _S1094;
                }
                else
                {
                    FixedArray<float3 , 4>  _S1095 = points_11;
                    FixedArray<float3 , 4>  _S1096 = points_11;
                    FixedArray<float3 , 4>  _S1097 = points_11;
                    points_11[int(0)] = points_11[int(0)];
                    points_11[int(1)] = _S1095[int(1)];
                    points_11[int(2)] = _S1096[int(2)];
                    points_11[int(3)] = _S1097[int(3)];
                    _S1068 = _S1067;
                }
            }
            else
            {
                FixedArray<float3 , 4>  _S1098 = points_11;
                FixedArray<float3 , 4>  _S1099 = points_11;
                FixedArray<float3 , 4>  _S1100 = points_11;
                points_11[int(0)] = points_11[int(0)];
                points_11[int(1)] = _S1098[int(1)];
                points_11[int(2)] = _S1099[int(2)];
                points_11[int(3)] = _S1100[int(3)];
                _S1068 = _S1067;
            }
            if(_S1032)
            {
                FixedArray<float3 , 4>  _S1101 = points_11;
                float3  _S1102 = _S1040 * points_11[int(1)];
                float _S1103 = _S1102.x + _S1102.y + _S1102.z;
                float4  _S1104 = _S1067;
                *&((&_S1104)->y) = _S1103;
                float4  _S1105 = _S1068 + _S1104;
                points_11[int(0)] = _S1028;
                points_11[int(1)] = _S1028;
                points_11[int(2)] = _S1028;
                points_11[int(3)] = _S1028;
                _S1033 = _S1101[int(0)];
                _S1068 = _S1105;
            }
            else
            {
                FixedArray<float3 , 4>  _S1106 = points_11;
                FixedArray<float3 , 4>  _S1107 = points_11;
                FixedArray<float3 , 4>  _S1108 = points_11;
                points_11[int(0)] = points_11[int(0)];
                points_11[int(1)] = _S1106[int(1)];
                points_11[int(2)] = _S1107[int(2)];
                points_11[int(3)] = _S1108[int(3)];
                _S1033 = _S1028;
            }
            float3  _S1109 = _S1041 * (points_11[int(0)] + _S1033);
            float _S1110 = _S1109.x + _S1109.y + _S1109.z;
            float4  _S1111 = _S1067;
            *&((&_S1111)->x) = _S1110;
            _S1068 = _S1068 + _S1111;
        }
        else
        {
            _S1068 = _S1067;
        }
    }
    else
    {
        _S1068 = _S1067;
    }
    *v_depths_4 = _S1068;
    return;
}

inline __device__ float ray_depth_to_linear_depth_factor_prism(float2  pix_center_12, float4  intrins_21, FixedArray<float, 8>  dist_coeffs_25, int camera_model_23)
{
    float _S1112;
    for(;;)
    {
        float2  uv_63 = (pix_center_12 - float2 {intrins_21.z, intrins_21.w}) / float2 {intrins_21.x, intrins_21.y};
        FixedArray<float, 8>  _S1113 = dist_coeffs_25;
        float2  uv_u_30;
        bool _S1114 = undistort_point_2(uv_63, &_S1113, int(12), &uv_u_30);
        if(!_S1114)
        {
            _S1112 = 0.0f;
            break;
        }
        float3  raydir_16 = unproject_raydir_0(uv_u_30, camera_model_23, false);
        _S1112 = float((F32_sign((raydir_16.z)))) / length_0(raydir_16);
        break;
    }
    return _S1112;
}

inline __device__ float depth_normal_loss_prism(float2  pix_center_13, float4  intrins_22, FixedArray<float, 8>  dist_coeffs_26, int camera_model_24, bool is_ray_depth_21, float4  depths_10, float3  gt_normal_4)
{
    float _S1115;
    for(;;)
    {
        float3  _S1116;
        float3  * _S1117;
        float3  * _S1118;
        float3  * _S1119;
        float3  * _S1120;
        int _S1121;
        FixedArray<float3 , 5>  points_12;
        for(;;)
        {
            float2  _S1122 = float2 {intrins_22.z, intrins_22.w};
            float2  _S1123 = float2 {intrins_22.x, intrins_22.y};
            float2  uv_64 = (pix_center_13 + make_float2 (-1.0f, -0.0f) - _S1122) / _S1123;
            FixedArray<float, 8>  _S1124 = dist_coeffs_26;
            float2  uv_u_31;
            bool _S1125 = undistort_point_2(uv_64, &_S1124, int(12), &uv_u_31);
            float3  _S1126 = make_float3 (0.0f);
            if(!_S1125)
            {
                _S1121 = int(0);
                _S1120 = nullptr;
                _S1119 = nullptr;
                _S1118 = nullptr;
                _S1117 = nullptr;
                _S1116 = _S1126;
                break;
            }
            float3  raydir_17 = unproject_raydir_0(uv_u_31, camera_model_24, is_ray_depth_21);
            points_12[int(0)] = make_float3 (depths_10.x) * raydir_17;
            float2  uv_65 = (pix_center_13 + make_float2 (1.0f, -0.0f) - _S1122) / _S1123;
            FixedArray<float, 8>  _S1127 = dist_coeffs_26;
            float2  uv_u_32;
            bool _S1128 = undistort_point_2(uv_65, &_S1127, int(12), &uv_u_32);
            if(!_S1128)
            {
                _S1121 = int(0);
                _S1120 = nullptr;
                _S1119 = &points_12[int(0)];
                _S1118 = nullptr;
                _S1117 = nullptr;
                _S1116 = _S1126;
                break;
            }
            float3  raydir_18 = unproject_raydir_0(uv_u_32, camera_model_24, is_ray_depth_21);
            points_12[int(1)] = make_float3 (depths_10.y) * raydir_18;
            float2  uv_66 = (pix_center_13 + make_float2 (0.0f, -1.0f) - _S1122) / _S1123;
            FixedArray<float, 8>  _S1129 = dist_coeffs_26;
            float2  uv_u_33;
            bool _S1130 = undistort_point_2(uv_66, &_S1129, int(12), &uv_u_33);
            if(!_S1130)
            {
                _S1121 = int(0);
                _S1120 = &points_12[int(1)];
                _S1119 = &points_12[int(0)];
                _S1118 = nullptr;
                _S1117 = nullptr;
                _S1116 = _S1126;
                break;
            }
            float3  raydir_19 = unproject_raydir_0(uv_u_33, camera_model_24, is_ray_depth_21);
            points_12[int(2)] = make_float3 (depths_10.z) * raydir_19;
            float2  uv_67 = (pix_center_13 + make_float2 (0.0f, 1.0f) - _S1122) / _S1123;
            FixedArray<float, 8>  _S1131 = dist_coeffs_26;
            float2  uv_u_34;
            bool _S1132 = undistort_point_2(uv_67, &_S1131, int(12), &uv_u_34);
            if(!_S1132)
            {
                _S1121 = int(0);
                _S1120 = &points_12[int(1)];
                _S1119 = &points_12[int(0)];
                _S1118 = nullptr;
                _S1117 = &points_12[int(2)];
                _S1116 = _S1126;
                break;
            }
            float3  raydir_20 = unproject_raydir_0(uv_u_34, camera_model_24, is_ray_depth_21);
            points_12[int(3)] = make_float3 (depths_10.w) * raydir_20;
            float2  uv_68 = (pix_center_13 + make_float2 (0.0f) * make_float2 (0.0f, 3.0f) - _S1122) / _S1123;
            FixedArray<float, 8>  _S1133 = dist_coeffs_26;
            float2  uv_u_35;
            bool _S1134 = undistort_point_2(uv_68, &_S1133, int(12), &uv_u_35);
            if(!_S1134)
            {
                _S1121 = int(0);
                _S1120 = &points_12[int(1)];
                _S1119 = &points_12[int(0)];
                _S1118 = &points_12[int(3)];
                _S1117 = &points_12[int(2)];
                _S1116 = _S1126;
                break;
            }
            float3  raydir_21 = unproject_raydir_0(uv_u_35, camera_model_24, is_ray_depth_21);
            _S1121 = int(1);
            _S1120 = &points_12[int(1)];
            _S1119 = &points_12[int(0)];
            _S1118 = &points_12[int(3)];
            _S1117 = &points_12[int(2)];
            _S1116 = raydir_21;
            break;
        }
        if(_S1121 != int(1))
        {
            _S1115 = 0.0f;
            break;
        }
        float3  normal_14 = cross_0(*_S1120 - *_S1119, - (*_S1118 - *_S1117));
        float3  normal_15;
        if((dot_0(normal_14, normal_14)) != 0.0f)
        {
            normal_15 = normalize_0(normal_14);
        }
        else
        {
            normal_15 = normal_14;
        }
        float3  _S1135;
        if((dot_0(gt_normal_4, gt_normal_4)) != 0.0f)
        {
            _S1135 = normalize_0(gt_normal_4);
        }
        else
        {
            _S1135 = gt_normal_4;
        }
        _S1115 = (1.0f - dot_0(normal_15, _S1135) + 0.00100000004749745f) / ((F32_max((dot_0(normal_15, - normalize_0(_S1116))), (0.0f))) + 0.00100000004749745f);
        break;
    }
    return _S1115;
}

struct s_bwd_prop_depth_normal_loss_Intermediates_2
{
    float2  _S1136;
    bool _S1137;
    float2  _S1138;
    bool _S1139;
    float2  _S1140;
    bool _S1141;
    float2  _S1142;
    bool _S1143;
    float2  _S1144;
    bool _S1145;
};

inline __device__ void depth_normal_loss_vjp_prism(float2  pix_center_14, float4  intrins_23, FixedArray<float, 8>  dist_coeffs_27, int camera_model_25, bool is_ray_depth_22, float4  depths_11, float3  gt_normal_5, float v_loss_2, float4  * v_depths_5, float3  * v_gt_normal_2)
{
    float2  _S1146 = make_float2 (0.0f);
    s_bwd_prop_depth_normal_loss_Intermediates_2 _S1147;
    (&_S1147)->_S1136 = _S1146;
    (&_S1147)->_S1137 = false;
    (&_S1147)->_S1138 = _S1146;
    (&_S1147)->_S1139 = false;
    (&_S1147)->_S1140 = _S1146;
    (&_S1147)->_S1141 = false;
    (&_S1147)->_S1142 = _S1146;
    (&_S1147)->_S1143 = false;
    (&_S1147)->_S1144 = _S1146;
    (&_S1147)->_S1145 = false;
    (&_S1147)->_S1138 = _S1146;
    (&_S1147)->_S1139 = false;
    (&_S1147)->_S1140 = _S1146;
    (&_S1147)->_S1141 = false;
    (&_S1147)->_S1142 = _S1146;
    (&_S1147)->_S1143 = false;
    (&_S1147)->_S1144 = _S1146;
    (&_S1147)->_S1145 = false;
    float2  _S1148 = float2 {intrins_23.z, intrins_23.w};
    float2  _S1149 = float2 {intrins_23.x, intrins_23.y};
    float2  uv_69 = (pix_center_14 + make_float2 (-1.0f, -0.0f) - _S1148) / _S1149;
    float2  _S1150 = _S1146;
    FixedArray<float, 8>  _S1151 = dist_coeffs_27;
    bool _S1152 = undistort_point_2(uv_69, &_S1151, int(12), &_S1150);
    (&_S1147)->_S1136 = _S1150;
    (&_S1147)->_S1137 = _S1152;
    bool _S1153 = !!_S1152;
    bool _runFlag_20;
    if(_S1153)
    {
        float2  uv_70 = (pix_center_14 + make_float2 (1.0f, -0.0f) - _S1148) / _S1149;
        float2  _S1154 = _S1146;
        FixedArray<float, 8>  _S1155 = dist_coeffs_27;
        bool _S1156 = undistort_point_2(uv_70, &_S1155, int(12), &_S1154);
        (&_S1147)->_S1138 = _S1154;
        (&_S1147)->_S1139 = _S1156;
        if(!_S1156)
        {
            _runFlag_20 = false;
        }
        else
        {
            _runFlag_20 = _S1153;
        }
        if(_runFlag_20)
        {
            float2  uv_71 = (pix_center_14 + make_float2 (0.0f, -1.0f) - _S1148) / _S1149;
            float2  _S1157 = _S1146;
            FixedArray<float, 8>  _S1158 = dist_coeffs_27;
            bool _S1159 = undistort_point_2(uv_71, &_S1158, int(12), &_S1157);
            (&_S1147)->_S1140 = _S1157;
            (&_S1147)->_S1141 = _S1159;
            if(!_S1159)
            {
                _runFlag_20 = false;
            }
            if(_runFlag_20)
            {
                float2  uv_72 = (pix_center_14 + make_float2 (0.0f, 1.0f) - _S1148) / _S1149;
                float2  _S1160 = _S1146;
                FixedArray<float, 8>  _S1161 = dist_coeffs_27;
                bool _S1162 = undistort_point_2(uv_72, &_S1161, int(12), &_S1160);
                (&_S1147)->_S1142 = _S1160;
                (&_S1147)->_S1143 = _S1162;
                if(!_S1162)
                {
                    _runFlag_20 = false;
                }
                if(_runFlag_20)
                {
                    float2  uv_73 = (pix_center_14 - _S1148) / _S1149;
                    float2  _S1163 = _S1146;
                    FixedArray<float, 8>  _S1164 = dist_coeffs_27;
                    bool _S1165 = undistort_point_2(uv_73, &_S1164, int(12), &_S1163);
                    (&_S1147)->_S1144 = _S1163;
                    (&_S1147)->_S1145 = _S1165;
                }
            }
        }
    }
    s_bwd_prop_depth_normal_loss_Intermediates_2 _S1166 = _S1147;
    float3  _S1167 = make_float3 (0.0f);
    bool _S1168 = !!_S1147._S1137;
    bool _runFlag_21;
    bool _runFlag_22;
    bool _runFlag_23;
    int _S1169;
    float3  raydir_22;
    float3  _S1170;
    float3  _S1171;
    float3  _S1172;
    float3  _S1173;
    FixedArray<float3 , 5>  points_13;
    if(_S1168)
    {
        float3  _S1174 = s_primal_ctx_unproject_raydir_0(_S1166._S1136, camera_model_25, is_ray_depth_22);
        float3  _S1175 = make_float3 (depths_11.x) * _S1174;
        if(!_S1166._S1139)
        {
            _runFlag_20 = false;
        }
        else
        {
            _runFlag_20 = _S1168;
        }
        if(_runFlag_20)
        {
            float3  _S1176 = s_primal_ctx_unproject_raydir_0(_S1166._S1138, camera_model_25, is_ray_depth_22);
            float3  _S1177 = make_float3 (depths_11.y) * _S1176;
            if(!_S1166._S1141)
            {
                _runFlag_21 = false;
            }
            else
            {
                _runFlag_21 = _runFlag_20;
            }
            if(_runFlag_21)
            {
                float3  _S1178 = s_primal_ctx_unproject_raydir_0(_S1166._S1140, camera_model_25, is_ray_depth_22);
                float3  _S1179 = make_float3 (depths_11.z) * _S1178;
                if(!_S1166._S1143)
                {
                    _runFlag_22 = false;
                }
                else
                {
                    _runFlag_22 = _runFlag_21;
                }
                if(_runFlag_22)
                {
                    float3  _S1180 = s_primal_ctx_unproject_raydir_0(_S1166._S1142, camera_model_25, is_ray_depth_22);
                    float3  _S1181 = make_float3 (depths_11.w) * _S1180;
                    if(!_S1166._S1145)
                    {
                        _runFlag_23 = false;
                    }
                    else
                    {
                        _runFlag_23 = _runFlag_22;
                    }
                    if(_runFlag_23)
                    {
                        float3  _S1182 = s_primal_ctx_unproject_raydir_0(_S1166._S1144, camera_model_25, is_ray_depth_22);
                        _S1169 = int(1);
                        raydir_22 = _S1182;
                    }
                    else
                    {
                        _S1169 = int(0);
                        raydir_22 = _S1180;
                    }
                    points_13[int(0)] = _S1175;
                    points_13[int(1)] = _S1177;
                    points_13[int(2)] = _S1179;
                    points_13[int(3)] = _S1181;
                    points_13[int(4)] = _S1167;
                    _S1170 = _S1180;
                }
                else
                {
                    _S1169 = int(0);
                    raydir_22 = _S1178;
                    points_13[int(0)] = _S1175;
                    points_13[int(1)] = _S1177;
                    points_13[int(2)] = _S1179;
                    points_13[int(3)] = _S1167;
                    points_13[int(4)] = _S1167;
                    _S1170 = _S1167;
                }
                _S1171 = _S1178;
            }
            else
            {
                _S1169 = int(0);
                raydir_22 = _S1176;
                points_13[int(0)] = _S1175;
                points_13[int(1)] = _S1177;
                points_13[int(2)] = _S1167;
                points_13[int(3)] = _S1167;
                points_13[int(4)] = _S1167;
                _runFlag_22 = false;
                _S1170 = _S1167;
                _S1171 = _S1167;
            }
            _S1172 = _S1176;
        }
        else
        {
            _S1169 = int(0);
            raydir_22 = _S1174;
            points_13[int(0)] = _S1175;
            points_13[int(1)] = _S1167;
            points_13[int(2)] = _S1167;
            points_13[int(3)] = _S1167;
            points_13[int(4)] = _S1167;
            _runFlag_21 = false;
            _runFlag_22 = false;
            _S1170 = _S1167;
            _S1171 = _S1167;
            _S1172 = _S1167;
        }
        _S1173 = _S1174;
    }
    else
    {
        _S1169 = int(0);
        points_13[int(0)] = _S1167;
        points_13[int(1)] = _S1167;
        points_13[int(2)] = _S1167;
        points_13[int(3)] = _S1167;
        points_13[int(4)] = _S1167;
        _runFlag_20 = false;
        _runFlag_21 = false;
        _runFlag_22 = false;
        _S1170 = _S1167;
        _S1171 = _S1167;
        _S1172 = _S1167;
        _S1173 = _S1167;
    }
    bool _S1183 = !(_S1169 != int(1));
    bool _S1184;
    float3  normal_16;
    float3  _S1185;
    float3  _S1186;
    float3  _S1187;
    float3  _S1188;
    float _S1189;
    float _S1190;
    float _S1191;
    float _S1192;
    if(_S1183)
    {
        float3  dx_6 = points_13[int(1)] - points_13[int(0)];
        float3  _S1193 = - (points_13[int(3)] - points_13[int(2)]);
        float3  _S1194 = s_primal_ctx_cross_0(dx_6, _S1193);
        bool _S1195 = (s_primal_ctx_dot_0(_S1194, _S1194)) != 0.0f;
        if(_S1195)
        {
            normal_16 = normalize_0(_S1194);
        }
        else
        {
            normal_16 = _S1194;
        }
        bool _S1196 = (s_primal_ctx_dot_0(gt_normal_5, gt_normal_5)) != 0.0f;
        if(_S1196)
        {
            _S1185 = normalize_0(gt_normal_5);
        }
        else
        {
            _S1185 = gt_normal_5;
        }
        float3  _S1197 = - normalize_0(raydir_22);
        float _S1198 = s_primal_ctx_dot_0(normal_16, _S1197);
        float _S1199 = 1.0f - s_primal_ctx_dot_0(normal_16, _S1185) + 0.00100000004749745f;
        float _S1200 = (F32_max((_S1198), (0.0f))) + 0.00100000004749745f;
        _S1189 = _S1200 * _S1200;
        _S1190 = _S1199;
        _S1191 = _S1200;
        _S1192 = _S1198;
        raydir_22 = normal_16;
        normal_16 = _S1197;
        _runFlag_23 = _S1196;
        _S1184 = _S1195;
        _S1186 = _S1194;
        _S1187 = dx_6;
        _S1188 = _S1193;
    }
    else
    {
        _S1189 = 0.0f;
        _S1190 = 0.0f;
        _S1191 = 0.0f;
        _S1192 = 0.0f;
        raydir_22 = _S1167;
        normal_16 = _S1167;
        _S1185 = _S1167;
        _runFlag_23 = false;
        _S1184 = false;
        _S1186 = _S1167;
        _S1187 = _S1167;
        _S1188 = _S1167;
    }
    float4  _S1201 = make_float4 (0.0f);
    if(_S1183)
    {
        float _S1202 = v_loss_2 / _S1189;
        float _S1203 = _S1190 * - _S1202;
        float s_diff_num_T_2 = _S1191 * _S1202;
        DiffPair_float_0 _S1204;
        (&_S1204)->primal_0 = _S1192;
        (&_S1204)->differential_0 = 0.0f;
        DiffPair_float_0 _S1205;
        (&_S1205)->primal_0 = 0.0f;
        (&_S1205)->differential_0 = 0.0f;
        _d_max_0(&_S1204, &_S1205, _S1203);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1206;
        (&_S1206)->primal_0 = raydir_22;
        (&_S1206)->differential_0 = _S1167;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1207;
        (&_S1207)->primal_0 = normal_16;
        (&_S1207)->differential_0 = _S1167;
        s_bwd_prop_dot_0(&_S1206, &_S1207, _S1204.differential_0);
        float _S1208 = - s_diff_num_T_2;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1209;
        (&_S1209)->primal_0 = raydir_22;
        (&_S1209)->differential_0 = _S1167;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1210;
        (&_S1210)->primal_0 = _S1185;
        (&_S1210)->differential_0 = _S1167;
        s_bwd_prop_dot_0(&_S1209, &_S1210, _S1208);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1211 = _S1210;
        float3  _S1212 = _S1206.differential_0 + _S1209.differential_0;
        if(_runFlag_23)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1213;
            (&_S1213)->primal_0 = gt_normal_5;
            (&_S1213)->differential_0 = _S1167;
            s_bwd_normalize_impl_0(&_S1213, _S1211.differential_0);
            raydir_22 = _S1213.differential_0;
        }
        else
        {
            raydir_22 = _S1211.differential_0;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1214;
        (&_S1214)->primal_0 = gt_normal_5;
        (&_S1214)->differential_0 = _S1167;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1215;
        (&_S1215)->primal_0 = gt_normal_5;
        (&_S1215)->differential_0 = _S1167;
        s_bwd_prop_dot_0(&_S1214, &_S1215, 0.0f);
        float3  _S1216 = _S1215.differential_0 + _S1214.differential_0 + raydir_22;
        if(_S1184)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1217;
            (&_S1217)->primal_0 = _S1186;
            (&_S1217)->differential_0 = _S1167;
            s_bwd_normalize_impl_0(&_S1217, _S1212);
            raydir_22 = _S1217.differential_0;
        }
        else
        {
            raydir_22 = _S1212;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1218;
        (&_S1218)->primal_0 = _S1186;
        (&_S1218)->differential_0 = _S1167;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1219;
        (&_S1219)->primal_0 = _S1186;
        (&_S1219)->differential_0 = _S1167;
        s_bwd_prop_dot_0(&_S1218, &_S1219, 0.0f);
        float3  _S1220 = _S1219.differential_0 + _S1218.differential_0 + raydir_22;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1221;
        (&_S1221)->primal_0 = _S1187;
        (&_S1221)->differential_0 = _S1167;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1222;
        (&_S1222)->primal_0 = _S1188;
        (&_S1222)->differential_0 = _S1167;
        s_bwd_prop_cross_0(&_S1221, &_S1222, _S1220);
        float3  s_diff_dy_T_6 = - _S1222.differential_0;
        float3  _S1223 = - s_diff_dy_T_6;
        float3  _S1224 = - _S1221.differential_0;
        FixedArray<float3 , 5>  _S1225;
        _S1225[int(0)] = _S1167;
        _S1225[int(1)] = _S1167;
        _S1225[int(2)] = _S1167;
        _S1225[int(3)] = _S1167;
        _S1225[int(4)] = _S1167;
        _S1225[int(2)] = _S1223;
        _S1225[int(3)] = s_diff_dy_T_6;
        _S1225[int(0)] = _S1224;
        _S1225[int(1)] = _S1221.differential_0;
        points_13[int(0)] = _S1225[int(0)];
        points_13[int(1)] = _S1225[int(1)];
        points_13[int(2)] = _S1225[int(2)];
        points_13[int(3)] = _S1225[int(3)];
        points_13[int(4)] = _S1225[int(4)];
        raydir_22 = _S1216;
    }
    else
    {
        points_13[int(0)] = _S1167;
        points_13[int(1)] = _S1167;
        points_13[int(2)] = _S1167;
        points_13[int(3)] = _S1167;
        points_13[int(4)] = _S1167;
        raydir_22 = _S1167;
    }
    float4  _S1226;
    if(_S1168)
    {
        if(_runFlag_20)
        {
            if(_runFlag_21)
            {
                if(_runFlag_22)
                {
                    FixedArray<float3 , 5>  _S1227 = points_13;
                    FixedArray<float3 , 5>  _S1228 = points_13;
                    FixedArray<float3 , 5>  _S1229 = points_13;
                    float3  _S1230 = _S1170 * points_13[int(3)];
                    float _S1231 = _S1230.x + _S1230.y + _S1230.z;
                    float4  _S1232 = _S1201;
                    *&((&_S1232)->w) = _S1231;
                    points_13[int(0)] = _S1167;
                    points_13[int(1)] = _S1167;
                    points_13[int(2)] = _S1167;
                    points_13[int(3)] = _S1167;
                    points_13[int(4)] = _S1167;
                    _S1170 = _S1229[int(2)];
                    normal_16 = _S1227[int(0)];
                    _S1185 = _S1228[int(1)];
                    _S1226 = _S1232;
                }
                else
                {
                    FixedArray<float3 , 5>  _S1233 = points_13;
                    FixedArray<float3 , 5>  _S1234 = points_13;
                    FixedArray<float3 , 5>  _S1235 = points_13;
                    FixedArray<float3 , 5>  _S1236 = points_13;
                    points_13[int(0)] = points_13[int(0)];
                    points_13[int(1)] = _S1233[int(1)];
                    points_13[int(2)] = _S1234[int(2)];
                    points_13[int(3)] = _S1235[int(3)];
                    points_13[int(4)] = _S1236[int(4)];
                    _S1170 = _S1167;
                    normal_16 = _S1167;
                    _S1185 = _S1167;
                    _S1226 = _S1201;
                }
                float3  _S1237 = _S1171 * (points_13[int(2)] + _S1170);
                float _S1238 = _S1237.x + _S1237.y + _S1237.z;
                float3  _S1239 = points_13[int(0)] + normal_16;
                float3  _S1240 = points_13[int(1)] + _S1185;
                float4  _S1241 = _S1201;
                *&((&_S1241)->z) = _S1238;
                float4  _S1242 = _S1226 + _S1241;
                points_13[int(0)] = _S1167;
                points_13[int(1)] = _S1167;
                points_13[int(2)] = _S1167;
                points_13[int(3)] = _S1167;
                points_13[int(4)] = _S1167;
                _S1170 = _S1240;
                _S1171 = _S1239;
                _S1226 = _S1242;
            }
            else
            {
                FixedArray<float3 , 5>  _S1243 = points_13;
                FixedArray<float3 , 5>  _S1244 = points_13;
                FixedArray<float3 , 5>  _S1245 = points_13;
                FixedArray<float3 , 5>  _S1246 = points_13;
                points_13[int(0)] = points_13[int(0)];
                points_13[int(1)] = _S1243[int(1)];
                points_13[int(2)] = _S1244[int(2)];
                points_13[int(3)] = _S1245[int(3)];
                points_13[int(4)] = _S1246[int(4)];
                _S1170 = _S1167;
                _S1171 = _S1167;
                _S1226 = _S1201;
            }
            float3  _S1247 = _S1172 * (points_13[int(1)] + _S1170);
            float _S1248 = _S1247.x + _S1247.y + _S1247.z;
            float3  _S1249 = points_13[int(0)] + _S1171;
            float4  _S1250 = _S1201;
            *&((&_S1250)->y) = _S1248;
            float4  _S1251 = _S1226 + _S1250;
            points_13[int(0)] = _S1167;
            points_13[int(1)] = _S1167;
            points_13[int(2)] = _S1167;
            points_13[int(3)] = _S1167;
            points_13[int(4)] = _S1167;
            _S1170 = _S1249;
            _S1226 = _S1251;
        }
        else
        {
            FixedArray<float3 , 5>  _S1252 = points_13;
            FixedArray<float3 , 5>  _S1253 = points_13;
            FixedArray<float3 , 5>  _S1254 = points_13;
            FixedArray<float3 , 5>  _S1255 = points_13;
            points_13[int(0)] = points_13[int(0)];
            points_13[int(1)] = _S1252[int(1)];
            points_13[int(2)] = _S1253[int(2)];
            points_13[int(3)] = _S1254[int(3)];
            points_13[int(4)] = _S1255[int(4)];
            _S1170 = _S1167;
            _S1226 = _S1201;
        }
        float3  _S1256 = _S1173 * (points_13[int(0)] + _S1170);
        float _S1257 = _S1256.x + _S1256.y + _S1256.z;
        float4  _S1258 = _S1201;
        *&((&_S1258)->x) = _S1257;
        _S1226 = _S1226 + _S1258;
    }
    else
    {
        _S1226 = _S1201;
    }
    *v_depths_5 = _S1226;
    *v_gt_normal_2 = raydir_22;
    return;
}

inline __device__ float3  generate_ray_d2n_rational(float2  pix_pos_9, float4  intrins_24, FixedArray<float, 8>  dist_coeffs_28, int camera_model_26, bool is_ray_depth_23)
{
    float3  _S1259;
    for(;;)
    {
        float2  uv_74 = (pix_pos_9 - float2 {intrins_24.z, intrins_24.w}) / float2 {intrins_24.x, intrins_24.y};
        FixedArray<float, 8>  _S1260 = dist_coeffs_28;
        float2  uv_u_36;
        bool _S1261 = undistort_point_3(uv_74, &_S1260, int(12), &uv_u_36);
        if(!_S1261)
        {
            int3  _S1262 = make_int3 (int(0));
            float3  _S1263 = make_float3 ((float)_S1262.x, (float)_S1262.y, (float)_S1262.z);
            _S1259 = _S1263;
            break;
        }
        _S1259 = unproject_raydir_0(uv_u_36, camera_model_26, is_ray_depth_23);
        break;
    }
    return _S1259;
}

inline __device__ float3  depth_to_point_rational(float2  pix_pos_10, float4  intrins_25, FixedArray<float, 8>  dist_coeffs_29, int camera_model_27, bool is_ray_depth_24, float depth_8)
{
    float3  _S1264;
    for(;;)
    {
        float2  uv_75 = (pix_pos_10 - float2 {intrins_25.z, intrins_25.w}) / float2 {intrins_25.x, intrins_25.y};
        FixedArray<float, 8>  _S1265 = dist_coeffs_29;
        float2  uv_u_37;
        bool _S1266 = undistort_point_3(uv_75, &_S1265, int(12), &uv_u_37);
        if(!_S1266)
        {
            _S1264 = make_float3 (0.0f);
            break;
        }
        _S1264 = make_float3 (depth_8) * unproject_raydir_0(uv_u_37, camera_model_27, is_ray_depth_24);
        break;
    }
    return _S1264;
}

struct s_bwd_prop_depth_to_point_Intermediates_3
{
    float2  _S1267;
    bool _S1268;
};

inline __device__ float depth_to_point_vjp_rational(float2  pix_pos_11, float4  intrins_26, FixedArray<float, 8>  dist_coeffs_30, int camera_model_28, bool is_ray_depth_25, float depth_9, float3  v_point_3)
{
    float2  _S1269 = make_float2 (0.0f);
    s_bwd_prop_depth_to_point_Intermediates_3 _S1270;
    (&_S1270)->_S1267 = _S1269;
    (&_S1270)->_S1268 = false;
    float2  uv_76 = (pix_pos_11 - float2 {intrins_26.z, intrins_26.w}) / float2 {intrins_26.x, intrins_26.y};
    float2  _S1271 = _S1269;
    FixedArray<float, 8>  _S1272 = dist_coeffs_30;
    bool _S1273 = undistort_point_3(uv_76, &_S1272, int(12), &_S1271);
    (&_S1270)->_S1267 = _S1271;
    (&_S1270)->_S1268 = _S1273;
    s_bwd_prop_depth_to_point_Intermediates_3 _S1274 = _S1270;
    float3  _S1275 = make_float3 (0.0f);
    bool _S1276 = !!_S1270._S1268;
    float3  _S1277;
    if(_S1276)
    {
        _S1277 = s_primal_ctx_unproject_raydir_0(_S1274._S1267, camera_model_28, is_ray_depth_25);
    }
    else
    {
        _S1277 = _S1275;
    }
    if(_S1276)
    {
        _S1277 = _S1277 * v_point_3;
    }
    else
    {
        _S1277 = _S1275;
    }
    return _S1277.x + _S1277.y + _S1277.z;
}

inline __device__ float3  depth_to_normal_rational(float2  pix_center_15, float4  intrins_27, FixedArray<float, 8>  dist_coeffs_31, int camera_model_29, bool is_ray_depth_26, float4  depths_12)
{
    float3  normal_17;
    for(;;)
    {
        bool _S1278;
        if((depths_12.x) == 0.0f)
        {
            _S1278 = true;
        }
        else
        {
            _S1278 = (depths_12.y) == 0.0f;
        }
        if(_S1278)
        {
            _S1278 = true;
        }
        else
        {
            _S1278 = (depths_12.z) == 0.0f;
        }
        if(_S1278)
        {
            _S1278 = true;
        }
        else
        {
            _S1278 = (depths_12.w) == 0.0f;
        }
        if(_S1278)
        {
            normal_17 = make_float3 (0.0f);
            break;
        }
        float3  * _S1279;
        float3  * _S1280;
        float3  * _S1281;
        float3  * _S1282;
        int _S1283;
        FixedArray<float3 , 4>  points_14;
        for(;;)
        {
            float2  _S1284 = float2 {intrins_27.z, intrins_27.w};
            float2  _S1285 = float2 {intrins_27.x, intrins_27.y};
            float2  uv_77 = (pix_center_15 + make_float2 (-1.0f, -0.0f) - _S1284) / _S1285;
            FixedArray<float, 8>  _S1286 = dist_coeffs_31;
            float2  uv_u_38;
            bool _S1287 = undistort_point_3(uv_77, &_S1286, int(12), &uv_u_38);
            if(!_S1287)
            {
                float3  _S1288 = make_float3 (0.0f);
                _S1283 = int(0);
                _S1282 = nullptr;
                _S1281 = nullptr;
                _S1280 = nullptr;
                _S1279 = nullptr;
                normal_17 = _S1288;
                break;
            }
            points_14[int(0)] = make_float3 (depths_12.x) * unproject_raydir_0(uv_u_38, camera_model_29, is_ray_depth_26);
            for(;;)
            {
                float2  uv_78 = (pix_center_15 + make_float2 (1.0f, -0.0f) - _S1284) / _S1285;
                FixedArray<float, 8>  _S1289 = dist_coeffs_31;
                float2  uv_u_39;
                bool _S1290 = undistort_point_3(uv_78, &_S1289, int(12), &uv_u_39);
                if(!_S1290)
                {
                    float3  _S1291 = make_float3 (0.0f);
                    _S1283 = int(0);
                    _S1282 = nullptr;
                    normal_17 = _S1291;
                    break;
                }
                points_14[int(1)] = make_float3 (depths_12.y) * unproject_raydir_0(uv_u_39, camera_model_29, is_ray_depth_26);
                _S1283 = int(2);
                _S1282 = &points_14[int(1)];
                break;
            }
            if(_S1283 != int(2))
            {
                _S1281 = &points_14[int(0)];
                _S1280 = nullptr;
                _S1279 = nullptr;
                break;
            }
            float2  uv_79 = (pix_center_15 + make_float2 (0.0f, -1.0f) - _S1284) / _S1285;
            FixedArray<float, 8>  _S1292 = dist_coeffs_31;
            float2  uv_u_40;
            bool _S1293 = undistort_point_3(uv_79, &_S1292, int(12), &uv_u_40);
            if(!_S1293)
            {
                float3  _S1294 = make_float3 (0.0f);
                _S1283 = int(0);
                _S1281 = &points_14[int(0)];
                _S1280 = nullptr;
                _S1279 = nullptr;
                normal_17 = _S1294;
                break;
            }
            points_14[int(2)] = make_float3 (depths_12.z) * unproject_raydir_0(uv_u_40, camera_model_29, is_ray_depth_26);
            for(;;)
            {
                float2  uv_80 = (pix_center_15 + make_float2 (0.0f, 1.0f) - _S1284) / _S1285;
                FixedArray<float, 8>  _S1295 = dist_coeffs_31;
                float2  uv_u_41;
                bool _S1296 = undistort_point_3(uv_80, &_S1295, int(12), &uv_u_41);
                if(!_S1296)
                {
                    float3  _S1297 = make_float3 (0.0f);
                    _S1283 = int(0);
                    _S1281 = nullptr;
                    normal_17 = _S1297;
                    break;
                }
                points_14[int(3)] = make_float3 (depths_12.w) * unproject_raydir_0(uv_u_41, camera_model_29, is_ray_depth_26);
                _S1283 = int(2);
                _S1281 = &points_14[int(3)];
                break;
            }
            if(_S1283 != int(2))
            {
                float3  * _S1298 = _S1281;
                _S1281 = &points_14[int(0)];
                _S1280 = _S1298;
                _S1279 = &points_14[int(2)];
                break;
            }
            float3  * _S1299 = _S1281;
            _S1283 = int(1);
            _S1281 = &points_14[int(0)];
            _S1280 = _S1299;
            _S1279 = &points_14[int(2)];
            break;
        }
        if(_S1283 != int(1))
        {
            break;
        }
        float3  normal_18 = cross_0(*_S1282 - *_S1281, - (*_S1280 - *_S1279));
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
    float2  _S1300;
    bool _S1301;
    float2  _S1302;
    bool _S1303;
    float2  _S1304;
    bool _S1305;
    float2  _S1306;
    bool _S1307;
};

inline __device__ void depth_to_normal_vjp_rational(float2  pix_center_16, float4  intrins_28, FixedArray<float, 8>  dist_coeffs_32, int camera_model_30, bool is_ray_depth_27, float4  depths_13, float3  v_normal_4, float4  * v_depths_6)
{
    float2  _S1308 = make_float2 (0.0f);
    s_bwd_prop_depth_to_normal_Intermediates_3 _S1309;
    (&_S1309)->_S1300 = _S1308;
    (&_S1309)->_S1301 = false;
    (&_S1309)->_S1302 = _S1308;
    (&_S1309)->_S1303 = false;
    (&_S1309)->_S1304 = _S1308;
    (&_S1309)->_S1305 = false;
    (&_S1309)->_S1306 = _S1308;
    (&_S1309)->_S1307 = false;
    (&_S1309)->_S1300 = _S1308;
    (&_S1309)->_S1301 = false;
    (&_S1309)->_S1302 = _S1308;
    (&_S1309)->_S1303 = false;
    (&_S1309)->_S1304 = _S1308;
    (&_S1309)->_S1305 = false;
    (&_S1309)->_S1306 = _S1308;
    (&_S1309)->_S1307 = false;
    bool _S1310 = (depths_13.x) == 0.0f;
    bool _runFlag_24;
    if(_S1310)
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
    int _S1311;
    if(!_runFlag_24)
    {
        float2  _S1312 = float2 {intrins_28.z, intrins_28.w};
        float2  _S1313 = float2 {intrins_28.x, intrins_28.y};
        float2  uv_81 = (pix_center_16 + make_float2 (-1.0f, -0.0f) - _S1312) / _S1313;
        float2  _S1314 = _S1308;
        FixedArray<float, 8>  _S1315 = dist_coeffs_32;
        bool _S1316 = undistort_point_3(uv_81, &_S1315, int(12), &_S1314);
        (&_S1309)->_S1300 = _S1314;
        (&_S1309)->_S1301 = _S1316;
        bool _S1317 = !!_S1316;
        if(_S1317)
        {
            float2  uv_82 = (pix_center_16 + make_float2 (1.0f, -0.0f) - _S1312) / _S1313;
            float2  _S1318 = _S1308;
            FixedArray<float, 8>  _S1319 = dist_coeffs_32;
            bool _S1320 = undistort_point_3(uv_82, &_S1319, int(12), &_S1318);
            (&_S1309)->_S1302 = _S1318;
            (&_S1309)->_S1303 = _S1320;
            if(!!_S1320)
            {
                _S1311 = int(2);
            }
            else
            {
                _S1311 = int(0);
            }
            if(_S1311 != int(2))
            {
                _runFlag_24 = false;
            }
            else
            {
                _runFlag_24 = _S1317;
            }
            if(_runFlag_24)
            {
                float2  uv_83 = (pix_center_16 + make_float2 (0.0f, -1.0f) - _S1312) / _S1313;
                float2  _S1321 = _S1308;
                FixedArray<float, 8>  _S1322 = dist_coeffs_32;
                bool _S1323 = undistort_point_3(uv_83, &_S1322, int(12), &_S1321);
                (&_S1309)->_S1304 = _S1321;
                (&_S1309)->_S1305 = _S1323;
                if(!_S1323)
                {
                    _runFlag_24 = false;
                }
                if(_runFlag_24)
                {
                    float2  uv_84 = (pix_center_16 + make_float2 (0.0f, 1.0f) - _S1312) / _S1313;
                    float2  _S1324 = _S1308;
                    FixedArray<float, 8>  _S1325 = dist_coeffs_32;
                    bool _S1326 = undistort_point_3(uv_84, &_S1325, int(12), &_S1324);
                    (&_S1309)->_S1306 = _S1324;
                    (&_S1309)->_S1307 = _S1326;
                }
            }
        }
    }
    s_bwd_prop_depth_to_normal_Intermediates_3 _S1327 = _S1309;
    float3  _S1328 = make_float3 (0.0f);
    if(_S1310)
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
    bool _S1329 = !_runFlag_24;
    bool _runFlag_25;
    bool _runFlag_26;
    bool _S1330;
    bool _runFlag_27;
    bool _S1331;
    bool _S1332;
    FixedArray<float3 , 4>  points_15;
    float3  _S1333;
    float3  _S1334;
    float3  _S1335;
    float3  _S1336;
    float3  _S1337;
    float3  _S1338;
    float3  _S1339;
    float3  _S1340;
    float3  _S1341;
    if(_S1329)
    {
        bool _S1342 = !!_S1327._S1301;
        if(_S1342)
        {
            float3  _S1343 = s_primal_ctx_unproject_raydir_0(_S1327._S1300, camera_model_30, is_ray_depth_27);
            float3  _S1344 = make_float3 (depths_13.x) * _S1343;
            bool _S1345 = !!_S1327._S1303;
            if(_S1345)
            {
                float3  _S1346 = s_primal_ctx_unproject_raydir_0(_S1327._S1302, camera_model_30, is_ray_depth_27);
                float3  _S1347 = make_float3 (depths_13.y) * _S1346;
                _S1311 = int(2);
                points_15[int(0)] = _S1344;
                points_15[int(1)] = _S1347;
                points_15[int(2)] = _S1328;
                points_15[int(3)] = _S1328;
                _S1333 = _S1346;
            }
            else
            {
                _S1311 = int(0);
                points_15[int(0)] = _S1344;
                points_15[int(1)] = _S1328;
                points_15[int(2)] = _S1328;
                points_15[int(3)] = _S1328;
                _S1333 = _S1328;
            }
            if(_S1311 != int(2))
            {
                _runFlag_24 = false;
            }
            else
            {
                _runFlag_24 = _S1342;
                _S1311 = int(0);
            }
            if(_runFlag_24)
            {
                if(!_S1327._S1305)
                {
                    _runFlag_25 = false;
                    _S1311 = int(0);
                }
                else
                {
                    _runFlag_25 = _runFlag_24;
                }
                if(_runFlag_25)
                {
                    float3  _S1348 = s_primal_ctx_unproject_raydir_0(_S1327._S1304, camera_model_30, is_ray_depth_27);
                    points_15[int(2)] = make_float3 (depths_13.z) * _S1348;
                    bool _S1349 = !!_S1327._S1307;
                    int _S1350;
                    if(_S1349)
                    {
                        float3  _S1351 = s_primal_ctx_unproject_raydir_0(_S1327._S1306, camera_model_30, is_ray_depth_27);
                        points_15[int(3)] = make_float3 (depths_13.w) * _S1351;
                        _S1350 = int(2);
                        _S1334 = _S1351;
                    }
                    else
                    {
                        _S1350 = int(0);
                        _S1334 = _S1328;
                    }
                    if(_S1350 != int(2))
                    {
                        _runFlag_26 = false;
                        _S1311 = _S1350;
                    }
                    else
                    {
                        _runFlag_26 = _runFlag_25;
                    }
                    if(_runFlag_26)
                    {
                        _S1311 = int(1);
                    }
                    _runFlag_26 = _S1349;
                    _S1335 = _S1348;
                }
                else
                {
                    _runFlag_26 = false;
                    _S1334 = _S1328;
                    _S1335 = _S1328;
                }
            }
            else
            {
                _runFlag_25 = false;
                _runFlag_26 = false;
                _S1334 = _S1328;
                _S1335 = _S1328;
            }
            float3  _S1352 = _S1333;
            _S1333 = _S1334;
            _S1334 = _S1335;
            _S1330 = _S1345;
            _S1335 = _S1352;
            _S1336 = _S1343;
        }
        else
        {
            _S1311 = int(0);
            points_15[int(0)] = _S1328;
            points_15[int(1)] = _S1328;
            points_15[int(2)] = _S1328;
            points_15[int(3)] = _S1328;
            _runFlag_24 = false;
            _runFlag_25 = false;
            _runFlag_26 = false;
            _S1333 = _S1328;
            _S1334 = _S1328;
            _S1330 = false;
            _S1335 = _S1328;
            _S1336 = _S1328;
        }
        if(_S1311 != int(1))
        {
            _runFlag_27 = false;
        }
        else
        {
            _runFlag_27 = _S1329;
        }
        if(_runFlag_27)
        {
            float3  dx_7 = points_15[int(1)] - points_15[int(0)];
            float3  _S1353 = - (points_15[int(3)] - points_15[int(2)]);
            float3  _S1354 = s_primal_ctx_cross_0(dx_7, _S1353);
            bool _S1355 = (s_primal_ctx_dot_0(_S1354, _S1354)) != 0.0f;
            if(_S1355)
            {
                float _S1356 = length_0(_S1354);
                float3  _S1357 = make_float3 (_S1356);
                _S1337 = make_float3 (_S1356 * _S1356);
                _S1338 = _S1357;
            }
            else
            {
                _S1337 = _S1328;
                _S1338 = _S1328;
            }
            float3  _S1358 = _S1338;
            _S1331 = _S1355;
            _S1338 = _S1354;
            _S1339 = _S1358;
            _S1340 = dx_7;
            _S1341 = _S1353;
        }
        else
        {
            _S1331 = false;
            _S1337 = _S1328;
            _S1338 = _S1328;
            _S1339 = _S1328;
            _S1340 = _S1328;
            _S1341 = _S1328;
        }
        bool _S1359 = _runFlag_24;
        bool _S1360 = _runFlag_25;
        bool _S1361 = _runFlag_26;
        float3  _S1362 = _S1333;
        float3  _S1363 = _S1334;
        bool _S1364 = _S1330;
        float3  _S1365 = _S1335;
        float3  _S1366 = _S1336;
        _runFlag_24 = _runFlag_27;
        _runFlag_25 = _S1331;
        _S1333 = _S1337;
        _S1334 = _S1338;
        _S1335 = _S1339;
        _S1336 = _S1340;
        _S1337 = _S1341;
        _runFlag_26 = _S1342;
        _S1330 = _S1359;
        _runFlag_27 = _S1360;
        _S1331 = _S1361;
        _S1338 = _S1362;
        _S1339 = _S1363;
        _S1332 = _S1364;
        _S1340 = _S1365;
        _S1341 = _S1366;
    }
    else
    {
        _runFlag_24 = false;
        _runFlag_25 = false;
        _S1333 = _S1328;
        _S1334 = _S1328;
        _S1335 = _S1328;
        _S1336 = _S1328;
        _S1337 = _S1328;
        _runFlag_26 = false;
        _S1330 = false;
        _runFlag_27 = false;
        _S1331 = false;
        _S1338 = _S1328;
        _S1339 = _S1328;
        _S1332 = false;
        _S1340 = _S1328;
        _S1341 = _S1328;
    }
    float4  _S1367 = make_float4 (0.0f);
    float4  _S1368;
    if(_S1329)
    {
        if(_runFlag_24)
        {
            if(_runFlag_25)
            {
                float3  _S1369 = v_normal_4 / _S1333;
                float3  _S1370 = _S1334 * - _S1369;
                float3  _S1371 = _S1335 * _S1369;
                float _S1372 = _S1370.x + _S1370.y + _S1370.z;
                DiffPair_vectorx3Cfloatx2C3x3E_0 _S1373;
                (&_S1373)->primal_0 = _S1334;
                (&_S1373)->differential_0 = _S1328;
                s_bwd_length_impl_0(&_S1373, _S1372);
                _S1333 = _S1371 + _S1373.differential_0;
            }
            else
            {
                _S1333 = v_normal_4;
            }
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1374;
            (&_S1374)->primal_0 = _S1334;
            (&_S1374)->differential_0 = _S1328;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1375;
            (&_S1375)->primal_0 = _S1334;
            (&_S1375)->differential_0 = _S1328;
            s_bwd_prop_dot_0(&_S1374, &_S1375, 0.0f);
            float3  _S1376 = _S1375.differential_0 + _S1374.differential_0 + _S1333;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1377;
            (&_S1377)->primal_0 = _S1336;
            (&_S1377)->differential_0 = _S1328;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1378;
            (&_S1378)->primal_0 = _S1337;
            (&_S1378)->differential_0 = _S1328;
            s_bwd_prop_cross_0(&_S1377, &_S1378, _S1376);
            float3  s_diff_dy_T_7 = - _S1378.differential_0;
            float3  _S1379 = - s_diff_dy_T_7;
            float3  _S1380 = - _S1377.differential_0;
            FixedArray<float3 , 4>  _S1381;
            _S1381[int(0)] = _S1328;
            _S1381[int(1)] = _S1328;
            _S1381[int(2)] = _S1328;
            _S1381[int(3)] = _S1328;
            _S1381[int(2)] = _S1379;
            _S1381[int(3)] = s_diff_dy_T_7;
            _S1381[int(0)] = _S1380;
            _S1381[int(1)] = _S1377.differential_0;
            points_15[int(0)] = _S1381[int(0)];
            points_15[int(1)] = _S1381[int(1)];
            points_15[int(2)] = _S1381[int(2)];
            points_15[int(3)] = _S1381[int(3)];
        }
        else
        {
            points_15[int(0)] = _S1328;
            points_15[int(1)] = _S1328;
            points_15[int(2)] = _S1328;
            points_15[int(3)] = _S1328;
        }
        if(_runFlag_26)
        {
            if(_S1330)
            {
                if(_runFlag_27)
                {
                    FixedArray<float3 , 4>  _S1382 = points_15;
                    FixedArray<float3 , 4>  _S1383 = points_15;
                    FixedArray<float3 , 4>  _S1384 = points_15;
                    FixedArray<float3 , 4>  _S1385 = points_15;
                    if(_S1331)
                    {
                        float3  _S1386 = _S1338 * _S1385[int(3)];
                        float _S1387 = _S1386.x + _S1386.y + _S1386.z;
                        float4  _S1388 = _S1367;
                        *&((&_S1388)->w) = _S1387;
                        points_15[int(0)] = _S1382[int(0)];
                        points_15[int(1)] = _S1383[int(1)];
                        points_15[int(2)] = _S1384[int(2)];
                        points_15[int(3)] = _S1328;
                        _S1368 = _S1388;
                    }
                    else
                    {
                        points_15[int(0)] = _S1382[int(0)];
                        points_15[int(1)] = _S1383[int(1)];
                        points_15[int(2)] = _S1384[int(2)];
                        points_15[int(3)] = _S1385[int(3)];
                        _S1368 = _S1367;
                    }
                    float3  _S1389 = _S1339 * points_15[int(2)];
                    float _S1390 = _S1389.x + _S1389.y + _S1389.z;
                    FixedArray<float3 , 4>  _S1391 = points_15;
                    FixedArray<float3 , 4>  _S1392 = points_15;
                    float4  _S1393 = _S1367;
                    *&((&_S1393)->z) = _S1390;
                    float4  _S1394 = _S1368 + _S1393;
                    points_15[int(0)] = points_15[int(0)];
                    points_15[int(1)] = _S1391[int(1)];
                    points_15[int(2)] = _S1328;
                    points_15[int(3)] = _S1392[int(3)];
                    _S1368 = _S1394;
                }
                else
                {
                    FixedArray<float3 , 4>  _S1395 = points_15;
                    FixedArray<float3 , 4>  _S1396 = points_15;
                    FixedArray<float3 , 4>  _S1397 = points_15;
                    points_15[int(0)] = points_15[int(0)];
                    points_15[int(1)] = _S1395[int(1)];
                    points_15[int(2)] = _S1396[int(2)];
                    points_15[int(3)] = _S1397[int(3)];
                    _S1368 = _S1367;
                }
            }
            else
            {
                FixedArray<float3 , 4>  _S1398 = points_15;
                FixedArray<float3 , 4>  _S1399 = points_15;
                FixedArray<float3 , 4>  _S1400 = points_15;
                points_15[int(0)] = points_15[int(0)];
                points_15[int(1)] = _S1398[int(1)];
                points_15[int(2)] = _S1399[int(2)];
                points_15[int(3)] = _S1400[int(3)];
                _S1368 = _S1367;
            }
            if(_S1332)
            {
                FixedArray<float3 , 4>  _S1401 = points_15;
                float3  _S1402 = _S1340 * points_15[int(1)];
                float _S1403 = _S1402.x + _S1402.y + _S1402.z;
                float4  _S1404 = _S1367;
                *&((&_S1404)->y) = _S1403;
                float4  _S1405 = _S1368 + _S1404;
                points_15[int(0)] = _S1328;
                points_15[int(1)] = _S1328;
                points_15[int(2)] = _S1328;
                points_15[int(3)] = _S1328;
                _S1333 = _S1401[int(0)];
                _S1368 = _S1405;
            }
            else
            {
                FixedArray<float3 , 4>  _S1406 = points_15;
                FixedArray<float3 , 4>  _S1407 = points_15;
                FixedArray<float3 , 4>  _S1408 = points_15;
                points_15[int(0)] = points_15[int(0)];
                points_15[int(1)] = _S1406[int(1)];
                points_15[int(2)] = _S1407[int(2)];
                points_15[int(3)] = _S1408[int(3)];
                _S1333 = _S1328;
            }
            float3  _S1409 = _S1341 * (points_15[int(0)] + _S1333);
            float _S1410 = _S1409.x + _S1409.y + _S1409.z;
            float4  _S1411 = _S1367;
            *&((&_S1411)->x) = _S1410;
            _S1368 = _S1368 + _S1411;
        }
        else
        {
            _S1368 = _S1367;
        }
    }
    else
    {
        _S1368 = _S1367;
    }
    *v_depths_6 = _S1368;
    return;
}

inline __device__ float ray_depth_to_linear_depth_factor_rational(float2  pix_center_17, float4  intrins_29, FixedArray<float, 8>  dist_coeffs_33, int camera_model_31)
{
    float _S1412;
    for(;;)
    {
        float2  uv_85 = (pix_center_17 - float2 {intrins_29.z, intrins_29.w}) / float2 {intrins_29.x, intrins_29.y};
        FixedArray<float, 8>  _S1413 = dist_coeffs_33;
        float2  uv_u_42;
        bool _S1414 = undistort_point_3(uv_85, &_S1413, int(12), &uv_u_42);
        if(!_S1414)
        {
            _S1412 = 0.0f;
            break;
        }
        float3  raydir_23 = unproject_raydir_0(uv_u_42, camera_model_31, false);
        _S1412 = float((F32_sign((raydir_23.z)))) / length_0(raydir_23);
        break;
    }
    return _S1412;
}

inline __device__ float depth_normal_loss_rational(float2  pix_center_18, float4  intrins_30, FixedArray<float, 8>  dist_coeffs_34, int camera_model_32, bool is_ray_depth_28, float4  depths_14, float3  gt_normal_6)
{
    float _S1415;
    for(;;)
    {
        float3  _S1416;
        float3  * _S1417;
        float3  * _S1418;
        float3  * _S1419;
        float3  * _S1420;
        int _S1421;
        FixedArray<float3 , 5>  points_16;
        for(;;)
        {
            float2  _S1422 = float2 {intrins_30.z, intrins_30.w};
            float2  _S1423 = float2 {intrins_30.x, intrins_30.y};
            float2  uv_86 = (pix_center_18 + make_float2 (-1.0f, -0.0f) - _S1422) / _S1423;
            FixedArray<float, 8>  _S1424 = dist_coeffs_34;
            float2  uv_u_43;
            bool _S1425 = undistort_point_3(uv_86, &_S1424, int(12), &uv_u_43);
            float3  _S1426 = make_float3 (0.0f);
            if(!_S1425)
            {
                _S1421 = int(0);
                _S1420 = nullptr;
                _S1419 = nullptr;
                _S1418 = nullptr;
                _S1417 = nullptr;
                _S1416 = _S1426;
                break;
            }
            float3  raydir_24 = unproject_raydir_0(uv_u_43, camera_model_32, is_ray_depth_28);
            points_16[int(0)] = make_float3 (depths_14.x) * raydir_24;
            float2  uv_87 = (pix_center_18 + make_float2 (1.0f, -0.0f) - _S1422) / _S1423;
            FixedArray<float, 8>  _S1427 = dist_coeffs_34;
            float2  uv_u_44;
            bool _S1428 = undistort_point_3(uv_87, &_S1427, int(12), &uv_u_44);
            if(!_S1428)
            {
                _S1421 = int(0);
                _S1420 = nullptr;
                _S1419 = &points_16[int(0)];
                _S1418 = nullptr;
                _S1417 = nullptr;
                _S1416 = _S1426;
                break;
            }
            float3  raydir_25 = unproject_raydir_0(uv_u_44, camera_model_32, is_ray_depth_28);
            points_16[int(1)] = make_float3 (depths_14.y) * raydir_25;
            float2  uv_88 = (pix_center_18 + make_float2 (0.0f, -1.0f) - _S1422) / _S1423;
            FixedArray<float, 8>  _S1429 = dist_coeffs_34;
            float2  uv_u_45;
            bool _S1430 = undistort_point_3(uv_88, &_S1429, int(12), &uv_u_45);
            if(!_S1430)
            {
                _S1421 = int(0);
                _S1420 = &points_16[int(1)];
                _S1419 = &points_16[int(0)];
                _S1418 = nullptr;
                _S1417 = nullptr;
                _S1416 = _S1426;
                break;
            }
            float3  raydir_26 = unproject_raydir_0(uv_u_45, camera_model_32, is_ray_depth_28);
            points_16[int(2)] = make_float3 (depths_14.z) * raydir_26;
            float2  uv_89 = (pix_center_18 + make_float2 (0.0f, 1.0f) - _S1422) / _S1423;
            FixedArray<float, 8>  _S1431 = dist_coeffs_34;
            float2  uv_u_46;
            bool _S1432 = undistort_point_3(uv_89, &_S1431, int(12), &uv_u_46);
            if(!_S1432)
            {
                _S1421 = int(0);
                _S1420 = &points_16[int(1)];
                _S1419 = &points_16[int(0)];
                _S1418 = nullptr;
                _S1417 = &points_16[int(2)];
                _S1416 = _S1426;
                break;
            }
            float3  raydir_27 = unproject_raydir_0(uv_u_46, camera_model_32, is_ray_depth_28);
            points_16[int(3)] = make_float3 (depths_14.w) * raydir_27;
            float2  uv_90 = (pix_center_18 + make_float2 (0.0f) * make_float2 (0.0f, 3.0f) - _S1422) / _S1423;
            FixedArray<float, 8>  _S1433 = dist_coeffs_34;
            float2  uv_u_47;
            bool _S1434 = undistort_point_3(uv_90, &_S1433, int(12), &uv_u_47);
            if(!_S1434)
            {
                _S1421 = int(0);
                _S1420 = &points_16[int(1)];
                _S1419 = &points_16[int(0)];
                _S1418 = &points_16[int(3)];
                _S1417 = &points_16[int(2)];
                _S1416 = _S1426;
                break;
            }
            float3  raydir_28 = unproject_raydir_0(uv_u_47, camera_model_32, is_ray_depth_28);
            _S1421 = int(1);
            _S1420 = &points_16[int(1)];
            _S1419 = &points_16[int(0)];
            _S1418 = &points_16[int(3)];
            _S1417 = &points_16[int(2)];
            _S1416 = raydir_28;
            break;
        }
        if(_S1421 != int(1))
        {
            _S1415 = 0.0f;
            break;
        }
        float3  normal_19 = cross_0(*_S1420 - *_S1419, - (*_S1418 - *_S1417));
        float3  normal_20;
        if((dot_0(normal_19, normal_19)) != 0.0f)
        {
            normal_20 = normalize_0(normal_19);
        }
        else
        {
            normal_20 = normal_19;
        }
        float3  _S1435;
        if((dot_0(gt_normal_6, gt_normal_6)) != 0.0f)
        {
            _S1435 = normalize_0(gt_normal_6);
        }
        else
        {
            _S1435 = gt_normal_6;
        }
        _S1415 = (1.0f - dot_0(normal_20, _S1435) + 0.00100000004749745f) / ((F32_max((dot_0(normal_20, - normalize_0(_S1416))), (0.0f))) + 0.00100000004749745f);
        break;
    }
    return _S1415;
}

struct s_bwd_prop_depth_normal_loss_Intermediates_3
{
    float2  _S1436;
    bool _S1437;
    float2  _S1438;
    bool _S1439;
    float2  _S1440;
    bool _S1441;
    float2  _S1442;
    bool _S1443;
    float2  _S1444;
    bool _S1445;
};

inline __device__ void depth_normal_loss_vjp_rational(float2  pix_center_19, float4  intrins_31, FixedArray<float, 8>  dist_coeffs_35, int camera_model_33, bool is_ray_depth_29, float4  depths_15, float3  gt_normal_7, float v_loss_3, float4  * v_depths_7, float3  * v_gt_normal_3)
{
    float2  _S1446 = make_float2 (0.0f);
    s_bwd_prop_depth_normal_loss_Intermediates_3 _S1447;
    (&_S1447)->_S1436 = _S1446;
    (&_S1447)->_S1437 = false;
    (&_S1447)->_S1438 = _S1446;
    (&_S1447)->_S1439 = false;
    (&_S1447)->_S1440 = _S1446;
    (&_S1447)->_S1441 = false;
    (&_S1447)->_S1442 = _S1446;
    (&_S1447)->_S1443 = false;
    (&_S1447)->_S1444 = _S1446;
    (&_S1447)->_S1445 = false;
    (&_S1447)->_S1438 = _S1446;
    (&_S1447)->_S1439 = false;
    (&_S1447)->_S1440 = _S1446;
    (&_S1447)->_S1441 = false;
    (&_S1447)->_S1442 = _S1446;
    (&_S1447)->_S1443 = false;
    (&_S1447)->_S1444 = _S1446;
    (&_S1447)->_S1445 = false;
    float2  _S1448 = float2 {intrins_31.z, intrins_31.w};
    float2  _S1449 = float2 {intrins_31.x, intrins_31.y};
    float2  uv_91 = (pix_center_19 + make_float2 (-1.0f, -0.0f) - _S1448) / _S1449;
    float2  _S1450 = _S1446;
    FixedArray<float, 8>  _S1451 = dist_coeffs_35;
    bool _S1452 = undistort_point_3(uv_91, &_S1451, int(12), &_S1450);
    (&_S1447)->_S1436 = _S1450;
    (&_S1447)->_S1437 = _S1452;
    bool _S1453 = !!_S1452;
    bool _runFlag_28;
    if(_S1453)
    {
        float2  uv_92 = (pix_center_19 + make_float2 (1.0f, -0.0f) - _S1448) / _S1449;
        float2  _S1454 = _S1446;
        FixedArray<float, 8>  _S1455 = dist_coeffs_35;
        bool _S1456 = undistort_point_3(uv_92, &_S1455, int(12), &_S1454);
        (&_S1447)->_S1438 = _S1454;
        (&_S1447)->_S1439 = _S1456;
        if(!_S1456)
        {
            _runFlag_28 = false;
        }
        else
        {
            _runFlag_28 = _S1453;
        }
        if(_runFlag_28)
        {
            float2  uv_93 = (pix_center_19 + make_float2 (0.0f, -1.0f) - _S1448) / _S1449;
            float2  _S1457 = _S1446;
            FixedArray<float, 8>  _S1458 = dist_coeffs_35;
            bool _S1459 = undistort_point_3(uv_93, &_S1458, int(12), &_S1457);
            (&_S1447)->_S1440 = _S1457;
            (&_S1447)->_S1441 = _S1459;
            if(!_S1459)
            {
                _runFlag_28 = false;
            }
            if(_runFlag_28)
            {
                float2  uv_94 = (pix_center_19 + make_float2 (0.0f, 1.0f) - _S1448) / _S1449;
                float2  _S1460 = _S1446;
                FixedArray<float, 8>  _S1461 = dist_coeffs_35;
                bool _S1462 = undistort_point_3(uv_94, &_S1461, int(12), &_S1460);
                (&_S1447)->_S1442 = _S1460;
                (&_S1447)->_S1443 = _S1462;
                if(!_S1462)
                {
                    _runFlag_28 = false;
                }
                if(_runFlag_28)
                {
                    float2  uv_95 = (pix_center_19 - _S1448) / _S1449;
                    float2  _S1463 = _S1446;
                    FixedArray<float, 8>  _S1464 = dist_coeffs_35;
                    bool _S1465 = undistort_point_3(uv_95, &_S1464, int(12), &_S1463);
                    (&_S1447)->_S1444 = _S1463;
                    (&_S1447)->_S1445 = _S1465;
                }
            }
        }
    }
    s_bwd_prop_depth_normal_loss_Intermediates_3 _S1466 = _S1447;
    float3  _S1467 = make_float3 (0.0f);
    bool _S1468 = !!_S1447._S1437;
    bool _runFlag_29;
    bool _runFlag_30;
    bool _runFlag_31;
    int _S1469;
    float3  raydir_29;
    float3  _S1470;
    float3  _S1471;
    float3  _S1472;
    float3  _S1473;
    FixedArray<float3 , 5>  points_17;
    if(_S1468)
    {
        float3  _S1474 = s_primal_ctx_unproject_raydir_0(_S1466._S1436, camera_model_33, is_ray_depth_29);
        float3  _S1475 = make_float3 (depths_15.x) * _S1474;
        if(!_S1466._S1439)
        {
            _runFlag_28 = false;
        }
        else
        {
            _runFlag_28 = _S1468;
        }
        if(_runFlag_28)
        {
            float3  _S1476 = s_primal_ctx_unproject_raydir_0(_S1466._S1438, camera_model_33, is_ray_depth_29);
            float3  _S1477 = make_float3 (depths_15.y) * _S1476;
            if(!_S1466._S1441)
            {
                _runFlag_29 = false;
            }
            else
            {
                _runFlag_29 = _runFlag_28;
            }
            if(_runFlag_29)
            {
                float3  _S1478 = s_primal_ctx_unproject_raydir_0(_S1466._S1440, camera_model_33, is_ray_depth_29);
                float3  _S1479 = make_float3 (depths_15.z) * _S1478;
                if(!_S1466._S1443)
                {
                    _runFlag_30 = false;
                }
                else
                {
                    _runFlag_30 = _runFlag_29;
                }
                if(_runFlag_30)
                {
                    float3  _S1480 = s_primal_ctx_unproject_raydir_0(_S1466._S1442, camera_model_33, is_ray_depth_29);
                    float3  _S1481 = make_float3 (depths_15.w) * _S1480;
                    if(!_S1466._S1445)
                    {
                        _runFlag_31 = false;
                    }
                    else
                    {
                        _runFlag_31 = _runFlag_30;
                    }
                    if(_runFlag_31)
                    {
                        float3  _S1482 = s_primal_ctx_unproject_raydir_0(_S1466._S1444, camera_model_33, is_ray_depth_29);
                        _S1469 = int(1);
                        raydir_29 = _S1482;
                    }
                    else
                    {
                        _S1469 = int(0);
                        raydir_29 = _S1480;
                    }
                    points_17[int(0)] = _S1475;
                    points_17[int(1)] = _S1477;
                    points_17[int(2)] = _S1479;
                    points_17[int(3)] = _S1481;
                    points_17[int(4)] = _S1467;
                    _S1470 = _S1480;
                }
                else
                {
                    _S1469 = int(0);
                    raydir_29 = _S1478;
                    points_17[int(0)] = _S1475;
                    points_17[int(1)] = _S1477;
                    points_17[int(2)] = _S1479;
                    points_17[int(3)] = _S1467;
                    points_17[int(4)] = _S1467;
                    _S1470 = _S1467;
                }
                _S1471 = _S1478;
            }
            else
            {
                _S1469 = int(0);
                raydir_29 = _S1476;
                points_17[int(0)] = _S1475;
                points_17[int(1)] = _S1477;
                points_17[int(2)] = _S1467;
                points_17[int(3)] = _S1467;
                points_17[int(4)] = _S1467;
                _runFlag_30 = false;
                _S1470 = _S1467;
                _S1471 = _S1467;
            }
            _S1472 = _S1476;
        }
        else
        {
            _S1469 = int(0);
            raydir_29 = _S1474;
            points_17[int(0)] = _S1475;
            points_17[int(1)] = _S1467;
            points_17[int(2)] = _S1467;
            points_17[int(3)] = _S1467;
            points_17[int(4)] = _S1467;
            _runFlag_29 = false;
            _runFlag_30 = false;
            _S1470 = _S1467;
            _S1471 = _S1467;
            _S1472 = _S1467;
        }
        _S1473 = _S1474;
    }
    else
    {
        _S1469 = int(0);
        points_17[int(0)] = _S1467;
        points_17[int(1)] = _S1467;
        points_17[int(2)] = _S1467;
        points_17[int(3)] = _S1467;
        points_17[int(4)] = _S1467;
        _runFlag_28 = false;
        _runFlag_29 = false;
        _runFlag_30 = false;
        _S1470 = _S1467;
        _S1471 = _S1467;
        _S1472 = _S1467;
        _S1473 = _S1467;
    }
    bool _S1483 = !(_S1469 != int(1));
    bool _S1484;
    float3  normal_21;
    float3  _S1485;
    float3  _S1486;
    float3  _S1487;
    float3  _S1488;
    float _S1489;
    float _S1490;
    float _S1491;
    float _S1492;
    if(_S1483)
    {
        float3  dx_8 = points_17[int(1)] - points_17[int(0)];
        float3  _S1493 = - (points_17[int(3)] - points_17[int(2)]);
        float3  _S1494 = s_primal_ctx_cross_0(dx_8, _S1493);
        bool _S1495 = (s_primal_ctx_dot_0(_S1494, _S1494)) != 0.0f;
        if(_S1495)
        {
            normal_21 = normalize_0(_S1494);
        }
        else
        {
            normal_21 = _S1494;
        }
        bool _S1496 = (s_primal_ctx_dot_0(gt_normal_7, gt_normal_7)) != 0.0f;
        if(_S1496)
        {
            _S1485 = normalize_0(gt_normal_7);
        }
        else
        {
            _S1485 = gt_normal_7;
        }
        float3  _S1497 = - normalize_0(raydir_29);
        float _S1498 = s_primal_ctx_dot_0(normal_21, _S1497);
        float _S1499 = 1.0f - s_primal_ctx_dot_0(normal_21, _S1485) + 0.00100000004749745f;
        float _S1500 = (F32_max((_S1498), (0.0f))) + 0.00100000004749745f;
        _S1489 = _S1500 * _S1500;
        _S1490 = _S1499;
        _S1491 = _S1500;
        _S1492 = _S1498;
        raydir_29 = normal_21;
        normal_21 = _S1497;
        _runFlag_31 = _S1496;
        _S1484 = _S1495;
        _S1486 = _S1494;
        _S1487 = dx_8;
        _S1488 = _S1493;
    }
    else
    {
        _S1489 = 0.0f;
        _S1490 = 0.0f;
        _S1491 = 0.0f;
        _S1492 = 0.0f;
        raydir_29 = _S1467;
        normal_21 = _S1467;
        _S1485 = _S1467;
        _runFlag_31 = false;
        _S1484 = false;
        _S1486 = _S1467;
        _S1487 = _S1467;
        _S1488 = _S1467;
    }
    float4  _S1501 = make_float4 (0.0f);
    if(_S1483)
    {
        float _S1502 = v_loss_3 / _S1489;
        float _S1503 = _S1490 * - _S1502;
        float s_diff_num_T_3 = _S1491 * _S1502;
        DiffPair_float_0 _S1504;
        (&_S1504)->primal_0 = _S1492;
        (&_S1504)->differential_0 = 0.0f;
        DiffPair_float_0 _S1505;
        (&_S1505)->primal_0 = 0.0f;
        (&_S1505)->differential_0 = 0.0f;
        _d_max_0(&_S1504, &_S1505, _S1503);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1506;
        (&_S1506)->primal_0 = raydir_29;
        (&_S1506)->differential_0 = _S1467;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1507;
        (&_S1507)->primal_0 = normal_21;
        (&_S1507)->differential_0 = _S1467;
        s_bwd_prop_dot_0(&_S1506, &_S1507, _S1504.differential_0);
        float _S1508 = - s_diff_num_T_3;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1509;
        (&_S1509)->primal_0 = raydir_29;
        (&_S1509)->differential_0 = _S1467;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1510;
        (&_S1510)->primal_0 = _S1485;
        (&_S1510)->differential_0 = _S1467;
        s_bwd_prop_dot_0(&_S1509, &_S1510, _S1508);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1511 = _S1510;
        float3  _S1512 = _S1506.differential_0 + _S1509.differential_0;
        if(_runFlag_31)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1513;
            (&_S1513)->primal_0 = gt_normal_7;
            (&_S1513)->differential_0 = _S1467;
            s_bwd_normalize_impl_0(&_S1513, _S1511.differential_0);
            raydir_29 = _S1513.differential_0;
        }
        else
        {
            raydir_29 = _S1511.differential_0;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1514;
        (&_S1514)->primal_0 = gt_normal_7;
        (&_S1514)->differential_0 = _S1467;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1515;
        (&_S1515)->primal_0 = gt_normal_7;
        (&_S1515)->differential_0 = _S1467;
        s_bwd_prop_dot_0(&_S1514, &_S1515, 0.0f);
        float3  _S1516 = _S1515.differential_0 + _S1514.differential_0 + raydir_29;
        if(_S1484)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1517;
            (&_S1517)->primal_0 = _S1486;
            (&_S1517)->differential_0 = _S1467;
            s_bwd_normalize_impl_0(&_S1517, _S1512);
            raydir_29 = _S1517.differential_0;
        }
        else
        {
            raydir_29 = _S1512;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1518;
        (&_S1518)->primal_0 = _S1486;
        (&_S1518)->differential_0 = _S1467;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1519;
        (&_S1519)->primal_0 = _S1486;
        (&_S1519)->differential_0 = _S1467;
        s_bwd_prop_dot_0(&_S1518, &_S1519, 0.0f);
        float3  _S1520 = _S1519.differential_0 + _S1518.differential_0 + raydir_29;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1521;
        (&_S1521)->primal_0 = _S1487;
        (&_S1521)->differential_0 = _S1467;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1522;
        (&_S1522)->primal_0 = _S1488;
        (&_S1522)->differential_0 = _S1467;
        s_bwd_prop_cross_0(&_S1521, &_S1522, _S1520);
        float3  s_diff_dy_T_8 = - _S1522.differential_0;
        float3  _S1523 = - s_diff_dy_T_8;
        float3  _S1524 = - _S1521.differential_0;
        FixedArray<float3 , 5>  _S1525;
        _S1525[int(0)] = _S1467;
        _S1525[int(1)] = _S1467;
        _S1525[int(2)] = _S1467;
        _S1525[int(3)] = _S1467;
        _S1525[int(4)] = _S1467;
        _S1525[int(2)] = _S1523;
        _S1525[int(3)] = s_diff_dy_T_8;
        _S1525[int(0)] = _S1524;
        _S1525[int(1)] = _S1521.differential_0;
        points_17[int(0)] = _S1525[int(0)];
        points_17[int(1)] = _S1525[int(1)];
        points_17[int(2)] = _S1525[int(2)];
        points_17[int(3)] = _S1525[int(3)];
        points_17[int(4)] = _S1525[int(4)];
        raydir_29 = _S1516;
    }
    else
    {
        points_17[int(0)] = _S1467;
        points_17[int(1)] = _S1467;
        points_17[int(2)] = _S1467;
        points_17[int(3)] = _S1467;
        points_17[int(4)] = _S1467;
        raydir_29 = _S1467;
    }
    float4  _S1526;
    if(_S1468)
    {
        if(_runFlag_28)
        {
            if(_runFlag_29)
            {
                if(_runFlag_30)
                {
                    FixedArray<float3 , 5>  _S1527 = points_17;
                    FixedArray<float3 , 5>  _S1528 = points_17;
                    FixedArray<float3 , 5>  _S1529 = points_17;
                    float3  _S1530 = _S1470 * points_17[int(3)];
                    float _S1531 = _S1530.x + _S1530.y + _S1530.z;
                    float4  _S1532 = _S1501;
                    *&((&_S1532)->w) = _S1531;
                    points_17[int(0)] = _S1467;
                    points_17[int(1)] = _S1467;
                    points_17[int(2)] = _S1467;
                    points_17[int(3)] = _S1467;
                    points_17[int(4)] = _S1467;
                    _S1470 = _S1529[int(2)];
                    normal_21 = _S1527[int(0)];
                    _S1485 = _S1528[int(1)];
                    _S1526 = _S1532;
                }
                else
                {
                    FixedArray<float3 , 5>  _S1533 = points_17;
                    FixedArray<float3 , 5>  _S1534 = points_17;
                    FixedArray<float3 , 5>  _S1535 = points_17;
                    FixedArray<float3 , 5>  _S1536 = points_17;
                    points_17[int(0)] = points_17[int(0)];
                    points_17[int(1)] = _S1533[int(1)];
                    points_17[int(2)] = _S1534[int(2)];
                    points_17[int(3)] = _S1535[int(3)];
                    points_17[int(4)] = _S1536[int(4)];
                    _S1470 = _S1467;
                    normal_21 = _S1467;
                    _S1485 = _S1467;
                    _S1526 = _S1501;
                }
                float3  _S1537 = _S1471 * (points_17[int(2)] + _S1470);
                float _S1538 = _S1537.x + _S1537.y + _S1537.z;
                float3  _S1539 = points_17[int(0)] + normal_21;
                float3  _S1540 = points_17[int(1)] + _S1485;
                float4  _S1541 = _S1501;
                *&((&_S1541)->z) = _S1538;
                float4  _S1542 = _S1526 + _S1541;
                points_17[int(0)] = _S1467;
                points_17[int(1)] = _S1467;
                points_17[int(2)] = _S1467;
                points_17[int(3)] = _S1467;
                points_17[int(4)] = _S1467;
                _S1470 = _S1540;
                _S1471 = _S1539;
                _S1526 = _S1542;
            }
            else
            {
                FixedArray<float3 , 5>  _S1543 = points_17;
                FixedArray<float3 , 5>  _S1544 = points_17;
                FixedArray<float3 , 5>  _S1545 = points_17;
                FixedArray<float3 , 5>  _S1546 = points_17;
                points_17[int(0)] = points_17[int(0)];
                points_17[int(1)] = _S1543[int(1)];
                points_17[int(2)] = _S1544[int(2)];
                points_17[int(3)] = _S1545[int(3)];
                points_17[int(4)] = _S1546[int(4)];
                _S1470 = _S1467;
                _S1471 = _S1467;
                _S1526 = _S1501;
            }
            float3  _S1547 = _S1472 * (points_17[int(1)] + _S1470);
            float _S1548 = _S1547.x + _S1547.y + _S1547.z;
            float3  _S1549 = points_17[int(0)] + _S1471;
            float4  _S1550 = _S1501;
            *&((&_S1550)->y) = _S1548;
            float4  _S1551 = _S1526 + _S1550;
            points_17[int(0)] = _S1467;
            points_17[int(1)] = _S1467;
            points_17[int(2)] = _S1467;
            points_17[int(3)] = _S1467;
            points_17[int(4)] = _S1467;
            _S1470 = _S1549;
            _S1526 = _S1551;
        }
        else
        {
            FixedArray<float3 , 5>  _S1552 = points_17;
            FixedArray<float3 , 5>  _S1553 = points_17;
            FixedArray<float3 , 5>  _S1554 = points_17;
            FixedArray<float3 , 5>  _S1555 = points_17;
            points_17[int(0)] = points_17[int(0)];
            points_17[int(1)] = _S1552[int(1)];
            points_17[int(2)] = _S1553[int(2)];
            points_17[int(3)] = _S1554[int(3)];
            points_17[int(4)] = _S1555[int(4)];
            _S1470 = _S1467;
            _S1526 = _S1501;
        }
        float3  _S1556 = _S1473 * (points_17[int(0)] + _S1470);
        float _S1557 = _S1556.x + _S1556.y + _S1556.z;
        float4  _S1558 = _S1501;
        *&((&_S1558)->x) = _S1557;
        _S1526 = _S1526 + _S1558;
    }
    else
    {
        _S1526 = _S1501;
    }
    *v_depths_7 = _S1526;
    *v_gt_normal_3 = raydir_29;
    return;
}

