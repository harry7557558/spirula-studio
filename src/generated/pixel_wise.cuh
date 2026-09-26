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

inline __device__ float dlogm_curve_to_linear_0(float code_0, float x_shift_0, float y_shift_0, float scale_1, float slope_0, float slope2_0, float intercept_0, float mid_gray_0)
{
    float t_0 = (F32_exp2((scale_1 * code_0 + y_shift_0))) + x_shift_0;
    float pw_0;
    if(t_0 < (intercept_0 / (slope2_0 - slope_0)))
    {
        pw_0 = t_0 * slope_0 + intercept_0;
    }
    else
    {
        pw_0 = t_0 * slope2_0;
    }
    return pw_0 * mid_gray_0;
}

inline __device__ float dlogm_osmo360_to_linear_0(float code_1)
{
    return dlogm_curve_to_linear_0(code_1, -2.36086249351501465f, 0.63083583116531372f, 6.69145584106445312f, 1.01188600063323975f, 3.03565812110900879f, 0.82205605506896973f, 0.00786506105214357f);
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

inline __device__ float3  dlogm_osmo360_to_rec2020(float3  code_2)
{
    return mul_0(makeMatrix<float, 3, 3> (0.80726855993270874f, 0.15266364812850952f, 0.04006779193878174f, 0.0428781621158123f, 0.9907376766204834f, -0.0336158275604248f, -0.00960387196391821f, -0.09426373988389969f, 1.10386765003204346f), make_float3 (dlogm_osmo360_to_linear_0(code_2.x), dlogm_osmo360_to_linear_0(code_2.y), dlogm_osmo360_to_linear_0(code_2.z)));
}

inline __device__ float dlogm_avata360_to_linear_0(float code_3)
{
    return dlogm_curve_to_linear_0(code_3, -2.06424880027770996f, 0.63083583116531372f, 3.62298870086669922f, 1.01188600063323975f, 3.03565812110900879f, 0.52191758155822754f, 0.02740182168781757f);
}

inline __device__ float3  dlogm_avata360_to_rec2020(float3  code_4)
{
    return mul_0(makeMatrix<float, 3, 3> (0.70652478933334351f, 0.17651677131652832f, 0.11695844680070877f, -0.18369156122207642f, 1.11619126796722412f, 0.0675002858042717f, -0.30225974321365356f, 0.03349689766764641f, 1.26876282691955566f), make_float3 (dlogm_avata360_to_linear_0(code_4.x), dlogm_avata360_to_linear_0(code_4.y), dlogm_avata360_to_linear_0(code_4.z)));
}

inline __device__ float3  input_curve_to_rec2020(int curve_0, float3  code_5)
{
    float3  _S41;
    if(curve_0 == int(2))
    {
        _S41 = dlogm_avata360_to_rec2020(code_5);
    }
    else
    {
        _S41 = dlogm_osmo360_to_rec2020(code_5);
    }
    return _S41;
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
    float t_1 = xfer_max0_0(x_10 - 0.00400000018998981f);
    float _S42 = 6.19999980926513672f * t_1;
    return t_1 * (_S42 + 0.5f) / (t_1 * (_S42 + 1.70000004768371582f) + 0.05999999865889549f);
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
    float _S43 = 0.15000000596046448f * x_14;
    return (x_14 * (_S43 + 0.05000000074505806f) + 0.00400000018998981f) / (x_14 * (_S43 + 0.5f) + 0.06000000238418579f) - 0.06666666269302368f;
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
    float _S44;
    if(transfer_0 == int(2))
    {
        float _S45 = xfer_clamp01_0(xfer_aces_0(xfer_max0_0(x_16)));
        if(_S45 < 0.00313080009073019f)
        {
            _S44 = _S45 * 12.92000007629394531f;
        }
        else
        {
            _S44 = 1.0549999475479126f * (F32_pow((_S45), (0.4166666567325592f))) - 0.05499999970197678f;
        }
        return _S44;
    }
    if(transfer_0 == int(4))
    {
        float _S46 = xfer_clamp01_0(xfer_uncharted2_0(x_16));
        if(_S46 < 0.00313080009073019f)
        {
            _S44 = _S46 * 12.92000007629394531f;
        }
        else
        {
            _S44 = 1.0549999475479126f * (F32_pow((_S46), (0.4166666567325592f))) - 0.05499999970197678f;
        }
        return _S44;
    }
    if(transfer_0 == int(1))
    {
        float _S47 = xfer_clamp01_0(x_16);
        if(_S47 < 0.00313080009073019f)
        {
            _S44 = _S47 * 12.92000007629394531f;
        }
        else
        {
            _S44 = 1.0549999475479126f * (F32_pow((_S47), (0.4166666567325592f))) - 0.05499999970197678f;
        }
        return _S44;
    }
    float _S48 = xfer_max0_0(x_16);
    if(_S48 < 0.00313080009073019f)
    {
        _S44 = _S48 * 12.92000007629394531f;
    }
    else
    {
        _S44 = 1.0549999475479126f * (F32_pow((_S48), (0.4166666567325592f))) - 0.05499999970197678f;
    }
    return _S44;
}

inline __device__ float3  working_to_display(float3  rgb_2, Matrix<float, 3, 3>  color_matrix_0, int transfer_1, bool is_linear_0)
{
    float3  _S49;
    if(!is_linear_0)
    {
        float _S50 = rgb_2.x;
        float _S51;
        if(_S50 < 0.04044999927282333f)
        {
            _S51 = _S50 * 0.07739938050508499f;
        }
        else
        {
            _S51 = (F32_pow((0.94786733388900757f * (_S50 + 0.05499999970197678f)), (2.40000009536743164f)));
        }
        float _S52 = rgb_2.y;
        float _S53;
        if(_S52 < 0.04044999927282333f)
        {
            _S53 = _S52 * 0.07739938050508499f;
        }
        else
        {
            _S53 = (F32_pow((0.94786733388900757f * (_S52 + 0.05499999970197678f)), (2.40000009536743164f)));
        }
        float _S54 = rgb_2.z;
        float _S55;
        if(_S54 < 0.04044999927282333f)
        {
            _S55 = _S54 * 0.07739938050508499f;
        }
        else
        {
            _S55 = (F32_pow((0.94786733388900757f * (_S54 + 0.05499999970197678f)), (2.40000009536743164f)));
        }
        _S49 = make_float3 (_S51, _S53, _S55);
    }
    else
    {
        _S49 = rgb_2;
    }
    float3  _S56 = mul_0(color_matrix_0, _S49);
    return make_float3 (tone_encode_0(_S56.x, transfer_1), tone_encode_0(_S56.y, transfer_1), tone_encode_0(_S56.z, transfer_1));
}

inline __device__ float s_primal_ctx_pow_0(float _S57, float _S58)
{
    return (F32_pow((_S57), (_S58)));
}

inline __device__ float3  s_primal_ctx_mul_0(Matrix<float, 3, 3>  _S59, float3  _S60)
{
    return mul_0(_S59, _S60);
}

inline __device__ float s_primal_ctx_xfer_max0_0(float _S61)
{
    return xfer_max0_0(_S61);
}

inline __device__ float s_primal_ctx_xfer_aces_0(float dpx_4)
{
    return dpx_4 * (2.50999999046325684f * dpx_4 + 0.02999999932944775f) / (dpx_4 * (2.43000006675720215f * dpx_4 + 0.5899999737739563f) + 0.14000000059604645f);
}

inline __device__ float s_primal_ctx_xfer_clamp01_0(float _S62)
{
    return xfer_clamp01_0(_S62);
}

inline __device__ float s_primal_ctx_xfer_hable_0(float dpx_5)
{
    float _S63 = 0.15000000596046448f * dpx_5;
    return (dpx_5 * (_S63 + 0.05000000074505806f) + 0.00400000018998981f) / (dpx_5 * (_S63 + 0.5f) + 0.06000000238418579f) - 0.06666666269302368f;
}

inline __device__ float s_primal_ctx_xfer_uncharted2_0(float dpx_6)
{
    return s_primal_ctx_xfer_hable_0(s_primal_ctx_xfer_max0_0(dpx_6)) / s_primal_ctx_xfer_hable_0(11.19999980926513672f);
}

inline __device__ void s_bwd_prop_pow_0(DiffPair_float_0 * _S64, DiffPair_float_0 * _S65, float _S66)
{
    _d_pow_0(_S64, _S65, _S66);
    return;
}

inline __device__ void s_bwd_prop_xfer_max0_0(DiffPair_float_0 * _S67, float _S68)
{
    xfer_pass_grad_0(_S67, _S68);
    return;
}

inline __device__ void s_bwd_prop_xfer_clamp01_0(DiffPair_float_0 * _S69, float _S70)
{
    xfer_pass_grad_0(_S69, _S70);
    return;
}

inline __device__ void s_bwd_prop_xfer_hable_0(DiffPair_float_0 * dpx_7, float _s_dOut_1)
{
    float _S71 = 0.15000000596046448f * (*dpx_7).primal_0;
    float _S72 = _S71 + 0.05000000074505806f;
    float _S73 = _S71 + 0.5f;
    float _S74 = (*dpx_7).primal_0 * _S73 + 0.06000000238418579f;
    float _S75 = _s_dOut_1 / (_S74 * _S74);
    float _S76 = ((*dpx_7).primal_0 * _S72 + 0.00400000018998981f) * - _S75;
    float _S77 = _S74 * _S75;
    float _S78 = _S73 * _S76 + _S72 * _S77 + 0.15000000596046448f * ((*dpx_7).primal_0 * _S76 + (*dpx_7).primal_0 * _S77);
    dpx_7->primal_0 = (*dpx_7).primal_0;
    dpx_7->differential_0 = _S78;
    return;
}

inline __device__ void s_bwd_prop_xfer_uncharted2_0(DiffPair_float_0 * dpx_8, float _s_dOut_2)
{
    float _S79 = s_primal_ctx_xfer_hable_0(11.19999980926513672f);
    float _S80 = _S79 * (_s_dOut_2 / (_S79 * _S79));
    DiffPair_float_0 _S81;
    (&_S81)->primal_0 = s_primal_ctx_xfer_max0_0((*dpx_8).primal_0);
    (&_S81)->differential_0 = 0.0f;
    s_bwd_prop_xfer_hable_0(&_S81, _S80);
    DiffPair_float_0 _S82;
    (&_S82)->primal_0 = (*dpx_8).primal_0;
    (&_S82)->differential_0 = 0.0f;
    s_bwd_prop_xfer_max0_0(&_S82, _S81.differential_0);
    dpx_8->primal_0 = (*dpx_8).primal_0;
    dpx_8->differential_0 = _S82.differential_0;
    return;
}

inline __device__ void s_bwd_prop_xfer_aces_0(DiffPair_float_0 * dpx_9, float _s_dOut_3)
{
    float _S83 = 2.50999999046325684f * (*dpx_9).primal_0 + 0.02999999932944775f;
    float _S84 = 2.43000006675720215f * (*dpx_9).primal_0 + 0.5899999737739563f;
    float _S85 = (*dpx_9).primal_0 * _S84 + 0.14000000059604645f;
    float _S86 = _s_dOut_3 / (_S85 * _S85);
    float _S87 = (*dpx_9).primal_0 * _S83 * - _S86;
    float _S88 = _S85 * _S86;
    float _S89 = _S84 * _S87 + 2.43000006675720215f * ((*dpx_9).primal_0 * _S87) + _S83 * _S88 + 2.50999999046325684f * ((*dpx_9).primal_0 * _S88);
    dpx_9->primal_0 = (*dpx_9).primal_0;
    dpx_9->differential_0 = _S89;
    return;
}

inline __device__ void s_bwd_prop_xfer_filmic_0(DiffPair_float_0 * dpx_10, float _s_dOut_4)
{
    float _S90 = (*dpx_10).primal_0 - 0.00400000018998981f;
    float _S91 = s_primal_ctx_xfer_max0_0(_S90);
    float _S92 = 6.19999980926513672f * _S91;
    float _S93 = _S92 + 0.5f;
    float _S94 = _S92 + 1.70000004768371582f;
    float _S95 = _S91 * _S94 + 0.05999999865889549f;
    float _S96 = _s_dOut_4 / (_S95 * _S95);
    float _S97 = _S91 * _S93 * - _S96;
    float _S98 = _S95 * _S96;
    float _S99 = _S94 * _S97 + _S93 * _S98 + 6.19999980926513672f * (_S91 * _S97 + _S91 * _S98);
    DiffPair_float_0 _S100;
    (&_S100)->primal_0 = _S90;
    (&_S100)->differential_0 = 0.0f;
    s_bwd_prop_xfer_max0_0(&_S100, _S99);
    dpx_10->primal_0 = (*dpx_10).primal_0;
    dpx_10->differential_0 = _S100.differential_0;
    return;
}

inline __device__ void s_bwd_prop_tone_encode_0(DiffPair_float_0 * dpx_11, int transfer_2, float _s_dOut_5)
{
    DiffPair_float_0 _S101 = *dpx_11;
    bool _S102 = transfer_2 == int(3);
    bool _S103 = !_S102;
    bool _runFlag_0;
    bool _runFlag_1;
    bool _runFlag_2;
    bool _S104;
    bool _S105;
    bool _S106;
    float _S107;
    float _S108;
    float _S109;
    float _S110;
    float _S111;
    float _S112;
    float _S113;
    if(_S103)
    {
        bool _S114 = transfer_2 == int(2);
        if(_S114)
        {
            float _S115 = s_primal_ctx_xfer_max0_0(_S101.primal_0);
            float _S116 = s_primal_ctx_xfer_aces_0(_S115);
            float _S117 = s_primal_ctx_xfer_clamp01_0(_S116);
            _runFlag_0 = false;
            _S107 = _S117;
            _S108 = _S116;
            _S109 = _S115;
        }
        else
        {
            _runFlag_0 = _S103;
            _S107 = 0.0f;
            _S108 = 0.0f;
            _S109 = 0.0f;
        }
        if(_runFlag_0)
        {
            bool _S118 = transfer_2 == int(4);
            if(_S118)
            {
                float _S119 = s_primal_ctx_xfer_uncharted2_0(_S101.primal_0);
                float _S120 = s_primal_ctx_xfer_clamp01_0(_S119);
                _runFlag_1 = false;
                _S110 = _S120;
                _S111 = _S119;
            }
            else
            {
                _runFlag_1 = _runFlag_0;
                _S110 = 0.0f;
                _S111 = 0.0f;
            }
            if(_runFlag_1)
            {
                bool _S121 = transfer_2 == int(1);
                if(_S121)
                {
                    float _S122 = s_primal_ctx_xfer_clamp01_0(_S101.primal_0);
                    _runFlag_2 = false;
                    _S112 = _S122;
                }
                else
                {
                    _runFlag_2 = _runFlag_1;
                    _S112 = 0.0f;
                }
                if(_runFlag_2)
                {
                    _S113 = s_primal_ctx_xfer_max0_0(_S101.primal_0);
                }
                else
                {
                    _S113 = 0.0f;
                }
                float _S123 = _S112;
                _S112 = _S113;
                _S104 = _S121;
                _S113 = _S123;
            }
            else
            {
                _runFlag_2 = false;
                _S112 = 0.0f;
                _S104 = false;
                _S113 = 0.0f;
            }
            float _S124 = _S110;
            float _S125 = _S111;
            _S110 = _S112;
            _S111 = _S113;
            _S105 = _S118;
            _S112 = _S124;
            _S113 = _S125;
        }
        else
        {
            _runFlag_1 = false;
            _runFlag_2 = false;
            _S110 = 0.0f;
            _S104 = false;
            _S111 = 0.0f;
            _S105 = false;
            _S112 = 0.0f;
            _S113 = 0.0f;
        }
        float _S126 = _S107;
        float _S127 = _S108;
        float _S128 = _S109;
        _S107 = _S110;
        _S108 = _S111;
        _S109 = _S112;
        _S110 = _S113;
        _S106 = _S114;
        _S111 = _S126;
        _S112 = _S127;
        _S113 = _S128;
    }
    else
    {
        _runFlag_0 = false;
        _runFlag_1 = false;
        _runFlag_2 = false;
        _S107 = 0.0f;
        _S104 = false;
        _S108 = 0.0f;
        _S105 = false;
        _S109 = 0.0f;
        _S110 = 0.0f;
        _S106 = false;
        _S111 = 0.0f;
        _S112 = 0.0f;
        _S113 = 0.0f;
    }
    if(_S103)
    {
        if(_runFlag_0)
        {
            if(_runFlag_1)
            {
                float _S129;
                if(_runFlag_2)
                {
                    if(_S107 < 0.00313080009073019f)
                    {
                        _S107 = 12.92000007629394531f * _s_dOut_5;
                    }
                    else
                    {
                        float _S130 = 1.0549999475479126f * _s_dOut_5;
                        DiffPair_float_0 _S131;
                        (&_S131)->primal_0 = _S107;
                        (&_S131)->differential_0 = 0.0f;
                        DiffPair_float_0 _S132;
                        (&_S132)->primal_0 = 0.4166666567325592f;
                        (&_S132)->differential_0 = 0.0f;
                        s_bwd_prop_pow_0(&_S131, &_S132, _S130);
                        _S107 = _S131.differential_0;
                    }
                    DiffPair_float_0 _S133;
                    (&_S133)->primal_0 = _S101.primal_0;
                    (&_S133)->differential_0 = 0.0f;
                    s_bwd_prop_xfer_max0_0(&_S133, _S107);
                    _S107 = 0.0f;
                    _S129 = _S133.differential_0;
                }
                else
                {
                    _S107 = _s_dOut_5;
                    _S129 = 0.0f;
                }
                if(_S104)
                {
                    if(_S108 < 0.00313080009073019f)
                    {
                        _S107 = 12.92000007629394531f * _S107;
                    }
                    else
                    {
                        float _S134 = 1.0549999475479126f * _S107;
                        DiffPair_float_0 _S135;
                        (&_S135)->primal_0 = _S108;
                        (&_S135)->differential_0 = 0.0f;
                        DiffPair_float_0 _S136;
                        (&_S136)->primal_0 = 0.4166666567325592f;
                        (&_S136)->differential_0 = 0.0f;
                        s_bwd_prop_pow_0(&_S135, &_S136, _S134);
                        _S107 = _S135.differential_0;
                    }
                    DiffPair_float_0 _S137;
                    (&_S137)->primal_0 = _S101.primal_0;
                    (&_S137)->differential_0 = 0.0f;
                    s_bwd_prop_xfer_clamp01_0(&_S137, _S107);
                    float _S138 = _S137.differential_0 + _S129;
                    _S107 = 0.0f;
                    _S108 = _S138;
                }
                else
                {
                    _S108 = _S129;
                }
            }
            else
            {
                _S107 = _s_dOut_5;
                _S108 = 0.0f;
            }
            if(_S105)
            {
                if(_S109 < 0.00313080009073019f)
                {
                    _S107 = 12.92000007629394531f * _S107;
                }
                else
                {
                    float _S139 = 1.0549999475479126f * _S107;
                    DiffPair_float_0 _S140;
                    (&_S140)->primal_0 = _S109;
                    (&_S140)->differential_0 = 0.0f;
                    DiffPair_float_0 _S141;
                    (&_S141)->primal_0 = 0.4166666567325592f;
                    (&_S141)->differential_0 = 0.0f;
                    s_bwd_prop_pow_0(&_S140, &_S141, _S139);
                    _S107 = _S140.differential_0;
                }
                DiffPair_float_0 _S142;
                (&_S142)->primal_0 = _S110;
                (&_S142)->differential_0 = 0.0f;
                s_bwd_prop_xfer_clamp01_0(&_S142, _S107);
                DiffPair_float_0 _S143;
                (&_S143)->primal_0 = _S101.primal_0;
                (&_S143)->differential_0 = 0.0f;
                s_bwd_prop_xfer_uncharted2_0(&_S143, _S142.differential_0);
                float _S144 = _S143.differential_0 + _S108;
                _S107 = 0.0f;
                _S108 = _S144;
            }
        }
        else
        {
            _S107 = _s_dOut_5;
            _S108 = 0.0f;
        }
        if(_S106)
        {
            if(_S111 < 0.00313080009073019f)
            {
                _S107 = 12.92000007629394531f * _S107;
            }
            else
            {
                float _S145 = 1.0549999475479126f * _S107;
                DiffPair_float_0 _S146;
                (&_S146)->primal_0 = _S111;
                (&_S146)->differential_0 = 0.0f;
                DiffPair_float_0 _S147;
                (&_S147)->primal_0 = 0.4166666567325592f;
                (&_S147)->differential_0 = 0.0f;
                s_bwd_prop_pow_0(&_S146, &_S147, _S145);
                _S107 = _S146.differential_0;
            }
            DiffPair_float_0 _S148;
            (&_S148)->primal_0 = _S112;
            (&_S148)->differential_0 = 0.0f;
            s_bwd_prop_xfer_clamp01_0(&_S148, _S107);
            DiffPair_float_0 _S149;
            (&_S149)->primal_0 = _S113;
            (&_S149)->differential_0 = 0.0f;
            s_bwd_prop_xfer_aces_0(&_S149, _S148.differential_0);
            DiffPair_float_0 _S150;
            (&_S150)->primal_0 = _S101.primal_0;
            (&_S150)->differential_0 = 0.0f;
            s_bwd_prop_xfer_max0_0(&_S150, _S149.differential_0);
            float _S151 = _S150.differential_0 + _S108;
            _S107 = 0.0f;
            _S108 = _S151;
        }
    }
    else
    {
        _S107 = _s_dOut_5;
        _S108 = 0.0f;
    }
    if(_S102)
    {
        DiffPair_float_0 _S152;
        (&_S152)->primal_0 = _S101.primal_0;
        (&_S152)->differential_0 = 0.0f;
        s_bwd_prop_xfer_filmic_0(&_S152, _S107);
        _S107 = _S152.differential_0 + _S108;
    }
    else
    {
        _S107 = _S108;
    }
    dpx_11->primal_0 = (*dpx_11).primal_0;
    dpx_11->differential_0 = _S107;
    return;
}

inline __device__ void s_bwd_prop_mul_0(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S153, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S154, float3  _S155)
{
    _d_mul_0(_S153, _S154, _S155);
    return;
}

inline __device__ void s_bwd_prop_working_to_display_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dprgb_0, Matrix<float, 3, 3>  color_matrix_1, int transfer_3, bool is_linear_1, float3  _s_dOut_6)
{
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S156 = *dprgb_0;
    bool _S157 = !is_linear_1;
    float _S158;
    float _S159;
    float _S160;
    float3  _S161;
    if(_S157)
    {
        float _S162 = _S156.primal_0.x;
        if(_S162 < 0.04044999927282333f)
        {
            _S158 = _S162 * 0.07739938050508499f;
        }
        else
        {
            _S158 = s_primal_ctx_pow_0(0.94786733388900757f * (_S162 + 0.05499999970197678f), 2.40000009536743164f);
        }
        float _S163 = _S156.primal_0.y;
        if(_S163 < 0.04044999927282333f)
        {
            _S159 = _S163 * 0.07739938050508499f;
        }
        else
        {
            _S159 = s_primal_ctx_pow_0(0.94786733388900757f * (_S163 + 0.05499999970197678f), 2.40000009536743164f);
        }
        float _S164 = _S156.primal_0.z;
        if(_S164 < 0.04044999927282333f)
        {
            _S160 = _S164 * 0.07739938050508499f;
        }
        else
        {
            _S160 = s_primal_ctx_pow_0(0.94786733388900757f * (_S164 + 0.05499999970197678f), 2.40000009536743164f);
        }
        _S161 = make_float3 (_S158, _S159, _S160);
        _S158 = _S164;
        _S159 = _S163;
        _S160 = _S162;
    }
    else
    {
        _S161 = _S156.primal_0;
        _S158 = 0.0f;
        _S159 = 0.0f;
        _S160 = 0.0f;
    }
    float3  _S165 = s_primal_ctx_mul_0(color_matrix_1, _S161);
    float _S166 = _S165.x;
    float _S167 = _S165.y;
    float _S168 = _S165.z;
    DiffPair_float_0 _S169;
    (&_S169)->primal_0 = _S168;
    (&_S169)->differential_0 = 0.0f;
    s_bwd_prop_tone_encode_0(&_S169, transfer_3, _s_dOut_6.z);
    DiffPair_float_0 _S170;
    (&_S170)->primal_0 = _S167;
    (&_S170)->differential_0 = 0.0f;
    s_bwd_prop_tone_encode_0(&_S170, transfer_3, _s_dOut_6.y);
    DiffPair_float_0 _S171;
    (&_S171)->primal_0 = _S166;
    (&_S171)->differential_0 = 0.0f;
    s_bwd_prop_tone_encode_0(&_S171, transfer_3, _s_dOut_6.x);
    float3  _S172 = make_float3 (_S171.differential_0, _S170.differential_0, _S169.differential_0);
    Matrix<float, 3, 3>  _S173 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S174;
    (&_S174)->primal_0 = color_matrix_1;
    (&_S174)->differential_0 = _S173;
    float3  _S175 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S176;
    (&_S176)->primal_0 = _S161;
    (&_S176)->differential_0 = _S175;
    s_bwd_prop_mul_0(&_S174, &_S176, _S172);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S177 = _S176;
    if(_S157)
    {
        bool _S178 = _S158 < 0.04044999927282333f;
        if(_S178)
        {
            _S158 = 0.0f;
        }
        else
        {
            _S158 = 0.94786733388900757f * (_S158 + 0.05499999970197678f);
        }
        if(_S178)
        {
            _S158 = 0.07739938050508499f * _S177.differential_0.z;
        }
        else
        {
            DiffPair_float_0 _S179;
            (&_S179)->primal_0 = _S158;
            (&_S179)->differential_0 = 0.0f;
            DiffPair_float_0 _S180;
            (&_S180)->primal_0 = 2.40000009536743164f;
            (&_S180)->differential_0 = 0.0f;
            s_bwd_prop_pow_0(&_S179, &_S180, _S177.differential_0.z);
            _S158 = 0.94786733388900757f * _S179.differential_0;
        }
        bool _S181 = _S159 < 0.04044999927282333f;
        if(_S181)
        {
            _S159 = 0.0f;
        }
        else
        {
            _S159 = 0.94786733388900757f * (_S159 + 0.05499999970197678f);
        }
        if(_S181)
        {
            _S159 = 0.07739938050508499f * _S177.differential_0.y;
        }
        else
        {
            DiffPair_float_0 _S182;
            (&_S182)->primal_0 = _S159;
            (&_S182)->differential_0 = 0.0f;
            DiffPair_float_0 _S183;
            (&_S183)->primal_0 = 2.40000009536743164f;
            (&_S183)->differential_0 = 0.0f;
            s_bwd_prop_pow_0(&_S182, &_S183, _S177.differential_0.y);
            _S159 = 0.94786733388900757f * _S182.differential_0;
        }
        bool _S184 = _S160 < 0.04044999927282333f;
        if(_S184)
        {
            _S160 = 0.0f;
        }
        else
        {
            _S160 = 0.94786733388900757f * (_S160 + 0.05499999970197678f);
        }
        if(_S184)
        {
            _S160 = 0.07739938050508499f * _S177.differential_0.x;
        }
        else
        {
            DiffPair_float_0 _S185;
            (&_S185)->primal_0 = _S160;
            (&_S185)->differential_0 = 0.0f;
            DiffPair_float_0 _S186;
            (&_S186)->primal_0 = 2.40000009536743164f;
            (&_S186)->differential_0 = 0.0f;
            s_bwd_prop_pow_0(&_S185, &_S186, _S177.differential_0.x);
            _S160 = 0.94786733388900757f * _S185.differential_0;
        }
        _S161 = make_float3 (_S160, _S159, _S158);
    }
    else
    {
        _S161 = _S177.differential_0;
    }
    dprgb_0->primal_0 = (*dprgb_0).primal_0;
    dprgb_0->differential_0 = _S161;
    return;
}

