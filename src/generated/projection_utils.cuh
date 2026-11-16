#pragma once

#include "generated/slang.cuh"

struct SurfelHit_0
{
    float g_0;
    float g_lp_0;
    float t_hit_0;
    float t_mean_0;
};

inline __device__ SurfelHit_0 SurfelHit_x24_syn_dzero_0()
{
    SurfelHit_0 result_0;
    (&result_0)->g_0 = 0.0f;
    (&result_0)->g_lp_0 = 0.0f;
    (&result_0)->t_hit_0 = 0.0f;
    (&result_0)->t_mean_0 = 0.0f;
    return result_0;
}

inline __device__ SurfelHit_0 SurfelHit_x24_syn_dadd_0(SurfelHit_0 * SLANG_anonymous_0_0, SurfelHit_0 * SLANG_anonymous_1_0)
{
    SurfelHit_0 result_1;
    (&result_1)->g_0 = SLANG_anonymous_0_0->g_0 + SLANG_anonymous_1_0->g_0;
    (&result_1)->g_lp_0 = SLANG_anonymous_0_0->g_lp_0 + SLANG_anonymous_1_0->g_lp_0;
    (&result_1)->t_hit_0 = SLANG_anonymous_0_0->t_hit_0 + SLANG_anonymous_1_0->t_hit_0;
    (&result_1)->t_mean_0 = SLANG_anonymous_0_0->t_mean_0 + SLANG_anonymous_1_0->t_mean_0;
    return result_1;
}

inline __device__ Matrix<float, 3, 3>  transpose_0(Matrix<float, 3, 3>  x_0)
{
    Matrix<float, 3, 3>  result_2;
    int r_0 = int(0);
    for(;;)
    {
        if(r_0 < int(3))
        {
        }
        else
        {
            break;
        }
        int c_0 = int(0);
        for(;;)
        {
            if(c_0 < int(3))
            {
            }
            else
            {
                break;
            }
            *_slang_vector_get_element_ptr(((&result_2)->rows + (r_0)), c_0) = _slang_vector_get_element(x_0.rows[c_0], r_0);
            c_0 = c_0 + int(1);
        }
        r_0 = r_0 + int(1);
    }
    return result_2;
}

inline __device__ Matrix<float, 2, 2>  transpose_1(Matrix<float, 2, 2>  x_1)
{
    Matrix<float, 2, 2>  result_3;
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
            *_slang_vector_get_element_ptr(((&result_3)->rows + (r_1)), c_1) = _slang_vector_get_element(x_1.rows[c_1], r_1);
            c_1 = c_1 + int(1);
        }
        r_1 = r_1 + int(1);
    }
    return result_3;
}

inline __device__ Matrix<float, 3, 3>  normalized_quat_to_rotmat(float4  quat_0)
{
    float x_2 = quat_0.y;
    float x2_0 = x_2 * x_2;
    float y2_0 = quat_0.z * quat_0.z;
    float z2_0 = quat_0.w * quat_0.w;
    float xy_0 = quat_0.y * quat_0.z;
    float xz_0 = quat_0.y * quat_0.w;
    float yz_0 = quat_0.z * quat_0.w;
    float wx_0 = quat_0.x * quat_0.y;
    float wy_0 = quat_0.x * quat_0.z;
    float wz_0 = quat_0.x * quat_0.w;
    return transpose_0(makeMatrix<float, 3, 3> (1.0f - 2.0f * (y2_0 + z2_0), 2.0f * (xy_0 + wz_0), 2.0f * (xz_0 - wy_0), 2.0f * (xy_0 - wz_0), 1.0f - 2.0f * (x2_0 + z2_0), 2.0f * (yz_0 + wx_0), 2.0f * (xz_0 + wy_0), 2.0f * (yz_0 - wx_0), 1.0f - 2.0f * (x2_0 + y2_0)));
}

struct DiffPair_matrixx3Cfloatx2C3x2C3x3E_0
{
    Matrix<float, 3, 3>  primal_0;
    Matrix<float, 3, 3>  differential_0;
};

inline __device__ void mul_0(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * left_0, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * right_0, Matrix<float, 3, 3>  dOut_0)
{
    Matrix<float, 3, 3>  left_d_result_0;
    *&(((&left_d_result_0)->rows + (int(0)))->x) = 0.0f;
    *&(((&left_d_result_0)->rows + (int(0)))->y) = 0.0f;
    *&(((&left_d_result_0)->rows + (int(0)))->z) = 0.0f;
    *&(((&left_d_result_0)->rows + (int(1)))->x) = 0.0f;
    *&(((&left_d_result_0)->rows + (int(1)))->y) = 0.0f;
    *&(((&left_d_result_0)->rows + (int(1)))->z) = 0.0f;
    *&(((&left_d_result_0)->rows + (int(2)))->x) = 0.0f;
    *&(((&left_d_result_0)->rows + (int(2)))->y) = 0.0f;
    *&(((&left_d_result_0)->rows + (int(2)))->z) = 0.0f;
    Matrix<float, 3, 3>  right_d_result_0;
    *&(((&right_d_result_0)->rows + (int(0)))->x) = 0.0f;
    *&(((&right_d_result_0)->rows + (int(0)))->y) = 0.0f;
    *&(((&right_d_result_0)->rows + (int(0)))->z) = 0.0f;
    *&(((&right_d_result_0)->rows + (int(1)))->x) = 0.0f;
    *&(((&right_d_result_0)->rows + (int(1)))->y) = 0.0f;
    *&(((&right_d_result_0)->rows + (int(1)))->z) = 0.0f;
    *&(((&right_d_result_0)->rows + (int(2)))->x) = 0.0f;
    *&(((&right_d_result_0)->rows + (int(2)))->y) = 0.0f;
    *&(((&right_d_result_0)->rows + (int(2)))->z) = 0.0f;
    *&(((&left_d_result_0)->rows + (int(0)))->x) = *&(((&left_d_result_0)->rows + (int(0)))->x) + (*right_0).primal_0.rows[int(0)].x * dOut_0.rows[int(0)].x;
    *&(((&right_d_result_0)->rows + (int(0)))->x) = *&(((&right_d_result_0)->rows + (int(0)))->x) + (*left_0).primal_0.rows[int(0)].x * dOut_0.rows[int(0)].x;
    *&(((&left_d_result_0)->rows + (int(0)))->y) = *&(((&left_d_result_0)->rows + (int(0)))->y) + (*right_0).primal_0.rows[int(1)].x * dOut_0.rows[int(0)].x;
    *&(((&right_d_result_0)->rows + (int(1)))->x) = *&(((&right_d_result_0)->rows + (int(1)))->x) + (*left_0).primal_0.rows[int(0)].y * dOut_0.rows[int(0)].x;
    *&(((&left_d_result_0)->rows + (int(0)))->z) = *&(((&left_d_result_0)->rows + (int(0)))->z) + (*right_0).primal_0.rows[int(2)].x * dOut_0.rows[int(0)].x;
    *&(((&right_d_result_0)->rows + (int(2)))->x) = *&(((&right_d_result_0)->rows + (int(2)))->x) + (*left_0).primal_0.rows[int(0)].z * dOut_0.rows[int(0)].x;
    *&(((&left_d_result_0)->rows + (int(0)))->x) = *&(((&left_d_result_0)->rows + (int(0)))->x) + (*right_0).primal_0.rows[int(0)].y * dOut_0.rows[int(0)].y;
    *&(((&right_d_result_0)->rows + (int(0)))->y) = *&(((&right_d_result_0)->rows + (int(0)))->y) + (*left_0).primal_0.rows[int(0)].x * dOut_0.rows[int(0)].y;
    *&(((&left_d_result_0)->rows + (int(0)))->y) = *&(((&left_d_result_0)->rows + (int(0)))->y) + (*right_0).primal_0.rows[int(1)].y * dOut_0.rows[int(0)].y;
    *&(((&right_d_result_0)->rows + (int(1)))->y) = *&(((&right_d_result_0)->rows + (int(1)))->y) + (*left_0).primal_0.rows[int(0)].y * dOut_0.rows[int(0)].y;
    *&(((&left_d_result_0)->rows + (int(0)))->z) = *&(((&left_d_result_0)->rows + (int(0)))->z) + (*right_0).primal_0.rows[int(2)].y * dOut_0.rows[int(0)].y;
    *&(((&right_d_result_0)->rows + (int(2)))->y) = *&(((&right_d_result_0)->rows + (int(2)))->y) + (*left_0).primal_0.rows[int(0)].z * dOut_0.rows[int(0)].y;
    *&(((&left_d_result_0)->rows + (int(0)))->x) = *&(((&left_d_result_0)->rows + (int(0)))->x) + (*right_0).primal_0.rows[int(0)].z * dOut_0.rows[int(0)].z;
    *&(((&right_d_result_0)->rows + (int(0)))->z) = *&(((&right_d_result_0)->rows + (int(0)))->z) + (*left_0).primal_0.rows[int(0)].x * dOut_0.rows[int(0)].z;
    *&(((&left_d_result_0)->rows + (int(0)))->y) = *&(((&left_d_result_0)->rows + (int(0)))->y) + (*right_0).primal_0.rows[int(1)].z * dOut_0.rows[int(0)].z;
    *&(((&right_d_result_0)->rows + (int(1)))->z) = *&(((&right_d_result_0)->rows + (int(1)))->z) + (*left_0).primal_0.rows[int(0)].y * dOut_0.rows[int(0)].z;
    *&(((&left_d_result_0)->rows + (int(0)))->z) = *&(((&left_d_result_0)->rows + (int(0)))->z) + (*right_0).primal_0.rows[int(2)].z * dOut_0.rows[int(0)].z;
    *&(((&right_d_result_0)->rows + (int(2)))->z) = *&(((&right_d_result_0)->rows + (int(2)))->z) + (*left_0).primal_0.rows[int(0)].z * dOut_0.rows[int(0)].z;
    *&(((&left_d_result_0)->rows + (int(1)))->x) = *&(((&left_d_result_0)->rows + (int(1)))->x) + (*right_0).primal_0.rows[int(0)].x * dOut_0.rows[int(1)].x;
    *&(((&right_d_result_0)->rows + (int(0)))->x) = *&(((&right_d_result_0)->rows + (int(0)))->x) + (*left_0).primal_0.rows[int(1)].x * dOut_0.rows[int(1)].x;
    *&(((&left_d_result_0)->rows + (int(1)))->y) = *&(((&left_d_result_0)->rows + (int(1)))->y) + (*right_0).primal_0.rows[int(1)].x * dOut_0.rows[int(1)].x;
    *&(((&right_d_result_0)->rows + (int(1)))->x) = *&(((&right_d_result_0)->rows + (int(1)))->x) + (*left_0).primal_0.rows[int(1)].y * dOut_0.rows[int(1)].x;
    *&(((&left_d_result_0)->rows + (int(1)))->z) = *&(((&left_d_result_0)->rows + (int(1)))->z) + (*right_0).primal_0.rows[int(2)].x * dOut_0.rows[int(1)].x;
    *&(((&right_d_result_0)->rows + (int(2)))->x) = *&(((&right_d_result_0)->rows + (int(2)))->x) + (*left_0).primal_0.rows[int(1)].z * dOut_0.rows[int(1)].x;
    *&(((&left_d_result_0)->rows + (int(1)))->x) = *&(((&left_d_result_0)->rows + (int(1)))->x) + (*right_0).primal_0.rows[int(0)].y * dOut_0.rows[int(1)].y;
    *&(((&right_d_result_0)->rows + (int(0)))->y) = *&(((&right_d_result_0)->rows + (int(0)))->y) + (*left_0).primal_0.rows[int(1)].x * dOut_0.rows[int(1)].y;
    *&(((&left_d_result_0)->rows + (int(1)))->y) = *&(((&left_d_result_0)->rows + (int(1)))->y) + (*right_0).primal_0.rows[int(1)].y * dOut_0.rows[int(1)].y;
    *&(((&right_d_result_0)->rows + (int(1)))->y) = *&(((&right_d_result_0)->rows + (int(1)))->y) + (*left_0).primal_0.rows[int(1)].y * dOut_0.rows[int(1)].y;
    *&(((&left_d_result_0)->rows + (int(1)))->z) = *&(((&left_d_result_0)->rows + (int(1)))->z) + (*right_0).primal_0.rows[int(2)].y * dOut_0.rows[int(1)].y;
    *&(((&right_d_result_0)->rows + (int(2)))->y) = *&(((&right_d_result_0)->rows + (int(2)))->y) + (*left_0).primal_0.rows[int(1)].z * dOut_0.rows[int(1)].y;
    *&(((&left_d_result_0)->rows + (int(1)))->x) = *&(((&left_d_result_0)->rows + (int(1)))->x) + (*right_0).primal_0.rows[int(0)].z * dOut_0.rows[int(1)].z;
    *&(((&right_d_result_0)->rows + (int(0)))->z) = *&(((&right_d_result_0)->rows + (int(0)))->z) + (*left_0).primal_0.rows[int(1)].x * dOut_0.rows[int(1)].z;
    *&(((&left_d_result_0)->rows + (int(1)))->y) = *&(((&left_d_result_0)->rows + (int(1)))->y) + (*right_0).primal_0.rows[int(1)].z * dOut_0.rows[int(1)].z;
    *&(((&right_d_result_0)->rows + (int(1)))->z) = *&(((&right_d_result_0)->rows + (int(1)))->z) + (*left_0).primal_0.rows[int(1)].y * dOut_0.rows[int(1)].z;
    *&(((&left_d_result_0)->rows + (int(1)))->z) = *&(((&left_d_result_0)->rows + (int(1)))->z) + (*right_0).primal_0.rows[int(2)].z * dOut_0.rows[int(1)].z;
    *&(((&right_d_result_0)->rows + (int(2)))->z) = *&(((&right_d_result_0)->rows + (int(2)))->z) + (*left_0).primal_0.rows[int(1)].z * dOut_0.rows[int(1)].z;
    *&(((&left_d_result_0)->rows + (int(2)))->x) = *&(((&left_d_result_0)->rows + (int(2)))->x) + (*right_0).primal_0.rows[int(0)].x * dOut_0.rows[int(2)].x;
    *&(((&right_d_result_0)->rows + (int(0)))->x) = *&(((&right_d_result_0)->rows + (int(0)))->x) + (*left_0).primal_0.rows[int(2)].x * dOut_0.rows[int(2)].x;
    *&(((&left_d_result_0)->rows + (int(2)))->y) = *&(((&left_d_result_0)->rows + (int(2)))->y) + (*right_0).primal_0.rows[int(1)].x * dOut_0.rows[int(2)].x;
    *&(((&right_d_result_0)->rows + (int(1)))->x) = *&(((&right_d_result_0)->rows + (int(1)))->x) + (*left_0).primal_0.rows[int(2)].y * dOut_0.rows[int(2)].x;
    *&(((&left_d_result_0)->rows + (int(2)))->z) = *&(((&left_d_result_0)->rows + (int(2)))->z) + (*right_0).primal_0.rows[int(2)].x * dOut_0.rows[int(2)].x;
    *&(((&right_d_result_0)->rows + (int(2)))->x) = *&(((&right_d_result_0)->rows + (int(2)))->x) + (*left_0).primal_0.rows[int(2)].z * dOut_0.rows[int(2)].x;
    *&(((&left_d_result_0)->rows + (int(2)))->x) = *&(((&left_d_result_0)->rows + (int(2)))->x) + (*right_0).primal_0.rows[int(0)].y * dOut_0.rows[int(2)].y;
    *&(((&right_d_result_0)->rows + (int(0)))->y) = *&(((&right_d_result_0)->rows + (int(0)))->y) + (*left_0).primal_0.rows[int(2)].x * dOut_0.rows[int(2)].y;
    *&(((&left_d_result_0)->rows + (int(2)))->y) = *&(((&left_d_result_0)->rows + (int(2)))->y) + (*right_0).primal_0.rows[int(1)].y * dOut_0.rows[int(2)].y;
    *&(((&right_d_result_0)->rows + (int(1)))->y) = *&(((&right_d_result_0)->rows + (int(1)))->y) + (*left_0).primal_0.rows[int(2)].y * dOut_0.rows[int(2)].y;
    *&(((&left_d_result_0)->rows + (int(2)))->z) = *&(((&left_d_result_0)->rows + (int(2)))->z) + (*right_0).primal_0.rows[int(2)].y * dOut_0.rows[int(2)].y;
    *&(((&right_d_result_0)->rows + (int(2)))->y) = *&(((&right_d_result_0)->rows + (int(2)))->y) + (*left_0).primal_0.rows[int(2)].z * dOut_0.rows[int(2)].y;
    *&(((&left_d_result_0)->rows + (int(2)))->x) = *&(((&left_d_result_0)->rows + (int(2)))->x) + (*right_0).primal_0.rows[int(0)].z * dOut_0.rows[int(2)].z;
    *&(((&right_d_result_0)->rows + (int(0)))->z) = *&(((&right_d_result_0)->rows + (int(0)))->z) + (*left_0).primal_0.rows[int(2)].x * dOut_0.rows[int(2)].z;
    *&(((&left_d_result_0)->rows + (int(2)))->y) = *&(((&left_d_result_0)->rows + (int(2)))->y) + (*right_0).primal_0.rows[int(1)].z * dOut_0.rows[int(2)].z;
    *&(((&right_d_result_0)->rows + (int(1)))->z) = *&(((&right_d_result_0)->rows + (int(1)))->z) + (*left_0).primal_0.rows[int(2)].y * dOut_0.rows[int(2)].z;
    *&(((&left_d_result_0)->rows + (int(2)))->z) = *&(((&left_d_result_0)->rows + (int(2)))->z) + (*right_0).primal_0.rows[int(2)].z * dOut_0.rows[int(2)].z;
    *&(((&right_d_result_0)->rows + (int(2)))->z) = *&(((&right_d_result_0)->rows + (int(2)))->z) + (*left_0).primal_0.rows[int(2)].z * dOut_0.rows[int(2)].z;
    left_0->primal_0 = (*left_0).primal_0;
    left_0->differential_0 = left_d_result_0;
    right_0->primal_0 = (*right_0).primal_0;
    right_0->differential_0 = right_d_result_0;
    return;
}

inline __device__ Matrix<float, 3, 3>  mul_1(Matrix<float, 3, 3>  left_1, Matrix<float, 3, 3>  right_1)
{
    Matrix<float, 3, 3>  result_4;
    int r_2 = int(0);
    for(;;)
    {
        if(r_2 < int(3))
        {
        }
        else
        {
            break;
        }
        int c_2 = int(0);
        for(;;)
        {
            if(c_2 < int(3))
            {
            }
            else
            {
                break;
            }
            int i_0 = int(0);
            float sum_0 = 0.0f;
            for(;;)
            {
                if(i_0 < int(3))
                {
                }
                else
                {
                    break;
                }
                float sum_1 = sum_0 + _slang_vector_get_element(left_1.rows[r_2], i_0) * _slang_vector_get_element(right_1.rows[i_0], c_2);
                i_0 = i_0 + int(1);
                sum_0 = sum_1;
            }
            *_slang_vector_get_element_ptr(((&result_4)->rows + (r_2)), c_2) = sum_0;
            c_2 = c_2 + int(1);
        }
        r_2 = r_2 + int(1);
    }
    return result_4;
}

inline __device__ void quat_scale_to_covar(float4  quat_1, float3  scale_0, Matrix<float, 3, 3>  * covar_0)
{
    float x_3 = quat_1.y;
    float x2_1 = x_3 * x_3;
    float y2_1 = quat_1.z * quat_1.z;
    float z2_1 = quat_1.w * quat_1.w;
    float xy_1 = quat_1.y * quat_1.z;
    float xz_1 = quat_1.y * quat_1.w;
    float yz_1 = quat_1.z * quat_1.w;
    float wx_1 = quat_1.x * quat_1.y;
    float wy_1 = quat_1.x * quat_1.z;
    float wz_1 = quat_1.x * quat_1.w;
    Matrix<float, 3, 3>  M_0 = mul_1(transpose_0(makeMatrix<float, 3, 3> (1.0f - 2.0f * (y2_1 + z2_1), 2.0f * (xy_1 + wz_1), 2.0f * (xz_1 - wy_1), 2.0f * (xy_1 - wz_1), 1.0f - 2.0f * (x2_1 + z2_1), 2.0f * (yz_1 + wx_1), 2.0f * (xz_1 + wy_1), 2.0f * (yz_1 - wx_1), 1.0f - 2.0f * (x2_1 + y2_1))), makeMatrix<float, 3, 3> (scale_0.x, 0.0f, 0.0f, 0.0f, scale_0.y, 0.0f, 0.0f, 0.0f, scale_0.z));
    *covar_0 = mul_1(M_0, transpose_0(M_0));
    return;
}

inline __device__ void quat_scale_to_sqrt_covar(float4  quat_2, float3  scale_1, Matrix<float, 3, 3>  * M_1)
{
    float x_4 = quat_2.y;
    float x2_2 = x_4 * x_4;
    float y2_2 = quat_2.z * quat_2.z;
    float z2_2 = quat_2.w * quat_2.w;
    float xy_2 = quat_2.y * quat_2.z;
    float xz_2 = quat_2.y * quat_2.w;
    float yz_2 = quat_2.z * quat_2.w;
    float wx_2 = quat_2.x * quat_2.y;
    float wy_2 = quat_2.x * quat_2.z;
    float wz_2 = quat_2.x * quat_2.w;
    *M_1 = mul_1(transpose_0(makeMatrix<float, 3, 3> (1.0f - 2.0f * (y2_2 + z2_2), 2.0f * (xy_2 + wz_2), 2.0f * (xz_2 - wy_2), 2.0f * (xy_2 - wz_2), 1.0f - 2.0f * (x2_2 + z2_2), 2.0f * (yz_2 + wx_2), 2.0f * (xz_2 + wy_2), 2.0f * (yz_2 - wx_2), 1.0f - 2.0f * (x2_2 + y2_2))), makeMatrix<float, 3, 3> (scale_1.x, 0.0f, 0.0f, 0.0f, scale_1.y, 0.0f, 0.0f, 0.0f, scale_1.z));
    return;
}

struct DiffPair_vectorx3Cfloatx2C3x3E_0
{
    float3  primal_0;
    float3  differential_0;
};

inline __device__ void _d_mul_0(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * left_2, DiffPair_vectorx3Cfloatx2C3x3E_0 * right_2, float3  dOut_1)
{
    float _S1 = (*left_2).primal_0.rows[int(0)].x * dOut_1.x;
    Matrix<float, 3, 3>  left_d_result_1;
    *&(((&left_d_result_1)->rows + (int(0)))->x) = (*right_2).primal_0.x * dOut_1.x;
    float sum_2 = _S1 + (*left_2).primal_0.rows[int(1)].x * dOut_1.y;
    *&(((&left_d_result_1)->rows + (int(1)))->x) = (*right_2).primal_0.x * dOut_1.y;
    float sum_3 = sum_2 + (*left_2).primal_0.rows[int(2)].x * dOut_1.z;
    *&(((&left_d_result_1)->rows + (int(2)))->x) = (*right_2).primal_0.x * dOut_1.z;
    float3  right_d_result_1;
    *&((&right_d_result_1)->x) = sum_3;
    float _S2 = (*left_2).primal_0.rows[int(0)].y * dOut_1.x;
    *&(((&left_d_result_1)->rows + (int(0)))->y) = (*right_2).primal_0.y * dOut_1.x;
    float sum_4 = _S2 + (*left_2).primal_0.rows[int(1)].y * dOut_1.y;
    *&(((&left_d_result_1)->rows + (int(1)))->y) = (*right_2).primal_0.y * dOut_1.y;
    float sum_5 = sum_4 + (*left_2).primal_0.rows[int(2)].y * dOut_1.z;
    *&(((&left_d_result_1)->rows + (int(2)))->y) = (*right_2).primal_0.y * dOut_1.z;
    *&((&right_d_result_1)->y) = sum_5;
    float _S3 = (*left_2).primal_0.rows[int(0)].z * dOut_1.x;
    *&(((&left_d_result_1)->rows + (int(0)))->z) = (*right_2).primal_0.z * dOut_1.x;
    float sum_6 = _S3 + (*left_2).primal_0.rows[int(1)].z * dOut_1.y;
    *&(((&left_d_result_1)->rows + (int(1)))->z) = (*right_2).primal_0.z * dOut_1.y;
    float sum_7 = sum_6 + (*left_2).primal_0.rows[int(2)].z * dOut_1.z;
    *&(((&left_d_result_1)->rows + (int(2)))->z) = (*right_2).primal_0.z * dOut_1.z;
    *&((&right_d_result_1)->z) = sum_7;
    left_2->primal_0 = (*left_2).primal_0;
    left_2->differential_0 = left_d_result_1;
    right_2->primal_0 = (*right_2).primal_0;
    right_2->differential_0 = right_d_result_1;
    return;
}

inline __device__ float3  mul_2(Matrix<float, 3, 3>  left_3, float3  right_3)
{
    float3  result_5;
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
        int j_0 = int(0);
        float sum_8 = 0.0f;
        for(;;)
        {
            if(j_0 < int(3))
            {
            }
            else
            {
                break;
            }
            float sum_9 = sum_8 + _slang_vector_get_element(left_3.rows[i_1], j_0) * _slang_vector_get_element(right_3, j_0);
            j_0 = j_0 + int(1);
            sum_8 = sum_9;
        }
        *_slang_vector_get_element_ptr(&result_5, i_1) = sum_8;
        i_1 = i_1 + int(1);
    }
    return result_5;
}

inline __device__ float3  apply_sqrt_covar_to_vec(float4  quat_3, float3  scale_2, float3  vec_0)
{
    float x_5 = quat_3.y;
    float x2_3 = x_5 * x_5;
    float y2_3 = quat_3.z * quat_3.z;
    float z2_3 = quat_3.w * quat_3.w;
    float xy_3 = quat_3.y * quat_3.z;
    float xz_3 = quat_3.y * quat_3.w;
    float yz_3 = quat_3.z * quat_3.w;
    float wx_3 = quat_3.x * quat_3.y;
    float wy_3 = quat_3.x * quat_3.z;
    float wz_3 = quat_3.x * quat_3.w;
    return mul_2(transpose_0(makeMatrix<float, 3, 3> (1.0f - 2.0f * (y2_3 + z2_3), 2.0f * (xy_3 + wz_3), 2.0f * (xz_3 - wy_3), 2.0f * (xy_3 - wz_3), 1.0f - 2.0f * (x2_3 + z2_3), 2.0f * (yz_3 + wx_3), 2.0f * (xz_3 + wy_3), 2.0f * (yz_3 - wx_3), 1.0f - 2.0f * (x2_3 + y2_3))), scale_2 * vec_0);
}

inline __device__ float3  apply_covar_to_vec(float4  quat_4, float3  scale_3, float3  vec_1)
{
    float x_6 = quat_4.y;
    float x2_4 = x_6 * x_6;
    float y2_4 = quat_4.z * quat_4.z;
    float z2_4 = quat_4.w * quat_4.w;
    float xy_4 = quat_4.y * quat_4.z;
    float xz_4 = quat_4.y * quat_4.w;
    float yz_4 = quat_4.z * quat_4.w;
    float wx_4 = quat_4.x * quat_4.y;
    float wy_4 = quat_4.x * quat_4.z;
    float wz_4 = quat_4.x * quat_4.w;
    Matrix<float, 3, 3>  M_2 = mul_1(transpose_0(makeMatrix<float, 3, 3> (1.0f - 2.0f * (y2_4 + z2_4), 2.0f * (xy_4 + wz_4), 2.0f * (xz_4 - wy_4), 2.0f * (xy_4 - wz_4), 1.0f - 2.0f * (x2_4 + z2_4), 2.0f * (yz_4 + wx_4), 2.0f * (xz_4 + wy_4), 2.0f * (yz_4 - wx_4), 1.0f - 2.0f * (x2_4 + y2_4))), makeMatrix<float, 3, 3> (scale_3.x, 0.0f, 0.0f, 0.0f, scale_3.y, 0.0f, 0.0f, 0.0f, scale_3.z));
    return mul_2(mul_1(M_2, transpose_0(M_2)), vec_1);
}

struct DiffPair_float_0
{
    float primal_0;
    float differential_0;
};

inline __device__ DiffPair_float_0 _d_atan2_0(DiffPair_float_0 * dpy_0, DiffPair_float_0 * dpx_0)
{
    float _S4 = dpx_0->primal_0 * dpx_0->primal_0 + dpy_0->primal_0 * dpy_0->primal_0;
    DiffPair_float_0 _S5 = { (F32_atan2((dpy_0->primal_0), (dpx_0->primal_0))), - dpy_0->primal_0 / _S4 * dpx_0->differential_0 + dpx_0->primal_0 / _S4 * dpy_0->differential_0 };
    return _S5;
}