inline __device__ void s_bwd_working_to_display_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S187, Matrix<float, 3, 3>  _S188, int _S189, bool _S190, float3  _S191)
{
    s_bwd_prop_working_to_display_0(_S187, _S188, _S189, _S190, _S191);
    return;
}

inline __device__ float3  working_to_display_bwd(float3  rgb_3, Matrix<float, 3, 3>  color_matrix_2, int transfer_4, bool is_linear_2, float3  v_out_rgb_1)
{
    float3  _S192 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 p_rgb_1;
    (&p_rgb_1)->primal_0 = rgb_3;
    (&p_rgb_1)->differential_0 = _S192;
    s_bwd_working_to_display_0(&p_rgb_1, color_matrix_2, transfer_4, is_linear_2, v_out_rgb_1);
    return p_rgb_1.differential_0;
}

inline __device__ void _d_sqrt_0(DiffPair_float_0 * dpx_12, float dOut_4)
{
    float _S193 = 0.5f / (F32_sqrt(((F32_max((1.00000001168609742e-07f), ((*dpx_12).primal_0)))))) * dOut_4;
    dpx_12->primal_0 = (*dpx_12).primal_0;
    dpx_12->differential_0 = _S193;
    return;
}

inline __device__ float xfer_filmic_inv_0(float y_4)
{
    float _S194 = (F32_min((y_4), (xfer_filmic_0(11.19999980926513672f))));
    float a_0 = 6.19999980926513672f * (1.0f - _S194);
    float b_0 = 0.5f - 1.70000004768371582f * _S194;
    return (- b_0 + (F32_sqrt(((F32_max((b_0 * b_0 - 4.0f * a_0 * (-0.05999999865889549f * _S194)), (0.0f))))))) / (2.0f * a_0) + 0.00400000018998981f;
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
    float _S195;
    if(transfer_5 == int(2))
    {
        if(d_0 < 0.04044999927282333f)
        {
            _S195 = d_0 * 0.07739938050508499f;
        }
        else
        {
            _S195 = (F32_pow((0.94786733388900757f * (d_0 + 0.05499999970197678f)), (2.40000009536743164f)));
        }
        return xfer_aces_inv_0(_S195);
    }
    if(transfer_5 == int(4))
    {
        if(d_0 < 0.04044999927282333f)
        {
            _S195 = d_0 * 0.07739938050508499f;
        }
        else
        {
            _S195 = (F32_pow((0.94786733388900757f * (d_0 + 0.05499999970197678f)), (2.40000009536743164f)));
        }
        return xfer_uncharted2_inv_0(_S195);
    }
    if(d_0 < 0.04044999927282333f)
    {
        _S195 = d_0 * 0.07739938050508499f;
    }
    else
    {
        _S195 = (F32_pow((0.94786733388900757f * (d_0 + 0.05499999970197678f)), (2.40000009536743164f)));
    }
    return _S195;
}

inline __device__ float3  display_to_working3(float3  rgb_4, int transfer_6, bool is_linear_3)
{
    float _S196 = tone_decode_0(rgb_4.x, transfer_6);
    float _S197 = tone_decode_0(rgb_4.y, transfer_6);
    float _S198 = tone_decode_0(rgb_4.z, transfer_6);
    float3  lin_0 = make_float3 (_S196, _S197, _S198);
    if(is_linear_3)
    {
        return lin_0;
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
    float _S201;
    if(_S198 < 0.00313080009073019f)
    {
        _S201 = _S198 * 12.92000007629394531f;
    }
    else
    {
        _S201 = 1.0549999475479126f * (F32_pow((_S198), (0.4166666567325592f))) - 0.05499999970197678f;
    }
    return make_float3 (_S199, _S200, _S201);
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
    float _S202 = dOut_5.y;
    float _S203 = dOut_5.z;
    float _S204 = dOut_5.x;
    float _S205 = (*a_3).primal_0.z * _S202 + - (*a_3).primal_0.y * _S203;
    float _S206 = - (*a_3).primal_0.z * _S204 + (*a_3).primal_0.x * _S203;
    float _S207 = (*a_3).primal_0.y * _S204 + - (*a_3).primal_0.x * _S202;
    float3  _S208 = make_float3 (- (*b_3).primal_0.z * _S202 + (*b_3).primal_0.y * _S203, (*b_3).primal_0.z * _S204 + - (*b_3).primal_0.x * _S203, - (*b_3).primal_0.y * _S204 + (*b_3).primal_0.x * _S202);
    a_3->primal_0 = (*a_3).primal_0;
    a_3->differential_0 = _S208;
    float3  _S209 = make_float3 (_S205, _S206, _S207);
    b_3->primal_0 = (*b_3).primal_0;
    b_3->differential_0 = _S209;
    return;
}

inline __device__ float3  cross_0(float3  left_2, float3  right_2)
{
    float _S210 = left_2.y;
    float _S211 = right_2.z;
    float _S212 = left_2.z;
    float _S213 = right_2.y;
    float _S214 = right_2.x;
    float _S215 = left_2.x;
    return make_float3 (_S210 * _S211 - _S212 * _S213, _S212 * _S214 - _S215 * _S211, _S215 * _S213 - _S210 * _S214);
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
    float3  _S216 = points_0[int(0)];
    bool _S217;
    if((dot_0(_S216, _S216)) == 0.0f)
    {
        _S217 = true;
    }
    else
    {
        float3  _S218 = points_0[int(1)];
        _S217 = (dot_0(_S218, _S218)) == 0.0f;
    }
    if(_S217)
    {
        _S217 = true;
    }
    else
    {
        float3  _S219 = points_0[int(2)];
        _S217 = (dot_0(_S219, _S219)) == 0.0f;
    }
    if(_S217)
    {
        _S217 = true;
    }
    else
    {
        float3  _S220 = points_0[int(3)];
        _S217 = (dot_0(_S220, _S220)) == 0.0f;
    }
    if(_S217)
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

inline __device__ float s_primal_ctx_dot_0(float3  _S221, float3  _S222)
{
    return dot_0(_S221, _S222);
}

inline __device__ float3  s_primal_ctx_cross_0(float3  _S223, float3  _S224)
{
    return cross_0(_S223, _S224);
}

inline __device__ void s_bwd_prop_sqrt_0(DiffPair_float_0 * _S225, float _S226)
{
    _d_sqrt_0(_S225, _S226);
    return;
}

inline __device__ void s_bwd_prop_length_impl_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_13, float _s_dOut_7)
{
    float _S227 = (*dpx_13).primal_0.x;
    float _S228 = (*dpx_13).primal_0.y;
    float _S229 = (*dpx_13).primal_0.z;
    DiffPair_float_0 _S230;
    (&_S230)->primal_0 = _S227 * _S227 + _S228 * _S228 + _S229 * _S229;
    (&_S230)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S230, _s_dOut_7);
    float _S231 = (*dpx_13).primal_0.z * _S230.differential_0;
    float _S232 = _S231 + _S231;
    float _S233 = (*dpx_13).primal_0.y * _S230.differential_0;
    float _S234 = _S233 + _S233;
    float _S235 = (*dpx_13).primal_0.x * _S230.differential_0;
    float _S236 = _S235 + _S235;
    float3  _S237 = make_float3 (0.0f);
    *&((&_S237)->z) = _S232;
    *&((&_S237)->y) = _S234;
    *&((&_S237)->x) = _S236;
    dpx_13->primal_0 = (*dpx_13).primal_0;
    dpx_13->differential_0 = _S237;
    return;
}

inline __device__ void s_bwd_length_impl_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S238, float _S239)
{
    s_bwd_prop_length_impl_0(_S238, _S239);
    return;
}

inline __device__ void s_bwd_prop_dot_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S240, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S241, float _S242)
{
    _d_dot_0(_S240, _S241, _S242);
    return;
}

inline __device__ void s_bwd_prop_cross_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S243, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S244, float3  _S245)
{
    _d_cross_0(_S243, _S244, _S245);
    return;
}

inline __device__ void s_bwd_prop_points_to_normal_0(DiffPair_arrayx3Cvectorx3Cfloatx2C3x3Ex2C4x3E_0 * dppoints_0, float3  _s_dOut_8)
{
    FixedArray<float3 , 4>  _S246 = dppoints_0->primal_0;
    float3  _S247 = make_float3 (0.0f);
    float3  _S248 = dppoints_0->primal_0[int(0)];
    bool _S249 = (s_primal_ctx_dot_0(_S248, _S248)) == 0.0f;
    bool _S250;
    float3  _S251;
    if(_S249)
    {
        _S250 = true;
        _S251 = _S247;
    }
    else
    {
        float3  _S252 = _S246[int(1)];
        _S250 = (s_primal_ctx_dot_0(_S252, _S252)) == 0.0f;
        _S251 = _S246[int(1)];
    }
    bool _S253;
    float3  _S254;
    if(_S250)
    {
        _S253 = true;
        _S254 = _S247;
    }
    else
    {
        float3  _S255 = _S246[int(2)];
        _S253 = (s_primal_ctx_dot_0(_S255, _S255)) == 0.0f;
        _S254 = _S246[int(2)];
    }
    bool _S256;
    float3  _S257;
    if(_S253)
    {
        _S256 = true;
        _S257 = _S247;
    }
    else
    {
        float3  _S258 = _S246[int(3)];
        _S256 = (s_primal_ctx_dot_0(_S258, _S258)) == 0.0f;
        _S257 = _S246[int(3)];
    }
    bool _S259 = !_S256;
    float3  _S260;
    float3  _S261;
    float3  _S262;
    float3  _S263;
    float3  _S264;
    if(_S259)
    {
        float3  dx_0 = _S246[int(1)] - _S246[int(0)];
        float3  _S265 = - (_S246[int(3)] - _S246[int(2)]);
        float3  _S266 = s_primal_ctx_cross_0(dx_0, _S265);
        bool _S267 = (s_primal_ctx_dot_0(_S266, _S266)) != 0.0f;
        if(_S267)
        {
            float _S268 = length_0(_S266);
            float3  _S269 = make_float3 (_S268);
            _S260 = make_float3 (_S268 * _S268);
            _S261 = _S269;
        }
        else
        {
            _S260 = _S247;
            _S261 = _S247;
        }
        float3  _S270 = _S261;
        _S256 = _S267;
        _S261 = _S266;
        _S262 = _S270;
        _S263 = dx_0;
        _S264 = _S265;
    }
    else
    {
        _S256 = false;
        _S260 = _S247;
        _S261 = _S247;
        _S262 = _S247;
        _S263 = _S247;
        _S264 = _S247;
    }
    FixedArray<float3 , 4>  _S271;
    if(_S259)
    {
        if(_S256)
        {
            float3  _S272 = _s_dOut_8 / _S260;
            float3  _S273 = _S261 * - _S272;
            float3  _S274 = _S262 * _S272;
            float _S275 = _S273.x + _S273.y + _S273.z;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S276;
            (&_S276)->primal_0 = _S261;
            (&_S276)->differential_0 = _S247;
            s_bwd_length_impl_0(&_S276, _S275);
            _S260 = _S274 + _S276.differential_0;
        }
        else
        {
            _S260 = _s_dOut_8;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S277;
        (&_S277)->primal_0 = _S261;
        (&_S277)->differential_0 = _S247;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S278;
        (&_S278)->primal_0 = _S261;
        (&_S278)->differential_0 = _S247;
        s_bwd_prop_dot_0(&_S277, &_S278, 0.0f);
        float3  _S279 = _S278.differential_0 + _S277.differential_0 + _S260;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S280;
        (&_S280)->primal_0 = _S263;
        (&_S280)->differential_0 = _S247;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S281;
        (&_S281)->primal_0 = _S264;
        (&_S281)->differential_0 = _S247;
        s_bwd_prop_cross_0(&_S280, &_S281, _S279);
        float3  s_diff_dy_T_0 = - _S281.differential_0;
        float3  _S282 = - s_diff_dy_T_0;
        float3  _S283 = - _S280.differential_0;
        FixedArray<float3 , 4>  _S284;
        _S284[int(0)] = _S247;
        _S284[int(1)] = _S247;
        _S284[int(2)] = _S247;
        _S284[int(3)] = _S247;
        _S284[int(2)] = _S282;
        _S284[int(3)] = s_diff_dy_T_0;
        _S284[int(1)] = _S280.differential_0;
        _S271[int(0)] = _S284[int(0)];
        _S271[int(1)] = _S284[int(1)];
        _S271[int(2)] = _S284[int(2)];
        _S271[int(3)] = _S284[int(3)];
        _S260 = _S283;
    }
    else
    {
        _S271[int(0)] = _S247;
        _S271[int(1)] = _S247;
        _S271[int(2)] = _S247;
        _S271[int(3)] = _S247;
        _S260 = _S247;
    }
    if(_S253)
    {
    }
    else
    {
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S285;
        (&_S285)->primal_0 = _S257;
        (&_S285)->differential_0 = _S247;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S286;
        (&_S286)->primal_0 = _S257;
        (&_S286)->differential_0 = _S247;
        s_bwd_prop_dot_0(&_S285, &_S286, 0.0f);
        float3  _S287 = _S286.differential_0 + _S285.differential_0;
        FixedArray<float3 , 4>  _S288;
        _S288[int(0)] = _S247;
        _S288[int(1)] = _S247;
        _S288[int(2)] = _S247;
        _S288[int(3)] = _S247;
        _S288[int(3)] = _S287;
        float3  _S289 = _S271[int(1)] + _S288[int(1)];
        float3  _S290 = _S271[int(2)] + _S288[int(2)];
        float3  _S291 = _S271[int(3)] + _S288[int(3)];
        _S271[int(0)] = _S271[int(0)] + _S288[int(0)];
        _S271[int(1)] = _S289;
        _S271[int(2)] = _S290;
        _S271[int(3)] = _S291;
    }
    if(_S250)
    {
    }
    else
    {
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S292;
        (&_S292)->primal_0 = _S254;
        (&_S292)->differential_0 = _S247;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S293;
        (&_S293)->primal_0 = _S254;
        (&_S293)->differential_0 = _S247;
        s_bwd_prop_dot_0(&_S292, &_S293, 0.0f);
        float3  _S294 = _S293.differential_0 + _S292.differential_0;
        FixedArray<float3 , 4>  _S295;
        _S295[int(0)] = _S247;
        _S295[int(1)] = _S247;
        _S295[int(2)] = _S247;
        _S295[int(3)] = _S247;
        _S295[int(2)] = _S294;
        float3  _S296 = _S271[int(1)] + _S295[int(1)];
        float3  _S297 = _S271[int(2)] + _S295[int(2)];
        float3  _S298 = _S271[int(3)] + _S295[int(3)];
        _S271[int(0)] = _S271[int(0)] + _S295[int(0)];
        _S271[int(1)] = _S296;
        _S271[int(2)] = _S297;
        _S271[int(3)] = _S298;
    }
    if(_S249)
    {
    }
    else
    {
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S299;
        (&_S299)->primal_0 = _S251;
        (&_S299)->differential_0 = _S247;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S300;
        (&_S300)->primal_0 = _S251;
        (&_S300)->differential_0 = _S247;
        s_bwd_prop_dot_0(&_S299, &_S300, 0.0f);
        float3  _S301 = _S300.differential_0 + _S299.differential_0;
        FixedArray<float3 , 4>  _S302;
        _S302[int(0)] = _S247;
        _S302[int(1)] = _S247;
        _S302[int(2)] = _S247;
        _S302[int(3)] = _S247;
        _S302[int(1)] = _S301;
        float3  _S303 = _S271[int(1)] + _S302[int(1)];
        float3  _S304 = _S271[int(2)] + _S302[int(2)];
        float3  _S305 = _S271[int(3)] + _S302[int(3)];
        _S271[int(0)] = _S271[int(0)] + _S302[int(0)];
        _S271[int(1)] = _S303;
        _S271[int(2)] = _S304;
        _S271[int(3)] = _S305;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S306;
    (&_S306)->primal_0 = _S246[int(0)];
    (&_S306)->differential_0 = _S247;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S307;
    (&_S307)->primal_0 = _S246[int(0)];
    (&_S307)->differential_0 = _S247;
    s_bwd_prop_dot_0(&_S306, &_S307, 0.0f);
    float3  _S308 = _S307.differential_0 + _S306.differential_0 + _S260;
    FixedArray<float3 , 4>  _S309;
    _S309[int(0)] = _S247;
    _S309[int(1)] = _S247;
    _S309[int(2)] = _S247;
    _S309[int(3)] = _S247;
    _S309[int(0)] = _S308;
    FixedArray<float3 , 4>  _S310 = {
        _S271[int(0)] + _S309[int(0)], _S271[int(1)] + _S309[int(1)], _S271[int(2)] + _S309[int(2)], _S271[int(3)] + _S309[int(3)]
    };
    dppoints_0->primal_0 = dppoints_0->primal_0;
    dppoints_0->differential_0 = _S310;
    return;
}

inline __device__ void s_bwd_points_to_normal_0(DiffPair_arrayx3Cvectorx3Cfloatx2C3x3Ex2C4x3E_0 * _S311, float3  _S312)
{
    s_bwd_prop_points_to_normal_0(_S311, _S312);
    return;
}

inline __device__ void points_to_normal_vjp(FixedArray<float3 , 4>  points_1, float3  v_normal_0, FixedArray<float3 , 4>  * v_points_0)
{
    FixedArray<float3 , 4>  _S313 = { make_float3 (0.0f), make_float3 (0.0f), make_float3 (0.0f), make_float3 (0.0f) };
    DiffPair_arrayx3Cvectorx3Cfloatx2C3x3Ex2C4x3E_0 dp_points_0;
    (&dp_points_0)->primal_0 = points_1;
    (&dp_points_0)->differential_0 = _S313;
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
    float _S314 = s_diff_u_0 * u_1;
    float _S315 = s_diff_v_0 * v_1;
    float r2_1 = u_1 * u_1 + v_1 * v_1;
    float s_diff_r2_0 = _S314 + _S314 + (_S315 + _S315);
    float _S316 = (*coeffs_1)[int(0)] + r2_1 * (*coeffs_1)[int(1)];
    float radial_0 = 1.0f + r2_1 * _S316;
    float _S317 = 2.0f * (*coeffs_1)[int(2)];
    float _S318 = _S317 * u_1;
    float _S319 = 2.0f * u_1;
    float _S320 = 2.0f * (*coeffs_1)[int(3)];
    float _S321 = _S320 * u_1;
    float _S322 = 2.0f * v_1;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S323 = { dpuv_0->primal_0 * make_float2 (radial_0) + make_float2 (_S318 * v_1 + (*coeffs_1)[int(3)] * (r2_1 + _S319 * u_1), _S321 * v_1 + (*coeffs_1)[int(2)] * (r2_1 + _S322 * v_1)), dpuv_0->differential_0 * make_float2 (radial_0) + make_float2 (s_diff_r2_0 * _S316 + s_diff_r2_0 * (*coeffs_1)[int(1)] * r2_1) * dpuv_0->primal_0 + make_float2 (s_diff_u_0 * _S317 * v_1 + s_diff_v_0 * _S318 + (s_diff_r2_0 + (s_diff_u_0 * 2.0f * u_1 + s_diff_u_0 * _S319)) * (*coeffs_1)[int(3)], s_diff_u_0 * _S320 * v_1 + s_diff_v_0 * _S321 + (s_diff_r2_0 + (s_diff_v_0 * 2.0f * v_1 + s_diff_v_0 * _S322)) * (*coeffs_1)[int(2)]) };
    return _S323;
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
        float2  _S324 = DistOpenCV_distort_0(q_0, dist_coeffs_1);
        float2  r_2 = _S324 - uv_2;
        float2  _S325 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S326;
        (&_S326)->primal_0 = q_0;
        (&_S326)->differential_0 = _S325;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S327 = s_fwd_DistOpenCV_distort_0(&_S326, dist_coeffs_1);
        float2  _S328 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S329;
        (&_S329)->primal_0 = q_0;
        (&_S329)->differential_0 = _S328;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S330 = s_fwd_DistOpenCV_distort_0(&_S329, dist_coeffs_1);
        Matrix<float, 2, 2>  _S331 = transpose_0(makeMatrix<float, 2, 2> (_S327.differential_0, _S330.differential_0));
        float inv_det_0 = 1.0f / (_S331.rows[int(0)].x * _S331.rows[int(1)].y - _S331.rows[int(0)].y * _S331.rows[int(1)].x);
        float _S332 = r_2.x;
        float _S333 = r_2.y;
        float2  q_1 = q_0 - make_float2 ((_S332 * _S331.rows[int(1)].y - _S333 * _S331.rows[int(0)].y) * inv_det_0, (- _S332 * _S331.rows[int(1)].x + _S333 * _S331.rows[int(0)].x) * inv_det_0);
        i_5 = i_5 + int(1);
        q_0 = q_1;
    }
    *uv_undist_1 = q_0;
    float2  _S334 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S335;
    (&_S335)->primal_0 = q_0;
    (&_S335)->differential_0 = _S334;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S336 = s_fwd_DistOpenCV_distort_0(&_S335, dist_coeffs_1);
    float2  _S337 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S338;
    (&_S338)->primal_0 = q_0;
    (&_S338)->differential_0 = _S337;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S339 = s_fwd_DistOpenCV_distort_0(&_S338, dist_coeffs_1);
    Matrix<float, 2, 2>  _S340 = transpose_0(makeMatrix<float, 2, 2> (_S336.differential_0, _S339.differential_0));
    float _S341 = (F32_min((determinant_0(_S340)), ((F32_min((_S340.rows[int(0)].x), (_S340.rows[int(1)].y))))));
    bool _S342;
    if(_S341 > 0.25f)
    {
        _S342 = _S341 < 4.0f;
    }
    else
    {
        _S342 = false;
    }
    if(_S342)
    {
        float2  _S343 = DistOpenCV_distort_0(q_0, dist_coeffs_1);
        _S342 = (dot_1(q_0, _S343)) >= 0.0f;
    }
    else
    {
        _S342 = false;
    }
    if(_S342)
    {
        float2  _S344 = DistOpenCV_distort_0(*uv_undist_1, dist_coeffs_1);
        _S342 = (length_1(_S344 - uv_2)) < 0.00999999977648258f;
    }
    else
    {
        _S342 = false;
    }
    return _S342;
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
    float _S345 = s_diff_u_1 * u_3;
    float _S346 = s_diff_v_1 * v_3;
    float r2_3 = u_3 * u_3 + v_3 * v_3;
    float s_diff_r2_1 = _S345 + _S345 + (_S346 + _S346);
    float _S347 = (*coeffs_3)[int(2)] + r2_3 * (*coeffs_3)[int(3)];
    float _S348 = (*coeffs_3)[int(1)] + r2_3 * _S347;
    float _S349 = (*coeffs_3)[int(0)] + r2_3 * _S348;
    float radial_1 = 1.0f + r2_3 * _S349;
    float _S350 = 2.0f * (*coeffs_3)[int(4)];
    float _S351 = _S350 * u_3;
    float _S352 = 2.0f * u_3;
    float _S353 = 2.0f * (*coeffs_3)[int(5)];
    float _S354 = _S353 * u_3;
    float _S355 = 2.0f * v_3;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S356 = { dpuv_1->primal_0 * make_float2 (radial_1) + make_float2 (_S351 * v_3 + (*coeffs_3)[int(5)] * (r2_3 + _S352 * u_3) + (*coeffs_3)[int(6)] * r2_3, _S354 * v_3 + (*coeffs_3)[int(4)] * (r2_3 + _S355 * v_3) + (*coeffs_3)[int(7)] * r2_3), dpuv_1->differential_0 * make_float2 (radial_1) + make_float2 (s_diff_r2_1 * _S349 + (s_diff_r2_1 * _S348 + (s_diff_r2_1 * _S347 + s_diff_r2_1 * (*coeffs_3)[int(3)] * r2_3) * r2_3) * r2_3) * dpuv_1->primal_0 + make_float2 (s_diff_u_1 * _S350 * v_3 + s_diff_v_1 * _S351 + (s_diff_r2_1 + (s_diff_u_1 * 2.0f * u_3 + s_diff_u_1 * _S352)) * (*coeffs_3)[int(5)] + s_diff_r2_1 * (*coeffs_3)[int(6)], s_diff_u_1 * _S353 * v_3 + s_diff_v_1 * _S354 + (s_diff_r2_1 + (s_diff_v_1 * 2.0f * v_3 + s_diff_v_1 * _S355)) * (*coeffs_3)[int(4)] + s_diff_r2_1 * (*coeffs_3)[int(7)]) };
    return _S356;
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
        float2  _S357 = DistThinPrism_distort_0(q_2, dist_coeffs_2);
        float2  r_3 = _S357 - uv_4;
        float2  _S358 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S359;
        (&_S359)->primal_0 = q_2;
        (&_S359)->differential_0 = _S358;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S360 = s_fwd_DistThinPrism_distort_0(&_S359, dist_coeffs_2);
        float2  _S361 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S362;
        (&_S362)->primal_0 = q_2;
        (&_S362)->differential_0 = _S361;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S363 = s_fwd_DistThinPrism_distort_0(&_S362, dist_coeffs_2);
        Matrix<float, 2, 2>  _S364 = transpose_0(makeMatrix<float, 2, 2> (_S360.differential_0, _S363.differential_0));
        float inv_det_1 = 1.0f / (_S364.rows[int(0)].x * _S364.rows[int(1)].y - _S364.rows[int(0)].y * _S364.rows[int(1)].x);
        float _S365 = r_3.x;
        float _S366 = r_3.y;
        float2  q_3 = q_2 - make_float2 ((_S365 * _S364.rows[int(1)].y - _S366 * _S364.rows[int(0)].y) * inv_det_1, (- _S365 * _S364.rows[int(1)].x + _S366 * _S364.rows[int(0)].x) * inv_det_1);
        i_6 = i_6 + int(1);
        q_2 = q_3;
    }
    *uv_undist_2 = q_2;
    float2  _S367 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S368;
    (&_S368)->primal_0 = q_2;
    (&_S368)->differential_0 = _S367;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S369 = s_fwd_DistThinPrism_distort_0(&_S368, dist_coeffs_2);
    float2  _S370 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S371;
    (&_S371)->primal_0 = q_2;
    (&_S371)->differential_0 = _S370;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S372 = s_fwd_DistThinPrism_distort_0(&_S371, dist_coeffs_2);
    Matrix<float, 2, 2>  _S373 = transpose_0(makeMatrix<float, 2, 2> (_S369.differential_0, _S372.differential_0));
    float _S374 = (F32_min((determinant_0(_S373)), ((F32_min((_S373.rows[int(0)].x), (_S373.rows[int(1)].y))))));
    bool _S375;
    if(_S374 > 0.25f)
    {
        _S375 = _S374 < 4.0f;
    }
    else
    {
        _S375 = false;
    }
    if(_S375)
    {
        float2  _S376 = DistThinPrism_distort_0(q_2, dist_coeffs_2);
        _S375 = (dot_1(q_2, _S376)) >= 0.0f;
    }
    else
    {
        _S375 = false;
    }
    if(_S375)
    {
        float2  _S377 = DistThinPrism_distort_0(*uv_undist_2, dist_coeffs_2);
        _S375 = (length_1(_S377 - uv_4)) < 0.00999999977648258f;
    }
    else
    {
        _S375 = false;
    }
    return _S375;
}

inline __device__ float3  normalize_0(float3  x_20)
{
    return x_20 / make_float3 (length_0(x_20));
}

inline __device__ float3  unproject_raydir_0(float2  uv_5, int camera_model_0, bool is_ray_depth_0)
{
    float3  raydir_0;
    bool is_unit_0;
    if(camera_model_0 == int(1))
    {
        float theta_0 = length_1(uv_5);
        float3  _S378 = make_float3 ((uv_5 / make_float2 ((F32_max((theta_0), (1.00000001168609742e-07f)))) * make_float2 ((F32_sin((theta_0))))).x, (uv_5 / make_float2 ((F32_max((theta_0), (1.00000001168609742e-07f)))) * make_float2 ((F32_sin((theta_0))))).y, (F32_cos((theta_0))));
        is_unit_0 = true;
        raydir_0 = _S378;
    }
    else
    {
        bool _S379 = camera_model_0 == int(2);
        if(_S379)
        {
            float r_4 = length_1(uv_5);
            raydir_0 = make_float3 ((uv_5 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_4 * r_4)))))))).x, (uv_5 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_4 * r_4)))))))).y, 1.0f - 0.5f * r_4 * r_4);
        }
        else
        {
            raydir_0 = make_float3 (uv_5.x, uv_5.y, 1.0f);
        }
        is_unit_0 = _S379;
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
    float3  raydir_1;
    bool is_unit_1;
    if(camera_model_3 == int(1))
    {
        float _S393 = length_1(dpuv_2);
        float3  _S394 = make_float3 ((dpuv_2 / make_float2 ((F32_max((_S393), (1.00000001168609742e-07f)))) * make_float2 (s_primal_ctx_sin_0(_S393))).x, (dpuv_2 / make_float2 ((F32_max((_S393), (1.00000001168609742e-07f)))) * make_float2 (s_primal_ctx_sin_0(_S393))).y, s_primal_ctx_cos_0(_S393));
        is_unit_1 = true;
        raydir_1 = _S394;
    }
    else
    {
        bool _S395 = camera_model_3 == int(2);
        if(_S395)
        {
            float _S396 = length_1(dpuv_2);
            raydir_1 = make_float3 ((dpuv_2 * make_float2 (s_primal_ctx_sqrt_0((F32_max((0.0f), (1.0f - 0.25f * _S396 * _S396)))))).x, (dpuv_2 * make_float2 (s_primal_ctx_sqrt_0((F32_max((0.0f), (1.0f - 0.25f * _S396 * _S396)))))).y, 1.0f - 0.5f * _S396 * _S396);
        }
        else
        {
            raydir_1 = make_float3 (dpuv_2.x, dpuv_2.y, 1.0f);
        }
        is_unit_1 = _S395;
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

inline __device__ float depth_to_point_vjp_none(float2  pix_pos_2, float4  intrins_2, FixedArray<float, 1>  dist_coeffs_5, int camera_model_4, bool is_ray_depth_4, float depth_3, float3  v_point_0)
{
    float2  _S397 = make_float2 (0.0f);
    s_bwd_prop_depth_to_point_Intermediates_0 _S398;
    (&_S398)->_S388 = _S397;
    (&_S398)->_S389 = false;
    float2  uv_8 = (pix_pos_2 - float2 {intrins_2.z, intrins_2.w}) / float2 {intrins_2.x, intrins_2.y};
    float2  _S399 = _S397;
    FixedArray<float, 1>  _S400 = dist_coeffs_5;
    bool _S401 = undistort_point_0(uv_8, &_S400, int(12), &_S399);
    (&_S398)->_S388 = _S399;
    (&_S398)->_S389 = _S401;
    s_bwd_prop_depth_to_point_Intermediates_0 _S402 = _S398;
    float3  _S403 = make_float3 (0.0f);
    bool _S404 = !!_S398._S389;
    float3  _S405;
    if(_S404)
    {
        _S405 = s_primal_ctx_unproject_raydir_0(_S402._S388, camera_model_4, is_ray_depth_4);
    }
    else
    {
        _S405 = _S403;
    }
    if(_S404)
    {
        _S405 = _S405 * v_point_0;
    }
    else
    {
        _S405 = _S403;
    }
    return _S405.x + _S405.y + _S405.z;
}

inline __device__ float3  depth_to_normal_none(float2  pix_center_0, float4  intrins_3, FixedArray<float, 1>  dist_coeffs_6, int camera_model_5, bool is_ray_depth_5, float4  depths_0)
{
    float3  normal_2;
    for(;;)
    {
        bool _S406;
        if((depths_0.x) == 0.0f)
        {
            _S406 = true;
        }
        else
        {
            _S406 = (depths_0.y) == 0.0f;
        }
        if(_S406)
        {
            _S406 = true;
        }
        else
        {
            _S406 = (depths_0.z) == 0.0f;
        }
        if(_S406)
        {
            _S406 = true;
        }
        else
        {
            _S406 = (depths_0.w) == 0.0f;
        }
        if(_S406)
        {
            normal_2 = make_float3 (0.0f);
            break;
        }
        float3  * _S407;
        float3  * _S408;
        float3  * _S409;
        float3  * _S410;
        int _S411;
        FixedArray<float3 , 4>  points_2;
        for(;;)
        {
            float2  _S412 = float2 {intrins_3.z, intrins_3.w};
            float2  _S413 = float2 {intrins_3.x, intrins_3.y};
            float2  uv_9 = (pix_center_0 + make_float2 (-1.0f, -0.0f) - _S412) / _S413;
            FixedArray<float, 1>  _S414 = dist_coeffs_6;
            float2  uv_u_2;
            bool _S415 = undistort_point_0(uv_9, &_S414, int(12), &uv_u_2);
            if(!_S415)
            {
                float3  _S416 = make_float3 (0.0f);
                _S411 = int(0);
                _S410 = nullptr;
                _S409 = nullptr;
                _S408 = nullptr;
                _S407 = nullptr;
                normal_2 = _S416;
                break;
            }
            points_2[int(0)] = make_float3 (depths_0.x) * unproject_raydir_0(uv_u_2, camera_model_5, is_ray_depth_5);
            for(;;)
            {
                float2  uv_10 = (pix_center_0 + make_float2 (1.0f, -0.0f) - _S412) / _S413;
                FixedArray<float, 1>  _S417 = dist_coeffs_6;
                float2  uv_u_3;
                bool _S418 = undistort_point_0(uv_10, &_S417, int(12), &uv_u_3);
                if(!_S418)
                {
                    float3  _S419 = make_float3 (0.0f);
                    _S411 = int(0);
                    _S410 = nullptr;
                    normal_2 = _S419;
                    break;
                }
                points_2[int(1)] = make_float3 (depths_0.y) * unproject_raydir_0(uv_u_3, camera_model_5, is_ray_depth_5);
                _S411 = int(2);
                _S410 = &points_2[int(1)];
                break;
            }
            if(_S411 != int(2))
            {
                _S409 = &points_2[int(0)];
                _S408 = nullptr;
                _S407 = nullptr;
                break;
            }
            float2  uv_11 = (pix_center_0 + make_float2 (0.0f, -1.0f) - _S412) / _S413;
            FixedArray<float, 1>  _S420 = dist_coeffs_6;
            float2  uv_u_4;
            bool _S421 = undistort_point_0(uv_11, &_S420, int(12), &uv_u_4);
            if(!_S421)
            {
                float3  _S422 = make_float3 (0.0f);
                _S411 = int(0);
                _S409 = &points_2[int(0)];
                _S408 = nullptr;
                _S407 = nullptr;
                normal_2 = _S422;
                break;
            }
            points_2[int(2)] = make_float3 (depths_0.z) * unproject_raydir_0(uv_u_4, camera_model_5, is_ray_depth_5);
            for(;;)
            {
                float2  uv_12 = (pix_center_0 + make_float2 (0.0f, 1.0f) - _S412) / _S413;
                FixedArray<float, 1>  _S423 = dist_coeffs_6;
                float2  uv_u_5;
                bool _S424 = undistort_point_0(uv_12, &_S423, int(12), &uv_u_5);
                if(!_S424)
                {
                    float3  _S425 = make_float3 (0.0f);
                    _S411 = int(0);
                    _S409 = nullptr;
                    normal_2 = _S425;
                    break;
                }
                points_2[int(3)] = make_float3 (depths_0.w) * unproject_raydir_0(uv_u_5, camera_model_5, is_ray_depth_5);
                _S411 = int(2);
                _S409 = &points_2[int(3)];
                break;
            }
            if(_S411 != int(2))
            {
                float3  * _S426 = _S409;
                _S409 = &points_2[int(0)];
                _S408 = _S426;
                _S407 = &points_2[int(2)];
                break;
            }
            float3  * _S427 = _S409;
            _S411 = int(1);
            _S409 = &points_2[int(0)];
            _S408 = _S427;
            _S407 = &points_2[int(2)];
            break;
        }
        if(_S411 != int(1))
        {
            break;
        }
        float3  normal_3 = cross_0(*_S410 - *_S409, - (*_S408 - *_S407));
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
    float2  _S428;
    bool _S429;
    float2  _S430;
    bool _S431;
    float2  _S432;
    bool _S433;
    float2  _S434;
    bool _S435;
};

inline __device__ void depth_to_normal_vjp_none(float2  pix_center_1, float4  intrins_4, FixedArray<float, 1>  dist_coeffs_7, int camera_model_6, bool is_ray_depth_6, float4  depths_1, float3  v_normal_1, float4  * v_depths_0)
{
    float2  _S436 = make_float2 (0.0f);
    s_bwd_prop_depth_to_normal_Intermediates_0 _S437;
    (&_S437)->_S428 = _S436;
    (&_S437)->_S429 = false;
    (&_S437)->_S430 = _S436;
    (&_S437)->_S431 = false;
    (&_S437)->_S432 = _S436;
    (&_S437)->_S433 = false;
    (&_S437)->_S434 = _S436;
    (&_S437)->_S435 = false;
    (&_S437)->_S428 = _S436;
    (&_S437)->_S429 = false;
    (&_S437)->_S430 = _S436;
    (&_S437)->_S431 = false;
    (&_S437)->_S432 = _S436;
    (&_S437)->_S433 = false;
    (&_S437)->_S434 = _S436;
    (&_S437)->_S435 = false;
    bool _S438 = (depths_1.x) == 0.0f;
    bool _runFlag_3;
    if(_S438)
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
    int _S439;
    if(!_runFlag_3)
    {
        float2  _S440 = float2 {intrins_4.z, intrins_4.w};
        float2  _S441 = float2 {intrins_4.x, intrins_4.y};
        float2  uv_13 = (pix_center_1 + make_float2 (-1.0f, -0.0f) - _S440) / _S441;
        float2  _S442 = _S436;
        FixedArray<float, 1>  _S443 = dist_coeffs_7;
        bool _S444 = undistort_point_0(uv_13, &_S443, int(12), &_S442);
        (&_S437)->_S428 = _S442;
        (&_S437)->_S429 = _S444;
        bool _S445 = !!_S444;
        if(_S445)
        {
            float2  uv_14 = (pix_center_1 + make_float2 (1.0f, -0.0f) - _S440) / _S441;
            float2  _S446 = _S436;
            FixedArray<float, 1>  _S447 = dist_coeffs_7;
            bool _S448 = undistort_point_0(uv_14, &_S447, int(12), &_S446);
            (&_S437)->_S430 = _S446;
            (&_S437)->_S431 = _S448;
            if(!!_S448)
            {
                _S439 = int(2);
            }
            else
            {
                _S439 = int(0);
            }
            if(_S439 != int(2))
            {
                _runFlag_3 = false;
            }
            else
            {
                _runFlag_3 = _S445;
            }
            if(_runFlag_3)
            {
                float2  uv_15 = (pix_center_1 + make_float2 (0.0f, -1.0f) - _S440) / _S441;
                float2  _S449 = _S436;
                FixedArray<float, 1>  _S450 = dist_coeffs_7;
                bool _S451 = undistort_point_0(uv_15, &_S450, int(12), &_S449);
                (&_S437)->_S432 = _S449;
                (&_S437)->_S433 = _S451;
                if(!_S451)
                {
                    _runFlag_3 = false;
                }
                if(_runFlag_3)
                {
                    float2  uv_16 = (pix_center_1 + make_float2 (0.0f, 1.0f) - _S440) / _S441;
                    float2  _S452 = _S436;
                    FixedArray<float, 1>  _S453 = dist_coeffs_7;
                    bool _S454 = undistort_point_0(uv_16, &_S453, int(12), &_S452);
                    (&_S437)->_S434 = _S452;
                    (&_S437)->_S435 = _S454;
                }
            }
        }
    }
    s_bwd_prop_depth_to_normal_Intermediates_0 _S455 = _S437;
    float3  _S456 = make_float3 (0.0f);
    if(_S438)
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
    bool _S457 = !_runFlag_3;
    bool _runFlag_4;
    bool _runFlag_5;
    bool _S458;
    bool _runFlag_6;
    bool _S459;
    bool _S460;
    FixedArray<float3 , 4>  points_3;
    float3  _S461;
    float3  _S462;
    float3  _S463;
    float3  _S464;
    float3  _S465;
    float3  _S466;
    float3  _S467;
    float3  _S468;
    float3  _S469;
    if(_S457)
    {
        bool _S470 = !!_S455._S429;
        if(_S470)
        {
            float3  _S471 = s_primal_ctx_unproject_raydir_0(_S455._S428, camera_model_6, is_ray_depth_6);
            float3  _S472 = make_float3 (depths_1.x) * _S471;
            bool _S473 = !!_S455._S431;
            if(_S473)
            {
                float3  _S474 = s_primal_ctx_unproject_raydir_0(_S455._S430, camera_model_6, is_ray_depth_6);
                float3  _S475 = make_float3 (depths_1.y) * _S474;
                _S439 = int(2);
                points_3[int(0)] = _S472;
                points_3[int(1)] = _S475;
                points_3[int(2)] = _S456;
                points_3[int(3)] = _S456;
                _S461 = _S474;
            }
            else
            {
                _S439 = int(0);
                points_3[int(0)] = _S472;
                points_3[int(1)] = _S456;
                points_3[int(2)] = _S456;
                points_3[int(3)] = _S456;
                _S461 = _S456;
            }
            if(_S439 != int(2))
            {
                _runFlag_3 = false;
            }
            else
            {
                _runFlag_3 = _S470;
                _S439 = int(0);
            }
            if(_runFlag_3)
            {
                if(!_S455._S433)
                {
                    _runFlag_4 = false;
                    _S439 = int(0);
                }
                else
                {
                    _runFlag_4 = _runFlag_3;
                }
                if(_runFlag_4)
                {
                    float3  _S476 = s_primal_ctx_unproject_raydir_0(_S455._S432, camera_model_6, is_ray_depth_6);
                    points_3[int(2)] = make_float3 (depths_1.z) * _S476;
                    bool _S477 = !!_S455._S435;
                    int _S478;
                    if(_S477)
                    {
                        float3  _S479 = s_primal_ctx_unproject_raydir_0(_S455._S434, camera_model_6, is_ray_depth_6);
                        points_3[int(3)] = make_float3 (depths_1.w) * _S479;
                        _S478 = int(2);
                        _S462 = _S479;
                    }
                    else
                    {
                        _S478 = int(0);
                        _S462 = _S456;
                    }
                    if(_S478 != int(2))
                    {
                        _runFlag_5 = false;
                        _S439 = _S478;
                    }
                    else
                    {
                        _runFlag_5 = _runFlag_4;
                    }
                    if(_runFlag_5)
                    {
                        _S439 = int(1);
                    }
                    _runFlag_5 = _S477;
                    _S463 = _S476;
                }
                else
                {
                    _runFlag_5 = false;
                    _S462 = _S456;
                    _S463 = _S456;
                }
            }
            else
            {
                _runFlag_4 = false;
                _runFlag_5 = false;
                _S462 = _S456;
                _S463 = _S456;
            }
            float3  _S480 = _S461;
            _S461 = _S462;
            _S462 = _S463;
            _S458 = _S473;
            _S463 = _S480;
            _S464 = _S471;
        }
        else
        {
            _S439 = int(0);
            points_3[int(0)] = _S456;
            points_3[int(1)] = _S456;
            points_3[int(2)] = _S456;
            points_3[int(3)] = _S456;
            _runFlag_3 = false;
            _runFlag_4 = false;
            _runFlag_5 = false;
            _S461 = _S456;
            _S462 = _S456;
            _S458 = false;
            _S463 = _S456;
            _S464 = _S456;
        }
        if(_S439 != int(1))
        {
            _runFlag_6 = false;
        }
        else
        {
            _runFlag_6 = _S457;
        }
        if(_runFlag_6)
        {
            float3  dx_1 = points_3[int(1)] - points_3[int(0)];
            float3  _S481 = - (points_3[int(3)] - points_3[int(2)]);
            float3  _S482 = s_primal_ctx_cross_0(dx_1, _S481);
            bool _S483 = (s_primal_ctx_dot_0(_S482, _S482)) != 0.0f;
            if(_S483)
            {
                float _S484 = length_0(_S482);
                float3  _S485 = make_float3 (_S484);
                _S465 = make_float3 (_S484 * _S484);
                _S466 = _S485;
            }
            else
            {
                _S465 = _S456;
                _S466 = _S456;
            }
            float3  _S486 = _S466;
            _S459 = _S483;
            _S466 = _S482;
            _S467 = _S486;
            _S468 = dx_1;
            _S469 = _S481;
        }
        else
        {
            _S459 = false;
            _S465 = _S456;
            _S466 = _S456;
            _S467 = _S456;
            _S468 = _S456;
            _S469 = _S456;
        }
        bool _S487 = _runFlag_3;
        bool _S488 = _runFlag_4;
        bool _S489 = _runFlag_5;
        float3  _S490 = _S461;
        float3  _S491 = _S462;
        bool _S492 = _S458;
        float3  _S493 = _S463;
        float3  _S494 = _S464;
        _runFlag_3 = _runFlag_6;
        _runFlag_4 = _S459;
        _S461 = _S465;
        _S462 = _S466;
        _S463 = _S467;
        _S464 = _S468;
        _S465 = _S469;
        _runFlag_5 = _S470;
        _S458 = _S487;
        _runFlag_6 = _S488;
        _S459 = _S489;
        _S466 = _S490;
        _S467 = _S491;
        _S460 = _S492;
        _S468 = _S493;
        _S469 = _S494;
    }
    else
    {
        _runFlag_3 = false;
        _runFlag_4 = false;
        _S461 = _S456;
        _S462 = _S456;
        _S463 = _S456;
        _S464 = _S456;
        _S465 = _S456;
        _runFlag_5 = false;
        _S458 = false;
        _runFlag_6 = false;
        _S459 = false;
        _S466 = _S456;
        _S467 = _S456;
        _S460 = false;
        _S468 = _S456;
        _S469 = _S456;
    }
    float4  _S495 = make_float4 (0.0f);
    float4  _S496;
    if(_S457)
    {
        if(_runFlag_3)
        {
            if(_runFlag_4)
            {
                float3  _S497 = v_normal_1 / _S461;
                float3  _S498 = _S462 * - _S497;
                float3  _S499 = _S463 * _S497;
                float _S500 = _S498.x + _S498.y + _S498.z;
                DiffPair_vectorx3Cfloatx2C3x3E_0 _S501;
                (&_S501)->primal_0 = _S462;
                (&_S501)->differential_0 = _S456;
                s_bwd_length_impl_0(&_S501, _S500);
                _S461 = _S499 + _S501.differential_0;
            }
            else
            {
                _S461 = v_normal_1;
            }
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S502;
            (&_S502)->primal_0 = _S462;
            (&_S502)->differential_0 = _S456;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S503;
            (&_S503)->primal_0 = _S462;
            (&_S503)->differential_0 = _S456;
            s_bwd_prop_dot_0(&_S502, &_S503, 0.0f);
            float3  _S504 = _S503.differential_0 + _S502.differential_0 + _S461;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S505;
            (&_S505)->primal_0 = _S464;
            (&_S505)->differential_0 = _S456;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S506;
            (&_S506)->primal_0 = _S465;
            (&_S506)->differential_0 = _S456;
            s_bwd_prop_cross_0(&_S505, &_S506, _S504);
            float3  s_diff_dy_T_1 = - _S506.differential_0;
            float3  _S507 = - s_diff_dy_T_1;
            float3  _S508 = - _S505.differential_0;
            FixedArray<float3 , 4>  _S509;
            _S509[int(0)] = _S456;
            _S509[int(1)] = _S456;
            _S509[int(2)] = _S456;
            _S509[int(3)] = _S456;
            _S509[int(2)] = _S507;
            _S509[int(3)] = s_diff_dy_T_1;
            _S509[int(0)] = _S508;
            _S509[int(1)] = _S505.differential_0;
            points_3[int(0)] = _S509[int(0)];
            points_3[int(1)] = _S509[int(1)];
            points_3[int(2)] = _S509[int(2)];
            points_3[int(3)] = _S509[int(3)];
        }
        else
        {
            points_3[int(0)] = _S456;
            points_3[int(1)] = _S456;
            points_3[int(2)] = _S456;
            points_3[int(3)] = _S456;
        }
        if(_runFlag_5)
        {
            if(_S458)
            {
                if(_runFlag_6)
                {
                    FixedArray<float3 , 4>  _S510 = points_3;
                    FixedArray<float3 , 4>  _S511 = points_3;
                    FixedArray<float3 , 4>  _S512 = points_3;
                    FixedArray<float3 , 4>  _S513 = points_3;
                    if(_S459)
                    {
                        float3  _S514 = _S466 * _S513[int(3)];
                        float _S515 = _S514.x + _S514.y + _S514.z;
                        float4  _S516 = _S495;
                        *&((&_S516)->w) = _S515;
                        points_3[int(0)] = _S510[int(0)];
                        points_3[int(1)] = _S511[int(1)];
                        points_3[int(2)] = _S512[int(2)];
                        points_3[int(3)] = _S456;
                        _S496 = _S516;
                    }
                    else
                    {
                        points_3[int(0)] = _S510[int(0)];
                        points_3[int(1)] = _S511[int(1)];
                        points_3[int(2)] = _S512[int(2)];
                        points_3[int(3)] = _S513[int(3)];
                        _S496 = _S495;
                    }
                    float3  _S517 = _S467 * points_3[int(2)];
                    float _S518 = _S517.x + _S517.y + _S517.z;
                    FixedArray<float3 , 4>  _S519 = points_3;
                    FixedArray<float3 , 4>  _S520 = points_3;
                    float4  _S521 = _S495;
                    *&((&_S521)->z) = _S518;
                    float4  _S522 = _S496 + _S521;
                    points_3[int(0)] = points_3[int(0)];
                    points_3[int(1)] = _S519[int(1)];
                    points_3[int(2)] = _S456;
                    points_3[int(3)] = _S520[int(3)];
                    _S496 = _S522;
                }
                else
                {
                    FixedArray<float3 , 4>  _S523 = points_3;
                    FixedArray<float3 , 4>  _S524 = points_3;
                    FixedArray<float3 , 4>  _S525 = points_3;
                    points_3[int(0)] = points_3[int(0)];
                    points_3[int(1)] = _S523[int(1)];
                    points_3[int(2)] = _S524[int(2)];
                    points_3[int(3)] = _S525[int(3)];
                    _S496 = _S495;
                }
            }
            else
            {
                FixedArray<float3 , 4>  _S526 = points_3;
                FixedArray<float3 , 4>  _S527 = points_3;
                FixedArray<float3 , 4>  _S528 = points_3;
                points_3[int(0)] = points_3[int(0)];
                points_3[int(1)] = _S526[int(1)];
                points_3[int(2)] = _S527[int(2)];
                points_3[int(3)] = _S528[int(3)];
                _S496 = _S495;
            }
            if(_S460)
            {
                FixedArray<float3 , 4>  _S529 = points_3;
                float3  _S530 = _S468 * points_3[int(1)];
                float _S531 = _S530.x + _S530.y + _S530.z;
                float4  _S532 = _S495;
                *&((&_S532)->y) = _S531;
                float4  _S533 = _S496 + _S532;
                points_3[int(0)] = _S456;
                points_3[int(1)] = _S456;
                points_3[int(2)] = _S456;
                points_3[int(3)] = _S456;
                _S461 = _S529[int(0)];
                _S496 = _S533;
            }
            else
            {
                FixedArray<float3 , 4>  _S534 = points_3;
                FixedArray<float3 , 4>  _S535 = points_3;
                FixedArray<float3 , 4>  _S536 = points_3;
                points_3[int(0)] = points_3[int(0)];
                points_3[int(1)] = _S534[int(1)];
                points_3[int(2)] = _S535[int(2)];
                points_3[int(3)] = _S536[int(3)];
                _S461 = _S456;
            }
            float3  _S537 = _S469 * (points_3[int(0)] + _S461);
            float _S538 = _S537.x + _S537.y + _S537.z;
            float4  _S539 = _S495;
            *&((&_S539)->x) = _S538;
            _S496 = _S496 + _S539;
        }
        else
        {
            _S496 = _S495;
        }
    }
    else
    {
        _S496 = _S495;
    }
    *v_depths_0 = _S496;
    return;
}

inline __device__ float ray_depth_to_linear_depth_factor_none(float2  pix_center_2, float4  intrins_5, FixedArray<float, 1>  dist_coeffs_8, int camera_model_7)
{
    float _S540;
    for(;;)
    {
        float2  uv_17 = (pix_center_2 - float2 {intrins_5.z, intrins_5.w}) / float2 {intrins_5.x, intrins_5.y};
        FixedArray<float, 1>  _S541 = dist_coeffs_8;
        float2  uv_u_6;
        bool _S542 = undistort_point_0(uv_17, &_S541, int(12), &uv_u_6);
        if(!_S542)
        {
            _S540 = 0.0f;
            break;
        }
        float3  raydir_2 = unproject_raydir_0(uv_u_6, camera_model_7, false);
        _S540 = float((F32_sign((raydir_2.z)))) / length_0(raydir_2);
        break;
    }
    return _S540;
}

inline __device__ float depth_normal_loss_none(float2  pix_center_3, float4  intrins_6, FixedArray<float, 1>  dist_coeffs_9, int camera_model_8, bool is_ray_depth_7, float4  depths_2, float3  gt_normal_0)
{
    float _S543;
    for(;;)
    {
        float3  _S544;
        float3  * _S545;
        float3  * _S546;
        float3  * _S547;
        float3  * _S548;
        int _S549;
        FixedArray<float3 , 5>  points_4;
        for(;;)
        {
            float2  _S550 = float2 {intrins_6.z, intrins_6.w};
            float2  _S551 = float2 {intrins_6.x, intrins_6.y};
            float2  uv_18 = (pix_center_3 + make_float2 (-1.0f, -0.0f) - _S550) / _S551;
            FixedArray<float, 1>  _S552 = dist_coeffs_9;
            float2  uv_u_7;
            bool _S553 = undistort_point_0(uv_18, &_S552, int(12), &uv_u_7);
            float3  _S554 = make_float3 (0.0f);
            if(!_S553)
            {
                _S549 = int(0);
                _S548 = nullptr;
                _S547 = nullptr;
                _S546 = nullptr;
                _S545 = nullptr;
                _S544 = _S554;
                break;
            }
            float3  raydir_3 = unproject_raydir_0(uv_u_7, camera_model_8, is_ray_depth_7);
            points_4[int(0)] = make_float3 (depths_2.x) * raydir_3;
            float2  uv_19 = (pix_center_3 + make_float2 (1.0f, -0.0f) - _S550) / _S551;
            FixedArray<float, 1>  _S555 = dist_coeffs_9;
            float2  uv_u_8;
            bool _S556 = undistort_point_0(uv_19, &_S555, int(12), &uv_u_8);
            if(!_S556)
            {
                _S549 = int(0);
                _S548 = nullptr;
                _S547 = &points_4[int(0)];
                _S546 = nullptr;
                _S545 = nullptr;
                _S544 = _S554;
                break;
            }
            float3  raydir_4 = unproject_raydir_0(uv_u_8, camera_model_8, is_ray_depth_7);
            points_4[int(1)] = make_float3 (depths_2.y) * raydir_4;
            float2  uv_20 = (pix_center_3 + make_float2 (0.0f, -1.0f) - _S550) / _S551;
            FixedArray<float, 1>  _S557 = dist_coeffs_9;
            float2  uv_u_9;
            bool _S558 = undistort_point_0(uv_20, &_S557, int(12), &uv_u_9);
            if(!_S558)
            {
                _S549 = int(0);
                _S548 = &points_4[int(1)];
                _S547 = &points_4[int(0)];
                _S546 = nullptr;
                _S545 = nullptr;
                _S544 = _S554;
                break;
            }
            float3  raydir_5 = unproject_raydir_0(uv_u_9, camera_model_8, is_ray_depth_7);
            points_4[int(2)] = make_float3 (depths_2.z) * raydir_5;
            float2  uv_21 = (pix_center_3 + make_float2 (0.0f, 1.0f) - _S550) / _S551;
            FixedArray<float, 1>  _S559 = dist_coeffs_9;
            float2  uv_u_10;
            bool _S560 = undistort_point_0(uv_21, &_S559, int(12), &uv_u_10);
            if(!_S560)
            {
                _S549 = int(0);
                _S548 = &points_4[int(1)];
                _S547 = &points_4[int(0)];
                _S546 = nullptr;
                _S545 = &points_4[int(2)];
                _S544 = _S554;
                break;
            }
            float3  raydir_6 = unproject_raydir_0(uv_u_10, camera_model_8, is_ray_depth_7);
            points_4[int(3)] = make_float3 (depths_2.w) * raydir_6;
            float2  uv_22 = (pix_center_3 + make_float2 (0.0f) * make_float2 (0.0f, 3.0f) - _S550) / _S551;
            FixedArray<float, 1>  _S561 = dist_coeffs_9;
            float2  uv_u_11;
            bool _S562 = undistort_point_0(uv_22, &_S561, int(12), &uv_u_11);
            if(!_S562)
            {
                _S549 = int(0);
                _S548 = &points_4[int(1)];
                _S547 = &points_4[int(0)];
                _S546 = &points_4[int(3)];
                _S545 = &points_4[int(2)];
                _S544 = _S554;
                break;
            }
            float3  raydir_7 = unproject_raydir_0(uv_u_11, camera_model_8, is_ray_depth_7);
            _S549 = int(1);
            _S548 = &points_4[int(1)];
            _S547 = &points_4[int(0)];
            _S546 = &points_4[int(3)];
            _S545 = &points_4[int(2)];
            _S544 = raydir_7;
            break;
        }
        if(_S549 != int(1))
        {
            _S543 = 0.0f;
            break;
        }
        float3  normal_4 = cross_0(*_S548 - *_S547, - (*_S546 - *_S545));
        float3  normal_5;
        if((dot_0(normal_4, normal_4)) != 0.0f)
        {
            normal_5 = normalize_0(normal_4);
        }
        else
        {
            normal_5 = normal_4;
        }
        float3  _S563;
        if((dot_0(gt_normal_0, gt_normal_0)) != 0.0f)
        {
            _S563 = normalize_0(gt_normal_0);
        }
        else
        {
            _S563 = gt_normal_0;
        }
        _S543 = (1.0f - dot_0(normal_5, _S563) + 0.00100000004749745f) / ((F32_max((dot_0(normal_5, - normalize_0(_S544))), (0.0f))) + 0.00100000004749745f);
        break;
    }
    return _S543;
}

struct s_bwd_prop_depth_normal_loss_Intermediates_0
{
    float2  _S564;
    bool _S565;
    float2  _S566;
    bool _S567;
    float2  _S568;
    bool _S569;
    float2  _S570;
    bool _S571;
    float2  _S572;
    bool _S573;
};

inline __device__ void s_bwd_prop_normalize_impl_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_14, float3  _s_dOut_9)
{
    float _S574 = length_0((*dpx_14).primal_0);
    float3  _S575 = (*dpx_14).primal_0 * _s_dOut_9;
    float3  _S576 = make_float3 (1.0f / _S574) * _s_dOut_9;
    float _S577 = - ((_S575.x + _S575.y + _S575.z) / (_S574 * _S574));
    float3  _S578 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S579;
    (&_S579)->primal_0 = (*dpx_14).primal_0;
    (&_S579)->differential_0 = _S578;
    s_bwd_length_impl_0(&_S579, _S577);
    float3  _S580 = _S576 + _S579.differential_0;
    dpx_14->primal_0 = (*dpx_14).primal_0;
    dpx_14->differential_0 = _S580;
    return;
}

inline __device__ void s_bwd_normalize_impl_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S581, float3  _S582)
{
    s_bwd_prop_normalize_impl_0(_S581, _S582);
    return;
}