inline __device__ void _d_sqrt_0(DiffPair_float_0 * dpx_1, float dOut_2)
{
    float _S6 = 0.5f / (F32_sqrt(((F32_max((1.00000001168609742e-07f), ((*dpx_1).primal_0)))))) * dOut_2;
    dpx_1->primal_0 = (*dpx_1).primal_0;
    dpx_1->differential_0 = _S6;
    return;
}

inline __device__ DiffPair_float_0 _d_sqrt_1(DiffPair_float_0 * dpx_2)
{
    DiffPair_float_0 _S7 = { (F32_sqrt((dpx_2->primal_0))), 0.5f / (F32_sqrt(((F32_max((1.00000001168609742e-07f), (dpx_2->primal_0)))))) * dpx_2->differential_0 };
    return _S7;
}

struct DiffPair_vectorx3Cfloatx2C2x3E_0
{
    float2  primal_0;
    float2  differential_0;
};

inline __device__ void _d_dot_0(DiffPair_vectorx3Cfloatx2C2x3E_0 * dpx_3, DiffPair_vectorx3Cfloatx2C2x3E_0 * dpy_1, float dOut_3)
{
    float2  x_d_result_0;
    *&((&x_d_result_0)->x) = (*dpy_1).primal_0.x * dOut_3;
    float2  y_d_result_0;
    *&((&y_d_result_0)->x) = (*dpx_3).primal_0.x * dOut_3;
    *&((&x_d_result_0)->y) = (*dpy_1).primal_0.y * dOut_3;
    *&((&y_d_result_0)->y) = (*dpx_3).primal_0.y * dOut_3;
    dpx_3->primal_0 = (*dpx_3).primal_0;
    dpx_3->differential_0 = x_d_result_0;
    dpy_1->primal_0 = (*dpy_1).primal_0;
    dpy_1->differential_0 = y_d_result_0;
    return;
}

inline __device__ void _d_dot_1(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_4, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpy_2, float dOut_4)
{
    float3  x_d_result_1;
    *&((&x_d_result_1)->x) = (*dpy_2).primal_0.x * dOut_4;
    float3  y_d_result_1;
    *&((&y_d_result_1)->x) = (*dpx_4).primal_0.x * dOut_4;
    *&((&x_d_result_1)->y) = (*dpy_2).primal_0.y * dOut_4;
    *&((&y_d_result_1)->y) = (*dpx_4).primal_0.y * dOut_4;
    *&((&x_d_result_1)->z) = (*dpy_2).primal_0.z * dOut_4;
    *&((&y_d_result_1)->z) = (*dpx_4).primal_0.z * dOut_4;
    dpx_4->primal_0 = (*dpx_4).primal_0;
    dpx_4->differential_0 = x_d_result_1;
    dpy_2->primal_0 = (*dpy_2).primal_0;
    dpy_2->differential_0 = y_d_result_1;
    return;
}

inline __device__ float dot_0(float2  x_7, float2  y_0)
{
    int i_2 = int(0);
    float result_6 = 0.0f;
    for(;;)
    {
        if(i_2 < int(2))
        {
        }
        else
        {
            break;
        }
        float result_7 = result_6 + _slang_vector_get_element(x_7, i_2) * _slang_vector_get_element(y_0, i_2);
        i_2 = i_2 + int(1);
        result_6 = result_7;
    }
    return result_6;
}

inline __device__ float dot_1(float3  x_8, float3  y_1)
{
    int i_3 = int(0);
    float result_8 = 0.0f;
    for(;;)
    {
        if(i_3 < int(3))
        {
        }
        else
        {
            break;
        }
        float result_9 = result_8 + _slang_vector_get_element(x_8, i_3) * _slang_vector_get_element(y_1, i_3);
        i_3 = i_3 + int(1);
        result_8 = result_9;
    }
    return result_8;
}

inline __device__ float length_0(float2  x_9)
{
    return (F32_sqrt((dot_0(x_9, x_9))));
}

inline __device__ float length_1(float3  x_10)
{
    return (F32_sqrt((dot_1(x_10, x_10))));
}

inline __device__ bool equirect_proj_nav(float3  p_view_0, float4  intrins_0, float2  * uv_0)
{
    *uv_0 = make_float2 (intrins_0.x * (F32_atan2((p_view_0.x), (p_view_0.z))) + intrins_0.z, intrins_0.y * (F32_atan2((p_view_0.y), (length_0(float2 {p_view_0.x, p_view_0.z})))) + intrins_0.w);
    return true;
}

inline __device__ DiffPair_float_0 s_fwd_length_impl_0(DiffPair_vectorx3Cfloatx2C2x3E_0 * dpx_5)
{
    float _S8 = *&((&dpx_5->differential_0)->x) * *&((&dpx_5->primal_0)->x);
    float _S9 = *&((&dpx_5->differential_0)->y) * *&((&dpx_5->primal_0)->y);
    float s_diff_len_0 = _S8 + _S8 + (_S9 + _S9);
    DiffPair_float_0 _S10;
    (&_S10)->primal_0 = *&((&dpx_5->primal_0)->x) * *&((&dpx_5->primal_0)->x) + *&((&dpx_5->primal_0)->y) * *&((&dpx_5->primal_0)->y);
    (&_S10)->differential_0 = s_diff_len_0;
    DiffPair_float_0 _S11 = _d_sqrt_1(&_S10);
    DiffPair_float_0 _S12 = { _S11.primal_0, _S11.differential_0 };
    return _S12;
}

inline __device__ Matrix<float, 2, 3>  equirect_proj_jac(float3  p_view_1, float4  intrins_1)
{
    float _S13 = p_view_1.x;
    float _S14 = p_view_1.z;
    DiffPair_float_0 _S15;
    (&_S15)->primal_0 = _S13;
    (&_S15)->differential_0 = 1.0f;
    DiffPair_float_0 _S16;
    (&_S16)->primal_0 = _S14;
    (&_S16)->differential_0 = 0.0f;
    DiffPair_float_0 _S17 = _d_atan2_0(&_S15, &_S16);
    float _S18 = p_view_1.y;
    float2  _S19 = float2 {p_view_1.x, p_view_1.z};
    float2  _S20 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S21;
    (&_S21)->primal_0 = _S19;
    (&_S21)->differential_0 = _S20;
    DiffPair_float_0 _S22 = s_fwd_length_impl_0(&_S21);
    DiffPair_float_0 _S23;
    (&_S23)->primal_0 = _S18;
    (&_S23)->differential_0 = 0.0f;
    DiffPair_float_0 _S24;
    (&_S24)->primal_0 = _S22.primal_0;
    (&_S24)->differential_0 = _S22.differential_0;
    DiffPair_float_0 _S25 = _d_atan2_0(&_S23, &_S24);
    float fx_0 = intrins_1.x;
    float fy_0 = intrins_1.y;
    float _S26 = _S25.differential_0 * fy_0;
    Matrix<float, 2, 3>  J_0;
    *&(((&J_0)->rows + (int(0)))->x) = _S17.differential_0 * fx_0;
    *&(((&J_0)->rows + (int(1)))->x) = _S26;
    DiffPair_float_0 _S27;
    (&_S27)->primal_0 = _S13;
    (&_S27)->differential_0 = 0.0f;
    DiffPair_float_0 _S28;
    (&_S28)->primal_0 = _S14;
    (&_S28)->differential_0 = 0.0f;
    DiffPair_float_0 _S29 = _d_atan2_0(&_S27, &_S28);
    float2  _S30 = make_float2 (0.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S31;
    (&_S31)->primal_0 = _S19;
    (&_S31)->differential_0 = _S30;
    DiffPair_float_0 _S32 = s_fwd_length_impl_0(&_S31);
    DiffPair_float_0 _S33;
    (&_S33)->primal_0 = _S18;
    (&_S33)->differential_0 = 1.0f;
    DiffPair_float_0 _S34;
    (&_S34)->primal_0 = _S32.primal_0;
    (&_S34)->differential_0 = _S32.differential_0;
    DiffPair_float_0 _S35 = _d_atan2_0(&_S33, &_S34);
    float _S36 = _S35.differential_0 * fy_0;
    *&(((&J_0)->rows + (int(0)))->y) = _S29.differential_0 * fx_0;
    *&(((&J_0)->rows + (int(1)))->y) = _S36;
    DiffPair_float_0 _S37;
    (&_S37)->primal_0 = _S13;
    (&_S37)->differential_0 = 0.0f;
    DiffPair_float_0 _S38;
    (&_S38)->primal_0 = _S14;
    (&_S38)->differential_0 = 1.0f;
    DiffPair_float_0 _S39 = _d_atan2_0(&_S37, &_S38);
    float2  _S40 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S41;
    (&_S41)->primal_0 = _S19;
    (&_S41)->differential_0 = _S40;
    DiffPair_float_0 _S42 = s_fwd_length_impl_0(&_S41);
    DiffPair_float_0 _S43;
    (&_S43)->primal_0 = _S18;
    (&_S43)->differential_0 = 0.0f;
    DiffPair_float_0 _S44;
    (&_S44)->primal_0 = _S42.primal_0;
    (&_S44)->differential_0 = _S42.differential_0;
    DiffPair_float_0 _S45 = _d_atan2_0(&_S43, &_S44);
    float _S46 = _S45.differential_0 * fy_0;
    *&(((&J_0)->rows + (int(0)))->z) = _S39.differential_0 * fx_0;
    *&(((&J_0)->rows + (int(1)))->z) = _S46;
    return J_0;
}

inline __device__ float determinant_0(Matrix<float, 2, 2>  m_0)
{
    return m_0.rows[int(0)].x * m_0.rows[int(1)].y - m_0.rows[int(0)].y * m_0.rows[int(1)].x;
}

inline __device__ bool is_valid_distortion_none(float2  uv_1, FixedArray<float, 1>  dist_coeffs_0)
{
    return true;
}

inline __device__ float2  DistNone_distort_0(float2  uv_2, FixedArray<float, 1>  * coeffs_0)
{
    return uv_2;
}

inline __device__ bool persp_proj_nav_none(float3  p_view_2, float4  intrins_2, FixedArray<float, 1>  dist_coeffs_1, float2  * uv_3)
{
    bool _S47;
    for(;;)
    {
        float2  _S48 = float2 {p_view_2.x, p_view_2.y};
        float _S49 = p_view_2.z;
        float2  uv0_0 = _S48 / make_float2 (_S49);
        bool _S50 = _S49 < 0.0f;
        if(_S50)
        {
            *uv_3 = uv0_0;
            _S47 = false;
            break;
        }
        float2  uv_4 = _S48 / make_float2 (_S49);
        FixedArray<float, 1>  _S51 = dist_coeffs_1;
        float2  _S52 = DistNone_distort_0(uv_4, &_S51);
        *uv_3 = make_float2 (intrins_2.x * _S52.x + intrins_2.z, intrins_2.y * _S52.y + intrins_2.w);
        _S47 = true;
        break;
    }
    return _S47;
}

inline __device__ bool fisheye_proj_nav_none(float3  p_view_3, float4  intrins_3, FixedArray<float, 1>  dist_coeffs_2, float2  * uv_5)
{
    float2  _S53 = float2 {p_view_3.x, p_view_3.y};
    float r_3 = length_0(_S53);
    float _S54 = p_view_3.z;
    float theta_0 = (F32_atan2((r_3), (_S54)));
    float k_0;
    if(theta_0 < 0.00100000004749745f)
    {
        k_0 = (1.0f - theta_0 * theta_0 / 3.0f) / _S54;
    }
    else
    {
        k_0 = theta_0 / r_3;
    }
    float2  _S55 = _S53 * make_float2 (k_0);
    FixedArray<float, 1>  _S56 = dist_coeffs_2;
    float2  _S57 = DistNone_distort_0(_S55, &_S56);
    *uv_5 = make_float2 (intrins_3.x * _S57.x + intrins_3.z, intrins_3.y * _S57.y + intrins_3.w);
    return true;
}

inline __device__ DiffPair_float_0 _d_sin_0(DiffPair_float_0 * dpx_6)
{
    DiffPair_float_0 _S58 = { (F32_sin((dpx_6->primal_0))), (F32_cos((dpx_6->primal_0))) * dpx_6->differential_0 };
    return _S58;
}

inline __device__ bool equisolid_proj_nav_none(float3  p_view_4, float4  intrins_4, FixedArray<float, 1>  dist_coeffs_3, float2  * uv_6)
{
    float2  _S59 = float2 {p_view_4.x, p_view_4.y};
    float r_4 = length_0(_S59);
    float _S60 = p_view_4.z;
    float theta_1 = (F32_atan2((r_4), (_S60)));
    float k_1;
    if(r_4 < 9.99999997475242708e-07f)
    {
        k_1 = (1.0f - theta_1 * theta_1 / 24.0f) / _S60;
    }
    else
    {
        k_1 = 2.0f * (F32_sin((0.5f * theta_1))) / r_4;
    }
    float2  _S61 = _S59 * make_float2 (k_1);
    FixedArray<float, 1>  _S62 = dist_coeffs_3;
    float2  _S63 = DistNone_distort_0(_S61, &_S62);
    *uv_6 = make_float2 (intrins_4.x * _S63.x + intrins_4.z, intrins_4.y * _S63.y + intrins_4.w);
    return true;
}

inline __device__ DiffPair_vectorx3Cfloatx2C2x3E_0 s_fwd_DistNone_distort_0(DiffPair_vectorx3Cfloatx2C2x3E_0 * dpuv_0, FixedArray<float, 1>  * coeffs_1)
{
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S64 = { dpuv_0->primal_0, dpuv_0->differential_0 };
    return _S64;
}

inline __device__ Matrix<float, 2, 3>  persp_proj_jac_none(float3  p_view_5, float4  intrins_5, FixedArray<float, 1>  dist_coeffs_4)
{
    float2  _S65 = float2 {p_view_5.x, p_view_5.y};
    float _S66 = p_view_5.z;
    float2  _S67 = _S65 * make_float2 (0.0f);
    float _S68 = _S66 * _S66;
    float2  s_diff_uv_0 = (make_float2 (1.0f, 0.0f) * make_float2 (_S66) - _S67) / make_float2 (_S68);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S69;
    (&_S69)->primal_0 = _S65 / make_float2 (_S66);
    (&_S69)->differential_0 = s_diff_uv_0;
    FixedArray<float, 1>  _S70 = dist_coeffs_4;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S71 = s_fwd_DistNone_distort_0(&_S69, &_S70);
    float fx_1 = intrins_5.x;
    float fy_1 = intrins_5.y;
    float _S72 = _S71.differential_0.y * fy_1;
    Matrix<float, 2, 3>  J_1;
    *&(((&J_1)->rows + (int(0)))->x) = _S71.differential_0.x * fx_1;
    *&(((&J_1)->rows + (int(1)))->x) = _S72;
    float2  s_diff_uv_1 = (make_float2 (0.0f, 1.0f) * make_float2 (_S66) - _S67) / make_float2 (_S68);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S73;
    (&_S73)->primal_0 = _S65 / make_float2 (_S66);
    (&_S73)->differential_0 = s_diff_uv_1;
    FixedArray<float, 1>  _S74 = dist_coeffs_4;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S75 = s_fwd_DistNone_distort_0(&_S73, &_S74);
    float _S76 = _S75.differential_0.y * fy_1;
    *&(((&J_1)->rows + (int(0)))->y) = _S75.differential_0.x * fx_1;
    *&(((&J_1)->rows + (int(1)))->y) = _S76;
    float2  s_diff_uv_2 = (make_float2 (0.0f, 0.0f) * make_float2 (_S66) - _S65) / make_float2 (_S68);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S77;
    (&_S77)->primal_0 = _S65 / make_float2 (_S66);
    (&_S77)->differential_0 = s_diff_uv_2;
    FixedArray<float, 1>  _S78 = dist_coeffs_4;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S79 = s_fwd_DistNone_distort_0(&_S77, &_S78);
    float _S80 = _S79.differential_0.y * fy_1;
    *&(((&J_1)->rows + (int(0)))->z) = _S79.differential_0.x * fx_1;
    *&(((&J_1)->rows + (int(1)))->z) = _S80;
    return J_1;
}

inline __device__ Matrix<float, 2, 3>  fisheye_proj_jac_none(float3  p_view_6, float4  intrins_6, FixedArray<float, 1>  dist_coeffs_5)
{
    Matrix<float, 2, 3>  J_2;
    float2  _S81 = float2 {p_view_6.x, p_view_6.y};
    float2  _S82 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S83;
    (&_S83)->primal_0 = _S81;
    (&_S83)->differential_0 = _S82;
    DiffPair_float_0 _S84 = s_fwd_length_impl_0(&_S83);
    float _S85 = p_view_6.z;
    DiffPair_float_0 _S86;
    (&_S86)->primal_0 = _S84.primal_0;
    (&_S86)->differential_0 = _S84.differential_0;
    DiffPair_float_0 _S87;
    (&_S87)->primal_0 = _S85;
    (&_S87)->differential_0 = 0.0f;
    DiffPair_float_0 _S88 = _d_atan2_0(&_S86, &_S87);
    float k_2;
    float s_diff_k_0;
    if((_S88.primal_0) < 0.00100000004749745f)
    {
        float _S89 = _S88.differential_0 * _S88.primal_0;
        float _S90 = 1.0f - _S88.primal_0 * _S88.primal_0 / 3.0f;
        float _S91 = ((0.0f - (_S89 + _S89) * 0.3333333432674408f) * _S85 - _S90 * 0.0f) / (_S85 * _S85);
        k_2 = _S90 / _S85;
        s_diff_k_0 = _S91;
    }
    else
    {
        float _S92 = (_S88.differential_0 * _S84.primal_0 - _S88.primal_0 * _S84.differential_0) / (_S84.primal_0 * _S84.primal_0);
        k_2 = _S88.primal_0 / _S84.primal_0;
        s_diff_k_0 = _S92;
    }
    float2  _S93 = _S81 * make_float2 (k_2);
    float2  _S94 = _S82 * make_float2 (k_2) + make_float2 (s_diff_k_0) * _S81;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S95;
    (&_S95)->primal_0 = _S93;
    (&_S95)->differential_0 = _S94;
    FixedArray<float, 1>  _S96 = dist_coeffs_5;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S97 = s_fwd_DistNone_distort_0(&_S95, &_S96);
    float fx_2 = intrins_6.x;
    float fy_2 = intrins_6.y;
    float _S98 = _S97.differential_0.y * fy_2;
    *&(((&J_2)->rows + (int(0)))->x) = _S97.differential_0.x * fx_2;
    *&(((&J_2)->rows + (int(1)))->x) = _S98;
    float2  _S99 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S100;
    (&_S100)->primal_0 = _S81;
    (&_S100)->differential_0 = _S99;
    DiffPair_float_0 _S101 = s_fwd_length_impl_0(&_S100);
    DiffPair_float_0 _S102;
    (&_S102)->primal_0 = _S101.primal_0;
    (&_S102)->differential_0 = _S101.differential_0;
    DiffPair_float_0 _S103;
    (&_S103)->primal_0 = _S85;
    (&_S103)->differential_0 = 0.0f;
    DiffPair_float_0 _S104 = _d_atan2_0(&_S102, &_S103);
    if((_S104.primal_0) < 0.00100000004749745f)
    {
        float _S105 = _S104.differential_0 * _S104.primal_0;
        float _S106 = 1.0f - _S104.primal_0 * _S104.primal_0 / 3.0f;
        float _S107 = ((0.0f - (_S105 + _S105) * 0.3333333432674408f) * _S85 - _S106 * 0.0f) / (_S85 * _S85);
        k_2 = _S106 / _S85;
        s_diff_k_0 = _S107;
    }
    else
    {
        float _S108 = (_S104.differential_0 * _S101.primal_0 - _S104.primal_0 * _S101.differential_0) / (_S101.primal_0 * _S101.primal_0);
        k_2 = _S104.primal_0 / _S101.primal_0;
        s_diff_k_0 = _S108;
    }
    float2  _S109 = _S81 * make_float2 (k_2);
    float2  _S110 = _S99 * make_float2 (k_2) + make_float2 (s_diff_k_0) * _S81;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S111;
    (&_S111)->primal_0 = _S109;
    (&_S111)->differential_0 = _S110;
    FixedArray<float, 1>  _S112 = dist_coeffs_5;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S113 = s_fwd_DistNone_distort_0(&_S111, &_S112);
    float _S114 = _S113.differential_0.y * fy_2;
    *&(((&J_2)->rows + (int(0)))->y) = _S113.differential_0.x * fx_2;
    *&(((&J_2)->rows + (int(1)))->y) = _S114;
    float2  _S115 = make_float2 (0.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S116;
    (&_S116)->primal_0 = _S81;
    (&_S116)->differential_0 = _S115;
    DiffPair_float_0 _S117 = s_fwd_length_impl_0(&_S116);
    DiffPair_float_0 _S118;
    (&_S118)->primal_0 = _S117.primal_0;
    (&_S118)->differential_0 = _S117.differential_0;
    DiffPair_float_0 _S119;
    (&_S119)->primal_0 = _S85;
    (&_S119)->differential_0 = 1.0f;
    DiffPair_float_0 _S120 = _d_atan2_0(&_S118, &_S119);
    if((_S120.primal_0) < 0.00100000004749745f)
    {
        float _S121 = _S120.differential_0 * _S120.primal_0;
        float _S122 = 1.0f - _S120.primal_0 * _S120.primal_0 / 3.0f;
        float _S123 = ((0.0f - (_S121 + _S121) * 0.3333333432674408f) * _S85 - _S122) / (_S85 * _S85);
        k_2 = _S122 / _S85;
        s_diff_k_0 = _S123;
    }
    else
    {
        float _S124 = (_S120.differential_0 * _S117.primal_0 - _S120.primal_0 * _S117.differential_0) / (_S117.primal_0 * _S117.primal_0);
        k_2 = _S120.primal_0 / _S117.primal_0;
        s_diff_k_0 = _S124;
    }
    float2  _S125 = _S81 * make_float2 (k_2);
    float2  _S126 = _S115 * make_float2 (k_2) + make_float2 (s_diff_k_0) * _S81;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S127;
    (&_S127)->primal_0 = _S125;
    (&_S127)->differential_0 = _S126;
    FixedArray<float, 1>  _S128 = dist_coeffs_5;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S129 = s_fwd_DistNone_distort_0(&_S127, &_S128);
    float _S130 = _S129.differential_0.y * fy_2;
    *&(((&J_2)->rows + (int(0)))->z) = _S129.differential_0.x * fx_2;
    *&(((&J_2)->rows + (int(1)))->z) = _S130;
    return J_2;
}

inline __device__ Matrix<float, 2, 3>  equisolid_proj_jac_none(float3  p_view_7, float4  intrins_7, FixedArray<float, 1>  dist_coeffs_6)
{
    Matrix<float, 2, 3>  J_3;
    float2  _S131 = float2 {p_view_7.x, p_view_7.y};
    float2  _S132 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S133;
    (&_S133)->primal_0 = _S131;
    (&_S133)->differential_0 = _S132;
    DiffPair_float_0 _S134 = s_fwd_length_impl_0(&_S133);
    float _S135 = p_view_7.z;
    DiffPair_float_0 _S136;
    (&_S136)->primal_0 = _S134.primal_0;
    (&_S136)->differential_0 = _S134.differential_0;
    DiffPair_float_0 _S137;
    (&_S137)->primal_0 = _S135;
    (&_S137)->differential_0 = 0.0f;
    DiffPair_float_0 _S138 = _d_atan2_0(&_S136, &_S137);
    float k_3;
    float s_diff_k_1;
    if((_S134.primal_0) < 9.99999997475242708e-07f)
    {
        float _S139 = _S138.differential_0 * _S138.primal_0;
        float _S140 = 1.0f - _S138.primal_0 * _S138.primal_0 / 24.0f;
        float _S141 = ((0.0f - (_S139 + _S139) * 0.0416666679084301f) * _S135 - _S140 * 0.0f) / (_S135 * _S135);
        k_3 = _S140 / _S135;
        s_diff_k_1 = _S141;
    }
    else
    {
        float _S142 = _S138.differential_0 * 0.5f;
        DiffPair_float_0 _S143;
        (&_S143)->primal_0 = 0.5f * _S138.primal_0;
        (&_S143)->differential_0 = _S142;
        DiffPair_float_0 _S144 = _d_sin_0(&_S143);
        float _S145 = 2.0f * _S144.primal_0;
        float _S146 = (_S144.differential_0 * 2.0f * _S134.primal_0 - _S145 * _S134.differential_0) / (_S134.primal_0 * _S134.primal_0);
        k_3 = _S145 / _S134.primal_0;
        s_diff_k_1 = _S146;
    }
    float2  _S147 = _S131 * make_float2 (k_3);
    float2  _S148 = _S132 * make_float2 (k_3) + make_float2 (s_diff_k_1) * _S131;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S149;
    (&_S149)->primal_0 = _S147;
    (&_S149)->differential_0 = _S148;
    FixedArray<float, 1>  _S150 = dist_coeffs_6;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S151 = s_fwd_DistNone_distort_0(&_S149, &_S150);
    float fx_3 = intrins_7.x;
    float fy_3 = intrins_7.y;
    float _S152 = _S151.differential_0.y * fy_3;
    *&(((&J_3)->rows + (int(0)))->x) = _S151.differential_0.x * fx_3;
    *&(((&J_3)->rows + (int(1)))->x) = _S152;
    float2  _S153 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S154;
    (&_S154)->primal_0 = _S131;
    (&_S154)->differential_0 = _S153;
    DiffPair_float_0 _S155 = s_fwd_length_impl_0(&_S154);
    DiffPair_float_0 _S156;
    (&_S156)->primal_0 = _S155.primal_0;
    (&_S156)->differential_0 = _S155.differential_0;
    DiffPair_float_0 _S157;
    (&_S157)->primal_0 = _S135;
    (&_S157)->differential_0 = 0.0f;
    DiffPair_float_0 _S158 = _d_atan2_0(&_S156, &_S157);
    if((_S155.primal_0) < 9.99999997475242708e-07f)
    {
        float _S159 = _S158.differential_0 * _S158.primal_0;
        float _S160 = 1.0f - _S158.primal_0 * _S158.primal_0 / 24.0f;
        float _S161 = ((0.0f - (_S159 + _S159) * 0.0416666679084301f) * _S135 - _S160 * 0.0f) / (_S135 * _S135);
        k_3 = _S160 / _S135;
        s_diff_k_1 = _S161;
    }
    else
    {
        float _S162 = _S158.differential_0 * 0.5f;
        DiffPair_float_0 _S163;
        (&_S163)->primal_0 = 0.5f * _S158.primal_0;
        (&_S163)->differential_0 = _S162;
        DiffPair_float_0 _S164 = _d_sin_0(&_S163);
        float _S165 = 2.0f * _S164.primal_0;
        float _S166 = (_S164.differential_0 * 2.0f * _S155.primal_0 - _S165 * _S155.differential_0) / (_S155.primal_0 * _S155.primal_0);
        k_3 = _S165 / _S155.primal_0;
        s_diff_k_1 = _S166;
    }
    float2  _S167 = _S131 * make_float2 (k_3);
    float2  _S168 = _S153 * make_float2 (k_3) + make_float2 (s_diff_k_1) * _S131;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S169;
    (&_S169)->primal_0 = _S167;
    (&_S169)->differential_0 = _S168;
    FixedArray<float, 1>  _S170 = dist_coeffs_6;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S171 = s_fwd_DistNone_distort_0(&_S169, &_S170);
    float _S172 = _S171.differential_0.y * fy_3;
    *&(((&J_3)->rows + (int(0)))->y) = _S171.differential_0.x * fx_3;
    *&(((&J_3)->rows + (int(1)))->y) = _S172;
    float2  _S173 = make_float2 (0.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S174;
    (&_S174)->primal_0 = _S131;
    (&_S174)->differential_0 = _S173;
    DiffPair_float_0 _S175 = s_fwd_length_impl_0(&_S174);
    DiffPair_float_0 _S176;
    (&_S176)->primal_0 = _S175.primal_0;
    (&_S176)->differential_0 = _S175.differential_0;
    DiffPair_float_0 _S177;
    (&_S177)->primal_0 = _S135;
    (&_S177)->differential_0 = 1.0f;
    DiffPair_float_0 _S178 = _d_atan2_0(&_S176, &_S177);
    if((_S175.primal_0) < 9.99999997475242708e-07f)
    {
        float _S179 = _S178.differential_0 * _S178.primal_0;
        float _S180 = 1.0f - _S178.primal_0 * _S178.primal_0 / 24.0f;
        float _S181 = ((0.0f - (_S179 + _S179) * 0.0416666679084301f) * _S135 - _S180) / (_S135 * _S135);
        k_3 = _S180 / _S135;
        s_diff_k_1 = _S181;
    }
    else
    {
        float _S182 = _S178.differential_0 * 0.5f;
        DiffPair_float_0 _S183;
        (&_S183)->primal_0 = 0.5f * _S178.primal_0;
        (&_S183)->differential_0 = _S182;
        DiffPair_float_0 _S184 = _d_sin_0(&_S183);
        float _S185 = 2.0f * _S184.primal_0;
        float _S186 = (_S184.differential_0 * 2.0f * _S175.primal_0 - _S185 * _S175.differential_0) / (_S175.primal_0 * _S175.primal_0);
        k_3 = _S185 / _S175.primal_0;
        s_diff_k_1 = _S186;
    }
    float2  _S187 = _S131 * make_float2 (k_3);
    float2  _S188 = _S173 * make_float2 (k_3) + make_float2 (s_diff_k_1) * _S131;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S189;
    (&_S189)->primal_0 = _S187;
    (&_S189)->differential_0 = _S188;
    FixedArray<float, 1>  _S190 = dist_coeffs_6;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S191 = s_fwd_DistNone_distort_0(&_S189, &_S190);
    float _S192 = _S191.differential_0.y * fy_3;
    *&(((&J_3)->rows + (int(0)))->z) = _S191.differential_0.x * fx_3;
    *&(((&J_3)->rows + (int(1)))->z) = _S192;
    return J_3;
}

inline __device__ float2  distort_point_none(float2  uv_7, int camera_model_0, FixedArray<float, 1>  dist_coeffs_7)
{
    float2  _S193;
    for(;;)
    {
        if(camera_model_0 == int(3))
        {
            _S193 = uv_7;
            break;
        }
        float k_4;
        if(camera_model_0 == int(1))
        {
            float r_5 = length_0(uv_7);
            float theta_2 = (F32_atan((r_5)));
            if(r_5 < 0.00100000004749745f)
            {
                k_4 = 1.0f - theta_2 * theta_2 / 6.0f;
            }
            else
            {
                k_4 = theta_2 / r_5;
            }
            _S193 = uv_7 * make_float2 (k_4);
        }
        else
        {
            if(camera_model_0 == int(2))
            {
                float r_6 = length_0(uv_7);
                float theta_3 = (F32_atan((r_6)));
                if(r_6 < 0.00100000004749745f)
                {
                    k_4 = 1.0f - theta_3 * theta_3 / 24.0f;
                }
                else
                {
                    k_4 = 2.0f * (F32_sin((0.5f * theta_3))) / r_6;
                }
                _S193 = uv_7 * make_float2 (k_4);
            }
            else
            {
                _S193 = uv_7;
            }
        }
        FixedArray<float, 1>  _S194 = dist_coeffs_7;
        float2  _S195 = DistNone_distort_0(_S193, &_S194);
        _S193 = _S195;
        break;
    }
    return _S193;
}

inline __device__ bool undistort_point_0(float2  uv_8, FixedArray<float, 1>  * dist_coeffs_8, int maxiter_0, float2  * uv_undist_0)
{
    *uv_undist_0 = uv_8;
    return true;
}

inline __device__ float2  DistOpenCV_distort_0(float2  uv_9, FixedArray<float, 4>  * coeffs_2)
{
    float u_0 = uv_9.x;
    float v_0 = uv_9.y;
    float r2_0 = u_0 * u_0 + v_0 * v_0;
    return uv_9 * make_float2 (1.0f + r2_0 * ((*coeffs_2)[int(0)] + r2_0 * (*coeffs_2)[int(1)])) + make_float2 (2.0f * (*coeffs_2)[int(2)] * u_0 * v_0 + (*coeffs_2)[int(3)] * (r2_0 + 2.0f * u_0 * u_0), 2.0f * (*coeffs_2)[int(3)] * u_0 * v_0 + (*coeffs_2)[int(2)] * (r2_0 + 2.0f * v_0 * v_0));
}

inline __device__ DiffPair_vectorx3Cfloatx2C2x3E_0 s_fwd_DistOpenCV_distort_0(DiffPair_vectorx3Cfloatx2C2x3E_0 * dpuv_1, FixedArray<float, 4>  * coeffs_3)
{
    float u_1 = dpuv_1->primal_0.x;
    float s_diff_u_0 = dpuv_1->differential_0.x;
    float v_1 = dpuv_1->primal_0.y;
    float s_diff_v_0 = dpuv_1->differential_0.y;
    float _S196 = s_diff_u_0 * u_1;
    float _S197 = s_diff_v_0 * v_1;
    float r2_1 = u_1 * u_1 + v_1 * v_1;
    float s_diff_r2_0 = _S196 + _S196 + (_S197 + _S197);
    float _S198 = (*coeffs_3)[int(0)] + r2_1 * (*coeffs_3)[int(1)];
    float radial_0 = 1.0f + r2_1 * _S198;
    float _S199 = 2.0f * (*coeffs_3)[int(2)];
    float _S200 = _S199 * u_1;
    float _S201 = 2.0f * u_1;
    float _S202 = 2.0f * (*coeffs_3)[int(3)];
    float _S203 = _S202 * u_1;
    float _S204 = 2.0f * v_1;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S205 = { dpuv_1->primal_0 * make_float2 (radial_0) + make_float2 (_S200 * v_1 + (*coeffs_3)[int(3)] * (r2_1 + _S201 * u_1), _S203 * v_1 + (*coeffs_3)[int(2)] * (r2_1 + _S204 * v_1)), dpuv_1->differential_0 * make_float2 (radial_0) + make_float2 (s_diff_r2_0 * _S198 + s_diff_r2_0 * (*coeffs_3)[int(1)] * r2_1) * dpuv_1->primal_0 + make_float2 (s_diff_u_0 * _S199 * v_1 + s_diff_v_0 * _S200 + (s_diff_r2_0 + (s_diff_u_0 * 2.0f * u_1 + s_diff_u_0 * _S201)) * (*coeffs_3)[int(3)], s_diff_u_0 * _S202 * v_1 + s_diff_v_0 * _S203 + (s_diff_r2_0 + (s_diff_v_0 * 2.0f * v_1 + s_diff_v_0 * _S204)) * (*coeffs_3)[int(2)]) };
    return _S205;
}

inline __device__ bool undistort_point_1(float2  uv_10, FixedArray<float, 4>  * dist_coeffs_9, int maxiter_1, float2  * uv_undist_1)
{
    int i_4 = int(0);
    float2  q_0 = uv_10;
    for(;;)
    {
        if(i_4 < maxiter_1)
        {
        }
        else
        {
            break;
        }
        float2  _S206 = DistOpenCV_distort_0(q_0, dist_coeffs_9);
        float2  r_7 = _S206 - uv_10;
        float2  _S207 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S208;
        (&_S208)->primal_0 = q_0;
        (&_S208)->differential_0 = _S207;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S209 = s_fwd_DistOpenCV_distort_0(&_S208, dist_coeffs_9);
        float2  _S210 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S211;
        (&_S211)->primal_0 = q_0;
        (&_S211)->differential_0 = _S210;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S212 = s_fwd_DistOpenCV_distort_0(&_S211, dist_coeffs_9);
        Matrix<float, 2, 2>  _S213 = transpose_1(makeMatrix<float, 2, 2> (_S209.differential_0, _S212.differential_0));
        float inv_det_0 = 1.0f / (_S213.rows[int(0)].x * _S213.rows[int(1)].y - _S213.rows[int(0)].y * _S213.rows[int(1)].x);
        float _S214 = r_7.x;
        float _S215 = r_7.y;
        float2  q_1 = q_0 - make_float2 ((_S214 * _S213.rows[int(1)].y - _S215 * _S213.rows[int(0)].y) * inv_det_0, (- _S214 * _S213.rows[int(1)].x + _S215 * _S213.rows[int(0)].x) * inv_det_0);
        i_4 = i_4 + int(1);
        q_0 = q_1;
    }
    *uv_undist_1 = q_0;
    float2  _S216 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S217;
    (&_S217)->primal_0 = q_0;
    (&_S217)->differential_0 = _S216;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S218 = s_fwd_DistOpenCV_distort_0(&_S217, dist_coeffs_9);
    float2  _S219 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S220;
    (&_S220)->primal_0 = q_0;
    (&_S220)->differential_0 = _S219;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S221 = s_fwd_DistOpenCV_distort_0(&_S220, dist_coeffs_9);
    Matrix<float, 2, 2>  _S222 = transpose_1(makeMatrix<float, 2, 2> (_S218.differential_0, _S221.differential_0));
    float _S223 = (F32_min((determinant_0(_S222)), ((F32_min((_S222.rows[int(0)].x), (_S222.rows[int(1)].y))))));
    bool _S224;
    if(_S223 > 0.25f)
    {
        _S224 = _S223 < 4.0f;
    }
    else
    {
        _S224 = false;
    }
    if(_S224)
    {
        float2  _S225 = DistOpenCV_distort_0(q_0, dist_coeffs_9);
        _S224 = (dot_0(q_0, _S225)) >= 0.0f;
    }
    else
    {
        _S224 = false;
    }
    if(_S224)
    {
        float2  _S226 = DistOpenCV_distort_0(*uv_undist_1, dist_coeffs_9);
        _S224 = (length_0(_S226 - uv_10)) < 0.00999999977648258f;
    }
    else
    {
        _S224 = false;
    }
    return _S224;
}

inline __device__ float2  DistThinPrism_distort_0(float2  uv_11, FixedArray<float, 8>  * coeffs_4)
{
    float u_2 = uv_11.x;
    float v_2 = uv_11.y;
    float r2_2 = u_2 * u_2 + v_2 * v_2;
    return uv_11 * make_float2 (1.0f + r2_2 * ((*coeffs_4)[int(0)] + r2_2 * ((*coeffs_4)[int(1)] + r2_2 * ((*coeffs_4)[int(2)] + r2_2 * (*coeffs_4)[int(3)])))) + make_float2 (2.0f * (*coeffs_4)[int(4)] * u_2 * v_2 + (*coeffs_4)[int(5)] * (r2_2 + 2.0f * u_2 * u_2) + (*coeffs_4)[int(6)] * r2_2, 2.0f * (*coeffs_4)[int(5)] * u_2 * v_2 + (*coeffs_4)[int(4)] * (r2_2 + 2.0f * v_2 * v_2) + (*coeffs_4)[int(7)] * r2_2);
}

inline __device__ DiffPair_vectorx3Cfloatx2C2x3E_0 s_fwd_DistThinPrism_distort_0(DiffPair_vectorx3Cfloatx2C2x3E_0 * dpuv_2, FixedArray<float, 8>  * coeffs_5)
{
    float u_3 = dpuv_2->primal_0.x;
    float s_diff_u_1 = dpuv_2->differential_0.x;
    float v_3 = dpuv_2->primal_0.y;
    float s_diff_v_1 = dpuv_2->differential_0.y;
    float _S227 = s_diff_u_1 * u_3;
    float _S228 = s_diff_v_1 * v_3;
    float r2_3 = u_3 * u_3 + v_3 * v_3;
    float s_diff_r2_1 = _S227 + _S227 + (_S228 + _S228);
    float _S229 = (*coeffs_5)[int(2)] + r2_3 * (*coeffs_5)[int(3)];
    float _S230 = (*coeffs_5)[int(1)] + r2_3 * _S229;
    float _S231 = (*coeffs_5)[int(0)] + r2_3 * _S230;
    float radial_1 = 1.0f + r2_3 * _S231;
    float _S232 = 2.0f * (*coeffs_5)[int(4)];
    float _S233 = _S232 * u_3;
    float _S234 = 2.0f * u_3;
    float _S235 = 2.0f * (*coeffs_5)[int(5)];
    float _S236 = _S235 * u_3;
    float _S237 = 2.0f * v_3;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S238 = { dpuv_2->primal_0 * make_float2 (radial_1) + make_float2 (_S233 * v_3 + (*coeffs_5)[int(5)] * (r2_3 + _S234 * u_3) + (*coeffs_5)[int(6)] * r2_3, _S236 * v_3 + (*coeffs_5)[int(4)] * (r2_3 + _S237 * v_3) + (*coeffs_5)[int(7)] * r2_3), dpuv_2->differential_0 * make_float2 (radial_1) + make_float2 (s_diff_r2_1 * _S231 + (s_diff_r2_1 * _S230 + (s_diff_r2_1 * _S229 + s_diff_r2_1 * (*coeffs_5)[int(3)] * r2_3) * r2_3) * r2_3) * dpuv_2->primal_0 + make_float2 (s_diff_u_1 * _S232 * v_3 + s_diff_v_1 * _S233 + (s_diff_r2_1 + (s_diff_u_1 * 2.0f * u_3 + s_diff_u_1 * _S234)) * (*coeffs_5)[int(5)] + s_diff_r2_1 * (*coeffs_5)[int(6)], s_diff_u_1 * _S235 * v_3 + s_diff_v_1 * _S236 + (s_diff_r2_1 + (s_diff_v_1 * 2.0f * v_3 + s_diff_v_1 * _S237)) * (*coeffs_5)[int(4)] + s_diff_r2_1 * (*coeffs_5)[int(7)]) };
    return _S238;
}

inline __device__ bool undistort_point_2(float2  uv_12, FixedArray<float, 8>  * dist_coeffs_10, int maxiter_2, float2  * uv_undist_2)
{
    int i_5 = int(0);
    float2  q_2 = uv_12;
    for(;;)
    {
        if(i_5 < maxiter_2)
        {
        }
        else
        {
            break;
        }
        float2  _S239 = DistThinPrism_distort_0(q_2, dist_coeffs_10);
        float2  r_8 = _S239 - uv_12;
        float2  _S240 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S241;
        (&_S241)->primal_0 = q_2;
        (&_S241)->differential_0 = _S240;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S242 = s_fwd_DistThinPrism_distort_0(&_S241, dist_coeffs_10);
        float2  _S243 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S244;
        (&_S244)->primal_0 = q_2;
        (&_S244)->differential_0 = _S243;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S245 = s_fwd_DistThinPrism_distort_0(&_S244, dist_coeffs_10);
        Matrix<float, 2, 2>  _S246 = transpose_1(makeMatrix<float, 2, 2> (_S242.differential_0, _S245.differential_0));
        float inv_det_1 = 1.0f / (_S246.rows[int(0)].x * _S246.rows[int(1)].y - _S246.rows[int(0)].y * _S246.rows[int(1)].x);
        float _S247 = r_8.x;
        float _S248 = r_8.y;
        float2  q_3 = q_2 - make_float2 ((_S247 * _S246.rows[int(1)].y - _S248 * _S246.rows[int(0)].y) * inv_det_1, (- _S247 * _S246.rows[int(1)].x + _S248 * _S246.rows[int(0)].x) * inv_det_1);
        i_5 = i_5 + int(1);
        q_2 = q_3;
    }
    *uv_undist_2 = q_2;
    float2  _S249 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S250;
    (&_S250)->primal_0 = q_2;
    (&_S250)->differential_0 = _S249;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S251 = s_fwd_DistThinPrism_distort_0(&_S250, dist_coeffs_10);
    float2  _S252 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S253;
    (&_S253)->primal_0 = q_2;
    (&_S253)->differential_0 = _S252;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S254 = s_fwd_DistThinPrism_distort_0(&_S253, dist_coeffs_10);
    Matrix<float, 2, 2>  _S255 = transpose_1(makeMatrix<float, 2, 2> (_S251.differential_0, _S254.differential_0));
    float _S256 = (F32_min((determinant_0(_S255)), ((F32_min((_S255.rows[int(0)].x), (_S255.rows[int(1)].y))))));
    bool _S257;
    if(_S256 > 0.25f)
    {
        _S257 = _S256 < 4.0f;
    }
    else
    {
        _S257 = false;
    }
    if(_S257)
    {
        float2  _S258 = DistThinPrism_distort_0(q_2, dist_coeffs_10);
        _S257 = (dot_0(q_2, _S258)) >= 0.0f;
    }
    else
    {
        _S257 = false;
    }
    if(_S257)
    {
        float2  _S259 = DistThinPrism_distort_0(*uv_undist_2, dist_coeffs_10);
        _S257 = (length_0(_S259 - uv_12)) < 0.00999999977648258f;
    }
    else
    {
        _S257 = false;
    }
    return _S257;
}

inline __device__ bool undistort_point_none(float2  uv_13, int camera_model_1, FixedArray<float, 1>  dist_coeffs_11, float2  * uv_undist_3)
{
    bool _S260;
    for(;;)
    {
        *uv_undist_3 = make_float2 (0.0f);
        if(camera_model_1 == int(3))
        {
            float lon_0 = uv_13.x;
            float lat_0 = uv_13.y;
            float cl_0 = (F32_cos((lat_0)));
            *uv_undist_3 = make_float2 (cl_0 * (F32_sin((lon_0))), (F32_sin((lat_0)))) / make_float2 ((F32_max((cl_0 * (F32_cos((lon_0)))), (9.999999960041972e-13f))));
            _S260 = true;
            break;
        }
        FixedArray<float, 1>  _S261 = dist_coeffs_11;
        float2  uv_u_0;
        bool _S262 = undistort_point_0(uv_13, &_S261, int(8), &uv_u_0);
        if(!_S262)
        {
            _S260 = false;
            break;
        }
        float2  _S263 = uv_u_0;
        float3  raydir_0;
        if(camera_model_1 == int(1))
        {
            float r_9 = length_0(_S263);
            float s_0;
            if(r_9 < 0.00100000004749745f)
            {
                s_0 = 1.0f - r_9 * r_9 / 6.0f;
            }
            else
            {
                s_0 = (F32_sin((r_9))) / r_9;
            }
            raydir_0 = make_float3 ((_S263 * make_float2 (s_0)).x, (_S263 * make_float2 (s_0)).y, (F32_cos((r_9))));
        }
        else
        {
            if(camera_model_1 == int(2))
            {
                float r_10 = length_0(_S263);
                raydir_0 = make_float3 ((_S263 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_10 * r_10)))))))).x, (_S263 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_10 * r_10)))))))).y, 1.0f - 0.5f * r_10 * r_10);
            }
            else
            {
                raydir_0 = make_float3 (_S263.x, _S263.y, 1.0f);
            }
        }
        *uv_undist_3 = float2 {raydir_0.x, raydir_0.y} / make_float2 ((F32_max((raydir_0.z), (9.999999960041972e-13f))));
        _S260 = true;
        break;
    }
    return _S260;
}