inline __device__ void depth_normal_loss_vjp_none(float2  pix_center_4, float4  intrins_7, FixedArray<float, 1>  dist_coeffs_10, int camera_model_9, bool is_ray_depth_8, float4  depths_3, float3  gt_normal_1, float v_loss_0, float4  * v_depths_1, float3  * v_gt_normal_0)
{
    float2  _S583 = make_float2 (0.0f);
    s_bwd_prop_depth_normal_loss_Intermediates_0 _S584;
    (&_S584)->_S564 = _S583;
    (&_S584)->_S565 = false;
    (&_S584)->_S566 = _S583;
    (&_S584)->_S567 = false;
    (&_S584)->_S568 = _S583;
    (&_S584)->_S569 = false;
    (&_S584)->_S570 = _S583;
    (&_S584)->_S571 = false;
    (&_S584)->_S572 = _S583;
    (&_S584)->_S573 = false;
    (&_S584)->_S566 = _S583;
    (&_S584)->_S567 = false;
    (&_S584)->_S568 = _S583;
    (&_S584)->_S569 = false;
    (&_S584)->_S570 = _S583;
    (&_S584)->_S571 = false;
    (&_S584)->_S572 = _S583;
    (&_S584)->_S573 = false;
    float2  _S585 = float2 {intrins_7.z, intrins_7.w};
    float2  _S586 = float2 {intrins_7.x, intrins_7.y};
    float2  uv_23 = (pix_center_4 + make_float2 (-1.0f, -0.0f) - _S585) / _S586;
    float2  _S587 = _S583;
    FixedArray<float, 1>  _S588 = dist_coeffs_10;
    bool _S589 = undistort_point_0(uv_23, &_S588, int(12), &_S587);
    (&_S584)->_S564 = _S587;
    (&_S584)->_S565 = _S589;
    bool _S590 = !!_S589;
    bool _runFlag_7;
    if(_S590)
    {
        float2  uv_24 = (pix_center_4 + make_float2 (1.0f, -0.0f) - _S585) / _S586;
        float2  _S591 = _S583;
        FixedArray<float, 1>  _S592 = dist_coeffs_10;
        bool _S593 = undistort_point_0(uv_24, &_S592, int(12), &_S591);
        (&_S584)->_S566 = _S591;
        (&_S584)->_S567 = _S593;
        if(!_S593)
        {
            _runFlag_7 = false;
        }
        else
        {
            _runFlag_7 = _S590;
        }
        if(_runFlag_7)
        {
            float2  uv_25 = (pix_center_4 + make_float2 (0.0f, -1.0f) - _S585) / _S586;
            float2  _S594 = _S583;
            FixedArray<float, 1>  _S595 = dist_coeffs_10;
            bool _S596 = undistort_point_0(uv_25, &_S595, int(12), &_S594);
            (&_S584)->_S568 = _S594;
            (&_S584)->_S569 = _S596;
            if(!_S596)
            {
                _runFlag_7 = false;
            }
            if(_runFlag_7)
            {
                float2  uv_26 = (pix_center_4 + make_float2 (0.0f, 1.0f) - _S585) / _S586;
                float2  _S597 = _S583;
                FixedArray<float, 1>  _S598 = dist_coeffs_10;
                bool _S599 = undistort_point_0(uv_26, &_S598, int(12), &_S597);
                (&_S584)->_S570 = _S597;
                (&_S584)->_S571 = _S599;
                if(!_S599)
                {
                    _runFlag_7 = false;
                }
                if(_runFlag_7)
                {
                    float2  uv_27 = (pix_center_4 - _S585) / _S586;
                    float2  _S600 = _S583;
                    FixedArray<float, 1>  _S601 = dist_coeffs_10;
                    bool _S602 = undistort_point_0(uv_27, &_S601, int(12), &_S600);
                    (&_S584)->_S572 = _S600;
                    (&_S584)->_S573 = _S602;
                }
            }
        }
    }
    s_bwd_prop_depth_normal_loss_Intermediates_0 _S603 = _S584;
    float3  _S604 = make_float3 (0.0f);
    bool _S605 = !!_S584._S565;
    bool _runFlag_8;
    bool _runFlag_9;
    bool _runFlag_10;
    int _S606;
    float3  raydir_8;
    float3  _S607;
    float3  _S608;
    float3  _S609;
    float3  _S610;
    FixedArray<float3 , 5>  points_5;
    if(_S605)
    {
        float3  _S611 = s_primal_ctx_unproject_raydir_0(_S603._S564, camera_model_9, is_ray_depth_8);
        float3  _S612 = make_float3 (depths_3.x) * _S611;
        if(!_S603._S567)
        {
            _runFlag_7 = false;
        }
        else
        {
            _runFlag_7 = _S605;
        }
        if(_runFlag_7)
        {
            float3  _S613 = s_primal_ctx_unproject_raydir_0(_S603._S566, camera_model_9, is_ray_depth_8);
            float3  _S614 = make_float3 (depths_3.y) * _S613;
            if(!_S603._S569)
            {
                _runFlag_8 = false;
            }
            else
            {
                _runFlag_8 = _runFlag_7;
            }
            if(_runFlag_8)
            {
                float3  _S615 = s_primal_ctx_unproject_raydir_0(_S603._S568, camera_model_9, is_ray_depth_8);
                float3  _S616 = make_float3 (depths_3.z) * _S615;
                if(!_S603._S571)
                {
                    _runFlag_9 = false;
                }
                else
                {
                    _runFlag_9 = _runFlag_8;
                }
                if(_runFlag_9)
                {
                    float3  _S617 = s_primal_ctx_unproject_raydir_0(_S603._S570, camera_model_9, is_ray_depth_8);
                    float3  _S618 = make_float3 (depths_3.w) * _S617;
                    if(!_S603._S573)
                    {
                        _runFlag_10 = false;
                    }
                    else
                    {
                        _runFlag_10 = _runFlag_9;
                    }
                    if(_runFlag_10)
                    {
                        float3  _S619 = s_primal_ctx_unproject_raydir_0(_S603._S572, camera_model_9, is_ray_depth_8);
                        _S606 = int(1);
                        raydir_8 = _S619;
                    }
                    else
                    {
                        _S606 = int(0);
                        raydir_8 = _S617;
                    }
                    points_5[int(0)] = _S612;
                    points_5[int(1)] = _S614;
                    points_5[int(2)] = _S616;
                    points_5[int(3)] = _S618;
                    points_5[int(4)] = _S604;
                    _S607 = _S617;
                }
                else
                {
                    _S606 = int(0);
                    raydir_8 = _S615;
                    points_5[int(0)] = _S612;
                    points_5[int(1)] = _S614;
                    points_5[int(2)] = _S616;
                    points_5[int(3)] = _S604;
                    points_5[int(4)] = _S604;
                    _S607 = _S604;
                }
                _S608 = _S615;
            }
            else
            {
                _S606 = int(0);
                raydir_8 = _S613;
                points_5[int(0)] = _S612;
                points_5[int(1)] = _S614;
                points_5[int(2)] = _S604;
                points_5[int(3)] = _S604;
                points_5[int(4)] = _S604;
                _runFlag_9 = false;
                _S607 = _S604;
                _S608 = _S604;
            }
            _S609 = _S613;
        }
        else
        {
            _S606 = int(0);
            raydir_8 = _S611;
            points_5[int(0)] = _S612;
            points_5[int(1)] = _S604;
            points_5[int(2)] = _S604;
            points_5[int(3)] = _S604;
            points_5[int(4)] = _S604;
            _runFlag_8 = false;
            _runFlag_9 = false;
            _S607 = _S604;
            _S608 = _S604;
            _S609 = _S604;
        }
        _S610 = _S611;
    }
    else
    {
        _S606 = int(0);
        points_5[int(0)] = _S604;
        points_5[int(1)] = _S604;
        points_5[int(2)] = _S604;
        points_5[int(3)] = _S604;
        points_5[int(4)] = _S604;
        _runFlag_7 = false;
        _runFlag_8 = false;
        _runFlag_9 = false;
        _S607 = _S604;
        _S608 = _S604;
        _S609 = _S604;
        _S610 = _S604;
    }
    bool _S620 = !(_S606 != int(1));
    bool _S621;
    float3  normal_6;
    float3  _S622;
    float3  _S623;
    float3  _S624;
    float3  _S625;
    float _S626;
    float _S627;
    float _S628;
    float _S629;
    if(_S620)
    {
        float3  dx_2 = points_5[int(1)] - points_5[int(0)];
        float3  _S630 = - (points_5[int(3)] - points_5[int(2)]);
        float3  _S631 = s_primal_ctx_cross_0(dx_2, _S630);
        bool _S632 = (s_primal_ctx_dot_0(_S631, _S631)) != 0.0f;
        if(_S632)
        {
            normal_6 = normalize_0(_S631);
        }
        else
        {
            normal_6 = _S631;
        }
        bool _S633 = (s_primal_ctx_dot_0(gt_normal_1, gt_normal_1)) != 0.0f;
        if(_S633)
        {
            _S622 = normalize_0(gt_normal_1);
        }
        else
        {
            _S622 = gt_normal_1;
        }
        float3  _S634 = - normalize_0(raydir_8);
        float _S635 = s_primal_ctx_dot_0(normal_6, _S634);
        float _S636 = 1.0f - s_primal_ctx_dot_0(normal_6, _S622) + 0.00100000004749745f;
        float _S637 = (F32_max((_S635), (0.0f))) + 0.00100000004749745f;
        _S626 = _S637 * _S637;
        _S627 = _S636;
        _S628 = _S637;
        _S629 = _S635;
        raydir_8 = normal_6;
        normal_6 = _S634;
        _runFlag_10 = _S633;
        _S621 = _S632;
        _S623 = _S631;
        _S624 = dx_2;
        _S625 = _S630;
    }
    else
    {
        _S626 = 0.0f;
        _S627 = 0.0f;
        _S628 = 0.0f;
        _S629 = 0.0f;
        raydir_8 = _S604;
        normal_6 = _S604;
        _S622 = _S604;
        _runFlag_10 = false;
        _S621 = false;
        _S623 = _S604;
        _S624 = _S604;
        _S625 = _S604;
    }
    float4  _S638 = make_float4 (0.0f);
    if(_S620)
    {
        float _S639 = v_loss_0 / _S626;
        float _S640 = _S627 * - _S639;
        float s_diff_num_T_0 = _S628 * _S639;
        DiffPair_float_0 _S641;
        (&_S641)->primal_0 = _S629;
        (&_S641)->differential_0 = 0.0f;
        DiffPair_float_0 _S642;
        (&_S642)->primal_0 = 0.0f;
        (&_S642)->differential_0 = 0.0f;
        _d_max_0(&_S641, &_S642, _S640);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S643;
        (&_S643)->primal_0 = raydir_8;
        (&_S643)->differential_0 = _S604;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S644;
        (&_S644)->primal_0 = normal_6;
        (&_S644)->differential_0 = _S604;
        s_bwd_prop_dot_0(&_S643, &_S644, _S641.differential_0);
        float _S645 = - s_diff_num_T_0;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S646;
        (&_S646)->primal_0 = raydir_8;
        (&_S646)->differential_0 = _S604;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S647;
        (&_S647)->primal_0 = _S622;
        (&_S647)->differential_0 = _S604;
        s_bwd_prop_dot_0(&_S646, &_S647, _S645);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S648 = _S647;
        float3  _S649 = _S643.differential_0 + _S646.differential_0;
        if(_runFlag_10)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S650;
            (&_S650)->primal_0 = gt_normal_1;
            (&_S650)->differential_0 = _S604;
            s_bwd_normalize_impl_0(&_S650, _S648.differential_0);
            raydir_8 = _S650.differential_0;
        }
        else
        {
            raydir_8 = _S648.differential_0;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S651;
        (&_S651)->primal_0 = gt_normal_1;
        (&_S651)->differential_0 = _S604;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S652;
        (&_S652)->primal_0 = gt_normal_1;
        (&_S652)->differential_0 = _S604;
        s_bwd_prop_dot_0(&_S651, &_S652, 0.0f);
        float3  _S653 = _S652.differential_0 + _S651.differential_0 + raydir_8;
        if(_S621)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S654;
            (&_S654)->primal_0 = _S623;
            (&_S654)->differential_0 = _S604;
            s_bwd_normalize_impl_0(&_S654, _S649);
            raydir_8 = _S654.differential_0;
        }
        else
        {
            raydir_8 = _S649;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S655;
        (&_S655)->primal_0 = _S623;
        (&_S655)->differential_0 = _S604;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S656;
        (&_S656)->primal_0 = _S623;
        (&_S656)->differential_0 = _S604;
        s_bwd_prop_dot_0(&_S655, &_S656, 0.0f);
        float3  _S657 = _S656.differential_0 + _S655.differential_0 + raydir_8;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S658;
        (&_S658)->primal_0 = _S624;
        (&_S658)->differential_0 = _S604;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S659;
        (&_S659)->primal_0 = _S625;
        (&_S659)->differential_0 = _S604;
        s_bwd_prop_cross_0(&_S658, &_S659, _S657);
        float3  s_diff_dy_T_2 = - _S659.differential_0;
        float3  _S660 = - s_diff_dy_T_2;
        float3  _S661 = - _S658.differential_0;
        FixedArray<float3 , 5>  _S662;
        _S662[int(0)] = _S604;
        _S662[int(1)] = _S604;
        _S662[int(2)] = _S604;
        _S662[int(3)] = _S604;
        _S662[int(4)] = _S604;
        _S662[int(2)] = _S660;
        _S662[int(3)] = s_diff_dy_T_2;
        _S662[int(0)] = _S661;
        _S662[int(1)] = _S658.differential_0;
        points_5[int(0)] = _S662[int(0)];
        points_5[int(1)] = _S662[int(1)];
        points_5[int(2)] = _S662[int(2)];
        points_5[int(3)] = _S662[int(3)];
        points_5[int(4)] = _S662[int(4)];
        raydir_8 = _S653;
    }
    else
    {
        points_5[int(0)] = _S604;
        points_5[int(1)] = _S604;
        points_5[int(2)] = _S604;
        points_5[int(3)] = _S604;
        points_5[int(4)] = _S604;
        raydir_8 = _S604;
    }
    float4  _S663;
    if(_S605)
    {
        if(_runFlag_7)
        {
            if(_runFlag_8)
            {
                if(_runFlag_9)
                {
                    FixedArray<float3 , 5>  _S664 = points_5;
                    FixedArray<float3 , 5>  _S665 = points_5;
                    FixedArray<float3 , 5>  _S666 = points_5;
                    float3  _S667 = _S607 * points_5[int(3)];
                    float _S668 = _S667.x + _S667.y + _S667.z;
                    float4  _S669 = _S638;
                    *&((&_S669)->w) = _S668;
                    points_5[int(0)] = _S604;
                    points_5[int(1)] = _S604;
                    points_5[int(2)] = _S604;
                    points_5[int(3)] = _S604;
                    points_5[int(4)] = _S604;
                    _S607 = _S666[int(2)];
                    normal_6 = _S664[int(0)];
                    _S622 = _S665[int(1)];
                    _S663 = _S669;
                }
                else
                {
                    FixedArray<float3 , 5>  _S670 = points_5;
                    FixedArray<float3 , 5>  _S671 = points_5;
                    FixedArray<float3 , 5>  _S672 = points_5;
                    FixedArray<float3 , 5>  _S673 = points_5;
                    points_5[int(0)] = points_5[int(0)];
                    points_5[int(1)] = _S670[int(1)];
                    points_5[int(2)] = _S671[int(2)];
                    points_5[int(3)] = _S672[int(3)];
                    points_5[int(4)] = _S673[int(4)];
                    _S607 = _S604;
                    normal_6 = _S604;
                    _S622 = _S604;
                    _S663 = _S638;
                }
                float3  _S674 = _S608 * (points_5[int(2)] + _S607);
                float _S675 = _S674.x + _S674.y + _S674.z;
                float3  _S676 = points_5[int(0)] + normal_6;
                float3  _S677 = points_5[int(1)] + _S622;
                float4  _S678 = _S638;
                *&((&_S678)->z) = _S675;
                float4  _S679 = _S663 + _S678;
                points_5[int(0)] = _S604;
                points_5[int(1)] = _S604;
                points_5[int(2)] = _S604;
                points_5[int(3)] = _S604;
                points_5[int(4)] = _S604;
                _S607 = _S677;
                _S608 = _S676;
                _S663 = _S679;
            }
            else
            {
                FixedArray<float3 , 5>  _S680 = points_5;
                FixedArray<float3 , 5>  _S681 = points_5;
                FixedArray<float3 , 5>  _S682 = points_5;
                FixedArray<float3 , 5>  _S683 = points_5;
                points_5[int(0)] = points_5[int(0)];
                points_5[int(1)] = _S680[int(1)];
                points_5[int(2)] = _S681[int(2)];
                points_5[int(3)] = _S682[int(3)];
                points_5[int(4)] = _S683[int(4)];
                _S607 = _S604;
                _S608 = _S604;
                _S663 = _S638;
            }
            float3  _S684 = _S609 * (points_5[int(1)] + _S607);
            float _S685 = _S684.x + _S684.y + _S684.z;
            float3  _S686 = points_5[int(0)] + _S608;
            float4  _S687 = _S638;
            *&((&_S687)->y) = _S685;
            float4  _S688 = _S663 + _S687;
            points_5[int(0)] = _S604;
            points_5[int(1)] = _S604;
            points_5[int(2)] = _S604;
            points_5[int(3)] = _S604;
            points_5[int(4)] = _S604;
            _S607 = _S686;
            _S663 = _S688;
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
            _S607 = _S604;
            _S663 = _S638;
        }
        float3  _S693 = _S610 * (points_5[int(0)] + _S607);
        float _S694 = _S693.x + _S693.y + _S693.z;
        float4  _S695 = _S638;
        *&((&_S695)->x) = _S694;
        _S663 = _S663 + _S695;
    }
    else
    {
        _S663 = _S638;
    }
    *v_depths_1 = _S663;
    *v_gt_normal_0 = raydir_8;
    return;
}