inline __device__ bool unproject_point_none(float2  uv_14, int camera_model_2, FixedArray<float, 1>  dist_coeffs_12, float3  * raydir_1)
{
    bool _S264;
    for(;;)
    {
        if(camera_model_2 == int(4))
        {
            *raydir_1 = make_float3 (0.0f, 0.0f, 1.0f);
            _S264 = true;
            break;
        }
        int3  _S265 = make_int3 (int(0));
        float3  _S266 = make_float3 ((float)_S265.x, (float)_S265.y, (float)_S265.z);
        *raydir_1 = _S266;
        if(camera_model_2 == int(3))
        {
            float lon_1 = uv_14.x;
            float lat_1 = uv_14.y;
            float cl_1 = (F32_cos((lat_1)));
            *raydir_1 = make_float3 (cl_1 * (F32_sin((lon_1))), (F32_sin((lat_1))), cl_1 * (F32_cos((lon_1))));
            _S264 = true;
            break;
        }
        FixedArray<float, 1>  _S267 = dist_coeffs_12;
        float2  uv_u_1;
        bool _S268 = undistort_point_0(uv_14, &_S267, int(8), &uv_u_1);
        if(!_S268)
        {
            _S264 = false;
            break;
        }
        float2  _S269 = uv_u_1;
        if(camera_model_2 == int(1))
        {
            float r_11 = length_0(_S269);
            float s_1;
            if(r_11 < 0.00100000004749745f)
            {
                s_1 = 1.0f - r_11 * r_11 / 6.0f;
            }
            else
            {
                s_1 = (F32_sin((r_11))) / r_11;
            }
            *raydir_1 = make_float3 ((_S269 * make_float2 (s_1)).x, (_S269 * make_float2 (s_1)).y, (F32_cos((r_11))));
        }
        else
        {
            if(camera_model_2 == int(2))
            {
                float r_12 = length_0(_S269);
                *raydir_1 = make_float3 ((_S269 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_12 * r_12)))))))).x, (_S269 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_12 * r_12)))))))).y, 1.0f - 0.5f * r_12 * r_12);
            }
            else
            {
                *raydir_1 = make_float3 (_S269.x, _S269.y, 1.0f);
            }
        }
        _S264 = true;
        break;
    }
    return _S264;
}

inline __device__ float3  normalize_0(float3  x_11)
{
    return x_11 / make_float3 (length_1(x_11));
}

inline __device__ bool generate_ray_none(float2  uv_15, int camera_model_3, FixedArray<float, 1>  dist_coeffs_13, float3  * raydir_2)
{
    bool _S270;
    for(;;)
    {
        if(camera_model_3 == int(4))
        {
            *raydir_2 = make_float3 (0.0f, 0.0f, 1.0f);
            _S270 = true;
            break;
        }
        if(camera_model_3 == int(3))
        {
            float _S271 = uv_15.x;
            if((F32_abs((_S271))) > 3.14159274101257324f)
            {
                _S270 = true;
            }
            else
            {
                _S270 = (F32_abs((uv_15.y))) > 1.57079637050628662f;
            }
            if(_S270)
            {
                int3  _S272 = make_int3 (int(0));
                float3  _S273 = make_float3 ((float)_S272.x, (float)_S272.y, (float)_S272.z);
                *raydir_2 = _S273;
                _S270 = false;
                break;
            }
            float lat_2 = uv_15.y;
            float cl_2 = (F32_cos((lat_2)));
            *raydir_2 = make_float3 (cl_2 * (F32_sin((_S271))), (F32_sin((lat_2))), cl_2 * (F32_cos((_S271))));
            _S270 = true;
            break;
        }
        FixedArray<float, 1>  _S274 = dist_coeffs_13;
        float2  uv_u_2;
        bool _S275 = undistort_point_0(uv_15, &_S274, int(8), &uv_u_2);
        if(!_S275)
        {
            int3  _S276 = make_int3 (int(0));
            float3  _S277 = make_float3 ((float)_S276.x, (float)_S276.y, (float)_S276.z);
            *raydir_2 = _S277;
            _S270 = false;
            break;
        }
        float2  _S278 = uv_u_2;
        if(camera_model_3 == int(1))
        {
            float r_13 = length_0(_S278);
            if(r_13 >= 3.14159274101257324f)
            {
                int3  _S279 = make_int3 (int(0));
                float3  _S280 = make_float3 ((float)_S279.x, (float)_S279.y, (float)_S279.z);
                *raydir_2 = _S280;
                _S270 = false;
                break;
            }
            float s_2;
            if(r_13 < 0.00100000004749745f)
            {
                s_2 = 1.0f - r_13 * r_13 / 6.0f;
            }
            else
            {
                s_2 = (F32_sin((r_13))) / r_13;
            }
            *raydir_2 = make_float3 ((_S278 * make_float2 (s_2)).x, (_S278 * make_float2 (s_2)).y, (F32_cos((r_13))));
        }
        else
        {
            if(camera_model_3 == int(2))
            {
                float r_14 = length_0(_S278);
                if(r_14 >= 2.0f)
                {
                    int3  _S281 = make_int3 (int(0));
                    float3  _S282 = make_float3 ((float)_S281.x, (float)_S281.y, (float)_S281.z);
                    *raydir_2 = _S282;
                    _S270 = false;
                    break;
                }
                *raydir_2 = make_float3 ((_S278 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_14 * r_14)))))))).x, (_S278 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_14 * r_14)))))))).y, 1.0f - 0.5f * r_14 * r_14);
            }
            else
            {
                *raydir_2 = make_float3 (_S278.x, _S278.y, 1.0f);
            }
        }
        *raydir_2 = normalize_0(*raydir_2);
        _S270 = true;
        break;
    }
    return _S270;
}

inline __device__ bool is_valid_distortion_opencv(float2  uv_16, FixedArray<float, 4>  dist_coeffs_14)
{
    float2  _S283 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S284;
    (&_S284)->primal_0 = uv_16;
    (&_S284)->differential_0 = _S283;
    FixedArray<float, 4>  _S285 = dist_coeffs_14;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S286 = s_fwd_DistOpenCV_distort_0(&_S284, &_S285);
    float2  _S287 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S288;
    (&_S288)->primal_0 = uv_16;
    (&_S288)->differential_0 = _S287;
    FixedArray<float, 4>  _S289 = dist_coeffs_14;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S290 = s_fwd_DistOpenCV_distort_0(&_S288, &_S289);
    Matrix<float, 2, 2>  _S291 = transpose_1(makeMatrix<float, 2, 2> (_S286.differential_0, _S290.differential_0));
    float _S292 = (F32_min((determinant_0(_S291)), ((F32_min((_S291.rows[int(0)].x), (_S291.rows[int(1)].y))))));
    bool _S293;
    if(_S292 > 0.25f)
    {
        _S293 = _S292 < 4.0f;
    }
    else
    {
        _S293 = false;
    }
    if(_S293)
    {
        FixedArray<float, 4>  _S294 = dist_coeffs_14;
        float2  _S295 = DistOpenCV_distort_0(uv_16, &_S294);
        _S293 = (dot_0(uv_16, _S295)) >= 0.0f;
    }
    else
    {
        _S293 = false;
    }
    return _S293;
}

inline __device__ bool persp_proj_nav_opencv(float3  p_view_8, float4  intrins_8, FixedArray<float, 4>  dist_coeffs_15, float2  * uv_17)
{
    bool _S296;
    for(;;)
    {
        float2  _S297 = float2 {p_view_8.x, p_view_8.y};
        float _S298 = p_view_8.z;
        float2  uv0_1 = _S297 / make_float2 (_S298);
        if(_S298 < 0.0f)
        {
            _S296 = true;
        }
        else
        {
            float2  _S299 = make_float2 (1.0f, 0.0f);
            DiffPair_vectorx3Cfloatx2C2x3E_0 _S300;
            (&_S300)->primal_0 = uv0_1;
            (&_S300)->differential_0 = _S299;
            FixedArray<float, 4>  _S301 = dist_coeffs_15;
            DiffPair_vectorx3Cfloatx2C2x3E_0 _S302 = s_fwd_DistOpenCV_distort_0(&_S300, &_S301);
            float2  _S303 = make_float2 (0.0f, 1.0f);
            DiffPair_vectorx3Cfloatx2C2x3E_0 _S304;
            (&_S304)->primal_0 = uv0_1;
            (&_S304)->differential_0 = _S303;
            FixedArray<float, 4>  _S305 = dist_coeffs_15;
            DiffPair_vectorx3Cfloatx2C2x3E_0 _S306 = s_fwd_DistOpenCV_distort_0(&_S304, &_S305);
            Matrix<float, 2, 2>  _S307 = transpose_1(makeMatrix<float, 2, 2> (_S302.differential_0, _S306.differential_0));
            float _S308 = (F32_min((determinant_0(_S307)), ((F32_min((_S307.rows[int(0)].x), (_S307.rows[int(1)].y))))));
            if(_S308 > 0.25f)
            {
                _S296 = _S308 < 4.0f;
            }
            else
            {
                _S296 = false;
            }
            if(_S296)
            {
                FixedArray<float, 4>  _S309 = dist_coeffs_15;
                float2  _S310 = DistOpenCV_distort_0(uv0_1, &_S309);
                _S296 = (dot_0(uv0_1, _S310)) >= 0.0f;
            }
            else
            {
                _S296 = false;
            }
            _S296 = !_S296;
        }
        if(_S296)
        {
            *uv_17 = uv0_1;
            _S296 = false;
            break;
        }
        float2  uv_18 = _S297 / make_float2 (_S298);
        FixedArray<float, 4>  _S311 = dist_coeffs_15;
        float2  _S312 = DistOpenCV_distort_0(uv_18, &_S311);
        *uv_17 = make_float2 (intrins_8.x * _S312.x + intrins_8.z, intrins_8.y * _S312.y + intrins_8.w);
        _S296 = true;
        break;
    }
    return _S296;
}