inline __device__ float3  generate_ray_d2n_opencv(float2  pix_pos_3, float4  intrins_8, FixedArray<float, 4>  dist_coeffs_11, int camera_model_10, bool is_ray_depth_9)
{
    float3  _S696;
    for(;;)
    {
        float2  uv_28 = (pix_pos_3 - float2 {intrins_8.z, intrins_8.w}) / float2 {intrins_8.x, intrins_8.y};
        FixedArray<float, 4>  _S697 = dist_coeffs_11;
        float2  uv_u_12;
        bool _S698 = undistort_point_1(uv_28, &_S697, int(12), &uv_u_12);
        if(!_S698)
        {
            int3  _S699 = make_int3 (int(0));
            float3  _S700 = make_float3 ((float)_S699.x, (float)_S699.y, (float)_S699.z);
            _S696 = _S700;
            break;
        }
        _S696 = unproject_raydir_0(uv_u_12, camera_model_10, is_ray_depth_9);
        break;
    }
    return _S696;
}

inline __device__ float3  depth_to_point_opencv(float2  pix_pos_4, float4  intrins_9, FixedArray<float, 4>  dist_coeffs_12, int camera_model_11, bool is_ray_depth_10, float depth_4)
{
    float3  _S701;
    for(;;)
    {
        float2  uv_29 = (pix_pos_4 - float2 {intrins_9.z, intrins_9.w}) / float2 {intrins_9.x, intrins_9.y};
        FixedArray<float, 4>  _S702 = dist_coeffs_12;
        float2  uv_u_13;
        bool _S703 = undistort_point_1(uv_29, &_S702, int(12), &uv_u_13);
        if(!_S703)
        {
            _S701 = make_float3 (0.0f);
            break;
        }
        _S701 = make_float3 (depth_4) * unproject_raydir_0(uv_u_13, camera_model_11, is_ray_depth_10);
        break;
    }
    return _S701;
}

struct s_bwd_prop_depth_to_point_Intermediates_1
{
    float2  _S704;
    bool _S705;
};

inline __device__ float depth_to_point_vjp_opencv(float2  pix_pos_5, float4  intrins_10, FixedArray<float, 4>  dist_coeffs_13, int camera_model_12, bool is_ray_depth_11, float depth_5, float3  v_point_1)
{
    float2  _S706 = make_float2 (0.0f);
    s_bwd_prop_depth_to_point_Intermediates_1 _S707;
    (&_S707)->_S704 = _S706;
    (&_S707)->_S705 = false;
    float2  uv_30 = (pix_pos_5 - float2 {intrins_10.z, intrins_10.w}) / float2 {intrins_10.x, intrins_10.y};
    float2  _S708 = _S706;
    FixedArray<float, 4>  _S709 = dist_coeffs_13;
    bool _S710 = undistort_point_1(uv_30, &_S709, int(12), &_S708);
    (&_S707)->_S704 = _S708;
    (&_S707)->_S705 = _S710;
    s_bwd_prop_depth_to_point_Intermediates_1 _S711 = _S707;
    float3  _S712 = make_float3 (0.0f);
    bool _S713 = !!_S707._S705;
    float3  _S714;
    if(_S713)
    {
        _S714 = s_primal_ctx_unproject_raydir_0(_S711._S704, camera_model_12, is_ray_depth_11);
    }
    else
    {
        _S714 = _S712;
    }
    if(_S713)
    {
        _S714 = _S714 * v_point_1;
    }
    else
    {
        _S714 = _S712;
    }
    return _S714.x + _S714.y + _S714.z;
}