inline __device__ bool fisheye_proj_nav_opencv(float3  p_view_9, float4  intrins_9, FixedArray<float, 4>  dist_coeffs_16, float2  * uv_19)
{
    bool _S313;
    for(;;)
    {
        float2  _S314 = float2 {p_view_9.x, p_view_9.y};
        float r_15 = length_0(_S314);
        float _S315 = p_view_9.z;
        float theta_4 = (F32_atan2((r_15), (_S315)));
        bool _S316 = theta_4 < 0.00100000004749745f;
        float k_5;
        if(_S316)
        {
            k_5 = (1.0f - theta_4 * theta_4 / 3.0f) / _S315;
        }
        else
        {
            k_5 = theta_4 / r_15;
        }
        float2  _S317 = _S314 * make_float2 (k_5);
        float2  _S318 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S319;
        (&_S319)->primal_0 = _S317;
        (&_S319)->differential_0 = _S318;
        FixedArray<float, 4>  _S320 = dist_coeffs_16;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S321 = s_fwd_DistOpenCV_distort_0(&_S319, &_S320);
        float2  _S322 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S323;
        (&_S323)->primal_0 = _S317;
        (&_S323)->differential_0 = _S322;
        FixedArray<float, 4>  _S324 = dist_coeffs_16;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S325 = s_fwd_DistOpenCV_distort_0(&_S323, &_S324);
        Matrix<float, 2, 2>  _S326 = transpose_1(makeMatrix<float, 2, 2> (_S321.differential_0, _S325.differential_0));
        float _S327 = (F32_min((determinant_0(_S326)), ((F32_min((_S326.rows[int(0)].x), (_S326.rows[int(1)].y))))));
        if(_S327 > 0.25f)
        {
            _S313 = _S327 < 4.0f;
        }
        else
        {
            _S313 = false;
        }
        if(_S313)
        {
            FixedArray<float, 4>  _S328 = dist_coeffs_16;
            float2  _S329 = DistOpenCV_distort_0(_S317, &_S328);
            _S313 = (dot_0(_S317, _S329)) >= 0.0f;
        }
        else
        {
            _S313 = false;
        }
        if(!_S313)
        {
            *uv_19 = _S317;
            _S313 = false;
            break;
        }
        if(_S316)
        {
            k_5 = (1.0f - theta_4 * theta_4 / 3.0f) / _S315;
        }
        else
        {
            k_5 = theta_4 / r_15;
        }
        float2  _S330 = _S314 * make_float2 (k_5);
        FixedArray<float, 4>  _S331 = dist_coeffs_16;
        float2  _S332 = DistOpenCV_distort_0(_S330, &_S331);
        *uv_19 = make_float2 (intrins_9.x * _S332.x + intrins_9.z, intrins_9.y * _S332.y + intrins_9.w);
        _S313 = true;
        break;
    }
    return _S313;
}

inline __device__ bool equisolid_proj_nav_opencv(float3  p_view_10, float4  intrins_10, FixedArray<float, 4>  dist_coeffs_17, float2  * uv_20)
{
    bool _S333;
    for(;;)
    {
        float2  _S334 = float2 {p_view_10.x, p_view_10.y};
        float r_16 = length_0(_S334);
        float _S335 = p_view_10.z;
        float theta_5 = (F32_atan2((r_16), (_S335)));
        bool _S336 = r_16 < 9.99999997475242708e-07f;
        float k_6;
        if(_S336)
        {
            k_6 = (1.0f - theta_5 * theta_5 / 24.0f) / _S335;
        }
        else
        {
            k_6 = 2.0f * (F32_sin((0.5f * theta_5))) / r_16;
        }
        float2  _S337 = _S334 * make_float2 (k_6);
        float2  _S338 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S339;
        (&_S339)->primal_0 = _S337;
        (&_S339)->differential_0 = _S338;
        FixedArray<float, 4>  _S340 = dist_coeffs_17;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S341 = s_fwd_DistOpenCV_distort_0(&_S339, &_S340);
        float2  _S342 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S343;
        (&_S343)->primal_0 = _S337;
        (&_S343)->differential_0 = _S342;
        FixedArray<float, 4>  _S344 = dist_coeffs_17;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S345 = s_fwd_DistOpenCV_distort_0(&_S343, &_S344);
        Matrix<float, 2, 2>  _S346 = transpose_1(makeMatrix<float, 2, 2> (_S341.differential_0, _S345.differential_0));
        float _S347 = (F32_min((determinant_0(_S346)), ((F32_min((_S346.rows[int(0)].x), (_S346.rows[int(1)].y))))));
        if(_S347 > 0.25f)
        {
            _S333 = _S347 < 4.0f;
        }
        else
        {
            _S333 = false;
        }
        if(_S333)
        {
            FixedArray<float, 4>  _S348 = dist_coeffs_17;
            float2  _S349 = DistOpenCV_distort_0(_S337, &_S348);
            _S333 = (dot_0(_S337, _S349)) >= 0.0f;
        }
        else
        {
            _S333 = false;
        }
        if(!_S333)
        {
            *uv_20 = _S337;
            _S333 = false;
            break;
        }
        if(_S336)
        {
            k_6 = (1.0f - theta_5 * theta_5 / 24.0f) / _S335;
        }
        else
        {
            k_6 = 2.0f * (F32_sin((0.5f * theta_5))) / r_16;
        }
        float2  _S350 = _S334 * make_float2 (k_6);
        FixedArray<float, 4>  _S351 = dist_coeffs_17;
        float2  _S352 = DistOpenCV_distort_0(_S350, &_S351);
        *uv_20 = make_float2 (intrins_10.x * _S352.x + intrins_10.z, intrins_10.y * _S352.y + intrins_10.w);
        _S333 = true;
        break;
    }
    return _S333;
}

inline __device__ Matrix<float, 2, 3>  persp_proj_jac_opencv(float3  p_view_11, float4  intrins_11, FixedArray<float, 4>  dist_coeffs_18)
{
    float2  _S353 = float2 {p_view_11.x, p_view_11.y};
    float _S354 = p_view_11.z;
    float2  _S355 = _S353 * make_float2 (0.0f);
    float _S356 = _S354 * _S354;
    float2  s_diff_uv_3 = (make_float2 (1.0f, 0.0f) * make_float2 (_S354) - _S355) / make_float2 (_S356);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S357;
    (&_S357)->primal_0 = _S353 / make_float2 (_S354);
    (&_S357)->differential_0 = s_diff_uv_3;
    FixedArray<float, 4>  _S358 = dist_coeffs_18;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S359 = s_fwd_DistOpenCV_distort_0(&_S357, &_S358);
    float fx_4 = intrins_11.x;
    float fy_4 = intrins_11.y;
    float _S360 = _S359.differential_0.y * fy_4;
    Matrix<float, 2, 3>  J_4;
    *&(((&J_4)->rows + (int(0)))->x) = _S359.differential_0.x * fx_4;
    *&(((&J_4)->rows + (int(1)))->x) = _S360;
    float2  s_diff_uv_4 = (make_float2 (0.0f, 1.0f) * make_float2 (_S354) - _S355) / make_float2 (_S356);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S361;
    (&_S361)->primal_0 = _S353 / make_float2 (_S354);
    (&_S361)->differential_0 = s_diff_uv_4;
    FixedArray<float, 4>  _S362 = dist_coeffs_18;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S363 = s_fwd_DistOpenCV_distort_0(&_S361, &_S362);
    float _S364 = _S363.differential_0.y * fy_4;
    *&(((&J_4)->rows + (int(0)))->y) = _S363.differential_0.x * fx_4;
    *&(((&J_4)->rows + (int(1)))->y) = _S364;
    float2  s_diff_uv_5 = (make_float2 (0.0f, 0.0f) * make_float2 (_S354) - _S353) / make_float2 (_S356);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S365;
    (&_S365)->primal_0 = _S353 / make_float2 (_S354);
    (&_S365)->differential_0 = s_diff_uv_5;
    FixedArray<float, 4>  _S366 = dist_coeffs_18;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S367 = s_fwd_DistOpenCV_distort_0(&_S365, &_S366);
    float _S368 = _S367.differential_0.y * fy_4;
    *&(((&J_4)->rows + (int(0)))->z) = _S367.differential_0.x * fx_4;
    *&(((&J_4)->rows + (int(1)))->z) = _S368;
    return J_4;
}

inline __device__ Matrix<float, 2, 3>  fisheye_proj_jac_opencv(float3  p_view_12, float4  intrins_12, FixedArray<float, 4>  dist_coeffs_19)
{
    Matrix<float, 2, 3>  J_5;
    float2  _S369 = float2 {p_view_12.x, p_view_12.y};
    float2  _S370 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S371;
    (&_S371)->primal_0 = _S369;
    (&_S371)->differential_0 = _S370;
    DiffPair_float_0 _S372 = s_fwd_length_impl_0(&_S371);
    float _S373 = p_view_12.z;
    DiffPair_float_0 _S374;
    (&_S374)->primal_0 = _S372.primal_0;
    (&_S374)->differential_0 = _S372.differential_0;
    DiffPair_float_0 _S375;
    (&_S375)->primal_0 = _S373;
    (&_S375)->differential_0 = 0.0f;
    DiffPair_float_0 _S376 = _d_atan2_0(&_S374, &_S375);
    float k_7;
    float s_diff_k_2;
    if((_S376.primal_0) < 0.00100000004749745f)
    {
        float _S377 = _S376.differential_0 * _S376.primal_0;
        float _S378 = 1.0f - _S376.primal_0 * _S376.primal_0 / 3.0f;
        float _S379 = ((0.0f - (_S377 + _S377) * 0.3333333432674408f) * _S373 - _S378 * 0.0f) / (_S373 * _S373);
        k_7 = _S378 / _S373;
        s_diff_k_2 = _S379;
    }
    else
    {
        float _S380 = (_S376.differential_0 * _S372.primal_0 - _S376.primal_0 * _S372.differential_0) / (_S372.primal_0 * _S372.primal_0);
        k_7 = _S376.primal_0 / _S372.primal_0;
        s_diff_k_2 = _S380;
    }
    float2  _S381 = _S369 * make_float2 (k_7);
    float2  _S382 = _S370 * make_float2 (k_7) + make_float2 (s_diff_k_2) * _S369;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S383;
    (&_S383)->primal_0 = _S381;
    (&_S383)->differential_0 = _S382;
    FixedArray<float, 4>  _S384 = dist_coeffs_19;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S385 = s_fwd_DistOpenCV_distort_0(&_S383, &_S384);
    float fx_5 = intrins_12.x;
    float fy_5 = intrins_12.y;
    float _S386 = _S385.differential_0.y * fy_5;
    *&(((&J_5)->rows + (int(0)))->x) = _S385.differential_0.x * fx_5;
    *&(((&J_5)->rows + (int(1)))->x) = _S386;
    float2  _S387 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S388;
    (&_S388)->primal_0 = _S369;
    (&_S388)->differential_0 = _S387;
    DiffPair_float_0 _S389 = s_fwd_length_impl_0(&_S388);
    DiffPair_float_0 _S390;
    (&_S390)->primal_0 = _S389.primal_0;
    (&_S390)->differential_0 = _S389.differential_0;
    DiffPair_float_0 _S391;
    (&_S391)->primal_0 = _S373;
    (&_S391)->differential_0 = 0.0f;
    DiffPair_float_0 _S392 = _d_atan2_0(&_S390, &_S391);
    if((_S392.primal_0) < 0.00100000004749745f)
    {
        float _S393 = _S392.differential_0 * _S392.primal_0;
        float _S394 = 1.0f - _S392.primal_0 * _S392.primal_0 / 3.0f;
        float _S395 = ((0.0f - (_S393 + _S393) * 0.3333333432674408f) * _S373 - _S394 * 0.0f) / (_S373 * _S373);
        k_7 = _S394 / _S373;
        s_diff_k_2 = _S395;
    }
    else
    {
        float _S396 = (_S392.differential_0 * _S389.primal_0 - _S392.primal_0 * _S389.differential_0) / (_S389.primal_0 * _S389.primal_0);
        k_7 = _S392.primal_0 / _S389.primal_0;
        s_diff_k_2 = _S396;
    }
    float2  _S397 = _S369 * make_float2 (k_7);
    float2  _S398 = _S387 * make_float2 (k_7) + make_float2 (s_diff_k_2) * _S369;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S399;
    (&_S399)->primal_0 = _S397;
    (&_S399)->differential_0 = _S398;
    FixedArray<float, 4>  _S400 = dist_coeffs_19;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S401 = s_fwd_DistOpenCV_distort_0(&_S399, &_S400);
    float _S402 = _S401.differential_0.y * fy_5;
    *&(((&J_5)->rows + (int(0)))->y) = _S401.differential_0.x * fx_5;
    *&(((&J_5)->rows + (int(1)))->y) = _S402;
    float2  _S403 = make_float2 (0.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S404;
    (&_S404)->primal_0 = _S369;
    (&_S404)->differential_0 = _S403;
    DiffPair_float_0 _S405 = s_fwd_length_impl_0(&_S404);
    DiffPair_float_0 _S406;
    (&_S406)->primal_0 = _S405.primal_0;
    (&_S406)->differential_0 = _S405.differential_0;
    DiffPair_float_0 _S407;
    (&_S407)->primal_0 = _S373;
    (&_S407)->differential_0 = 1.0f;
    DiffPair_float_0 _S408 = _d_atan2_0(&_S406, &_S407);
    if((_S408.primal_0) < 0.00100000004749745f)
    {
        float _S409 = _S408.differential_0 * _S408.primal_0;
        float _S410 = 1.0f - _S408.primal_0 * _S408.primal_0 / 3.0f;
        float _S411 = ((0.0f - (_S409 + _S409) * 0.3333333432674408f) * _S373 - _S410) / (_S373 * _S373);
        k_7 = _S410 / _S373;
        s_diff_k_2 = _S411;
    }
    else
    {
        float _S412 = (_S408.differential_0 * _S405.primal_0 - _S408.primal_0 * _S405.differential_0) / (_S405.primal_0 * _S405.primal_0);
        k_7 = _S408.primal_0 / _S405.primal_0;
        s_diff_k_2 = _S412;
    }
    float2  _S413 = _S369 * make_float2 (k_7);
    float2  _S414 = _S403 * make_float2 (k_7) + make_float2 (s_diff_k_2) * _S369;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S415;
    (&_S415)->primal_0 = _S413;
    (&_S415)->differential_0 = _S414;
    FixedArray<float, 4>  _S416 = dist_coeffs_19;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S417 = s_fwd_DistOpenCV_distort_0(&_S415, &_S416);
    float _S418 = _S417.differential_0.y * fy_5;
    *&(((&J_5)->rows + (int(0)))->z) = _S417.differential_0.x * fx_5;
    *&(((&J_5)->rows + (int(1)))->z) = _S418;
    return J_5;
}

inline __device__ Matrix<float, 2, 3>  equisolid_proj_jac_opencv(float3  p_view_13, float4  intrins_13, FixedArray<float, 4>  dist_coeffs_20)
{
    Matrix<float, 2, 3>  J_6;
    float2  _S419 = float2 {p_view_13.x, p_view_13.y};
    float2  _S420 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S421;
    (&_S421)->primal_0 = _S419;
    (&_S421)->differential_0 = _S420;
    DiffPair_float_0 _S422 = s_fwd_length_impl_0(&_S421);
    float _S423 = p_view_13.z;
    DiffPair_float_0 _S424;
    (&_S424)->primal_0 = _S422.primal_0;
    (&_S424)->differential_0 = _S422.differential_0;
    DiffPair_float_0 _S425;
    (&_S425)->primal_0 = _S423;
    (&_S425)->differential_0 = 0.0f;
    DiffPair_float_0 _S426 = _d_atan2_0(&_S424, &_S425);
    float k_8;
    float s_diff_k_3;
    if((_S422.primal_0) < 9.99999997475242708e-07f)
    {
        float _S427 = _S426.differential_0 * _S426.primal_0;
        float _S428 = 1.0f - _S426.primal_0 * _S426.primal_0 / 24.0f;
        float _S429 = ((0.0f - (_S427 + _S427) * 0.0416666679084301f) * _S423 - _S428 * 0.0f) / (_S423 * _S423);
        k_8 = _S428 / _S423;
        s_diff_k_3 = _S429;
    }
    else
    {
        float _S430 = _S426.differential_0 * 0.5f;
        DiffPair_float_0 _S431;
        (&_S431)->primal_0 = 0.5f * _S426.primal_0;
        (&_S431)->differential_0 = _S430;
        DiffPair_float_0 _S432 = _d_sin_0(&_S431);
        float _S433 = 2.0f * _S432.primal_0;
        float _S434 = (_S432.differential_0 * 2.0f * _S422.primal_0 - _S433 * _S422.differential_0) / (_S422.primal_0 * _S422.primal_0);
        k_8 = _S433 / _S422.primal_0;
        s_diff_k_3 = _S434;
    }
    float2  _S435 = _S419 * make_float2 (k_8);
    float2  _S436 = _S420 * make_float2 (k_8) + make_float2 (s_diff_k_3) * _S419;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S437;
    (&_S437)->primal_0 = _S435;
    (&_S437)->differential_0 = _S436;
    FixedArray<float, 4>  _S438 = dist_coeffs_20;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S439 = s_fwd_DistOpenCV_distort_0(&_S437, &_S438);
    float fx_6 = intrins_13.x;
    float fy_6 = intrins_13.y;
    float _S440 = _S439.differential_0.y * fy_6;
    *&(((&J_6)->rows + (int(0)))->x) = _S439.differential_0.x * fx_6;
    *&(((&J_6)->rows + (int(1)))->x) = _S440;
    float2  _S441 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S442;
    (&_S442)->primal_0 = _S419;
    (&_S442)->differential_0 = _S441;
    DiffPair_float_0 _S443 = s_fwd_length_impl_0(&_S442);
    DiffPair_float_0 _S444;
    (&_S444)->primal_0 = _S443.primal_0;
    (&_S444)->differential_0 = _S443.differential_0;
    DiffPair_float_0 _S445;
    (&_S445)->primal_0 = _S423;
    (&_S445)->differential_0 = 0.0f;
    DiffPair_float_0 _S446 = _d_atan2_0(&_S444, &_S445);
    if((_S443.primal_0) < 9.99999997475242708e-07f)
    {
        float _S447 = _S446.differential_0 * _S446.primal_0;
        float _S448 = 1.0f - _S446.primal_0 * _S446.primal_0 / 24.0f;
        float _S449 = ((0.0f - (_S447 + _S447) * 0.0416666679084301f) * _S423 - _S448 * 0.0f) / (_S423 * _S423);
        k_8 = _S448 / _S423;
        s_diff_k_3 = _S449;
    }
    else
    {
        float _S450 = _S446.differential_0 * 0.5f;
        DiffPair_float_0 _S451;
        (&_S451)->primal_0 = 0.5f * _S446.primal_0;
        (&_S451)->differential_0 = _S450;
        DiffPair_float_0 _S452 = _d_sin_0(&_S451);
        float _S453 = 2.0f * _S452.primal_0;
        float _S454 = (_S452.differential_0 * 2.0f * _S443.primal_0 - _S453 * _S443.differential_0) / (_S443.primal_0 * _S443.primal_0);
        k_8 = _S453 / _S443.primal_0;
        s_diff_k_3 = _S454;
    }
    float2  _S455 = _S419 * make_float2 (k_8);
    float2  _S456 = _S441 * make_float2 (k_8) + make_float2 (s_diff_k_3) * _S419;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S457;
    (&_S457)->primal_0 = _S455;
    (&_S457)->differential_0 = _S456;
    FixedArray<float, 4>  _S458 = dist_coeffs_20;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S459 = s_fwd_DistOpenCV_distort_0(&_S457, &_S458);
    float _S460 = _S459.differential_0.y * fy_6;
    *&(((&J_6)->rows + (int(0)))->y) = _S459.differential_0.x * fx_6;
    *&(((&J_6)->rows + (int(1)))->y) = _S460;
    float2  _S461 = make_float2 (0.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S462;
    (&_S462)->primal_0 = _S419;
    (&_S462)->differential_0 = _S461;
    DiffPair_float_0 _S463 = s_fwd_length_impl_0(&_S462);
    DiffPair_float_0 _S464;
    (&_S464)->primal_0 = _S463.primal_0;
    (&_S464)->differential_0 = _S463.differential_0;
    DiffPair_float_0 _S465;
    (&_S465)->primal_0 = _S423;
    (&_S465)->differential_0 = 1.0f;
    DiffPair_float_0 _S466 = _d_atan2_0(&_S464, &_S465);
    if((_S463.primal_0) < 9.99999997475242708e-07f)
    {
        float _S467 = _S466.differential_0 * _S466.primal_0;
        float _S468 = 1.0f - _S466.primal_0 * _S466.primal_0 / 24.0f;
        float _S469 = ((0.0f - (_S467 + _S467) * 0.0416666679084301f) * _S423 - _S468) / (_S423 * _S423);
        k_8 = _S468 / _S423;
        s_diff_k_3 = _S469;
    }
    else
    {
        float _S470 = _S466.differential_0 * 0.5f;
        DiffPair_float_0 _S471;
        (&_S471)->primal_0 = 0.5f * _S466.primal_0;
        (&_S471)->differential_0 = _S470;
        DiffPair_float_0 _S472 = _d_sin_0(&_S471);
        float _S473 = 2.0f * _S472.primal_0;
        float _S474 = (_S472.differential_0 * 2.0f * _S463.primal_0 - _S473 * _S463.differential_0) / (_S463.primal_0 * _S463.primal_0);
        k_8 = _S473 / _S463.primal_0;
        s_diff_k_3 = _S474;
    }
    float2  _S475 = _S419 * make_float2 (k_8);
    float2  _S476 = _S461 * make_float2 (k_8) + make_float2 (s_diff_k_3) * _S419;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S477;
    (&_S477)->primal_0 = _S475;
    (&_S477)->differential_0 = _S476;
    FixedArray<float, 4>  _S478 = dist_coeffs_20;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S479 = s_fwd_DistOpenCV_distort_0(&_S477, &_S478);
    float _S480 = _S479.differential_0.y * fy_6;
    *&(((&J_6)->rows + (int(0)))->z) = _S479.differential_0.x * fx_6;
    *&(((&J_6)->rows + (int(1)))->z) = _S480;
    return J_6;
}

inline __device__ float2  distort_point_opencv(float2  uv_21, int camera_model_4, FixedArray<float, 4>  dist_coeffs_21)
{
    float2  _S481;
    for(;;)
    {
        if(camera_model_4 == int(3))
        {
            _S481 = uv_21;
            break;
        }
        float k_9;
        if(camera_model_4 == int(1))
        {
            float r_17 = length_0(uv_21);
            float theta_6 = (F32_atan((r_17)));
            if(r_17 < 0.00100000004749745f)
            {
                k_9 = 1.0f - theta_6 * theta_6 / 6.0f;
            }
            else
            {
                k_9 = theta_6 / r_17;
            }
            _S481 = uv_21 * make_float2 (k_9);
        }
        else
        {
            if(camera_model_4 == int(2))
            {
                float r_18 = length_0(uv_21);
                float theta_7 = (F32_atan((r_18)));
                if(r_18 < 0.00100000004749745f)
                {
                    k_9 = 1.0f - theta_7 * theta_7 / 24.0f;
                }
                else
                {
                    k_9 = 2.0f * (F32_sin((0.5f * theta_7))) / r_18;
                }
                _S481 = uv_21 * make_float2 (k_9);
            }
            else
            {
                _S481 = uv_21;
            }
        }
        FixedArray<float, 4>  _S482 = dist_coeffs_21;
        float2  _S483 = DistOpenCV_distort_0(_S481, &_S482);
        _S481 = _S483;
        break;
    }
    return _S481;
}

inline __device__ bool undistort_point_opencv(float2  uv_22, int camera_model_5, FixedArray<float, 4>  dist_coeffs_22, float2  * uv_undist_4)
{
    bool _S484;
    for(;;)
    {
        *uv_undist_4 = make_float2 (0.0f);
        if(camera_model_5 == int(3))
        {
            float lon_2 = uv_22.x;
            float lat_3 = uv_22.y;
            float cl_3 = (F32_cos((lat_3)));
            *uv_undist_4 = make_float2 (cl_3 * (F32_sin((lon_2))), (F32_sin((lat_3)))) / make_float2 ((F32_max((cl_3 * (F32_cos((lon_2)))), (9.999999960041972e-13f))));
            _S484 = true;
            break;
        }
        FixedArray<float, 4>  _S485 = dist_coeffs_22;
        float2  uv_u_3;
        bool _S486 = undistort_point_1(uv_22, &_S485, int(8), &uv_u_3);
        if(!_S486)
        {
            _S484 = false;
            break;
        }
        float2  _S487 = uv_u_3;
        float3  raydir_3;
        if(camera_model_5 == int(1))
        {
            float r_19 = length_0(_S487);
            float s_3;
            if(r_19 < 0.00100000004749745f)
            {
                s_3 = 1.0f - r_19 * r_19 / 6.0f;
            }
            else
            {
                s_3 = (F32_sin((r_19))) / r_19;
            }
            raydir_3 = make_float3 ((_S487 * make_float2 (s_3)).x, (_S487 * make_float2 (s_3)).y, (F32_cos((r_19))));
        }
        else
        {
            if(camera_model_5 == int(2))
            {
                float r_20 = length_0(_S487);
                raydir_3 = make_float3 ((_S487 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_20 * r_20)))))))).x, (_S487 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_20 * r_20)))))))).y, 1.0f - 0.5f * r_20 * r_20);
            }
            else
            {
                raydir_3 = make_float3 (_S487.x, _S487.y, 1.0f);
            }
        }
        *uv_undist_4 = float2 {raydir_3.x, raydir_3.y} / make_float2 ((F32_max((raydir_3.z), (9.999999960041972e-13f))));
        _S484 = true;
        break;
    }
    return _S484;
}

inline __device__ bool unproject_point_opencv(float2  uv_23, int camera_model_6, FixedArray<float, 4>  dist_coeffs_23, float3  * raydir_4)
{
    bool _S488;
    for(;;)
    {
        if(camera_model_6 == int(4))
        {
            *raydir_4 = make_float3 (0.0f, 0.0f, 1.0f);
            _S488 = true;
            break;
        }
        int3  _S489 = make_int3 (int(0));
        float3  _S490 = make_float3 ((float)_S489.x, (float)_S489.y, (float)_S489.z);
        *raydir_4 = _S490;
        if(camera_model_6 == int(3))
        {
            float lon_3 = uv_23.x;
            float lat_4 = uv_23.y;
            float cl_4 = (F32_cos((lat_4)));
            *raydir_4 = make_float3 (cl_4 * (F32_sin((lon_3))), (F32_sin((lat_4))), cl_4 * (F32_cos((lon_3))));
            _S488 = true;
            break;
        }
        FixedArray<float, 4>  _S491 = dist_coeffs_23;
        float2  uv_u_4;
        bool _S492 = undistort_point_1(uv_23, &_S491, int(8), &uv_u_4);
        if(!_S492)
        {
            _S488 = false;
            break;
        }
        float2  _S493 = uv_u_4;
        if(camera_model_6 == int(1))
        {
            float r_21 = length_0(_S493);
            float s_4;
            if(r_21 < 0.00100000004749745f)
            {
                s_4 = 1.0f - r_21 * r_21 / 6.0f;
            }
            else
            {
                s_4 = (F32_sin((r_21))) / r_21;
            }
            *raydir_4 = make_float3 ((_S493 * make_float2 (s_4)).x, (_S493 * make_float2 (s_4)).y, (F32_cos((r_21))));
        }
        else
        {
            if(camera_model_6 == int(2))
            {
                float r_22 = length_0(_S493);
                *raydir_4 = make_float3 ((_S493 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_22 * r_22)))))))).x, (_S493 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_22 * r_22)))))))).y, 1.0f - 0.5f * r_22 * r_22);
            }
            else
            {
                *raydir_4 = make_float3 (_S493.x, _S493.y, 1.0f);
            }
        }
        _S488 = true;
        break;
    }
    return _S488;
}

inline __device__ bool generate_ray_opencv(float2  uv_24, int camera_model_7, FixedArray<float, 4>  dist_coeffs_24, float3  * raydir_5)
{
    bool _S494;
    for(;;)
    {
        if(camera_model_7 == int(4))
        {
            *raydir_5 = make_float3 (0.0f, 0.0f, 1.0f);
            _S494 = true;
            break;
        }
        if(camera_model_7 == int(3))
        {
            float _S495 = uv_24.x;
            if((F32_abs((_S495))) > 3.14159274101257324f)
            {
                _S494 = true;
            }
            else
            {
                _S494 = (F32_abs((uv_24.y))) > 1.57079637050628662f;
            }
            if(_S494)
            {
                int3  _S496 = make_int3 (int(0));
                float3  _S497 = make_float3 ((float)_S496.x, (float)_S496.y, (float)_S496.z);
                *raydir_5 = _S497;
                _S494 = false;
                break;
            }
            float lat_5 = uv_24.y;
            float cl_5 = (F32_cos((lat_5)));
            *raydir_5 = make_float3 (cl_5 * (F32_sin((_S495))), (F32_sin((lat_5))), cl_5 * (F32_cos((_S495))));
            _S494 = true;
            break;
        }
        FixedArray<float, 4>  _S498 = dist_coeffs_24;
        float2  uv_u_5;
        bool _S499 = undistort_point_1(uv_24, &_S498, int(8), &uv_u_5);
        if(!_S499)
        {
            int3  _S500 = make_int3 (int(0));
            float3  _S501 = make_float3 ((float)_S500.x, (float)_S500.y, (float)_S500.z);
            *raydir_5 = _S501;
            _S494 = false;
            break;
        }
        float2  _S502 = uv_u_5;
        if(camera_model_7 == int(1))
        {
            float r_23 = length_0(_S502);
            if(r_23 >= 3.14159274101257324f)
            {
                int3  _S503 = make_int3 (int(0));
                float3  _S504 = make_float3 ((float)_S503.x, (float)_S503.y, (float)_S503.z);
                *raydir_5 = _S504;
                _S494 = false;
                break;
            }
            float s_5;
            if(r_23 < 0.00100000004749745f)
            {
                s_5 = 1.0f - r_23 * r_23 / 6.0f;
            }
            else
            {
                s_5 = (F32_sin((r_23))) / r_23;
            }
            *raydir_5 = make_float3 ((_S502 * make_float2 (s_5)).x, (_S502 * make_float2 (s_5)).y, (F32_cos((r_23))));
        }
        else
        {
            if(camera_model_7 == int(2))
            {
                float r_24 = length_0(_S502);
                if(r_24 >= 2.0f)
                {
                    int3  _S505 = make_int3 (int(0));
                    float3  _S506 = make_float3 ((float)_S505.x, (float)_S505.y, (float)_S505.z);
                    *raydir_5 = _S506;
                    _S494 = false;
                    break;
                }
                *raydir_5 = make_float3 ((_S502 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_24 * r_24)))))))).x, (_S502 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_24 * r_24)))))))).y, 1.0f - 0.5f * r_24 * r_24);
            }
            else
            {
                *raydir_5 = make_float3 (_S502.x, _S502.y, 1.0f);
            }
        }
        *raydir_5 = normalize_0(*raydir_5);
        _S494 = true;
        break;
    }
    return _S494;
}

inline __device__ bool is_valid_distortion_prism(float2  uv_25, FixedArray<float, 8>  dist_coeffs_25)
{
    float2  _S507 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S508;
    (&_S508)->primal_0 = uv_25;
    (&_S508)->differential_0 = _S507;
    FixedArray<float, 8>  _S509 = dist_coeffs_25;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S510 = s_fwd_DistThinPrism_distort_0(&_S508, &_S509);
    float2  _S511 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S512;
    (&_S512)->primal_0 = uv_25;
    (&_S512)->differential_0 = _S511;
    FixedArray<float, 8>  _S513 = dist_coeffs_25;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S514 = s_fwd_DistThinPrism_distort_0(&_S512, &_S513);
    Matrix<float, 2, 2>  _S515 = transpose_1(makeMatrix<float, 2, 2> (_S510.differential_0, _S514.differential_0));
    float _S516 = (F32_min((determinant_0(_S515)), ((F32_min((_S515.rows[int(0)].x), (_S515.rows[int(1)].y))))));
    bool _S517;
    if(_S516 > 0.25f)
    {
        _S517 = _S516 < 4.0f;
    }
    else
    {
        _S517 = false;
    }
    if(_S517)
    {
        FixedArray<float, 8>  _S518 = dist_coeffs_25;
        float2  _S519 = DistThinPrism_distort_0(uv_25, &_S518);
        _S517 = (dot_0(uv_25, _S519)) >= 0.0f;
    }
    else
    {
        _S517 = false;
    }
    return _S517;
}

inline __device__ bool persp_proj_nav_prism(float3  p_view_14, float4  intrins_14, FixedArray<float, 8>  dist_coeffs_26, float2  * uv_26)
{
    bool _S520;
    for(;;)
    {
        float2  _S521 = float2 {p_view_14.x, p_view_14.y};
        float _S522 = p_view_14.z;
        float2  uv0_2 = _S521 / make_float2 (_S522);
        if(_S522 < 0.0f)
        {
            _S520 = true;
        }
        else
        {
            float2  _S523 = make_float2 (1.0f, 0.0f);
            DiffPair_vectorx3Cfloatx2C2x3E_0 _S524;
            (&_S524)->primal_0 = uv0_2;
            (&_S524)->differential_0 = _S523;
            FixedArray<float, 8>  _S525 = dist_coeffs_26;
            DiffPair_vectorx3Cfloatx2C2x3E_0 _S526 = s_fwd_DistThinPrism_distort_0(&_S524, &_S525);
            float2  _S527 = make_float2 (0.0f, 1.0f);
            DiffPair_vectorx3Cfloatx2C2x3E_0 _S528;
            (&_S528)->primal_0 = uv0_2;
            (&_S528)->differential_0 = _S527;
            FixedArray<float, 8>  _S529 = dist_coeffs_26;
            DiffPair_vectorx3Cfloatx2C2x3E_0 _S530 = s_fwd_DistThinPrism_distort_0(&_S528, &_S529);
            Matrix<float, 2, 2>  _S531 = transpose_1(makeMatrix<float, 2, 2> (_S526.differential_0, _S530.differential_0));
            float _S532 = (F32_min((determinant_0(_S531)), ((F32_min((_S531.rows[int(0)].x), (_S531.rows[int(1)].y))))));
            if(_S532 > 0.25f)
            {
                _S520 = _S532 < 4.0f;
            }
            else
            {
                _S520 = false;
            }
            if(_S520)
            {
                FixedArray<float, 8>  _S533 = dist_coeffs_26;
                float2  _S534 = DistThinPrism_distort_0(uv0_2, &_S533);
                _S520 = (dot_0(uv0_2, _S534)) >= 0.0f;
            }
            else
            {
                _S520 = false;
            }
            _S520 = !_S520;
        }
        if(_S520)
        {
            *uv_26 = uv0_2;
            _S520 = false;
            break;
        }
        float2  uv_27 = _S521 / make_float2 (_S522);
        FixedArray<float, 8>  _S535 = dist_coeffs_26;
        float2  _S536 = DistThinPrism_distort_0(uv_27, &_S535);
        *uv_26 = make_float2 (intrins_14.x * _S536.x + intrins_14.z, intrins_14.y * _S536.y + intrins_14.w);
        _S520 = true;
        break;
    }
    return _S520;
}

inline __device__ bool fisheye_proj_nav_prism(float3  p_view_15, float4  intrins_15, FixedArray<float, 8>  dist_coeffs_27, float2  * uv_28)
{
    bool _S537;
    for(;;)
    {
        float2  _S538 = float2 {p_view_15.x, p_view_15.y};
        float r_25 = length_0(_S538);
        float _S539 = p_view_15.z;
        float theta_8 = (F32_atan2((r_25), (_S539)));
        bool _S540 = theta_8 < 0.00100000004749745f;
        float k_10;
        if(_S540)
        {
            k_10 = (1.0f - theta_8 * theta_8 / 3.0f) / _S539;
        }
        else
        {
            k_10 = theta_8 / r_25;
        }
        float2  _S541 = _S538 * make_float2 (k_10);
        float2  _S542 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S543;
        (&_S543)->primal_0 = _S541;
        (&_S543)->differential_0 = _S542;
        FixedArray<float, 8>  _S544 = dist_coeffs_27;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S545 = s_fwd_DistThinPrism_distort_0(&_S543, &_S544);
        float2  _S546 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S547;
        (&_S547)->primal_0 = _S541;
        (&_S547)->differential_0 = _S546;
        FixedArray<float, 8>  _S548 = dist_coeffs_27;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S549 = s_fwd_DistThinPrism_distort_0(&_S547, &_S548);
        Matrix<float, 2, 2>  _S550 = transpose_1(makeMatrix<float, 2, 2> (_S545.differential_0, _S549.differential_0));
        float _S551 = (F32_min((determinant_0(_S550)), ((F32_min((_S550.rows[int(0)].x), (_S550.rows[int(1)].y))))));
        if(_S551 > 0.25f)
        {
            _S537 = _S551 < 4.0f;
        }
        else
        {
            _S537 = false;
        }
        if(_S537)
        {
            FixedArray<float, 8>  _S552 = dist_coeffs_27;
            float2  _S553 = DistThinPrism_distort_0(_S541, &_S552);
            _S537 = (dot_0(_S541, _S553)) >= 0.0f;
        }
        else
        {
            _S537 = false;
        }
        if(!_S537)
        {
            *uv_28 = _S541;
            _S537 = false;
            break;
        }
        if(_S540)
        {
            k_10 = (1.0f - theta_8 * theta_8 / 3.0f) / _S539;
        }
        else
        {
            k_10 = theta_8 / r_25;
        }
        float2  _S554 = _S538 * make_float2 (k_10);
        FixedArray<float, 8>  _S555 = dist_coeffs_27;
        float2  _S556 = DistThinPrism_distort_0(_S554, &_S555);
        *uv_28 = make_float2 (intrins_15.x * _S556.x + intrins_15.z, intrins_15.y * _S556.y + intrins_15.w);
        _S537 = true;
        break;
    }
    return _S537;
}

inline __device__ bool equisolid_proj_nav_prism(float3  p_view_16, float4  intrins_16, FixedArray<float, 8>  dist_coeffs_28, float2  * uv_29)
{
    bool _S557;
    for(;;)
    {
        float2  _S558 = float2 {p_view_16.x, p_view_16.y};
        float r_26 = length_0(_S558);
        float _S559 = p_view_16.z;
        float theta_9 = (F32_atan2((r_26), (_S559)));
        bool _S560 = r_26 < 9.99999997475242708e-07f;
        float k_11;
        if(_S560)
        {
            k_11 = (1.0f - theta_9 * theta_9 / 24.0f) / _S559;
        }
        else
        {
            k_11 = 2.0f * (F32_sin((0.5f * theta_9))) / r_26;
        }
        float2  _S561 = _S558 * make_float2 (k_11);
        float2  _S562 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S563;
        (&_S563)->primal_0 = _S561;
        (&_S563)->differential_0 = _S562;
        FixedArray<float, 8>  _S564 = dist_coeffs_28;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S565 = s_fwd_DistThinPrism_distort_0(&_S563, &_S564);
        float2  _S566 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S567;
        (&_S567)->primal_0 = _S561;
        (&_S567)->differential_0 = _S566;
        FixedArray<float, 8>  _S568 = dist_coeffs_28;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S569 = s_fwd_DistThinPrism_distort_0(&_S567, &_S568);
        Matrix<float, 2, 2>  _S570 = transpose_1(makeMatrix<float, 2, 2> (_S565.differential_0, _S569.differential_0));
        float _S571 = (F32_min((determinant_0(_S570)), ((F32_min((_S570.rows[int(0)].x), (_S570.rows[int(1)].y))))));
        if(_S571 > 0.25f)
        {
            _S557 = _S571 < 4.0f;
        }
        else
        {
            _S557 = false;
        }
        if(_S557)
        {
            FixedArray<float, 8>  _S572 = dist_coeffs_28;
            float2  _S573 = DistThinPrism_distort_0(_S561, &_S572);
            _S557 = (dot_0(_S561, _S573)) >= 0.0f;
        }
        else
        {
            _S557 = false;
        }
        if(!_S557)
        {
            *uv_29 = _S561;
            _S557 = false;
            break;
        }
        if(_S560)
        {
            k_11 = (1.0f - theta_9 * theta_9 / 24.0f) / _S559;
        }
        else
        {
            k_11 = 2.0f * (F32_sin((0.5f * theta_9))) / r_26;
        }
        float2  _S574 = _S558 * make_float2 (k_11);
        FixedArray<float, 8>  _S575 = dist_coeffs_28;
        float2  _S576 = DistThinPrism_distort_0(_S574, &_S575);
        *uv_29 = make_float2 (intrins_16.x * _S576.x + intrins_16.z, intrins_16.y * _S576.y + intrins_16.w);
        _S557 = true;
        break;
    }
    return _S557;
}

inline __device__ Matrix<float, 2, 3>  persp_proj_jac_prism(float3  p_view_17, float4  intrins_17, FixedArray<float, 8>  dist_coeffs_29)
{
    float2  _S577 = float2 {p_view_17.x, p_view_17.y};
    float _S578 = p_view_17.z;
    float2  _S579 = _S577 * make_float2 (0.0f);
    float _S580 = _S578 * _S578;
    float2  s_diff_uv_6 = (make_float2 (1.0f, 0.0f) * make_float2 (_S578) - _S579) / make_float2 (_S580);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S581;
    (&_S581)->primal_0 = _S577 / make_float2 (_S578);
    (&_S581)->differential_0 = s_diff_uv_6;
    FixedArray<float, 8>  _S582 = dist_coeffs_29;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S583 = s_fwd_DistThinPrism_distort_0(&_S581, &_S582);
    float fx_7 = intrins_17.x;
    float fy_7 = intrins_17.y;
    float _S584 = _S583.differential_0.y * fy_7;
    Matrix<float, 2, 3>  J_7;
    *&(((&J_7)->rows + (int(0)))->x) = _S583.differential_0.x * fx_7;
    *&(((&J_7)->rows + (int(1)))->x) = _S584;
    float2  s_diff_uv_7 = (make_float2 (0.0f, 1.0f) * make_float2 (_S578) - _S579) / make_float2 (_S580);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S585;
    (&_S585)->primal_0 = _S577 / make_float2 (_S578);
    (&_S585)->differential_0 = s_diff_uv_7;
    FixedArray<float, 8>  _S586 = dist_coeffs_29;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S587 = s_fwd_DistThinPrism_distort_0(&_S585, &_S586);
    float _S588 = _S587.differential_0.y * fy_7;
    *&(((&J_7)->rows + (int(0)))->y) = _S587.differential_0.x * fx_7;
    *&(((&J_7)->rows + (int(1)))->y) = _S588;
    float2  s_diff_uv_8 = (make_float2 (0.0f, 0.0f) * make_float2 (_S578) - _S577) / make_float2 (_S580);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S589;
    (&_S589)->primal_0 = _S577 / make_float2 (_S578);
    (&_S589)->differential_0 = s_diff_uv_8;
    FixedArray<float, 8>  _S590 = dist_coeffs_29;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S591 = s_fwd_DistThinPrism_distort_0(&_S589, &_S590);
    float _S592 = _S591.differential_0.y * fy_7;
    *&(((&J_7)->rows + (int(0)))->z) = _S591.differential_0.x * fx_7;
    *&(((&J_7)->rows + (int(1)))->z) = _S592;
    return J_7;
}

inline __device__ Matrix<float, 2, 3>  fisheye_proj_jac_prism(float3  p_view_18, float4  intrins_18, FixedArray<float, 8>  dist_coeffs_30)
{
    Matrix<float, 2, 3>  J_8;
    float2  _S593 = float2 {p_view_18.x, p_view_18.y};
    float2  _S594 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S595;
    (&_S595)->primal_0 = _S593;
    (&_S595)->differential_0 = _S594;
    DiffPair_float_0 _S596 = s_fwd_length_impl_0(&_S595);
    float _S597 = p_view_18.z;
    DiffPair_float_0 _S598;
    (&_S598)->primal_0 = _S596.primal_0;
    (&_S598)->differential_0 = _S596.differential_0;
    DiffPair_float_0 _S599;
    (&_S599)->primal_0 = _S597;
    (&_S599)->differential_0 = 0.0f;
    DiffPair_float_0 _S600 = _d_atan2_0(&_S598, &_S599);
    float k_12;
    float s_diff_k_4;
    if((_S600.primal_0) < 0.00100000004749745f)
    {
        float _S601 = _S600.differential_0 * _S600.primal_0;
        float _S602 = 1.0f - _S600.primal_0 * _S600.primal_0 / 3.0f;
        float _S603 = ((0.0f - (_S601 + _S601) * 0.3333333432674408f) * _S597 - _S602 * 0.0f) / (_S597 * _S597);
        k_12 = _S602 / _S597;
        s_diff_k_4 = _S603;
    }
    else
    {
        float _S604 = (_S600.differential_0 * _S596.primal_0 - _S600.primal_0 * _S596.differential_0) / (_S596.primal_0 * _S596.primal_0);
        k_12 = _S600.primal_0 / _S596.primal_0;
        s_diff_k_4 = _S604;
    }
    float2  _S605 = _S593 * make_float2 (k_12);
    float2  _S606 = _S594 * make_float2 (k_12) + make_float2 (s_diff_k_4) * _S593;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S607;
    (&_S607)->primal_0 = _S605;
    (&_S607)->differential_0 = _S606;
    FixedArray<float, 8>  _S608 = dist_coeffs_30;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S609 = s_fwd_DistThinPrism_distort_0(&_S607, &_S608);
    float fx_8 = intrins_18.x;
    float fy_8 = intrins_18.y;
    float _S610 = _S609.differential_0.y * fy_8;
    *&(((&J_8)->rows + (int(0)))->x) = _S609.differential_0.x * fx_8;
    *&(((&J_8)->rows + (int(1)))->x) = _S610;
    float2  _S611 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S612;
    (&_S612)->primal_0 = _S593;
    (&_S612)->differential_0 = _S611;
    DiffPair_float_0 _S613 = s_fwd_length_impl_0(&_S612);
    DiffPair_float_0 _S614;
    (&_S614)->primal_0 = _S613.primal_0;
    (&_S614)->differential_0 = _S613.differential_0;
    DiffPair_float_0 _S615;
    (&_S615)->primal_0 = _S597;
    (&_S615)->differential_0 = 0.0f;
    DiffPair_float_0 _S616 = _d_atan2_0(&_S614, &_S615);
    if((_S616.primal_0) < 0.00100000004749745f)
    {
        float _S617 = _S616.differential_0 * _S616.primal_0;
        float _S618 = 1.0f - _S616.primal_0 * _S616.primal_0 / 3.0f;
        float _S619 = ((0.0f - (_S617 + _S617) * 0.3333333432674408f) * _S597 - _S618 * 0.0f) / (_S597 * _S597);
        k_12 = _S618 / _S597;
        s_diff_k_4 = _S619;
    }
    else
    {
        float _S620 = (_S616.differential_0 * _S613.primal_0 - _S616.primal_0 * _S613.differential_0) / (_S613.primal_0 * _S613.primal_0);
        k_12 = _S616.primal_0 / _S613.primal_0;
        s_diff_k_4 = _S620;
    }
    float2  _S621 = _S593 * make_float2 (k_12);
    float2  _S622 = _S611 * make_float2 (k_12) + make_float2 (s_diff_k_4) * _S593;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S623;
    (&_S623)->primal_0 = _S621;
    (&_S623)->differential_0 = _S622;
    FixedArray<float, 8>  _S624 = dist_coeffs_30;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S625 = s_fwd_DistThinPrism_distort_0(&_S623, &_S624);
    float _S626 = _S625.differential_0.y * fy_8;
    *&(((&J_8)->rows + (int(0)))->y) = _S625.differential_0.x * fx_8;
    *&(((&J_8)->rows + (int(1)))->y) = _S626;
    float2  _S627 = make_float2 (0.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S628;
    (&_S628)->primal_0 = _S593;
    (&_S628)->differential_0 = _S627;
    DiffPair_float_0 _S629 = s_fwd_length_impl_0(&_S628);
    DiffPair_float_0 _S630;
    (&_S630)->primal_0 = _S629.primal_0;
    (&_S630)->differential_0 = _S629.differential_0;
    DiffPair_float_0 _S631;
    (&_S631)->primal_0 = _S597;
    (&_S631)->differential_0 = 1.0f;
    DiffPair_float_0 _S632 = _d_atan2_0(&_S630, &_S631);
    if((_S632.primal_0) < 0.00100000004749745f)
    {
        float _S633 = _S632.differential_0 * _S632.primal_0;
        float _S634 = 1.0f - _S632.primal_0 * _S632.primal_0 / 3.0f;
        float _S635 = ((0.0f - (_S633 + _S633) * 0.3333333432674408f) * _S597 - _S634) / (_S597 * _S597);
        k_12 = _S634 / _S597;
        s_diff_k_4 = _S635;
    }
    else
    {
        float _S636 = (_S632.differential_0 * _S629.primal_0 - _S632.primal_0 * _S629.differential_0) / (_S629.primal_0 * _S629.primal_0);
        k_12 = _S632.primal_0 / _S629.primal_0;
        s_diff_k_4 = _S636;
    }
    float2  _S637 = _S593 * make_float2 (k_12);
    float2  _S638 = _S627 * make_float2 (k_12) + make_float2 (s_diff_k_4) * _S593;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S639;
    (&_S639)->primal_0 = _S637;
    (&_S639)->differential_0 = _S638;
    FixedArray<float, 8>  _S640 = dist_coeffs_30;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S641 = s_fwd_DistThinPrism_distort_0(&_S639, &_S640);
    float _S642 = _S641.differential_0.y * fy_8;
    *&(((&J_8)->rows + (int(0)))->z) = _S641.differential_0.x * fx_8;
    *&(((&J_8)->rows + (int(1)))->z) = _S642;
    return J_8;
}

inline __device__ Matrix<float, 2, 3>  equisolid_proj_jac_prism(float3  p_view_19, float4  intrins_19, FixedArray<float, 8>  dist_coeffs_31)
{
    Matrix<float, 2, 3>  J_9;
    float2  _S643 = float2 {p_view_19.x, p_view_19.y};
    float2  _S644 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S645;
    (&_S645)->primal_0 = _S643;
    (&_S645)->differential_0 = _S644;
    DiffPair_float_0 _S646 = s_fwd_length_impl_0(&_S645);
    float _S647 = p_view_19.z;
    DiffPair_float_0 _S648;
    (&_S648)->primal_0 = _S646.primal_0;
    (&_S648)->differential_0 = _S646.differential_0;
    DiffPair_float_0 _S649;
    (&_S649)->primal_0 = _S647;
    (&_S649)->differential_0 = 0.0f;
    DiffPair_float_0 _S650 = _d_atan2_0(&_S648, &_S649);
    float k_13;
    float s_diff_k_5;
    if((_S646.primal_0) < 9.99999997475242708e-07f)
    {
        float _S651 = _S650.differential_0 * _S650.primal_0;
        float _S652 = 1.0f - _S650.primal_0 * _S650.primal_0 / 24.0f;
        float _S653 = ((0.0f - (_S651 + _S651) * 0.0416666679084301f) * _S647 - _S652 * 0.0f) / (_S647 * _S647);
        k_13 = _S652 / _S647;
        s_diff_k_5 = _S653;
    }
    else
    {
        float _S654 = _S650.differential_0 * 0.5f;
        DiffPair_float_0 _S655;
        (&_S655)->primal_0 = 0.5f * _S650.primal_0;
        (&_S655)->differential_0 = _S654;
        DiffPair_float_0 _S656 = _d_sin_0(&_S655);
        float _S657 = 2.0f * _S656.primal_0;
        float _S658 = (_S656.differential_0 * 2.0f * _S646.primal_0 - _S657 * _S646.differential_0) / (_S646.primal_0 * _S646.primal_0);
        k_13 = _S657 / _S646.primal_0;
        s_diff_k_5 = _S658;
    }
    float2  _S659 = _S643 * make_float2 (k_13);
    float2  _S660 = _S644 * make_float2 (k_13) + make_float2 (s_diff_k_5) * _S643;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S661;
    (&_S661)->primal_0 = _S659;
    (&_S661)->differential_0 = _S660;
    FixedArray<float, 8>  _S662 = dist_coeffs_31;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S663 = s_fwd_DistThinPrism_distort_0(&_S661, &_S662);
    float fx_9 = intrins_19.x;
    float fy_9 = intrins_19.y;
    float _S664 = _S663.differential_0.y * fy_9;
    *&(((&J_9)->rows + (int(0)))->x) = _S663.differential_0.x * fx_9;
    *&(((&J_9)->rows + (int(1)))->x) = _S664;
    float2  _S665 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S666;
    (&_S666)->primal_0 = _S643;
    (&_S666)->differential_0 = _S665;
    DiffPair_float_0 _S667 = s_fwd_length_impl_0(&_S666);
    DiffPair_float_0 _S668;
    (&_S668)->primal_0 = _S667.primal_0;
    (&_S668)->differential_0 = _S667.differential_0;
    DiffPair_float_0 _S669;
    (&_S669)->primal_0 = _S647;
    (&_S669)->differential_0 = 0.0f;
    DiffPair_float_0 _S670 = _d_atan2_0(&_S668, &_S669);
    if((_S667.primal_0) < 9.99999997475242708e-07f)
    {
        float _S671 = _S670.differential_0 * _S670.primal_0;
        float _S672 = 1.0f - _S670.primal_0 * _S670.primal_0 / 24.0f;
        float _S673 = ((0.0f - (_S671 + _S671) * 0.0416666679084301f) * _S647 - _S672 * 0.0f) / (_S647 * _S647);
        k_13 = _S672 / _S647;
        s_diff_k_5 = _S673;
    }
    else
    {
        float _S674 = _S670.differential_0 * 0.5f;
        DiffPair_float_0 _S675;
        (&_S675)->primal_0 = 0.5f * _S670.primal_0;
        (&_S675)->differential_0 = _S674;
        DiffPair_float_0 _S676 = _d_sin_0(&_S675);
        float _S677 = 2.0f * _S676.primal_0;
        float _S678 = (_S676.differential_0 * 2.0f * _S667.primal_0 - _S677 * _S667.differential_0) / (_S667.primal_0 * _S667.primal_0);
        k_13 = _S677 / _S667.primal_0;
        s_diff_k_5 = _S678;
    }
    float2  _S679 = _S643 * make_float2 (k_13);
    float2  _S680 = _S665 * make_float2 (k_13) + make_float2 (s_diff_k_5) * _S643;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S681;
    (&_S681)->primal_0 = _S679;
    (&_S681)->differential_0 = _S680;
    FixedArray<float, 8>  _S682 = dist_coeffs_31;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S683 = s_fwd_DistThinPrism_distort_0(&_S681, &_S682);
    float _S684 = _S683.differential_0.y * fy_9;
    *&(((&J_9)->rows + (int(0)))->y) = _S683.differential_0.x * fx_9;
    *&(((&J_9)->rows + (int(1)))->y) = _S684;
    float2  _S685 = make_float2 (0.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S686;
    (&_S686)->primal_0 = _S643;
    (&_S686)->differential_0 = _S685;
    DiffPair_float_0 _S687 = s_fwd_length_impl_0(&_S686);
    DiffPair_float_0 _S688;
    (&_S688)->primal_0 = _S687.primal_0;
    (&_S688)->differential_0 = _S687.differential_0;
    DiffPair_float_0 _S689;
    (&_S689)->primal_0 = _S647;
    (&_S689)->differential_0 = 1.0f;
    DiffPair_float_0 _S690 = _d_atan2_0(&_S688, &_S689);
    if((_S687.primal_0) < 9.99999997475242708e-07f)
    {
        float _S691 = _S690.differential_0 * _S690.primal_0;
        float _S692 = 1.0f - _S690.primal_0 * _S690.primal_0 / 24.0f;
        float _S693 = ((0.0f - (_S691 + _S691) * 0.0416666679084301f) * _S647 - _S692) / (_S647 * _S647);
        k_13 = _S692 / _S647;
        s_diff_k_5 = _S693;
    }
    else
    {
        float _S694 = _S690.differential_0 * 0.5f;
        DiffPair_float_0 _S695;
        (&_S695)->primal_0 = 0.5f * _S690.primal_0;
        (&_S695)->differential_0 = _S694;
        DiffPair_float_0 _S696 = _d_sin_0(&_S695);
        float _S697 = 2.0f * _S696.primal_0;
        float _S698 = (_S696.differential_0 * 2.0f * _S687.primal_0 - _S697 * _S687.differential_0) / (_S687.primal_0 * _S687.primal_0);
        k_13 = _S697 / _S687.primal_0;
        s_diff_k_5 = _S698;
    }
    float2  _S699 = _S643 * make_float2 (k_13);
    float2  _S700 = _S685 * make_float2 (k_13) + make_float2 (s_diff_k_5) * _S643;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S701;
    (&_S701)->primal_0 = _S699;
    (&_S701)->differential_0 = _S700;
    FixedArray<float, 8>  _S702 = dist_coeffs_31;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S703 = s_fwd_DistThinPrism_distort_0(&_S701, &_S702);
    float _S704 = _S703.differential_0.y * fy_9;
    *&(((&J_9)->rows + (int(0)))->z) = _S703.differential_0.x * fx_9;
    *&(((&J_9)->rows + (int(1)))->z) = _S704;
    return J_9;
}

inline __device__ float2  distort_point_prism(float2  uv_30, int camera_model_8, FixedArray<float, 8>  dist_coeffs_32)
{
    float2  _S705;
    for(;;)
    {
        if(camera_model_8 == int(3))
        {
            _S705 = uv_30;
            break;
        }
        float k_14;
        if(camera_model_8 == int(1))
        {
            float r_27 = length_0(uv_30);
            float theta_10 = (F32_atan((r_27)));
            if(r_27 < 0.00100000004749745f)
            {
                k_14 = 1.0f - theta_10 * theta_10 / 6.0f;
            }
            else
            {
                k_14 = theta_10 / r_27;
            }
            _S705 = uv_30 * make_float2 (k_14);
        }
        else
        {
            if(camera_model_8 == int(2))
            {
                float r_28 = length_0(uv_30);
                float theta_11 = (F32_atan((r_28)));
                if(r_28 < 0.00100000004749745f)
                {
                    k_14 = 1.0f - theta_11 * theta_11 / 24.0f;
                }
                else
                {
                    k_14 = 2.0f * (F32_sin((0.5f * theta_11))) / r_28;
                }
                _S705 = uv_30 * make_float2 (k_14);
            }
            else
            {
                _S705 = uv_30;
            }
        }
        FixedArray<float, 8>  _S706 = dist_coeffs_32;
        float2  _S707 = DistThinPrism_distort_0(_S705, &_S706);
        _S705 = _S707;
        break;
    }
    return _S705;
}

inline __device__ bool undistort_point_prism(float2  uv_31, int camera_model_9, FixedArray<float, 8>  dist_coeffs_33, float2  * uv_undist_5)
{
    bool _S708;
    for(;;)
    {
        *uv_undist_5 = make_float2 (0.0f);
        if(camera_model_9 == int(3))
        {
            float lon_4 = uv_31.x;
            float lat_6 = uv_31.y;
            float cl_6 = (F32_cos((lat_6)));
            *uv_undist_5 = make_float2 (cl_6 * (F32_sin((lon_4))), (F32_sin((lat_6)))) / make_float2 ((F32_max((cl_6 * (F32_cos((lon_4)))), (9.999999960041972e-13f))));
            _S708 = true;
            break;
        }
        FixedArray<float, 8>  _S709 = dist_coeffs_33;
        float2  uv_u_6;
        bool _S710 = undistort_point_2(uv_31, &_S709, int(8), &uv_u_6);
        if(!_S710)
        {
            _S708 = false;
            break;
        }
        float2  _S711 = uv_u_6;
        float3  raydir_6;
        if(camera_model_9 == int(1))
        {
            float r_29 = length_0(_S711);
            float s_6;
            if(r_29 < 0.00100000004749745f)
            {
                s_6 = 1.0f - r_29 * r_29 / 6.0f;
            }
            else
            {
                s_6 = (F32_sin((r_29))) / r_29;
            }
            raydir_6 = make_float3 ((_S711 * make_float2 (s_6)).x, (_S711 * make_float2 (s_6)).y, (F32_cos((r_29))));
        }
        else
        {
            if(camera_model_9 == int(2))
            {
                float r_30 = length_0(_S711);
                raydir_6 = make_float3 ((_S711 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_30 * r_30)))))))).x, (_S711 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_30 * r_30)))))))).y, 1.0f - 0.5f * r_30 * r_30);
            }
            else
            {
                raydir_6 = make_float3 (_S711.x, _S711.y, 1.0f);
            }
        }
        *uv_undist_5 = float2 {raydir_6.x, raydir_6.y} / make_float2 ((F32_max((raydir_6.z), (9.999999960041972e-13f))));
        _S708 = true;
        break;
    }
    return _S708;
}