inline __device__ float3  depth_to_normal_opencv(float2  pix_center_5, float4  intrins_11, FixedArray<float, 4>  dist_coeffs_14, int camera_model_13, bool is_ray_depth_12, float4  depths_4)
{
    float3  normal_7;
    for(;;)
    {
        bool _S715;
        if((depths_4.x) == 0.0f)
        {
            _S715 = true;
        }
        else
        {
            _S715 = (depths_4.y) == 0.0f;
        }
        if(_S715)
        {
            _S715 = true;
        }
        else
        {
            _S715 = (depths_4.z) == 0.0f;
        }
        if(_S715)
        {
            _S715 = true;
        }
        else
        {
            _S715 = (depths_4.w) == 0.0f;
        }
        if(_S715)
        {
            normal_7 = make_float3 (0.0f);
            break;
        }
        float3  * _S716;
        float3  * _S717;
        float3  * _S718;
        float3  * _S719;
        int _S720;
        FixedArray<float3 , 4>  points_6;
        for(;;)
        {
            float2  _S721 = float2 {intrins_11.z, intrins_11.w};
            float2  _S722 = float2 {intrins_11.x, intrins_11.y};
            float2  uv_31 = (pix_center_5 + make_float2 (-1.0f, -0.0f) - _S721) / _S722;
            FixedArray<float, 4>  _S723 = dist_coeffs_14;
            float2  uv_u_14;
            bool _S724 = undistort_point_1(uv_31, &_S723, int(12), &uv_u_14);
            if(!_S724)
            {
                float3  _S725 = make_float3 (0.0f);
                _S720 = int(0);
                _S719 = nullptr;
                _S718 = nullptr;
                _S717 = nullptr;
                _S716 = nullptr;
                normal_7 = _S725;
                break;
            }
            points_6[int(0)] = make_float3 (depths_4.x) * unproject_raydir_0(uv_u_14, camera_model_13, is_ray_depth_12);
            for(;;)
            {
                float2  uv_32 = (pix_center_5 + make_float2 (1.0f, -0.0f) - _S721) / _S722;
                FixedArray<float, 4>  _S726 = dist_coeffs_14;
                float2  uv_u_15;
                bool _S727 = undistort_point_1(uv_32, &_S726, int(12), &uv_u_15);
                if(!_S727)
                {
                    float3  _S728 = make_float3 (0.0f);
                    _S720 = int(0);
                    _S719 = nullptr;
                    normal_7 = _S728;
                    break;
                }
                points_6[int(1)] = make_float3 (depths_4.y) * unproject_raydir_0(uv_u_15, camera_model_13, is_ray_depth_12);
                _S720 = int(2);
                _S719 = &points_6[int(1)];
                break;
            }
            if(_S720 != int(2))
            {
                _S718 = &points_6[int(0)];
                _S717 = nullptr;
                _S716 = nullptr;
                break;
            }
            float2  uv_33 = (pix_center_5 + make_float2 (0.0f, -1.0f) - _S721) / _S722;
            FixedArray<float, 4>  _S729 = dist_coeffs_14;
            float2  uv_u_16;
            bool _S730 = undistort_point_1(uv_33, &_S729, int(12), &uv_u_16);
            if(!_S730)
            {
                float3  _S731 = make_float3 (0.0f);
                _S720 = int(0);
                _S718 = &points_6[int(0)];
                _S717 = nullptr;
                _S716 = nullptr;
                normal_7 = _S731;
                break;
            }
            points_6[int(2)] = make_float3 (depths_4.z) * unproject_raydir_0(uv_u_16, camera_model_13, is_ray_depth_12);
            for(;;)
            {
                float2  uv_34 = (pix_center_5 + make_float2 (0.0f, 1.0f) - _S721) / _S722;
                FixedArray<float, 4>  _S732 = dist_coeffs_14;
                float2  uv_u_17;
                bool _S733 = undistort_point_1(uv_34, &_S732, int(12), &uv_u_17);
                if(!_S733)
                {
                    float3  _S734 = make_float3 (0.0f);
                    _S720 = int(0);
                    _S718 = nullptr;
                    normal_7 = _S734;
                    break;
                }
                points_6[int(3)] = make_float3 (depths_4.w) * unproject_raydir_0(uv_u_17, camera_model_13, is_ray_depth_12);
                _S720 = int(2);
                _S718 = &points_6[int(3)];
                break;
            }
            if(_S720 != int(2))
            {
                float3  * _S735 = _S718;
                _S718 = &points_6[int(0)];
                _S717 = _S735;
                _S716 = &points_6[int(2)];
                break;
            }
            float3  * _S736 = _S718;
            _S720 = int(1);
            _S718 = &points_6[int(0)];
            _S717 = _S736;
            _S716 = &points_6[int(2)];
            break;
        }
        if(_S720 != int(1))
        {
            break;
        }
        float3  normal_8 = cross_0(*_S719 - *_S718, - (*_S717 - *_S716));
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
    float2  _S737;
    bool _S738;
    float2  _S739;
    bool _S740;
    float2  _S741;
    bool _S742;
    float2  _S743;
    bool _S744;
};

inline __device__ void depth_to_normal_vjp_opencv(float2  pix_center_6, float4  intrins_12, FixedArray<float, 4>  dist_coeffs_15, int camera_model_14, bool is_ray_depth_13, float4  depths_5, float3  v_normal_2, float4  * v_depths_2)
{
    float2  _S745 = make_float2 (0.0f);
    s_bwd_prop_depth_to_normal_Intermediates_1 _S746;
    (&_S746)->_S737 = _S745;
    (&_S746)->_S738 = false;
    (&_S746)->_S739 = _S745;
    (&_S746)->_S740 = false;
    (&_S746)->_S741 = _S745;
    (&_S746)->_S742 = false;
    (&_S746)->_S743 = _S745;
    (&_S746)->_S744 = false;
    (&_S746)->_S737 = _S745;
    (&_S746)->_S738 = false;
    (&_S746)->_S739 = _S745;
    (&_S746)->_S740 = false;
    (&_S746)->_S741 = _S745;
    (&_S746)->_S742 = false;
    (&_S746)->_S743 = _S745;
    (&_S746)->_S744 = false;
    bool _S747 = (depths_5.x) == 0.0f;
    bool _runFlag_11;
    if(_S747)
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
    int _S748;
    if(!_runFlag_11)
    {
        float2  _S749 = float2 {intrins_12.z, intrins_12.w};
        float2  _S750 = float2 {intrins_12.x, intrins_12.y};
        float2  uv_35 = (pix_center_6 + make_float2 (-1.0f, -0.0f) - _S749) / _S750;
        float2  _S751 = _S745;
        FixedArray<float, 4>  _S752 = dist_coeffs_15;
        bool _S753 = undistort_point_1(uv_35, &_S752, int(12), &_S751);
        (&_S746)->_S737 = _S751;
        (&_S746)->_S738 = _S753;
        bool _S754 = !!_S753;
        if(_S754)
        {
            float2  uv_36 = (pix_center_6 + make_float2 (1.0f, -0.0f) - _S749) / _S750;
            float2  _S755 = _S745;
            FixedArray<float, 4>  _S756 = dist_coeffs_15;
            bool _S757 = undistort_point_1(uv_36, &_S756, int(12), &_S755);
            (&_S746)->_S739 = _S755;
            (&_S746)->_S740 = _S757;
            if(!!_S757)
            {
                _S748 = int(2);
            }
            else
            {
                _S748 = int(0);
            }
            if(_S748 != int(2))
            {
                _runFlag_11 = false;
            }
            else
            {
                _runFlag_11 = _S754;
            }
            if(_runFlag_11)
            {
                float2  uv_37 = (pix_center_6 + make_float2 (0.0f, -1.0f) - _S749) / _S750;
                float2  _S758 = _S745;
                FixedArray<float, 4>  _S759 = dist_coeffs_15;
                bool _S760 = undistort_point_1(uv_37, &_S759, int(12), &_S758);
                (&_S746)->_S741 = _S758;
                (&_S746)->_S742 = _S760;
                if(!_S760)
                {
                    _runFlag_11 = false;
                }
                if(_runFlag_11)
                {
                    float2  uv_38 = (pix_center_6 + make_float2 (0.0f, 1.0f) - _S749) / _S750;
                    float2  _S761 = _S745;
                    FixedArray<float, 4>  _S762 = dist_coeffs_15;
                    bool _S763 = undistort_point_1(uv_38, &_S762, int(12), &_S761);
                    (&_S746)->_S743 = _S761;
                    (&_S746)->_S744 = _S763;
                }
            }
        }
    }
    s_bwd_prop_depth_to_normal_Intermediates_1 _S764 = _S746;
    float3  _S765 = make_float3 (0.0f);
    if(_S747)
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
    bool _S766 = !_runFlag_11;
    bool _runFlag_12;
    bool _runFlag_13;
    bool _S767;
    bool _runFlag_14;
    bool _S768;
    bool _S769;
    FixedArray<float3 , 4>  points_7;
    float3  _S770;
    float3  _S771;
    float3  _S772;
    float3  _S773;
    float3  _S774;
    float3  _S775;
    float3  _S776;
    float3  _S777;
    float3  _S778;
    if(_S766)
    {
        bool _S779 = !!_S764._S738;
        if(_S779)
        {
            float3  _S780 = s_primal_ctx_unproject_raydir_0(_S764._S737, camera_model_14, is_ray_depth_13);
            float3  _S781 = make_float3 (depths_5.x) * _S780;
            bool _S782 = !!_S764._S740;
            if(_S782)
            {
                float3  _S783 = s_primal_ctx_unproject_raydir_0(_S764._S739, camera_model_14, is_ray_depth_13);
                float3  _S784 = make_float3 (depths_5.y) * _S783;
                _S748 = int(2);
                points_7[int(0)] = _S781;
                points_7[int(1)] = _S784;
                points_7[int(2)] = _S765;
                points_7[int(3)] = _S765;
                _S770 = _S783;
            }
            else
            {
                _S748 = int(0);
                points_7[int(0)] = _S781;
                points_7[int(1)] = _S765;
                points_7[int(2)] = _S765;
                points_7[int(3)] = _S765;
                _S770 = _S765;
            }
            if(_S748 != int(2))
            {
                _runFlag_11 = false;
            }
            else
            {
                _runFlag_11 = _S779;
                _S748 = int(0);
            }
            if(_runFlag_11)
            {
                if(!_S764._S742)
                {
                    _runFlag_12 = false;
                    _S748 = int(0);
                }
                else
                {
                    _runFlag_12 = _runFlag_11;
                }
                if(_runFlag_12)
                {
                    float3  _S785 = s_primal_ctx_unproject_raydir_0(_S764._S741, camera_model_14, is_ray_depth_13);
                    points_7[int(2)] = make_float3 (depths_5.z) * _S785;
                    bool _S786 = !!_S764._S744;
                    int _S787;
                    if(_S786)
                    {
                        float3  _S788 = s_primal_ctx_unproject_raydir_0(_S764._S743, camera_model_14, is_ray_depth_13);
                        points_7[int(3)] = make_float3 (depths_5.w) * _S788;
                        _S787 = int(2);
                        _S771 = _S788;
                    }
                    else
                    {
                        _S787 = int(0);
                        _S771 = _S765;
                    }
                    if(_S787 != int(2))
                    {
                        _runFlag_13 = false;
                        _S748 = _S787;
                    }
                    else
                    {
                        _runFlag_13 = _runFlag_12;
                    }
                    if(_runFlag_13)
                    {
                        _S748 = int(1);
                    }
                    _runFlag_13 = _S786;
                    _S772 = _S785;
                }
                else
                {
                    _runFlag_13 = false;
                    _S771 = _S765;
                    _S772 = _S765;
                }
            }
            else
            {
                _runFlag_12 = false;
                _runFlag_13 = false;
                _S771 = _S765;
                _S772 = _S765;
            }
            float3  _S789 = _S770;
            _S770 = _S771;
            _S771 = _S772;
            _S767 = _S782;
            _S772 = _S789;
            _S773 = _S780;
        }
        else
        {
            _S748 = int(0);
            points_7[int(0)] = _S765;
            points_7[int(1)] = _S765;
            points_7[int(2)] = _S765;
            points_7[int(3)] = _S765;
            _runFlag_11 = false;
            _runFlag_12 = false;
            _runFlag_13 = false;
            _S770 = _S765;
            _S771 = _S765;
            _S767 = false;
            _S772 = _S765;
            _S773 = _S765;
        }
        if(_S748 != int(1))
        {
            _runFlag_14 = false;
        }
        else
        {
            _runFlag_14 = _S766;
        }
        if(_runFlag_14)
        {
            float3  dx_3 = points_7[int(1)] - points_7[int(0)];
            float3  _S790 = - (points_7[int(3)] - points_7[int(2)]);
            float3  _S791 = s_primal_ctx_cross_0(dx_3, _S790);
            bool _S792 = (s_primal_ctx_dot_0(_S791, _S791)) != 0.0f;
            if(_S792)
            {
                float _S793 = length_0(_S791);
                float3  _S794 = make_float3 (_S793);
                _S774 = make_float3 (_S793 * _S793);
                _S775 = _S794;
            }
            else
            {
                _S774 = _S765;
                _S775 = _S765;
            }
            float3  _S795 = _S775;
            _S768 = _S792;
            _S775 = _S791;
            _S776 = _S795;
            _S777 = dx_3;
            _S778 = _S790;
        }
        else
        {
            _S768 = false;
            _S774 = _S765;
            _S775 = _S765;
            _S776 = _S765;
            _S777 = _S765;
            _S778 = _S765;
        }
        bool _S796 = _runFlag_11;
        bool _S797 = _runFlag_12;
        bool _S798 = _runFlag_13;
        float3  _S799 = _S770;
        float3  _S800 = _S771;
        bool _S801 = _S767;
        float3  _S802 = _S772;
        float3  _S803 = _S773;
        _runFlag_11 = _runFlag_14;
        _runFlag_12 = _S768;
        _S770 = _S774;
        _S771 = _S775;
        _S772 = _S776;
        _S773 = _S777;
        _S774 = _S778;
        _runFlag_13 = _S779;
        _S767 = _S796;
        _runFlag_14 = _S797;
        _S768 = _S798;
        _S775 = _S799;
        _S776 = _S800;
        _S769 = _S801;
        _S777 = _S802;
        _S778 = _S803;
    }
    else
    {
        _runFlag_11 = false;
        _runFlag_12 = false;
        _S770 = _S765;
        _S771 = _S765;
        _S772 = _S765;
        _S773 = _S765;
        _S774 = _S765;
        _runFlag_13 = false;
        _S767 = false;
        _runFlag_14 = false;
        _S768 = false;
        _S775 = _S765;
        _S776 = _S765;
        _S769 = false;
        _S777 = _S765;
        _S778 = _S765;
    }
    float4  _S804 = make_float4 (0.0f);
    float4  _S805;
    if(_S766)
    {
        if(_runFlag_11)
        {
            if(_runFlag_12)
            {
                float3  _S806 = v_normal_2 / _S770;
                float3  _S807 = _S771 * - _S806;
                float3  _S808 = _S772 * _S806;
                float _S809 = _S807.x + _S807.y + _S807.z;
                DiffPair_vectorx3Cfloatx2C3x3E_0 _S810;
                (&_S810)->primal_0 = _S771;
                (&_S810)->differential_0 = _S765;
                s_bwd_length_impl_0(&_S810, _S809);
                _S770 = _S808 + _S810.differential_0;
            }
            else
            {
                _S770 = v_normal_2;
            }
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S811;
            (&_S811)->primal_0 = _S771;
            (&_S811)->differential_0 = _S765;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S812;
            (&_S812)->primal_0 = _S771;
            (&_S812)->differential_0 = _S765;
            s_bwd_prop_dot_0(&_S811, &_S812, 0.0f);
            float3  _S813 = _S812.differential_0 + _S811.differential_0 + _S770;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S814;
            (&_S814)->primal_0 = _S773;
            (&_S814)->differential_0 = _S765;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S815;
            (&_S815)->primal_0 = _S774;
            (&_S815)->differential_0 = _S765;
            s_bwd_prop_cross_0(&_S814, &_S815, _S813);
            float3  s_diff_dy_T_3 = - _S815.differential_0;
            float3  _S816 = - s_diff_dy_T_3;
            float3  _S817 = - _S814.differential_0;
            FixedArray<float3 , 4>  _S818;
            _S818[int(0)] = _S765;
            _S818[int(1)] = _S765;
            _S818[int(2)] = _S765;
            _S818[int(3)] = _S765;
            _S818[int(2)] = _S816;
            _S818[int(3)] = s_diff_dy_T_3;
            _S818[int(0)] = _S817;
            _S818[int(1)] = _S814.differential_0;
            points_7[int(0)] = _S818[int(0)];
            points_7[int(1)] = _S818[int(1)];
            points_7[int(2)] = _S818[int(2)];
            points_7[int(3)] = _S818[int(3)];
        }
        else
        {
            points_7[int(0)] = _S765;
            points_7[int(1)] = _S765;
            points_7[int(2)] = _S765;
            points_7[int(3)] = _S765;
        }
        if(_runFlag_13)
        {
            if(_S767)
            {
                if(_runFlag_14)
                {
                    FixedArray<float3 , 4>  _S819 = points_7;
                    FixedArray<float3 , 4>  _S820 = points_7;
                    FixedArray<float3 , 4>  _S821 = points_7;
                    FixedArray<float3 , 4>  _S822 = points_7;
                    if(_S768)
                    {
                        float3  _S823 = _S775 * _S822[int(3)];
                        float _S824 = _S823.x + _S823.y + _S823.z;
                        float4  _S825 = _S804;
                        *&((&_S825)->w) = _S824;
                        points_7[int(0)] = _S819[int(0)];
                        points_7[int(1)] = _S820[int(1)];
                        points_7[int(2)] = _S821[int(2)];
                        points_7[int(3)] = _S765;
                        _S805 = _S825;
                    }
                    else
                    {
                        points_7[int(0)] = _S819[int(0)];
                        points_7[int(1)] = _S820[int(1)];
                        points_7[int(2)] = _S821[int(2)];
                        points_7[int(3)] = _S822[int(3)];
                        _S805 = _S804;
                    }
                    float3  _S826 = _S776 * points_7[int(2)];
                    float _S827 = _S826.x + _S826.y + _S826.z;
                    FixedArray<float3 , 4>  _S828 = points_7;
                    FixedArray<float3 , 4>  _S829 = points_7;
                    float4  _S830 = _S804;
                    *&((&_S830)->z) = _S827;
                    float4  _S831 = _S805 + _S830;
                    points_7[int(0)] = points_7[int(0)];
                    points_7[int(1)] = _S828[int(1)];
                    points_7[int(2)] = _S765;
                    points_7[int(3)] = _S829[int(3)];
                    _S805 = _S831;
                }
                else
                {
                    FixedArray<float3 , 4>  _S832 = points_7;
                    FixedArray<float3 , 4>  _S833 = points_7;
                    FixedArray<float3 , 4>  _S834 = points_7;
                    points_7[int(0)] = points_7[int(0)];
                    points_7[int(1)] = _S832[int(1)];
                    points_7[int(2)] = _S833[int(2)];
                    points_7[int(3)] = _S834[int(3)];
                    _S805 = _S804;
                }
            }
            else
            {
                FixedArray<float3 , 4>  _S835 = points_7;
                FixedArray<float3 , 4>  _S836 = points_7;
                FixedArray<float3 , 4>  _S837 = points_7;
                points_7[int(0)] = points_7[int(0)];
                points_7[int(1)] = _S835[int(1)];
                points_7[int(2)] = _S836[int(2)];
                points_7[int(3)] = _S837[int(3)];
                _S805 = _S804;
            }
            if(_S769)
            {
                FixedArray<float3 , 4>  _S838 = points_7;
                float3  _S839 = _S777 * points_7[int(1)];
                float _S840 = _S839.x + _S839.y + _S839.z;
                float4  _S841 = _S804;
                *&((&_S841)->y) = _S840;
                float4  _S842 = _S805 + _S841;
                points_7[int(0)] = _S765;
                points_7[int(1)] = _S765;
                points_7[int(2)] = _S765;
                points_7[int(3)] = _S765;
                _S770 = _S838[int(0)];
                _S805 = _S842;
            }
            else
            {
                FixedArray<float3 , 4>  _S843 = points_7;
                FixedArray<float3 , 4>  _S844 = points_7;
                FixedArray<float3 , 4>  _S845 = points_7;
                points_7[int(0)] = points_7[int(0)];
                points_7[int(1)] = _S843[int(1)];
                points_7[int(2)] = _S844[int(2)];
                points_7[int(3)] = _S845[int(3)];
                _S770 = _S765;
            }
            float3  _S846 = _S778 * (points_7[int(0)] + _S770);
            float _S847 = _S846.x + _S846.y + _S846.z;
            float4  _S848 = _S804;
            *&((&_S848)->x) = _S847;
            _S805 = _S805 + _S848;
        }
        else
        {
            _S805 = _S804;
        }
    }
    else
    {
        _S805 = _S804;
    }
    *v_depths_2 = _S805;
    return;
}

inline __device__ float ray_depth_to_linear_depth_factor_opencv(float2  pix_center_7, float4  intrins_13, FixedArray<float, 4>  dist_coeffs_16, int camera_model_15)
{
    float _S849;
    for(;;)
    {
        float2  uv_39 = (pix_center_7 - float2 {intrins_13.z, intrins_13.w}) / float2 {intrins_13.x, intrins_13.y};
        FixedArray<float, 4>  _S850 = dist_coeffs_16;
        float2  uv_u_18;
        bool _S851 = undistort_point_1(uv_39, &_S850, int(12), &uv_u_18);
        if(!_S851)
        {
            _S849 = 0.0f;
            break;
        }
        float3  raydir_9 = unproject_raydir_0(uv_u_18, camera_model_15, false);
        _S849 = float((F32_sign((raydir_9.z)))) / length_0(raydir_9);
        break;
    }
    return _S849;
}

inline __device__ float depth_normal_loss_opencv(float2  pix_center_8, float4  intrins_14, FixedArray<float, 4>  dist_coeffs_17, int camera_model_16, bool is_ray_depth_14, float4  depths_6, float3  gt_normal_2)
{
    float _S852;
    for(;;)
    {
        float3  _S853;
        float3  * _S854;
        float3  * _S855;
        float3  * _S856;
        float3  * _S857;
        int _S858;
        FixedArray<float3 , 5>  points_8;
        for(;;)
        {
            float2  _S859 = float2 {intrins_14.z, intrins_14.w};
            float2  _S860 = float2 {intrins_14.x, intrins_14.y};
            float2  uv_40 = (pix_center_8 + make_float2 (-1.0f, -0.0f) - _S859) / _S860;
            FixedArray<float, 4>  _S861 = dist_coeffs_17;
            float2  uv_u_19;
            bool _S862 = undistort_point_1(uv_40, &_S861, int(12), &uv_u_19);
            float3  _S863 = make_float3 (0.0f);
            if(!_S862)
            {
                _S858 = int(0);
                _S857 = nullptr;
                _S856 = nullptr;
                _S855 = nullptr;
                _S854 = nullptr;
                _S853 = _S863;
                break;
            }
            float3  raydir_10 = unproject_raydir_0(uv_u_19, camera_model_16, is_ray_depth_14);
            points_8[int(0)] = make_float3 (depths_6.x) * raydir_10;
            float2  uv_41 = (pix_center_8 + make_float2 (1.0f, -0.0f) - _S859) / _S860;
            FixedArray<float, 4>  _S864 = dist_coeffs_17;
            float2  uv_u_20;
            bool _S865 = undistort_point_1(uv_41, &_S864, int(12), &uv_u_20);
            if(!_S865)
            {
                _S858 = int(0);
                _S857 = nullptr;
                _S856 = &points_8[int(0)];
                _S855 = nullptr;
                _S854 = nullptr;
                _S853 = _S863;
                break;
            }
            float3  raydir_11 = unproject_raydir_0(uv_u_20, camera_model_16, is_ray_depth_14);
            points_8[int(1)] = make_float3 (depths_6.y) * raydir_11;
            float2  uv_42 = (pix_center_8 + make_float2 (0.0f, -1.0f) - _S859) / _S860;
            FixedArray<float, 4>  _S866 = dist_coeffs_17;
            float2  uv_u_21;
            bool _S867 = undistort_point_1(uv_42, &_S866, int(12), &uv_u_21);
            if(!_S867)
            {
                _S858 = int(0);
                _S857 = &points_8[int(1)];
                _S856 = &points_8[int(0)];
                _S855 = nullptr;
                _S854 = nullptr;
                _S853 = _S863;
                break;
            }
            float3  raydir_12 = unproject_raydir_0(uv_u_21, camera_model_16, is_ray_depth_14);
            points_8[int(2)] = make_float3 (depths_6.z) * raydir_12;
            float2  uv_43 = (pix_center_8 + make_float2 (0.0f, 1.0f) - _S859) / _S860;
            FixedArray<float, 4>  _S868 = dist_coeffs_17;
            float2  uv_u_22;
            bool _S869 = undistort_point_1(uv_43, &_S868, int(12), &uv_u_22);
            if(!_S869)
            {
                _S858 = int(0);
                _S857 = &points_8[int(1)];
                _S856 = &points_8[int(0)];
                _S855 = nullptr;
                _S854 = &points_8[int(2)];
                _S853 = _S863;
                break;
            }
            float3  raydir_13 = unproject_raydir_0(uv_u_22, camera_model_16, is_ray_depth_14);
            points_8[int(3)] = make_float3 (depths_6.w) * raydir_13;
            float2  uv_44 = (pix_center_8 + make_float2 (0.0f) * make_float2 (0.0f, 3.0f) - _S859) / _S860;
            FixedArray<float, 4>  _S870 = dist_coeffs_17;
            float2  uv_u_23;
            bool _S871 = undistort_point_1(uv_44, &_S870, int(12), &uv_u_23);
            if(!_S871)
            {
                _S858 = int(0);
                _S857 = &points_8[int(1)];
                _S856 = &points_8[int(0)];
                _S855 = &points_8[int(3)];
                _S854 = &points_8[int(2)];
                _S853 = _S863;
                break;
            }
            float3  raydir_14 = unproject_raydir_0(uv_u_23, camera_model_16, is_ray_depth_14);
            _S858 = int(1);
            _S857 = &points_8[int(1)];
            _S856 = &points_8[int(0)];
            _S855 = &points_8[int(3)];
            _S854 = &points_8[int(2)];
            _S853 = raydir_14;
            break;
        }
        if(_S858 != int(1))
        {
            _S852 = 0.0f;
            break;
        }
        float3  normal_9 = cross_0(*_S857 - *_S856, - (*_S855 - *_S854));
        float3  normal_10;
        if((dot_0(normal_9, normal_9)) != 0.0f)
        {
            normal_10 = normalize_0(normal_9);
        }
        else
        {
            normal_10 = normal_9;
        }
        float3  _S872;
        if((dot_0(gt_normal_2, gt_normal_2)) != 0.0f)
        {
            _S872 = normalize_0(gt_normal_2);
        }
        else
        {
            _S872 = gt_normal_2;
        }
        _S852 = (1.0f - dot_0(normal_10, _S872) + 0.00100000004749745f) / ((F32_max((dot_0(normal_10, - normalize_0(_S853))), (0.0f))) + 0.00100000004749745f);
        break;
    }
    return _S852;
}

struct s_bwd_prop_depth_normal_loss_Intermediates_1
{
    float2  _S873;
    bool _S874;
    float2  _S875;
    bool _S876;
    float2  _S877;
    bool _S878;
    float2  _S879;
    bool _S880;
    float2  _S881;
    bool _S882;
};

inline __device__ void depth_normal_loss_vjp_opencv(float2  pix_center_9, float4  intrins_15, FixedArray<float, 4>  dist_coeffs_18, int camera_model_17, bool is_ray_depth_15, float4  depths_7, float3  gt_normal_3, float v_loss_1, float4  * v_depths_3, float3  * v_gt_normal_1)
{
    float2  _S883 = make_float2 (0.0f);
    s_bwd_prop_depth_normal_loss_Intermediates_1 _S884;
    (&_S884)->_S873 = _S883;
    (&_S884)->_S874 = false;
    (&_S884)->_S875 = _S883;
    (&_S884)->_S876 = false;
    (&_S884)->_S877 = _S883;
    (&_S884)->_S878 = false;
    (&_S884)->_S879 = _S883;
    (&_S884)->_S880 = false;
    (&_S884)->_S881 = _S883;
    (&_S884)->_S882 = false;
    (&_S884)->_S875 = _S883;
    (&_S884)->_S876 = false;
    (&_S884)->_S877 = _S883;
    (&_S884)->_S878 = false;
    (&_S884)->_S879 = _S883;
    (&_S884)->_S880 = false;
    (&_S884)->_S881 = _S883;
    (&_S884)->_S882 = false;
    float2  _S885 = float2 {intrins_15.z, intrins_15.w};
    float2  _S886 = float2 {intrins_15.x, intrins_15.y};
    float2  uv_45 = (pix_center_9 + make_float2 (-1.0f, -0.0f) - _S885) / _S886;
    float2  _S887 = _S883;
    FixedArray<float, 4>  _S888 = dist_coeffs_18;
    bool _S889 = undistort_point_1(uv_45, &_S888, int(12), &_S887);
    (&_S884)->_S873 = _S887;
    (&_S884)->_S874 = _S889;
    bool _S890 = !!_S889;
    bool _runFlag_15;
    if(_S890)
    {
        float2  uv_46 = (pix_center_9 + make_float2 (1.0f, -0.0f) - _S885) / _S886;
        float2  _S891 = _S883;
        FixedArray<float, 4>  _S892 = dist_coeffs_18;
        bool _S893 = undistort_point_1(uv_46, &_S892, int(12), &_S891);
        (&_S884)->_S875 = _S891;
        (&_S884)->_S876 = _S893;
        if(!_S893)
        {
            _runFlag_15 = false;
        }
        else
        {
            _runFlag_15 = _S890;
        }
        if(_runFlag_15)
        {
            float2  uv_47 = (pix_center_9 + make_float2 (0.0f, -1.0f) - _S885) / _S886;
            float2  _S894 = _S883;
            FixedArray<float, 4>  _S895 = dist_coeffs_18;
            bool _S896 = undistort_point_1(uv_47, &_S895, int(12), &_S894);
            (&_S884)->_S877 = _S894;
            (&_S884)->_S878 = _S896;
            if(!_S896)
            {
                _runFlag_15 = false;
            }
            if(_runFlag_15)
            {
                float2  uv_48 = (pix_center_9 + make_float2 (0.0f, 1.0f) - _S885) / _S886;
                float2  _S897 = _S883;
                FixedArray<float, 4>  _S898 = dist_coeffs_18;
                bool _S899 = undistort_point_1(uv_48, &_S898, int(12), &_S897);
                (&_S884)->_S879 = _S897;
                (&_S884)->_S880 = _S899;
                if(!_S899)
                {
                    _runFlag_15 = false;
                }
                if(_runFlag_15)
                {
                    float2  uv_49 = (pix_center_9 - _S885) / _S886;
                    float2  _S900 = _S883;
                    FixedArray<float, 4>  _S901 = dist_coeffs_18;
                    bool _S902 = undistort_point_1(uv_49, &_S901, int(12), &_S900);
                    (&_S884)->_S881 = _S900;
                    (&_S884)->_S882 = _S902;
                }
            }
        }
    }
    s_bwd_prop_depth_normal_loss_Intermediates_1 _S903 = _S884;
    float3  _S904 = make_float3 (0.0f);
    bool _S905 = !!_S884._S874;
    bool _runFlag_16;
    bool _runFlag_17;
    bool _runFlag_18;
    int _S906;
    float3  raydir_15;
    float3  _S907;
    float3  _S908;
    float3  _S909;
    float3  _S910;
    FixedArray<float3 , 5>  points_9;
    if(_S905)
    {
        float3  _S911 = s_primal_ctx_unproject_raydir_0(_S903._S873, camera_model_17, is_ray_depth_15);
        float3  _S912 = make_float3 (depths_7.x) * _S911;
        if(!_S903._S876)
        {
            _runFlag_15 = false;
        }
        else
        {
            _runFlag_15 = _S905;
        }
        if(_runFlag_15)
        {
            float3  _S913 = s_primal_ctx_unproject_raydir_0(_S903._S875, camera_model_17, is_ray_depth_15);
            float3  _S914 = make_float3 (depths_7.y) * _S913;
            if(!_S903._S878)
            {
                _runFlag_16 = false;
            }
            else
            {
                _runFlag_16 = _runFlag_15;
            }
            if(_runFlag_16)
            {
                float3  _S915 = s_primal_ctx_unproject_raydir_0(_S903._S877, camera_model_17, is_ray_depth_15);
                float3  _S916 = make_float3 (depths_7.z) * _S915;
                if(!_S903._S880)
                {
                    _runFlag_17 = false;
                }
                else
                {
                    _runFlag_17 = _runFlag_16;
                }
                if(_runFlag_17)
                {
                    float3  _S917 = s_primal_ctx_unproject_raydir_0(_S903._S879, camera_model_17, is_ray_depth_15);
                    float3  _S918 = make_float3 (depths_7.w) * _S917;
                    if(!_S903._S882)
                    {
                        _runFlag_18 = false;
                    }
                    else
                    {
                        _runFlag_18 = _runFlag_17;
                    }
                    if(_runFlag_18)
                    {
                        float3  _S919 = s_primal_ctx_unproject_raydir_0(_S903._S881, camera_model_17, is_ray_depth_15);
                        _S906 = int(1);
                        raydir_15 = _S919;
                    }
                    else
                    {
                        _S906 = int(0);
                        raydir_15 = _S917;
                    }
                    points_9[int(0)] = _S912;
                    points_9[int(1)] = _S914;
                    points_9[int(2)] = _S916;
                    points_9[int(3)] = _S918;
                    points_9[int(4)] = _S904;
                    _S907 = _S917;
                }
                else
                {
                    _S906 = int(0);
                    raydir_15 = _S915;
                    points_9[int(0)] = _S912;
                    points_9[int(1)] = _S914;
                    points_9[int(2)] = _S916;
                    points_9[int(3)] = _S904;
                    points_9[int(4)] = _S904;
                    _S907 = _S904;
                }
                _S908 = _S915;
            }
            else
            {
                _S906 = int(0);
                raydir_15 = _S913;
                points_9[int(0)] = _S912;
                points_9[int(1)] = _S914;
                points_9[int(2)] = _S904;
                points_9[int(3)] = _S904;
                points_9[int(4)] = _S904;
                _runFlag_17 = false;
                _S907 = _S904;
                _S908 = _S904;
            }
            _S909 = _S913;
        }
        else
        {
            _S906 = int(0);
            raydir_15 = _S911;
            points_9[int(0)] = _S912;
            points_9[int(1)] = _S904;
            points_9[int(2)] = _S904;
            points_9[int(3)] = _S904;
            points_9[int(4)] = _S904;
            _runFlag_16 = false;
            _runFlag_17 = false;
            _S907 = _S904;
            _S908 = _S904;
            _S909 = _S904;
        }
        _S910 = _S911;
    }
    else
    {
        _S906 = int(0);
        points_9[int(0)] = _S904;
        points_9[int(1)] = _S904;
        points_9[int(2)] = _S904;
        points_9[int(3)] = _S904;
        points_9[int(4)] = _S904;
        _runFlag_15 = false;
        _runFlag_16 = false;
        _runFlag_17 = false;
        _S907 = _S904;
        _S908 = _S904;
        _S909 = _S904;
        _S910 = _S904;
    }
    bool _S920 = !(_S906 != int(1));
    bool _S921;
    float3  normal_11;
    float3  _S922;
    float3  _S923;
    float3  _S924;
    float3  _S925;
    float _S926;
    float _S927;
    float _S928;
    float _S929;
    if(_S920)
    {
        float3  dx_4 = points_9[int(1)] - points_9[int(0)];
        float3  _S930 = - (points_9[int(3)] - points_9[int(2)]);
        float3  _S931 = s_primal_ctx_cross_0(dx_4, _S930);
        bool _S932 = (s_primal_ctx_dot_0(_S931, _S931)) != 0.0f;
        if(_S932)
        {
            normal_11 = normalize_0(_S931);
        }
        else
        {
            normal_11 = _S931;
        }
        bool _S933 = (s_primal_ctx_dot_0(gt_normal_3, gt_normal_3)) != 0.0f;
        if(_S933)
        {
            _S922 = normalize_0(gt_normal_3);
        }
        else
        {
            _S922 = gt_normal_3;
        }
        float3  _S934 = - normalize_0(raydir_15);
        float _S935 = s_primal_ctx_dot_0(normal_11, _S934);
        float _S936 = 1.0f - s_primal_ctx_dot_0(normal_11, _S922) + 0.00100000004749745f;
        float _S937 = (F32_max((_S935), (0.0f))) + 0.00100000004749745f;
        _S926 = _S937 * _S937;
        _S927 = _S936;
        _S928 = _S937;
        _S929 = _S935;
        raydir_15 = normal_11;
        normal_11 = _S934;
        _runFlag_18 = _S933;
        _S921 = _S932;
        _S923 = _S931;
        _S924 = dx_4;
        _S925 = _S930;
    }
    else
    {
        _S926 = 0.0f;
        _S927 = 0.0f;
        _S928 = 0.0f;
        _S929 = 0.0f;
        raydir_15 = _S904;
        normal_11 = _S904;
        _S922 = _S904;
        _runFlag_18 = false;
        _S921 = false;
        _S923 = _S904;
        _S924 = _S904;
        _S925 = _S904;
    }
    float4  _S938 = make_float4 (0.0f);
    if(_S920)
    {
        float _S939 = v_loss_1 / _S926;
        float _S940 = _S927 * - _S939;
        float s_diff_num_T_1 = _S928 * _S939;
        DiffPair_float_0 _S941;
        (&_S941)->primal_0 = _S929;
        (&_S941)->differential_0 = 0.0f;
        DiffPair_float_0 _S942;
        (&_S942)->primal_0 = 0.0f;
        (&_S942)->differential_0 = 0.0f;
        _d_max_0(&_S941, &_S942, _S940);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S943;
        (&_S943)->primal_0 = raydir_15;
        (&_S943)->differential_0 = _S904;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S944;
        (&_S944)->primal_0 = normal_11;
        (&_S944)->differential_0 = _S904;
        s_bwd_prop_dot_0(&_S943, &_S944, _S941.differential_0);
        float _S945 = - s_diff_num_T_1;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S946;
        (&_S946)->primal_0 = raydir_15;
        (&_S946)->differential_0 = _S904;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S947;
        (&_S947)->primal_0 = _S922;
        (&_S947)->differential_0 = _S904;
        s_bwd_prop_dot_0(&_S946, &_S947, _S945);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S948 = _S947;
        float3  _S949 = _S943.differential_0 + _S946.differential_0;
        if(_runFlag_18)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S950;
            (&_S950)->primal_0 = gt_normal_3;
            (&_S950)->differential_0 = _S904;
            s_bwd_normalize_impl_0(&_S950, _S948.differential_0);
            raydir_15 = _S950.differential_0;
        }
        else
        {
            raydir_15 = _S948.differential_0;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S951;
        (&_S951)->primal_0 = gt_normal_3;
        (&_S951)->differential_0 = _S904;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S952;
        (&_S952)->primal_0 = gt_normal_3;
        (&_S952)->differential_0 = _S904;
        s_bwd_prop_dot_0(&_S951, &_S952, 0.0f);
        float3  _S953 = _S952.differential_0 + _S951.differential_0 + raydir_15;
        if(_S921)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S954;
            (&_S954)->primal_0 = _S923;
            (&_S954)->differential_0 = _S904;
            s_bwd_normalize_impl_0(&_S954, _S949);
            raydir_15 = _S954.differential_0;
        }
        else
        {
            raydir_15 = _S949;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S955;
        (&_S955)->primal_0 = _S923;
        (&_S955)->differential_0 = _S904;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S956;
        (&_S956)->primal_0 = _S923;
        (&_S956)->differential_0 = _S904;
        s_bwd_prop_dot_0(&_S955, &_S956, 0.0f);
        float3  _S957 = _S956.differential_0 + _S955.differential_0 + raydir_15;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S958;
        (&_S958)->primal_0 = _S924;
        (&_S958)->differential_0 = _S904;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S959;
        (&_S959)->primal_0 = _S925;
        (&_S959)->differential_0 = _S904;
        s_bwd_prop_cross_0(&_S958, &_S959, _S957);
        float3  s_diff_dy_T_4 = - _S959.differential_0;
        float3  _S960 = - s_diff_dy_T_4;
        float3  _S961 = - _S958.differential_0;
        FixedArray<float3 , 5>  _S962;
        _S962[int(0)] = _S904;
        _S962[int(1)] = _S904;
        _S962[int(2)] = _S904;
        _S962[int(3)] = _S904;
        _S962[int(4)] = _S904;
        _S962[int(2)] = _S960;
        _S962[int(3)] = s_diff_dy_T_4;
        _S962[int(0)] = _S961;
        _S962[int(1)] = _S958.differential_0;
        points_9[int(0)] = _S962[int(0)];
        points_9[int(1)] = _S962[int(1)];
        points_9[int(2)] = _S962[int(2)];
        points_9[int(3)] = _S962[int(3)];
        points_9[int(4)] = _S962[int(4)];
        raydir_15 = _S953;
    }
    else
    {
        points_9[int(0)] = _S904;
        points_9[int(1)] = _S904;
        points_9[int(2)] = _S904;
        points_9[int(3)] = _S904;
        points_9[int(4)] = _S904;
        raydir_15 = _S904;
    }
    float4  _S963;
    if(_S905)
    {
        if(_runFlag_15)
        {
            if(_runFlag_16)
            {
                if(_runFlag_17)
                {
                    FixedArray<float3 , 5>  _S964 = points_9;
                    FixedArray<float3 , 5>  _S965 = points_9;
                    FixedArray<float3 , 5>  _S966 = points_9;
                    float3  _S967 = _S907 * points_9[int(3)];
                    float _S968 = _S967.x + _S967.y + _S967.z;
                    float4  _S969 = _S938;
                    *&((&_S969)->w) = _S968;
                    points_9[int(0)] = _S904;
                    points_9[int(1)] = _S904;
                    points_9[int(2)] = _S904;
                    points_9[int(3)] = _S904;
                    points_9[int(4)] = _S904;
                    _S907 = _S966[int(2)];
                    normal_11 = _S964[int(0)];
                    _S922 = _S965[int(1)];
                    _S963 = _S969;
                }
                else
                {
                    FixedArray<float3 , 5>  _S970 = points_9;
                    FixedArray<float3 , 5>  _S971 = points_9;
                    FixedArray<float3 , 5>  _S972 = points_9;
                    FixedArray<float3 , 5>  _S973 = points_9;
                    points_9[int(0)] = points_9[int(0)];
                    points_9[int(1)] = _S970[int(1)];
                    points_9[int(2)] = _S971[int(2)];
                    points_9[int(3)] = _S972[int(3)];
                    points_9[int(4)] = _S973[int(4)];
                    _S907 = _S904;
                    normal_11 = _S904;
                    _S922 = _S904;
                    _S963 = _S938;
                }
                float3  _S974 = _S908 * (points_9[int(2)] + _S907);
                float _S975 = _S974.x + _S974.y + _S974.z;
                float3  _S976 = points_9[int(0)] + normal_11;
                float3  _S977 = points_9[int(1)] + _S922;
                float4  _S978 = _S938;
                *&((&_S978)->z) = _S975;
                float4  _S979 = _S963 + _S978;
                points_9[int(0)] = _S904;
                points_9[int(1)] = _S904;
                points_9[int(2)] = _S904;
                points_9[int(3)] = _S904;
                points_9[int(4)] = _S904;
                _S907 = _S977;
                _S908 = _S976;
                _S963 = _S979;
            }
            else
            {
                FixedArray<float3 , 5>  _S980 = points_9;
                FixedArray<float3 , 5>  _S981 = points_9;
                FixedArray<float3 , 5>  _S982 = points_9;
                FixedArray<float3 , 5>  _S983 = points_9;
                points_9[int(0)] = points_9[int(0)];
                points_9[int(1)] = _S980[int(1)];
                points_9[int(2)] = _S981[int(2)];
                points_9[int(3)] = _S982[int(3)];
                points_9[int(4)] = _S983[int(4)];
                _S907 = _S904;
                _S908 = _S904;
                _S963 = _S938;
            }
            float3  _S984 = _S909 * (points_9[int(1)] + _S907);
            float _S985 = _S984.x + _S984.y + _S984.z;
            float3  _S986 = points_9[int(0)] + _S908;
            float4  _S987 = _S938;
            *&((&_S987)->y) = _S985;
            float4  _S988 = _S963 + _S987;
            points_9[int(0)] = _S904;
            points_9[int(1)] = _S904;
            points_9[int(2)] = _S904;
            points_9[int(3)] = _S904;
            points_9[int(4)] = _S904;
            _S907 = _S986;
            _S963 = _S988;
        }
        else
        {
            FixedArray<float3 , 5>  _S989 = points_9;
            FixedArray<float3 , 5>  _S990 = points_9;
            FixedArray<float3 , 5>  _S991 = points_9;
            FixedArray<float3 , 5>  _S992 = points_9;
            points_9[int(0)] = points_9[int(0)];
            points_9[int(1)] = _S989[int(1)];
            points_9[int(2)] = _S990[int(2)];
            points_9[int(3)] = _S991[int(3)];
            points_9[int(4)] = _S992[int(4)];
            _S907 = _S904;
            _S963 = _S938;
        }
        float3  _S993 = _S910 * (points_9[int(0)] + _S907);
        float _S994 = _S993.x + _S993.y + _S993.z;
        float4  _S995 = _S938;
        *&((&_S995)->x) = _S994;
        _S963 = _S963 + _S995;
    }
    else
    {
        _S963 = _S938;
    }
    *v_depths_3 = _S963;
    *v_gt_normal_1 = raydir_15;
    return;
}

inline __device__ float3  generate_ray_d2n_prism(float2  pix_pos_6, float4  intrins_16, FixedArray<float, 8>  dist_coeffs_19, int camera_model_18, bool is_ray_depth_16)
{
    float3  _S996;
    for(;;)
    {
        float2  uv_50 = (pix_pos_6 - float2 {intrins_16.z, intrins_16.w}) / float2 {intrins_16.x, intrins_16.y};
        FixedArray<float, 8>  _S997 = dist_coeffs_19;
        float2  uv_u_24;
        bool _S998 = undistort_point_2(uv_50, &_S997, int(12), &uv_u_24);
        if(!_S998)
        {
            int3  _S999 = make_int3 (int(0));
            float3  _S1000 = make_float3 ((float)_S999.x, (float)_S999.y, (float)_S999.z);
            _S996 = _S1000;
            break;
        }
        _S996 = unproject_raydir_0(uv_u_24, camera_model_18, is_ray_depth_16);
        break;
    }
    return _S996;
}

inline __device__ float3  depth_to_point_prism(float2  pix_pos_7, float4  intrins_17, FixedArray<float, 8>  dist_coeffs_20, int camera_model_19, bool is_ray_depth_17, float depth_6)
{
    float3  _S1001;
    for(;;)
    {
        float2  uv_51 = (pix_pos_7 - float2 {intrins_17.z, intrins_17.w}) / float2 {intrins_17.x, intrins_17.y};
        FixedArray<float, 8>  _S1002 = dist_coeffs_20;
        float2  uv_u_25;
        bool _S1003 = undistort_point_2(uv_51, &_S1002, int(12), &uv_u_25);
        if(!_S1003)
        {
            _S1001 = make_float3 (0.0f);
            break;
        }
        _S1001 = make_float3 (depth_6) * unproject_raydir_0(uv_u_25, camera_model_19, is_ray_depth_17);
        break;
    }
    return _S1001;
}

struct s_bwd_prop_depth_to_point_Intermediates_2
{
    float2  _S1004;
    bool _S1005;
};

inline __device__ float depth_to_point_vjp_prism(float2  pix_pos_8, float4  intrins_18, FixedArray<float, 8>  dist_coeffs_21, int camera_model_20, bool is_ray_depth_18, float depth_7, float3  v_point_2)
{
    float2  _S1006 = make_float2 (0.0f);
    s_bwd_prop_depth_to_point_Intermediates_2 _S1007;
    (&_S1007)->_S1004 = _S1006;
    (&_S1007)->_S1005 = false;
    float2  uv_52 = (pix_pos_8 - float2 {intrins_18.z, intrins_18.w}) / float2 {intrins_18.x, intrins_18.y};
    float2  _S1008 = _S1006;
    FixedArray<float, 8>  _S1009 = dist_coeffs_21;
    bool _S1010 = undistort_point_2(uv_52, &_S1009, int(12), &_S1008);
    (&_S1007)->_S1004 = _S1008;
    (&_S1007)->_S1005 = _S1010;
    s_bwd_prop_depth_to_point_Intermediates_2 _S1011 = _S1007;
    float3  _S1012 = make_float3 (0.0f);
    bool _S1013 = !!_S1007._S1005;
    float3  _S1014;
    if(_S1013)
    {
        _S1014 = s_primal_ctx_unproject_raydir_0(_S1011._S1004, camera_model_20, is_ray_depth_18);
    }
    else
    {
        _S1014 = _S1012;
    }
    if(_S1013)
    {
        _S1014 = _S1014 * v_point_2;
    }
    else
    {
        _S1014 = _S1012;
    }
    return _S1014.x + _S1014.y + _S1014.z;
}

inline __device__ float3  depth_to_normal_prism(float2  pix_center_10, float4  intrins_19, FixedArray<float, 8>  dist_coeffs_22, int camera_model_21, bool is_ray_depth_19, float4  depths_8)
{
    float3  normal_12;
    for(;;)
    {
        bool _S1015;
        if((depths_8.x) == 0.0f)
        {
            _S1015 = true;
        }
        else
        {
            _S1015 = (depths_8.y) == 0.0f;
        }
        if(_S1015)
        {
            _S1015 = true;
        }
        else
        {
            _S1015 = (depths_8.z) == 0.0f;
        }
        if(_S1015)
        {
            _S1015 = true;
        }
        else
        {
            _S1015 = (depths_8.w) == 0.0f;
        }
        if(_S1015)
        {
            normal_12 = make_float3 (0.0f);
            break;
        }
        float3  * _S1016;
        float3  * _S1017;
        float3  * _S1018;
        float3  * _S1019;
        int _S1020;
        FixedArray<float3 , 4>  points_10;
        for(;;)
        {
            float2  _S1021 = float2 {intrins_19.z, intrins_19.w};
            float2  _S1022 = float2 {intrins_19.x, intrins_19.y};
            float2  uv_53 = (pix_center_10 + make_float2 (-1.0f, -0.0f) - _S1021) / _S1022;
            FixedArray<float, 8>  _S1023 = dist_coeffs_22;
            float2  uv_u_26;
            bool _S1024 = undistort_point_2(uv_53, &_S1023, int(12), &uv_u_26);
            if(!_S1024)
            {
                float3  _S1025 = make_float3 (0.0f);
                _S1020 = int(0);
                _S1019 = nullptr;
                _S1018 = nullptr;
                _S1017 = nullptr;
                _S1016 = nullptr;
                normal_12 = _S1025;
                break;
            }
            points_10[int(0)] = make_float3 (depths_8.x) * unproject_raydir_0(uv_u_26, camera_model_21, is_ray_depth_19);
            for(;;)
            {
                float2  uv_54 = (pix_center_10 + make_float2 (1.0f, -0.0f) - _S1021) / _S1022;
                FixedArray<float, 8>  _S1026 = dist_coeffs_22;
                float2  uv_u_27;
                bool _S1027 = undistort_point_2(uv_54, &_S1026, int(12), &uv_u_27);
                if(!_S1027)
                {
                    float3  _S1028 = make_float3 (0.0f);
                    _S1020 = int(0);
                    _S1019 = nullptr;
                    normal_12 = _S1028;
                    break;
                }
                points_10[int(1)] = make_float3 (depths_8.y) * unproject_raydir_0(uv_u_27, camera_model_21, is_ray_depth_19);
                _S1020 = int(2);
                _S1019 = &points_10[int(1)];
                break;
            }
            if(_S1020 != int(2))
            {
                _S1018 = &points_10[int(0)];
                _S1017 = nullptr;
                _S1016 = nullptr;
                break;
            }
            float2  uv_55 = (pix_center_10 + make_float2 (0.0f, -1.0f) - _S1021) / _S1022;
            FixedArray<float, 8>  _S1029 = dist_coeffs_22;
            float2  uv_u_28;
            bool _S1030 = undistort_point_2(uv_55, &_S1029, int(12), &uv_u_28);
            if(!_S1030)
            {
                float3  _S1031 = make_float3 (0.0f);
                _S1020 = int(0);
                _S1018 = &points_10[int(0)];
                _S1017 = nullptr;
                _S1016 = nullptr;
                normal_12 = _S1031;
                break;
            }
            points_10[int(2)] = make_float3 (depths_8.z) * unproject_raydir_0(uv_u_28, camera_model_21, is_ray_depth_19);
            for(;;)
            {
                float2  uv_56 = (pix_center_10 + make_float2 (0.0f, 1.0f) - _S1021) / _S1022;
                FixedArray<float, 8>  _S1032 = dist_coeffs_22;
                float2  uv_u_29;
                bool _S1033 = undistort_point_2(uv_56, &_S1032, int(12), &uv_u_29);
                if(!_S1033)
                {
                    float3  _S1034 = make_float3 (0.0f);
                    _S1020 = int(0);
                    _S1018 = nullptr;
                    normal_12 = _S1034;
                    break;
                }
                points_10[int(3)] = make_float3 (depths_8.w) * unproject_raydir_0(uv_u_29, camera_model_21, is_ray_depth_19);
                _S1020 = int(2);
                _S1018 = &points_10[int(3)];
                break;
            }
            if(_S1020 != int(2))
            {
                float3  * _S1035 = _S1018;
                _S1018 = &points_10[int(0)];
                _S1017 = _S1035;
                _S1016 = &points_10[int(2)];
                break;
            }
            float3  * _S1036 = _S1018;
            _S1020 = int(1);
            _S1018 = &points_10[int(0)];
            _S1017 = _S1036;
            _S1016 = &points_10[int(2)];
            break;
        }
        if(_S1020 != int(1))
        {
            break;
        }
        float3  normal_13 = cross_0(*_S1019 - *_S1018, - (*_S1017 - *_S1016));
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
    float2  _S1037;
    bool _S1038;
    float2  _S1039;
    bool _S1040;
    float2  _S1041;
    bool _S1042;
    float2  _S1043;
    bool _S1044;
};

inline __device__ void depth_to_normal_vjp_prism(float2  pix_center_11, float4  intrins_20, FixedArray<float, 8>  dist_coeffs_23, int camera_model_22, bool is_ray_depth_20, float4  depths_9, float3  v_normal_3, float4  * v_depths_4)
{
    float2  _S1045 = make_float2 (0.0f);
    s_bwd_prop_depth_to_normal_Intermediates_2 _S1046;
    (&_S1046)->_S1037 = _S1045;
    (&_S1046)->_S1038 = false;
    (&_S1046)->_S1039 = _S1045;
    (&_S1046)->_S1040 = false;
    (&_S1046)->_S1041 = _S1045;
    (&_S1046)->_S1042 = false;
    (&_S1046)->_S1043 = _S1045;
    (&_S1046)->_S1044 = false;
    (&_S1046)->_S1037 = _S1045;
    (&_S1046)->_S1038 = false;
    (&_S1046)->_S1039 = _S1045;
    (&_S1046)->_S1040 = false;
    (&_S1046)->_S1041 = _S1045;
    (&_S1046)->_S1042 = false;
    (&_S1046)->_S1043 = _S1045;
    (&_S1046)->_S1044 = false;
    bool _S1047 = (depths_9.x) == 0.0f;
    bool _runFlag_19;
    if(_S1047)
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
    int _S1048;
    if(!_runFlag_19)
    {
        float2  _S1049 = float2 {intrins_20.z, intrins_20.w};
        float2  _S1050 = float2 {intrins_20.x, intrins_20.y};
        float2  uv_57 = (pix_center_11 + make_float2 (-1.0f, -0.0f) - _S1049) / _S1050;
        float2  _S1051 = _S1045;
        FixedArray<float, 8>  _S1052 = dist_coeffs_23;
        bool _S1053 = undistort_point_2(uv_57, &_S1052, int(12), &_S1051);
        (&_S1046)->_S1037 = _S1051;
        (&_S1046)->_S1038 = _S1053;
        bool _S1054 = !!_S1053;
        if(_S1054)
        {
            float2  uv_58 = (pix_center_11 + make_float2 (1.0f, -0.0f) - _S1049) / _S1050;
            float2  _S1055 = _S1045;
            FixedArray<float, 8>  _S1056 = dist_coeffs_23;
            bool _S1057 = undistort_point_2(uv_58, &_S1056, int(12), &_S1055);
            (&_S1046)->_S1039 = _S1055;
            (&_S1046)->_S1040 = _S1057;
            if(!!_S1057)
            {
                _S1048 = int(2);
            }
            else
            {
                _S1048 = int(0);
            }
            if(_S1048 != int(2))
            {
                _runFlag_19 = false;
            }
            else
            {
                _runFlag_19 = _S1054;
            }
            if(_runFlag_19)
            {
                float2  uv_59 = (pix_center_11 + make_float2 (0.0f, -1.0f) - _S1049) / _S1050;
                float2  _S1058 = _S1045;
                FixedArray<float, 8>  _S1059 = dist_coeffs_23;
                bool _S1060 = undistort_point_2(uv_59, &_S1059, int(12), &_S1058);
                (&_S1046)->_S1041 = _S1058;
                (&_S1046)->_S1042 = _S1060;
                if(!_S1060)
                {
                    _runFlag_19 = false;
                }
                if(_runFlag_19)
                {
                    float2  uv_60 = (pix_center_11 + make_float2 (0.0f, 1.0f) - _S1049) / _S1050;
                    float2  _S1061 = _S1045;
                    FixedArray<float, 8>  _S1062 = dist_coeffs_23;
                    bool _S1063 = undistort_point_2(uv_60, &_S1062, int(12), &_S1061);
                    (&_S1046)->_S1043 = _S1061;
                    (&_S1046)->_S1044 = _S1063;
                }
            }
        }
    }
    s_bwd_prop_depth_to_normal_Intermediates_2 _S1064 = _S1046;
    float3  _S1065 = make_float3 (0.0f);
    if(_S1047)
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
    bool _S1066 = !_runFlag_19;
    bool _runFlag_20;
    bool _runFlag_21;
    bool _S1067;
    bool _runFlag_22;
    bool _S1068;
    bool _S1069;
    FixedArray<float3 , 4>  points_11;
    float3  _S1070;
    float3  _S1071;
    float3  _S1072;
    float3  _S1073;
    float3  _S1074;
    float3  _S1075;
    float3  _S1076;
    float3  _S1077;
    float3  _S1078;
    if(_S1066)
    {
        bool _S1079 = !!_S1064._S1038;
        if(_S1079)
        {
            float3  _S1080 = s_primal_ctx_unproject_raydir_0(_S1064._S1037, camera_model_22, is_ray_depth_20);
            float3  _S1081 = make_float3 (depths_9.x) * _S1080;
            bool _S1082 = !!_S1064._S1040;
            if(_S1082)
            {
                float3  _S1083 = s_primal_ctx_unproject_raydir_0(_S1064._S1039, camera_model_22, is_ray_depth_20);
                float3  _S1084 = make_float3 (depths_9.y) * _S1083;
                _S1048 = int(2);
                points_11[int(0)] = _S1081;
                points_11[int(1)] = _S1084;
                points_11[int(2)] = _S1065;
                points_11[int(3)] = _S1065;
                _S1070 = _S1083;
            }
            else
            {
                _S1048 = int(0);
                points_11[int(0)] = _S1081;
                points_11[int(1)] = _S1065;
                points_11[int(2)] = _S1065;
                points_11[int(3)] = _S1065;
                _S1070 = _S1065;
            }
            if(_S1048 != int(2))
            {
                _runFlag_19 = false;
            }
            else
            {
                _runFlag_19 = _S1079;
                _S1048 = int(0);
            }
            if(_runFlag_19)
            {
                if(!_S1064._S1042)
                {
                    _runFlag_20 = false;
                    _S1048 = int(0);
                }
                else
                {
                    _runFlag_20 = _runFlag_19;
                }
                if(_runFlag_20)
                {
                    float3  _S1085 = s_primal_ctx_unproject_raydir_0(_S1064._S1041, camera_model_22, is_ray_depth_20);
                    points_11[int(2)] = make_float3 (depths_9.z) * _S1085;
                    bool _S1086 = !!_S1064._S1044;
                    int _S1087;
                    if(_S1086)
                    {
                        float3  _S1088 = s_primal_ctx_unproject_raydir_0(_S1064._S1043, camera_model_22, is_ray_depth_20);
                        points_11[int(3)] = make_float3 (depths_9.w) * _S1088;
                        _S1087 = int(2);
                        _S1071 = _S1088;
                    }
                    else
                    {
                        _S1087 = int(0);
                        _S1071 = _S1065;
                    }
                    if(_S1087 != int(2))
                    {
                        _runFlag_21 = false;
                        _S1048 = _S1087;
                    }
                    else
                    {
                        _runFlag_21 = _runFlag_20;
                    }
                    if(_runFlag_21)
                    {
                        _S1048 = int(1);
                    }
                    _runFlag_21 = _S1086;
                    _S1072 = _S1085;
                }
                else
                {
                    _runFlag_21 = false;
                    _S1071 = _S1065;
                    _S1072 = _S1065;
                }
            }
            else
            {
                _runFlag_20 = false;
                _runFlag_21 = false;
                _S1071 = _S1065;
                _S1072 = _S1065;
            }
            float3  _S1089 = _S1070;
            _S1070 = _S1071;
            _S1071 = _S1072;
            _S1067 = _S1082;
            _S1072 = _S1089;
            _S1073 = _S1080;
        }
        else
        {
            _S1048 = int(0);
            points_11[int(0)] = _S1065;
            points_11[int(1)] = _S1065;
            points_11[int(2)] = _S1065;
            points_11[int(3)] = _S1065;
            _runFlag_19 = false;
            _runFlag_20 = false;
            _runFlag_21 = false;
            _S1070 = _S1065;
            _S1071 = _S1065;
            _S1067 = false;
            _S1072 = _S1065;
            _S1073 = _S1065;
        }
        if(_S1048 != int(1))
        {
            _runFlag_22 = false;
        }
        else
        {
            _runFlag_22 = _S1066;
        }
        if(_runFlag_22)
        {
            float3  dx_5 = points_11[int(1)] - points_11[int(0)];
            float3  _S1090 = - (points_11[int(3)] - points_11[int(2)]);
            float3  _S1091 = s_primal_ctx_cross_0(dx_5, _S1090);
            bool _S1092 = (s_primal_ctx_dot_0(_S1091, _S1091)) != 0.0f;
            if(_S1092)
            {
                float _S1093 = length_0(_S1091);
                float3  _S1094 = make_float3 (_S1093);
                _S1074 = make_float3 (_S1093 * _S1093);
                _S1075 = _S1094;
            }
            else
            {
                _S1074 = _S1065;
                _S1075 = _S1065;
            }
            float3  _S1095 = _S1075;
            _S1068 = _S1092;
            _S1075 = _S1091;
            _S1076 = _S1095;
            _S1077 = dx_5;
            _S1078 = _S1090;
        }
        else
        {
            _S1068 = false;
            _S1074 = _S1065;
            _S1075 = _S1065;
            _S1076 = _S1065;
            _S1077 = _S1065;
            _S1078 = _S1065;
        }
        bool _S1096 = _runFlag_19;
        bool _S1097 = _runFlag_20;
        bool _S1098 = _runFlag_21;
        float3  _S1099 = _S1070;
        float3  _S1100 = _S1071;
        bool _S1101 = _S1067;
        float3  _S1102 = _S1072;
        float3  _S1103 = _S1073;
        _runFlag_19 = _runFlag_22;
        _runFlag_20 = _S1068;
        _S1070 = _S1074;
        _S1071 = _S1075;
        _S1072 = _S1076;
        _S1073 = _S1077;
        _S1074 = _S1078;
        _runFlag_21 = _S1079;
        _S1067 = _S1096;
        _runFlag_22 = _S1097;
        _S1068 = _S1098;
        _S1075 = _S1099;
        _S1076 = _S1100;
        _S1069 = _S1101;
        _S1077 = _S1102;
        _S1078 = _S1103;
    }
    else
    {
        _runFlag_19 = false;
        _runFlag_20 = false;
        _S1070 = _S1065;
        _S1071 = _S1065;
        _S1072 = _S1065;
        _S1073 = _S1065;
        _S1074 = _S1065;
        _runFlag_21 = false;
        _S1067 = false;
        _runFlag_22 = false;
        _S1068 = false;
        _S1075 = _S1065;
        _S1076 = _S1065;
        _S1069 = false;
        _S1077 = _S1065;
        _S1078 = _S1065;
    }
    float4  _S1104 = make_float4 (0.0f);
    float4  _S1105;
    if(_S1066)
    {
        if(_runFlag_19)
        {
            if(_runFlag_20)
            {
                float3  _S1106 = v_normal_3 / _S1070;
                float3  _S1107 = _S1071 * - _S1106;
                float3  _S1108 = _S1072 * _S1106;
                float _S1109 = _S1107.x + _S1107.y + _S1107.z;
                DiffPair_vectorx3Cfloatx2C3x3E_0 _S1110;
                (&_S1110)->primal_0 = _S1071;
                (&_S1110)->differential_0 = _S1065;
                s_bwd_length_impl_0(&_S1110, _S1109);
                _S1070 = _S1108 + _S1110.differential_0;
            }
            else
            {
                _S1070 = v_normal_3;
            }
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1111;
            (&_S1111)->primal_0 = _S1071;
            (&_S1111)->differential_0 = _S1065;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1112;
            (&_S1112)->primal_0 = _S1071;
            (&_S1112)->differential_0 = _S1065;
            s_bwd_prop_dot_0(&_S1111, &_S1112, 0.0f);
            float3  _S1113 = _S1112.differential_0 + _S1111.differential_0 + _S1070;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1114;
            (&_S1114)->primal_0 = _S1073;
            (&_S1114)->differential_0 = _S1065;
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1115;
            (&_S1115)->primal_0 = _S1074;
            (&_S1115)->differential_0 = _S1065;
            s_bwd_prop_cross_0(&_S1114, &_S1115, _S1113);
            float3  s_diff_dy_T_5 = - _S1115.differential_0;
            float3  _S1116 = - s_diff_dy_T_5;
            float3  _S1117 = - _S1114.differential_0;
            FixedArray<float3 , 4>  _S1118;
            _S1118[int(0)] = _S1065;
            _S1118[int(1)] = _S1065;
            _S1118[int(2)] = _S1065;
            _S1118[int(3)] = _S1065;
            _S1118[int(2)] = _S1116;
            _S1118[int(3)] = s_diff_dy_T_5;
            _S1118[int(0)] = _S1117;
            _S1118[int(1)] = _S1114.differential_0;
            points_11[int(0)] = _S1118[int(0)];
            points_11[int(1)] = _S1118[int(1)];
            points_11[int(2)] = _S1118[int(2)];
            points_11[int(3)] = _S1118[int(3)];
        }
        else
        {
            points_11[int(0)] = _S1065;
            points_11[int(1)] = _S1065;
            points_11[int(2)] = _S1065;
            points_11[int(3)] = _S1065;
        }
        if(_runFlag_21)
        {
            if(_S1067)
            {
                if(_runFlag_22)
                {
                    FixedArray<float3 , 4>  _S1119 = points_11;
                    FixedArray<float3 , 4>  _S1120 = points_11;
                    FixedArray<float3 , 4>  _S1121 = points_11;
                    FixedArray<float3 , 4>  _S1122 = points_11;
                    if(_S1068)
                    {
                        float3  _S1123 = _S1075 * _S1122[int(3)];
                        float _S1124 = _S1123.x + _S1123.y + _S1123.z;
                        float4  _S1125 = _S1104;
                        *&((&_S1125)->w) = _S1124;
                        points_11[int(0)] = _S1119[int(0)];
                        points_11[int(1)] = _S1120[int(1)];
                        points_11[int(2)] = _S1121[int(2)];
                        points_11[int(3)] = _S1065;
                        _S1105 = _S1125;
                    }
                    else
                    {
                        points_11[int(0)] = _S1119[int(0)];
                        points_11[int(1)] = _S1120[int(1)];
                        points_11[int(2)] = _S1121[int(2)];
                        points_11[int(3)] = _S1122[int(3)];
                        _S1105 = _S1104;
                    }
                    float3  _S1126 = _S1076 * points_11[int(2)];
                    float _S1127 = _S1126.x + _S1126.y + _S1126.z;
                    FixedArray<float3 , 4>  _S1128 = points_11;
                    FixedArray<float3 , 4>  _S1129 = points_11;
                    float4  _S1130 = _S1104;
                    *&((&_S1130)->z) = _S1127;
                    float4  _S1131 = _S1105 + _S1130;
                    points_11[int(0)] = points_11[int(0)];
                    points_11[int(1)] = _S1128[int(1)];
                    points_11[int(2)] = _S1065;
                    points_11[int(3)] = _S1129[int(3)];
                    _S1105 = _S1131;
                }
                else
                {
                    FixedArray<float3 , 4>  _S1132 = points_11;
                    FixedArray<float3 , 4>  _S1133 = points_11;
                    FixedArray<float3 , 4>  _S1134 = points_11;
                    points_11[int(0)] = points_11[int(0)];
                    points_11[int(1)] = _S1132[int(1)];
                    points_11[int(2)] = _S1133[int(2)];
                    points_11[int(3)] = _S1134[int(3)];
                    _S1105 = _S1104;
                }
            }
            else
            {
                FixedArray<float3 , 4>  _S1135 = points_11;
                FixedArray<float3 , 4>  _S1136 = points_11;
                FixedArray<float3 , 4>  _S1137 = points_11;
                points_11[int(0)] = points_11[int(0)];
                points_11[int(1)] = _S1135[int(1)];
                points_11[int(2)] = _S1136[int(2)];
                points_11[int(3)] = _S1137[int(3)];
                _S1105 = _S1104;
            }
            if(_S1069)
            {
                FixedArray<float3 , 4>  _S1138 = points_11;
                float3  _S1139 = _S1077 * points_11[int(1)];
                float _S1140 = _S1139.x + _S1139.y + _S1139.z;
                float4  _S1141 = _S1104;
                *&((&_S1141)->y) = _S1140;
                float4  _S1142 = _S1105 + _S1141;
                points_11[int(0)] = _S1065;
                points_11[int(1)] = _S1065;
                points_11[int(2)] = _S1065;
                points_11[int(3)] = _S1065;
                _S1070 = _S1138[int(0)];
                _S1105 = _S1142;
            }
            else
            {
                FixedArray<float3 , 4>  _S1143 = points_11;
                FixedArray<float3 , 4>  _S1144 = points_11;
                FixedArray<float3 , 4>  _S1145 = points_11;
                points_11[int(0)] = points_11[int(0)];
                points_11[int(1)] = _S1143[int(1)];
                points_11[int(2)] = _S1144[int(2)];
                points_11[int(3)] = _S1145[int(3)];
                _S1070 = _S1065;
            }
            float3  _S1146 = _S1078 * (points_11[int(0)] + _S1070);
            float _S1147 = _S1146.x + _S1146.y + _S1146.z;
            float4  _S1148 = _S1104;
            *&((&_S1148)->x) = _S1147;
            _S1105 = _S1105 + _S1148;
        }
        else
        {
            _S1105 = _S1104;
        }
    }
    else
    {
        _S1105 = _S1104;
    }
    *v_depths_4 = _S1105;
    return;
}

inline __device__ float ray_depth_to_linear_depth_factor_prism(float2  pix_center_12, float4  intrins_21, FixedArray<float, 8>  dist_coeffs_24, int camera_model_23)
{
    float _S1149;
    for(;;)
    {
        float2  uv_61 = (pix_center_12 - float2 {intrins_21.z, intrins_21.w}) / float2 {intrins_21.x, intrins_21.y};
        FixedArray<float, 8>  _S1150 = dist_coeffs_24;
        float2  uv_u_30;
        bool _S1151 = undistort_point_2(uv_61, &_S1150, int(12), &uv_u_30);
        if(!_S1151)
        {
            _S1149 = 0.0f;
            break;
        }
        float3  raydir_16 = unproject_raydir_0(uv_u_30, camera_model_23, false);
        _S1149 = float((F32_sign((raydir_16.z)))) / length_0(raydir_16);
        break;
    }
    return _S1149;
}

inline __device__ float depth_normal_loss_prism(float2  pix_center_13, float4  intrins_22, FixedArray<float, 8>  dist_coeffs_25, int camera_model_24, bool is_ray_depth_21, float4  depths_10, float3  gt_normal_4)
{
    float _S1152;
    for(;;)
    {
        float3  _S1153;
        float3  * _S1154;
        float3  * _S1155;
        float3  * _S1156;
        float3  * _S1157;
        int _S1158;
        FixedArray<float3 , 5>  points_12;
        for(;;)
        {
            float2  _S1159 = float2 {intrins_22.z, intrins_22.w};
            float2  _S1160 = float2 {intrins_22.x, intrins_22.y};
            float2  uv_62 = (pix_center_13 + make_float2 (-1.0f, -0.0f) - _S1159) / _S1160;
            FixedArray<float, 8>  _S1161 = dist_coeffs_25;
            float2  uv_u_31;
            bool _S1162 = undistort_point_2(uv_62, &_S1161, int(12), &uv_u_31);
            float3  _S1163 = make_float3 (0.0f);
            if(!_S1162)
            {
                _S1158 = int(0);
                _S1157 = nullptr;
                _S1156 = nullptr;
                _S1155 = nullptr;
                _S1154 = nullptr;
                _S1153 = _S1163;
                break;
            }
            float3  raydir_17 = unproject_raydir_0(uv_u_31, camera_model_24, is_ray_depth_21);
            points_12[int(0)] = make_float3 (depths_10.x) * raydir_17;
            float2  uv_63 = (pix_center_13 + make_float2 (1.0f, -0.0f) - _S1159) / _S1160;
            FixedArray<float, 8>  _S1164 = dist_coeffs_25;
            float2  uv_u_32;
            bool _S1165 = undistort_point_2(uv_63, &_S1164, int(12), &uv_u_32);
            if(!_S1165)
            {
                _S1158 = int(0);
                _S1157 = nullptr;
                _S1156 = &points_12[int(0)];
                _S1155 = nullptr;
                _S1154 = nullptr;
                _S1153 = _S1163;
                break;
            }
            float3  raydir_18 = unproject_raydir_0(uv_u_32, camera_model_24, is_ray_depth_21);
            points_12[int(1)] = make_float3 (depths_10.y) * raydir_18;
            float2  uv_64 = (pix_center_13 + make_float2 (0.0f, -1.0f) - _S1159) / _S1160;
            FixedArray<float, 8>  _S1166 = dist_coeffs_25;
            float2  uv_u_33;
            bool _S1167 = undistort_point_2(uv_64, &_S1166, int(12), &uv_u_33);
            if(!_S1167)
            {
                _S1158 = int(0);
                _S1157 = &points_12[int(1)];
                _S1156 = &points_12[int(0)];
                _S1155 = nullptr;
                _S1154 = nullptr;
                _S1153 = _S1163;
                break;
            }
            float3  raydir_19 = unproject_raydir_0(uv_u_33, camera_model_24, is_ray_depth_21);
            points_12[int(2)] = make_float3 (depths_10.z) * raydir_19;
            float2  uv_65 = (pix_center_13 + make_float2 (0.0f, 1.0f) - _S1159) / _S1160;
            FixedArray<float, 8>  _S1168 = dist_coeffs_25;
            float2  uv_u_34;
            bool _S1169 = undistort_point_2(uv_65, &_S1168, int(12), &uv_u_34);
            if(!_S1169)
            {
                _S1158 = int(0);
                _S1157 = &points_12[int(1)];
                _S1156 = &points_12[int(0)];
                _S1155 = nullptr;
                _S1154 = &points_12[int(2)];
                _S1153 = _S1163;
                break;
            }
            float3  raydir_20 = unproject_raydir_0(uv_u_34, camera_model_24, is_ray_depth_21);
            points_12[int(3)] = make_float3 (depths_10.w) * raydir_20;
            float2  uv_66 = (pix_center_13 + make_float2 (0.0f) * make_float2 (0.0f, 3.0f) - _S1159) / _S1160;
            FixedArray<float, 8>  _S1170 = dist_coeffs_25;
            float2  uv_u_35;
            bool _S1171 = undistort_point_2(uv_66, &_S1170, int(12), &uv_u_35);
            if(!_S1171)
            {
                _S1158 = int(0);
                _S1157 = &points_12[int(1)];
                _S1156 = &points_12[int(0)];
                _S1155 = &points_12[int(3)];
                _S1154 = &points_12[int(2)];
                _S1153 = _S1163;
                break;
            }
            float3  raydir_21 = unproject_raydir_0(uv_u_35, camera_model_24, is_ray_depth_21);
            _S1158 = int(1);
            _S1157 = &points_12[int(1)];
            _S1156 = &points_12[int(0)];
            _S1155 = &points_12[int(3)];
            _S1154 = &points_12[int(2)];
            _S1153 = raydir_21;
            break;
        }
        if(_S1158 != int(1))
        {
            _S1152 = 0.0f;
            break;
        }
        float3  normal_14 = cross_0(*_S1157 - *_S1156, - (*_S1155 - *_S1154));
        float3  normal_15;
        if((dot_0(normal_14, normal_14)) != 0.0f)
        {
            normal_15 = normalize_0(normal_14);
        }
        else
        {
            normal_15 = normal_14;
        }
        float3  _S1172;
        if((dot_0(gt_normal_4, gt_normal_4)) != 0.0f)
        {
            _S1172 = normalize_0(gt_normal_4);
        }
        else
        {
            _S1172 = gt_normal_4;
        }
        _S1152 = (1.0f - dot_0(normal_15, _S1172) + 0.00100000004749745f) / ((F32_max((dot_0(normal_15, - normalize_0(_S1153))), (0.0f))) + 0.00100000004749745f);
        break;
    }
    return _S1152;
}

struct s_bwd_prop_depth_normal_loss_Intermediates_2
{
    float2  _S1173;
    bool _S1174;
    float2  _S1175;
    bool _S1176;
    float2  _S1177;
    bool _S1178;
    float2  _S1179;
    bool _S1180;
    float2  _S1181;
    bool _S1182;
};

inline __device__ void depth_normal_loss_vjp_prism(float2  pix_center_14, float4  intrins_23, FixedArray<float, 8>  dist_coeffs_26, int camera_model_25, bool is_ray_depth_22, float4  depths_11, float3  gt_normal_5, float v_loss_2, float4  * v_depths_5, float3  * v_gt_normal_2)
{
    float2  _S1183 = make_float2 (0.0f);
    s_bwd_prop_depth_normal_loss_Intermediates_2 _S1184;
    (&_S1184)->_S1173 = _S1183;
    (&_S1184)->_S1174 = false;
    (&_S1184)->_S1175 = _S1183;
    (&_S1184)->_S1176 = false;
    (&_S1184)->_S1177 = _S1183;
    (&_S1184)->_S1178 = false;
    (&_S1184)->_S1179 = _S1183;
    (&_S1184)->_S1180 = false;
    (&_S1184)->_S1181 = _S1183;
    (&_S1184)->_S1182 = false;
    (&_S1184)->_S1175 = _S1183;
    (&_S1184)->_S1176 = false;
    (&_S1184)->_S1177 = _S1183;
    (&_S1184)->_S1178 = false;
    (&_S1184)->_S1179 = _S1183;
    (&_S1184)->_S1180 = false;
    (&_S1184)->_S1181 = _S1183;
    (&_S1184)->_S1182 = false;
    float2  _S1185 = float2 {intrins_23.z, intrins_23.w};
    float2  _S1186 = float2 {intrins_23.x, intrins_23.y};
    float2  uv_67 = (pix_center_14 + make_float2 (-1.0f, -0.0f) - _S1185) / _S1186;
    float2  _S1187 = _S1183;
    FixedArray<float, 8>  _S1188 = dist_coeffs_26;
    bool _S1189 = undistort_point_2(uv_67, &_S1188, int(12), &_S1187);
    (&_S1184)->_S1173 = _S1187;
    (&_S1184)->_S1174 = _S1189;
    bool _S1190 = !!_S1189;
    bool _runFlag_23;
    if(_S1190)
    {
        float2  uv_68 = (pix_center_14 + make_float2 (1.0f, -0.0f) - _S1185) / _S1186;
        float2  _S1191 = _S1183;
        FixedArray<float, 8>  _S1192 = dist_coeffs_26;
        bool _S1193 = undistort_point_2(uv_68, &_S1192, int(12), &_S1191);
        (&_S1184)->_S1175 = _S1191;
        (&_S1184)->_S1176 = _S1193;
        if(!_S1193)
        {
            _runFlag_23 = false;
        }
        else
        {
            _runFlag_23 = _S1190;
        }
        if(_runFlag_23)
        {
            float2  uv_69 = (pix_center_14 + make_float2 (0.0f, -1.0f) - _S1185) / _S1186;
            float2  _S1194 = _S1183;
            FixedArray<float, 8>  _S1195 = dist_coeffs_26;
            bool _S1196 = undistort_point_2(uv_69, &_S1195, int(12), &_S1194);
            (&_S1184)->_S1177 = _S1194;
            (&_S1184)->_S1178 = _S1196;
            if(!_S1196)
            {
                _runFlag_23 = false;
            }
            if(_runFlag_23)
            {
                float2  uv_70 = (pix_center_14 + make_float2 (0.0f, 1.0f) - _S1185) / _S1186;
                float2  _S1197 = _S1183;
                FixedArray<float, 8>  _S1198 = dist_coeffs_26;
                bool _S1199 = undistort_point_2(uv_70, &_S1198, int(12), &_S1197);
                (&_S1184)->_S1179 = _S1197;
                (&_S1184)->_S1180 = _S1199;
                if(!_S1199)
                {
                    _runFlag_23 = false;
                }
                if(_runFlag_23)
                {
                    float2  uv_71 = (pix_center_14 - _S1185) / _S1186;
                    float2  _S1200 = _S1183;
                    FixedArray<float, 8>  _S1201 = dist_coeffs_26;
                    bool _S1202 = undistort_point_2(uv_71, &_S1201, int(12), &_S1200);
                    (&_S1184)->_S1181 = _S1200;
                    (&_S1184)->_S1182 = _S1202;
                }
            }
        }
    }
    s_bwd_prop_depth_normal_loss_Intermediates_2 _S1203 = _S1184;
    float3  _S1204 = make_float3 (0.0f);
    bool _S1205 = !!_S1184._S1174;
    bool _runFlag_24;
    bool _runFlag_25;
    bool _runFlag_26;
    int _S1206;
    float3  raydir_22;
    float3  _S1207;
    float3  _S1208;
    float3  _S1209;
    float3  _S1210;
    FixedArray<float3 , 5>  points_13;
    if(_S1205)
    {
        float3  _S1211 = s_primal_ctx_unproject_raydir_0(_S1203._S1173, camera_model_25, is_ray_depth_22);
        float3  _S1212 = make_float3 (depths_11.x) * _S1211;
        if(!_S1203._S1176)
        {
            _runFlag_23 = false;
        }
        else
        {
            _runFlag_23 = _S1205;
        }
        if(_runFlag_23)
        {
            float3  _S1213 = s_primal_ctx_unproject_raydir_0(_S1203._S1175, camera_model_25, is_ray_depth_22);
            float3  _S1214 = make_float3 (depths_11.y) * _S1213;
            if(!_S1203._S1178)
            {
                _runFlag_24 = false;
            }
            else
            {
                _runFlag_24 = _runFlag_23;
            }
            if(_runFlag_24)
            {
                float3  _S1215 = s_primal_ctx_unproject_raydir_0(_S1203._S1177, camera_model_25, is_ray_depth_22);
                float3  _S1216 = make_float3 (depths_11.z) * _S1215;
                if(!_S1203._S1180)
                {
                    _runFlag_25 = false;
                }
                else
                {
                    _runFlag_25 = _runFlag_24;
                }
                if(_runFlag_25)
                {
                    float3  _S1217 = s_primal_ctx_unproject_raydir_0(_S1203._S1179, camera_model_25, is_ray_depth_22);
                    float3  _S1218 = make_float3 (depths_11.w) * _S1217;
                    if(!_S1203._S1182)
                    {
                        _runFlag_26 = false;
                    }
                    else
                    {
                        _runFlag_26 = _runFlag_25;
                    }
                    if(_runFlag_26)
                    {
                        float3  _S1219 = s_primal_ctx_unproject_raydir_0(_S1203._S1181, camera_model_25, is_ray_depth_22);
                        _S1206 = int(1);
                        raydir_22 = _S1219;
                    }
                    else
                    {
                        _S1206 = int(0);
                        raydir_22 = _S1217;
                    }
                    points_13[int(0)] = _S1212;
                    points_13[int(1)] = _S1214;
                    points_13[int(2)] = _S1216;
                    points_13[int(3)] = _S1218;
                    points_13[int(4)] = _S1204;
                    _S1207 = _S1217;
                }
                else
                {
                    _S1206 = int(0);
                    raydir_22 = _S1215;
                    points_13[int(0)] = _S1212;
                    points_13[int(1)] = _S1214;
                    points_13[int(2)] = _S1216;
                    points_13[int(3)] = _S1204;
                    points_13[int(4)] = _S1204;
                    _S1207 = _S1204;
                }
                _S1208 = _S1215;
            }
            else
            {
                _S1206 = int(0);
                raydir_22 = _S1213;
                points_13[int(0)] = _S1212;
                points_13[int(1)] = _S1214;
                points_13[int(2)] = _S1204;
                points_13[int(3)] = _S1204;
                points_13[int(4)] = _S1204;
                _runFlag_25 = false;
                _S1207 = _S1204;
                _S1208 = _S1204;
            }
            _S1209 = _S1213;
        }
        else
        {
            _S1206 = int(0);
            raydir_22 = _S1211;
            points_13[int(0)] = _S1212;
            points_13[int(1)] = _S1204;
            points_13[int(2)] = _S1204;
            points_13[int(3)] = _S1204;
            points_13[int(4)] = _S1204;
            _runFlag_24 = false;
            _runFlag_25 = false;
            _S1207 = _S1204;
            _S1208 = _S1204;
            _S1209 = _S1204;
        }
        _S1210 = _S1211;
    }
    else
    {
        _S1206 = int(0);
        points_13[int(0)] = _S1204;
        points_13[int(1)] = _S1204;
        points_13[int(2)] = _S1204;
        points_13[int(3)] = _S1204;
        points_13[int(4)] = _S1204;
        _runFlag_23 = false;
        _runFlag_24 = false;
        _runFlag_25 = false;
        _S1207 = _S1204;
        _S1208 = _S1204;
        _S1209 = _S1204;
        _S1210 = _S1204;
    }
    bool _S1220 = !(_S1206 != int(1));
    bool _S1221;
    float3  normal_16;
    float3  _S1222;
    float3  _S1223;
    float3  _S1224;
    float3  _S1225;
    float _S1226;
    float _S1227;
    float _S1228;
    float _S1229;
    if(_S1220)
    {
        float3  dx_6 = points_13[int(1)] - points_13[int(0)];
        float3  _S1230 = - (points_13[int(3)] - points_13[int(2)]);
        float3  _S1231 = s_primal_ctx_cross_0(dx_6, _S1230);
        bool _S1232 = (s_primal_ctx_dot_0(_S1231, _S1231)) != 0.0f;
        if(_S1232)
        {
            normal_16 = normalize_0(_S1231);
        }
        else
        {
            normal_16 = _S1231;
        }
        bool _S1233 = (s_primal_ctx_dot_0(gt_normal_5, gt_normal_5)) != 0.0f;
        if(_S1233)
        {
            _S1222 = normalize_0(gt_normal_5);
        }
        else
        {
            _S1222 = gt_normal_5;
        }
        float3  _S1234 = - normalize_0(raydir_22);
        float _S1235 = s_primal_ctx_dot_0(normal_16, _S1234);
        float _S1236 = 1.0f - s_primal_ctx_dot_0(normal_16, _S1222) + 0.00100000004749745f;
        float _S1237 = (F32_max((_S1235), (0.0f))) + 0.00100000004749745f;
        _S1226 = _S1237 * _S1237;
        _S1227 = _S1236;
        _S1228 = _S1237;
        _S1229 = _S1235;
        raydir_22 = normal_16;
        normal_16 = _S1234;
        _runFlag_26 = _S1233;
        _S1221 = _S1232;
        _S1223 = _S1231;
        _S1224 = dx_6;
        _S1225 = _S1230;
    }
    else
    {
        _S1226 = 0.0f;
        _S1227 = 0.0f;
        _S1228 = 0.0f;
        _S1229 = 0.0f;
        raydir_22 = _S1204;
        normal_16 = _S1204;
        _S1222 = _S1204;
        _runFlag_26 = false;
        _S1221 = false;
        _S1223 = _S1204;
        _S1224 = _S1204;
        _S1225 = _S1204;
    }
    float4  _S1238 = make_float4 (0.0f);
    if(_S1220)
    {
        float _S1239 = v_loss_2 / _S1226;
        float _S1240 = _S1227 * - _S1239;
        float s_diff_num_T_2 = _S1228 * _S1239;
        DiffPair_float_0 _S1241;
        (&_S1241)->primal_0 = _S1229;
        (&_S1241)->differential_0 = 0.0f;
        DiffPair_float_0 _S1242;
        (&_S1242)->primal_0 = 0.0f;
        (&_S1242)->differential_0 = 0.0f;
        _d_max_0(&_S1241, &_S1242, _S1240);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1243;
        (&_S1243)->primal_0 = raydir_22;
        (&_S1243)->differential_0 = _S1204;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1244;
        (&_S1244)->primal_0 = normal_16;
        (&_S1244)->differential_0 = _S1204;
        s_bwd_prop_dot_0(&_S1243, &_S1244, _S1241.differential_0);
        float _S1245 = - s_diff_num_T_2;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1246;
        (&_S1246)->primal_0 = raydir_22;
        (&_S1246)->differential_0 = _S1204;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1247;
        (&_S1247)->primal_0 = _S1222;
        (&_S1247)->differential_0 = _S1204;
        s_bwd_prop_dot_0(&_S1246, &_S1247, _S1245);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1248 = _S1247;
        float3  _S1249 = _S1243.differential_0 + _S1246.differential_0;
        if(_runFlag_26)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1250;
            (&_S1250)->primal_0 = gt_normal_5;
            (&_S1250)->differential_0 = _S1204;
            s_bwd_normalize_impl_0(&_S1250, _S1248.differential_0);
            raydir_22 = _S1250.differential_0;
        }
        else
        {
            raydir_22 = _S1248.differential_0;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1251;
        (&_S1251)->primal_0 = gt_normal_5;
        (&_S1251)->differential_0 = _S1204;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1252;
        (&_S1252)->primal_0 = gt_normal_5;
        (&_S1252)->differential_0 = _S1204;
        s_bwd_prop_dot_0(&_S1251, &_S1252, 0.0f);
        float3  _S1253 = _S1252.differential_0 + _S1251.differential_0 + raydir_22;
        if(_S1221)
        {
            DiffPair_vectorx3Cfloatx2C3x3E_0 _S1254;
            (&_S1254)->primal_0 = _S1223;
            (&_S1254)->differential_0 = _S1204;
            s_bwd_normalize_impl_0(&_S1254, _S1249);
            raydir_22 = _S1254.differential_0;
        }
        else
        {
            raydir_22 = _S1249;
        }
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1255;
        (&_S1255)->primal_0 = _S1223;
        (&_S1255)->differential_0 = _S1204;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1256;
        (&_S1256)->primal_0 = _S1223;
        (&_S1256)->differential_0 = _S1204;
        s_bwd_prop_dot_0(&_S1255, &_S1256, 0.0f);
        float3  _S1257 = _S1256.differential_0 + _S1255.differential_0 + raydir_22;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1258;
        (&_S1258)->primal_0 = _S1224;
        (&_S1258)->differential_0 = _S1204;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1259;
        (&_S1259)->primal_0 = _S1225;
        (&_S1259)->differential_0 = _S1204;
        s_bwd_prop_cross_0(&_S1258, &_S1259, _S1257);
        float3  s_diff_dy_T_6 = - _S1259.differential_0;
        float3  _S1260 = - s_diff_dy_T_6;
        float3  _S1261 = - _S1258.differential_0;
        FixedArray<float3 , 5>  _S1262;
        _S1262[int(0)] = _S1204;
        _S1262[int(1)] = _S1204;
        _S1262[int(2)] = _S1204;
        _S1262[int(3)] = _S1204;
        _S1262[int(4)] = _S1204;
        _S1262[int(2)] = _S1260;
        _S1262[int(3)] = s_diff_dy_T_6;
        _S1262[int(0)] = _S1261;
        _S1262[int(1)] = _S1258.differential_0;
        points_13[int(0)] = _S1262[int(0)];
        points_13[int(1)] = _S1262[int(1)];
        points_13[int(2)] = _S1262[int(2)];
        points_13[int(3)] = _S1262[int(3)];
        points_13[int(4)] = _S1262[int(4)];
        raydir_22 = _S1253;
    }
    else
    {
        points_13[int(0)] = _S1204;
        points_13[int(1)] = _S1204;
        points_13[int(2)] = _S1204;
        points_13[int(3)] = _S1204;
        points_13[int(4)] = _S1204;
        raydir_22 = _S1204;
    }
    float4  _S1263;
    if(_S1205)
    {
        if(_runFlag_23)
        {
            if(_runFlag_24)
            {
                if(_runFlag_25)
                {
                    FixedArray<float3 , 5>  _S1264 = points_13;
                    FixedArray<float3 , 5>  _S1265 = points_13;
                    FixedArray<float3 , 5>  _S1266 = points_13;
                    float3  _S1267 = _S1207 * points_13[int(3)];
                    float _S1268 = _S1267.x + _S1267.y + _S1267.z;
                    float4  _S1269 = _S1238;
                    *&((&_S1269)->w) = _S1268;
                    points_13[int(0)] = _S1204;
                    points_13[int(1)] = _S1204;
                    points_13[int(2)] = _S1204;
                    points_13[int(3)] = _S1204;
                    points_13[int(4)] = _S1204;
                    _S1207 = _S1266[int(2)];
                    normal_16 = _S1264[int(0)];
                    _S1222 = _S1265[int(1)];
                    _S1263 = _S1269;
                }
                else
                {
                    FixedArray<float3 , 5>  _S1270 = points_13;
                    FixedArray<float3 , 5>  _S1271 = points_13;
                    FixedArray<float3 , 5>  _S1272 = points_13;
                    FixedArray<float3 , 5>  _S1273 = points_13;
                    points_13[int(0)] = points_13[int(0)];
                    points_13[int(1)] = _S1270[int(1)];
                    points_13[int(2)] = _S1271[int(2)];
                    points_13[int(3)] = _S1272[int(3)];
                    points_13[int(4)] = _S1273[int(4)];
                    _S1207 = _S1204;
                    normal_16 = _S1204;
                    _S1222 = _S1204;
                    _S1263 = _S1238;
                }
                float3  _S1274 = _S1208 * (points_13[int(2)] + _S1207);
                float _S1275 = _S1274.x + _S1274.y + _S1274.z;
                float3  _S1276 = points_13[int(0)] + normal_16;
                float3  _S1277 = points_13[int(1)] + _S1222;
                float4  _S1278 = _S1238;
                *&((&_S1278)->z) = _S1275;
                float4  _S1279 = _S1263 + _S1278;
                points_13[int(0)] = _S1204;
                points_13[int(1)] = _S1204;
                points_13[int(2)] = _S1204;
                points_13[int(3)] = _S1204;
                points_13[int(4)] = _S1204;
                _S1207 = _S1277;
                _S1208 = _S1276;
                _S1263 = _S1279;
            }
            else
            {
                FixedArray<float3 , 5>  _S1280 = points_13;
                FixedArray<float3 , 5>  _S1281 = points_13;
                FixedArray<float3 , 5>  _S1282 = points_13;
                FixedArray<float3 , 5>  _S1283 = points_13;
                points_13[int(0)] = points_13[int(0)];
                points_13[int(1)] = _S1280[int(1)];
                points_13[int(2)] = _S1281[int(2)];
                points_13[int(3)] = _S1282[int(3)];
                points_13[int(4)] = _S1283[int(4)];
                _S1207 = _S1204;
                _S1208 = _S1204;
                _S1263 = _S1238;
            }
            float3  _S1284 = _S1209 * (points_13[int(1)] + _S1207);
            float _S1285 = _S1284.x + _S1284.y + _S1284.z;
            float3  _S1286 = points_13[int(0)] + _S1208;
            float4  _S1287 = _S1238;
            *&((&_S1287)->y) = _S1285;
            float4  _S1288 = _S1263 + _S1287;
            points_13[int(0)] = _S1204;
            points_13[int(1)] = _S1204;
            points_13[int(2)] = _S1204;
            points_13[int(3)] = _S1204;
            points_13[int(4)] = _S1204;
            _S1207 = _S1286;
            _S1263 = _S1288;
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
            _S1207 = _S1204;
            _S1263 = _S1238;
        }
        float3  _S1293 = _S1210 * (points_13[int(0)] + _S1207);
        float _S1294 = _S1293.x + _S1293.y + _S1293.z;
        float4  _S1295 = _S1238;
        *&((&_S1295)->x) = _S1294;
        _S1263 = _S1263 + _S1295;
    }
    else
    {
        _S1263 = _S1238;
    }
    *v_depths_5 = _S1263;
    *v_gt_normal_2 = raydir_22;
    return;
}