inline __device__ bool unproject_point_prism(float2  uv_32, int camera_model_10, FixedArray<float, 8>  dist_coeffs_34, float3  * raydir_7)
{
    bool _S712;
    for(;;)
    {
        if(camera_model_10 == int(4))
        {
            *raydir_7 = make_float3 (0.0f, 0.0f, 1.0f);
            _S712 = true;
            break;
        }
        int3  _S713 = make_int3 (int(0));
        float3  _S714 = make_float3 ((float)_S713.x, (float)_S713.y, (float)_S713.z);
        *raydir_7 = _S714;
        if(camera_model_10 == int(3))
        {
            float lon_5 = uv_32.x;
            float lat_7 = uv_32.y;
            float cl_7 = (F32_cos((lat_7)));
            *raydir_7 = make_float3 (cl_7 * (F32_sin((lon_5))), (F32_sin((lat_7))), cl_7 * (F32_cos((lon_5))));
            _S712 = true;
            break;
        }
        FixedArray<float, 8>  _S715 = dist_coeffs_34;
        float2  uv_u_7;
        bool _S716 = undistort_point_2(uv_32, &_S715, int(8), &uv_u_7);
        if(!_S716)
        {
            _S712 = false;
            break;
        }
        float2  _S717 = uv_u_7;
        if(camera_model_10 == int(1))
        {
            float r_31 = length_0(_S717);
            float s_7;
            if(r_31 < 0.00100000004749745f)
            {
                s_7 = 1.0f - r_31 * r_31 / 6.0f;
            }
            else
            {
                s_7 = (F32_sin((r_31))) / r_31;
            }
            *raydir_7 = make_float3 ((_S717 * make_float2 (s_7)).x, (_S717 * make_float2 (s_7)).y, (F32_cos((r_31))));
        }
        else
        {
            if(camera_model_10 == int(2))
            {
                float r_32 = length_0(_S717);
                *raydir_7 = make_float3 ((_S717 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_32 * r_32)))))))).x, (_S717 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_32 * r_32)))))))).y, 1.0f - 0.5f * r_32 * r_32);
            }
            else
            {
                *raydir_7 = make_float3 (_S717.x, _S717.y, 1.0f);
            }
        }
        _S712 = true;
        break;
    }
    return _S712;
}

inline __device__ bool generate_ray_prism(float2  uv_33, int camera_model_11, FixedArray<float, 8>  dist_coeffs_35, float3  * raydir_8)
{
    bool _S718;
    for(;;)
    {
        if(camera_model_11 == int(4))
        {
            *raydir_8 = make_float3 (0.0f, 0.0f, 1.0f);
            _S718 = true;
            break;
        }
        if(camera_model_11 == int(3))
        {
            float _S719 = uv_33.x;
            if((F32_abs((_S719))) > 3.14159274101257324f)
            {
                _S718 = true;
            }
            else
            {
                _S718 = (F32_abs((uv_33.y))) > 1.57079637050628662f;
            }
            if(_S718)
            {
                int3  _S720 = make_int3 (int(0));
                float3  _S721 = make_float3 ((float)_S720.x, (float)_S720.y, (float)_S720.z);
                *raydir_8 = _S721;
                _S718 = false;
                break;
            }
            float lat_8 = uv_33.y;
            float cl_8 = (F32_cos((lat_8)));
            *raydir_8 = make_float3 (cl_8 * (F32_sin((_S719))), (F32_sin((lat_8))), cl_8 * (F32_cos((_S719))));
            _S718 = true;
            break;
        }
        FixedArray<float, 8>  _S722 = dist_coeffs_35;
        float2  uv_u_8;
        bool _S723 = undistort_point_2(uv_33, &_S722, int(8), &uv_u_8);
        if(!_S723)
        {
            int3  _S724 = make_int3 (int(0));
            float3  _S725 = make_float3 ((float)_S724.x, (float)_S724.y, (float)_S724.z);
            *raydir_8 = _S725;
            _S718 = false;
            break;
        }
        float2  _S726 = uv_u_8;
        if(camera_model_11 == int(1))
        {
            float r_33 = length_0(_S726);
            if(r_33 >= 3.14159274101257324f)
            {
                int3  _S727 = make_int3 (int(0));
                float3  _S728 = make_float3 ((float)_S727.x, (float)_S727.y, (float)_S727.z);
                *raydir_8 = _S728;
                _S718 = false;
                break;
            }
            float s_8;
            if(r_33 < 0.00100000004749745f)
            {
                s_8 = 1.0f - r_33 * r_33 / 6.0f;
            }
            else
            {
                s_8 = (F32_sin((r_33))) / r_33;
            }
            *raydir_8 = make_float3 ((_S726 * make_float2 (s_8)).x, (_S726 * make_float2 (s_8)).y, (F32_cos((r_33))));
        }
        else
        {
            if(camera_model_11 == int(2))
            {
                float r_34 = length_0(_S726);
                if(r_34 >= 2.0f)
                {
                    int3  _S729 = make_int3 (int(0));
                    float3  _S730 = make_float3 ((float)_S729.x, (float)_S729.y, (float)_S729.z);
                    *raydir_8 = _S730;
                    _S718 = false;
                    break;
                }
                *raydir_8 = make_float3 ((_S726 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_34 * r_34)))))))).x, (_S726 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_34 * r_34)))))))).y, 1.0f - 0.5f * r_34 * r_34);
            }
            else
            {
                *raydir_8 = make_float3 (_S726.x, _S726.y, 1.0f);
            }
        }
        *raydir_8 = normalize_0(*raydir_8);
        _S718 = true;
        break;
    }
    return _S718;
}

inline __device__ void _d_mul_1(DiffPair_vectorx3Cfloatx2C3x3E_0 * left_4, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * right_4, float3  dOut_5)
{
    float _S731 = (*right_4).primal_0.rows[int(0)].x * dOut_5.x;
    Matrix<float, 3, 3>  right_d_result_2;
    *&(((&right_d_result_2)->rows + (int(0)))->x) = (*left_4).primal_0.x * dOut_5.x;
    float sum_10 = _S731 + (*right_4).primal_0.rows[int(0)].y * dOut_5.y;
    *&(((&right_d_result_2)->rows + (int(0)))->y) = (*left_4).primal_0.x * dOut_5.y;
    float sum_11 = sum_10 + (*right_4).primal_0.rows[int(0)].z * dOut_5.z;
    *&(((&right_d_result_2)->rows + (int(0)))->z) = (*left_4).primal_0.x * dOut_5.z;
    float3  left_d_result_2;
    *&((&left_d_result_2)->x) = sum_11;
    float _S732 = (*right_4).primal_0.rows[int(1)].x * dOut_5.x;
    *&(((&right_d_result_2)->rows + (int(1)))->x) = (*left_4).primal_0.y * dOut_5.x;
    float sum_12 = _S732 + (*right_4).primal_0.rows[int(1)].y * dOut_5.y;
    *&(((&right_d_result_2)->rows + (int(1)))->y) = (*left_4).primal_0.y * dOut_5.y;
    float sum_13 = sum_12 + (*right_4).primal_0.rows[int(1)].z * dOut_5.z;
    *&(((&right_d_result_2)->rows + (int(1)))->z) = (*left_4).primal_0.y * dOut_5.z;
    *&((&left_d_result_2)->y) = sum_13;
    float _S733 = (*right_4).primal_0.rows[int(2)].x * dOut_5.x;
    *&(((&right_d_result_2)->rows + (int(2)))->x) = (*left_4).primal_0.z * dOut_5.x;
    float sum_14 = _S733 + (*right_4).primal_0.rows[int(2)].y * dOut_5.y;
    *&(((&right_d_result_2)->rows + (int(2)))->y) = (*left_4).primal_0.z * dOut_5.y;
    float sum_15 = sum_14 + (*right_4).primal_0.rows[int(2)].z * dOut_5.z;
    *&(((&right_d_result_2)->rows + (int(2)))->z) = (*left_4).primal_0.z * dOut_5.z;
    *&((&left_d_result_2)->z) = sum_15;
    left_4->primal_0 = (*left_4).primal_0;
    left_4->differential_0 = left_d_result_2;
    right_4->primal_0 = (*right_4).primal_0;
    right_4->differential_0 = right_d_result_2;
    return;
}

inline __device__ float3  mul_3(float3  left_5, Matrix<float, 3, 3>  right_5)
{
    float3  result_10;
    int j_1 = int(0);
    for(;;)
    {
        if(j_1 < int(3))
        {
        }
        else
        {
            break;
        }
        int i_6 = int(0);
        float sum_16 = 0.0f;
        for(;;)
        {
            if(i_6 < int(3))
            {
            }
            else
            {
                break;
            }
            float sum_17 = sum_16 + _slang_vector_get_element(left_5, i_6) * _slang_vector_get_element(right_5.rows[i_6], j_1);
            i_6 = i_6 + int(1);
            sum_16 = sum_17;
        }
        *_slang_vector_get_element_ptr(&result_10, j_1) = sum_16;
        j_1 = j_1 + int(1);
    }
    return result_10;
}

inline __device__ float3  transform_ray_o(Matrix<float, 3, 3>  R_0, float3  t_0)
{
    return - mul_3(t_0, R_0);
}

inline __device__ float3  transform_ray_d(Matrix<float, 3, 3>  R_1, float3  raydir_9)
{
    return mul_3(raydir_9, R_1);
}

inline __device__ float3  undo_transform_ray_d(Matrix<float, 3, 3>  R_2, float3  raydir_10)
{
    return mul_3(raydir_10, transpose_0(R_2));
}

inline __device__ void s_bwd_prop_mul_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S734, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S735, float3  _S736)
{
    _d_mul_1(_S734, _S735, _S736);
    return;
}

inline __device__ void s_bwd_prop_transform_ray_o_0(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * dpR_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpt_0, float3  _s_dOut_0)
{
    float3  _S737 = - _s_dOut_0;
    float3  _S738 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S739;
    (&_S739)->primal_0 = (*dpt_0).primal_0;
    (&_S739)->differential_0 = _S738;
    Matrix<float, 3, 3>  _S740 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S741;
    (&_S741)->primal_0 = (*dpR_0).primal_0;
    (&_S741)->differential_0 = _S740;
    s_bwd_prop_mul_0(&_S739, &_S741, _S737);
    dpt_0->primal_0 = (*dpt_0).primal_0;
    dpt_0->differential_0 = _S739.differential_0;
    dpR_0->primal_0 = (*dpR_0).primal_0;
    dpR_0->differential_0 = _S741.differential_0;
    return;
}

inline __device__ void s_bwd_transform_ray_o_0(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S742, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S743, float3  _S744)
{
    s_bwd_prop_transform_ray_o_0(_S742, _S743, _S744);
    return;
}

inline __device__ void transform_ray_o_vjp(Matrix<float, 3, 3>  R_3, float3  t_1, float3  v_ray_o_0, Matrix<float, 3, 3>  * v_R_0, float3  * v_t_0)
{
    Matrix<float, 3, 3>  _S745 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 dp_R_0;
    (&dp_R_0)->primal_0 = R_3;
    (&dp_R_0)->differential_0 = _S745;
    float3  _S746 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_t_0;
    (&dp_t_0)->primal_0 = t_1;
    (&dp_t_0)->differential_0 = _S746;
    s_bwd_transform_ray_o_0(&dp_R_0, &dp_t_0, v_ray_o_0);
    *v_R_0 = dp_R_0.differential_0;
    *v_t_0 = dp_t_0.differential_0;
    return;
}

inline __device__ void s_bwd_prop_transform_ray_d_0(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * dpR_1, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpraydir_0, float3  _s_dOut_1)
{
    float3  _S747 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S748;
    (&_S748)->primal_0 = (*dpraydir_0).primal_0;
    (&_S748)->differential_0 = _S747;
    Matrix<float, 3, 3>  _S749 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S750;
    (&_S750)->primal_0 = (*dpR_1).primal_0;
    (&_S750)->differential_0 = _S749;
    s_bwd_prop_mul_0(&_S748, &_S750, _s_dOut_1);
    dpraydir_0->primal_0 = (*dpraydir_0).primal_0;
    dpraydir_0->differential_0 = _S748.differential_0;
    dpR_1->primal_0 = (*dpR_1).primal_0;
    dpR_1->differential_0 = _S750.differential_0;
    return;
}

inline __device__ void s_bwd_transform_ray_d_0(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S751, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S752, float3  _S753)
{
    s_bwd_prop_transform_ray_d_0(_S751, _S752, _S753);
    return;
}

inline __device__ void transform_ray_d_vjp(Matrix<float, 3, 3>  R_4, float3  raydir_11, float3  v_ray_d_0, Matrix<float, 3, 3>  * v_R_1, float3  * v_raydir_0)
{
    Matrix<float, 3, 3>  _S754 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 dp_R_1;
    (&dp_R_1)->primal_0 = R_4;
    (&dp_R_1)->differential_0 = _S754;
    float3  _S755 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_raydir_0;
    (&dp_raydir_0)->primal_0 = raydir_11;
    (&dp_raydir_0)->differential_0 = _S755;
    s_bwd_transform_ray_d_0(&dp_R_1, &dp_raydir_0, v_ray_d_0);
    *v_R_1 = dp_R_1.differential_0;
    *v_raydir_0 = dp_raydir_0.differential_0;
    return;
}

inline __device__ void _d_exp_0(DiffPair_float_0 * dpx_7, float dOut_6)
{
    float _S756 = (F32_exp(((*dpx_7).primal_0))) * dOut_6;
    dpx_7->primal_0 = (*dpx_7).primal_0;
    dpx_7->differential_0 = _S756;
    return;
}

inline __device__ float3  exp_0(float3  x_12)
{
    float3  result_11;
    int i_7 = int(0);
    for(;;)
    {
        if(i_7 < int(3))
        {
        }
        else
        {
            break;
        }
        *_slang_vector_get_element_ptr(&result_11, i_7) = (F32_exp((_slang_vector_get_element(x_12, i_7))));
        i_7 = i_7 + int(1);
    }
    return result_11;
}

inline __device__ void _d_exp_vector_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_8, float3  dOut_7)
{
    float3  _S757 = exp_0((*dpx_8).primal_0) * dOut_7;
    dpx_8->primal_0 = (*dpx_8).primal_0;
    dpx_8->differential_0 = _S757;
    return;
}

inline __device__ Matrix<float, 3, 3>  compute_3dgut_iscl_rot(float4  quat_5, float3  scale_4)
{
    float x_13 = quat_5.y;
    float x2_5 = x_13 * x_13;
    float y2_5 = quat_5.z * quat_5.z;
    float z2_5 = quat_5.w * quat_5.w;
    float xy_5 = quat_5.y * quat_5.z;
    float xz_5 = quat_5.y * quat_5.w;
    float yz_5 = quat_5.z * quat_5.w;
    float wx_5 = quat_5.x * quat_5.y;
    float wy_5 = quat_5.x * quat_5.z;
    float wz_5 = quat_5.x * quat_5.w;
    float3  _S758 = exp_0(- scale_4);
    return mul_1(makeMatrix<float, 3, 3> (_S758.x, 0.0f, 0.0f, 0.0f, _S758.y, 0.0f, 0.0f, 0.0f, _S758.z), transpose_0(transpose_0(makeMatrix<float, 3, 3> (1.0f - 2.0f * (y2_5 + z2_5), 2.0f * (xy_5 + wz_5), 2.0f * (xz_5 - wy_5), 2.0f * (xy_5 - wz_5), 1.0f - 2.0f * (x2_5 + z2_5), 2.0f * (yz_5 + wx_5), 2.0f * (xz_5 + wy_5), 2.0f * (yz_5 - wx_5), 1.0f - 2.0f * (x2_5 + y2_5)))));
}

struct DiffPair_vectorx3Cfloatx2C4x3E_0
{
    float4  primal_0;
    float4  differential_0;
};

inline __device__ float3  s_primal_ctx_exp_0(float3  _S759)
{
    return exp_0(_S759);
}

inline __device__ void s_bwd_prop_mul_1(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S760, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S761, Matrix<float, 3, 3>  _S762)
{
    mul_0(_S760, _S761, _S762);
    return;
}

inline __device__ void s_bwd_prop_exp_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S763, float3  _S764)
{
    _d_exp_vector_0(_S763, _S764);
    return;
}

inline __device__ void s_bwd_prop_compute_3dgut_iscl_rot_0(DiffPair_vectorx3Cfloatx2C4x3E_0 * dpquat_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpscale_0, Matrix<float, 3, 3>  _s_dOut_2)
{
    float _S765 = (*dpquat_0).primal_0.y;
    float x2_6 = _S765 * _S765;
    float y2_6 = (*dpquat_0).primal_0.z * (*dpquat_0).primal_0.z;
    float z2_6 = (*dpquat_0).primal_0.w * (*dpquat_0).primal_0.w;
    float xy_6 = (*dpquat_0).primal_0.y * (*dpquat_0).primal_0.z;
    float xz_6 = (*dpquat_0).primal_0.y * (*dpquat_0).primal_0.w;
    float yz_6 = (*dpquat_0).primal_0.z * (*dpquat_0).primal_0.w;
    float wx_6 = (*dpquat_0).primal_0.x * (*dpquat_0).primal_0.y;
    float wy_6 = (*dpquat_0).primal_0.x * (*dpquat_0).primal_0.z;
    float wz_6 = (*dpquat_0).primal_0.x * (*dpquat_0).primal_0.w;
    float3  _S766 = - (*dpscale_0).primal_0;
    float3  _S767 = s_primal_ctx_exp_0(_S766);
    Matrix<float, 3, 3>  _S768 = transpose_0(transpose_0(makeMatrix<float, 3, 3> (1.0f - 2.0f * (y2_6 + z2_6), 2.0f * (xy_6 + wz_6), 2.0f * (xz_6 - wy_6), 2.0f * (xy_6 - wz_6), 1.0f - 2.0f * (x2_6 + z2_6), 2.0f * (yz_6 + wx_6), 2.0f * (xz_6 + wy_6), 2.0f * (yz_6 - wx_6), 1.0f - 2.0f * (x2_6 + y2_6))));
    Matrix<float, 3, 3>  _S769 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S770;
    (&_S770)->primal_0 = makeMatrix<float, 3, 3> (_S767.x, 0.0f, 0.0f, 0.0f, _S767.y, 0.0f, 0.0f, 0.0f, _S767.z);
    (&_S770)->differential_0 = _S769;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S771;
    (&_S771)->primal_0 = _S768;
    (&_S771)->differential_0 = _S769;
    s_bwd_prop_mul_1(&_S770, &_S771, _s_dOut_2);
    Matrix<float, 3, 3>  _S772 = transpose_0(_S771.differential_0);
    float3  _S773 = make_float3 (_S770.differential_0.rows[int(0)].x, _S770.differential_0.rows[int(1)].y, _S770.differential_0.rows[int(2)].z);
    float3  _S774 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S775;
    (&_S775)->primal_0 = _S766;
    (&_S775)->differential_0 = _S774;
    s_bwd_prop_exp_0(&_S775, _S773);
    float3  _S776 = - _S775.differential_0;
    Matrix<float, 3, 3>  _S777 = transpose_0(_S772);
    float _S778 = 2.0f * - _S777.rows[int(2)].z;
    float _S779 = 2.0f * _S777.rows[int(2)].y;
    float _S780 = 2.0f * _S777.rows[int(2)].x;
    float _S781 = 2.0f * _S777.rows[int(1)].z;
    float _S782 = 2.0f * - _S777.rows[int(1)].y;
    float _S783 = 2.0f * _S777.rows[int(1)].x;
    float _S784 = 2.0f * _S777.rows[int(0)].z;
    float _S785 = 2.0f * _S777.rows[int(0)].y;
    float _S786 = 2.0f * - _S777.rows[int(0)].x;
    float _S787 = - _S783 + _S785;
    float _S788 = _S780 + - _S784;
    float _S789 = - _S779 + _S781;
    float _S790 = _S779 + _S781;
    float _S791 = _S780 + _S784;
    float _S792 = _S783 + _S785;
    float _S793 = (*dpquat_0).primal_0.w * (_S782 + _S786);
    float _S794 = (*dpquat_0).primal_0.z * (_S778 + _S786);
    float _S795 = (*dpquat_0).primal_0.y * (_S778 + _S782);
    float _S796 = (*dpquat_0).primal_0.x * _S787 + (*dpquat_0).primal_0.z * _S790 + (*dpquat_0).primal_0.y * _S791 + _S793 + _S793;
    float _S797 = (*dpquat_0).primal_0.x * _S788 + (*dpquat_0).primal_0.w * _S790 + (*dpquat_0).primal_0.y * _S792 + _S794 + _S794;
    float _S798 = (*dpquat_0).primal_0.x * _S789 + (*dpquat_0).primal_0.w * _S791 + (*dpquat_0).primal_0.z * _S792 + _S795 + _S795;
    float _S799 = (*dpquat_0).primal_0.w * _S787 + (*dpquat_0).primal_0.z * _S788 + (*dpquat_0).primal_0.y * _S789;
    dpscale_0->primal_0 = (*dpscale_0).primal_0;
    dpscale_0->differential_0 = _S776;
    float4  _S800 = make_float4 (0.0f);
    *&((&_S800)->w) = _S796;
    *&((&_S800)->z) = _S797;
    *&((&_S800)->y) = _S798;
    *&((&_S800)->x) = _S799;
    dpquat_0->primal_0 = (*dpquat_0).primal_0;
    dpquat_0->differential_0 = _S800;
    return;
}

inline __device__ void s_bwd_compute_3dgut_iscl_rot_0(DiffPair_vectorx3Cfloatx2C4x3E_0 * _S801, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S802, Matrix<float, 3, 3>  _S803)
{
    s_bwd_prop_compute_3dgut_iscl_rot_0(_S801, _S802, _S803);
    return;
}

inline __device__ void compute_3dgut_iscl_rot_vjp(float4  quat_6, float3  scale_5, Matrix<float, 3, 3>  v_iscl_rot_0, float4  * v_quat_0, float3  * v_scale_0)
{
    float4  _S804 = make_float4 (0.0f);
    DiffPair_vectorx3Cfloatx2C4x3E_0 dp_quat_0;
    (&dp_quat_0)->primal_0 = quat_6;
    (&dp_quat_0)->differential_0 = _S804;
    float3  _S805 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_scale_0;
    (&dp_scale_0)->primal_0 = scale_5;
    (&dp_scale_0)->differential_0 = _S805;
    s_bwd_compute_3dgut_iscl_rot_0(&dp_quat_0, &dp_scale_0, v_iscl_rot_0);
    *v_quat_0 = dp_quat_0.differential_0;
    *v_scale_0 = dp_scale_0.differential_0;
    return;
}

inline __device__ void _d_cross_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * a_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * b_0, float3  dOut_8)
{
    float _S806 = dOut_8.y;
    float _S807 = dOut_8.z;
    float _S808 = dOut_8.x;
    float _S809 = (*a_0).primal_0.z * _S806 + - (*a_0).primal_0.y * _S807;
    float _S810 = - (*a_0).primal_0.z * _S808 + (*a_0).primal_0.x * _S807;
    float _S811 = (*a_0).primal_0.y * _S808 + - (*a_0).primal_0.x * _S806;
    float3  _S812 = make_float3 (- (*b_0).primal_0.z * _S806 + (*b_0).primal_0.y * _S807, (*b_0).primal_0.z * _S808 + - (*b_0).primal_0.x * _S807, - (*b_0).primal_0.y * _S808 + (*b_0).primal_0.x * _S806);
    a_0->primal_0 = (*a_0).primal_0;
    a_0->differential_0 = _S812;
    float3  _S813 = make_float3 (_S809, _S810, _S811);
    b_0->primal_0 = (*b_0).primal_0;
    b_0->differential_0 = _S813;
    return;
}

inline __device__ float3  cross_0(float3  left_6, float3  right_6)
{
    float _S814 = left_6.y;
    float _S815 = right_6.z;
    float _S816 = left_6.z;
    float _S817 = right_6.y;
    float _S818 = right_6.x;
    float _S819 = left_6.x;
    return make_float3 (_S814 * _S815 - _S816 * _S817, _S816 * _S818 - _S819 * _S815, _S819 * _S817 - _S814 * _S818);
}

inline __device__ float evaluate_alpha_3dgs(float3  mean_0, Matrix<float, 3, 3>  iscl_rot_0, float opacity_0, float3  ray_o_0, float3  ray_d_0)
{
    float3  grd_0 = mul_2(iscl_rot_0, ray_d_0);
    float3  gcrod_0 = cross_0(grd_0, mul_2(iscl_rot_0, ray_o_0 - mean_0));
    return opacity_0 * (F32_exp((-0.5f * dot_1(gcrod_0, gcrod_0) / dot_1(grd_0, grd_0))));
}

inline __device__ float3  s_primal_ctx_mul_0(Matrix<float, 3, 3>  _S820, float3  _S821)
{
    return mul_2(_S820, _S821);
}

inline __device__ float3  s_primal_ctx_cross_0(float3  _S822, float3  _S823)
{
    return cross_0(_S822, _S823);
}

inline __device__ float s_primal_ctx_dot_0(float3  _S824, float3  _S825)
{
    return dot_1(_S824, _S825);
}

inline __device__ float s_primal_ctx_exp_1(float _S826)
{
    return (F32_exp((_S826)));
}

inline __device__ void s_bwd_prop_exp_1(DiffPair_float_0 * _S827, float _S828)
{
    _d_exp_0(_S827, _S828);
    return;
}

inline __device__ void s_bwd_prop_dot_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S829, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S830, float _S831)
{
    _d_dot_1(_S829, _S830, _S831);
    return;
}

inline __device__ void s_bwd_prop_cross_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S832, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S833, float3  _S834)
{
    _d_cross_0(_S832, _S833, _S834);
    return;
}

inline __device__ void s_bwd_prop_mul_2(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S835, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S836, float3  _S837)
{
    _d_mul_0(_S835, _S836, _S837);
    return;
}

inline __device__ void s_bwd_prop_evaluate_alpha_3dgs_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpmean_0, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * dpiscl_rot_0, DiffPair_float_0 * dpopacity_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpray_o_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpray_d_0, float _s_dOut_3)
{
    float3  _S838 = (*dpray_o_0).primal_0 - (*dpmean_0).primal_0;
    float3  _S839 = s_primal_ctx_mul_0((*dpiscl_rot_0).primal_0, _S838);
    float3  _S840 = s_primal_ctx_mul_0((*dpiscl_rot_0).primal_0, (*dpray_d_0).primal_0);
    float3  _S841 = s_primal_ctx_cross_0(_S840, _S839);
    float _S842 = -0.5f * s_primal_ctx_dot_0(_S841, _S841);
    float _S843 = s_primal_ctx_dot_0(_S840, _S840);
    float _S844 = _S842 / _S843;
    float _S845 = _S843 * _S843;
    float _S846 = (*dpopacity_0).primal_0 * _s_dOut_3;
    float _S847 = s_primal_ctx_exp_1(_S844) * _s_dOut_3;
    DiffPair_float_0 _S848;
    (&_S848)->primal_0 = _S844;
    (&_S848)->differential_0 = 0.0f;
    s_bwd_prop_exp_1(&_S848, _S846);
    float _S849 = _S848.differential_0 / _S845;
    float _S850 = _S842 * - _S849;
    float _S851 = _S843 * _S849;
    float3  _S852 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S853;
    (&_S853)->primal_0 = _S840;
    (&_S853)->differential_0 = _S852;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S854;
    (&_S854)->primal_0 = _S840;
    (&_S854)->differential_0 = _S852;
    s_bwd_prop_dot_0(&_S853, &_S854, _S850);
    float _S855 = -0.5f * _S851;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S856;
    (&_S856)->primal_0 = _S841;
    (&_S856)->differential_0 = _S852;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S857;
    (&_S857)->primal_0 = _S841;
    (&_S857)->differential_0 = _S852;
    s_bwd_prop_dot_0(&_S856, &_S857, _S855);
    float3  _S858 = _S857.differential_0 + _S856.differential_0;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S859;
    (&_S859)->primal_0 = _S840;
    (&_S859)->differential_0 = _S852;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S860;
    (&_S860)->primal_0 = _S839;
    (&_S860)->differential_0 = _S852;
    s_bwd_prop_cross_0(&_S859, &_S860, _S858);
    float3  _S861 = _S854.differential_0 + _S853.differential_0 + _S859.differential_0;
    Matrix<float, 3, 3>  _S862 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S863;
    (&_S863)->primal_0 = (*dpiscl_rot_0).primal_0;
    (&_S863)->differential_0 = _S862;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S864;
    (&_S864)->primal_0 = (*dpray_d_0).primal_0;
    (&_S864)->differential_0 = _S852;
    s_bwd_prop_mul_2(&_S863, &_S864, _S861);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S865;
    (&_S865)->primal_0 = (*dpiscl_rot_0).primal_0;
    (&_S865)->differential_0 = _S862;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S866;
    (&_S866)->primal_0 = _S838;
    (&_S866)->differential_0 = _S852;
    s_bwd_prop_mul_2(&_S865, &_S866, _S860.differential_0);
    float3  _S867 = - _S866.differential_0;
    dpray_d_0->primal_0 = (*dpray_d_0).primal_0;
    dpray_d_0->differential_0 = _S864.differential_0;
    dpray_o_0->primal_0 = (*dpray_o_0).primal_0;
    dpray_o_0->differential_0 = _S866.differential_0;
    dpopacity_0->primal_0 = (*dpopacity_0).primal_0;
    dpopacity_0->differential_0 = _S847;
    Matrix<float, 3, 3>  _S868 = _S863.differential_0 + _S865.differential_0;
    dpiscl_rot_0->primal_0 = (*dpiscl_rot_0).primal_0;
    dpiscl_rot_0->differential_0 = _S868;
    dpmean_0->primal_0 = (*dpmean_0).primal_0;
    dpmean_0->differential_0 = _S867;
    return;
}

inline __device__ void s_bwd_evaluate_alpha_3dgs_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S869, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S870, DiffPair_float_0 * _S871, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S872, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S873, float _S874)
{
    s_bwd_prop_evaluate_alpha_3dgs_0(_S869, _S870, _S871, _S872, _S873, _S874);
    return;
}

inline __device__ void evaluate_alpha_3dgs_vjp(float3  mean_1, Matrix<float, 3, 3>  iscl_rot_1, float opacity_1, float3  ray_o_1, float3  ray_d_1, float v_alpha_0, float3  * v_mean_0, Matrix<float, 3, 3>  * v_iscl_rot_1, float * v_opacity_0, float3  * v_ray_o_1, float3  * v_ray_d_1)
{
    float3  _S875 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_mean_0;
    (&dp_mean_0)->primal_0 = mean_1;
    (&dp_mean_0)->differential_0 = _S875;
    Matrix<float, 3, 3>  _S876 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 dp_iscl_rot_0;
    (&dp_iscl_rot_0)->primal_0 = iscl_rot_1;
    (&dp_iscl_rot_0)->differential_0 = _S876;
    DiffPair_float_0 dp_opacity_0;
    (&dp_opacity_0)->primal_0 = opacity_1;
    (&dp_opacity_0)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ray_o_0;
    (&dp_ray_o_0)->primal_0 = ray_o_1;
    (&dp_ray_o_0)->differential_0 = _S875;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ray_d_0;
    (&dp_ray_d_0)->primal_0 = ray_d_1;
    (&dp_ray_d_0)->differential_0 = _S875;
    s_bwd_evaluate_alpha_3dgs_0(&dp_mean_0, &dp_iscl_rot_0, &dp_opacity_0, &dp_ray_o_0, &dp_ray_d_0, v_alpha_0);
    *v_mean_0 = dp_mean_0.differential_0;
    *v_iscl_rot_1 = dp_iscl_rot_0.differential_0;
    *v_opacity_0 = dp_opacity_0.differential_0;
    *v_ray_o_1 = dp_ray_o_0.differential_0;
    *v_ray_d_1 = dp_ray_d_0.differential_0;
    return;
}

inline __device__ void evaluate_color_3dgs(float3  mean_2, Matrix<float, 3, 3>  iscl_rot_2, float opacity_2, float3  rgb_0, float3  ray_o_2, float3  ray_d_2, float3  * out_rgb_0, float * depth_0)
{
    *out_rgb_0 = rgb_0;
    float3  grd_1 = mul_2(iscl_rot_2, ray_d_2);
    *depth_0 = - dot_1(mul_2(iscl_rot_2, ray_o_2 - mean_2), grd_1) / dot_1(grd_1, grd_1);
    return;
}

inline __device__ void s_bwd_prop_evaluate_color_3dgs_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpmean_1, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * dpiscl_rot_1, DiffPair_float_0 * dpopacity_1, DiffPair_vectorx3Cfloatx2C3x3E_0 * dprgb_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpray_o_1, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpray_d_1, float3  dpout_rgb_0, float dpdepth_0)
{
    float3  _S877 = (*dpray_o_1).primal_0 - (*dpmean_1).primal_0;
    float3  _S878 = s_primal_ctx_mul_0((*dpiscl_rot_1).primal_0, _S877);
    float3  _S879 = s_primal_ctx_mul_0((*dpiscl_rot_1).primal_0, (*dpray_d_1).primal_0);
    float _S880 = s_primal_ctx_dot_0(_S879, _S879);
    float _S881 = dpdepth_0 / (_S880 * _S880);
    float _S882 = - s_primal_ctx_dot_0(_S878, _S879) * - _S881;
    float _S883 = _S880 * _S881;
    float3  _S884 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S885;
    (&_S885)->primal_0 = _S879;
    (&_S885)->differential_0 = _S884;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S886;
    (&_S886)->primal_0 = _S879;
    (&_S886)->differential_0 = _S884;
    s_bwd_prop_dot_0(&_S885, &_S886, _S882);
    float _S887 = - _S883;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S888;
    (&_S888)->primal_0 = _S878;
    (&_S888)->differential_0 = _S884;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S889;
    (&_S889)->primal_0 = _S879;
    (&_S889)->differential_0 = _S884;
    s_bwd_prop_dot_0(&_S888, &_S889, _S887);
    float3  _S890 = _S886.differential_0 + _S885.differential_0 + _S889.differential_0;
    Matrix<float, 3, 3>  _S891 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S892;
    (&_S892)->primal_0 = (*dpiscl_rot_1).primal_0;
    (&_S892)->differential_0 = _S891;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S893;
    (&_S893)->primal_0 = (*dpray_d_1).primal_0;
    (&_S893)->differential_0 = _S884;
    s_bwd_prop_mul_2(&_S892, &_S893, _S890);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S894;
    (&_S894)->primal_0 = (*dpiscl_rot_1).primal_0;
    (&_S894)->differential_0 = _S891;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S895;
    (&_S895)->primal_0 = _S877;
    (&_S895)->differential_0 = _S884;
    s_bwd_prop_mul_2(&_S894, &_S895, _S888.differential_0);
    float3  _S896 = - _S895.differential_0;
    dpray_d_1->primal_0 = (*dpray_d_1).primal_0;
    dpray_d_1->differential_0 = _S893.differential_0;
    dpray_o_1->primal_0 = (*dpray_o_1).primal_0;
    dpray_o_1->differential_0 = _S895.differential_0;
    dprgb_0->primal_0 = (*dprgb_0).primal_0;
    dprgb_0->differential_0 = dpout_rgb_0;
    dpopacity_1->primal_0 = (*dpopacity_1).primal_0;
    dpopacity_1->differential_0 = 0.0f;
    Matrix<float, 3, 3>  _S897 = _S892.differential_0 + _S894.differential_0;
    dpiscl_rot_1->primal_0 = (*dpiscl_rot_1).primal_0;
    dpiscl_rot_1->differential_0 = _S897;
    dpmean_1->primal_0 = (*dpmean_1).primal_0;
    dpmean_1->differential_0 = _S896;
    return;
}

inline __device__ void s_bwd_evaluate_color_3dgs_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S898, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S899, DiffPair_float_0 * _S900, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S901, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S902, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S903, float3  _S904, float _S905)
{
    s_bwd_prop_evaluate_color_3dgs_0(_S898, _S899, _S900, _S901, _S902, _S903, _S904, _S905);
    return;
}

inline __device__ void evaluate_color_3dgs_vjp(float3  mean_3, Matrix<float, 3, 3>  iscl_rot_3, float opacity_3, float3  rgb_1, float3  ray_o_3, float3  ray_d_3, float3  v_out_rgb_0, float v_depth_0, float3  * v_mean_1, Matrix<float, 3, 3>  * v_iscl_rot_2, float * v_opacity_1, float3  * v_rgb_0, float3  * v_ray_o_2, float3  * v_ray_d_2)
{
    float3  _S906 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_mean_1;
    (&dp_mean_1)->primal_0 = mean_3;
    (&dp_mean_1)->differential_0 = _S906;
    Matrix<float, 3, 3>  _S907 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 dp_iscl_rot_1;
    (&dp_iscl_rot_1)->primal_0 = iscl_rot_3;
    (&dp_iscl_rot_1)->differential_0 = _S907;
    DiffPair_float_0 dp_opacity_1;
    (&dp_opacity_1)->primal_0 = opacity_3;
    (&dp_opacity_1)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_rgb_0;
    (&dp_rgb_0)->primal_0 = rgb_1;
    (&dp_rgb_0)->differential_0 = _S906;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ray_o_1;
    (&dp_ray_o_1)->primal_0 = ray_o_3;
    (&dp_ray_o_1)->differential_0 = _S906;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ray_d_1;
    (&dp_ray_d_1)->primal_0 = ray_d_3;
    (&dp_ray_d_1)->differential_0 = _S906;
    s_bwd_evaluate_color_3dgs_0(&dp_mean_1, &dp_iscl_rot_1, &dp_opacity_1, &dp_rgb_0, &dp_ray_o_1, &dp_ray_d_1, v_out_rgb_0, v_depth_0);
    *v_mean_1 = dp_mean_1.differential_0;
    *v_iscl_rot_2 = dp_iscl_rot_1.differential_0;
    *v_opacity_1 = dp_opacity_1.differential_0;
    *v_rgb_0 = dp_rgb_0.differential_0;
    *v_ray_o_2 = dp_ray_o_1.differential_0;
    *v_ray_d_2 = dp_ray_d_1.differential_0;
    return;
}

inline __device__ Matrix<float, 3, 3>  compute_2dgs_iscl_rot(float4  quat_7, float3  scale_6)
{
    float x_14 = quat_7.y;
    float x2_7 = x_14 * x_14;
    float y2_7 = quat_7.z * quat_7.z;
    float z2_7 = quat_7.w * quat_7.w;
    float xy_7 = quat_7.y * quat_7.z;
    float xz_7 = quat_7.y * quat_7.w;
    float yz_7 = quat_7.z * quat_7.w;
    float wx_7 = quat_7.x * quat_7.y;
    float wy_7 = quat_7.x * quat_7.z;
    float wz_7 = quat_7.x * quat_7.w;
    return mul_1(makeMatrix<float, 3, 3> ((F32_exp((- scale_6.x))), 0.0f, 0.0f, 0.0f, (F32_exp((- scale_6.y))), 0.0f, 0.0f, 0.0f, 1.0f), transpose_0(transpose_0(makeMatrix<float, 3, 3> (1.0f - 2.0f * (y2_7 + z2_7), 2.0f * (xy_7 + wz_7), 2.0f * (xz_7 - wy_7), 2.0f * (xy_7 - wz_7), 1.0f - 2.0f * (x2_7 + z2_7), 2.0f * (yz_7 + wx_7), 2.0f * (xz_7 + wy_7), 2.0f * (yz_7 - wx_7), 1.0f - 2.0f * (x2_7 + y2_7)))));
}

inline __device__ void s_bwd_prop_compute_2dgs_iscl_rot_0(DiffPair_vectorx3Cfloatx2C4x3E_0 * dpquat_1, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpscale_1, Matrix<float, 3, 3>  _s_dOut_4)
{
    float _S908 = (*dpquat_1).primal_0.y;
    float x2_8 = _S908 * _S908;
    float y2_8 = (*dpquat_1).primal_0.z * (*dpquat_1).primal_0.z;
    float z2_8 = (*dpquat_1).primal_0.w * (*dpquat_1).primal_0.w;
    float xy_8 = (*dpquat_1).primal_0.y * (*dpquat_1).primal_0.z;
    float xz_8 = (*dpquat_1).primal_0.y * (*dpquat_1).primal_0.w;
    float yz_8 = (*dpquat_1).primal_0.z * (*dpquat_1).primal_0.w;
    float wx_8 = (*dpquat_1).primal_0.x * (*dpquat_1).primal_0.y;
    float wy_8 = (*dpquat_1).primal_0.x * (*dpquat_1).primal_0.z;
    float wz_8 = (*dpquat_1).primal_0.x * (*dpquat_1).primal_0.w;
    float _S909 = - (*dpscale_1).primal_0.x;
    float _S910 = - (*dpscale_1).primal_0.y;
    Matrix<float, 3, 3>  _S911 = transpose_0(transpose_0(makeMatrix<float, 3, 3> (1.0f - 2.0f * (y2_8 + z2_8), 2.0f * (xy_8 + wz_8), 2.0f * (xz_8 - wy_8), 2.0f * (xy_8 - wz_8), 1.0f - 2.0f * (x2_8 + z2_8), 2.0f * (yz_8 + wx_8), 2.0f * (xz_8 + wy_8), 2.0f * (yz_8 - wx_8), 1.0f - 2.0f * (x2_8 + y2_8))));
    Matrix<float, 3, 3>  _S912 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S913;
    (&_S913)->primal_0 = makeMatrix<float, 3, 3> (s_primal_ctx_exp_1(_S909), 0.0f, 0.0f, 0.0f, s_primal_ctx_exp_1(_S910), 0.0f, 0.0f, 0.0f, 1.0f);
    (&_S913)->differential_0 = _S912;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S914;
    (&_S914)->primal_0 = _S911;
    (&_S914)->differential_0 = _S912;
    s_bwd_prop_mul_1(&_S913, &_S914, _s_dOut_4);
    Matrix<float, 3, 3>  _S915 = transpose_0(_S914.differential_0);
    DiffPair_float_0 _S916;
    (&_S916)->primal_0 = _S910;
    (&_S916)->differential_0 = 0.0f;
    s_bwd_prop_exp_1(&_S916, _S913.differential_0.rows[int(1)].y);
    float _S917 = - _S916.differential_0;
    DiffPair_float_0 _S918;
    (&_S918)->primal_0 = _S909;
    (&_S918)->differential_0 = 0.0f;
    s_bwd_prop_exp_1(&_S918, _S913.differential_0.rows[int(0)].x);
    Matrix<float, 3, 3>  _S919 = transpose_0(_S915);
    float _S920 = 2.0f * - _S919.rows[int(2)].z;
    float _S921 = 2.0f * _S919.rows[int(2)].y;
    float _S922 = 2.0f * _S919.rows[int(2)].x;
    float _S923 = 2.0f * _S919.rows[int(1)].z;
    float _S924 = 2.0f * - _S919.rows[int(1)].y;
    float _S925 = 2.0f * _S919.rows[int(1)].x;
    float _S926 = 2.0f * _S919.rows[int(0)].z;
    float _S927 = 2.0f * _S919.rows[int(0)].y;
    float _S928 = 2.0f * - _S919.rows[int(0)].x;
    float _S929 = - _S925 + _S927;
    float _S930 = _S922 + - _S926;
    float _S931 = - _S921 + _S923;
    float _S932 = _S921 + _S923;
    float _S933 = _S922 + _S926;
    float _S934 = _S925 + _S927;
    float _S935 = (*dpquat_1).primal_0.w * (_S924 + _S928);
    float _S936 = (*dpquat_1).primal_0.z * (_S920 + _S928);
    float _S937 = (*dpquat_1).primal_0.y * (_S920 + _S924);
    float _S938 = (*dpquat_1).primal_0.x * _S929 + (*dpquat_1).primal_0.z * _S932 + (*dpquat_1).primal_0.y * _S933 + _S935 + _S935;
    float _S939 = (*dpquat_1).primal_0.x * _S930 + (*dpquat_1).primal_0.w * _S932 + (*dpquat_1).primal_0.y * _S934 + _S936 + _S936;
    float _S940 = (*dpquat_1).primal_0.x * _S931 + (*dpquat_1).primal_0.w * _S933 + (*dpquat_1).primal_0.z * _S934 + _S937 + _S937;
    float _S941 = (*dpquat_1).primal_0.w * _S929 + (*dpquat_1).primal_0.z * _S930 + (*dpquat_1).primal_0.y * _S931;
    float3  _S942 = make_float3 (- _S918.differential_0, _S917, 0.0f);
    dpscale_1->primal_0 = (*dpscale_1).primal_0;
    dpscale_1->differential_0 = _S942;
    float4  _S943 = make_float4 (0.0f);
    *&((&_S943)->w) = _S938;
    *&((&_S943)->z) = _S939;
    *&((&_S943)->y) = _S940;
    *&((&_S943)->x) = _S941;
    dpquat_1->primal_0 = (*dpquat_1).primal_0;
    dpquat_1->differential_0 = _S943;
    return;
}

inline __device__ void s_bwd_compute_2dgs_iscl_rot_0(DiffPair_vectorx3Cfloatx2C4x3E_0 * _S944, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S945, Matrix<float, 3, 3>  _S946)
{
    s_bwd_prop_compute_2dgs_iscl_rot_0(_S944, _S945, _S946);
    return;
}

inline __device__ void compute_2dgs_iscl_rot_vjp(float4  quat_8, float3  scale_7, Matrix<float, 3, 3>  v_iscl_rot_3, float4  * v_quat_1, float3  * v_scale_1)
{
    float4  _S947 = make_float4 (0.0f);
    DiffPair_vectorx3Cfloatx2C4x3E_0 dp_quat_1;
    (&dp_quat_1)->primal_0 = quat_8;
    (&dp_quat_1)->differential_0 = _S947;
    float3  _S948 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_scale_1;
    (&dp_scale_1)->primal_0 = scale_7;
    (&dp_scale_1)->differential_0 = _S948;
    s_bwd_compute_2dgs_iscl_rot_0(&dp_quat_1, &dp_scale_1, v_iscl_rot_3);
    *v_quat_1 = dp_quat_1.differential_0;
    *v_scale_1 = dp_scale_1.differential_0;
    return;
}

inline __device__ float evaluate_alpha_2dgs(float3  mean_4, Matrix<float, 3, 3>  iscl_rot_4, float opacity_4, float3  ray_o_4, float3  ray_d_4, float2  pix_size_0)
{
    SurfelHit_0 h_0;
    float3  gro_0 = mul_2(iscl_rot_4, ray_o_4 - mean_4);
    float3  grd_2 = mul_2(iscl_rot_4, ray_d_4);
    float dd_0 = dot_1(ray_d_4, ray_d_4);
    float _S949 = grd_2.z;
    if((_S949 * _S949) > (9.99999997475242708e-07f * dd_0))
    {
        float _S950 = - gro_0.z / _S949;
        (&h_0)->t_hit_0 = _S950;
        float2  uv_34 = float2 {gro_0.x, gro_0.y} + make_float2 (_S950) * float2 {grd_2.x, grd_2.y};
        (&h_0)->g_0 = (F32_exp((-0.5f * dot_0(uv_34, uv_34))));
    }
    else
    {
        (&h_0)->t_hit_0 = 0.0f;
        (&h_0)->g_0 = 0.0f;
    }
    float3  m_1 = mean_4 - ray_o_4;
    float _S951 = dot_1(m_1, ray_d_4) / dd_0;
    (&h_0)->t_mean_0 = _S951;
    float fp_0 = pix_size_0.x + pix_size_0.y * _S951 * (F32_sqrt((dd_0)));
    bool _S952;
    if(_S951 > 0.0f)
    {
        _S952 = fp_0 > 0.0f;
    }
    else
    {
        _S952 = false;
    }
    if(_S952)
    {
        float3  perp_0 = m_1 - make_float3 ((&h_0)->t_mean_0) * ray_d_4;
        (&h_0)->g_lp_0 = (F32_exp((- dot_1(perp_0, perp_0) / (fp_0 * fp_0))));
    }
    else
    {
        (&h_0)->g_lp_0 = 0.0f;
    }
    SurfelHit_0 _S953 = h_0;
    float _S954;
    if((_S953.g_0) >= (_S953.g_lp_0))
    {
        _S954 = _S953.g_0;
    }
    else
    {
        _S954 = _S953.g_lp_0;
    }
    return opacity_4 * _S954;
}

inline __device__ float s_primal_ctx_dot_1(float2  _S955, float2  _S956)
{
    return dot_0(_S955, _S956);
}

inline __device__ float s_primal_ctx_sqrt_0(float _S957)
{
    return (F32_sqrt((_S957)));
}

inline __device__ void s_bwd_prop_sqrt_0(DiffPair_float_0 * _S958, float _S959)
{
    _d_sqrt_0(_S958, _S959);
    return;
}

inline __device__ void s_bwd_prop_dot_1(DiffPair_vectorx3Cfloatx2C2x3E_0 * _S960, DiffPair_vectorx3Cfloatx2C2x3E_0 * _S961, float _S962)
{
    _d_dot_0(_S960, _S961, _S962);
    return;
}

inline __device__ void s_bwd_prop_evaluate_alpha_2dgs_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpmean_2, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * dpiscl_rot_2, DiffPair_float_0 * dpopacity_2, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpray_o_2, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpray_d_2, float2  pix_size_1, float _s_dOut_5)
{
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S963 = *dpmean_2;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S964 = *dpiscl_rot_2;
    DiffPair_float_0 _S965 = *dpopacity_2;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S966 = *dpray_o_2;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S967 = *dpray_d_2;
    float2  _S968 = make_float2 (0.0f);
    float3  _S969 = make_float3 (0.0f);
    float3  _S970 = (*dpray_o_2).primal_0 - (*dpmean_2).primal_0;
    float3  _S971 = s_primal_ctx_mul_0((*dpiscl_rot_2).primal_0, _S970);
    float3  _S972 = s_primal_ctx_mul_0((*dpiscl_rot_2).primal_0, (*dpray_d_2).primal_0);
    float _S973 = s_primal_ctx_dot_0((*dpray_d_2).primal_0, (*dpray_d_2).primal_0);
    float _S974 = _S972.z;
    float _S975 = _S974 * _S974;
    bool _S976 = _S975 > (9.99999997475242708e-07f * _S973);
    SurfelHit_0 h_1;
    float _S977;
    float _S978;
    float _S979;
    float2  _S980;
    float2  _S981;
    float2  _S982;
    if(_S976)
    {
        float _S983 = - _S971.z;
        float _S984 = _S983 / _S974;
        float2  _S985 = make_float2 (_S984);
        float2  _S986 = float2 {_S972.x, _S972.y};
        float2  uv_35 = float2 {_S971.x, _S971.y} + make_float2 (_S984) * _S986;
        float _S987 = -0.5f * s_primal_ctx_dot_1(uv_35, uv_35);
        (&h_1)->g_0 = s_primal_ctx_exp_1(_S987);
        (&h_1)->g_lp_0 = 0.0f;
        (&h_1)->t_hit_0 = _S984;
        (&h_1)->t_mean_0 = 0.0f;
        _S977 = _S987;
        _S980 = uv_35;
        _S981 = _S985;
        _S982 = _S986;
        _S978 = _S975;
        _S979 = _S983;
    }
    else
    {
        (&h_1)->g_0 = 0.0f;
        (&h_1)->g_lp_0 = 0.0f;
        (&h_1)->t_hit_0 = 0.0f;
        (&h_1)->t_mean_0 = 0.0f;
        _S977 = 0.0f;
        _S980 = _S968;
        _S981 = _S968;
        _S982 = _S968;
        _S978 = 0.0f;
        _S979 = 0.0f;
    }
    float3  m_2 = _S963.primal_0 - _S966.primal_0;
    float _S988 = s_primal_ctx_dot_0(m_2, _S967.primal_0);
    float _S989 = _S988 / _S973;
    float3  _S990 = make_float3 (_S989);
    float _S991 = _S973 * _S973;
    (&h_1)->t_mean_0 = _S989;
    float _S992 = pix_size_1.y;
    float _S993 = _S992 * _S989;
    float _S994 = s_primal_ctx_sqrt_0(_S973);
    float fp_1 = pix_size_1.x + _S993 * _S994;
    bool _S995;
    if(_S989 > 0.0f)
    {
        _S995 = fp_1 > 0.0f;
    }
    else
    {
        _S995 = false;
    }
    float _S996;
    float _S997;
    float _S998;
    float _S999;
    float3  _S1000;
    if(_S995)
    {
        float3  perp_1 = m_2 - make_float3 (_S989) * _S967.primal_0;
        float _S1001 = - s_primal_ctx_dot_0(perp_1, perp_1);
        float _S1002 = fp_1 * fp_1;
        float _S1003 = _S1001 / _S1002;
        float _S1004 = _S1002 * _S1002;
        (&h_1)->g_lp_0 = s_primal_ctx_exp_1(_S1003);
        _S996 = _S1003;
        _S997 = _S1004;
        _S998 = _S1001;
        _S999 = _S1002;
        _S1000 = perp_1;
    }
    else
    {
        (&h_1)->g_lp_0 = 0.0f;
        _S996 = 0.0f;
        _S997 = 0.0f;
        _S998 = 0.0f;
        _S999 = 0.0f;
        _S1000 = _S969;
    }
    SurfelHit_0 _S1005 = h_1;
    SurfelHit_0 _S1006 = h_1;
    bool _S1007 = (h_1.g_0) >= (h_1.g_lp_0);
    float _S1008;
    if(_S1007)
    {
        _S1008 = _S1005.g_0;
    }
    else
    {
        _S1008 = _S1006.g_lp_0;
    }
    SurfelHit_0 _S1009 = SurfelHit_x24_syn_dzero_0();
    float _S1010 = _S965.primal_0 * _s_dOut_5;
    float _S1011 = _S1008 * _s_dOut_5;
    float _S1012;
    if(_S1007)
    {
        _S1008 = 0.0f;
        _S1012 = _S1010;
    }
    else
    {
        _S1008 = _S1010;
        _S1012 = 0.0f;
    }
    SurfelHit_0 _S1013 = _S1009;
    (&_S1013)->g_lp_0 = _S1008;
    (&_S1013)->g_0 = _S1012;
    SurfelHit_0 _S1014 = _S1009;
    SurfelHit_0 _S1015 = _S1013;
    SurfelHit_0 _S1016 = SurfelHit_x24_syn_dadd_0(&_S1014, &_S1015);
    float3  _S1017;
    float3  _S1018;
    if(_S995)
    {
        h_1 = _S1016;
        (&h_1)->g_lp_0 = 0.0f;
        DiffPair_float_0 _S1019;
        (&_S1019)->primal_0 = _S996;
        (&_S1019)->differential_0 = 0.0f;
        s_bwd_prop_exp_1(&_S1019, _S1016.g_lp_0);
        float _S1020 = _S1019.differential_0 / _S997;
        float _S1021 = fp_1 * (_S998 * - _S1020);
        float _S1022 = - (_S999 * _S1020);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1023;
        (&_S1023)->primal_0 = _S1000;
        (&_S1023)->differential_0 = _S969;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1024;
        (&_S1024)->primal_0 = _S1000;
        (&_S1024)->differential_0 = _S969;
        s_bwd_prop_dot_0(&_S1023, &_S1024, _S1022);
        float3  _S1025 = _S1024.differential_0 + _S1023.differential_0;
        float3  _S1026 = - _S1025;
        float3  _S1027 = _S990 * _S1026;
        float3  _S1028 = _S967.primal_0 * _S1026;
        SurfelHit_0 _S1029 = h_1;
        SurfelHit_0 _S1030 = _S1009;
        SurfelHit_0 _S1031 = SurfelHit_x24_syn_dadd_0(&_S1029, &_S1030);
        _S996 = _S1021 + _S1021;
        h_1 = _S1031;
        _S1000 = _S1028;
        _S1017 = _S1025;
        _S1018 = _S1027;
    }
    else
    {
        h_1 = _S1016;
        (&h_1)->g_lp_0 = 0.0f;
        SurfelHit_0 _S1032 = h_1;
        SurfelHit_0 _S1033 = _S1009;
        SurfelHit_0 _S1034 = SurfelHit_x24_syn_dadd_0(&_S1032, &_S1033);
        _S996 = 0.0f;
        h_1 = _S1034;
        _S1000 = _S969;
        _S1017 = _S969;
        _S1018 = _S969;
    }
    float _S1035 = _S993 * _S996;
    float _S1036 = _S994 * _S996;
    DiffPair_float_0 _S1037;
    (&_S1037)->primal_0 = _S973;
    (&_S1037)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S1037, _S1035);
    float _S1038 = (_S992 * _S1036 + h_1.t_mean_0 + _S1000.x + _S1000.y + _S1000.z) / _S991;
    float _S1039 = _S988 * - _S1038;
    float _S1040 = _S973 * _S1038;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1041;
    (&_S1041)->primal_0 = m_2;
    (&_S1041)->differential_0 = _S969;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1042;
    (&_S1042)->primal_0 = _S967.primal_0;
    (&_S1042)->differential_0 = _S969;
    s_bwd_prop_dot_0(&_S1041, &_S1042, _S1040);
    float3  _S1043 = _S1041.differential_0 + _S1017;
    float3  _S1044 = - _S1043;
    float _S1045 = _S1037.differential_0 + _S1039;
    float3  _S1046 = _S1042.differential_0 + _S1018;
    if(_S976)
    {
        DiffPair_float_0 _S1047;
        (&_S1047)->primal_0 = _S977;
        (&_S1047)->differential_0 = 0.0f;
        s_bwd_prop_exp_1(&_S1047, h_1.g_0);
        float _S1048 = -0.5f * _S1047.differential_0;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S1049;
        (&_S1049)->primal_0 = _S980;
        (&_S1049)->differential_0 = _S968;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S1050;
        (&_S1050)->primal_0 = _S980;
        (&_S1050)->differential_0 = _S968;
        s_bwd_prop_dot_1(&_S1049, &_S1050, _S1048);
        float2  _S1051 = _S1050.differential_0 + _S1049.differential_0;
        float2  _S1052 = _S981 * _S1051;
        float2  _S1053 = _S982 * _S1051;
        float _S1054 = (h_1.t_hit_0 + _S1053.x + _S1053.y) / _S978;
        float3  _S1055 = make_float3 (_S1051.x, _S1051.y, - (_S974 * _S1054));
        float3  _S1056 = make_float3 (_S1052.x, _S1052.y, 0.0f);
        _S977 = _S979 * - _S1054;
        _S1000 = _S1056;
        _S1017 = _S1055;
    }
    else
    {
        _S977 = 0.0f;
        _S1000 = _S969;
        _S1017 = _S969;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1057;
    (&_S1057)->primal_0 = _S967.primal_0;
    (&_S1057)->differential_0 = _S969;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1058;
    (&_S1058)->primal_0 = _S967.primal_0;
    (&_S1058)->differential_0 = _S969;
    s_bwd_prop_dot_0(&_S1057, &_S1058, _S1045);
    float3  _S1059 = _S1000 + make_float3 (0.0f, 0.0f, _S977);
    Matrix<float, 3, 3>  _S1060 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1061;
    (&_S1061)->primal_0 = _S964.primal_0;
    (&_S1061)->differential_0 = _S1060;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1062;
    (&_S1062)->primal_0 = _S967.primal_0;
    (&_S1062)->differential_0 = _S969;
    s_bwd_prop_mul_2(&_S1061, &_S1062, _S1059);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1063;
    (&_S1063)->primal_0 = _S964.primal_0;
    (&_S1063)->differential_0 = _S1060;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1064;
    (&_S1064)->primal_0 = _S970;
    (&_S1064)->differential_0 = _S969;
    s_bwd_prop_mul_2(&_S1063, &_S1064, _S1017);
    float3  _S1065 = - _S1064.differential_0;
    float3  _S1066 = _S1058.differential_0 + _S1057.differential_0 + _S1062.differential_0 + _S1046;
    dpray_d_2->primal_0 = (*dpray_d_2).primal_0;
    dpray_d_2->differential_0 = _S1066;
    float3  _S1067 = _S1064.differential_0 + _S1044;
    dpray_o_2->primal_0 = (*dpray_o_2).primal_0;
    dpray_o_2->differential_0 = _S1067;
    dpopacity_2->primal_0 = (*dpopacity_2).primal_0;
    dpopacity_2->differential_0 = _S1011;
    Matrix<float, 3, 3>  _S1068 = _S1061.differential_0 + _S1063.differential_0;
    dpiscl_rot_2->primal_0 = (*dpiscl_rot_2).primal_0;
    dpiscl_rot_2->differential_0 = _S1068;
    float3  _S1069 = _S1065 + _S1043;
    dpmean_2->primal_0 = (*dpmean_2).primal_0;
    dpmean_2->differential_0 = _S1069;
    return;
}

inline __device__ void s_bwd_evaluate_alpha_2dgs_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1070, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S1071, DiffPair_float_0 * _S1072, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1073, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1074, float2  _S1075, float _S1076)
{
    s_bwd_prop_evaluate_alpha_2dgs_0(_S1070, _S1071, _S1072, _S1073, _S1074, _S1075, _S1076);
    return;
}

inline __device__ void evaluate_alpha_2dgs_vjp(float3  mean_5, Matrix<float, 3, 3>  iscl_rot_5, float opacity_5, float3  ray_o_5, float3  ray_d_5, float2  pix_size_2, float v_alpha_1, float3  * v_mean_2, Matrix<float, 3, 3>  * v_iscl_rot_4, float * v_opacity_2, float3  * v_ray_o_3, float3  * v_ray_d_3)
{
    float3  _S1077 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_mean_2;
    (&dp_mean_2)->primal_0 = mean_5;
    (&dp_mean_2)->differential_0 = _S1077;
    Matrix<float, 3, 3>  _S1078 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 dp_iscl_rot_2;
    (&dp_iscl_rot_2)->primal_0 = iscl_rot_5;
    (&dp_iscl_rot_2)->differential_0 = _S1078;
    DiffPair_float_0 dp_opacity_2;
    (&dp_opacity_2)->primal_0 = opacity_5;
    (&dp_opacity_2)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ray_o_2;
    (&dp_ray_o_2)->primal_0 = ray_o_5;
    (&dp_ray_o_2)->differential_0 = _S1077;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ray_d_2;
    (&dp_ray_d_2)->primal_0 = ray_d_5;
    (&dp_ray_d_2)->differential_0 = _S1077;
    s_bwd_evaluate_alpha_2dgs_0(&dp_mean_2, &dp_iscl_rot_2, &dp_opacity_2, &dp_ray_o_2, &dp_ray_d_2, pix_size_2, v_alpha_1);
    *v_mean_2 = dp_mean_2.differential_0;
    *v_iscl_rot_4 = dp_iscl_rot_2.differential_0;
    *v_opacity_2 = dp_opacity_2.differential_0;
    *v_ray_o_3 = dp_ray_o_2.differential_0;
    *v_ray_d_3 = dp_ray_d_2.differential_0;
    return;
}

inline __device__ void evaluate_color_2dgs(float3  mean_6, Matrix<float, 3, 3>  iscl_rot_6, float opacity_6, float3  rgb_2, float3  ray_o_6, float3  ray_d_6, float2  pix_size_3, float3  * out_rgb_1, float * depth_1, float3  * normal_0)
{
    *out_rgb_1 = rgb_2;
    SurfelHit_0 h_2;
    float3  gro_1 = mul_2(iscl_rot_6, ray_o_6 - mean_6);
    float3  grd_3 = mul_2(iscl_rot_6, ray_d_6);
    float dd_1 = dot_1(ray_d_6, ray_d_6);
    float _S1079 = grd_3.z;
    if((_S1079 * _S1079) > (9.99999997475242708e-07f * dd_1))
    {
        float _S1080 = - gro_1.z / _S1079;
        (&h_2)->t_hit_0 = _S1080;
        float2  uv_36 = float2 {gro_1.x, gro_1.y} + make_float2 (_S1080) * float2 {grd_3.x, grd_3.y};
        (&h_2)->g_0 = (F32_exp((-0.5f * dot_0(uv_36, uv_36))));
    }
    else
    {
        (&h_2)->t_hit_0 = 0.0f;
        (&h_2)->g_0 = 0.0f;
    }
    float3  m_3 = mean_6 - ray_o_6;
    float _S1081 = dot_1(m_3, ray_d_6) / dd_1;
    (&h_2)->t_mean_0 = _S1081;
    float fp_2 = pix_size_3.x + pix_size_3.y * _S1081 * (F32_sqrt((dd_1)));
    bool _S1082;
    if(_S1081 > 0.0f)
    {
        _S1082 = fp_2 > 0.0f;
    }
    else
    {
        _S1082 = false;
    }
    if(_S1082)
    {
        float3  perp_2 = m_3 - make_float3 ((&h_2)->t_mean_0) * ray_d_6;
        (&h_2)->g_lp_0 = (F32_exp((- dot_1(perp_2, perp_2) / (fp_2 * fp_2))));
    }
    else
    {
        (&h_2)->g_lp_0 = 0.0f;
    }
    SurfelHit_0 _S1083 = h_2;
    float _S1084;
    if((_S1083.g_0) >= (_S1083.g_lp_0))
    {
        _S1084 = _S1083.t_hit_0;
    }
    else
    {
        _S1084 = _S1083.t_mean_0;
    }
    *depth_1 = _S1084;
    float3  n_0 = make_float3 (iscl_rot_6.rows[int(2)].x, iscl_rot_6.rows[int(2)].y, iscl_rot_6.rows[int(2)].z);
    float3  _S1085;
    if((dot_1(n_0, ray_d_6)) > 0.0f)
    {
        _S1085 = - n_0;
    }
    else
    {
        _S1085 = n_0;
    }
    *normal_0 = _S1085;
    return;
}

inline __device__ void s_bwd_prop_evaluate_color_2dgs_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpmean_3, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * dpiscl_rot_3, DiffPair_float_0 * dpopacity_3, DiffPair_vectorx3Cfloatx2C3x3E_0 * dprgb_1, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpray_o_3, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpray_d_3, float2  pix_size_4, float3  dpout_rgb_1, float dpdepth_1, float3  dpnormal_0)
{
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1086 = *dpmean_3;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1087 = *dpiscl_rot_3;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1088 = *dpray_o_3;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1089 = *dpray_d_3;
    float2  _S1090 = make_float2 (0.0f);
    float3  _S1091 = make_float3 (0.0f);
    float3  _S1092 = (*dpray_o_3).primal_0 - (*dpmean_3).primal_0;
    float3  _S1093 = s_primal_ctx_mul_0((*dpiscl_rot_3).primal_0, _S1092);
    float3  _S1094 = s_primal_ctx_mul_0((*dpiscl_rot_3).primal_0, (*dpray_d_3).primal_0);
    float _S1095 = s_primal_ctx_dot_0((*dpray_d_3).primal_0, (*dpray_d_3).primal_0);
    float _S1096 = _S1094.z;
    float _S1097 = _S1096 * _S1096;
    bool _S1098 = _S1097 > (9.99999997475242708e-07f * _S1095);
    SurfelHit_0 h_3;
    float _S1099;
    float _S1100;
    float _S1101;
    float2  _S1102;
    float2  _S1103;
    float2  _S1104;
    if(_S1098)
    {
        float _S1105 = - _S1093.z;
        float _S1106 = _S1105 / _S1096;
        float2  _S1107 = make_float2 (_S1106);
        float2  _S1108 = float2 {_S1094.x, _S1094.y};
        float2  uv_37 = float2 {_S1093.x, _S1093.y} + make_float2 (_S1106) * _S1108;
        float _S1109 = -0.5f * s_primal_ctx_dot_1(uv_37, uv_37);
        (&h_3)->g_0 = s_primal_ctx_exp_1(_S1109);
        (&h_3)->g_lp_0 = 0.0f;
        (&h_3)->t_hit_0 = _S1106;
        (&h_3)->t_mean_0 = 0.0f;
        _S1099 = _S1109;
        _S1102 = uv_37;
        _S1103 = _S1107;
        _S1104 = _S1108;
        _S1100 = _S1097;
        _S1101 = _S1105;
    }
    else
    {
        (&h_3)->g_0 = 0.0f;
        (&h_3)->g_lp_0 = 0.0f;
        (&h_3)->t_hit_0 = 0.0f;
        (&h_3)->t_mean_0 = 0.0f;
        _S1099 = 0.0f;
        _S1102 = _S1090;
        _S1103 = _S1090;
        _S1104 = _S1090;
        _S1100 = 0.0f;
        _S1101 = 0.0f;
    }
    float3  m_4 = _S1086.primal_0 - _S1088.primal_0;
    float _S1110 = s_primal_ctx_dot_0(m_4, _S1089.primal_0);
    float _S1111 = _S1110 / _S1095;
    float3  _S1112 = make_float3 (_S1111);
    float _S1113 = _S1095 * _S1095;
    (&h_3)->t_mean_0 = _S1111;
    float _S1114 = pix_size_4.y;
    float _S1115 = _S1114 * _S1111;
    float _S1116 = s_primal_ctx_sqrt_0(_S1095);
    float fp_3 = pix_size_4.x + _S1115 * _S1116;
    bool _S1117;
    if(_S1111 > 0.0f)
    {
        _S1117 = fp_3 > 0.0f;
    }
    else
    {
        _S1117 = false;
    }
    float _S1118;
    float _S1119;
    float _S1120;
    float _S1121;
    float3  _S1122;
    if(_S1117)
    {
        float3  perp_3 = m_4 - make_float3 (_S1111) * _S1089.primal_0;
        float _S1123 = - s_primal_ctx_dot_0(perp_3, perp_3);
        float _S1124 = fp_3 * fp_3;
        float _S1125 = _S1123 / _S1124;
        float _S1126 = _S1124 * _S1124;
        (&h_3)->g_lp_0 = s_primal_ctx_exp_1(_S1125);
        _S1118 = _S1125;
        _S1119 = _S1126;
        _S1120 = _S1123;
        _S1121 = _S1124;
        _S1122 = perp_3;
    }
    else
    {
        (&h_3)->g_lp_0 = 0.0f;
        _S1118 = 0.0f;
        _S1119 = 0.0f;
        _S1120 = 0.0f;
        _S1121 = 0.0f;
        _S1122 = _S1091;
    }
    bool _S1127 = (h_3.g_0) >= (h_3.g_lp_0);
    float3  n_1 = make_float3 (_S1087.primal_0.rows[int(2)].x, _S1087.primal_0.rows[int(2)].y, _S1087.primal_0.rows[int(2)].z);
    SurfelHit_0 _S1128 = SurfelHit_x24_syn_dzero_0();
    float3  _S1129;
    if((s_primal_ctx_dot_0(n_1, _S1089.primal_0)) > 0.0f)
    {
        _S1129 = - dpnormal_0;
    }
    else
    {
        _S1129 = dpnormal_0;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1130;
    (&_S1130)->primal_0 = n_1;
    (&_S1130)->differential_0 = _S1091;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1131;
    (&_S1131)->primal_0 = _S1089.primal_0;
    (&_S1131)->differential_0 = _S1091;
    s_bwd_prop_dot_0(&_S1130, &_S1131, 0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1132 = _S1131;
    float3  _S1133 = _S1130.differential_0 + _S1129;
    float3  _S1134 = _S1091;
    *&((&_S1134)->z) = _S1133.z;
    *&((&_S1134)->y) = _S1133.y;
    *&((&_S1134)->x) = _S1133.x;
    Matrix<float, 3, 3>  _S1135 = makeMatrix<float, 3, 3> (0.0f);
    Matrix<float, 3, 3>  _S1136 = _S1135;
    _S1136[int(2)] = _S1134;
    Matrix<float, 3, 3>  _S1137 = _S1136;
    if(_S1127)
    {
        SurfelHit_0 _S1138 = _S1128;
        (&_S1138)->t_hit_0 = dpdepth_1;
        SurfelHit_0 _S1139 = _S1128;
        SurfelHit_0 _S1140 = _S1138;
        SurfelHit_0 _S1141 = SurfelHit_x24_syn_dadd_0(&_S1139, &_S1140);
        h_3 = _S1141;
    }
    else
    {
        SurfelHit_0 _S1142 = _S1128;
        (&_S1142)->t_mean_0 = dpdepth_1;
        SurfelHit_0 _S1143 = _S1128;
        SurfelHit_0 _S1144 = _S1142;
        SurfelHit_0 _S1145 = SurfelHit_x24_syn_dadd_0(&_S1143, &_S1144);
        h_3 = _S1145;
    }
    SurfelHit_0 _S1146 = _S1128;
    (&_S1146)->g_lp_0 = 0.0f;
    (&_S1146)->g_0 = 0.0f;
    SurfelHit_0 _S1147 = h_3;
    SurfelHit_0 _S1148 = _S1146;
    SurfelHit_0 _S1149 = SurfelHit_x24_syn_dadd_0(&_S1147, &_S1148);
    float3  _S1150;
    if(_S1117)
    {
        h_3 = _S1149;
        (&h_3)->g_lp_0 = 0.0f;
        DiffPair_float_0 _S1151;
        (&_S1151)->primal_0 = _S1118;
        (&_S1151)->differential_0 = 0.0f;
        s_bwd_prop_exp_1(&_S1151, _S1149.g_lp_0);
        float _S1152 = _S1151.differential_0 / _S1119;
        float _S1153 = fp_3 * (_S1120 * - _S1152);
        float _S1154 = - (_S1121 * _S1152);
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1155;
        (&_S1155)->primal_0 = _S1122;
        (&_S1155)->differential_0 = _S1091;
        DiffPair_vectorx3Cfloatx2C3x3E_0 _S1156;
        (&_S1156)->primal_0 = _S1122;
        (&_S1156)->differential_0 = _S1091;
        s_bwd_prop_dot_0(&_S1155, &_S1156, _S1154);
        float3  _S1157 = _S1156.differential_0 + _S1155.differential_0;
        float3  _S1158 = - _S1157;
        float3  _S1159 = _S1112 * _S1158;
        float3  _S1160 = _S1089.primal_0 * _S1158;
        SurfelHit_0 _S1161 = h_3;
        SurfelHit_0 _S1162 = _S1128;
        SurfelHit_0 _S1163 = SurfelHit_x24_syn_dadd_0(&_S1161, &_S1162);
        float3  _S1164 = _S1159 + _S1132.differential_0;
        _S1118 = _S1153 + _S1153;
        h_3 = _S1163;
        _S1122 = _S1160;
        _S1129 = _S1157;
        _S1150 = _S1164;
    }
    else
    {
        h_3 = _S1149;
        (&h_3)->g_lp_0 = 0.0f;
        SurfelHit_0 _S1165 = h_3;
        SurfelHit_0 _S1166 = _S1128;
        SurfelHit_0 _S1167 = SurfelHit_x24_syn_dadd_0(&_S1165, &_S1166);
        _S1118 = 0.0f;
        h_3 = _S1167;
        _S1122 = _S1091;
        _S1129 = _S1091;
        _S1150 = _S1132.differential_0;
    }
    float _S1168 = _S1115 * _S1118;
    float _S1169 = _S1116 * _S1118;
    DiffPair_float_0 _S1170;
    (&_S1170)->primal_0 = _S1095;
    (&_S1170)->differential_0 = 0.0f;
    s_bwd_prop_sqrt_0(&_S1170, _S1168);
    float _S1171 = (_S1114 * _S1169 + h_3.t_mean_0 + _S1122.x + _S1122.y + _S1122.z) / _S1113;
    float _S1172 = _S1110 * - _S1171;
    float _S1173 = _S1095 * _S1171;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1174;
    (&_S1174)->primal_0 = m_4;
    (&_S1174)->differential_0 = _S1091;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1175;
    (&_S1175)->primal_0 = _S1089.primal_0;
    (&_S1175)->differential_0 = _S1091;
    s_bwd_prop_dot_0(&_S1174, &_S1175, _S1173);
    float3  _S1176 = _S1174.differential_0 + _S1129;
    float3  _S1177 = - _S1176;
    float _S1178 = _S1170.differential_0 + _S1172;
    float3  _S1179 = _S1175.differential_0 + _S1150;
    if(_S1098)
    {
        DiffPair_float_0 _S1180;
        (&_S1180)->primal_0 = _S1099;
        (&_S1180)->differential_0 = 0.0f;
        s_bwd_prop_exp_1(&_S1180, h_3.g_0);
        float _S1181 = -0.5f * _S1180.differential_0;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S1182;
        (&_S1182)->primal_0 = _S1102;
        (&_S1182)->differential_0 = _S1090;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S1183;
        (&_S1183)->primal_0 = _S1102;
        (&_S1183)->differential_0 = _S1090;
        s_bwd_prop_dot_1(&_S1182, &_S1183, _S1181);
        float2  _S1184 = _S1183.differential_0 + _S1182.differential_0;
        float2  _S1185 = _S1103 * _S1184;
        float2  _S1186 = _S1104 * _S1184;
        float _S1187 = (h_3.t_hit_0 + _S1186.x + _S1186.y) / _S1100;
        float3  _S1188 = make_float3 (_S1184.x, _S1184.y, - (_S1096 * _S1187));
        float3  _S1189 = make_float3 (_S1185.x, _S1185.y, 0.0f);
        _S1099 = _S1101 * - _S1187;
        _S1122 = _S1189;
        _S1129 = _S1188;
    }
    else
    {
        _S1099 = 0.0f;
        _S1122 = _S1091;
        _S1129 = _S1091;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1190;
    (&_S1190)->primal_0 = _S1089.primal_0;
    (&_S1190)->differential_0 = _S1091;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1191;
    (&_S1191)->primal_0 = _S1089.primal_0;
    (&_S1191)->differential_0 = _S1091;
    s_bwd_prop_dot_0(&_S1190, &_S1191, _S1178);
    float3  _S1192 = _S1122 + make_float3 (0.0f, 0.0f, _S1099);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1193;
    (&_S1193)->primal_0 = _S1087.primal_0;
    (&_S1193)->differential_0 = _S1135;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1194;
    (&_S1194)->primal_0 = _S1089.primal_0;
    (&_S1194)->differential_0 = _S1091;
    s_bwd_prop_mul_2(&_S1193, &_S1194, _S1192);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1195;
    (&_S1195)->primal_0 = _S1087.primal_0;
    (&_S1195)->differential_0 = _S1135;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1196;
    (&_S1196)->primal_0 = _S1092;
    (&_S1196)->differential_0 = _S1091;
    s_bwd_prop_mul_2(&_S1195, &_S1196, _S1129);
    float3  _S1197 = - _S1196.differential_0;
    float3  _S1198 = _S1191.differential_0 + _S1190.differential_0 + _S1194.differential_0 + _S1179;
    dpray_d_3->primal_0 = (*dpray_d_3).primal_0;
    dpray_d_3->differential_0 = _S1198;
    float3  _S1199 = _S1196.differential_0 + _S1177;
    dpray_o_3->primal_0 = (*dpray_o_3).primal_0;
    dpray_o_3->differential_0 = _S1199;
    dprgb_1->primal_0 = (*dprgb_1).primal_0;
    dprgb_1->differential_0 = dpout_rgb_1;
    dpopacity_3->primal_0 = (*dpopacity_3).primal_0;
    dpopacity_3->differential_0 = 0.0f;
    Matrix<float, 3, 3>  _S1200 = _S1193.differential_0 + _S1195.differential_0 + _S1137;
    dpiscl_rot_3->primal_0 = (*dpiscl_rot_3).primal_0;
    dpiscl_rot_3->differential_0 = _S1200;
    float3  _S1201 = _S1197 + _S1176;
    dpmean_3->primal_0 = (*dpmean_3).primal_0;
    dpmean_3->differential_0 = _S1201;
    return;
}

inline __device__ void s_bwd_evaluate_color_2dgs_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1202, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S1203, DiffPair_float_0 * _S1204, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1205, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1206, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1207, float2  _S1208, float3  _S1209, float _S1210, float3  _S1211)
{
    s_bwd_prop_evaluate_color_2dgs_0(_S1202, _S1203, _S1204, _S1205, _S1206, _S1207, _S1208, _S1209, _S1210, _S1211);
    return;
}

inline __device__ void evaluate_color_2dgs_vjp(float3  mean_7, Matrix<float, 3, 3>  iscl_rot_7, float opacity_7, float3  rgb_3, float3  ray_o_7, float3  ray_d_7, float2  pix_size_5, float3  v_out_rgb_1, float v_depth_1, float3  v_normal_0, float3  * v_mean_3, Matrix<float, 3, 3>  * v_iscl_rot_5, float * v_opacity_3, float3  * v_rgb_1, float3  * v_ray_o_4, float3  * v_ray_d_4)
{
    float3  _S1212 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_mean_3;
    (&dp_mean_3)->primal_0 = mean_7;
    (&dp_mean_3)->differential_0 = _S1212;
    Matrix<float, 3, 3>  _S1213 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 dp_iscl_rot_3;
    (&dp_iscl_rot_3)->primal_0 = iscl_rot_7;
    (&dp_iscl_rot_3)->differential_0 = _S1213;
    DiffPair_float_0 dp_opacity_3;
    (&dp_opacity_3)->primal_0 = opacity_7;
    (&dp_opacity_3)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_rgb_1;
    (&dp_rgb_1)->primal_0 = rgb_3;
    (&dp_rgb_1)->differential_0 = _S1212;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ray_o_3;
    (&dp_ray_o_3)->primal_0 = ray_o_7;
    (&dp_ray_o_3)->differential_0 = _S1212;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ray_d_3;
    (&dp_ray_d_3)->primal_0 = ray_d_7;
    (&dp_ray_d_3)->differential_0 = _S1212;
    s_bwd_evaluate_color_2dgs_0(&dp_mean_3, &dp_iscl_rot_3, &dp_opacity_3, &dp_rgb_1, &dp_ray_o_3, &dp_ray_d_3, pix_size_5, v_out_rgb_1, v_depth_1, v_normal_0);
    *v_mean_3 = dp_mean_3.differential_0;
    *v_iscl_rot_5 = dp_iscl_rot_3.differential_0;
    *v_opacity_3 = dp_opacity_3.differential_0;
    *v_rgb_1 = dp_rgb_1.differential_0;
    *v_ray_o_4 = dp_ray_o_3.differential_0;
    *v_ray_d_4 = dp_ray_d_3.differential_0;
    return;
}

inline __device__ float view_radius_3dgs(float3  mean_8, float3  log_scale_0, float logit_opacity_0, float3  campos_0)
{
    float radius_0 = (F32_exp(((F32_max((log_scale_0.x), ((F32_max((log_scale_0.y), (log_scale_0.z))))))))) * (F32_sqrt((2.0f * (F32_log(((F32_max((255.0f / (1.0f + (F32_exp((- logit_opacity_0))))), (1.0f)))))))));
    float dist_0 = length_1(mean_8 - campos_0);
    return radius_0 / ((F32_max((dist_0), (radius_0))) + (F32_sqrt(((F32_max((dist_0 * dist_0 - radius_0 * radius_0), (0.0f)))))));
}

