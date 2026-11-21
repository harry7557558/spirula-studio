#pragma once

#include "generated/slang.cuh"

struct SurfelHit_0
{
    float g_0;
    float t_hit_0;
};

inline __device__ SurfelHit_0 SurfelHit_x24_syn_dzero_0()
{
    SurfelHit_0 result_0;
    (&result_0)->g_0 = 0.0f;
    (&result_0)->t_hit_0 = 0.0f;
    return result_0;
}

inline __device__ SurfelHit_0 SurfelHit_x24_syn_dadd_0(SurfelHit_0 * SLANG_anonymous_0_0, SurfelHit_0 * SLANG_anonymous_1_0)
{
    SurfelHit_0 result_1;
    (&result_1)->g_0 = SLANG_anonymous_0_0->g_0 + SLANG_anonymous_1_0->g_0;
    (&result_1)->t_hit_0 = SLANG_anonymous_0_0->t_hit_0 + SLANG_anonymous_1_0->t_hit_0;
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

inline __device__ DiffPair_float_0 _d_sqrt_0(DiffPair_float_0 * dpx_1)
{
    DiffPair_float_0 _S6 = { (F32_sqrt((dpx_1->primal_0))), 0.5f / (F32_sqrt(((F32_max((1.00000001168609742e-07f), (dpx_1->primal_0)))))) * dpx_1->differential_0 };
    return _S6;
}

struct DiffPair_vectorx3Cfloatx2C2x3E_0
{
    float2  primal_0;
    float2  differential_0;
};

inline __device__ void _d_dot_0(DiffPair_vectorx3Cfloatx2C2x3E_0 * dpx_2, DiffPair_vectorx3Cfloatx2C2x3E_0 * dpy_1, float dOut_2)
{
    float2  x_d_result_0;
    *&((&x_d_result_0)->x) = (*dpy_1).primal_0.x * dOut_2;
    float2  y_d_result_0;
    *&((&y_d_result_0)->x) = (*dpx_2).primal_0.x * dOut_2;
    *&((&x_d_result_0)->y) = (*dpy_1).primal_0.y * dOut_2;
    *&((&y_d_result_0)->y) = (*dpx_2).primal_0.y * dOut_2;
    dpx_2->primal_0 = (*dpx_2).primal_0;
    dpx_2->differential_0 = x_d_result_0;
    dpy_1->primal_0 = (*dpy_1).primal_0;
    dpy_1->differential_0 = y_d_result_0;
    return;
}

inline __device__ void _d_dot_1(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_3, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpy_2, float dOut_3)
{
    float3  x_d_result_1;
    *&((&x_d_result_1)->x) = (*dpy_2).primal_0.x * dOut_3;
    float3  y_d_result_1;
    *&((&y_d_result_1)->x) = (*dpx_3).primal_0.x * dOut_3;
    *&((&x_d_result_1)->y) = (*dpy_2).primal_0.y * dOut_3;
    *&((&y_d_result_1)->y) = (*dpx_3).primal_0.y * dOut_3;
    *&((&x_d_result_1)->z) = (*dpy_2).primal_0.z * dOut_3;
    *&((&y_d_result_1)->z) = (*dpx_3).primal_0.z * dOut_3;
    dpx_3->primal_0 = (*dpx_3).primal_0;
    dpx_3->differential_0 = x_d_result_1;
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

inline __device__ float dot_2(float4  x_9, float4  y_2)
{
    int i_4 = int(0);
    float result_10 = 0.0f;
    for(;;)
    {
        if(i_4 < int(4))
        {
        }
        else
        {
            break;
        }
        float result_11 = result_10 + _slang_vector_get_element(x_9, i_4) * _slang_vector_get_element(y_2, i_4);
        i_4 = i_4 + int(1);
        result_10 = result_11;
    }
    return result_10;
}

inline __device__ float length_0(float2  x_10)
{
    return (F32_sqrt((dot_0(x_10, x_10))));
}

inline __device__ float length_1(float3  x_11)
{
    return (F32_sqrt((dot_1(x_11, x_11))));
}

inline __device__ float length_2(float4  x_12)
{
    return (F32_sqrt((dot_2(x_12, x_12))));
}

inline __device__ bool equirect_proj_nav(float3  p_view_0, float4  intrins_0, float2  * uv_0)
{
    *uv_0 = make_float2 (intrins_0.x * (F32_atan2((p_view_0.x), (p_view_0.z))) + intrins_0.z, intrins_0.y * (F32_atan2((p_view_0.y), (length_0(float2 {p_view_0.x, p_view_0.z})))) + intrins_0.w);
    return true;
}

inline __device__ DiffPair_float_0 s_fwd_length_impl_0(DiffPair_vectorx3Cfloatx2C2x3E_0 * dpx_4)
{
    float _S7 = *&((&dpx_4->differential_0)->x) * *&((&dpx_4->primal_0)->x);
    float _S8 = *&((&dpx_4->differential_0)->y) * *&((&dpx_4->primal_0)->y);
    float s_diff_len_0 = _S7 + _S7 + (_S8 + _S8);
    DiffPair_float_0 _S9;
    (&_S9)->primal_0 = *&((&dpx_4->primal_0)->x) * *&((&dpx_4->primal_0)->x) + *&((&dpx_4->primal_0)->y) * *&((&dpx_4->primal_0)->y);
    (&_S9)->differential_0 = s_diff_len_0;
    DiffPair_float_0 _S10 = _d_sqrt_0(&_S9);
    DiffPair_float_0 _S11 = { _S10.primal_0, _S10.differential_0 };
    return _S11;
}

inline __device__ Matrix<float, 2, 3>  equirect_proj_jac(float3  p_view_1, float4  intrins_1)
{
    float _S12 = p_view_1.x;
    float _S13 = p_view_1.z;
    DiffPair_float_0 _S14;
    (&_S14)->primal_0 = _S12;
    (&_S14)->differential_0 = 1.0f;
    DiffPair_float_0 _S15;
    (&_S15)->primal_0 = _S13;
    (&_S15)->differential_0 = 0.0f;
    DiffPair_float_0 _S16 = _d_atan2_0(&_S14, &_S15);
    float _S17 = p_view_1.y;
    float2  _S18 = float2 {p_view_1.x, p_view_1.z};
    float2  _S19 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S20;
    (&_S20)->primal_0 = _S18;
    (&_S20)->differential_0 = _S19;
    DiffPair_float_0 _S21 = s_fwd_length_impl_0(&_S20);
    DiffPair_float_0 _S22;
    (&_S22)->primal_0 = _S17;
    (&_S22)->differential_0 = 0.0f;
    DiffPair_float_0 _S23;
    (&_S23)->primal_0 = _S21.primal_0;
    (&_S23)->differential_0 = _S21.differential_0;
    DiffPair_float_0 _S24 = _d_atan2_0(&_S22, &_S23);
    float fx_0 = intrins_1.x;
    float fy_0 = intrins_1.y;
    float _S25 = _S24.differential_0 * fy_0;
    Matrix<float, 2, 3>  J_0;
    *&(((&J_0)->rows + (int(0)))->x) = _S16.differential_0 * fx_0;
    *&(((&J_0)->rows + (int(1)))->x) = _S25;
    DiffPair_float_0 _S26;
    (&_S26)->primal_0 = _S12;
    (&_S26)->differential_0 = 0.0f;
    DiffPair_float_0 _S27;
    (&_S27)->primal_0 = _S13;
    (&_S27)->differential_0 = 0.0f;
    DiffPair_float_0 _S28 = _d_atan2_0(&_S26, &_S27);
    float2  _S29 = make_float2 (0.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S30;
    (&_S30)->primal_0 = _S18;
    (&_S30)->differential_0 = _S29;
    DiffPair_float_0 _S31 = s_fwd_length_impl_0(&_S30);
    DiffPair_float_0 _S32;
    (&_S32)->primal_0 = _S17;
    (&_S32)->differential_0 = 1.0f;
    DiffPair_float_0 _S33;
    (&_S33)->primal_0 = _S31.primal_0;
    (&_S33)->differential_0 = _S31.differential_0;
    DiffPair_float_0 _S34 = _d_atan2_0(&_S32, &_S33);
    float _S35 = _S34.differential_0 * fy_0;
    *&(((&J_0)->rows + (int(0)))->y) = _S28.differential_0 * fx_0;
    *&(((&J_0)->rows + (int(1)))->y) = _S35;
    DiffPair_float_0 _S36;
    (&_S36)->primal_0 = _S12;
    (&_S36)->differential_0 = 0.0f;
    DiffPair_float_0 _S37;
    (&_S37)->primal_0 = _S13;
    (&_S37)->differential_0 = 1.0f;
    DiffPair_float_0 _S38 = _d_atan2_0(&_S36, &_S37);
    float2  _S39 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S40;
    (&_S40)->primal_0 = _S18;
    (&_S40)->differential_0 = _S39;
    DiffPair_float_0 _S41 = s_fwd_length_impl_0(&_S40);
    DiffPair_float_0 _S42;
    (&_S42)->primal_0 = _S17;
    (&_S42)->differential_0 = 0.0f;
    DiffPair_float_0 _S43;
    (&_S43)->primal_0 = _S41.primal_0;
    (&_S43)->differential_0 = _S41.differential_0;
    DiffPair_float_0 _S44 = _d_atan2_0(&_S42, &_S43);
    float _S45 = _S44.differential_0 * fy_0;
    *&(((&J_0)->rows + (int(0)))->z) = _S38.differential_0 * fx_0;
    *&(((&J_0)->rows + (int(1)))->z) = _S45;
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
    bool _S46;
    for(;;)
    {
        float2  _S47 = float2 {p_view_2.x, p_view_2.y};
        float _S48 = p_view_2.z;
        float2  uv0_0 = _S47 / make_float2 (_S48);
        bool _S49 = _S48 < 0.0f;
        if(_S49)
        {
            *uv_3 = uv0_0;
            _S46 = false;
            break;
        }
        float2  uv_4 = _S47 / make_float2 (_S48);
        FixedArray<float, 1>  _S50 = dist_coeffs_1;
        float2  _S51 = DistNone_distort_0(uv_4, &_S50);
        *uv_3 = make_float2 (intrins_2.x * _S51.x + intrins_2.z, intrins_2.y * _S51.y + intrins_2.w);
        _S46 = true;
        break;
    }
    return _S46;
}

inline __device__ bool fisheye_proj_nav_none(float3  p_view_3, float4  intrins_3, FixedArray<float, 1>  dist_coeffs_2, float2  * uv_5)
{
    float2  _S52 = float2 {p_view_3.x, p_view_3.y};
    float r_3 = length_0(_S52);
    float _S53 = p_view_3.z;
    float theta_0 = (F32_atan2((r_3), (_S53)));
    float k_0;
    if(theta_0 < 0.00100000004749745f)
    {
        k_0 = (1.0f - theta_0 * theta_0 / 3.0f) / _S53;
    }
    else
    {
        k_0 = theta_0 / r_3;
    }
    float2  _S54 = _S52 * make_float2 (k_0);
    FixedArray<float, 1>  _S55 = dist_coeffs_2;
    float2  _S56 = DistNone_distort_0(_S54, &_S55);
    *uv_5 = make_float2 (intrins_3.x * _S56.x + intrins_3.z, intrins_3.y * _S56.y + intrins_3.w);
    return true;
}

inline __device__ DiffPair_float_0 _d_sin_0(DiffPair_float_0 * dpx_5)
{
    DiffPair_float_0 _S57 = { (F32_sin((dpx_5->primal_0))), (F32_cos((dpx_5->primal_0))) * dpx_5->differential_0 };
    return _S57;
}

inline __device__ bool equisolid_proj_nav_none(float3  p_view_4, float4  intrins_4, FixedArray<float, 1>  dist_coeffs_3, float2  * uv_6)
{
    float2  _S58 = float2 {p_view_4.x, p_view_4.y};
    float r_4 = length_0(_S58);
    float _S59 = p_view_4.z;
    float theta_1 = (F32_atan2((r_4), (_S59)));
    float k_1;
    if(r_4 < 9.99999997475242708e-07f)
    {
        k_1 = (1.0f - theta_1 * theta_1 / 24.0f) / _S59;
    }
    else
    {
        k_1 = 2.0f * (F32_sin((0.5f * theta_1))) / r_4;
    }
    float2  _S60 = _S58 * make_float2 (k_1);
    FixedArray<float, 1>  _S61 = dist_coeffs_3;
    float2  _S62 = DistNone_distort_0(_S60, &_S61);
    *uv_6 = make_float2 (intrins_4.x * _S62.x + intrins_4.z, intrins_4.y * _S62.y + intrins_4.w);
    return true;
}

inline __device__ DiffPair_vectorx3Cfloatx2C2x3E_0 s_fwd_DistNone_distort_0(DiffPair_vectorx3Cfloatx2C2x3E_0 * dpuv_0, FixedArray<float, 1>  * coeffs_1)
{
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S63 = { dpuv_0->primal_0, dpuv_0->differential_0 };
    return _S63;
}

inline __device__ Matrix<float, 2, 3>  persp_proj_jac_none(float3  p_view_5, float4  intrins_5, FixedArray<float, 1>  dist_coeffs_4)
{
    float2  _S64 = float2 {p_view_5.x, p_view_5.y};
    float _S65 = p_view_5.z;
    float2  _S66 = _S64 * make_float2 (0.0f);
    float _S67 = _S65 * _S65;
    float2  s_diff_uv_0 = (make_float2 (1.0f, 0.0f) * make_float2 (_S65) - _S66) / make_float2 (_S67);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S68;
    (&_S68)->primal_0 = _S64 / make_float2 (_S65);
    (&_S68)->differential_0 = s_diff_uv_0;
    FixedArray<float, 1>  _S69 = dist_coeffs_4;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S70 = s_fwd_DistNone_distort_0(&_S68, &_S69);
    float fx_1 = intrins_5.x;
    float fy_1 = intrins_5.y;
    float _S71 = _S70.differential_0.y * fy_1;
    Matrix<float, 2, 3>  J_1;
    *&(((&J_1)->rows + (int(0)))->x) = _S70.differential_0.x * fx_1;
    *&(((&J_1)->rows + (int(1)))->x) = _S71;
    float2  s_diff_uv_1 = (make_float2 (0.0f, 1.0f) * make_float2 (_S65) - _S66) / make_float2 (_S67);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S72;
    (&_S72)->primal_0 = _S64 / make_float2 (_S65);
    (&_S72)->differential_0 = s_diff_uv_1;
    FixedArray<float, 1>  _S73 = dist_coeffs_4;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S74 = s_fwd_DistNone_distort_0(&_S72, &_S73);
    float _S75 = _S74.differential_0.y * fy_1;
    *&(((&J_1)->rows + (int(0)))->y) = _S74.differential_0.x * fx_1;
    *&(((&J_1)->rows + (int(1)))->y) = _S75;
    float2  s_diff_uv_2 = (make_float2 (0.0f, 0.0f) * make_float2 (_S65) - _S64) / make_float2 (_S67);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S76;
    (&_S76)->primal_0 = _S64 / make_float2 (_S65);
    (&_S76)->differential_0 = s_diff_uv_2;
    FixedArray<float, 1>  _S77 = dist_coeffs_4;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S78 = s_fwd_DistNone_distort_0(&_S76, &_S77);
    float _S79 = _S78.differential_0.y * fy_1;
    *&(((&J_1)->rows + (int(0)))->z) = _S78.differential_0.x * fx_1;
    *&(((&J_1)->rows + (int(1)))->z) = _S79;
    return J_1;
}

inline __device__ Matrix<float, 2, 3>  fisheye_proj_jac_none(float3  p_view_6, float4  intrins_6, FixedArray<float, 1>  dist_coeffs_5)
{
    Matrix<float, 2, 3>  J_2;
    float2  _S80 = float2 {p_view_6.x, p_view_6.y};
    float2  _S81 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S82;
    (&_S82)->primal_0 = _S80;
    (&_S82)->differential_0 = _S81;
    DiffPair_float_0 _S83 = s_fwd_length_impl_0(&_S82);
    float _S84 = p_view_6.z;
    DiffPair_float_0 _S85;
    (&_S85)->primal_0 = _S83.primal_0;
    (&_S85)->differential_0 = _S83.differential_0;
    DiffPair_float_0 _S86;
    (&_S86)->primal_0 = _S84;
    (&_S86)->differential_0 = 0.0f;
    DiffPair_float_0 _S87 = _d_atan2_0(&_S85, &_S86);
    float k_2;
    float s_diff_k_0;
    if((_S87.primal_0) < 0.00100000004749745f)
    {
        float _S88 = _S87.differential_0 * _S87.primal_0;
        float _S89 = 1.0f - _S87.primal_0 * _S87.primal_0 / 3.0f;
        float _S90 = ((0.0f - (_S88 + _S88) * 0.3333333432674408f) * _S84 - _S89 * 0.0f) / (_S84 * _S84);
        k_2 = _S89 / _S84;
        s_diff_k_0 = _S90;
    }
    else
    {
        float _S91 = (_S87.differential_0 * _S83.primal_0 - _S87.primal_0 * _S83.differential_0) / (_S83.primal_0 * _S83.primal_0);
        k_2 = _S87.primal_0 / _S83.primal_0;
        s_diff_k_0 = _S91;
    }
    float2  _S92 = _S80 * make_float2 (k_2);
    float2  _S93 = _S81 * make_float2 (k_2) + make_float2 (s_diff_k_0) * _S80;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S94;
    (&_S94)->primal_0 = _S92;
    (&_S94)->differential_0 = _S93;
    FixedArray<float, 1>  _S95 = dist_coeffs_5;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S96 = s_fwd_DistNone_distort_0(&_S94, &_S95);
    float fx_2 = intrins_6.x;
    float fy_2 = intrins_6.y;
    float _S97 = _S96.differential_0.y * fy_2;
    *&(((&J_2)->rows + (int(0)))->x) = _S96.differential_0.x * fx_2;
    *&(((&J_2)->rows + (int(1)))->x) = _S97;
    float2  _S98 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S99;
    (&_S99)->primal_0 = _S80;
    (&_S99)->differential_0 = _S98;
    DiffPair_float_0 _S100 = s_fwd_length_impl_0(&_S99);
    DiffPair_float_0 _S101;
    (&_S101)->primal_0 = _S100.primal_0;
    (&_S101)->differential_0 = _S100.differential_0;
    DiffPair_float_0 _S102;
    (&_S102)->primal_0 = _S84;
    (&_S102)->differential_0 = 0.0f;
    DiffPair_float_0 _S103 = _d_atan2_0(&_S101, &_S102);
    if((_S103.primal_0) < 0.00100000004749745f)
    {
        float _S104 = _S103.differential_0 * _S103.primal_0;
        float _S105 = 1.0f - _S103.primal_0 * _S103.primal_0 / 3.0f;
        float _S106 = ((0.0f - (_S104 + _S104) * 0.3333333432674408f) * _S84 - _S105 * 0.0f) / (_S84 * _S84);
        k_2 = _S105 / _S84;
        s_diff_k_0 = _S106;
    }
    else
    {
        float _S107 = (_S103.differential_0 * _S100.primal_0 - _S103.primal_0 * _S100.differential_0) / (_S100.primal_0 * _S100.primal_0);
        k_2 = _S103.primal_0 / _S100.primal_0;
        s_diff_k_0 = _S107;
    }
    float2  _S108 = _S80 * make_float2 (k_2);
    float2  _S109 = _S98 * make_float2 (k_2) + make_float2 (s_diff_k_0) * _S80;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S110;
    (&_S110)->primal_0 = _S108;
    (&_S110)->differential_0 = _S109;
    FixedArray<float, 1>  _S111 = dist_coeffs_5;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S112 = s_fwd_DistNone_distort_0(&_S110, &_S111);
    float _S113 = _S112.differential_0.y * fy_2;
    *&(((&J_2)->rows + (int(0)))->y) = _S112.differential_0.x * fx_2;
    *&(((&J_2)->rows + (int(1)))->y) = _S113;
    float2  _S114 = make_float2 (0.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S115;
    (&_S115)->primal_0 = _S80;
    (&_S115)->differential_0 = _S114;
    DiffPair_float_0 _S116 = s_fwd_length_impl_0(&_S115);
    DiffPair_float_0 _S117;
    (&_S117)->primal_0 = _S116.primal_0;
    (&_S117)->differential_0 = _S116.differential_0;
    DiffPair_float_0 _S118;
    (&_S118)->primal_0 = _S84;
    (&_S118)->differential_0 = 1.0f;
    DiffPair_float_0 _S119 = _d_atan2_0(&_S117, &_S118);
    if((_S119.primal_0) < 0.00100000004749745f)
    {
        float _S120 = _S119.differential_0 * _S119.primal_0;
        float _S121 = 1.0f - _S119.primal_0 * _S119.primal_0 / 3.0f;
        float _S122 = ((0.0f - (_S120 + _S120) * 0.3333333432674408f) * _S84 - _S121) / (_S84 * _S84);
        k_2 = _S121 / _S84;
        s_diff_k_0 = _S122;
    }
    else
    {
        float _S123 = (_S119.differential_0 * _S116.primal_0 - _S119.primal_0 * _S116.differential_0) / (_S116.primal_0 * _S116.primal_0);
        k_2 = _S119.primal_0 / _S116.primal_0;
        s_diff_k_0 = _S123;
    }
    float2  _S124 = _S80 * make_float2 (k_2);
    float2  _S125 = _S114 * make_float2 (k_2) + make_float2 (s_diff_k_0) * _S80;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S126;
    (&_S126)->primal_0 = _S124;
    (&_S126)->differential_0 = _S125;
    FixedArray<float, 1>  _S127 = dist_coeffs_5;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S128 = s_fwd_DistNone_distort_0(&_S126, &_S127);
    float _S129 = _S128.differential_0.y * fy_2;
    *&(((&J_2)->rows + (int(0)))->z) = _S128.differential_0.x * fx_2;
    *&(((&J_2)->rows + (int(1)))->z) = _S129;
    return J_2;
}

inline __device__ Matrix<float, 2, 3>  equisolid_proj_jac_none(float3  p_view_7, float4  intrins_7, FixedArray<float, 1>  dist_coeffs_6)
{
    Matrix<float, 2, 3>  J_3;
    float2  _S130 = float2 {p_view_7.x, p_view_7.y};
    float2  _S131 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S132;
    (&_S132)->primal_0 = _S130;
    (&_S132)->differential_0 = _S131;
    DiffPair_float_0 _S133 = s_fwd_length_impl_0(&_S132);
    float _S134 = p_view_7.z;
    DiffPair_float_0 _S135;
    (&_S135)->primal_0 = _S133.primal_0;
    (&_S135)->differential_0 = _S133.differential_0;
    DiffPair_float_0 _S136;
    (&_S136)->primal_0 = _S134;
    (&_S136)->differential_0 = 0.0f;
    DiffPair_float_0 _S137 = _d_atan2_0(&_S135, &_S136);
    float k_3;
    float s_diff_k_1;
    if((_S133.primal_0) < 9.99999997475242708e-07f)
    {
        float _S138 = _S137.differential_0 * _S137.primal_0;
        float _S139 = 1.0f - _S137.primal_0 * _S137.primal_0 / 24.0f;
        float _S140 = ((0.0f - (_S138 + _S138) * 0.0416666679084301f) * _S134 - _S139 * 0.0f) / (_S134 * _S134);
        k_3 = _S139 / _S134;
        s_diff_k_1 = _S140;
    }
    else
    {
        float _S141 = _S137.differential_0 * 0.5f;
        DiffPair_float_0 _S142;
        (&_S142)->primal_0 = 0.5f * _S137.primal_0;
        (&_S142)->differential_0 = _S141;
        DiffPair_float_0 _S143 = _d_sin_0(&_S142);
        float _S144 = 2.0f * _S143.primal_0;
        float _S145 = (_S143.differential_0 * 2.0f * _S133.primal_0 - _S144 * _S133.differential_0) / (_S133.primal_0 * _S133.primal_0);
        k_3 = _S144 / _S133.primal_0;
        s_diff_k_1 = _S145;
    }
    float2  _S146 = _S130 * make_float2 (k_3);
    float2  _S147 = _S131 * make_float2 (k_3) + make_float2 (s_diff_k_1) * _S130;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S148;
    (&_S148)->primal_0 = _S146;
    (&_S148)->differential_0 = _S147;
    FixedArray<float, 1>  _S149 = dist_coeffs_6;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S150 = s_fwd_DistNone_distort_0(&_S148, &_S149);
    float fx_3 = intrins_7.x;
    float fy_3 = intrins_7.y;
    float _S151 = _S150.differential_0.y * fy_3;
    *&(((&J_3)->rows + (int(0)))->x) = _S150.differential_0.x * fx_3;
    *&(((&J_3)->rows + (int(1)))->x) = _S151;
    float2  _S152 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S153;
    (&_S153)->primal_0 = _S130;
    (&_S153)->differential_0 = _S152;
    DiffPair_float_0 _S154 = s_fwd_length_impl_0(&_S153);
    DiffPair_float_0 _S155;
    (&_S155)->primal_0 = _S154.primal_0;
    (&_S155)->differential_0 = _S154.differential_0;
    DiffPair_float_0 _S156;
    (&_S156)->primal_0 = _S134;
    (&_S156)->differential_0 = 0.0f;
    DiffPair_float_0 _S157 = _d_atan2_0(&_S155, &_S156);
    if((_S154.primal_0) < 9.99999997475242708e-07f)
    {
        float _S158 = _S157.differential_0 * _S157.primal_0;
        float _S159 = 1.0f - _S157.primal_0 * _S157.primal_0 / 24.0f;
        float _S160 = ((0.0f - (_S158 + _S158) * 0.0416666679084301f) * _S134 - _S159 * 0.0f) / (_S134 * _S134);
        k_3 = _S159 / _S134;
        s_diff_k_1 = _S160;
    }
    else
    {
        float _S161 = _S157.differential_0 * 0.5f;
        DiffPair_float_0 _S162;
        (&_S162)->primal_0 = 0.5f * _S157.primal_0;
        (&_S162)->differential_0 = _S161;
        DiffPair_float_0 _S163 = _d_sin_0(&_S162);
        float _S164 = 2.0f * _S163.primal_0;
        float _S165 = (_S163.differential_0 * 2.0f * _S154.primal_0 - _S164 * _S154.differential_0) / (_S154.primal_0 * _S154.primal_0);
        k_3 = _S164 / _S154.primal_0;
        s_diff_k_1 = _S165;
    }
    float2  _S166 = _S130 * make_float2 (k_3);
    float2  _S167 = _S152 * make_float2 (k_3) + make_float2 (s_diff_k_1) * _S130;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S168;
    (&_S168)->primal_0 = _S166;
    (&_S168)->differential_0 = _S167;
    FixedArray<float, 1>  _S169 = dist_coeffs_6;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S170 = s_fwd_DistNone_distort_0(&_S168, &_S169);
    float _S171 = _S170.differential_0.y * fy_3;
    *&(((&J_3)->rows + (int(0)))->y) = _S170.differential_0.x * fx_3;
    *&(((&J_3)->rows + (int(1)))->y) = _S171;
    float2  _S172 = make_float2 (0.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S173;
    (&_S173)->primal_0 = _S130;
    (&_S173)->differential_0 = _S172;
    DiffPair_float_0 _S174 = s_fwd_length_impl_0(&_S173);
    DiffPair_float_0 _S175;
    (&_S175)->primal_0 = _S174.primal_0;
    (&_S175)->differential_0 = _S174.differential_0;
    DiffPair_float_0 _S176;
    (&_S176)->primal_0 = _S134;
    (&_S176)->differential_0 = 1.0f;
    DiffPair_float_0 _S177 = _d_atan2_0(&_S175, &_S176);
    if((_S174.primal_0) < 9.99999997475242708e-07f)
    {
        float _S178 = _S177.differential_0 * _S177.primal_0;
        float _S179 = 1.0f - _S177.primal_0 * _S177.primal_0 / 24.0f;
        float _S180 = ((0.0f - (_S178 + _S178) * 0.0416666679084301f) * _S134 - _S179) / (_S134 * _S134);
        k_3 = _S179 / _S134;
        s_diff_k_1 = _S180;
    }
    else
    {
        float _S181 = _S177.differential_0 * 0.5f;
        DiffPair_float_0 _S182;
        (&_S182)->primal_0 = 0.5f * _S177.primal_0;
        (&_S182)->differential_0 = _S181;
        DiffPair_float_0 _S183 = _d_sin_0(&_S182);
        float _S184 = 2.0f * _S183.primal_0;
        float _S185 = (_S183.differential_0 * 2.0f * _S174.primal_0 - _S184 * _S174.differential_0) / (_S174.primal_0 * _S174.primal_0);
        k_3 = _S184 / _S174.primal_0;
        s_diff_k_1 = _S185;
    }
    float2  _S186 = _S130 * make_float2 (k_3);
    float2  _S187 = _S172 * make_float2 (k_3) + make_float2 (s_diff_k_1) * _S130;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S188;
    (&_S188)->primal_0 = _S186;
    (&_S188)->differential_0 = _S187;
    FixedArray<float, 1>  _S189 = dist_coeffs_6;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S190 = s_fwd_DistNone_distort_0(&_S188, &_S189);
    float _S191 = _S190.differential_0.y * fy_3;
    *&(((&J_3)->rows + (int(0)))->z) = _S190.differential_0.x * fx_3;
    *&(((&J_3)->rows + (int(1)))->z) = _S191;
    return J_3;
}

inline __device__ float2  distort_point_none(float2  uv_7, int camera_model_0, FixedArray<float, 1>  dist_coeffs_7)
{
    float2  _S192;
    for(;;)
    {
        if(camera_model_0 == int(3))
        {
            _S192 = uv_7;
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
            _S192 = uv_7 * make_float2 (k_4);
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
                _S192 = uv_7 * make_float2 (k_4);
            }
            else
            {
                _S192 = uv_7;
            }
        }
        FixedArray<float, 1>  _S193 = dist_coeffs_7;
        float2  _S194 = DistNone_distort_0(_S192, &_S193);
        _S192 = _S194;
        break;
    }
    return _S192;
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
    float _S195 = s_diff_u_0 * u_1;
    float _S196 = s_diff_v_0 * v_1;
    float r2_1 = u_1 * u_1 + v_1 * v_1;
    float s_diff_r2_0 = _S195 + _S195 + (_S196 + _S196);
    float _S197 = (*coeffs_3)[int(0)] + r2_1 * (*coeffs_3)[int(1)];
    float radial_0 = 1.0f + r2_1 * _S197;
    float _S198 = 2.0f * (*coeffs_3)[int(2)];
    float _S199 = _S198 * u_1;
    float _S200 = 2.0f * u_1;
    float _S201 = 2.0f * (*coeffs_3)[int(3)];
    float _S202 = _S201 * u_1;
    float _S203 = 2.0f * v_1;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S204 = { dpuv_1->primal_0 * make_float2 (radial_0) + make_float2 (_S199 * v_1 + (*coeffs_3)[int(3)] * (r2_1 + _S200 * u_1), _S202 * v_1 + (*coeffs_3)[int(2)] * (r2_1 + _S203 * v_1)), dpuv_1->differential_0 * make_float2 (radial_0) + make_float2 (s_diff_r2_0 * _S197 + s_diff_r2_0 * (*coeffs_3)[int(1)] * r2_1) * dpuv_1->primal_0 + make_float2 (s_diff_u_0 * _S198 * v_1 + s_diff_v_0 * _S199 + (s_diff_r2_0 + (s_diff_u_0 * 2.0f * u_1 + s_diff_u_0 * _S200)) * (*coeffs_3)[int(3)], s_diff_u_0 * _S201 * v_1 + s_diff_v_0 * _S202 + (s_diff_r2_0 + (s_diff_v_0 * 2.0f * v_1 + s_diff_v_0 * _S203)) * (*coeffs_3)[int(2)]) };
    return _S204;
}

inline __device__ bool undistort_point_1(float2  uv_10, FixedArray<float, 4>  * dist_coeffs_9, int maxiter_1, float2  * uv_undist_1)
{
    int i_5 = int(0);
    float2  q_0 = uv_10;
    for(;;)
    {
        if(i_5 < maxiter_1)
        {
        }
        else
        {
            break;
        }
        float2  _S205 = DistOpenCV_distort_0(q_0, dist_coeffs_9);
        float2  r_7 = _S205 - uv_10;
        float2  _S206 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S207;
        (&_S207)->primal_0 = q_0;
        (&_S207)->differential_0 = _S206;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S208 = s_fwd_DistOpenCV_distort_0(&_S207, dist_coeffs_9);
        float2  _S209 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S210;
        (&_S210)->primal_0 = q_0;
        (&_S210)->differential_0 = _S209;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S211 = s_fwd_DistOpenCV_distort_0(&_S210, dist_coeffs_9);
        Matrix<float, 2, 2>  _S212 = transpose_1(makeMatrix<float, 2, 2> (_S208.differential_0, _S211.differential_0));
        float inv_det_0 = 1.0f / (_S212.rows[int(0)].x * _S212.rows[int(1)].y - _S212.rows[int(0)].y * _S212.rows[int(1)].x);
        float _S213 = r_7.x;
        float _S214 = r_7.y;
        float2  q_1 = q_0 - make_float2 ((_S213 * _S212.rows[int(1)].y - _S214 * _S212.rows[int(0)].y) * inv_det_0, (- _S213 * _S212.rows[int(1)].x + _S214 * _S212.rows[int(0)].x) * inv_det_0);
        i_5 = i_5 + int(1);
        q_0 = q_1;
    }
    *uv_undist_1 = q_0;
    float2  _S215 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S216;
    (&_S216)->primal_0 = q_0;
    (&_S216)->differential_0 = _S215;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S217 = s_fwd_DistOpenCV_distort_0(&_S216, dist_coeffs_9);
    float2  _S218 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S219;
    (&_S219)->primal_0 = q_0;
    (&_S219)->differential_0 = _S218;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S220 = s_fwd_DistOpenCV_distort_0(&_S219, dist_coeffs_9);
    Matrix<float, 2, 2>  _S221 = transpose_1(makeMatrix<float, 2, 2> (_S217.differential_0, _S220.differential_0));
    float _S222 = (F32_min((determinant_0(_S221)), ((F32_min((_S221.rows[int(0)].x), (_S221.rows[int(1)].y))))));
    bool _S223;
    if(_S222 > 0.25f)
    {
        _S223 = _S222 < 4.0f;
    }
    else
    {
        _S223 = false;
    }
    if(_S223)
    {
        float2  _S224 = DistOpenCV_distort_0(q_0, dist_coeffs_9);
        _S223 = (dot_0(q_0, _S224)) >= 0.0f;
    }
    else
    {
        _S223 = false;
    }
    if(_S223)
    {
        float2  _S225 = DistOpenCV_distort_0(*uv_undist_1, dist_coeffs_9);
        _S223 = (length_0(_S225 - uv_10)) < 0.00999999977648258f;
    }
    else
    {
        _S223 = false;
    }
    return _S223;
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
    float _S226 = s_diff_u_1 * u_3;
    float _S227 = s_diff_v_1 * v_3;
    float r2_3 = u_3 * u_3 + v_3 * v_3;
    float s_diff_r2_1 = _S226 + _S226 + (_S227 + _S227);
    float _S228 = (*coeffs_5)[int(2)] + r2_3 * (*coeffs_5)[int(3)];
    float _S229 = (*coeffs_5)[int(1)] + r2_3 * _S228;
    float _S230 = (*coeffs_5)[int(0)] + r2_3 * _S229;
    float radial_1 = 1.0f + r2_3 * _S230;
    float _S231 = 2.0f * (*coeffs_5)[int(4)];
    float _S232 = _S231 * u_3;
    float _S233 = 2.0f * u_3;
    float _S234 = 2.0f * (*coeffs_5)[int(5)];
    float _S235 = _S234 * u_3;
    float _S236 = 2.0f * v_3;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S237 = { dpuv_2->primal_0 * make_float2 (radial_1) + make_float2 (_S232 * v_3 + (*coeffs_5)[int(5)] * (r2_3 + _S233 * u_3) + (*coeffs_5)[int(6)] * r2_3, _S235 * v_3 + (*coeffs_5)[int(4)] * (r2_3 + _S236 * v_3) + (*coeffs_5)[int(7)] * r2_3), dpuv_2->differential_0 * make_float2 (radial_1) + make_float2 (s_diff_r2_1 * _S230 + (s_diff_r2_1 * _S229 + (s_diff_r2_1 * _S228 + s_diff_r2_1 * (*coeffs_5)[int(3)] * r2_3) * r2_3) * r2_3) * dpuv_2->primal_0 + make_float2 (s_diff_u_1 * _S231 * v_3 + s_diff_v_1 * _S232 + (s_diff_r2_1 + (s_diff_u_1 * 2.0f * u_3 + s_diff_u_1 * _S233)) * (*coeffs_5)[int(5)] + s_diff_r2_1 * (*coeffs_5)[int(6)], s_diff_u_1 * _S234 * v_3 + s_diff_v_1 * _S235 + (s_diff_r2_1 + (s_diff_v_1 * 2.0f * v_3 + s_diff_v_1 * _S236)) * (*coeffs_5)[int(4)] + s_diff_r2_1 * (*coeffs_5)[int(7)]) };
    return _S237;
}

inline __device__ bool undistort_point_2(float2  uv_12, FixedArray<float, 8>  * dist_coeffs_10, int maxiter_2, float2  * uv_undist_2)
{
    int i_6 = int(0);
    float2  q_2 = uv_12;
    for(;;)
    {
        if(i_6 < maxiter_2)
        {
        }
        else
        {
            break;
        }
        float2  _S238 = DistThinPrism_distort_0(q_2, dist_coeffs_10);
        float2  r_8 = _S238 - uv_12;
        float2  _S239 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S240;
        (&_S240)->primal_0 = q_2;
        (&_S240)->differential_0 = _S239;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S241 = s_fwd_DistThinPrism_distort_0(&_S240, dist_coeffs_10);
        float2  _S242 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S243;
        (&_S243)->primal_0 = q_2;
        (&_S243)->differential_0 = _S242;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S244 = s_fwd_DistThinPrism_distort_0(&_S243, dist_coeffs_10);
        Matrix<float, 2, 2>  _S245 = transpose_1(makeMatrix<float, 2, 2> (_S241.differential_0, _S244.differential_0));
        float inv_det_1 = 1.0f / (_S245.rows[int(0)].x * _S245.rows[int(1)].y - _S245.rows[int(0)].y * _S245.rows[int(1)].x);
        float _S246 = r_8.x;
        float _S247 = r_8.y;
        float2  q_3 = q_2 - make_float2 ((_S246 * _S245.rows[int(1)].y - _S247 * _S245.rows[int(0)].y) * inv_det_1, (- _S246 * _S245.rows[int(1)].x + _S247 * _S245.rows[int(0)].x) * inv_det_1);
        i_6 = i_6 + int(1);
        q_2 = q_3;
    }
    *uv_undist_2 = q_2;
    float2  _S248 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S249;
    (&_S249)->primal_0 = q_2;
    (&_S249)->differential_0 = _S248;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S250 = s_fwd_DistThinPrism_distort_0(&_S249, dist_coeffs_10);
    float2  _S251 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S252;
    (&_S252)->primal_0 = q_2;
    (&_S252)->differential_0 = _S251;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S253 = s_fwd_DistThinPrism_distort_0(&_S252, dist_coeffs_10);
    Matrix<float, 2, 2>  _S254 = transpose_1(makeMatrix<float, 2, 2> (_S250.differential_0, _S253.differential_0));
    float _S255 = (F32_min((determinant_0(_S254)), ((F32_min((_S254.rows[int(0)].x), (_S254.rows[int(1)].y))))));
    bool _S256;
    if(_S255 > 0.25f)
    {
        _S256 = _S255 < 4.0f;
    }
    else
    {
        _S256 = false;
    }
    if(_S256)
    {
        float2  _S257 = DistThinPrism_distort_0(q_2, dist_coeffs_10);
        _S256 = (dot_0(q_2, _S257)) >= 0.0f;
    }
    else
    {
        _S256 = false;
    }
    if(_S256)
    {
        float2  _S258 = DistThinPrism_distort_0(*uv_undist_2, dist_coeffs_10);
        _S256 = (length_0(_S258 - uv_12)) < 0.00999999977648258f;
    }
    else
    {
        _S256 = false;
    }
    return _S256;
}

inline __device__ bool undistort_point_none(float2  uv_13, int camera_model_1, FixedArray<float, 1>  dist_coeffs_11, float2  * uv_undist_3)
{
    bool _S259;
    for(;;)
    {
        *uv_undist_3 = make_float2 (0.0f);
        if(camera_model_1 == int(3))
        {
            float lon_0 = uv_13.x;
            float lat_0 = uv_13.y;
            float cl_0 = (F32_cos((lat_0)));
            *uv_undist_3 = make_float2 (cl_0 * (F32_sin((lon_0))), (F32_sin((lat_0)))) / make_float2 ((F32_max((cl_0 * (F32_cos((lon_0)))), (9.999999960041972e-13f))));
            _S259 = true;
            break;
        }
        FixedArray<float, 1>  _S260 = dist_coeffs_11;
        float2  uv_u_0;
        bool _S261 = undistort_point_0(uv_13, &_S260, int(8), &uv_u_0);
        if(!_S261)
        {
            _S259 = false;
            break;
        }
        float2  _S262 = uv_u_0;
        float3  raydir_0;
        if(camera_model_1 == int(1))
        {
            float r_9 = length_0(_S262);
            float s_0;
            if(r_9 < 0.00100000004749745f)
            {
                s_0 = 1.0f - r_9 * r_9 / 6.0f;
            }
            else
            {
                s_0 = (F32_sin((r_9))) / r_9;
            }
            raydir_0 = make_float3 ((_S262 * make_float2 (s_0)).x, (_S262 * make_float2 (s_0)).y, (F32_cos((r_9))));
        }
        else
        {
            if(camera_model_1 == int(2))
            {
                float r_10 = length_0(_S262);
                raydir_0 = make_float3 ((_S262 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_10 * r_10)))))))).x, (_S262 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_10 * r_10)))))))).y, 1.0f - 0.5f * r_10 * r_10);
            }
            else
            {
                raydir_0 = make_float3 (_S262.x, _S262.y, 1.0f);
            }
        }
        *uv_undist_3 = float2 {raydir_0.x, raydir_0.y} / make_float2 ((F32_max((raydir_0.z), (9.999999960041972e-13f))));
        _S259 = true;
        break;
    }
    return _S259;
}

inline __device__ bool unproject_point_none(float2  uv_14, int camera_model_2, FixedArray<float, 1>  dist_coeffs_12, float3  * raydir_1)
{
    bool _S263;
    for(;;)
    {
        if(camera_model_2 == int(4))
        {
            *raydir_1 = make_float3 (0.0f, 0.0f, 1.0f);
            _S263 = true;
            break;
        }
        int3  _S264 = make_int3 (int(0));
        float3  _S265 = make_float3 ((float)_S264.x, (float)_S264.y, (float)_S264.z);
        *raydir_1 = _S265;
        if(camera_model_2 == int(3))
        {
            float lon_1 = uv_14.x;
            float lat_1 = uv_14.y;
            float cl_1 = (F32_cos((lat_1)));
            *raydir_1 = make_float3 (cl_1 * (F32_sin((lon_1))), (F32_sin((lat_1))), cl_1 * (F32_cos((lon_1))));
            _S263 = true;
            break;
        }
        FixedArray<float, 1>  _S266 = dist_coeffs_12;
        float2  uv_u_1;
        bool _S267 = undistort_point_0(uv_14, &_S266, int(8), &uv_u_1);
        if(!_S267)
        {
            _S263 = false;
            break;
        }
        float2  _S268 = uv_u_1;
        if(camera_model_2 == int(1))
        {
            float r_11 = length_0(_S268);
            float s_1;
            if(r_11 < 0.00100000004749745f)
            {
                s_1 = 1.0f - r_11 * r_11 / 6.0f;
            }
            else
            {
                s_1 = (F32_sin((r_11))) / r_11;
            }
            *raydir_1 = make_float3 ((_S268 * make_float2 (s_1)).x, (_S268 * make_float2 (s_1)).y, (F32_cos((r_11))));
        }
        else
        {
            if(camera_model_2 == int(2))
            {
                float r_12 = length_0(_S268);
                *raydir_1 = make_float3 ((_S268 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_12 * r_12)))))))).x, (_S268 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_12 * r_12)))))))).y, 1.0f - 0.5f * r_12 * r_12);
            }
            else
            {
                *raydir_1 = make_float3 (_S268.x, _S268.y, 1.0f);
            }
        }
        _S263 = true;
        break;
    }
    return _S263;
}

inline __device__ float3  normalize_0(float3  x_13)
{
    return x_13 / make_float3 (length_1(x_13));
}

inline __device__ float4  normalize_1(float4  x_14)
{
    return x_14 / make_float4 (length_2(x_14));
}

inline __device__ bool generate_ray_none(float2  uv_15, int camera_model_3, FixedArray<float, 1>  dist_coeffs_13, float3  * raydir_2)
{
    bool _S269;
    for(;;)
    {
        if(camera_model_3 == int(4))
        {
            *raydir_2 = make_float3 (0.0f, 0.0f, 1.0f);
            _S269 = true;
            break;
        }
        if(camera_model_3 == int(3))
        {
            float _S270 = uv_15.x;
            if((F32_abs((_S270))) > 3.14159274101257324f)
            {
                _S269 = true;
            }
            else
            {
                _S269 = (F32_abs((uv_15.y))) > 1.57079637050628662f;
            }
            if(_S269)
            {
                int3  _S271 = make_int3 (int(0));
                float3  _S272 = make_float3 ((float)_S271.x, (float)_S271.y, (float)_S271.z);
                *raydir_2 = _S272;
                _S269 = false;
                break;
            }
            float lat_2 = uv_15.y;
            float cl_2 = (F32_cos((lat_2)));
            *raydir_2 = make_float3 (cl_2 * (F32_sin((_S270))), (F32_sin((lat_2))), cl_2 * (F32_cos((_S270))));
            _S269 = true;
            break;
        }
        FixedArray<float, 1>  _S273 = dist_coeffs_13;
        float2  uv_u_2;
        bool _S274 = undistort_point_0(uv_15, &_S273, int(8), &uv_u_2);
        if(!_S274)
        {
            int3  _S275 = make_int3 (int(0));
            float3  _S276 = make_float3 ((float)_S275.x, (float)_S275.y, (float)_S275.z);
            *raydir_2 = _S276;
            _S269 = false;
            break;
        }
        float2  _S277 = uv_u_2;
        if(camera_model_3 == int(1))
        {
            float r_13 = length_0(_S277);
            if(r_13 >= 3.14159274101257324f)
            {
                int3  _S278 = make_int3 (int(0));
                float3  _S279 = make_float3 ((float)_S278.x, (float)_S278.y, (float)_S278.z);
                *raydir_2 = _S279;
                _S269 = false;
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
            *raydir_2 = make_float3 ((_S277 * make_float2 (s_2)).x, (_S277 * make_float2 (s_2)).y, (F32_cos((r_13))));
        }
        else
        {
            if(camera_model_3 == int(2))
            {
                float r_14 = length_0(_S277);
                if(r_14 >= 2.0f)
                {
                    int3  _S280 = make_int3 (int(0));
                    float3  _S281 = make_float3 ((float)_S280.x, (float)_S280.y, (float)_S280.z);
                    *raydir_2 = _S281;
                    _S269 = false;
                    break;
                }
                *raydir_2 = make_float3 ((_S277 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_14 * r_14)))))))).x, (_S277 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_14 * r_14)))))))).y, 1.0f - 0.5f * r_14 * r_14);
            }
            else
            {
                *raydir_2 = make_float3 (_S277.x, _S277.y, 1.0f);
            }
        }
        *raydir_2 = normalize_0(*raydir_2);
        _S269 = true;
        break;
    }
    return _S269;
}

inline __device__ void _d_cross_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * a_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * b_0, float3  dOut_4)
{
    float _S282 = dOut_4.y;
    float _S283 = dOut_4.z;
    float _S284 = dOut_4.x;
    float _S285 = (*a_0).primal_0.z * _S282 + - (*a_0).primal_0.y * _S283;
    float _S286 = - (*a_0).primal_0.z * _S284 + (*a_0).primal_0.x * _S283;
    float _S287 = (*a_0).primal_0.y * _S284 + - (*a_0).primal_0.x * _S282;
    float3  _S288 = make_float3 (- (*b_0).primal_0.z * _S282 + (*b_0).primal_0.y * _S283, (*b_0).primal_0.z * _S284 + - (*b_0).primal_0.x * _S283, - (*b_0).primal_0.y * _S284 + (*b_0).primal_0.x * _S282);
    a_0->primal_0 = (*a_0).primal_0;
    a_0->differential_0 = _S288;
    float3  _S289 = make_float3 (_S285, _S286, _S287);
    b_0->primal_0 = (*b_0).primal_0;
    b_0->differential_0 = _S289;
    return;
}

inline __device__ float3  cross_0(float3  left_4, float3  right_4)
{
    float _S290 = left_4.y;
    float _S291 = right_4.z;
    float _S292 = left_4.z;
    float _S293 = right_4.y;
    float _S294 = right_4.x;
    float _S295 = left_4.x;
    return make_float3 (_S290 * _S291 - _S292 * _S293, _S292 * _S294 - _S295 * _S291, _S295 * _S293 - _S290 * _S294);
}

inline __device__ void _d_rsqrt_0(DiffPair_float_0 * dpx_6, float dOut_5)
{
    float _S296 = -0.5f / ((*dpx_6).primal_0 * (F32_sqrt(((*dpx_6).primal_0)))) * dOut_5;
    dpx_6->primal_0 = (*dpx_6).primal_0;
    dpx_6->differential_0 = _S296;
    return;
}

inline __device__ bool pixel_rect_quad_0(int camera_model_4, float4  intrins_8, FixedArray<float, 1>  * dist_coeffs_14, float2  lo_0, float2  hi_0, float3  * q0_0, float3  * q1_0, float3  * q2_0, float3  * q3_0)
{
    bool ok_0;
    float s_3;
    float2  _S297;
    float2  _S298;
    float3  * _S299;
    bool _S300;
    float3  _S301 = make_float3 (0.0f);
    *q0_0 = _S301;
    *q1_0 = _S301;
    *q2_0 = *q0_0;
    *q3_0 = *q0_0;
    float2  _S302 = make_float2 (hi_0.x, lo_0.y);
    float2  _S303 = make_float2 (lo_0.x, hi_0.y);
    FixedArray<float3 , 4>  r_15;
    FixedArray<float3 , 4>  n_0;
    for(;;)
    {
        float2  _S304 = float2 {intrins_8.z, intrins_8.w};
        _S297 = _S304;
        float2  _S305 = lo_0 - _S304;
        float2  _S306 = float2 {intrins_8.x, intrins_8.y};
        _S298 = _S306;
        float2  _S307 = _S305 / _S306;
        _S299 = &r_15[int(0)];
        for(;;)
        {
            bool _S308 = camera_model_4 == int(4);
            _S300 = _S308;
            if(_S308)
            {
                r_15[int(0)] = make_float3 (0.0f, 0.0f, 1.0f);
                ok_0 = true;
                break;
            }
            if(camera_model_4 == int(3))
            {
                float _S309 = _S307.x;
                if((F32_abs((_S309))) > 3.14159274101257324f)
                {
                    ok_0 = true;
                }
                else
                {
                    ok_0 = (F32_abs((_S307.y))) > 1.57079637050628662f;
                }
                if(ok_0)
                {
                    int3  _S310 = make_int3 (int(0));
                    float3  _S311 = make_float3 ((float)_S310.x, (float)_S310.y, (float)_S310.z);
                    r_15[int(0)] = _S311;
                    ok_0 = false;
                    break;
                }
                float lat_3 = _S307.y;
                float cl_3 = (F32_cos((lat_3)));
                r_15[int(0)] = make_float3 (cl_3 * (F32_sin((_S309))), (F32_sin((lat_3))), cl_3 * (F32_cos((_S309))));
                ok_0 = true;
                break;
            }
            float2  uv_u_3;
            bool _S312 = undistort_point_0(_S307, dist_coeffs_14, int(8), &uv_u_3);
            if(!_S312)
            {
                int3  _S313 = make_int3 (int(0));
                float3  _S314 = make_float3 ((float)_S313.x, (float)_S313.y, (float)_S313.z);
                r_15[int(0)] = _S314;
                ok_0 = false;
                break;
            }
            float2  _S315 = uv_u_3;
            if(camera_model_4 == int(1))
            {
                float r_16 = length_0(_S315);
                if(r_16 >= 3.14159274101257324f)
                {
                    int3  _S316 = make_int3 (int(0));
                    float3  _S317 = make_float3 ((float)_S316.x, (float)_S316.y, (float)_S316.z);
                    r_15[int(0)] = _S317;
                    ok_0 = false;
                    break;
                }
                if(r_16 < 0.00100000004749745f)
                {
                    s_3 = 1.0f - r_16 * r_16 / 6.0f;
                }
                else
                {
                    s_3 = (F32_sin((r_16))) / r_16;
                }
                r_15[int(0)] = make_float3 ((_S315 * make_float2 (s_3)).x, (_S315 * make_float2 (s_3)).y, (F32_cos((r_16))));
            }
            else
            {
                if(camera_model_4 == int(2))
                {
                    float r_17 = length_0(_S315);
                    if(r_17 >= 2.0f)
                    {
                        int3  _S318 = make_int3 (int(0));
                        float3  _S319 = make_float3 ((float)_S318.x, (float)_S318.y, (float)_S318.z);
                        r_15[int(0)] = _S319;
                        ok_0 = false;
                        break;
                    }
                    r_15[int(0)] = make_float3 ((_S315 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_17 * r_17)))))))).x, (_S315 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_17 * r_17)))))))).y, 1.0f - 0.5f * r_17 * r_17);
                }
                else
                {
                    r_15[int(0)] = make_float3 (_S315.x, _S315.y, 1.0f);
                }
            }
            r_15[int(0)] = normalize_0(r_15[int(0)]);
            ok_0 = true;
            break;
        }
        if(ok_0)
        {
            float2  _S320 = (_S302 - _S304) / _S306;
            for(;;)
            {
                if(_S300)
                {
                    r_15[int(1)] = make_float3 (0.0f, 0.0f, 1.0f);
                    ok_0 = true;
                    break;
                }
                if(camera_model_4 == int(3))
                {
                    float _S321 = _S320.x;
                    if((F32_abs((_S321))) > 3.14159274101257324f)
                    {
                        ok_0 = true;
                    }
                    else
                    {
                        ok_0 = (F32_abs((_S320.y))) > 1.57079637050628662f;
                    }
                    if(ok_0)
                    {
                        int3  _S322 = make_int3 (int(0));
                        float3  _S323 = make_float3 ((float)_S322.x, (float)_S322.y, (float)_S322.z);
                        r_15[int(1)] = _S323;
                        ok_0 = false;
                        break;
                    }
                    float lat_4 = _S320.y;
                    float cl_4 = (F32_cos((lat_4)));
                    r_15[int(1)] = make_float3 (cl_4 * (F32_sin((_S321))), (F32_sin((lat_4))), cl_4 * (F32_cos((_S321))));
                    ok_0 = true;
                    break;
                }
                float2  uv_u_4;
                bool _S324 = undistort_point_0(_S320, dist_coeffs_14, int(8), &uv_u_4);
                if(!_S324)
                {
                    int3  _S325 = make_int3 (int(0));
                    float3  _S326 = make_float3 ((float)_S325.x, (float)_S325.y, (float)_S325.z);
                    r_15[int(1)] = _S326;
                    ok_0 = false;
                    break;
                }
                float2  _S327 = uv_u_4;
                if(camera_model_4 == int(1))
                {
                    float r_18 = length_0(_S327);
                    if(r_18 >= 3.14159274101257324f)
                    {
                        int3  _S328 = make_int3 (int(0));
                        float3  _S329 = make_float3 ((float)_S328.x, (float)_S328.y, (float)_S328.z);
                        r_15[int(1)] = _S329;
                        ok_0 = false;
                        break;
                    }
                    if(r_18 < 0.00100000004749745f)
                    {
                        s_3 = 1.0f - r_18 * r_18 / 6.0f;
                    }
                    else
                    {
                        s_3 = (F32_sin((r_18))) / r_18;
                    }
                    r_15[int(1)] = make_float3 ((_S327 * make_float2 (s_3)).x, (_S327 * make_float2 (s_3)).y, (F32_cos((r_18))));
                }
                else
                {
                    if(camera_model_4 == int(2))
                    {
                        float r_19 = length_0(_S327);
                        if(r_19 >= 2.0f)
                        {
                            int3  _S330 = make_int3 (int(0));
                            float3  _S331 = make_float3 ((float)_S330.x, (float)_S330.y, (float)_S330.z);
                            r_15[int(1)] = _S331;
                            ok_0 = false;
                            break;
                        }
                        r_15[int(1)] = make_float3 ((_S327 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_19 * r_19)))))))).x, (_S327 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_19 * r_19)))))))).y, 1.0f - 0.5f * r_19 * r_19);
                    }
                    else
                    {
                        r_15[int(1)] = make_float3 (_S327.x, _S327.y, 1.0f);
                    }
                }
                r_15[int(1)] = normalize_0(r_15[int(1)]);
                ok_0 = true;
                break;
            }
        }
        else
        {
            ok_0 = false;
        }
        if(ok_0)
        {
            float2  _S332 = (hi_0 - _S304) / _S306;
            for(;;)
            {
                if(_S300)
                {
                    r_15[int(2)] = make_float3 (0.0f, 0.0f, 1.0f);
                    ok_0 = true;
                    break;
                }
                if(camera_model_4 == int(3))
                {
                    float _S333 = _S332.x;
                    if((F32_abs((_S333))) > 3.14159274101257324f)
                    {
                        ok_0 = true;
                    }
                    else
                    {
                        ok_0 = (F32_abs((_S332.y))) > 1.57079637050628662f;
                    }
                    if(ok_0)
                    {
                        int3  _S334 = make_int3 (int(0));
                        float3  _S335 = make_float3 ((float)_S334.x, (float)_S334.y, (float)_S334.z);
                        r_15[int(2)] = _S335;
                        ok_0 = false;
                        break;
                    }
                    float lat_5 = _S332.y;
                    float cl_5 = (F32_cos((lat_5)));
                    r_15[int(2)] = make_float3 (cl_5 * (F32_sin((_S333))), (F32_sin((lat_5))), cl_5 * (F32_cos((_S333))));
                    ok_0 = true;
                    break;
                }
                float2  uv_u_5;
                bool _S336 = undistort_point_0(_S332, dist_coeffs_14, int(8), &uv_u_5);
                if(!_S336)
                {
                    int3  _S337 = make_int3 (int(0));
                    float3  _S338 = make_float3 ((float)_S337.x, (float)_S337.y, (float)_S337.z);
                    r_15[int(2)] = _S338;
                    ok_0 = false;
                    break;
                }
                float2  _S339 = uv_u_5;
                if(camera_model_4 == int(1))
                {
                    float r_20 = length_0(_S339);
                    if(r_20 >= 3.14159274101257324f)
                    {
                        int3  _S340 = make_int3 (int(0));
                        float3  _S341 = make_float3 ((float)_S340.x, (float)_S340.y, (float)_S340.z);
                        r_15[int(2)] = _S341;
                        ok_0 = false;
                        break;
                    }
                    if(r_20 < 0.00100000004749745f)
                    {
                        s_3 = 1.0f - r_20 * r_20 / 6.0f;
                    }
                    else
                    {
                        s_3 = (F32_sin((r_20))) / r_20;
                    }
                    r_15[int(2)] = make_float3 ((_S339 * make_float2 (s_3)).x, (_S339 * make_float2 (s_3)).y, (F32_cos((r_20))));
                }
                else
                {
                    if(camera_model_4 == int(2))
                    {
                        float r_21 = length_0(_S339);
                        if(r_21 >= 2.0f)
                        {
                            int3  _S342 = make_int3 (int(0));
                            float3  _S343 = make_float3 ((float)_S342.x, (float)_S342.y, (float)_S342.z);
                            r_15[int(2)] = _S343;
                            ok_0 = false;
                            break;
                        }
                        r_15[int(2)] = make_float3 ((_S339 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_21 * r_21)))))))).x, (_S339 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_21 * r_21)))))))).y, 1.0f - 0.5f * r_21 * r_21);
                    }
                    else
                    {
                        r_15[int(2)] = make_float3 (_S339.x, _S339.y, 1.0f);
                    }
                }
                r_15[int(2)] = normalize_0(r_15[int(2)]);
                ok_0 = true;
                break;
            }
        }
        else
        {
            ok_0 = false;
        }
        if(ok_0)
        {
            float2  _S344 = (_S303 - _S304) / _S306;
            for(;;)
            {
                if(_S300)
                {
                    r_15[int(3)] = make_float3 (0.0f, 0.0f, 1.0f);
                    ok_0 = true;
                    break;
                }
                if(camera_model_4 == int(3))
                {
                    float _S345 = _S344.x;
                    if((F32_abs((_S345))) > 3.14159274101257324f)
                    {
                        ok_0 = true;
                    }
                    else
                    {
                        ok_0 = (F32_abs((_S344.y))) > 1.57079637050628662f;
                    }
                    if(ok_0)
                    {
                        int3  _S346 = make_int3 (int(0));
                        float3  _S347 = make_float3 ((float)_S346.x, (float)_S346.y, (float)_S346.z);
                        r_15[int(3)] = _S347;
                        ok_0 = false;
                        break;
                    }
                    float lat_6 = _S344.y;
                    float cl_6 = (F32_cos((lat_6)));
                    r_15[int(3)] = make_float3 (cl_6 * (F32_sin((_S345))), (F32_sin((lat_6))), cl_6 * (F32_cos((_S345))));
                    ok_0 = true;
                    break;
                }
                float2  uv_u_6;
                bool _S348 = undistort_point_0(_S344, dist_coeffs_14, int(8), &uv_u_6);
                if(!_S348)
                {
                    int3  _S349 = make_int3 (int(0));
                    float3  _S350 = make_float3 ((float)_S349.x, (float)_S349.y, (float)_S349.z);
                    r_15[int(3)] = _S350;
                    ok_0 = false;
                    break;
                }
                float2  _S351 = uv_u_6;
                if(camera_model_4 == int(1))
                {
                    float r_22 = length_0(_S351);
                    if(r_22 >= 3.14159274101257324f)
                    {
                        int3  _S352 = make_int3 (int(0));
                        float3  _S353 = make_float3 ((float)_S352.x, (float)_S352.y, (float)_S352.z);
                        r_15[int(3)] = _S353;
                        ok_0 = false;
                        break;
                    }
                    if(r_22 < 0.00100000004749745f)
                    {
                        s_3 = 1.0f - r_22 * r_22 / 6.0f;
                    }
                    else
                    {
                        s_3 = (F32_sin((r_22))) / r_22;
                    }
                    r_15[int(3)] = make_float3 ((_S351 * make_float2 (s_3)).x, (_S351 * make_float2 (s_3)).y, (F32_cos((r_22))));
                }
                else
                {
                    if(camera_model_4 == int(2))
                    {
                        float r_23 = length_0(_S351);
                        if(r_23 >= 2.0f)
                        {
                            int3  _S354 = make_int3 (int(0));
                            float3  _S355 = make_float3 ((float)_S354.x, (float)_S354.y, (float)_S354.z);
                            r_15[int(3)] = _S355;
                            ok_0 = false;
                            break;
                        }
                        r_15[int(3)] = make_float3 ((_S351 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_23 * r_23)))))))).x, (_S351 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_23 * r_23)))))))).y, 1.0f - 0.5f * r_23 * r_23);
                    }
                    else
                    {
                        r_15[int(3)] = make_float3 (_S351.x, _S351.y, 1.0f);
                    }
                }
                r_15[int(3)] = normalize_0(r_15[int(3)]);
                ok_0 = true;
                break;
            }
        }
        else
        {
            ok_0 = false;
        }
        break;
    }
    if(ok_0)
    {
        ok_0 = (dot_1(*_S299, r_15[int(2)])) > 0.5f;
    }
    else
    {
        ok_0 = false;
    }
    if(ok_0)
    {
        ok_0 = (dot_1(r_15[int(1)], r_15[int(3)])) > 0.5f;
    }
    else
    {
        ok_0 = false;
    }
    float3  mid_0;
    if(ok_0)
    {
        float2  _S356 = (make_float2 (0.5f) * (lo_0 + _S302) - _S297) / _S298;
        for(;;)
        {
            if(_S300)
            {
                float3  _S357 = make_float3 (0.0f, 0.0f, 1.0f);
                ok_0 = true;
                mid_0 = _S357;
                break;
            }
            if(camera_model_4 == int(3))
            {
                float _S358 = _S356.x;
                if((F32_abs((_S358))) > 3.14159274101257324f)
                {
                    ok_0 = true;
                }
                else
                {
                    ok_0 = (F32_abs((_S356.y))) > 1.57079637050628662f;
                }
                if(ok_0)
                {
                    int3  _S359 = make_int3 (int(0));
                    float3  _S360 = make_float3 ((float)_S359.x, (float)_S359.y, (float)_S359.z);
                    ok_0 = false;
                    mid_0 = _S360;
                    break;
                }
                float lat_7 = _S356.y;
                float cl_7 = (F32_cos((lat_7)));
                float3  _S361 = make_float3 (cl_7 * (F32_sin((_S358))), (F32_sin((lat_7))), cl_7 * (F32_cos((_S358))));
                ok_0 = true;
                mid_0 = _S361;
                break;
            }
            float2  uv_u_7;
            bool _S362 = undistort_point_0(_S356, dist_coeffs_14, int(8), &uv_u_7);
            if(!_S362)
            {
                int3  _S363 = make_int3 (int(0));
                float3  _S364 = make_float3 ((float)_S363.x, (float)_S363.y, (float)_S363.z);
                ok_0 = false;
                mid_0 = _S364;
                break;
            }
            float2  _S365 = uv_u_7;
            if(camera_model_4 == int(1))
            {
                float r_24 = length_0(_S365);
                if(r_24 >= 3.14159274101257324f)
                {
                    int3  _S366 = make_int3 (int(0));
                    float3  _S367 = make_float3 ((float)_S366.x, (float)_S366.y, (float)_S366.z);
                    ok_0 = false;
                    mid_0 = _S367;
                    break;
                }
                if(r_24 < 0.00100000004749745f)
                {
                    s_3 = 1.0f - r_24 * r_24 / 6.0f;
                }
                else
                {
                    s_3 = (F32_sin((r_24))) / r_24;
                }
                mid_0 = make_float3 ((_S365 * make_float2 (s_3)).x, (_S365 * make_float2 (s_3)).y, (F32_cos((r_24))));
            }
            else
            {
                if(camera_model_4 == int(2))
                {
                    float r_25 = length_0(_S365);
                    if(r_25 >= 2.0f)
                    {
                        int3  _S368 = make_int3 (int(0));
                        float3  _S369 = make_float3 ((float)_S368.x, (float)_S368.y, (float)_S368.z);
                        ok_0 = false;
                        mid_0 = _S369;
                        break;
                    }
                    mid_0 = make_float3 ((_S365 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_25 * r_25)))))))).x, (_S365 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_25 * r_25)))))))).y, 1.0f - 0.5f * r_25 * r_25);
                }
                else
                {
                    mid_0 = make_float3 (_S365.x, _S365.y, 1.0f);
                }
            }
            float3  _S370 = normalize_0(mid_0);
            ok_0 = true;
            mid_0 = _S370;
            break;
        }
    }
    else
    {
        ok_0 = false;
    }
    float3  along_0 = r_15[int(1)] - *_S299;
    float3  _S371 = cross_0(*_S299, along_0);
    n_0[int(0)] = _S371;
    float3  chord_0 = normalize_0(*_S299 + r_15[int(1)]);
    if((dot_1(_S371, mid_0)) < 0.0f)
    {
        n_0[int(0)] = cross_0(mid_0 + (mid_0 - chord_0), along_0);
    }
    if(ok_0)
    {
        float2  _S372 = (make_float2 (0.5f) * (_S302 + hi_0) - _S297) / _S298;
        for(;;)
        {
            if(_S300)
            {
                float3  _S373 = make_float3 (0.0f, 0.0f, 1.0f);
                ok_0 = true;
                mid_0 = _S373;
                break;
            }
            if(camera_model_4 == int(3))
            {
                float _S374 = _S372.x;
                if((F32_abs((_S374))) > 3.14159274101257324f)
                {
                    ok_0 = true;
                }
                else
                {
                    ok_0 = (F32_abs((_S372.y))) > 1.57079637050628662f;
                }
                if(ok_0)
                {
                    int3  _S375 = make_int3 (int(0));
                    float3  _S376 = make_float3 ((float)_S375.x, (float)_S375.y, (float)_S375.z);
                    ok_0 = false;
                    mid_0 = _S376;
                    break;
                }
                float lat_8 = _S372.y;
                float cl_8 = (F32_cos((lat_8)));
                float3  _S377 = make_float3 (cl_8 * (F32_sin((_S374))), (F32_sin((lat_8))), cl_8 * (F32_cos((_S374))));
                ok_0 = true;
                mid_0 = _S377;
                break;
            }
            float2  uv_u_8;
            bool _S378 = undistort_point_0(_S372, dist_coeffs_14, int(8), &uv_u_8);
            if(!_S378)
            {
                int3  _S379 = make_int3 (int(0));
                float3  _S380 = make_float3 ((float)_S379.x, (float)_S379.y, (float)_S379.z);
                ok_0 = false;
                mid_0 = _S380;
                break;
            }
            float2  _S381 = uv_u_8;
            if(camera_model_4 == int(1))
            {
                float r_26 = length_0(_S381);
                if(r_26 >= 3.14159274101257324f)
                {
                    int3  _S382 = make_int3 (int(0));
                    float3  _S383 = make_float3 ((float)_S382.x, (float)_S382.y, (float)_S382.z);
                    ok_0 = false;
                    mid_0 = _S383;
                    break;
                }
                if(r_26 < 0.00100000004749745f)
                {
                    s_3 = 1.0f - r_26 * r_26 / 6.0f;
                }
                else
                {
                    s_3 = (F32_sin((r_26))) / r_26;
                }
                mid_0 = make_float3 ((_S381 * make_float2 (s_3)).x, (_S381 * make_float2 (s_3)).y, (F32_cos((r_26))));
            }
            else
            {
                if(camera_model_4 == int(2))
                {
                    float r_27 = length_0(_S381);
                    if(r_27 >= 2.0f)
                    {
                        int3  _S384 = make_int3 (int(0));
                        float3  _S385 = make_float3 ((float)_S384.x, (float)_S384.y, (float)_S384.z);
                        ok_0 = false;
                        mid_0 = _S385;
                        break;
                    }
                    mid_0 = make_float3 ((_S381 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_27 * r_27)))))))).x, (_S381 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_27 * r_27)))))))).y, 1.0f - 0.5f * r_27 * r_27);
                }
                else
                {
                    mid_0 = make_float3 (_S381.x, _S381.y, 1.0f);
                }
            }
            float3  _S386 = normalize_0(mid_0);
            ok_0 = true;
            mid_0 = _S386;
            break;
        }
    }
    else
    {
        ok_0 = false;
    }
    float3  along_1 = r_15[int(2)] - r_15[int(1)];
    float3  _S387 = cross_0(r_15[int(1)], along_1);
    n_0[int(1)] = _S387;
    float3  chord_1 = normalize_0(r_15[int(1)] + r_15[int(2)]);
    if((dot_1(_S387, mid_0)) < 0.0f)
    {
        n_0[int(1)] = cross_0(mid_0 + (mid_0 - chord_1), along_1);
    }
    if(ok_0)
    {
        float2  _S388 = (make_float2 (0.5f) * (hi_0 + _S303) - _S297) / _S298;
        for(;;)
        {
            if(_S300)
            {
                float3  _S389 = make_float3 (0.0f, 0.0f, 1.0f);
                ok_0 = true;
                mid_0 = _S389;
                break;
            }
            if(camera_model_4 == int(3))
            {
                float _S390 = _S388.x;
                if((F32_abs((_S390))) > 3.14159274101257324f)
                {
                    ok_0 = true;
                }
                else
                {
                    ok_0 = (F32_abs((_S388.y))) > 1.57079637050628662f;
                }
                if(ok_0)
                {
                    int3  _S391 = make_int3 (int(0));
                    float3  _S392 = make_float3 ((float)_S391.x, (float)_S391.y, (float)_S391.z);
                    ok_0 = false;
                    mid_0 = _S392;
                    break;
                }
                float lat_9 = _S388.y;
                float cl_9 = (F32_cos((lat_9)));
                float3  _S393 = make_float3 (cl_9 * (F32_sin((_S390))), (F32_sin((lat_9))), cl_9 * (F32_cos((_S390))));
                ok_0 = true;
                mid_0 = _S393;
                break;
            }
            float2  uv_u_9;
            bool _S394 = undistort_point_0(_S388, dist_coeffs_14, int(8), &uv_u_9);
            if(!_S394)
            {
                int3  _S395 = make_int3 (int(0));
                float3  _S396 = make_float3 ((float)_S395.x, (float)_S395.y, (float)_S395.z);
                ok_0 = false;
                mid_0 = _S396;
                break;
            }
            float2  _S397 = uv_u_9;
            if(camera_model_4 == int(1))
            {
                float r_28 = length_0(_S397);
                if(r_28 >= 3.14159274101257324f)
                {
                    int3  _S398 = make_int3 (int(0));
                    float3  _S399 = make_float3 ((float)_S398.x, (float)_S398.y, (float)_S398.z);
                    ok_0 = false;
                    mid_0 = _S399;
                    break;
                }
                if(r_28 < 0.00100000004749745f)
                {
                    s_3 = 1.0f - r_28 * r_28 / 6.0f;
                }
                else
                {
                    s_3 = (F32_sin((r_28))) / r_28;
                }
                mid_0 = make_float3 ((_S397 * make_float2 (s_3)).x, (_S397 * make_float2 (s_3)).y, (F32_cos((r_28))));
            }
            else
            {
                if(camera_model_4 == int(2))
                {
                    float r_29 = length_0(_S397);
                    if(r_29 >= 2.0f)
                    {
                        int3  _S400 = make_int3 (int(0));
                        float3  _S401 = make_float3 ((float)_S400.x, (float)_S400.y, (float)_S400.z);
                        ok_0 = false;
                        mid_0 = _S401;
                        break;
                    }
                    mid_0 = make_float3 ((_S397 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_29 * r_29)))))))).x, (_S397 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_29 * r_29)))))))).y, 1.0f - 0.5f * r_29 * r_29);
                }
                else
                {
                    mid_0 = make_float3 (_S397.x, _S397.y, 1.0f);
                }
            }
            float3  _S402 = normalize_0(mid_0);
            ok_0 = true;
            mid_0 = _S402;
            break;
        }
    }
    else
    {
        ok_0 = false;
    }
    float3  along_2 = r_15[int(3)] - r_15[int(2)];
    float3  _S403 = cross_0(r_15[int(2)], along_2);
    n_0[int(2)] = _S403;
    float3  chord_2 = normalize_0(r_15[int(2)] + r_15[int(3)]);
    if((dot_1(_S403, mid_0)) < 0.0f)
    {
        n_0[int(2)] = cross_0(mid_0 + (mid_0 - chord_2), along_2);
    }
    if(ok_0)
    {
        float2  _S404 = (make_float2 (0.5f) * (_S303 + lo_0) - _S297) / _S298;
        for(;;)
        {
            if(_S300)
            {
                float3  _S405 = make_float3 (0.0f, 0.0f, 1.0f);
                ok_0 = true;
                mid_0 = _S405;
                break;
            }
            if(camera_model_4 == int(3))
            {
                float _S406 = _S404.x;
                if((F32_abs((_S406))) > 3.14159274101257324f)
                {
                    ok_0 = true;
                }
                else
                {
                    ok_0 = (F32_abs((_S404.y))) > 1.57079637050628662f;
                }
                if(ok_0)
                {
                    int3  _S407 = make_int3 (int(0));
                    float3  _S408 = make_float3 ((float)_S407.x, (float)_S407.y, (float)_S407.z);
                    ok_0 = false;
                    mid_0 = _S408;
                    break;
                }
                float lat_10 = _S404.y;
                float cl_10 = (F32_cos((lat_10)));
                float3  _S409 = make_float3 (cl_10 * (F32_sin((_S406))), (F32_sin((lat_10))), cl_10 * (F32_cos((_S406))));
                ok_0 = true;
                mid_0 = _S409;
                break;
            }
            float2  uv_u_10;
            bool _S410 = undistort_point_0(_S404, dist_coeffs_14, int(8), &uv_u_10);
            if(!_S410)
            {
                int3  _S411 = make_int3 (int(0));
                float3  _S412 = make_float3 ((float)_S411.x, (float)_S411.y, (float)_S411.z);
                ok_0 = false;
                mid_0 = _S412;
                break;
            }
            float2  _S413 = uv_u_10;
            if(camera_model_4 == int(1))
            {
                float r_30 = length_0(_S413);
                if(r_30 >= 3.14159274101257324f)
                {
                    int3  _S414 = make_int3 (int(0));
                    float3  _S415 = make_float3 ((float)_S414.x, (float)_S414.y, (float)_S414.z);
                    ok_0 = false;
                    mid_0 = _S415;
                    break;
                }
                if(r_30 < 0.00100000004749745f)
                {
                    s_3 = 1.0f - r_30 * r_30 / 6.0f;
                }
                else
                {
                    s_3 = (F32_sin((r_30))) / r_30;
                }
                mid_0 = make_float3 ((_S413 * make_float2 (s_3)).x, (_S413 * make_float2 (s_3)).y, (F32_cos((r_30))));
            }
            else
            {
                if(camera_model_4 == int(2))
                {
                    float r_31 = length_0(_S413);
                    if(r_31 >= 2.0f)
                    {
                        int3  _S416 = make_int3 (int(0));
                        float3  _S417 = make_float3 ((float)_S416.x, (float)_S416.y, (float)_S416.z);
                        ok_0 = false;
                        mid_0 = _S417;
                        break;
                    }
                    mid_0 = make_float3 ((_S413 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_31 * r_31)))))))).x, (_S413 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_31 * r_31)))))))).y, 1.0f - 0.5f * r_31 * r_31);
                }
                else
                {
                    mid_0 = make_float3 (_S413.x, _S413.y, 1.0f);
                }
            }
            float3  _S418 = normalize_0(mid_0);
            ok_0 = true;
            mid_0 = _S418;
            break;
        }
    }
    else
    {
        ok_0 = false;
    }
    float3  along_3 = *_S299 - r_15[int(3)];
    float3  _S419 = cross_0(r_15[int(3)], along_3);
    n_0[int(3)] = _S419;
    float3  chord_3 = normalize_0(r_15[int(3)] + *_S299);
    if((dot_1(_S419, mid_0)) < 0.0f)
    {
        n_0[int(3)] = cross_0(mid_0 + (mid_0 - chord_3), along_3);
    }
    if(!ok_0)
    {
        return false;
    }
    FixedArray<float3 , 4>  q_4;
    float3  x_15 = cross_0(n_0[int(3)], n_0[int(0)]);
    float xx_0 = dot_1(x_15, x_15);
    if(xx_0 > 9.99999968265522539e-21f)
    {
        float _S420 = (F32_rsqrt((xx_0)));
        if((dot_1(x_15, *_S299)) < 0.0f)
        {
            s_3 = -1.0f;
        }
        else
        {
            s_3 = 1.0f;
        }
        mid_0 = x_15 * make_float3 (_S420 * s_3);
    }
    else
    {
        mid_0 = *_S299;
    }
    q_4[int(0)] = mid_0;
    float3  x_16 = cross_0(n_0[int(0)], n_0[int(1)]);
    float xx_1 = dot_1(x_16, x_16);
    if(xx_1 > 9.99999968265522539e-21f)
    {
        float _S421 = (F32_rsqrt((xx_1)));
        if((dot_1(x_16, r_15[int(1)])) < 0.0f)
        {
            s_3 = -1.0f;
        }
        else
        {
            s_3 = 1.0f;
        }
        mid_0 = x_16 * make_float3 (_S421 * s_3);
    }
    else
    {
        mid_0 = r_15[int(1)];
    }
    q_4[int(1)] = mid_0;
    float3  x_17 = cross_0(n_0[int(1)], n_0[int(2)]);
    float xx_2 = dot_1(x_17, x_17);
    if(xx_2 > 9.99999968265522539e-21f)
    {
        float _S422 = (F32_rsqrt((xx_2)));
        if((dot_1(x_17, r_15[int(2)])) < 0.0f)
        {
            s_3 = -1.0f;
        }
        else
        {
            s_3 = 1.0f;
        }
        mid_0 = x_17 * make_float3 (_S422 * s_3);
    }
    else
    {
        mid_0 = r_15[int(2)];
    }
    q_4[int(2)] = mid_0;
    float3  x_18 = cross_0(n_0[int(2)], n_0[int(3)]);
    float xx_3 = dot_1(x_18, x_18);
    if(xx_3 > 9.99999968265522539e-21f)
    {
        float _S423 = (F32_rsqrt((xx_3)));
        if((dot_1(x_18, r_15[int(3)])) < 0.0f)
        {
            s_3 = -1.0f;
        }
        else
        {
            s_3 = 1.0f;
        }
        mid_0 = x_18 * make_float3 (_S423 * s_3);
    }
    else
    {
        mid_0 = r_15[int(3)];
    }
    q_4[int(3)] = mid_0;
    *q0_0 = q_4[int(0)];
    *q1_0 = q_4[int(1)];
    *q2_0 = q_4[int(2)];
    *q3_0 = q_4[int(3)];
    return true;
}

inline __device__ bool pixel_rect_quad_1(int camera_model_5, float4  intrins_9, FixedArray<float, 4>  * dist_coeffs_15, float2  lo_1, float2  hi_1, float3  * q0_1, float3  * q1_1, float3  * q2_1, float3  * q3_1)
{
    bool ok_1;
    float s_4;
    float2  _S424;
    float2  _S425;
    float3  * _S426;
    bool _S427;
    float3  _S428 = make_float3 (0.0f);
    *q0_1 = _S428;
    *q1_1 = _S428;
    *q2_1 = *q0_1;
    *q3_1 = *q0_1;
    float2  _S429 = make_float2 (hi_1.x, lo_1.y);
    float2  _S430 = make_float2 (lo_1.x, hi_1.y);
    FixedArray<float3 , 4>  r_32;
    FixedArray<float3 , 4>  n_1;
    for(;;)
    {
        float2  _S431 = float2 {intrins_9.z, intrins_9.w};
        _S424 = _S431;
        float2  _S432 = lo_1 - _S431;
        float2  _S433 = float2 {intrins_9.x, intrins_9.y};
        _S425 = _S433;
        float2  _S434 = _S432 / _S433;
        _S426 = &r_32[int(0)];
        for(;;)
        {
            bool _S435 = camera_model_5 == int(4);
            _S427 = _S435;
            if(_S435)
            {
                r_32[int(0)] = make_float3 (0.0f, 0.0f, 1.0f);
                ok_1 = true;
                break;
            }
            if(camera_model_5 == int(3))
            {
                float _S436 = _S434.x;
                if((F32_abs((_S436))) > 3.14159274101257324f)
                {
                    ok_1 = true;
                }
                else
                {
                    ok_1 = (F32_abs((_S434.y))) > 1.57079637050628662f;
                }
                if(ok_1)
                {
                    int3  _S437 = make_int3 (int(0));
                    float3  _S438 = make_float3 ((float)_S437.x, (float)_S437.y, (float)_S437.z);
                    r_32[int(0)] = _S438;
                    ok_1 = false;
                    break;
                }
                float lat_11 = _S434.y;
                float cl_11 = (F32_cos((lat_11)));
                r_32[int(0)] = make_float3 (cl_11 * (F32_sin((_S436))), (F32_sin((lat_11))), cl_11 * (F32_cos((_S436))));
                ok_1 = true;
                break;
            }
            float2  uv_u_11;
            bool _S439 = undistort_point_1(_S434, dist_coeffs_15, int(8), &uv_u_11);
            if(!_S439)
            {
                int3  _S440 = make_int3 (int(0));
                float3  _S441 = make_float3 ((float)_S440.x, (float)_S440.y, (float)_S440.z);
                r_32[int(0)] = _S441;
                ok_1 = false;
                break;
            }
            float2  _S442 = uv_u_11;
            if(camera_model_5 == int(1))
            {
                float r_33 = length_0(_S442);
                if(r_33 >= 3.14159274101257324f)
                {
                    int3  _S443 = make_int3 (int(0));
                    float3  _S444 = make_float3 ((float)_S443.x, (float)_S443.y, (float)_S443.z);
                    r_32[int(0)] = _S444;
                    ok_1 = false;
                    break;
                }
                if(r_33 < 0.00100000004749745f)
                {
                    s_4 = 1.0f - r_33 * r_33 / 6.0f;
                }
                else
                {
                    s_4 = (F32_sin((r_33))) / r_33;
                }
                r_32[int(0)] = make_float3 ((_S442 * make_float2 (s_4)).x, (_S442 * make_float2 (s_4)).y, (F32_cos((r_33))));
            }
            else
            {
                if(camera_model_5 == int(2))
                {
                    float r_34 = length_0(_S442);
                    if(r_34 >= 2.0f)
                    {
                        int3  _S445 = make_int3 (int(0));
                        float3  _S446 = make_float3 ((float)_S445.x, (float)_S445.y, (float)_S445.z);
                        r_32[int(0)] = _S446;
                        ok_1 = false;
                        break;
                    }
                    r_32[int(0)] = make_float3 ((_S442 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_34 * r_34)))))))).x, (_S442 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_34 * r_34)))))))).y, 1.0f - 0.5f * r_34 * r_34);
                }
                else
                {
                    r_32[int(0)] = make_float3 (_S442.x, _S442.y, 1.0f);
                }
            }
            r_32[int(0)] = normalize_0(r_32[int(0)]);
            ok_1 = true;
            break;
        }
        if(ok_1)
        {
            float2  _S447 = (_S429 - _S431) / _S433;
            for(;;)
            {
                if(_S427)
                {
                    r_32[int(1)] = make_float3 (0.0f, 0.0f, 1.0f);
                    ok_1 = true;
                    break;
                }
                if(camera_model_5 == int(3))
                {
                    float _S448 = _S447.x;
                    if((F32_abs((_S448))) > 3.14159274101257324f)
                    {
                        ok_1 = true;
                    }
                    else
                    {
                        ok_1 = (F32_abs((_S447.y))) > 1.57079637050628662f;
                    }
                    if(ok_1)
                    {
                        int3  _S449 = make_int3 (int(0));
                        float3  _S450 = make_float3 ((float)_S449.x, (float)_S449.y, (float)_S449.z);
                        r_32[int(1)] = _S450;
                        ok_1 = false;
                        break;
                    }
                    float lat_12 = _S447.y;
                    float cl_12 = (F32_cos((lat_12)));
                    r_32[int(1)] = make_float3 (cl_12 * (F32_sin((_S448))), (F32_sin((lat_12))), cl_12 * (F32_cos((_S448))));
                    ok_1 = true;
                    break;
                }
                float2  uv_u_12;
                bool _S451 = undistort_point_1(_S447, dist_coeffs_15, int(8), &uv_u_12);
                if(!_S451)
                {
                    int3  _S452 = make_int3 (int(0));
                    float3  _S453 = make_float3 ((float)_S452.x, (float)_S452.y, (float)_S452.z);
                    r_32[int(1)] = _S453;
                    ok_1 = false;
                    break;
                }
                float2  _S454 = uv_u_12;
                if(camera_model_5 == int(1))
                {
                    float r_35 = length_0(_S454);
                    if(r_35 >= 3.14159274101257324f)
                    {
                        int3  _S455 = make_int3 (int(0));
                        float3  _S456 = make_float3 ((float)_S455.x, (float)_S455.y, (float)_S455.z);
                        r_32[int(1)] = _S456;
                        ok_1 = false;
                        break;
                    }
                    if(r_35 < 0.00100000004749745f)
                    {
                        s_4 = 1.0f - r_35 * r_35 / 6.0f;
                    }
                    else
                    {
                        s_4 = (F32_sin((r_35))) / r_35;
                    }
                    r_32[int(1)] = make_float3 ((_S454 * make_float2 (s_4)).x, (_S454 * make_float2 (s_4)).y, (F32_cos((r_35))));
                }
                else
                {
                    if(camera_model_5 == int(2))
                    {
                        float r_36 = length_0(_S454);
                        if(r_36 >= 2.0f)
                        {
                            int3  _S457 = make_int3 (int(0));
                            float3  _S458 = make_float3 ((float)_S457.x, (float)_S457.y, (float)_S457.z);
                            r_32[int(1)] = _S458;
                            ok_1 = false;
                            break;
                        }
                        r_32[int(1)] = make_float3 ((_S454 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_36 * r_36)))))))).x, (_S454 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_36 * r_36)))))))).y, 1.0f - 0.5f * r_36 * r_36);
                    }
                    else
                    {
                        r_32[int(1)] = make_float3 (_S454.x, _S454.y, 1.0f);
                    }
                }
                r_32[int(1)] = normalize_0(r_32[int(1)]);
                ok_1 = true;
                break;
            }
        }
        else
        {
            ok_1 = false;
        }
        if(ok_1)
        {
            float2  _S459 = (hi_1 - _S431) / _S433;
            for(;;)
            {
                if(_S427)
                {
                    r_32[int(2)] = make_float3 (0.0f, 0.0f, 1.0f);
                    ok_1 = true;
                    break;
                }
                if(camera_model_5 == int(3))
                {
                    float _S460 = _S459.x;
                    if((F32_abs((_S460))) > 3.14159274101257324f)
                    {
                        ok_1 = true;
                    }
                    else
                    {
                        ok_1 = (F32_abs((_S459.y))) > 1.57079637050628662f;
                    }
                    if(ok_1)
                    {
                        int3  _S461 = make_int3 (int(0));
                        float3  _S462 = make_float3 ((float)_S461.x, (float)_S461.y, (float)_S461.z);
                        r_32[int(2)] = _S462;
                        ok_1 = false;
                        break;
                    }
                    float lat_13 = _S459.y;
                    float cl_13 = (F32_cos((lat_13)));
                    r_32[int(2)] = make_float3 (cl_13 * (F32_sin((_S460))), (F32_sin((lat_13))), cl_13 * (F32_cos((_S460))));
                    ok_1 = true;
                    break;
                }
                float2  uv_u_13;
                bool _S463 = undistort_point_1(_S459, dist_coeffs_15, int(8), &uv_u_13);
                if(!_S463)
                {
                    int3  _S464 = make_int3 (int(0));
                    float3  _S465 = make_float3 ((float)_S464.x, (float)_S464.y, (float)_S464.z);
                    r_32[int(2)] = _S465;
                    ok_1 = false;
                    break;
                }
                float2  _S466 = uv_u_13;
                if(camera_model_5 == int(1))
                {
                    float r_37 = length_0(_S466);
                    if(r_37 >= 3.14159274101257324f)
                    {
                        int3  _S467 = make_int3 (int(0));
                        float3  _S468 = make_float3 ((float)_S467.x, (float)_S467.y, (float)_S467.z);
                        r_32[int(2)] = _S468;
                        ok_1 = false;
                        break;
                    }
                    if(r_37 < 0.00100000004749745f)
                    {
                        s_4 = 1.0f - r_37 * r_37 / 6.0f;
                    }
                    else
                    {
                        s_4 = (F32_sin((r_37))) / r_37;
                    }
                    r_32[int(2)] = make_float3 ((_S466 * make_float2 (s_4)).x, (_S466 * make_float2 (s_4)).y, (F32_cos((r_37))));
                }
                else
                {
                    if(camera_model_5 == int(2))
                    {
                        float r_38 = length_0(_S466);
                        if(r_38 >= 2.0f)
                        {
                            int3  _S469 = make_int3 (int(0));
                            float3  _S470 = make_float3 ((float)_S469.x, (float)_S469.y, (float)_S469.z);
                            r_32[int(2)] = _S470;
                            ok_1 = false;
                            break;
                        }
                        r_32[int(2)] = make_float3 ((_S466 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_38 * r_38)))))))).x, (_S466 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_38 * r_38)))))))).y, 1.0f - 0.5f * r_38 * r_38);
                    }
                    else
                    {
                        r_32[int(2)] = make_float3 (_S466.x, _S466.y, 1.0f);
                    }
                }
                r_32[int(2)] = normalize_0(r_32[int(2)]);
                ok_1 = true;
                break;
            }
        }
        else
        {
            ok_1 = false;
        }
        if(ok_1)
        {
            float2  _S471 = (_S430 - _S431) / _S433;
            for(;;)
            {
                if(_S427)
                {
                    r_32[int(3)] = make_float3 (0.0f, 0.0f, 1.0f);
                    ok_1 = true;
                    break;
                }
                if(camera_model_5 == int(3))
                {
                    float _S472 = _S471.x;
                    if((F32_abs((_S472))) > 3.14159274101257324f)
                    {
                        ok_1 = true;
                    }
                    else
                    {
                        ok_1 = (F32_abs((_S471.y))) > 1.57079637050628662f;
                    }
                    if(ok_1)
                    {
                        int3  _S473 = make_int3 (int(0));
                        float3  _S474 = make_float3 ((float)_S473.x, (float)_S473.y, (float)_S473.z);
                        r_32[int(3)] = _S474;
                        ok_1 = false;
                        break;
                    }
                    float lat_14 = _S471.y;
                    float cl_14 = (F32_cos((lat_14)));
                    r_32[int(3)] = make_float3 (cl_14 * (F32_sin((_S472))), (F32_sin((lat_14))), cl_14 * (F32_cos((_S472))));
                    ok_1 = true;
                    break;
                }
                float2  uv_u_14;
                bool _S475 = undistort_point_1(_S471, dist_coeffs_15, int(8), &uv_u_14);
                if(!_S475)
                {
                    int3  _S476 = make_int3 (int(0));
                    float3  _S477 = make_float3 ((float)_S476.x, (float)_S476.y, (float)_S476.z);
                    r_32[int(3)] = _S477;
                    ok_1 = false;
                    break;
                }
                float2  _S478 = uv_u_14;
                if(camera_model_5 == int(1))
                {
                    float r_39 = length_0(_S478);
                    if(r_39 >= 3.14159274101257324f)
                    {
                        int3  _S479 = make_int3 (int(0));
                        float3  _S480 = make_float3 ((float)_S479.x, (float)_S479.y, (float)_S479.z);
                        r_32[int(3)] = _S480;
                        ok_1 = false;
                        break;
                    }
                    if(r_39 < 0.00100000004749745f)
                    {
                        s_4 = 1.0f - r_39 * r_39 / 6.0f;
                    }
                    else
                    {
                        s_4 = (F32_sin((r_39))) / r_39;
                    }
                    r_32[int(3)] = make_float3 ((_S478 * make_float2 (s_4)).x, (_S478 * make_float2 (s_4)).y, (F32_cos((r_39))));
                }
                else
                {
                    if(camera_model_5 == int(2))
                    {
                        float r_40 = length_0(_S478);
                        if(r_40 >= 2.0f)
                        {
                            int3  _S481 = make_int3 (int(0));
                            float3  _S482 = make_float3 ((float)_S481.x, (float)_S481.y, (float)_S481.z);
                            r_32[int(3)] = _S482;
                            ok_1 = false;
                            break;
                        }
                        r_32[int(3)] = make_float3 ((_S478 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_40 * r_40)))))))).x, (_S478 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_40 * r_40)))))))).y, 1.0f - 0.5f * r_40 * r_40);
                    }
                    else
                    {
                        r_32[int(3)] = make_float3 (_S478.x, _S478.y, 1.0f);
                    }
                }
                r_32[int(3)] = normalize_0(r_32[int(3)]);
                ok_1 = true;
                break;
            }
        }
        else
        {
            ok_1 = false;
        }
        break;
    }
    if(ok_1)
    {
        ok_1 = (dot_1(*_S426, r_32[int(2)])) > 0.5f;
    }
    else
    {
        ok_1 = false;
    }
    if(ok_1)
    {
        ok_1 = (dot_1(r_32[int(1)], r_32[int(3)])) > 0.5f;
    }
    else
    {
        ok_1 = false;
    }
    float3  mid_1;
    if(ok_1)
    {
        float2  _S483 = (make_float2 (0.5f) * (lo_1 + _S429) - _S424) / _S425;
        for(;;)
        {
            if(_S427)
            {
                float3  _S484 = make_float3 (0.0f, 0.0f, 1.0f);
                ok_1 = true;
                mid_1 = _S484;
                break;
            }
            if(camera_model_5 == int(3))
            {
                float _S485 = _S483.x;
                if((F32_abs((_S485))) > 3.14159274101257324f)
                {
                    ok_1 = true;
                }
                else
                {
                    ok_1 = (F32_abs((_S483.y))) > 1.57079637050628662f;
                }
                if(ok_1)
                {
                    int3  _S486 = make_int3 (int(0));
                    float3  _S487 = make_float3 ((float)_S486.x, (float)_S486.y, (float)_S486.z);
                    ok_1 = false;
                    mid_1 = _S487;
                    break;
                }
                float lat_15 = _S483.y;
                float cl_15 = (F32_cos((lat_15)));
                float3  _S488 = make_float3 (cl_15 * (F32_sin((_S485))), (F32_sin((lat_15))), cl_15 * (F32_cos((_S485))));
                ok_1 = true;
                mid_1 = _S488;
                break;
            }
            float2  uv_u_15;
            bool _S489 = undistort_point_1(_S483, dist_coeffs_15, int(8), &uv_u_15);
            if(!_S489)
            {
                int3  _S490 = make_int3 (int(0));
                float3  _S491 = make_float3 ((float)_S490.x, (float)_S490.y, (float)_S490.z);
                ok_1 = false;
                mid_1 = _S491;
                break;
            }
            float2  _S492 = uv_u_15;
            if(camera_model_5 == int(1))
            {
                float r_41 = length_0(_S492);
                if(r_41 >= 3.14159274101257324f)
                {
                    int3  _S493 = make_int3 (int(0));
                    float3  _S494 = make_float3 ((float)_S493.x, (float)_S493.y, (float)_S493.z);
                    ok_1 = false;
                    mid_1 = _S494;
                    break;
                }
                if(r_41 < 0.00100000004749745f)
                {
                    s_4 = 1.0f - r_41 * r_41 / 6.0f;
                }
                else
                {
                    s_4 = (F32_sin((r_41))) / r_41;
                }
                mid_1 = make_float3 ((_S492 * make_float2 (s_4)).x, (_S492 * make_float2 (s_4)).y, (F32_cos((r_41))));
            }
            else
            {
                if(camera_model_5 == int(2))
                {
                    float r_42 = length_0(_S492);
                    if(r_42 >= 2.0f)
                    {
                        int3  _S495 = make_int3 (int(0));
                        float3  _S496 = make_float3 ((float)_S495.x, (float)_S495.y, (float)_S495.z);
                        ok_1 = false;
                        mid_1 = _S496;
                        break;
                    }
                    mid_1 = make_float3 ((_S492 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_42 * r_42)))))))).x, (_S492 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_42 * r_42)))))))).y, 1.0f - 0.5f * r_42 * r_42);
                }
                else
                {
                    mid_1 = make_float3 (_S492.x, _S492.y, 1.0f);
                }
            }
            float3  _S497 = normalize_0(mid_1);
            ok_1 = true;
            mid_1 = _S497;
            break;
        }
    }
    else
    {
        ok_1 = false;
    }
    float3  along_4 = r_32[int(1)] - *_S426;
    float3  _S498 = cross_0(*_S426, along_4);
    n_1[int(0)] = _S498;
    float3  chord_4 = normalize_0(*_S426 + r_32[int(1)]);
    if((dot_1(_S498, mid_1)) < 0.0f)
    {
        n_1[int(0)] = cross_0(mid_1 + (mid_1 - chord_4), along_4);
    }
    if(ok_1)
    {
        float2  _S499 = (make_float2 (0.5f) * (_S429 + hi_1) - _S424) / _S425;
        for(;;)
        {
            if(_S427)
            {
                float3  _S500 = make_float3 (0.0f, 0.0f, 1.0f);
                ok_1 = true;
                mid_1 = _S500;
                break;
            }
            if(camera_model_5 == int(3))
            {
                float _S501 = _S499.x;
                if((F32_abs((_S501))) > 3.14159274101257324f)
                {
                    ok_1 = true;
                }
                else
                {
                    ok_1 = (F32_abs((_S499.y))) > 1.57079637050628662f;
                }
                if(ok_1)
                {
                    int3  _S502 = make_int3 (int(0));
                    float3  _S503 = make_float3 ((float)_S502.x, (float)_S502.y, (float)_S502.z);
                    ok_1 = false;
                    mid_1 = _S503;
                    break;
                }
                float lat_16 = _S499.y;
                float cl_16 = (F32_cos((lat_16)));
                float3  _S504 = make_float3 (cl_16 * (F32_sin((_S501))), (F32_sin((lat_16))), cl_16 * (F32_cos((_S501))));
                ok_1 = true;
                mid_1 = _S504;
                break;
            }
            float2  uv_u_16;
            bool _S505 = undistort_point_1(_S499, dist_coeffs_15, int(8), &uv_u_16);
            if(!_S505)
            {
                int3  _S506 = make_int3 (int(0));
                float3  _S507 = make_float3 ((float)_S506.x, (float)_S506.y, (float)_S506.z);
                ok_1 = false;
                mid_1 = _S507;
                break;
            }
            float2  _S508 = uv_u_16;
            if(camera_model_5 == int(1))
            {
                float r_43 = length_0(_S508);
                if(r_43 >= 3.14159274101257324f)
                {
                    int3  _S509 = make_int3 (int(0));
                    float3  _S510 = make_float3 ((float)_S509.x, (float)_S509.y, (float)_S509.z);
                    ok_1 = false;
                    mid_1 = _S510;
                    break;
                }
                if(r_43 < 0.00100000004749745f)
                {
                    s_4 = 1.0f - r_43 * r_43 / 6.0f;
                }
                else
                {
                    s_4 = (F32_sin((r_43))) / r_43;
                }
                mid_1 = make_float3 ((_S508 * make_float2 (s_4)).x, (_S508 * make_float2 (s_4)).y, (F32_cos((r_43))));
            }
            else
            {
                if(camera_model_5 == int(2))
                {
                    float r_44 = length_0(_S508);
                    if(r_44 >= 2.0f)
                    {
                        int3  _S511 = make_int3 (int(0));
                        float3  _S512 = make_float3 ((float)_S511.x, (float)_S511.y, (float)_S511.z);
                        ok_1 = false;
                        mid_1 = _S512;
                        break;
                    }
                    mid_1 = make_float3 ((_S508 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_44 * r_44)))))))).x, (_S508 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_44 * r_44)))))))).y, 1.0f - 0.5f * r_44 * r_44);
                }
                else
                {
                    mid_1 = make_float3 (_S508.x, _S508.y, 1.0f);
                }
            }
            float3  _S513 = normalize_0(mid_1);
            ok_1 = true;
            mid_1 = _S513;
            break;
        }
    }
    else
    {
        ok_1 = false;
    }
    float3  along_5 = r_32[int(2)] - r_32[int(1)];
    float3  _S514 = cross_0(r_32[int(1)], along_5);
    n_1[int(1)] = _S514;
    float3  chord_5 = normalize_0(r_32[int(1)] + r_32[int(2)]);
    if((dot_1(_S514, mid_1)) < 0.0f)
    {
        n_1[int(1)] = cross_0(mid_1 + (mid_1 - chord_5), along_5);
    }
    if(ok_1)
    {
        float2  _S515 = (make_float2 (0.5f) * (hi_1 + _S430) - _S424) / _S425;
        for(;;)
        {
            if(_S427)
            {
                float3  _S516 = make_float3 (0.0f, 0.0f, 1.0f);
                ok_1 = true;
                mid_1 = _S516;
                break;
            }
            if(camera_model_5 == int(3))
            {
                float _S517 = _S515.x;
                if((F32_abs((_S517))) > 3.14159274101257324f)
                {
                    ok_1 = true;
                }
                else
                {
                    ok_1 = (F32_abs((_S515.y))) > 1.57079637050628662f;
                }
                if(ok_1)
                {
                    int3  _S518 = make_int3 (int(0));
                    float3  _S519 = make_float3 ((float)_S518.x, (float)_S518.y, (float)_S518.z);
                    ok_1 = false;
                    mid_1 = _S519;
                    break;
                }
                float lat_17 = _S515.y;
                float cl_17 = (F32_cos((lat_17)));
                float3  _S520 = make_float3 (cl_17 * (F32_sin((_S517))), (F32_sin((lat_17))), cl_17 * (F32_cos((_S517))));
                ok_1 = true;
                mid_1 = _S520;
                break;
            }
            float2  uv_u_17;
            bool _S521 = undistort_point_1(_S515, dist_coeffs_15, int(8), &uv_u_17);
            if(!_S521)
            {
                int3  _S522 = make_int3 (int(0));
                float3  _S523 = make_float3 ((float)_S522.x, (float)_S522.y, (float)_S522.z);
                ok_1 = false;
                mid_1 = _S523;
                break;
            }
            float2  _S524 = uv_u_17;
            if(camera_model_5 == int(1))
            {
                float r_45 = length_0(_S524);
                if(r_45 >= 3.14159274101257324f)
                {
                    int3  _S525 = make_int3 (int(0));
                    float3  _S526 = make_float3 ((float)_S525.x, (float)_S525.y, (float)_S525.z);
                    ok_1 = false;
                    mid_1 = _S526;
                    break;
                }
                if(r_45 < 0.00100000004749745f)
                {
                    s_4 = 1.0f - r_45 * r_45 / 6.0f;
                }
                else
                {
                    s_4 = (F32_sin((r_45))) / r_45;
                }
                mid_1 = make_float3 ((_S524 * make_float2 (s_4)).x, (_S524 * make_float2 (s_4)).y, (F32_cos((r_45))));
            }
            else
            {
                if(camera_model_5 == int(2))
                {
                    float r_46 = length_0(_S524);
                    if(r_46 >= 2.0f)
                    {
                        int3  _S527 = make_int3 (int(0));
                        float3  _S528 = make_float3 ((float)_S527.x, (float)_S527.y, (float)_S527.z);
                        ok_1 = false;
                        mid_1 = _S528;
                        break;
                    }
                    mid_1 = make_float3 ((_S524 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_46 * r_46)))))))).x, (_S524 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_46 * r_46)))))))).y, 1.0f - 0.5f * r_46 * r_46);
                }
                else
                {
                    mid_1 = make_float3 (_S524.x, _S524.y, 1.0f);
                }
            }
            float3  _S529 = normalize_0(mid_1);
            ok_1 = true;
            mid_1 = _S529;
            break;
        }
    }
    else
    {
        ok_1 = false;
    }
    float3  along_6 = r_32[int(3)] - r_32[int(2)];
    float3  _S530 = cross_0(r_32[int(2)], along_6);
    n_1[int(2)] = _S530;
    float3  chord_6 = normalize_0(r_32[int(2)] + r_32[int(3)]);
    if((dot_1(_S530, mid_1)) < 0.0f)
    {
        n_1[int(2)] = cross_0(mid_1 + (mid_1 - chord_6), along_6);
    }
    if(ok_1)
    {
        float2  _S531 = (make_float2 (0.5f) * (_S430 + lo_1) - _S424) / _S425;
        for(;;)
        {
            if(_S427)
            {
                float3  _S532 = make_float3 (0.0f, 0.0f, 1.0f);
                ok_1 = true;
                mid_1 = _S532;
                break;
            }
            if(camera_model_5 == int(3))
            {
                float _S533 = _S531.x;
                if((F32_abs((_S533))) > 3.14159274101257324f)
                {
                    ok_1 = true;
                }
                else
                {
                    ok_1 = (F32_abs((_S531.y))) > 1.57079637050628662f;
                }
                if(ok_1)
                {
                    int3  _S534 = make_int3 (int(0));
                    float3  _S535 = make_float3 ((float)_S534.x, (float)_S534.y, (float)_S534.z);
                    ok_1 = false;
                    mid_1 = _S535;
                    break;
                }
                float lat_18 = _S531.y;
                float cl_18 = (F32_cos((lat_18)));
                float3  _S536 = make_float3 (cl_18 * (F32_sin((_S533))), (F32_sin((lat_18))), cl_18 * (F32_cos((_S533))));
                ok_1 = true;
                mid_1 = _S536;
                break;
            }
            float2  uv_u_18;
            bool _S537 = undistort_point_1(_S531, dist_coeffs_15, int(8), &uv_u_18);
            if(!_S537)
            {
                int3  _S538 = make_int3 (int(0));
                float3  _S539 = make_float3 ((float)_S538.x, (float)_S538.y, (float)_S538.z);
                ok_1 = false;
                mid_1 = _S539;
                break;
            }
            float2  _S540 = uv_u_18;
            if(camera_model_5 == int(1))
            {
                float r_47 = length_0(_S540);
                if(r_47 >= 3.14159274101257324f)
                {
                    int3  _S541 = make_int3 (int(0));
                    float3  _S542 = make_float3 ((float)_S541.x, (float)_S541.y, (float)_S541.z);
                    ok_1 = false;
                    mid_1 = _S542;
                    break;
                }
                if(r_47 < 0.00100000004749745f)
                {
                    s_4 = 1.0f - r_47 * r_47 / 6.0f;
                }
                else
                {
                    s_4 = (F32_sin((r_47))) / r_47;
                }
                mid_1 = make_float3 ((_S540 * make_float2 (s_4)).x, (_S540 * make_float2 (s_4)).y, (F32_cos((r_47))));
            }
            else
            {
                if(camera_model_5 == int(2))
                {
                    float r_48 = length_0(_S540);
                    if(r_48 >= 2.0f)
                    {
                        int3  _S543 = make_int3 (int(0));
                        float3  _S544 = make_float3 ((float)_S543.x, (float)_S543.y, (float)_S543.z);
                        ok_1 = false;
                        mid_1 = _S544;
                        break;
                    }
                    mid_1 = make_float3 ((_S540 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_48 * r_48)))))))).x, (_S540 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_48 * r_48)))))))).y, 1.0f - 0.5f * r_48 * r_48);
                }
                else
                {
                    mid_1 = make_float3 (_S540.x, _S540.y, 1.0f);
                }
            }
            float3  _S545 = normalize_0(mid_1);
            ok_1 = true;
            mid_1 = _S545;
            break;
        }
    }
    else
    {
        ok_1 = false;
    }
    float3  along_7 = *_S426 - r_32[int(3)];
    float3  _S546 = cross_0(r_32[int(3)], along_7);
    n_1[int(3)] = _S546;
    float3  chord_7 = normalize_0(r_32[int(3)] + *_S426);
    if((dot_1(_S546, mid_1)) < 0.0f)
    {
        n_1[int(3)] = cross_0(mid_1 + (mid_1 - chord_7), along_7);
    }
    if(!ok_1)
    {
        return false;
    }
    FixedArray<float3 , 4>  q_5;
    float3  x_19 = cross_0(n_1[int(3)], n_1[int(0)]);
    float xx_4 = dot_1(x_19, x_19);
    if(xx_4 > 9.99999968265522539e-21f)
    {
        float _S547 = (F32_rsqrt((xx_4)));
        if((dot_1(x_19, *_S426)) < 0.0f)
        {
            s_4 = -1.0f;
        }
        else
        {
            s_4 = 1.0f;
        }
        mid_1 = x_19 * make_float3 (_S547 * s_4);
    }
    else
    {
        mid_1 = *_S426;
    }
    q_5[int(0)] = mid_1;
    float3  x_20 = cross_0(n_1[int(0)], n_1[int(1)]);
    float xx_5 = dot_1(x_20, x_20);
    if(xx_5 > 9.99999968265522539e-21f)
    {
        float _S548 = (F32_rsqrt((xx_5)));
        if((dot_1(x_20, r_32[int(1)])) < 0.0f)
        {
            s_4 = -1.0f;
        }
        else
        {
            s_4 = 1.0f;
        }
        mid_1 = x_20 * make_float3 (_S548 * s_4);
    }
    else
    {
        mid_1 = r_32[int(1)];
    }
    q_5[int(1)] = mid_1;
    float3  x_21 = cross_0(n_1[int(1)], n_1[int(2)]);
    float xx_6 = dot_1(x_21, x_21);
    if(xx_6 > 9.99999968265522539e-21f)
    {
        float _S549 = (F32_rsqrt((xx_6)));
        if((dot_1(x_21, r_32[int(2)])) < 0.0f)
        {
            s_4 = -1.0f;
        }
        else
        {
            s_4 = 1.0f;
        }
        mid_1 = x_21 * make_float3 (_S549 * s_4);
    }
    else
    {
        mid_1 = r_32[int(2)];
    }
    q_5[int(2)] = mid_1;
    float3  x_22 = cross_0(n_1[int(2)], n_1[int(3)]);
    float xx_7 = dot_1(x_22, x_22);
    if(xx_7 > 9.99999968265522539e-21f)
    {
        float _S550 = (F32_rsqrt((xx_7)));
        if((dot_1(x_22, r_32[int(3)])) < 0.0f)
        {
            s_4 = -1.0f;
        }
        else
        {
            s_4 = 1.0f;
        }
        mid_1 = x_22 * make_float3 (_S550 * s_4);
    }
    else
    {
        mid_1 = r_32[int(3)];
    }
    q_5[int(3)] = mid_1;
    *q0_1 = q_5[int(0)];
    *q1_1 = q_5[int(1)];
    *q2_1 = q_5[int(2)];
    *q3_1 = q_5[int(3)];
    return true;
}

inline __device__ bool pixel_rect_quad_2(int camera_model_6, float4  intrins_10, FixedArray<float, 8>  * dist_coeffs_16, float2  lo_2, float2  hi_2, float3  * q0_2, float3  * q1_2, float3  * q2_2, float3  * q3_2)
{
    bool ok_2;
    float s_5;
    float2  _S551;
    float2  _S552;
    float3  * _S553;
    bool _S554;
    float3  _S555 = make_float3 (0.0f);
    *q0_2 = _S555;
    *q1_2 = _S555;
    *q2_2 = *q0_2;
    *q3_2 = *q0_2;
    float2  _S556 = make_float2 (hi_2.x, lo_2.y);
    float2  _S557 = make_float2 (lo_2.x, hi_2.y);
    FixedArray<float3 , 4>  r_49;
    FixedArray<float3 , 4>  n_2;
    for(;;)
    {
        float2  _S558 = float2 {intrins_10.z, intrins_10.w};
        _S551 = _S558;
        float2  _S559 = lo_2 - _S558;
        float2  _S560 = float2 {intrins_10.x, intrins_10.y};
        _S552 = _S560;
        float2  _S561 = _S559 / _S560;
        _S553 = &r_49[int(0)];
        for(;;)
        {
            bool _S562 = camera_model_6 == int(4);
            _S554 = _S562;
            if(_S562)
            {
                r_49[int(0)] = make_float3 (0.0f, 0.0f, 1.0f);
                ok_2 = true;
                break;
            }
            if(camera_model_6 == int(3))
            {
                float _S563 = _S561.x;
                if((F32_abs((_S563))) > 3.14159274101257324f)
                {
                    ok_2 = true;
                }
                else
                {
                    ok_2 = (F32_abs((_S561.y))) > 1.57079637050628662f;
                }
                if(ok_2)
                {
                    int3  _S564 = make_int3 (int(0));
                    float3  _S565 = make_float3 ((float)_S564.x, (float)_S564.y, (float)_S564.z);
                    r_49[int(0)] = _S565;
                    ok_2 = false;
                    break;
                }
                float lat_19 = _S561.y;
                float cl_19 = (F32_cos((lat_19)));
                r_49[int(0)] = make_float3 (cl_19 * (F32_sin((_S563))), (F32_sin((lat_19))), cl_19 * (F32_cos((_S563))));
                ok_2 = true;
                break;
            }
            float2  uv_u_19;
            bool _S566 = undistort_point_2(_S561, dist_coeffs_16, int(8), &uv_u_19);
            if(!_S566)
            {
                int3  _S567 = make_int3 (int(0));
                float3  _S568 = make_float3 ((float)_S567.x, (float)_S567.y, (float)_S567.z);
                r_49[int(0)] = _S568;
                ok_2 = false;
                break;
            }
            float2  _S569 = uv_u_19;
            if(camera_model_6 == int(1))
            {
                float r_50 = length_0(_S569);
                if(r_50 >= 3.14159274101257324f)
                {
                    int3  _S570 = make_int3 (int(0));
                    float3  _S571 = make_float3 ((float)_S570.x, (float)_S570.y, (float)_S570.z);
                    r_49[int(0)] = _S571;
                    ok_2 = false;
                    break;
                }
                if(r_50 < 0.00100000004749745f)
                {
                    s_5 = 1.0f - r_50 * r_50 / 6.0f;
                }
                else
                {
                    s_5 = (F32_sin((r_50))) / r_50;
                }
                r_49[int(0)] = make_float3 ((_S569 * make_float2 (s_5)).x, (_S569 * make_float2 (s_5)).y, (F32_cos((r_50))));
            }
            else
            {
                if(camera_model_6 == int(2))
                {
                    float r_51 = length_0(_S569);
                    if(r_51 >= 2.0f)
                    {
                        int3  _S572 = make_int3 (int(0));
                        float3  _S573 = make_float3 ((float)_S572.x, (float)_S572.y, (float)_S572.z);
                        r_49[int(0)] = _S573;
                        ok_2 = false;
                        break;
                    }
                    r_49[int(0)] = make_float3 ((_S569 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_51 * r_51)))))))).x, (_S569 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_51 * r_51)))))))).y, 1.0f - 0.5f * r_51 * r_51);
                }
                else
                {
                    r_49[int(0)] = make_float3 (_S569.x, _S569.y, 1.0f);
                }
            }
            r_49[int(0)] = normalize_0(r_49[int(0)]);
            ok_2 = true;
            break;
        }
        if(ok_2)
        {
            float2  _S574 = (_S556 - _S558) / _S560;
            for(;;)
            {
                if(_S554)
                {
                    r_49[int(1)] = make_float3 (0.0f, 0.0f, 1.0f);
                    ok_2 = true;
                    break;
                }
                if(camera_model_6 == int(3))
                {
                    float _S575 = _S574.x;
                    if((F32_abs((_S575))) > 3.14159274101257324f)
                    {
                        ok_2 = true;
                    }
                    else
                    {
                        ok_2 = (F32_abs((_S574.y))) > 1.57079637050628662f;
                    }
                    if(ok_2)
                    {
                        int3  _S576 = make_int3 (int(0));
                        float3  _S577 = make_float3 ((float)_S576.x, (float)_S576.y, (float)_S576.z);
                        r_49[int(1)] = _S577;
                        ok_2 = false;
                        break;
                    }
                    float lat_20 = _S574.y;
                    float cl_20 = (F32_cos((lat_20)));
                    r_49[int(1)] = make_float3 (cl_20 * (F32_sin((_S575))), (F32_sin((lat_20))), cl_20 * (F32_cos((_S575))));
                    ok_2 = true;
                    break;
                }
                float2  uv_u_20;
                bool _S578 = undistort_point_2(_S574, dist_coeffs_16, int(8), &uv_u_20);
                if(!_S578)
                {
                    int3  _S579 = make_int3 (int(0));
                    float3  _S580 = make_float3 ((float)_S579.x, (float)_S579.y, (float)_S579.z);
                    r_49[int(1)] = _S580;
                    ok_2 = false;
                    break;
                }
                float2  _S581 = uv_u_20;
                if(camera_model_6 == int(1))
                {
                    float r_52 = length_0(_S581);
                    if(r_52 >= 3.14159274101257324f)
                    {
                        int3  _S582 = make_int3 (int(0));
                        float3  _S583 = make_float3 ((float)_S582.x, (float)_S582.y, (float)_S582.z);
                        r_49[int(1)] = _S583;
                        ok_2 = false;
                        break;
                    }
                    if(r_52 < 0.00100000004749745f)
                    {
                        s_5 = 1.0f - r_52 * r_52 / 6.0f;
                    }
                    else
                    {
                        s_5 = (F32_sin((r_52))) / r_52;
                    }
                    r_49[int(1)] = make_float3 ((_S581 * make_float2 (s_5)).x, (_S581 * make_float2 (s_5)).y, (F32_cos((r_52))));
                }
                else
                {
                    if(camera_model_6 == int(2))
                    {
                        float r_53 = length_0(_S581);
                        if(r_53 >= 2.0f)
                        {
                            int3  _S584 = make_int3 (int(0));
                            float3  _S585 = make_float3 ((float)_S584.x, (float)_S584.y, (float)_S584.z);
                            r_49[int(1)] = _S585;
                            ok_2 = false;
                            break;
                        }
                        r_49[int(1)] = make_float3 ((_S581 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_53 * r_53)))))))).x, (_S581 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_53 * r_53)))))))).y, 1.0f - 0.5f * r_53 * r_53);
                    }
                    else
                    {
                        r_49[int(1)] = make_float3 (_S581.x, _S581.y, 1.0f);
                    }
                }
                r_49[int(1)] = normalize_0(r_49[int(1)]);
                ok_2 = true;
                break;
            }
        }
        else
        {
            ok_2 = false;
        }
        if(ok_2)
        {
            float2  _S586 = (hi_2 - _S558) / _S560;
            for(;;)
            {
                if(_S554)
                {
                    r_49[int(2)] = make_float3 (0.0f, 0.0f, 1.0f);
                    ok_2 = true;
                    break;
                }
                if(camera_model_6 == int(3))
                {
                    float _S587 = _S586.x;
                    if((F32_abs((_S587))) > 3.14159274101257324f)
                    {
                        ok_2 = true;
                    }
                    else
                    {
                        ok_2 = (F32_abs((_S586.y))) > 1.57079637050628662f;
                    }
                    if(ok_2)
                    {
                        int3  _S588 = make_int3 (int(0));
                        float3  _S589 = make_float3 ((float)_S588.x, (float)_S588.y, (float)_S588.z);
                        r_49[int(2)] = _S589;
                        ok_2 = false;
                        break;
                    }
                    float lat_21 = _S586.y;
                    float cl_21 = (F32_cos((lat_21)));
                    r_49[int(2)] = make_float3 (cl_21 * (F32_sin((_S587))), (F32_sin((lat_21))), cl_21 * (F32_cos((_S587))));
                    ok_2 = true;
                    break;
                }
                float2  uv_u_21;
                bool _S590 = undistort_point_2(_S586, dist_coeffs_16, int(8), &uv_u_21);
                if(!_S590)
                {
                    int3  _S591 = make_int3 (int(0));
                    float3  _S592 = make_float3 ((float)_S591.x, (float)_S591.y, (float)_S591.z);
                    r_49[int(2)] = _S592;
                    ok_2 = false;
                    break;
                }
                float2  _S593 = uv_u_21;
                if(camera_model_6 == int(1))
                {
                    float r_54 = length_0(_S593);
                    if(r_54 >= 3.14159274101257324f)
                    {
                        int3  _S594 = make_int3 (int(0));
                        float3  _S595 = make_float3 ((float)_S594.x, (float)_S594.y, (float)_S594.z);
                        r_49[int(2)] = _S595;
                        ok_2 = false;
                        break;
                    }
                    if(r_54 < 0.00100000004749745f)
                    {
                        s_5 = 1.0f - r_54 * r_54 / 6.0f;
                    }
                    else
                    {
                        s_5 = (F32_sin((r_54))) / r_54;
                    }
                    r_49[int(2)] = make_float3 ((_S593 * make_float2 (s_5)).x, (_S593 * make_float2 (s_5)).y, (F32_cos((r_54))));
                }
                else
                {
                    if(camera_model_6 == int(2))
                    {
                        float r_55 = length_0(_S593);
                        if(r_55 >= 2.0f)
                        {
                            int3  _S596 = make_int3 (int(0));
                            float3  _S597 = make_float3 ((float)_S596.x, (float)_S596.y, (float)_S596.z);
                            r_49[int(2)] = _S597;
                            ok_2 = false;
                            break;
                        }
                        r_49[int(2)] = make_float3 ((_S593 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_55 * r_55)))))))).x, (_S593 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_55 * r_55)))))))).y, 1.0f - 0.5f * r_55 * r_55);
                    }
                    else
                    {
                        r_49[int(2)] = make_float3 (_S593.x, _S593.y, 1.0f);
                    }
                }
                r_49[int(2)] = normalize_0(r_49[int(2)]);
                ok_2 = true;
                break;
            }
        }
        else
        {
            ok_2 = false;
        }
        if(ok_2)
        {
            float2  _S598 = (_S557 - _S558) / _S560;
            for(;;)
            {
                if(_S554)
                {
                    r_49[int(3)] = make_float3 (0.0f, 0.0f, 1.0f);
                    ok_2 = true;
                    break;
                }
                if(camera_model_6 == int(3))
                {
                    float _S599 = _S598.x;
                    if((F32_abs((_S599))) > 3.14159274101257324f)
                    {
                        ok_2 = true;
                    }
                    else
                    {
                        ok_2 = (F32_abs((_S598.y))) > 1.57079637050628662f;
                    }
                    if(ok_2)
                    {
                        int3  _S600 = make_int3 (int(0));
                        float3  _S601 = make_float3 ((float)_S600.x, (float)_S600.y, (float)_S600.z);
                        r_49[int(3)] = _S601;
                        ok_2 = false;
                        break;
                    }
                    float lat_22 = _S598.y;
                    float cl_22 = (F32_cos((lat_22)));
                    r_49[int(3)] = make_float3 (cl_22 * (F32_sin((_S599))), (F32_sin((lat_22))), cl_22 * (F32_cos((_S599))));
                    ok_2 = true;
                    break;
                }
                float2  uv_u_22;
                bool _S602 = undistort_point_2(_S598, dist_coeffs_16, int(8), &uv_u_22);
                if(!_S602)
                {
                    int3  _S603 = make_int3 (int(0));
                    float3  _S604 = make_float3 ((float)_S603.x, (float)_S603.y, (float)_S603.z);
                    r_49[int(3)] = _S604;
                    ok_2 = false;
                    break;
                }
                float2  _S605 = uv_u_22;
                if(camera_model_6 == int(1))
                {
                    float r_56 = length_0(_S605);
                    if(r_56 >= 3.14159274101257324f)
                    {
                        int3  _S606 = make_int3 (int(0));
                        float3  _S607 = make_float3 ((float)_S606.x, (float)_S606.y, (float)_S606.z);
                        r_49[int(3)] = _S607;
                        ok_2 = false;
                        break;
                    }
                    if(r_56 < 0.00100000004749745f)
                    {
                        s_5 = 1.0f - r_56 * r_56 / 6.0f;
                    }
                    else
                    {
                        s_5 = (F32_sin((r_56))) / r_56;
                    }
                    r_49[int(3)] = make_float3 ((_S605 * make_float2 (s_5)).x, (_S605 * make_float2 (s_5)).y, (F32_cos((r_56))));
                }
                else
                {
                    if(camera_model_6 == int(2))
                    {
                        float r_57 = length_0(_S605);
                        if(r_57 >= 2.0f)
                        {
                            int3  _S608 = make_int3 (int(0));
                            float3  _S609 = make_float3 ((float)_S608.x, (float)_S608.y, (float)_S608.z);
                            r_49[int(3)] = _S609;
                            ok_2 = false;
                            break;
                        }
                        r_49[int(3)] = make_float3 ((_S605 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_57 * r_57)))))))).x, (_S605 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_57 * r_57)))))))).y, 1.0f - 0.5f * r_57 * r_57);
                    }
                    else
                    {
                        r_49[int(3)] = make_float3 (_S605.x, _S605.y, 1.0f);
                    }
                }
                r_49[int(3)] = normalize_0(r_49[int(3)]);
                ok_2 = true;
                break;
            }
        }
        else
        {
            ok_2 = false;
        }
        break;
    }
    if(ok_2)
    {
        ok_2 = (dot_1(*_S553, r_49[int(2)])) > 0.5f;
    }
    else
    {
        ok_2 = false;
    }
    if(ok_2)
    {
        ok_2 = (dot_1(r_49[int(1)], r_49[int(3)])) > 0.5f;
    }
    else
    {
        ok_2 = false;
    }
    float3  mid_2;
    if(ok_2)
    {
        float2  _S610 = (make_float2 (0.5f) * (lo_2 + _S556) - _S551) / _S552;
        for(;;)
        {
            if(_S554)
            {
                float3  _S611 = make_float3 (0.0f, 0.0f, 1.0f);
                ok_2 = true;
                mid_2 = _S611;
                break;
            }
            if(camera_model_6 == int(3))
            {
                float _S612 = _S610.x;
                if((F32_abs((_S612))) > 3.14159274101257324f)
                {
                    ok_2 = true;
                }
                else
                {
                    ok_2 = (F32_abs((_S610.y))) > 1.57079637050628662f;
                }
                if(ok_2)
                {
                    int3  _S613 = make_int3 (int(0));
                    float3  _S614 = make_float3 ((float)_S613.x, (float)_S613.y, (float)_S613.z);
                    ok_2 = false;
                    mid_2 = _S614;
                    break;
                }
                float lat_23 = _S610.y;
                float cl_23 = (F32_cos((lat_23)));
                float3  _S615 = make_float3 (cl_23 * (F32_sin((_S612))), (F32_sin((lat_23))), cl_23 * (F32_cos((_S612))));
                ok_2 = true;
                mid_2 = _S615;
                break;
            }
            float2  uv_u_23;
            bool _S616 = undistort_point_2(_S610, dist_coeffs_16, int(8), &uv_u_23);
            if(!_S616)
            {
                int3  _S617 = make_int3 (int(0));
                float3  _S618 = make_float3 ((float)_S617.x, (float)_S617.y, (float)_S617.z);
                ok_2 = false;
                mid_2 = _S618;
                break;
            }
            float2  _S619 = uv_u_23;
            if(camera_model_6 == int(1))
            {
                float r_58 = length_0(_S619);
                if(r_58 >= 3.14159274101257324f)
                {
                    int3  _S620 = make_int3 (int(0));
                    float3  _S621 = make_float3 ((float)_S620.x, (float)_S620.y, (float)_S620.z);
                    ok_2 = false;
                    mid_2 = _S621;
                    break;
                }
                if(r_58 < 0.00100000004749745f)
                {
                    s_5 = 1.0f - r_58 * r_58 / 6.0f;
                }
                else
                {
                    s_5 = (F32_sin((r_58))) / r_58;
                }
                mid_2 = make_float3 ((_S619 * make_float2 (s_5)).x, (_S619 * make_float2 (s_5)).y, (F32_cos((r_58))));
            }
            else
            {
                if(camera_model_6 == int(2))
                {
                    float r_59 = length_0(_S619);
                    if(r_59 >= 2.0f)
                    {
                        int3  _S622 = make_int3 (int(0));
                        float3  _S623 = make_float3 ((float)_S622.x, (float)_S622.y, (float)_S622.z);
                        ok_2 = false;
                        mid_2 = _S623;
                        break;
                    }
                    mid_2 = make_float3 ((_S619 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_59 * r_59)))))))).x, (_S619 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_59 * r_59)))))))).y, 1.0f - 0.5f * r_59 * r_59);
                }
                else
                {
                    mid_2 = make_float3 (_S619.x, _S619.y, 1.0f);
                }
            }
            float3  _S624 = normalize_0(mid_2);
            ok_2 = true;
            mid_2 = _S624;
            break;
        }
    }
    else
    {
        ok_2 = false;
    }
    float3  along_8 = r_49[int(1)] - *_S553;
    float3  _S625 = cross_0(*_S553, along_8);
    n_2[int(0)] = _S625;
    float3  chord_8 = normalize_0(*_S553 + r_49[int(1)]);
    if((dot_1(_S625, mid_2)) < 0.0f)
    {
        n_2[int(0)] = cross_0(mid_2 + (mid_2 - chord_8), along_8);
    }
    if(ok_2)
    {
        float2  _S626 = (make_float2 (0.5f) * (_S556 + hi_2) - _S551) / _S552;
        for(;;)
        {
            if(_S554)
            {
                float3  _S627 = make_float3 (0.0f, 0.0f, 1.0f);
                ok_2 = true;
                mid_2 = _S627;
                break;
            }
            if(camera_model_6 == int(3))
            {
                float _S628 = _S626.x;
                if((F32_abs((_S628))) > 3.14159274101257324f)
                {
                    ok_2 = true;
                }
                else
                {
                    ok_2 = (F32_abs((_S626.y))) > 1.57079637050628662f;
                }
                if(ok_2)
                {
                    int3  _S629 = make_int3 (int(0));
                    float3  _S630 = make_float3 ((float)_S629.x, (float)_S629.y, (float)_S629.z);
                    ok_2 = false;
                    mid_2 = _S630;
                    break;
                }
                float lat_24 = _S626.y;
                float cl_24 = (F32_cos((lat_24)));
                float3  _S631 = make_float3 (cl_24 * (F32_sin((_S628))), (F32_sin((lat_24))), cl_24 * (F32_cos((_S628))));
                ok_2 = true;
                mid_2 = _S631;
                break;
            }
            float2  uv_u_24;
            bool _S632 = undistort_point_2(_S626, dist_coeffs_16, int(8), &uv_u_24);
            if(!_S632)
            {
                int3  _S633 = make_int3 (int(0));
                float3  _S634 = make_float3 ((float)_S633.x, (float)_S633.y, (float)_S633.z);
                ok_2 = false;
                mid_2 = _S634;
                break;
            }
            float2  _S635 = uv_u_24;
            if(camera_model_6 == int(1))
            {
                float r_60 = length_0(_S635);
                if(r_60 >= 3.14159274101257324f)
                {
                    int3  _S636 = make_int3 (int(0));
                    float3  _S637 = make_float3 ((float)_S636.x, (float)_S636.y, (float)_S636.z);
                    ok_2 = false;
                    mid_2 = _S637;
                    break;
                }
                if(r_60 < 0.00100000004749745f)
                {
                    s_5 = 1.0f - r_60 * r_60 / 6.0f;
                }
                else
                {
                    s_5 = (F32_sin((r_60))) / r_60;
                }
                mid_2 = make_float3 ((_S635 * make_float2 (s_5)).x, (_S635 * make_float2 (s_5)).y, (F32_cos((r_60))));
            }
            else
            {
                if(camera_model_6 == int(2))
                {
                    float r_61 = length_0(_S635);
                    if(r_61 >= 2.0f)
                    {
                        int3  _S638 = make_int3 (int(0));
                        float3  _S639 = make_float3 ((float)_S638.x, (float)_S638.y, (float)_S638.z);
                        ok_2 = false;
                        mid_2 = _S639;
                        break;
                    }
                    mid_2 = make_float3 ((_S635 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_61 * r_61)))))))).x, (_S635 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_61 * r_61)))))))).y, 1.0f - 0.5f * r_61 * r_61);
                }
                else
                {
                    mid_2 = make_float3 (_S635.x, _S635.y, 1.0f);
                }
            }
            float3  _S640 = normalize_0(mid_2);
            ok_2 = true;
            mid_2 = _S640;
            break;
        }
    }
    else
    {
        ok_2 = false;
    }
    float3  along_9 = r_49[int(2)] - r_49[int(1)];
    float3  _S641 = cross_0(r_49[int(1)], along_9);
    n_2[int(1)] = _S641;
    float3  chord_9 = normalize_0(r_49[int(1)] + r_49[int(2)]);
    if((dot_1(_S641, mid_2)) < 0.0f)
    {
        n_2[int(1)] = cross_0(mid_2 + (mid_2 - chord_9), along_9);
    }
    if(ok_2)
    {
        float2  _S642 = (make_float2 (0.5f) * (hi_2 + _S557) - _S551) / _S552;
        for(;;)
        {
            if(_S554)
            {
                float3  _S643 = make_float3 (0.0f, 0.0f, 1.0f);
                ok_2 = true;
                mid_2 = _S643;
                break;
            }
            if(camera_model_6 == int(3))
            {
                float _S644 = _S642.x;
                if((F32_abs((_S644))) > 3.14159274101257324f)
                {
                    ok_2 = true;
                }
                else
                {
                    ok_2 = (F32_abs((_S642.y))) > 1.57079637050628662f;
                }
                if(ok_2)
                {
                    int3  _S645 = make_int3 (int(0));
                    float3  _S646 = make_float3 ((float)_S645.x, (float)_S645.y, (float)_S645.z);
                    ok_2 = false;
                    mid_2 = _S646;
                    break;
                }
                float lat_25 = _S642.y;
                float cl_25 = (F32_cos((lat_25)));
                float3  _S647 = make_float3 (cl_25 * (F32_sin((_S644))), (F32_sin((lat_25))), cl_25 * (F32_cos((_S644))));
                ok_2 = true;
                mid_2 = _S647;
                break;
            }
            float2  uv_u_25;
            bool _S648 = undistort_point_2(_S642, dist_coeffs_16, int(8), &uv_u_25);
            if(!_S648)
            {
                int3  _S649 = make_int3 (int(0));
                float3  _S650 = make_float3 ((float)_S649.x, (float)_S649.y, (float)_S649.z);
                ok_2 = false;
                mid_2 = _S650;
                break;
            }
            float2  _S651 = uv_u_25;
            if(camera_model_6 == int(1))
            {
                float r_62 = length_0(_S651);
                if(r_62 >= 3.14159274101257324f)
                {
                    int3  _S652 = make_int3 (int(0));
                    float3  _S653 = make_float3 ((float)_S652.x, (float)_S652.y, (float)_S652.z);
                    ok_2 = false;
                    mid_2 = _S653;
                    break;
                }
                if(r_62 < 0.00100000004749745f)
                {
                    s_5 = 1.0f - r_62 * r_62 / 6.0f;
                }
                else
                {
                    s_5 = (F32_sin((r_62))) / r_62;
                }
                mid_2 = make_float3 ((_S651 * make_float2 (s_5)).x, (_S651 * make_float2 (s_5)).y, (F32_cos((r_62))));
            }
            else
            {
                if(camera_model_6 == int(2))
                {
                    float r_63 = length_0(_S651);
                    if(r_63 >= 2.0f)
                    {
                        int3  _S654 = make_int3 (int(0));
                        float3  _S655 = make_float3 ((float)_S654.x, (float)_S654.y, (float)_S654.z);
                        ok_2 = false;
                        mid_2 = _S655;
                        break;
                    }
                    mid_2 = make_float3 ((_S651 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_63 * r_63)))))))).x, (_S651 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_63 * r_63)))))))).y, 1.0f - 0.5f * r_63 * r_63);
                }
                else
                {
                    mid_2 = make_float3 (_S651.x, _S651.y, 1.0f);
                }
            }
            float3  _S656 = normalize_0(mid_2);
            ok_2 = true;
            mid_2 = _S656;
            break;
        }
    }
    else
    {
        ok_2 = false;
    }
    float3  along_10 = r_49[int(3)] - r_49[int(2)];
    float3  _S657 = cross_0(r_49[int(2)], along_10);
    n_2[int(2)] = _S657;
    float3  chord_10 = normalize_0(r_49[int(2)] + r_49[int(3)]);
    if((dot_1(_S657, mid_2)) < 0.0f)
    {
        n_2[int(2)] = cross_0(mid_2 + (mid_2 - chord_10), along_10);
    }
    if(ok_2)
    {
        float2  _S658 = (make_float2 (0.5f) * (_S557 + lo_2) - _S551) / _S552;
        for(;;)
        {
            if(_S554)
            {
                float3  _S659 = make_float3 (0.0f, 0.0f, 1.0f);
                ok_2 = true;
                mid_2 = _S659;
                break;
            }
            if(camera_model_6 == int(3))
            {
                float _S660 = _S658.x;
                if((F32_abs((_S660))) > 3.14159274101257324f)
                {
                    ok_2 = true;
                }
                else
                {
                    ok_2 = (F32_abs((_S658.y))) > 1.57079637050628662f;
                }
                if(ok_2)
                {
                    int3  _S661 = make_int3 (int(0));
                    float3  _S662 = make_float3 ((float)_S661.x, (float)_S661.y, (float)_S661.z);
                    ok_2 = false;
                    mid_2 = _S662;
                    break;
                }
                float lat_26 = _S658.y;
                float cl_26 = (F32_cos((lat_26)));
                float3  _S663 = make_float3 (cl_26 * (F32_sin((_S660))), (F32_sin((lat_26))), cl_26 * (F32_cos((_S660))));
                ok_2 = true;
                mid_2 = _S663;
                break;
            }
            float2  uv_u_26;
            bool _S664 = undistort_point_2(_S658, dist_coeffs_16, int(8), &uv_u_26);
            if(!_S664)
            {
                int3  _S665 = make_int3 (int(0));
                float3  _S666 = make_float3 ((float)_S665.x, (float)_S665.y, (float)_S665.z);
                ok_2 = false;
                mid_2 = _S666;
                break;
            }
            float2  _S667 = uv_u_26;
            if(camera_model_6 == int(1))
            {
                float r_64 = length_0(_S667);
                if(r_64 >= 3.14159274101257324f)
                {
                    int3  _S668 = make_int3 (int(0));
                    float3  _S669 = make_float3 ((float)_S668.x, (float)_S668.y, (float)_S668.z);
                    ok_2 = false;
                    mid_2 = _S669;
                    break;
                }
                if(r_64 < 0.00100000004749745f)
                {
                    s_5 = 1.0f - r_64 * r_64 / 6.0f;
                }
                else
                {
                    s_5 = (F32_sin((r_64))) / r_64;
                }
                mid_2 = make_float3 ((_S667 * make_float2 (s_5)).x, (_S667 * make_float2 (s_5)).y, (F32_cos((r_64))));
            }
            else
            {
                if(camera_model_6 == int(2))
                {
                    float r_65 = length_0(_S667);
                    if(r_65 >= 2.0f)
                    {
                        int3  _S670 = make_int3 (int(0));
                        float3  _S671 = make_float3 ((float)_S670.x, (float)_S670.y, (float)_S670.z);
                        ok_2 = false;
                        mid_2 = _S671;
                        break;
                    }
                    mid_2 = make_float3 ((_S667 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_65 * r_65)))))))).x, (_S667 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_65 * r_65)))))))).y, 1.0f - 0.5f * r_65 * r_65);
                }
                else
                {
                    mid_2 = make_float3 (_S667.x, _S667.y, 1.0f);
                }
            }
            float3  _S672 = normalize_0(mid_2);
            ok_2 = true;
            mid_2 = _S672;
            break;
        }
    }
    else
    {
        ok_2 = false;
    }
    float3  along_11 = *_S553 - r_49[int(3)];
    float3  _S673 = cross_0(r_49[int(3)], along_11);
    n_2[int(3)] = _S673;
    float3  chord_11 = normalize_0(r_49[int(3)] + *_S553);
    if((dot_1(_S673, mid_2)) < 0.0f)
    {
        n_2[int(3)] = cross_0(mid_2 + (mid_2 - chord_11), along_11);
    }
    if(!ok_2)
    {
        return false;
    }
    FixedArray<float3 , 4>  q_6;
    float3  x_23 = cross_0(n_2[int(3)], n_2[int(0)]);
    float xx_8 = dot_1(x_23, x_23);
    if(xx_8 > 9.99999968265522539e-21f)
    {
        float _S674 = (F32_rsqrt((xx_8)));
        if((dot_1(x_23, *_S553)) < 0.0f)
        {
            s_5 = -1.0f;
        }
        else
        {
            s_5 = 1.0f;
        }
        mid_2 = x_23 * make_float3 (_S674 * s_5);
    }
    else
    {
        mid_2 = *_S553;
    }
    q_6[int(0)] = mid_2;
    float3  x_24 = cross_0(n_2[int(0)], n_2[int(1)]);
    float xx_9 = dot_1(x_24, x_24);
    if(xx_9 > 9.99999968265522539e-21f)
    {
        float _S675 = (F32_rsqrt((xx_9)));
        if((dot_1(x_24, r_49[int(1)])) < 0.0f)
        {
            s_5 = -1.0f;
        }
        else
        {
            s_5 = 1.0f;
        }
        mid_2 = x_24 * make_float3 (_S675 * s_5);
    }
    else
    {
        mid_2 = r_49[int(1)];
    }
    q_6[int(1)] = mid_2;
    float3  x_25 = cross_0(n_2[int(1)], n_2[int(2)]);
    float xx_10 = dot_1(x_25, x_25);
    if(xx_10 > 9.99999968265522539e-21f)
    {
        float _S676 = (F32_rsqrt((xx_10)));
        if((dot_1(x_25, r_49[int(2)])) < 0.0f)
        {
            s_5 = -1.0f;
        }
        else
        {
            s_5 = 1.0f;
        }
        mid_2 = x_25 * make_float3 (_S676 * s_5);
    }
    else
    {
        mid_2 = r_49[int(2)];
    }
    q_6[int(2)] = mid_2;
    float3  x_26 = cross_0(n_2[int(2)], n_2[int(3)]);
    float xx_11 = dot_1(x_26, x_26);
    if(xx_11 > 9.99999968265522539e-21f)
    {
        float _S677 = (F32_rsqrt((xx_11)));
        if((dot_1(x_26, r_49[int(3)])) < 0.0f)
        {
            s_5 = -1.0f;
        }
        else
        {
            s_5 = 1.0f;
        }
        mid_2 = x_26 * make_float3 (_S677 * s_5);
    }
    else
    {
        mid_2 = r_49[int(3)];
    }
    q_6[int(3)] = mid_2;
    *q0_2 = q_6[int(0)];
    *q1_2 = q_6[int(1)];
    *q2_2 = q_6[int(2)];
    *q3_2 = q_6[int(3)];
    return true;
}

inline __device__ bool pixel_rect_quad_none(int camera_model_7, float4  intrins_11, FixedArray<float, 1>  dist_coeffs_17, float2  lo_3, float2  hi_3, float3  * q0_3, float3  * q1_3, float3  * q2_3, float3  * q3_3)
{
    FixedArray<float, 1>  _S678 = dist_coeffs_17;
    bool _S679 = pixel_rect_quad_0(camera_model_7, intrins_11, &_S678, lo_3, hi_3, q0_3, q1_3, q2_3, q3_3);
    return _S679;
}

inline __device__ bool is_valid_distortion_opencv(float2  uv_16, FixedArray<float, 4>  dist_coeffs_18)
{
    float2  _S680 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S681;
    (&_S681)->primal_0 = uv_16;
    (&_S681)->differential_0 = _S680;
    FixedArray<float, 4>  _S682 = dist_coeffs_18;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S683 = s_fwd_DistOpenCV_distort_0(&_S681, &_S682);
    float2  _S684 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S685;
    (&_S685)->primal_0 = uv_16;
    (&_S685)->differential_0 = _S684;
    FixedArray<float, 4>  _S686 = dist_coeffs_18;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S687 = s_fwd_DistOpenCV_distort_0(&_S685, &_S686);
    Matrix<float, 2, 2>  _S688 = transpose_1(makeMatrix<float, 2, 2> (_S683.differential_0, _S687.differential_0));
    float _S689 = (F32_min((determinant_0(_S688)), ((F32_min((_S688.rows[int(0)].x), (_S688.rows[int(1)].y))))));
    bool _S690;
    if(_S689 > 0.25f)
    {
        _S690 = _S689 < 4.0f;
    }
    else
    {
        _S690 = false;
    }
    if(_S690)
    {
        FixedArray<float, 4>  _S691 = dist_coeffs_18;
        float2  _S692 = DistOpenCV_distort_0(uv_16, &_S691);
        _S690 = (dot_0(uv_16, _S692)) >= 0.0f;
    }
    else
    {
        _S690 = false;
    }
    return _S690;
}

inline __device__ bool persp_proj_nav_opencv(float3  p_view_8, float4  intrins_12, FixedArray<float, 4>  dist_coeffs_19, float2  * uv_17)
{
    bool _S693;
    for(;;)
    {
        float2  _S694 = float2 {p_view_8.x, p_view_8.y};
        float _S695 = p_view_8.z;
        float2  uv0_1 = _S694 / make_float2 (_S695);
        if(_S695 < 0.0f)
        {
            _S693 = true;
        }
        else
        {
            float2  _S696 = make_float2 (1.0f, 0.0f);
            DiffPair_vectorx3Cfloatx2C2x3E_0 _S697;
            (&_S697)->primal_0 = uv0_1;
            (&_S697)->differential_0 = _S696;
            FixedArray<float, 4>  _S698 = dist_coeffs_19;
            DiffPair_vectorx3Cfloatx2C2x3E_0 _S699 = s_fwd_DistOpenCV_distort_0(&_S697, &_S698);
            float2  _S700 = make_float2 (0.0f, 1.0f);
            DiffPair_vectorx3Cfloatx2C2x3E_0 _S701;
            (&_S701)->primal_0 = uv0_1;
            (&_S701)->differential_0 = _S700;
            FixedArray<float, 4>  _S702 = dist_coeffs_19;
            DiffPair_vectorx3Cfloatx2C2x3E_0 _S703 = s_fwd_DistOpenCV_distort_0(&_S701, &_S702);
            Matrix<float, 2, 2>  _S704 = transpose_1(makeMatrix<float, 2, 2> (_S699.differential_0, _S703.differential_0));
            float _S705 = (F32_min((determinant_0(_S704)), ((F32_min((_S704.rows[int(0)].x), (_S704.rows[int(1)].y))))));
            if(_S705 > 0.25f)
            {
                _S693 = _S705 < 4.0f;
            }
            else
            {
                _S693 = false;
            }
            if(_S693)
            {
                FixedArray<float, 4>  _S706 = dist_coeffs_19;
                float2  _S707 = DistOpenCV_distort_0(uv0_1, &_S706);
                _S693 = (dot_0(uv0_1, _S707)) >= 0.0f;
            }
            else
            {
                _S693 = false;
            }
            _S693 = !_S693;
        }
        if(_S693)
        {
            *uv_17 = uv0_1;
            _S693 = false;
            break;
        }
        float2  uv_18 = _S694 / make_float2 (_S695);
        FixedArray<float, 4>  _S708 = dist_coeffs_19;
        float2  _S709 = DistOpenCV_distort_0(uv_18, &_S708);
        *uv_17 = make_float2 (intrins_12.x * _S709.x + intrins_12.z, intrins_12.y * _S709.y + intrins_12.w);
        _S693 = true;
        break;
    }
    return _S693;
}

inline __device__ bool fisheye_proj_nav_opencv(float3  p_view_9, float4  intrins_13, FixedArray<float, 4>  dist_coeffs_20, float2  * uv_19)
{
    bool _S710;
    for(;;)
    {
        float2  _S711 = float2 {p_view_9.x, p_view_9.y};
        float r_66 = length_0(_S711);
        float _S712 = p_view_9.z;
        float theta_4 = (F32_atan2((r_66), (_S712)));
        bool _S713 = theta_4 < 0.00100000004749745f;
        float k_5;
        if(_S713)
        {
            k_5 = (1.0f - theta_4 * theta_4 / 3.0f) / _S712;
        }
        else
        {
            k_5 = theta_4 / r_66;
        }
        float2  _S714 = _S711 * make_float2 (k_5);
        float2  _S715 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S716;
        (&_S716)->primal_0 = _S714;
        (&_S716)->differential_0 = _S715;
        FixedArray<float, 4>  _S717 = dist_coeffs_20;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S718 = s_fwd_DistOpenCV_distort_0(&_S716, &_S717);
        float2  _S719 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S720;
        (&_S720)->primal_0 = _S714;
        (&_S720)->differential_0 = _S719;
        FixedArray<float, 4>  _S721 = dist_coeffs_20;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S722 = s_fwd_DistOpenCV_distort_0(&_S720, &_S721);
        Matrix<float, 2, 2>  _S723 = transpose_1(makeMatrix<float, 2, 2> (_S718.differential_0, _S722.differential_0));
        float _S724 = (F32_min((determinant_0(_S723)), ((F32_min((_S723.rows[int(0)].x), (_S723.rows[int(1)].y))))));
        if(_S724 > 0.25f)
        {
            _S710 = _S724 < 4.0f;
        }
        else
        {
            _S710 = false;
        }
        if(_S710)
        {
            FixedArray<float, 4>  _S725 = dist_coeffs_20;
            float2  _S726 = DistOpenCV_distort_0(_S714, &_S725);
            _S710 = (dot_0(_S714, _S726)) >= 0.0f;
        }
        else
        {
            _S710 = false;
        }
        if(!_S710)
        {
            *uv_19 = _S714;
            _S710 = false;
            break;
        }
        if(_S713)
        {
            k_5 = (1.0f - theta_4 * theta_4 / 3.0f) / _S712;
        }
        else
        {
            k_5 = theta_4 / r_66;
        }
        float2  _S727 = _S711 * make_float2 (k_5);
        FixedArray<float, 4>  _S728 = dist_coeffs_20;
        float2  _S729 = DistOpenCV_distort_0(_S727, &_S728);
        *uv_19 = make_float2 (intrins_13.x * _S729.x + intrins_13.z, intrins_13.y * _S729.y + intrins_13.w);
        _S710 = true;
        break;
    }
    return _S710;
}

inline __device__ bool equisolid_proj_nav_opencv(float3  p_view_10, float4  intrins_14, FixedArray<float, 4>  dist_coeffs_21, float2  * uv_20)
{
    bool _S730;
    for(;;)
    {
        float2  _S731 = float2 {p_view_10.x, p_view_10.y};
        float r_67 = length_0(_S731);
        float _S732 = p_view_10.z;
        float theta_5 = (F32_atan2((r_67), (_S732)));
        bool _S733 = r_67 < 9.99999997475242708e-07f;
        float k_6;
        if(_S733)
        {
            k_6 = (1.0f - theta_5 * theta_5 / 24.0f) / _S732;
        }
        else
        {
            k_6 = 2.0f * (F32_sin((0.5f * theta_5))) / r_67;
        }
        float2  _S734 = _S731 * make_float2 (k_6);
        float2  _S735 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S736;
        (&_S736)->primal_0 = _S734;
        (&_S736)->differential_0 = _S735;
        FixedArray<float, 4>  _S737 = dist_coeffs_21;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S738 = s_fwd_DistOpenCV_distort_0(&_S736, &_S737);
        float2  _S739 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S740;
        (&_S740)->primal_0 = _S734;
        (&_S740)->differential_0 = _S739;
        FixedArray<float, 4>  _S741 = dist_coeffs_21;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S742 = s_fwd_DistOpenCV_distort_0(&_S740, &_S741);
        Matrix<float, 2, 2>  _S743 = transpose_1(makeMatrix<float, 2, 2> (_S738.differential_0, _S742.differential_0));
        float _S744 = (F32_min((determinant_0(_S743)), ((F32_min((_S743.rows[int(0)].x), (_S743.rows[int(1)].y))))));
        if(_S744 > 0.25f)
        {
            _S730 = _S744 < 4.0f;
        }
        else
        {
            _S730 = false;
        }
        if(_S730)
        {
            FixedArray<float, 4>  _S745 = dist_coeffs_21;
            float2  _S746 = DistOpenCV_distort_0(_S734, &_S745);
            _S730 = (dot_0(_S734, _S746)) >= 0.0f;
        }
        else
        {
            _S730 = false;
        }
        if(!_S730)
        {
            *uv_20 = _S734;
            _S730 = false;
            break;
        }
        if(_S733)
        {
            k_6 = (1.0f - theta_5 * theta_5 / 24.0f) / _S732;
        }
        else
        {
            k_6 = 2.0f * (F32_sin((0.5f * theta_5))) / r_67;
        }
        float2  _S747 = _S731 * make_float2 (k_6);
        FixedArray<float, 4>  _S748 = dist_coeffs_21;
        float2  _S749 = DistOpenCV_distort_0(_S747, &_S748);
        *uv_20 = make_float2 (intrins_14.x * _S749.x + intrins_14.z, intrins_14.y * _S749.y + intrins_14.w);
        _S730 = true;
        break;
    }
    return _S730;
}

inline __device__ Matrix<float, 2, 3>  persp_proj_jac_opencv(float3  p_view_11, float4  intrins_15, FixedArray<float, 4>  dist_coeffs_22)
{
    float2  _S750 = float2 {p_view_11.x, p_view_11.y};
    float _S751 = p_view_11.z;
    float2  _S752 = _S750 * make_float2 (0.0f);
    float _S753 = _S751 * _S751;
    float2  s_diff_uv_3 = (make_float2 (1.0f, 0.0f) * make_float2 (_S751) - _S752) / make_float2 (_S753);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S754;
    (&_S754)->primal_0 = _S750 / make_float2 (_S751);
    (&_S754)->differential_0 = s_diff_uv_3;
    FixedArray<float, 4>  _S755 = dist_coeffs_22;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S756 = s_fwd_DistOpenCV_distort_0(&_S754, &_S755);
    float fx_4 = intrins_15.x;
    float fy_4 = intrins_15.y;
    float _S757 = _S756.differential_0.y * fy_4;
    Matrix<float, 2, 3>  J_4;
    *&(((&J_4)->rows + (int(0)))->x) = _S756.differential_0.x * fx_4;
    *&(((&J_4)->rows + (int(1)))->x) = _S757;
    float2  s_diff_uv_4 = (make_float2 (0.0f, 1.0f) * make_float2 (_S751) - _S752) / make_float2 (_S753);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S758;
    (&_S758)->primal_0 = _S750 / make_float2 (_S751);
    (&_S758)->differential_0 = s_diff_uv_4;
    FixedArray<float, 4>  _S759 = dist_coeffs_22;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S760 = s_fwd_DistOpenCV_distort_0(&_S758, &_S759);
    float _S761 = _S760.differential_0.y * fy_4;
    *&(((&J_4)->rows + (int(0)))->y) = _S760.differential_0.x * fx_4;
    *&(((&J_4)->rows + (int(1)))->y) = _S761;
    float2  s_diff_uv_5 = (make_float2 (0.0f, 0.0f) * make_float2 (_S751) - _S750) / make_float2 (_S753);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S762;
    (&_S762)->primal_0 = _S750 / make_float2 (_S751);
    (&_S762)->differential_0 = s_diff_uv_5;
    FixedArray<float, 4>  _S763 = dist_coeffs_22;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S764 = s_fwd_DistOpenCV_distort_0(&_S762, &_S763);
    float _S765 = _S764.differential_0.y * fy_4;
    *&(((&J_4)->rows + (int(0)))->z) = _S764.differential_0.x * fx_4;
    *&(((&J_4)->rows + (int(1)))->z) = _S765;
    return J_4;
}

inline __device__ Matrix<float, 2, 3>  fisheye_proj_jac_opencv(float3  p_view_12, float4  intrins_16, FixedArray<float, 4>  dist_coeffs_23)
{
    Matrix<float, 2, 3>  J_5;
    float2  _S766 = float2 {p_view_12.x, p_view_12.y};
    float2  _S767 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S768;
    (&_S768)->primal_0 = _S766;
    (&_S768)->differential_0 = _S767;
    DiffPair_float_0 _S769 = s_fwd_length_impl_0(&_S768);
    float _S770 = p_view_12.z;
    DiffPair_float_0 _S771;
    (&_S771)->primal_0 = _S769.primal_0;
    (&_S771)->differential_0 = _S769.differential_0;
    DiffPair_float_0 _S772;
    (&_S772)->primal_0 = _S770;
    (&_S772)->differential_0 = 0.0f;
    DiffPair_float_0 _S773 = _d_atan2_0(&_S771, &_S772);
    float k_7;
    float s_diff_k_2;
    if((_S773.primal_0) < 0.00100000004749745f)
    {
        float _S774 = _S773.differential_0 * _S773.primal_0;
        float _S775 = 1.0f - _S773.primal_0 * _S773.primal_0 / 3.0f;
        float _S776 = ((0.0f - (_S774 + _S774) * 0.3333333432674408f) * _S770 - _S775 * 0.0f) / (_S770 * _S770);
        k_7 = _S775 / _S770;
        s_diff_k_2 = _S776;
    }
    else
    {
        float _S777 = (_S773.differential_0 * _S769.primal_0 - _S773.primal_0 * _S769.differential_0) / (_S769.primal_0 * _S769.primal_0);
        k_7 = _S773.primal_0 / _S769.primal_0;
        s_diff_k_2 = _S777;
    }
    float2  _S778 = _S766 * make_float2 (k_7);
    float2  _S779 = _S767 * make_float2 (k_7) + make_float2 (s_diff_k_2) * _S766;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S780;
    (&_S780)->primal_0 = _S778;
    (&_S780)->differential_0 = _S779;
    FixedArray<float, 4>  _S781 = dist_coeffs_23;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S782 = s_fwd_DistOpenCV_distort_0(&_S780, &_S781);
    float fx_5 = intrins_16.x;
    float fy_5 = intrins_16.y;
    float _S783 = _S782.differential_0.y * fy_5;
    *&(((&J_5)->rows + (int(0)))->x) = _S782.differential_0.x * fx_5;
    *&(((&J_5)->rows + (int(1)))->x) = _S783;
    float2  _S784 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S785;
    (&_S785)->primal_0 = _S766;
    (&_S785)->differential_0 = _S784;
    DiffPair_float_0 _S786 = s_fwd_length_impl_0(&_S785);
    DiffPair_float_0 _S787;
    (&_S787)->primal_0 = _S786.primal_0;
    (&_S787)->differential_0 = _S786.differential_0;
    DiffPair_float_0 _S788;
    (&_S788)->primal_0 = _S770;
    (&_S788)->differential_0 = 0.0f;
    DiffPair_float_0 _S789 = _d_atan2_0(&_S787, &_S788);
    if((_S789.primal_0) < 0.00100000004749745f)
    {
        float _S790 = _S789.differential_0 * _S789.primal_0;
        float _S791 = 1.0f - _S789.primal_0 * _S789.primal_0 / 3.0f;
        float _S792 = ((0.0f - (_S790 + _S790) * 0.3333333432674408f) * _S770 - _S791 * 0.0f) / (_S770 * _S770);
        k_7 = _S791 / _S770;
        s_diff_k_2 = _S792;
    }
    else
    {
        float _S793 = (_S789.differential_0 * _S786.primal_0 - _S789.primal_0 * _S786.differential_0) / (_S786.primal_0 * _S786.primal_0);
        k_7 = _S789.primal_0 / _S786.primal_0;
        s_diff_k_2 = _S793;
    }
    float2  _S794 = _S766 * make_float2 (k_7);
    float2  _S795 = _S784 * make_float2 (k_7) + make_float2 (s_diff_k_2) * _S766;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S796;
    (&_S796)->primal_0 = _S794;
    (&_S796)->differential_0 = _S795;
    FixedArray<float, 4>  _S797 = dist_coeffs_23;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S798 = s_fwd_DistOpenCV_distort_0(&_S796, &_S797);
    float _S799 = _S798.differential_0.y * fy_5;
    *&(((&J_5)->rows + (int(0)))->y) = _S798.differential_0.x * fx_5;
    *&(((&J_5)->rows + (int(1)))->y) = _S799;
    float2  _S800 = make_float2 (0.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S801;
    (&_S801)->primal_0 = _S766;
    (&_S801)->differential_0 = _S800;
    DiffPair_float_0 _S802 = s_fwd_length_impl_0(&_S801);
    DiffPair_float_0 _S803;
    (&_S803)->primal_0 = _S802.primal_0;
    (&_S803)->differential_0 = _S802.differential_0;
    DiffPair_float_0 _S804;
    (&_S804)->primal_0 = _S770;
    (&_S804)->differential_0 = 1.0f;
    DiffPair_float_0 _S805 = _d_atan2_0(&_S803, &_S804);
    if((_S805.primal_0) < 0.00100000004749745f)
    {
        float _S806 = _S805.differential_0 * _S805.primal_0;
        float _S807 = 1.0f - _S805.primal_0 * _S805.primal_0 / 3.0f;
        float _S808 = ((0.0f - (_S806 + _S806) * 0.3333333432674408f) * _S770 - _S807) / (_S770 * _S770);
        k_7 = _S807 / _S770;
        s_diff_k_2 = _S808;
    }
    else
    {
        float _S809 = (_S805.differential_0 * _S802.primal_0 - _S805.primal_0 * _S802.differential_0) / (_S802.primal_0 * _S802.primal_0);
        k_7 = _S805.primal_0 / _S802.primal_0;
        s_diff_k_2 = _S809;
    }
    float2  _S810 = _S766 * make_float2 (k_7);
    float2  _S811 = _S800 * make_float2 (k_7) + make_float2 (s_diff_k_2) * _S766;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S812;
    (&_S812)->primal_0 = _S810;
    (&_S812)->differential_0 = _S811;
    FixedArray<float, 4>  _S813 = dist_coeffs_23;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S814 = s_fwd_DistOpenCV_distort_0(&_S812, &_S813);
    float _S815 = _S814.differential_0.y * fy_5;
    *&(((&J_5)->rows + (int(0)))->z) = _S814.differential_0.x * fx_5;
    *&(((&J_5)->rows + (int(1)))->z) = _S815;
    return J_5;
}

inline __device__ Matrix<float, 2, 3>  equisolid_proj_jac_opencv(float3  p_view_13, float4  intrins_17, FixedArray<float, 4>  dist_coeffs_24)
{
    Matrix<float, 2, 3>  J_6;
    float2  _S816 = float2 {p_view_13.x, p_view_13.y};
    float2  _S817 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S818;
    (&_S818)->primal_0 = _S816;
    (&_S818)->differential_0 = _S817;
    DiffPair_float_0 _S819 = s_fwd_length_impl_0(&_S818);
    float _S820 = p_view_13.z;
    DiffPair_float_0 _S821;
    (&_S821)->primal_0 = _S819.primal_0;
    (&_S821)->differential_0 = _S819.differential_0;
    DiffPair_float_0 _S822;
    (&_S822)->primal_0 = _S820;
    (&_S822)->differential_0 = 0.0f;
    DiffPair_float_0 _S823 = _d_atan2_0(&_S821, &_S822);
    float k_8;
    float s_diff_k_3;
    if((_S819.primal_0) < 9.99999997475242708e-07f)
    {
        float _S824 = _S823.differential_0 * _S823.primal_0;
        float _S825 = 1.0f - _S823.primal_0 * _S823.primal_0 / 24.0f;
        float _S826 = ((0.0f - (_S824 + _S824) * 0.0416666679084301f) * _S820 - _S825 * 0.0f) / (_S820 * _S820);
        k_8 = _S825 / _S820;
        s_diff_k_3 = _S826;
    }
    else
    {
        float _S827 = _S823.differential_0 * 0.5f;
        DiffPair_float_0 _S828;
        (&_S828)->primal_0 = 0.5f * _S823.primal_0;
        (&_S828)->differential_0 = _S827;
        DiffPair_float_0 _S829 = _d_sin_0(&_S828);
        float _S830 = 2.0f * _S829.primal_0;
        float _S831 = (_S829.differential_0 * 2.0f * _S819.primal_0 - _S830 * _S819.differential_0) / (_S819.primal_0 * _S819.primal_0);
        k_8 = _S830 / _S819.primal_0;
        s_diff_k_3 = _S831;
    }
    float2  _S832 = _S816 * make_float2 (k_8);
    float2  _S833 = _S817 * make_float2 (k_8) + make_float2 (s_diff_k_3) * _S816;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S834;
    (&_S834)->primal_0 = _S832;
    (&_S834)->differential_0 = _S833;
    FixedArray<float, 4>  _S835 = dist_coeffs_24;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S836 = s_fwd_DistOpenCV_distort_0(&_S834, &_S835);
    float fx_6 = intrins_17.x;
    float fy_6 = intrins_17.y;
    float _S837 = _S836.differential_0.y * fy_6;
    *&(((&J_6)->rows + (int(0)))->x) = _S836.differential_0.x * fx_6;
    *&(((&J_6)->rows + (int(1)))->x) = _S837;
    float2  _S838 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S839;
    (&_S839)->primal_0 = _S816;
    (&_S839)->differential_0 = _S838;
    DiffPair_float_0 _S840 = s_fwd_length_impl_0(&_S839);
    DiffPair_float_0 _S841;
    (&_S841)->primal_0 = _S840.primal_0;
    (&_S841)->differential_0 = _S840.differential_0;
    DiffPair_float_0 _S842;
    (&_S842)->primal_0 = _S820;
    (&_S842)->differential_0 = 0.0f;
    DiffPair_float_0 _S843 = _d_atan2_0(&_S841, &_S842);
    if((_S840.primal_0) < 9.99999997475242708e-07f)
    {
        float _S844 = _S843.differential_0 * _S843.primal_0;
        float _S845 = 1.0f - _S843.primal_0 * _S843.primal_0 / 24.0f;
        float _S846 = ((0.0f - (_S844 + _S844) * 0.0416666679084301f) * _S820 - _S845 * 0.0f) / (_S820 * _S820);
        k_8 = _S845 / _S820;
        s_diff_k_3 = _S846;
    }
    else
    {
        float _S847 = _S843.differential_0 * 0.5f;
        DiffPair_float_0 _S848;
        (&_S848)->primal_0 = 0.5f * _S843.primal_0;
        (&_S848)->differential_0 = _S847;
        DiffPair_float_0 _S849 = _d_sin_0(&_S848);
        float _S850 = 2.0f * _S849.primal_0;
        float _S851 = (_S849.differential_0 * 2.0f * _S840.primal_0 - _S850 * _S840.differential_0) / (_S840.primal_0 * _S840.primal_0);
        k_8 = _S850 / _S840.primal_0;
        s_diff_k_3 = _S851;
    }
    float2  _S852 = _S816 * make_float2 (k_8);
    float2  _S853 = _S838 * make_float2 (k_8) + make_float2 (s_diff_k_3) * _S816;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S854;
    (&_S854)->primal_0 = _S852;
    (&_S854)->differential_0 = _S853;
    FixedArray<float, 4>  _S855 = dist_coeffs_24;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S856 = s_fwd_DistOpenCV_distort_0(&_S854, &_S855);
    float _S857 = _S856.differential_0.y * fy_6;
    *&(((&J_6)->rows + (int(0)))->y) = _S856.differential_0.x * fx_6;
    *&(((&J_6)->rows + (int(1)))->y) = _S857;
    float2  _S858 = make_float2 (0.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S859;
    (&_S859)->primal_0 = _S816;
    (&_S859)->differential_0 = _S858;
    DiffPair_float_0 _S860 = s_fwd_length_impl_0(&_S859);
    DiffPair_float_0 _S861;
    (&_S861)->primal_0 = _S860.primal_0;
    (&_S861)->differential_0 = _S860.differential_0;
    DiffPair_float_0 _S862;
    (&_S862)->primal_0 = _S820;
    (&_S862)->differential_0 = 1.0f;
    DiffPair_float_0 _S863 = _d_atan2_0(&_S861, &_S862);
    if((_S860.primal_0) < 9.99999997475242708e-07f)
    {
        float _S864 = _S863.differential_0 * _S863.primal_0;
        float _S865 = 1.0f - _S863.primal_0 * _S863.primal_0 / 24.0f;
        float _S866 = ((0.0f - (_S864 + _S864) * 0.0416666679084301f) * _S820 - _S865) / (_S820 * _S820);
        k_8 = _S865 / _S820;
        s_diff_k_3 = _S866;
    }
    else
    {
        float _S867 = _S863.differential_0 * 0.5f;
        DiffPair_float_0 _S868;
        (&_S868)->primal_0 = 0.5f * _S863.primal_0;
        (&_S868)->differential_0 = _S867;
        DiffPair_float_0 _S869 = _d_sin_0(&_S868);
        float _S870 = 2.0f * _S869.primal_0;
        float _S871 = (_S869.differential_0 * 2.0f * _S860.primal_0 - _S870 * _S860.differential_0) / (_S860.primal_0 * _S860.primal_0);
        k_8 = _S870 / _S860.primal_0;
        s_diff_k_3 = _S871;
    }
    float2  _S872 = _S816 * make_float2 (k_8);
    float2  _S873 = _S858 * make_float2 (k_8) + make_float2 (s_diff_k_3) * _S816;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S874;
    (&_S874)->primal_0 = _S872;
    (&_S874)->differential_0 = _S873;
    FixedArray<float, 4>  _S875 = dist_coeffs_24;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S876 = s_fwd_DistOpenCV_distort_0(&_S874, &_S875);
    float _S877 = _S876.differential_0.y * fy_6;
    *&(((&J_6)->rows + (int(0)))->z) = _S876.differential_0.x * fx_6;
    *&(((&J_6)->rows + (int(1)))->z) = _S877;
    return J_6;
}

inline __device__ float2  distort_point_opencv(float2  uv_21, int camera_model_8, FixedArray<float, 4>  dist_coeffs_25)
{
    float2  _S878;
    for(;;)
    {
        if(camera_model_8 == int(3))
        {
            _S878 = uv_21;
            break;
        }
        float k_9;
        if(camera_model_8 == int(1))
        {
            float r_68 = length_0(uv_21);
            float theta_6 = (F32_atan((r_68)));
            if(r_68 < 0.00100000004749745f)
            {
                k_9 = 1.0f - theta_6 * theta_6 / 6.0f;
            }
            else
            {
                k_9 = theta_6 / r_68;
            }
            _S878 = uv_21 * make_float2 (k_9);
        }
        else
        {
            if(camera_model_8 == int(2))
            {
                float r_69 = length_0(uv_21);
                float theta_7 = (F32_atan((r_69)));
                if(r_69 < 0.00100000004749745f)
                {
                    k_9 = 1.0f - theta_7 * theta_7 / 24.0f;
                }
                else
                {
                    k_9 = 2.0f * (F32_sin((0.5f * theta_7))) / r_69;
                }
                _S878 = uv_21 * make_float2 (k_9);
            }
            else
            {
                _S878 = uv_21;
            }
        }
        FixedArray<float, 4>  _S879 = dist_coeffs_25;
        float2  _S880 = DistOpenCV_distort_0(_S878, &_S879);
        _S878 = _S880;
        break;
    }
    return _S878;
}

inline __device__ bool undistort_point_opencv(float2  uv_22, int camera_model_9, FixedArray<float, 4>  dist_coeffs_26, float2  * uv_undist_4)
{
    bool _S881;
    for(;;)
    {
        *uv_undist_4 = make_float2 (0.0f);
        if(camera_model_9 == int(3))
        {
            float lon_2 = uv_22.x;
            float lat_27 = uv_22.y;
            float cl_27 = (F32_cos((lat_27)));
            *uv_undist_4 = make_float2 (cl_27 * (F32_sin((lon_2))), (F32_sin((lat_27)))) / make_float2 ((F32_max((cl_27 * (F32_cos((lon_2)))), (9.999999960041972e-13f))));
            _S881 = true;
            break;
        }
        FixedArray<float, 4>  _S882 = dist_coeffs_26;
        float2  uv_u_27;
        bool _S883 = undistort_point_1(uv_22, &_S882, int(8), &uv_u_27);
        if(!_S883)
        {
            _S881 = false;
            break;
        }
        float2  _S884 = uv_u_27;
        float3  raydir_3;
        if(camera_model_9 == int(1))
        {
            float r_70 = length_0(_S884);
            float s_6;
            if(r_70 < 0.00100000004749745f)
            {
                s_6 = 1.0f - r_70 * r_70 / 6.0f;
            }
            else
            {
                s_6 = (F32_sin((r_70))) / r_70;
            }
            raydir_3 = make_float3 ((_S884 * make_float2 (s_6)).x, (_S884 * make_float2 (s_6)).y, (F32_cos((r_70))));
        }
        else
        {
            if(camera_model_9 == int(2))
            {
                float r_71 = length_0(_S884);
                raydir_3 = make_float3 ((_S884 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_71 * r_71)))))))).x, (_S884 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_71 * r_71)))))))).y, 1.0f - 0.5f * r_71 * r_71);
            }
            else
            {
                raydir_3 = make_float3 (_S884.x, _S884.y, 1.0f);
            }
        }
        *uv_undist_4 = float2 {raydir_3.x, raydir_3.y} / make_float2 ((F32_max((raydir_3.z), (9.999999960041972e-13f))));
        _S881 = true;
        break;
    }
    return _S881;
}

inline __device__ bool unproject_point_opencv(float2  uv_23, int camera_model_10, FixedArray<float, 4>  dist_coeffs_27, float3  * raydir_4)
{
    bool _S885;
    for(;;)
    {
        if(camera_model_10 == int(4))
        {
            *raydir_4 = make_float3 (0.0f, 0.0f, 1.0f);
            _S885 = true;
            break;
        }
        int3  _S886 = make_int3 (int(0));
        float3  _S887 = make_float3 ((float)_S886.x, (float)_S886.y, (float)_S886.z);
        *raydir_4 = _S887;
        if(camera_model_10 == int(3))
        {
            float lon_3 = uv_23.x;
            float lat_28 = uv_23.y;
            float cl_28 = (F32_cos((lat_28)));
            *raydir_4 = make_float3 (cl_28 * (F32_sin((lon_3))), (F32_sin((lat_28))), cl_28 * (F32_cos((lon_3))));
            _S885 = true;
            break;
        }
        FixedArray<float, 4>  _S888 = dist_coeffs_27;
        float2  uv_u_28;
        bool _S889 = undistort_point_1(uv_23, &_S888, int(8), &uv_u_28);
        if(!_S889)
        {
            _S885 = false;
            break;
        }
        float2  _S890 = uv_u_28;
        if(camera_model_10 == int(1))
        {
            float r_72 = length_0(_S890);
            float s_7;
            if(r_72 < 0.00100000004749745f)
            {
                s_7 = 1.0f - r_72 * r_72 / 6.0f;
            }
            else
            {
                s_7 = (F32_sin((r_72))) / r_72;
            }
            *raydir_4 = make_float3 ((_S890 * make_float2 (s_7)).x, (_S890 * make_float2 (s_7)).y, (F32_cos((r_72))));
        }
        else
        {
            if(camera_model_10 == int(2))
            {
                float r_73 = length_0(_S890);
                *raydir_4 = make_float3 ((_S890 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_73 * r_73)))))))).x, (_S890 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_73 * r_73)))))))).y, 1.0f - 0.5f * r_73 * r_73);
            }
            else
            {
                *raydir_4 = make_float3 (_S890.x, _S890.y, 1.0f);
            }
        }
        _S885 = true;
        break;
    }
    return _S885;
}

inline __device__ bool generate_ray_opencv(float2  uv_24, int camera_model_11, FixedArray<float, 4>  dist_coeffs_28, float3  * raydir_5)
{
    bool _S891;
    for(;;)
    {
        if(camera_model_11 == int(4))
        {
            *raydir_5 = make_float3 (0.0f, 0.0f, 1.0f);
            _S891 = true;
            break;
        }
        if(camera_model_11 == int(3))
        {
            float _S892 = uv_24.x;
            if((F32_abs((_S892))) > 3.14159274101257324f)
            {
                _S891 = true;
            }
            else
            {
                _S891 = (F32_abs((uv_24.y))) > 1.57079637050628662f;
            }
            if(_S891)
            {
                int3  _S893 = make_int3 (int(0));
                float3  _S894 = make_float3 ((float)_S893.x, (float)_S893.y, (float)_S893.z);
                *raydir_5 = _S894;
                _S891 = false;
                break;
            }
            float lat_29 = uv_24.y;
            float cl_29 = (F32_cos((lat_29)));
            *raydir_5 = make_float3 (cl_29 * (F32_sin((_S892))), (F32_sin((lat_29))), cl_29 * (F32_cos((_S892))));
            _S891 = true;
            break;
        }
        FixedArray<float, 4>  _S895 = dist_coeffs_28;
        float2  uv_u_29;
        bool _S896 = undistort_point_1(uv_24, &_S895, int(8), &uv_u_29);
        if(!_S896)
        {
            int3  _S897 = make_int3 (int(0));
            float3  _S898 = make_float3 ((float)_S897.x, (float)_S897.y, (float)_S897.z);
            *raydir_5 = _S898;
            _S891 = false;
            break;
        }
        float2  _S899 = uv_u_29;
        if(camera_model_11 == int(1))
        {
            float r_74 = length_0(_S899);
            if(r_74 >= 3.14159274101257324f)
            {
                int3  _S900 = make_int3 (int(0));
                float3  _S901 = make_float3 ((float)_S900.x, (float)_S900.y, (float)_S900.z);
                *raydir_5 = _S901;
                _S891 = false;
                break;
            }
            float s_8;
            if(r_74 < 0.00100000004749745f)
            {
                s_8 = 1.0f - r_74 * r_74 / 6.0f;
            }
            else
            {
                s_8 = (F32_sin((r_74))) / r_74;
            }
            *raydir_5 = make_float3 ((_S899 * make_float2 (s_8)).x, (_S899 * make_float2 (s_8)).y, (F32_cos((r_74))));
        }
        else
        {
            if(camera_model_11 == int(2))
            {
                float r_75 = length_0(_S899);
                if(r_75 >= 2.0f)
                {
                    int3  _S902 = make_int3 (int(0));
                    float3  _S903 = make_float3 ((float)_S902.x, (float)_S902.y, (float)_S902.z);
                    *raydir_5 = _S903;
                    _S891 = false;
                    break;
                }
                *raydir_5 = make_float3 ((_S899 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_75 * r_75)))))))).x, (_S899 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_75 * r_75)))))))).y, 1.0f - 0.5f * r_75 * r_75);
            }
            else
            {
                *raydir_5 = make_float3 (_S899.x, _S899.y, 1.0f);
            }
        }
        *raydir_5 = normalize_0(*raydir_5);
        _S891 = true;
        break;
    }
    return _S891;
}

inline __device__ bool pixel_rect_quad_opencv(int camera_model_12, float4  intrins_18, FixedArray<float, 4>  dist_coeffs_29, float2  lo_4, float2  hi_4, float3  * q0_4, float3  * q1_4, float3  * q2_4, float3  * q3_4)
{
    FixedArray<float, 4>  _S904 = dist_coeffs_29;
    bool _S905 = pixel_rect_quad_1(camera_model_12, intrins_18, &_S904, lo_4, hi_4, q0_4, q1_4, q2_4, q3_4);
    return _S905;
}

inline __device__ bool is_valid_distortion_prism(float2  uv_25, FixedArray<float, 8>  dist_coeffs_30)
{
    float2  _S906 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S907;
    (&_S907)->primal_0 = uv_25;
    (&_S907)->differential_0 = _S906;
    FixedArray<float, 8>  _S908 = dist_coeffs_30;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S909 = s_fwd_DistThinPrism_distort_0(&_S907, &_S908);
    float2  _S910 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S911;
    (&_S911)->primal_0 = uv_25;
    (&_S911)->differential_0 = _S910;
    FixedArray<float, 8>  _S912 = dist_coeffs_30;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S913 = s_fwd_DistThinPrism_distort_0(&_S911, &_S912);
    Matrix<float, 2, 2>  _S914 = transpose_1(makeMatrix<float, 2, 2> (_S909.differential_0, _S913.differential_0));
    float _S915 = (F32_min((determinant_0(_S914)), ((F32_min((_S914.rows[int(0)].x), (_S914.rows[int(1)].y))))));
    bool _S916;
    if(_S915 > 0.25f)
    {
        _S916 = _S915 < 4.0f;
    }
    else
    {
        _S916 = false;
    }
    if(_S916)
    {
        FixedArray<float, 8>  _S917 = dist_coeffs_30;
        float2  _S918 = DistThinPrism_distort_0(uv_25, &_S917);
        _S916 = (dot_0(uv_25, _S918)) >= 0.0f;
    }
    else
    {
        _S916 = false;
    }
    return _S916;
}

inline __device__ bool persp_proj_nav_prism(float3  p_view_14, float4  intrins_19, FixedArray<float, 8>  dist_coeffs_31, float2  * uv_26)
{
    bool _S919;
    for(;;)
    {
        float2  _S920 = float2 {p_view_14.x, p_view_14.y};
        float _S921 = p_view_14.z;
        float2  uv0_2 = _S920 / make_float2 (_S921);
        if(_S921 < 0.0f)
        {
            _S919 = true;
        }
        else
        {
            float2  _S922 = make_float2 (1.0f, 0.0f);
            DiffPair_vectorx3Cfloatx2C2x3E_0 _S923;
            (&_S923)->primal_0 = uv0_2;
            (&_S923)->differential_0 = _S922;
            FixedArray<float, 8>  _S924 = dist_coeffs_31;
            DiffPair_vectorx3Cfloatx2C2x3E_0 _S925 = s_fwd_DistThinPrism_distort_0(&_S923, &_S924);
            float2  _S926 = make_float2 (0.0f, 1.0f);
            DiffPair_vectorx3Cfloatx2C2x3E_0 _S927;
            (&_S927)->primal_0 = uv0_2;
            (&_S927)->differential_0 = _S926;
            FixedArray<float, 8>  _S928 = dist_coeffs_31;
            DiffPair_vectorx3Cfloatx2C2x3E_0 _S929 = s_fwd_DistThinPrism_distort_0(&_S927, &_S928);
            Matrix<float, 2, 2>  _S930 = transpose_1(makeMatrix<float, 2, 2> (_S925.differential_0, _S929.differential_0));
            float _S931 = (F32_min((determinant_0(_S930)), ((F32_min((_S930.rows[int(0)].x), (_S930.rows[int(1)].y))))));
            if(_S931 > 0.25f)
            {
                _S919 = _S931 < 4.0f;
            }
            else
            {
                _S919 = false;
            }
            if(_S919)
            {
                FixedArray<float, 8>  _S932 = dist_coeffs_31;
                float2  _S933 = DistThinPrism_distort_0(uv0_2, &_S932);
                _S919 = (dot_0(uv0_2, _S933)) >= 0.0f;
            }
            else
            {
                _S919 = false;
            }
            _S919 = !_S919;
        }
        if(_S919)
        {
            *uv_26 = uv0_2;
            _S919 = false;
            break;
        }
        float2  uv_27 = _S920 / make_float2 (_S921);
        FixedArray<float, 8>  _S934 = dist_coeffs_31;
        float2  _S935 = DistThinPrism_distort_0(uv_27, &_S934);
        *uv_26 = make_float2 (intrins_19.x * _S935.x + intrins_19.z, intrins_19.y * _S935.y + intrins_19.w);
        _S919 = true;
        break;
    }
    return _S919;
}

inline __device__ bool fisheye_proj_nav_prism(float3  p_view_15, float4  intrins_20, FixedArray<float, 8>  dist_coeffs_32, float2  * uv_28)
{
    bool _S936;
    for(;;)
    {
        float2  _S937 = float2 {p_view_15.x, p_view_15.y};
        float r_76 = length_0(_S937);
        float _S938 = p_view_15.z;
        float theta_8 = (F32_atan2((r_76), (_S938)));
        bool _S939 = theta_8 < 0.00100000004749745f;
        float k_10;
        if(_S939)
        {
            k_10 = (1.0f - theta_8 * theta_8 / 3.0f) / _S938;
        }
        else
        {
            k_10 = theta_8 / r_76;
        }
        float2  _S940 = _S937 * make_float2 (k_10);
        float2  _S941 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S942;
        (&_S942)->primal_0 = _S940;
        (&_S942)->differential_0 = _S941;
        FixedArray<float, 8>  _S943 = dist_coeffs_32;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S944 = s_fwd_DistThinPrism_distort_0(&_S942, &_S943);
        float2  _S945 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S946;
        (&_S946)->primal_0 = _S940;
        (&_S946)->differential_0 = _S945;
        FixedArray<float, 8>  _S947 = dist_coeffs_32;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S948 = s_fwd_DistThinPrism_distort_0(&_S946, &_S947);
        Matrix<float, 2, 2>  _S949 = transpose_1(makeMatrix<float, 2, 2> (_S944.differential_0, _S948.differential_0));
        float _S950 = (F32_min((determinant_0(_S949)), ((F32_min((_S949.rows[int(0)].x), (_S949.rows[int(1)].y))))));
        if(_S950 > 0.25f)
        {
            _S936 = _S950 < 4.0f;
        }
        else
        {
            _S936 = false;
        }
        if(_S936)
        {
            FixedArray<float, 8>  _S951 = dist_coeffs_32;
            float2  _S952 = DistThinPrism_distort_0(_S940, &_S951);
            _S936 = (dot_0(_S940, _S952)) >= 0.0f;
        }
        else
        {
            _S936 = false;
        }
        if(!_S936)
        {
            *uv_28 = _S940;
            _S936 = false;
            break;
        }
        if(_S939)
        {
            k_10 = (1.0f - theta_8 * theta_8 / 3.0f) / _S938;
        }
        else
        {
            k_10 = theta_8 / r_76;
        }
        float2  _S953 = _S937 * make_float2 (k_10);
        FixedArray<float, 8>  _S954 = dist_coeffs_32;
        float2  _S955 = DistThinPrism_distort_0(_S953, &_S954);
        *uv_28 = make_float2 (intrins_20.x * _S955.x + intrins_20.z, intrins_20.y * _S955.y + intrins_20.w);
        _S936 = true;
        break;
    }
    return _S936;
}

inline __device__ bool equisolid_proj_nav_prism(float3  p_view_16, float4  intrins_21, FixedArray<float, 8>  dist_coeffs_33, float2  * uv_29)
{
    bool _S956;
    for(;;)
    {
        float2  _S957 = float2 {p_view_16.x, p_view_16.y};
        float r_77 = length_0(_S957);
        float _S958 = p_view_16.z;
        float theta_9 = (F32_atan2((r_77), (_S958)));
        bool _S959 = r_77 < 9.99999997475242708e-07f;
        float k_11;
        if(_S959)
        {
            k_11 = (1.0f - theta_9 * theta_9 / 24.0f) / _S958;
        }
        else
        {
            k_11 = 2.0f * (F32_sin((0.5f * theta_9))) / r_77;
        }
        float2  _S960 = _S957 * make_float2 (k_11);
        float2  _S961 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S962;
        (&_S962)->primal_0 = _S960;
        (&_S962)->differential_0 = _S961;
        FixedArray<float, 8>  _S963 = dist_coeffs_33;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S964 = s_fwd_DistThinPrism_distort_0(&_S962, &_S963);
        float2  _S965 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S966;
        (&_S966)->primal_0 = _S960;
        (&_S966)->differential_0 = _S965;
        FixedArray<float, 8>  _S967 = dist_coeffs_33;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S968 = s_fwd_DistThinPrism_distort_0(&_S966, &_S967);
        Matrix<float, 2, 2>  _S969 = transpose_1(makeMatrix<float, 2, 2> (_S964.differential_0, _S968.differential_0));
        float _S970 = (F32_min((determinant_0(_S969)), ((F32_min((_S969.rows[int(0)].x), (_S969.rows[int(1)].y))))));
        if(_S970 > 0.25f)
        {
            _S956 = _S970 < 4.0f;
        }
        else
        {
            _S956 = false;
        }
        if(_S956)
        {
            FixedArray<float, 8>  _S971 = dist_coeffs_33;
            float2  _S972 = DistThinPrism_distort_0(_S960, &_S971);
            _S956 = (dot_0(_S960, _S972)) >= 0.0f;
        }
        else
        {
            _S956 = false;
        }
        if(!_S956)
        {
            *uv_29 = _S960;
            _S956 = false;
            break;
        }
        if(_S959)
        {
            k_11 = (1.0f - theta_9 * theta_9 / 24.0f) / _S958;
        }
        else
        {
            k_11 = 2.0f * (F32_sin((0.5f * theta_9))) / r_77;
        }
        float2  _S973 = _S957 * make_float2 (k_11);
        FixedArray<float, 8>  _S974 = dist_coeffs_33;
        float2  _S975 = DistThinPrism_distort_0(_S973, &_S974);
        *uv_29 = make_float2 (intrins_21.x * _S975.x + intrins_21.z, intrins_21.y * _S975.y + intrins_21.w);
        _S956 = true;
        break;
    }
    return _S956;
}

inline __device__ Matrix<float, 2, 3>  persp_proj_jac_prism(float3  p_view_17, float4  intrins_22, FixedArray<float, 8>  dist_coeffs_34)
{
    float2  _S976 = float2 {p_view_17.x, p_view_17.y};
    float _S977 = p_view_17.z;
    float2  _S978 = _S976 * make_float2 (0.0f);
    float _S979 = _S977 * _S977;
    float2  s_diff_uv_6 = (make_float2 (1.0f, 0.0f) * make_float2 (_S977) - _S978) / make_float2 (_S979);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S980;
    (&_S980)->primal_0 = _S976 / make_float2 (_S977);
    (&_S980)->differential_0 = s_diff_uv_6;
    FixedArray<float, 8>  _S981 = dist_coeffs_34;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S982 = s_fwd_DistThinPrism_distort_0(&_S980, &_S981);
    float fx_7 = intrins_22.x;
    float fy_7 = intrins_22.y;
    float _S983 = _S982.differential_0.y * fy_7;
    Matrix<float, 2, 3>  J_7;
    *&(((&J_7)->rows + (int(0)))->x) = _S982.differential_0.x * fx_7;
    *&(((&J_7)->rows + (int(1)))->x) = _S983;
    float2  s_diff_uv_7 = (make_float2 (0.0f, 1.0f) * make_float2 (_S977) - _S978) / make_float2 (_S979);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S984;
    (&_S984)->primal_0 = _S976 / make_float2 (_S977);
    (&_S984)->differential_0 = s_diff_uv_7;
    FixedArray<float, 8>  _S985 = dist_coeffs_34;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S986 = s_fwd_DistThinPrism_distort_0(&_S984, &_S985);
    float _S987 = _S986.differential_0.y * fy_7;
    *&(((&J_7)->rows + (int(0)))->y) = _S986.differential_0.x * fx_7;
    *&(((&J_7)->rows + (int(1)))->y) = _S987;
    float2  s_diff_uv_8 = (make_float2 (0.0f, 0.0f) * make_float2 (_S977) - _S976) / make_float2 (_S979);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S988;
    (&_S988)->primal_0 = _S976 / make_float2 (_S977);
    (&_S988)->differential_0 = s_diff_uv_8;
    FixedArray<float, 8>  _S989 = dist_coeffs_34;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S990 = s_fwd_DistThinPrism_distort_0(&_S988, &_S989);
    float _S991 = _S990.differential_0.y * fy_7;
    *&(((&J_7)->rows + (int(0)))->z) = _S990.differential_0.x * fx_7;
    *&(((&J_7)->rows + (int(1)))->z) = _S991;
    return J_7;
}

inline __device__ Matrix<float, 2, 3>  fisheye_proj_jac_prism(float3  p_view_18, float4  intrins_23, FixedArray<float, 8>  dist_coeffs_35)
{
    Matrix<float, 2, 3>  J_8;
    float2  _S992 = float2 {p_view_18.x, p_view_18.y};
    float2  _S993 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S994;
    (&_S994)->primal_0 = _S992;
    (&_S994)->differential_0 = _S993;
    DiffPair_float_0 _S995 = s_fwd_length_impl_0(&_S994);
    float _S996 = p_view_18.z;
    DiffPair_float_0 _S997;
    (&_S997)->primal_0 = _S995.primal_0;
    (&_S997)->differential_0 = _S995.differential_0;
    DiffPair_float_0 _S998;
    (&_S998)->primal_0 = _S996;
    (&_S998)->differential_0 = 0.0f;
    DiffPair_float_0 _S999 = _d_atan2_0(&_S997, &_S998);
    float k_12;
    float s_diff_k_4;
    if((_S999.primal_0) < 0.00100000004749745f)
    {
        float _S1000 = _S999.differential_0 * _S999.primal_0;
        float _S1001 = 1.0f - _S999.primal_0 * _S999.primal_0 / 3.0f;
        float _S1002 = ((0.0f - (_S1000 + _S1000) * 0.3333333432674408f) * _S996 - _S1001 * 0.0f) / (_S996 * _S996);
        k_12 = _S1001 / _S996;
        s_diff_k_4 = _S1002;
    }
    else
    {
        float _S1003 = (_S999.differential_0 * _S995.primal_0 - _S999.primal_0 * _S995.differential_0) / (_S995.primal_0 * _S995.primal_0);
        k_12 = _S999.primal_0 / _S995.primal_0;
        s_diff_k_4 = _S1003;
    }
    float2  _S1004 = _S992 * make_float2 (k_12);
    float2  _S1005 = _S993 * make_float2 (k_12) + make_float2 (s_diff_k_4) * _S992;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1006;
    (&_S1006)->primal_0 = _S1004;
    (&_S1006)->differential_0 = _S1005;
    FixedArray<float, 8>  _S1007 = dist_coeffs_35;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1008 = s_fwd_DistThinPrism_distort_0(&_S1006, &_S1007);
    float fx_8 = intrins_23.x;
    float fy_8 = intrins_23.y;
    float _S1009 = _S1008.differential_0.y * fy_8;
    *&(((&J_8)->rows + (int(0)))->x) = _S1008.differential_0.x * fx_8;
    *&(((&J_8)->rows + (int(1)))->x) = _S1009;
    float2  _S1010 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1011;
    (&_S1011)->primal_0 = _S992;
    (&_S1011)->differential_0 = _S1010;
    DiffPair_float_0 _S1012 = s_fwd_length_impl_0(&_S1011);
    DiffPair_float_0 _S1013;
    (&_S1013)->primal_0 = _S1012.primal_0;
    (&_S1013)->differential_0 = _S1012.differential_0;
    DiffPair_float_0 _S1014;
    (&_S1014)->primal_0 = _S996;
    (&_S1014)->differential_0 = 0.0f;
    DiffPair_float_0 _S1015 = _d_atan2_0(&_S1013, &_S1014);
    if((_S1015.primal_0) < 0.00100000004749745f)
    {
        float _S1016 = _S1015.differential_0 * _S1015.primal_0;
        float _S1017 = 1.0f - _S1015.primal_0 * _S1015.primal_0 / 3.0f;
        float _S1018 = ((0.0f - (_S1016 + _S1016) * 0.3333333432674408f) * _S996 - _S1017 * 0.0f) / (_S996 * _S996);
        k_12 = _S1017 / _S996;
        s_diff_k_4 = _S1018;
    }
    else
    {
        float _S1019 = (_S1015.differential_0 * _S1012.primal_0 - _S1015.primal_0 * _S1012.differential_0) / (_S1012.primal_0 * _S1012.primal_0);
        k_12 = _S1015.primal_0 / _S1012.primal_0;
        s_diff_k_4 = _S1019;
    }
    float2  _S1020 = _S992 * make_float2 (k_12);
    float2  _S1021 = _S1010 * make_float2 (k_12) + make_float2 (s_diff_k_4) * _S992;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1022;
    (&_S1022)->primal_0 = _S1020;
    (&_S1022)->differential_0 = _S1021;
    FixedArray<float, 8>  _S1023 = dist_coeffs_35;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1024 = s_fwd_DistThinPrism_distort_0(&_S1022, &_S1023);
    float _S1025 = _S1024.differential_0.y * fy_8;
    *&(((&J_8)->rows + (int(0)))->y) = _S1024.differential_0.x * fx_8;
    *&(((&J_8)->rows + (int(1)))->y) = _S1025;
    float2  _S1026 = make_float2 (0.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1027;
    (&_S1027)->primal_0 = _S992;
    (&_S1027)->differential_0 = _S1026;
    DiffPair_float_0 _S1028 = s_fwd_length_impl_0(&_S1027);
    DiffPair_float_0 _S1029;
    (&_S1029)->primal_0 = _S1028.primal_0;
    (&_S1029)->differential_0 = _S1028.differential_0;
    DiffPair_float_0 _S1030;
    (&_S1030)->primal_0 = _S996;
    (&_S1030)->differential_0 = 1.0f;
    DiffPair_float_0 _S1031 = _d_atan2_0(&_S1029, &_S1030);
    if((_S1031.primal_0) < 0.00100000004749745f)
    {
        float _S1032 = _S1031.differential_0 * _S1031.primal_0;
        float _S1033 = 1.0f - _S1031.primal_0 * _S1031.primal_0 / 3.0f;
        float _S1034 = ((0.0f - (_S1032 + _S1032) * 0.3333333432674408f) * _S996 - _S1033) / (_S996 * _S996);
        k_12 = _S1033 / _S996;
        s_diff_k_4 = _S1034;
    }
    else
    {
        float _S1035 = (_S1031.differential_0 * _S1028.primal_0 - _S1031.primal_0 * _S1028.differential_0) / (_S1028.primal_0 * _S1028.primal_0);
        k_12 = _S1031.primal_0 / _S1028.primal_0;
        s_diff_k_4 = _S1035;
    }
    float2  _S1036 = _S992 * make_float2 (k_12);
    float2  _S1037 = _S1026 * make_float2 (k_12) + make_float2 (s_diff_k_4) * _S992;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1038;
    (&_S1038)->primal_0 = _S1036;
    (&_S1038)->differential_0 = _S1037;
    FixedArray<float, 8>  _S1039 = dist_coeffs_35;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1040 = s_fwd_DistThinPrism_distort_0(&_S1038, &_S1039);
    float _S1041 = _S1040.differential_0.y * fy_8;
    *&(((&J_8)->rows + (int(0)))->z) = _S1040.differential_0.x * fx_8;
    *&(((&J_8)->rows + (int(1)))->z) = _S1041;
    return J_8;
}

inline __device__ Matrix<float, 2, 3>  equisolid_proj_jac_prism(float3  p_view_19, float4  intrins_24, FixedArray<float, 8>  dist_coeffs_36)
{
    Matrix<float, 2, 3>  J_9;
    float2  _S1042 = float2 {p_view_19.x, p_view_19.y};
    float2  _S1043 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1044;
    (&_S1044)->primal_0 = _S1042;
    (&_S1044)->differential_0 = _S1043;
    DiffPair_float_0 _S1045 = s_fwd_length_impl_0(&_S1044);
    float _S1046 = p_view_19.z;
    DiffPair_float_0 _S1047;
    (&_S1047)->primal_0 = _S1045.primal_0;
    (&_S1047)->differential_0 = _S1045.differential_0;
    DiffPair_float_0 _S1048;
    (&_S1048)->primal_0 = _S1046;
    (&_S1048)->differential_0 = 0.0f;
    DiffPair_float_0 _S1049 = _d_atan2_0(&_S1047, &_S1048);
    float k_13;
    float s_diff_k_5;
    if((_S1045.primal_0) < 9.99999997475242708e-07f)
    {
        float _S1050 = _S1049.differential_0 * _S1049.primal_0;
        float _S1051 = 1.0f - _S1049.primal_0 * _S1049.primal_0 / 24.0f;
        float _S1052 = ((0.0f - (_S1050 + _S1050) * 0.0416666679084301f) * _S1046 - _S1051 * 0.0f) / (_S1046 * _S1046);
        k_13 = _S1051 / _S1046;
        s_diff_k_5 = _S1052;
    }
    else
    {
        float _S1053 = _S1049.differential_0 * 0.5f;
        DiffPair_float_0 _S1054;
        (&_S1054)->primal_0 = 0.5f * _S1049.primal_0;
        (&_S1054)->differential_0 = _S1053;
        DiffPair_float_0 _S1055 = _d_sin_0(&_S1054);
        float _S1056 = 2.0f * _S1055.primal_0;
        float _S1057 = (_S1055.differential_0 * 2.0f * _S1045.primal_0 - _S1056 * _S1045.differential_0) / (_S1045.primal_0 * _S1045.primal_0);
        k_13 = _S1056 / _S1045.primal_0;
        s_diff_k_5 = _S1057;
    }
    float2  _S1058 = _S1042 * make_float2 (k_13);
    float2  _S1059 = _S1043 * make_float2 (k_13) + make_float2 (s_diff_k_5) * _S1042;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1060;
    (&_S1060)->primal_0 = _S1058;
    (&_S1060)->differential_0 = _S1059;
    FixedArray<float, 8>  _S1061 = dist_coeffs_36;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1062 = s_fwd_DistThinPrism_distort_0(&_S1060, &_S1061);
    float fx_9 = intrins_24.x;
    float fy_9 = intrins_24.y;
    float _S1063 = _S1062.differential_0.y * fy_9;
    *&(((&J_9)->rows + (int(0)))->x) = _S1062.differential_0.x * fx_9;
    *&(((&J_9)->rows + (int(1)))->x) = _S1063;
    float2  _S1064 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1065;
    (&_S1065)->primal_0 = _S1042;
    (&_S1065)->differential_0 = _S1064;
    DiffPair_float_0 _S1066 = s_fwd_length_impl_0(&_S1065);
    DiffPair_float_0 _S1067;
    (&_S1067)->primal_0 = _S1066.primal_0;
    (&_S1067)->differential_0 = _S1066.differential_0;
    DiffPair_float_0 _S1068;
    (&_S1068)->primal_0 = _S1046;
    (&_S1068)->differential_0 = 0.0f;
    DiffPair_float_0 _S1069 = _d_atan2_0(&_S1067, &_S1068);
    if((_S1066.primal_0) < 9.99999997475242708e-07f)
    {
        float _S1070 = _S1069.differential_0 * _S1069.primal_0;
        float _S1071 = 1.0f - _S1069.primal_0 * _S1069.primal_0 / 24.0f;
        float _S1072 = ((0.0f - (_S1070 + _S1070) * 0.0416666679084301f) * _S1046 - _S1071 * 0.0f) / (_S1046 * _S1046);
        k_13 = _S1071 / _S1046;
        s_diff_k_5 = _S1072;
    }
    else
    {
        float _S1073 = _S1069.differential_0 * 0.5f;
        DiffPair_float_0 _S1074;
        (&_S1074)->primal_0 = 0.5f * _S1069.primal_0;
        (&_S1074)->differential_0 = _S1073;
        DiffPair_float_0 _S1075 = _d_sin_0(&_S1074);
        float _S1076 = 2.0f * _S1075.primal_0;
        float _S1077 = (_S1075.differential_0 * 2.0f * _S1066.primal_0 - _S1076 * _S1066.differential_0) / (_S1066.primal_0 * _S1066.primal_0);
        k_13 = _S1076 / _S1066.primal_0;
        s_diff_k_5 = _S1077;
    }
    float2  _S1078 = _S1042 * make_float2 (k_13);
    float2  _S1079 = _S1064 * make_float2 (k_13) + make_float2 (s_diff_k_5) * _S1042;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1080;
    (&_S1080)->primal_0 = _S1078;
    (&_S1080)->differential_0 = _S1079;
    FixedArray<float, 8>  _S1081 = dist_coeffs_36;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1082 = s_fwd_DistThinPrism_distort_0(&_S1080, &_S1081);
    float _S1083 = _S1082.differential_0.y * fy_9;
    *&(((&J_9)->rows + (int(0)))->y) = _S1082.differential_0.x * fx_9;
    *&(((&J_9)->rows + (int(1)))->y) = _S1083;
    float2  _S1084 = make_float2 (0.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1085;
    (&_S1085)->primal_0 = _S1042;
    (&_S1085)->differential_0 = _S1084;
    DiffPair_float_0 _S1086 = s_fwd_length_impl_0(&_S1085);
    DiffPair_float_0 _S1087;
    (&_S1087)->primal_0 = _S1086.primal_0;
    (&_S1087)->differential_0 = _S1086.differential_0;
    DiffPair_float_0 _S1088;
    (&_S1088)->primal_0 = _S1046;
    (&_S1088)->differential_0 = 1.0f;
    DiffPair_float_0 _S1089 = _d_atan2_0(&_S1087, &_S1088);
    if((_S1086.primal_0) < 9.99999997475242708e-07f)
    {
        float _S1090 = _S1089.differential_0 * _S1089.primal_0;
        float _S1091 = 1.0f - _S1089.primal_0 * _S1089.primal_0 / 24.0f;
        float _S1092 = ((0.0f - (_S1090 + _S1090) * 0.0416666679084301f) * _S1046 - _S1091) / (_S1046 * _S1046);
        k_13 = _S1091 / _S1046;
        s_diff_k_5 = _S1092;
    }
    else
    {
        float _S1093 = _S1089.differential_0 * 0.5f;
        DiffPair_float_0 _S1094;
        (&_S1094)->primal_0 = 0.5f * _S1089.primal_0;
        (&_S1094)->differential_0 = _S1093;
        DiffPair_float_0 _S1095 = _d_sin_0(&_S1094);
        float _S1096 = 2.0f * _S1095.primal_0;
        float _S1097 = (_S1095.differential_0 * 2.0f * _S1086.primal_0 - _S1096 * _S1086.differential_0) / (_S1086.primal_0 * _S1086.primal_0);
        k_13 = _S1096 / _S1086.primal_0;
        s_diff_k_5 = _S1097;
    }
    float2  _S1098 = _S1042 * make_float2 (k_13);
    float2  _S1099 = _S1084 * make_float2 (k_13) + make_float2 (s_diff_k_5) * _S1042;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1100;
    (&_S1100)->primal_0 = _S1098;
    (&_S1100)->differential_0 = _S1099;
    FixedArray<float, 8>  _S1101 = dist_coeffs_36;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S1102 = s_fwd_DistThinPrism_distort_0(&_S1100, &_S1101);
    float _S1103 = _S1102.differential_0.y * fy_9;
    *&(((&J_9)->rows + (int(0)))->z) = _S1102.differential_0.x * fx_9;
    *&(((&J_9)->rows + (int(1)))->z) = _S1103;
    return J_9;
}

inline __device__ float2  distort_point_prism(float2  uv_30, int camera_model_13, FixedArray<float, 8>  dist_coeffs_37)
{
    float2  _S1104;
    for(;;)
    {
        if(camera_model_13 == int(3))
        {
            _S1104 = uv_30;
            break;
        }
        float k_14;
        if(camera_model_13 == int(1))
        {
            float r_78 = length_0(uv_30);
            float theta_10 = (F32_atan((r_78)));
            if(r_78 < 0.00100000004749745f)
            {
                k_14 = 1.0f - theta_10 * theta_10 / 6.0f;
            }
            else
            {
                k_14 = theta_10 / r_78;
            }
            _S1104 = uv_30 * make_float2 (k_14);
        }
        else
        {
            if(camera_model_13 == int(2))
            {
                float r_79 = length_0(uv_30);
                float theta_11 = (F32_atan((r_79)));
                if(r_79 < 0.00100000004749745f)
                {
                    k_14 = 1.0f - theta_11 * theta_11 / 24.0f;
                }
                else
                {
                    k_14 = 2.0f * (F32_sin((0.5f * theta_11))) / r_79;
                }
                _S1104 = uv_30 * make_float2 (k_14);
            }
            else
            {
                _S1104 = uv_30;
            }
        }
        FixedArray<float, 8>  _S1105 = dist_coeffs_37;
        float2  _S1106 = DistThinPrism_distort_0(_S1104, &_S1105);
        _S1104 = _S1106;
        break;
    }
    return _S1104;
}

inline __device__ bool undistort_point_prism(float2  uv_31, int camera_model_14, FixedArray<float, 8>  dist_coeffs_38, float2  * uv_undist_5)
{
    bool _S1107;
    for(;;)
    {
        *uv_undist_5 = make_float2 (0.0f);
        if(camera_model_14 == int(3))
        {
            float lon_4 = uv_31.x;
            float lat_30 = uv_31.y;
            float cl_30 = (F32_cos((lat_30)));
            *uv_undist_5 = make_float2 (cl_30 * (F32_sin((lon_4))), (F32_sin((lat_30)))) / make_float2 ((F32_max((cl_30 * (F32_cos((lon_4)))), (9.999999960041972e-13f))));
            _S1107 = true;
            break;
        }
        FixedArray<float, 8>  _S1108 = dist_coeffs_38;
        float2  uv_u_30;
        bool _S1109 = undistort_point_2(uv_31, &_S1108, int(8), &uv_u_30);
        if(!_S1109)
        {
            _S1107 = false;
            break;
        }
        float2  _S1110 = uv_u_30;
        float3  raydir_6;
        if(camera_model_14 == int(1))
        {
            float r_80 = length_0(_S1110);
            float s_9;
            if(r_80 < 0.00100000004749745f)
            {
                s_9 = 1.0f - r_80 * r_80 / 6.0f;
            }
            else
            {
                s_9 = (F32_sin((r_80))) / r_80;
            }
            raydir_6 = make_float3 ((_S1110 * make_float2 (s_9)).x, (_S1110 * make_float2 (s_9)).y, (F32_cos((r_80))));
        }
        else
        {
            if(camera_model_14 == int(2))
            {
                float r_81 = length_0(_S1110);
                raydir_6 = make_float3 ((_S1110 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_81 * r_81)))))))).x, (_S1110 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_81 * r_81)))))))).y, 1.0f - 0.5f * r_81 * r_81);
            }
            else
            {
                raydir_6 = make_float3 (_S1110.x, _S1110.y, 1.0f);
            }
        }
        *uv_undist_5 = float2 {raydir_6.x, raydir_6.y} / make_float2 ((F32_max((raydir_6.z), (9.999999960041972e-13f))));
        _S1107 = true;
        break;
    }
    return _S1107;
}

inline __device__ bool unproject_point_prism(float2  uv_32, int camera_model_15, FixedArray<float, 8>  dist_coeffs_39, float3  * raydir_7)
{
    bool _S1111;
    for(;;)
    {
        if(camera_model_15 == int(4))
        {
            *raydir_7 = make_float3 (0.0f, 0.0f, 1.0f);
            _S1111 = true;
            break;
        }
        int3  _S1112 = make_int3 (int(0));
        float3  _S1113 = make_float3 ((float)_S1112.x, (float)_S1112.y, (float)_S1112.z);
        *raydir_7 = _S1113;
        if(camera_model_15 == int(3))
        {
            float lon_5 = uv_32.x;
            float lat_31 = uv_32.y;
            float cl_31 = (F32_cos((lat_31)));
            *raydir_7 = make_float3 (cl_31 * (F32_sin((lon_5))), (F32_sin((lat_31))), cl_31 * (F32_cos((lon_5))));
            _S1111 = true;
            break;
        }
        FixedArray<float, 8>  _S1114 = dist_coeffs_39;
        float2  uv_u_31;
        bool _S1115 = undistort_point_2(uv_32, &_S1114, int(8), &uv_u_31);
        if(!_S1115)
        {
            _S1111 = false;
            break;
        }
        float2  _S1116 = uv_u_31;
        if(camera_model_15 == int(1))
        {
            float r_82 = length_0(_S1116);
            float s_10;
            if(r_82 < 0.00100000004749745f)
            {
                s_10 = 1.0f - r_82 * r_82 / 6.0f;
            }
            else
            {
                s_10 = (F32_sin((r_82))) / r_82;
            }
            *raydir_7 = make_float3 ((_S1116 * make_float2 (s_10)).x, (_S1116 * make_float2 (s_10)).y, (F32_cos((r_82))));
        }
        else
        {
            if(camera_model_15 == int(2))
            {
                float r_83 = length_0(_S1116);
                *raydir_7 = make_float3 ((_S1116 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_83 * r_83)))))))).x, (_S1116 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_83 * r_83)))))))).y, 1.0f - 0.5f * r_83 * r_83);
            }
            else
            {
                *raydir_7 = make_float3 (_S1116.x, _S1116.y, 1.0f);
            }
        }
        _S1111 = true;
        break;
    }
    return _S1111;
}

inline __device__ bool generate_ray_prism(float2  uv_33, int camera_model_16, FixedArray<float, 8>  dist_coeffs_40, float3  * raydir_8)
{
    bool _S1117;
    for(;;)
    {
        if(camera_model_16 == int(4))
        {
            *raydir_8 = make_float3 (0.0f, 0.0f, 1.0f);
            _S1117 = true;
            break;
        }
        if(camera_model_16 == int(3))
        {
            float _S1118 = uv_33.x;
            if((F32_abs((_S1118))) > 3.14159274101257324f)
            {
                _S1117 = true;
            }
            else
            {
                _S1117 = (F32_abs((uv_33.y))) > 1.57079637050628662f;
            }
            if(_S1117)
            {
                int3  _S1119 = make_int3 (int(0));
                float3  _S1120 = make_float3 ((float)_S1119.x, (float)_S1119.y, (float)_S1119.z);
                *raydir_8 = _S1120;
                _S1117 = false;
                break;
            }
            float lat_32 = uv_33.y;
            float cl_32 = (F32_cos((lat_32)));
            *raydir_8 = make_float3 (cl_32 * (F32_sin((_S1118))), (F32_sin((lat_32))), cl_32 * (F32_cos((_S1118))));
            _S1117 = true;
            break;
        }
        FixedArray<float, 8>  _S1121 = dist_coeffs_40;
        float2  uv_u_32;
        bool _S1122 = undistort_point_2(uv_33, &_S1121, int(8), &uv_u_32);
        if(!_S1122)
        {
            int3  _S1123 = make_int3 (int(0));
            float3  _S1124 = make_float3 ((float)_S1123.x, (float)_S1123.y, (float)_S1123.z);
            *raydir_8 = _S1124;
            _S1117 = false;
            break;
        }
        float2  _S1125 = uv_u_32;
        if(camera_model_16 == int(1))
        {
            float r_84 = length_0(_S1125);
            if(r_84 >= 3.14159274101257324f)
            {
                int3  _S1126 = make_int3 (int(0));
                float3  _S1127 = make_float3 ((float)_S1126.x, (float)_S1126.y, (float)_S1126.z);
                *raydir_8 = _S1127;
                _S1117 = false;
                break;
            }
            float s_11;
            if(r_84 < 0.00100000004749745f)
            {
                s_11 = 1.0f - r_84 * r_84 / 6.0f;
            }
            else
            {
                s_11 = (F32_sin((r_84))) / r_84;
            }
            *raydir_8 = make_float3 ((_S1125 * make_float2 (s_11)).x, (_S1125 * make_float2 (s_11)).y, (F32_cos((r_84))));
        }
        else
        {
            if(camera_model_16 == int(2))
            {
                float r_85 = length_0(_S1125);
                if(r_85 >= 2.0f)
                {
                    int3  _S1128 = make_int3 (int(0));
                    float3  _S1129 = make_float3 ((float)_S1128.x, (float)_S1128.y, (float)_S1128.z);
                    *raydir_8 = _S1129;
                    _S1117 = false;
                    break;
                }
                *raydir_8 = make_float3 ((_S1125 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_85 * r_85)))))))).x, (_S1125 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_85 * r_85)))))))).y, 1.0f - 0.5f * r_85 * r_85);
            }
            else
            {
                *raydir_8 = make_float3 (_S1125.x, _S1125.y, 1.0f);
            }
        }
        *raydir_8 = normalize_0(*raydir_8);
        _S1117 = true;
        break;
    }
    return _S1117;
}

inline __device__ bool pixel_rect_quad_prism(int camera_model_17, float4  intrins_25, FixedArray<float, 8>  dist_coeffs_41, float2  lo_5, float2  hi_5, float3  * q0_5, float3  * q1_5, float3  * q2_5, float3  * q3_5)
{
    FixedArray<float, 8>  _S1130 = dist_coeffs_41;
    bool _S1131 = pixel_rect_quad_2(camera_model_17, intrins_25, &_S1130, lo_5, hi_5, q0_5, q1_5, q2_5, q3_5);
    return _S1131;
}

inline __device__ void _d_mul_1(DiffPair_vectorx3Cfloatx2C3x3E_0 * left_5, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * right_5, float3  dOut_6)
{
    float _S1132 = (*right_5).primal_0.rows[int(0)].x * dOut_6.x;
    Matrix<float, 3, 3>  right_d_result_2;
    *&(((&right_d_result_2)->rows + (int(0)))->x) = (*left_5).primal_0.x * dOut_6.x;
    float sum_10 = _S1132 + (*right_5).primal_0.rows[int(0)].y * dOut_6.y;
    *&(((&right_d_result_2)->rows + (int(0)))->y) = (*left_5).primal_0.x * dOut_6.y;
    float sum_11 = sum_10 + (*right_5).primal_0.rows[int(0)].z * dOut_6.z;
    *&(((&right_d_result_2)->rows + (int(0)))->z) = (*left_5).primal_0.x * dOut_6.z;
    float3  left_d_result_2;
    *&((&left_d_result_2)->x) = sum_11;
    float _S1133 = (*right_5).primal_0.rows[int(1)].x * dOut_6.x;
    *&(((&right_d_result_2)->rows + (int(1)))->x) = (*left_5).primal_0.y * dOut_6.x;
    float sum_12 = _S1133 + (*right_5).primal_0.rows[int(1)].y * dOut_6.y;
    *&(((&right_d_result_2)->rows + (int(1)))->y) = (*left_5).primal_0.y * dOut_6.y;
    float sum_13 = sum_12 + (*right_5).primal_0.rows[int(1)].z * dOut_6.z;
    *&(((&right_d_result_2)->rows + (int(1)))->z) = (*left_5).primal_0.y * dOut_6.z;
    *&((&left_d_result_2)->y) = sum_13;
    float _S1134 = (*right_5).primal_0.rows[int(2)].x * dOut_6.x;
    *&(((&right_d_result_2)->rows + (int(2)))->x) = (*left_5).primal_0.z * dOut_6.x;
    float sum_14 = _S1134 + (*right_5).primal_0.rows[int(2)].y * dOut_6.y;
    *&(((&right_d_result_2)->rows + (int(2)))->y) = (*left_5).primal_0.z * dOut_6.y;
    float sum_15 = sum_14 + (*right_5).primal_0.rows[int(2)].z * dOut_6.z;
    *&(((&right_d_result_2)->rows + (int(2)))->z) = (*left_5).primal_0.z * dOut_6.z;
    *&((&left_d_result_2)->z) = sum_15;
    left_5->primal_0 = (*left_5).primal_0;
    left_5->differential_0 = left_d_result_2;
    right_5->primal_0 = (*right_5).primal_0;
    right_5->differential_0 = right_d_result_2;
    return;
}

inline __device__ float3  mul_3(float3  left_6, Matrix<float, 3, 3>  right_6)
{
    float3  result_12;
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
        int i_7 = int(0);
        float sum_16 = 0.0f;
        for(;;)
        {
            if(i_7 < int(3))
            {
            }
            else
            {
                break;
            }
            float sum_17 = sum_16 + _slang_vector_get_element(left_6, i_7) * _slang_vector_get_element(right_6.rows[i_7], j_1);
            i_7 = i_7 + int(1);
            sum_16 = sum_17;
        }
        *_slang_vector_get_element_ptr(&result_12, j_1) = sum_16;
        j_1 = j_1 + int(1);
    }
    return result_12;
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

inline __device__ void s_bwd_prop_mul_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1135, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S1136, float3  _S1137)
{
    _d_mul_1(_S1135, _S1136, _S1137);
    return;
}

inline __device__ void s_bwd_prop_transform_ray_o_0(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * dpR_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpt_0, float3  _s_dOut_0)
{
    float3  _S1138 = - _s_dOut_0;
    float3  _S1139 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1140;
    (&_S1140)->primal_0 = (*dpt_0).primal_0;
    (&_S1140)->differential_0 = _S1139;
    Matrix<float, 3, 3>  _S1141 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1142;
    (&_S1142)->primal_0 = (*dpR_0).primal_0;
    (&_S1142)->differential_0 = _S1141;
    s_bwd_prop_mul_0(&_S1140, &_S1142, _S1138);
    dpt_0->primal_0 = (*dpt_0).primal_0;
    dpt_0->differential_0 = _S1140.differential_0;
    dpR_0->primal_0 = (*dpR_0).primal_0;
    dpR_0->differential_0 = _S1142.differential_0;
    return;
}

inline __device__ void s_bwd_transform_ray_o_0(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S1143, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1144, float3  _S1145)
{
    s_bwd_prop_transform_ray_o_0(_S1143, _S1144, _S1145);
    return;
}

inline __device__ void transform_ray_o_vjp(Matrix<float, 3, 3>  R_3, float3  t_1, float3  v_ray_o_0, Matrix<float, 3, 3>  * v_R_0, float3  * v_t_0)
{
    Matrix<float, 3, 3>  _S1146 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 dp_R_0;
    (&dp_R_0)->primal_0 = R_3;
    (&dp_R_0)->differential_0 = _S1146;
    float3  _S1147 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_t_0;
    (&dp_t_0)->primal_0 = t_1;
    (&dp_t_0)->differential_0 = _S1147;
    s_bwd_transform_ray_o_0(&dp_R_0, &dp_t_0, v_ray_o_0);
    *v_R_0 = dp_R_0.differential_0;
    *v_t_0 = dp_t_0.differential_0;
    return;
}

inline __device__ void s_bwd_prop_transform_ray_d_0(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * dpR_1, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpraydir_0, float3  _s_dOut_1)
{
    float3  _S1148 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1149;
    (&_S1149)->primal_0 = (*dpraydir_0).primal_0;
    (&_S1149)->differential_0 = _S1148;
    Matrix<float, 3, 3>  _S1150 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1151;
    (&_S1151)->primal_0 = (*dpR_1).primal_0;
    (&_S1151)->differential_0 = _S1150;
    s_bwd_prop_mul_0(&_S1149, &_S1151, _s_dOut_1);
    dpraydir_0->primal_0 = (*dpraydir_0).primal_0;
    dpraydir_0->differential_0 = _S1149.differential_0;
    dpR_1->primal_0 = (*dpR_1).primal_0;
    dpR_1->differential_0 = _S1151.differential_0;
    return;
}

inline __device__ void s_bwd_transform_ray_d_0(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S1152, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1153, float3  _S1154)
{
    s_bwd_prop_transform_ray_d_0(_S1152, _S1153, _S1154);
    return;
}

inline __device__ void transform_ray_d_vjp(Matrix<float, 3, 3>  R_4, float3  raydir_11, float3  v_ray_d_0, Matrix<float, 3, 3>  * v_R_1, float3  * v_raydir_0)
{
    Matrix<float, 3, 3>  _S1155 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 dp_R_1;
    (&dp_R_1)->primal_0 = R_4;
    (&dp_R_1)->differential_0 = _S1155;
    float3  _S1156 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_raydir_0;
    (&dp_raydir_0)->primal_0 = raydir_11;
    (&dp_raydir_0)->differential_0 = _S1156;
    s_bwd_transform_ray_d_0(&dp_R_1, &dp_raydir_0, v_ray_d_0);
    *v_R_1 = dp_R_1.differential_0;
    *v_raydir_0 = dp_raydir_0.differential_0;
    return;
}

inline __device__ void _d_exp_0(DiffPair_float_0 * dpx_7, float dOut_7)
{
    float _S1157 = (F32_exp(((*dpx_7).primal_0))) * dOut_7;
    dpx_7->primal_0 = (*dpx_7).primal_0;
    dpx_7->differential_0 = _S1157;
    return;
}

inline __device__ float3  exp_0(float3  x_27)
{
    float3  result_13;
    int i_8 = int(0);
    for(;;)
    {
        if(i_8 < int(3))
        {
        }
        else
        {
            break;
        }
        *_slang_vector_get_element_ptr(&result_13, i_8) = (F32_exp((_slang_vector_get_element(x_27, i_8))));
        i_8 = i_8 + int(1);
    }
    return result_13;
}

inline __device__ void _d_exp_vector_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_8, float3  dOut_8)
{
    float3  _S1158 = exp_0((*dpx_8).primal_0) * dOut_8;
    dpx_8->primal_0 = (*dpx_8).primal_0;
    dpx_8->differential_0 = _S1158;
    return;
}

inline __device__ Matrix<float, 3, 3>  compute_3dgut_iscl_rot(float4  quat_5, float3  scale_4)
{
    float x_28 = quat_5.y;
    float x2_5 = x_28 * x_28;
    float y2_5 = quat_5.z * quat_5.z;
    float z2_5 = quat_5.w * quat_5.w;
    float xy_5 = quat_5.y * quat_5.z;
    float xz_5 = quat_5.y * quat_5.w;
    float yz_5 = quat_5.z * quat_5.w;
    float wx_5 = quat_5.x * quat_5.y;
    float wy_5 = quat_5.x * quat_5.z;
    float wz_5 = quat_5.x * quat_5.w;
    float3  _S1159 = exp_0(- scale_4);
    return mul_1(makeMatrix<float, 3, 3> (_S1159.x, 0.0f, 0.0f, 0.0f, _S1159.y, 0.0f, 0.0f, 0.0f, _S1159.z), transpose_0(transpose_0(makeMatrix<float, 3, 3> (1.0f - 2.0f * (y2_5 + z2_5), 2.0f * (xy_5 + wz_5), 2.0f * (xz_5 - wy_5), 2.0f * (xy_5 - wz_5), 1.0f - 2.0f * (x2_5 + z2_5), 2.0f * (yz_5 + wx_5), 2.0f * (xz_5 + wy_5), 2.0f * (yz_5 - wx_5), 1.0f - 2.0f * (x2_5 + y2_5)))));
}

struct DiffPair_vectorx3Cfloatx2C4x3E_0
{
    float4  primal_0;
    float4  differential_0;
};

inline __device__ float3  s_primal_ctx_exp_0(float3  _S1160)
{
    return exp_0(_S1160);
}

inline __device__ void s_bwd_prop_mul_1(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S1161, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S1162, Matrix<float, 3, 3>  _S1163)
{
    mul_0(_S1161, _S1162, _S1163);
    return;
}

inline __device__ void s_bwd_prop_exp_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1164, float3  _S1165)
{
    _d_exp_vector_0(_S1164, _S1165);
    return;
}

inline __device__ void s_bwd_prop_compute_3dgut_iscl_rot_0(DiffPair_vectorx3Cfloatx2C4x3E_0 * dpquat_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpscale_0, Matrix<float, 3, 3>  _s_dOut_2)
{
    float _S1166 = (*dpquat_0).primal_0.y;
    float x2_6 = _S1166 * _S1166;
    float y2_6 = (*dpquat_0).primal_0.z * (*dpquat_0).primal_0.z;
    float z2_6 = (*dpquat_0).primal_0.w * (*dpquat_0).primal_0.w;
    float xy_6 = (*dpquat_0).primal_0.y * (*dpquat_0).primal_0.z;
    float xz_6 = (*dpquat_0).primal_0.y * (*dpquat_0).primal_0.w;
    float yz_6 = (*dpquat_0).primal_0.z * (*dpquat_0).primal_0.w;
    float wx_6 = (*dpquat_0).primal_0.x * (*dpquat_0).primal_0.y;
    float wy_6 = (*dpquat_0).primal_0.x * (*dpquat_0).primal_0.z;
    float wz_6 = (*dpquat_0).primal_0.x * (*dpquat_0).primal_0.w;
    float3  _S1167 = - (*dpscale_0).primal_0;
    float3  _S1168 = s_primal_ctx_exp_0(_S1167);
    Matrix<float, 3, 3>  _S1169 = transpose_0(transpose_0(makeMatrix<float, 3, 3> (1.0f - 2.0f * (y2_6 + z2_6), 2.0f * (xy_6 + wz_6), 2.0f * (xz_6 - wy_6), 2.0f * (xy_6 - wz_6), 1.0f - 2.0f * (x2_6 + z2_6), 2.0f * (yz_6 + wx_6), 2.0f * (xz_6 + wy_6), 2.0f * (yz_6 - wx_6), 1.0f - 2.0f * (x2_6 + y2_6))));
    Matrix<float, 3, 3>  _S1170 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1171;
    (&_S1171)->primal_0 = makeMatrix<float, 3, 3> (_S1168.x, 0.0f, 0.0f, 0.0f, _S1168.y, 0.0f, 0.0f, 0.0f, _S1168.z);
    (&_S1171)->differential_0 = _S1170;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1172;
    (&_S1172)->primal_0 = _S1169;
    (&_S1172)->differential_0 = _S1170;
    s_bwd_prop_mul_1(&_S1171, &_S1172, _s_dOut_2);
    Matrix<float, 3, 3>  _S1173 = transpose_0(_S1172.differential_0);
    float3  _S1174 = make_float3 (_S1171.differential_0.rows[int(0)].x, _S1171.differential_0.rows[int(1)].y, _S1171.differential_0.rows[int(2)].z);
    float3  _S1175 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1176;
    (&_S1176)->primal_0 = _S1167;
    (&_S1176)->differential_0 = _S1175;
    s_bwd_prop_exp_0(&_S1176, _S1174);
    float3  _S1177 = - _S1176.differential_0;
    Matrix<float, 3, 3>  _S1178 = transpose_0(_S1173);
    float _S1179 = 2.0f * - _S1178.rows[int(2)].z;
    float _S1180 = 2.0f * _S1178.rows[int(2)].y;
    float _S1181 = 2.0f * _S1178.rows[int(2)].x;
    float _S1182 = 2.0f * _S1178.rows[int(1)].z;
    float _S1183 = 2.0f * - _S1178.rows[int(1)].y;
    float _S1184 = 2.0f * _S1178.rows[int(1)].x;
    float _S1185 = 2.0f * _S1178.rows[int(0)].z;
    float _S1186 = 2.0f * _S1178.rows[int(0)].y;
    float _S1187 = 2.0f * - _S1178.rows[int(0)].x;
    float _S1188 = - _S1184 + _S1186;
    float _S1189 = _S1181 + - _S1185;
    float _S1190 = - _S1180 + _S1182;
    float _S1191 = _S1180 + _S1182;
    float _S1192 = _S1181 + _S1185;
    float _S1193 = _S1184 + _S1186;
    float _S1194 = (*dpquat_0).primal_0.w * (_S1183 + _S1187);
    float _S1195 = (*dpquat_0).primal_0.z * (_S1179 + _S1187);
    float _S1196 = (*dpquat_0).primal_0.y * (_S1179 + _S1183);
    float _S1197 = (*dpquat_0).primal_0.x * _S1188 + (*dpquat_0).primal_0.z * _S1191 + (*dpquat_0).primal_0.y * _S1192 + _S1194 + _S1194;
    float _S1198 = (*dpquat_0).primal_0.x * _S1189 + (*dpquat_0).primal_0.w * _S1191 + (*dpquat_0).primal_0.y * _S1193 + _S1195 + _S1195;
    float _S1199 = (*dpquat_0).primal_0.x * _S1190 + (*dpquat_0).primal_0.w * _S1192 + (*dpquat_0).primal_0.z * _S1193 + _S1196 + _S1196;
    float _S1200 = (*dpquat_0).primal_0.w * _S1188 + (*dpquat_0).primal_0.z * _S1189 + (*dpquat_0).primal_0.y * _S1190;
    dpscale_0->primal_0 = (*dpscale_0).primal_0;
    dpscale_0->differential_0 = _S1177;
    float4  _S1201 = make_float4 (0.0f);
    *&((&_S1201)->w) = _S1197;
    *&((&_S1201)->z) = _S1198;
    *&((&_S1201)->y) = _S1199;
    *&((&_S1201)->x) = _S1200;
    dpquat_0->primal_0 = (*dpquat_0).primal_0;
    dpquat_0->differential_0 = _S1201;
    return;
}

inline __device__ void s_bwd_compute_3dgut_iscl_rot_0(DiffPair_vectorx3Cfloatx2C4x3E_0 * _S1202, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1203, Matrix<float, 3, 3>  _S1204)
{
    s_bwd_prop_compute_3dgut_iscl_rot_0(_S1202, _S1203, _S1204);
    return;
}

inline __device__ void compute_3dgut_iscl_rot_vjp(float4  quat_6, float3  scale_5, Matrix<float, 3, 3>  v_iscl_rot_0, float4  * v_quat_0, float3  * v_scale_0)
{
    float4  _S1205 = make_float4 (0.0f);
    DiffPair_vectorx3Cfloatx2C4x3E_0 dp_quat_0;
    (&dp_quat_0)->primal_0 = quat_6;
    (&dp_quat_0)->differential_0 = _S1205;
    float3  _S1206 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_scale_0;
    (&dp_scale_0)->primal_0 = scale_5;
    (&dp_scale_0)->differential_0 = _S1206;
    s_bwd_compute_3dgut_iscl_rot_0(&dp_quat_0, &dp_scale_0, v_iscl_rot_0);
    *v_quat_0 = dp_quat_0.differential_0;
    *v_scale_0 = dp_scale_0.differential_0;
    return;
}

inline __device__ float evaluate_alpha_3dgs(float3  mean_0, Matrix<float, 3, 3>  iscl_rot_0, float opacity_0, float3  ray_o_0, float3  ray_d_0)
{
    float3  grd_0 = mul_2(iscl_rot_0, ray_d_0);
    float3  gcrod_0 = cross_0(grd_0, mul_2(iscl_rot_0, ray_o_0 - mean_0));
    return opacity_0 * (F32_exp((-0.5f * dot_1(gcrod_0, gcrod_0) / dot_1(grd_0, grd_0))));
}

inline __device__ float3  s_primal_ctx_mul_0(Matrix<float, 3, 3>  _S1207, float3  _S1208)
{
    return mul_2(_S1207, _S1208);
}

inline __device__ float3  s_primal_ctx_cross_0(float3  _S1209, float3  _S1210)
{
    return cross_0(_S1209, _S1210);
}

inline __device__ float s_primal_ctx_dot_0(float3  _S1211, float3  _S1212)
{
    return dot_1(_S1211, _S1212);
}

inline __device__ float s_primal_ctx_exp_1(float _S1213)
{
    return (F32_exp((_S1213)));
}

inline __device__ void s_bwd_prop_exp_1(DiffPair_float_0 * _S1214, float _S1215)
{
    _d_exp_0(_S1214, _S1215);
    return;
}

inline __device__ void s_bwd_prop_dot_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1216, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1217, float _S1218)
{
    _d_dot_1(_S1216, _S1217, _S1218);
    return;
}

inline __device__ void s_bwd_prop_cross_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1219, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1220, float3  _S1221)
{
    _d_cross_0(_S1219, _S1220, _S1221);
    return;
}

inline __device__ void s_bwd_prop_mul_2(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S1222, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1223, float3  _S1224)
{
    _d_mul_0(_S1222, _S1223, _S1224);
    return;
}

inline __device__ void s_bwd_prop_evaluate_alpha_3dgs_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpmean_0, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * dpiscl_rot_0, DiffPair_float_0 * dpopacity_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpray_o_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpray_d_0, float _s_dOut_3)
{
    float3  _S1225 = (*dpray_o_0).primal_0 - (*dpmean_0).primal_0;
    float3  _S1226 = s_primal_ctx_mul_0((*dpiscl_rot_0).primal_0, _S1225);
    float3  _S1227 = s_primal_ctx_mul_0((*dpiscl_rot_0).primal_0, (*dpray_d_0).primal_0);
    float3  _S1228 = s_primal_ctx_cross_0(_S1227, _S1226);
    float _S1229 = -0.5f * s_primal_ctx_dot_0(_S1228, _S1228);
    float _S1230 = s_primal_ctx_dot_0(_S1227, _S1227);
    float _S1231 = _S1229 / _S1230;
    float _S1232 = _S1230 * _S1230;
    float _S1233 = (*dpopacity_0).primal_0 * _s_dOut_3;
    float _S1234 = s_primal_ctx_exp_1(_S1231) * _s_dOut_3;
    DiffPair_float_0 _S1235;
    (&_S1235)->primal_0 = _S1231;
    (&_S1235)->differential_0 = 0.0f;
    s_bwd_prop_exp_1(&_S1235, _S1233);
    float _S1236 = _S1235.differential_0 / _S1232;
    float _S1237 = _S1229 * - _S1236;
    float _S1238 = _S1230 * _S1236;
    float3  _S1239 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1240;
    (&_S1240)->primal_0 = _S1227;
    (&_S1240)->differential_0 = _S1239;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1241;
    (&_S1241)->primal_0 = _S1227;
    (&_S1241)->differential_0 = _S1239;
    s_bwd_prop_dot_0(&_S1240, &_S1241, _S1237);
    float _S1242 = -0.5f * _S1238;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1243;
    (&_S1243)->primal_0 = _S1228;
    (&_S1243)->differential_0 = _S1239;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1244;
    (&_S1244)->primal_0 = _S1228;
    (&_S1244)->differential_0 = _S1239;
    s_bwd_prop_dot_0(&_S1243, &_S1244, _S1242);
    float3  _S1245 = _S1244.differential_0 + _S1243.differential_0;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1246;
    (&_S1246)->primal_0 = _S1227;
    (&_S1246)->differential_0 = _S1239;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1247;
    (&_S1247)->primal_0 = _S1226;
    (&_S1247)->differential_0 = _S1239;
    s_bwd_prop_cross_0(&_S1246, &_S1247, _S1245);
    float3  _S1248 = _S1241.differential_0 + _S1240.differential_0 + _S1246.differential_0;
    Matrix<float, 3, 3>  _S1249 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1250;
    (&_S1250)->primal_0 = (*dpiscl_rot_0).primal_0;
    (&_S1250)->differential_0 = _S1249;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1251;
    (&_S1251)->primal_0 = (*dpray_d_0).primal_0;
    (&_S1251)->differential_0 = _S1239;
    s_bwd_prop_mul_2(&_S1250, &_S1251, _S1248);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1252;
    (&_S1252)->primal_0 = (*dpiscl_rot_0).primal_0;
    (&_S1252)->differential_0 = _S1249;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1253;
    (&_S1253)->primal_0 = _S1225;
    (&_S1253)->differential_0 = _S1239;
    s_bwd_prop_mul_2(&_S1252, &_S1253, _S1247.differential_0);
    float3  _S1254 = - _S1253.differential_0;
    dpray_d_0->primal_0 = (*dpray_d_0).primal_0;
    dpray_d_0->differential_0 = _S1251.differential_0;
    dpray_o_0->primal_0 = (*dpray_o_0).primal_0;
    dpray_o_0->differential_0 = _S1253.differential_0;
    dpopacity_0->primal_0 = (*dpopacity_0).primal_0;
    dpopacity_0->differential_0 = _S1234;
    Matrix<float, 3, 3>  _S1255 = _S1250.differential_0 + _S1252.differential_0;
    dpiscl_rot_0->primal_0 = (*dpiscl_rot_0).primal_0;
    dpiscl_rot_0->differential_0 = _S1255;
    dpmean_0->primal_0 = (*dpmean_0).primal_0;
    dpmean_0->differential_0 = _S1254;
    return;
}

inline __device__ void s_bwd_evaluate_alpha_3dgs_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1256, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S1257, DiffPair_float_0 * _S1258, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1259, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1260, float _S1261)
{
    s_bwd_prop_evaluate_alpha_3dgs_0(_S1256, _S1257, _S1258, _S1259, _S1260, _S1261);
    return;
}

inline __device__ void evaluate_alpha_3dgs_vjp(float3  mean_1, Matrix<float, 3, 3>  iscl_rot_1, float opacity_1, float3  ray_o_1, float3  ray_d_1, float v_alpha_0, float3  * v_mean_0, Matrix<float, 3, 3>  * v_iscl_rot_1, float * v_opacity_0, float3  * v_ray_o_1, float3  * v_ray_d_1)
{
    float3  _S1262 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_mean_0;
    (&dp_mean_0)->primal_0 = mean_1;
    (&dp_mean_0)->differential_0 = _S1262;
    Matrix<float, 3, 3>  _S1263 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 dp_iscl_rot_0;
    (&dp_iscl_rot_0)->primal_0 = iscl_rot_1;
    (&dp_iscl_rot_0)->differential_0 = _S1263;
    DiffPair_float_0 dp_opacity_0;
    (&dp_opacity_0)->primal_0 = opacity_1;
    (&dp_opacity_0)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ray_o_0;
    (&dp_ray_o_0)->primal_0 = ray_o_1;
    (&dp_ray_o_0)->differential_0 = _S1262;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ray_d_0;
    (&dp_ray_d_0)->primal_0 = ray_d_1;
    (&dp_ray_d_0)->differential_0 = _S1262;
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
    float3  _S1264 = (*dpray_o_1).primal_0 - (*dpmean_1).primal_0;
    float3  _S1265 = s_primal_ctx_mul_0((*dpiscl_rot_1).primal_0, _S1264);
    float3  _S1266 = s_primal_ctx_mul_0((*dpiscl_rot_1).primal_0, (*dpray_d_1).primal_0);
    float _S1267 = s_primal_ctx_dot_0(_S1266, _S1266);
    float _S1268 = dpdepth_0 / (_S1267 * _S1267);
    float _S1269 = - s_primal_ctx_dot_0(_S1265, _S1266) * - _S1268;
    float _S1270 = _S1267 * _S1268;
    float3  _S1271 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1272;
    (&_S1272)->primal_0 = _S1266;
    (&_S1272)->differential_0 = _S1271;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1273;
    (&_S1273)->primal_0 = _S1266;
    (&_S1273)->differential_0 = _S1271;
    s_bwd_prop_dot_0(&_S1272, &_S1273, _S1269);
    float _S1274 = - _S1270;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1275;
    (&_S1275)->primal_0 = _S1265;
    (&_S1275)->differential_0 = _S1271;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1276;
    (&_S1276)->primal_0 = _S1266;
    (&_S1276)->differential_0 = _S1271;
    s_bwd_prop_dot_0(&_S1275, &_S1276, _S1274);
    float3  _S1277 = _S1273.differential_0 + _S1272.differential_0 + _S1276.differential_0;
    Matrix<float, 3, 3>  _S1278 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1279;
    (&_S1279)->primal_0 = (*dpiscl_rot_1).primal_0;
    (&_S1279)->differential_0 = _S1278;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1280;
    (&_S1280)->primal_0 = (*dpray_d_1).primal_0;
    (&_S1280)->differential_0 = _S1271;
    s_bwd_prop_mul_2(&_S1279, &_S1280, _S1277);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1281;
    (&_S1281)->primal_0 = (*dpiscl_rot_1).primal_0;
    (&_S1281)->differential_0 = _S1278;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1282;
    (&_S1282)->primal_0 = _S1264;
    (&_S1282)->differential_0 = _S1271;
    s_bwd_prop_mul_2(&_S1281, &_S1282, _S1275.differential_0);
    float3  _S1283 = - _S1282.differential_0;
    dpray_d_1->primal_0 = (*dpray_d_1).primal_0;
    dpray_d_1->differential_0 = _S1280.differential_0;
    dpray_o_1->primal_0 = (*dpray_o_1).primal_0;
    dpray_o_1->differential_0 = _S1282.differential_0;
    dprgb_0->primal_0 = (*dprgb_0).primal_0;
    dprgb_0->differential_0 = dpout_rgb_0;
    dpopacity_1->primal_0 = (*dpopacity_1).primal_0;
    dpopacity_1->differential_0 = 0.0f;
    Matrix<float, 3, 3>  _S1284 = _S1279.differential_0 + _S1281.differential_0;
    dpiscl_rot_1->primal_0 = (*dpiscl_rot_1).primal_0;
    dpiscl_rot_1->differential_0 = _S1284;
    dpmean_1->primal_0 = (*dpmean_1).primal_0;
    dpmean_1->differential_0 = _S1283;
    return;
}

inline __device__ void s_bwd_evaluate_color_3dgs_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1285, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S1286, DiffPair_float_0 * _S1287, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1288, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1289, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1290, float3  _S1291, float _S1292)
{
    s_bwd_prop_evaluate_color_3dgs_0(_S1285, _S1286, _S1287, _S1288, _S1289, _S1290, _S1291, _S1292);
    return;
}

inline __device__ void evaluate_color_3dgs_vjp(float3  mean_3, Matrix<float, 3, 3>  iscl_rot_3, float opacity_3, float3  rgb_1, float3  ray_o_3, float3  ray_d_3, float3  v_out_rgb_0, float v_depth_0, float3  * v_mean_1, Matrix<float, 3, 3>  * v_iscl_rot_2, float * v_opacity_1, float3  * v_rgb_0, float3  * v_ray_o_2, float3  * v_ray_d_2)
{
    float3  _S1293 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_mean_1;
    (&dp_mean_1)->primal_0 = mean_3;
    (&dp_mean_1)->differential_0 = _S1293;
    Matrix<float, 3, 3>  _S1294 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 dp_iscl_rot_1;
    (&dp_iscl_rot_1)->primal_0 = iscl_rot_3;
    (&dp_iscl_rot_1)->differential_0 = _S1294;
    DiffPair_float_0 dp_opacity_1;
    (&dp_opacity_1)->primal_0 = opacity_3;
    (&dp_opacity_1)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_rgb_0;
    (&dp_rgb_0)->primal_0 = rgb_1;
    (&dp_rgb_0)->differential_0 = _S1293;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ray_o_1;
    (&dp_ray_o_1)->primal_0 = ray_o_3;
    (&dp_ray_o_1)->differential_0 = _S1293;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ray_d_1;
    (&dp_ray_d_1)->primal_0 = ray_d_3;
    (&dp_ray_d_1)->differential_0 = _S1293;
    s_bwd_evaluate_color_3dgs_0(&dp_mean_1, &dp_iscl_rot_1, &dp_opacity_1, &dp_rgb_0, &dp_ray_o_1, &dp_ray_d_1, v_out_rgb_0, v_depth_0);
    *v_mean_1 = dp_mean_1.differential_0;
    *v_iscl_rot_2 = dp_iscl_rot_1.differential_0;
    *v_opacity_1 = dp_opacity_1.differential_0;
    *v_rgb_0 = dp_rgb_0.differential_0;
    *v_ray_o_2 = dp_ray_o_1.differential_0;
    *v_ray_d_2 = dp_ray_d_1.differential_0;
    return;
}

inline __device__ void evaluate_color_3dgs_normal(float3  mean_4, Matrix<float, 3, 3>  iscl_rot_4, float opacity_4, float3  rgb_2, float3  ray_o_4, float3  ray_d_4, float3  * out_rgb_1, float * depth_1, float3  * normal_0)
{
    *out_rgb_1 = rgb_2;
    float3  grd_2 = mul_2(iscl_rot_4, ray_d_4);
    *depth_1 = - dot_1(mul_2(iscl_rot_4, ray_o_4 - mean_4), grd_2) / dot_1(grd_2, grd_2);
    float3  n_3 = mul_3(grd_2, iscl_rot_4);
    *normal_0 = - n_3 * make_float3 ((F32_rsqrt((dot_1(n_3, n_3)))));
    return;
}

inline __device__ float3  s_primal_ctx_mul_1(float3  _S1295, Matrix<float, 3, 3>  _S1296)
{
    return mul_3(_S1295, _S1296);
}

inline __device__ float s_primal_ctx_rsqrt_0(float _S1297)
{
    return (F32_rsqrt((_S1297)));
}

inline __device__ void s_bwd_prop_rsqrt_0(DiffPair_float_0 * _S1298, float _S1299)
{
    _d_rsqrt_0(_S1298, _S1299);
    return;
}

inline __device__ void s_bwd_prop_evaluate_color_3dgs_normal_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpmean_2, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * dpiscl_rot_2, DiffPair_float_0 * dpopacity_2, DiffPair_vectorx3Cfloatx2C3x3E_0 * dprgb_1, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpray_o_2, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpray_d_2, float3  dpout_rgb_1, float dpdepth_1, float3  dpnormal_0)
{
    float3  _S1300 = (*dpray_o_2).primal_0 - (*dpmean_2).primal_0;
    float3  _S1301 = s_primal_ctx_mul_0((*dpiscl_rot_2).primal_0, _S1300);
    float3  _S1302 = s_primal_ctx_mul_0((*dpiscl_rot_2).primal_0, (*dpray_d_2).primal_0);
    float _S1303 = s_primal_ctx_dot_0(_S1302, _S1302);
    float _S1304 = - s_primal_ctx_dot_0(_S1301, _S1302);
    float _S1305 = _S1303 * _S1303;
    float3  _S1306 = s_primal_ctx_mul_1(_S1302, (*dpiscl_rot_2).primal_0);
    float _S1307 = s_primal_ctx_dot_0(_S1306, _S1306);
    float3  _S1308 = - _S1306 * dpnormal_0;
    float3  _S1309 = make_float3 (s_primal_ctx_rsqrt_0(_S1307)) * dpnormal_0;
    float _S1310 = _S1308.x + _S1308.y + _S1308.z;
    DiffPair_float_0 _S1311;
    (&_S1311)->primal_0 = _S1307;
    (&_S1311)->differential_0 = 0.0f;
    s_bwd_prop_rsqrt_0(&_S1311, _S1310);
    float3  _S1312 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1313;
    (&_S1313)->primal_0 = _S1306;
    (&_S1313)->differential_0 = _S1312;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1314;
    (&_S1314)->primal_0 = _S1306;
    (&_S1314)->differential_0 = _S1312;
    s_bwd_prop_dot_0(&_S1313, &_S1314, _S1311.differential_0);
    float3  _S1315 = _S1314.differential_0 + _S1313.differential_0 + - _S1309;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1316;
    (&_S1316)->primal_0 = _S1302;
    (&_S1316)->differential_0 = _S1312;
    Matrix<float, 3, 3>  _S1317 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1318;
    (&_S1318)->primal_0 = (*dpiscl_rot_2).primal_0;
    (&_S1318)->differential_0 = _S1317;
    s_bwd_prop_mul_0(&_S1316, &_S1318, _S1315);
    float _S1319 = dpdepth_1 / _S1305;
    float _S1320 = _S1304 * - _S1319;
    float _S1321 = - (_S1303 * _S1319);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1322;
    (&_S1322)->primal_0 = _S1301;
    (&_S1322)->differential_0 = _S1312;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1323;
    (&_S1323)->primal_0 = _S1302;
    (&_S1323)->differential_0 = _S1312;
    s_bwd_prop_dot_0(&_S1322, &_S1323, _S1321);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1324;
    (&_S1324)->primal_0 = _S1302;
    (&_S1324)->differential_0 = _S1312;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1325;
    (&_S1325)->primal_0 = _S1302;
    (&_S1325)->differential_0 = _S1312;
    s_bwd_prop_dot_0(&_S1324, &_S1325, _S1320);
    float3  _S1326 = _S1316.differential_0 + _S1323.differential_0 + _S1325.differential_0 + _S1324.differential_0;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1327;
    (&_S1327)->primal_0 = (*dpiscl_rot_2).primal_0;
    (&_S1327)->differential_0 = _S1317;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1328;
    (&_S1328)->primal_0 = (*dpray_d_2).primal_0;
    (&_S1328)->differential_0 = _S1312;
    s_bwd_prop_mul_2(&_S1327, &_S1328, _S1326);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1329;
    (&_S1329)->primal_0 = (*dpiscl_rot_2).primal_0;
    (&_S1329)->differential_0 = _S1317;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1330;
    (&_S1330)->primal_0 = _S1300;
    (&_S1330)->differential_0 = _S1312;
    s_bwd_prop_mul_2(&_S1329, &_S1330, _S1322.differential_0);
    float3  _S1331 = - _S1330.differential_0;
    dpray_d_2->primal_0 = (*dpray_d_2).primal_0;
    dpray_d_2->differential_0 = _S1328.differential_0;
    dpray_o_2->primal_0 = (*dpray_o_2).primal_0;
    dpray_o_2->differential_0 = _S1330.differential_0;
    dprgb_1->primal_0 = (*dprgb_1).primal_0;
    dprgb_1->differential_0 = dpout_rgb_1;
    dpopacity_2->primal_0 = (*dpopacity_2).primal_0;
    dpopacity_2->differential_0 = 0.0f;
    Matrix<float, 3, 3>  _S1332 = _S1318.differential_0 + _S1327.differential_0 + _S1329.differential_0;
    dpiscl_rot_2->primal_0 = (*dpiscl_rot_2).primal_0;
    dpiscl_rot_2->differential_0 = _S1332;
    dpmean_2->primal_0 = (*dpmean_2).primal_0;
    dpmean_2->differential_0 = _S1331;
    return;
}

inline __device__ void s_bwd_evaluate_color_3dgs_normal_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1333, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S1334, DiffPair_float_0 * _S1335, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1336, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1337, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1338, float3  _S1339, float _S1340, float3  _S1341)
{
    s_bwd_prop_evaluate_color_3dgs_normal_0(_S1333, _S1334, _S1335, _S1336, _S1337, _S1338, _S1339, _S1340, _S1341);
    return;
}

inline __device__ void evaluate_color_3dgs_normal_vjp(float3  mean_5, Matrix<float, 3, 3>  iscl_rot_5, float opacity_5, float3  rgb_3, float3  ray_o_5, float3  ray_d_5, float3  v_out_rgb_1, float v_depth_1, float3  v_normal_0, float3  * v_mean_2, Matrix<float, 3, 3>  * v_iscl_rot_3, float * v_opacity_2, float3  * v_rgb_1, float3  * v_ray_o_3, float3  * v_ray_d_3)
{
    float3  _S1342 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_mean_2;
    (&dp_mean_2)->primal_0 = mean_5;
    (&dp_mean_2)->differential_0 = _S1342;
    Matrix<float, 3, 3>  _S1343 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 dp_iscl_rot_2;
    (&dp_iscl_rot_2)->primal_0 = iscl_rot_5;
    (&dp_iscl_rot_2)->differential_0 = _S1343;
    DiffPair_float_0 dp_opacity_2;
    (&dp_opacity_2)->primal_0 = opacity_5;
    (&dp_opacity_2)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_rgb_1;
    (&dp_rgb_1)->primal_0 = rgb_3;
    (&dp_rgb_1)->differential_0 = _S1342;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ray_o_2;
    (&dp_ray_o_2)->primal_0 = ray_o_5;
    (&dp_ray_o_2)->differential_0 = _S1342;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ray_d_2;
    (&dp_ray_d_2)->primal_0 = ray_d_5;
    (&dp_ray_d_2)->differential_0 = _S1342;
    s_bwd_evaluate_color_3dgs_normal_0(&dp_mean_2, &dp_iscl_rot_2, &dp_opacity_2, &dp_rgb_1, &dp_ray_o_2, &dp_ray_d_2, v_out_rgb_1, v_depth_1, v_normal_0);
    *v_mean_2 = dp_mean_2.differential_0;
    *v_iscl_rot_3 = dp_iscl_rot_2.differential_0;
    *v_opacity_2 = dp_opacity_2.differential_0;
    *v_rgb_1 = dp_rgb_1.differential_0;
    *v_ray_o_3 = dp_ray_o_2.differential_0;
    *v_ray_d_3 = dp_ray_d_2.differential_0;
    return;
}

inline __device__ Matrix<float, 3, 3>  compute_2dgs_iscl_rot(float4  quat_7, float3  scale_6)
{
    float x_29 = quat_7.y;
    float x2_7 = x_29 * x_29;
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
    float _S1344 = (*dpquat_1).primal_0.y;
    float x2_8 = _S1344 * _S1344;
    float y2_8 = (*dpquat_1).primal_0.z * (*dpquat_1).primal_0.z;
    float z2_8 = (*dpquat_1).primal_0.w * (*dpquat_1).primal_0.w;
    float xy_8 = (*dpquat_1).primal_0.y * (*dpquat_1).primal_0.z;
    float xz_8 = (*dpquat_1).primal_0.y * (*dpquat_1).primal_0.w;
    float yz_8 = (*dpquat_1).primal_0.z * (*dpquat_1).primal_0.w;
    float wx_8 = (*dpquat_1).primal_0.x * (*dpquat_1).primal_0.y;
    float wy_8 = (*dpquat_1).primal_0.x * (*dpquat_1).primal_0.z;
    float wz_8 = (*dpquat_1).primal_0.x * (*dpquat_1).primal_0.w;
    float _S1345 = - (*dpscale_1).primal_0.x;
    float _S1346 = - (*dpscale_1).primal_0.y;
    Matrix<float, 3, 3>  _S1347 = transpose_0(transpose_0(makeMatrix<float, 3, 3> (1.0f - 2.0f * (y2_8 + z2_8), 2.0f * (xy_8 + wz_8), 2.0f * (xz_8 - wy_8), 2.0f * (xy_8 - wz_8), 1.0f - 2.0f * (x2_8 + z2_8), 2.0f * (yz_8 + wx_8), 2.0f * (xz_8 + wy_8), 2.0f * (yz_8 - wx_8), 1.0f - 2.0f * (x2_8 + y2_8))));
    Matrix<float, 3, 3>  _S1348 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1349;
    (&_S1349)->primal_0 = makeMatrix<float, 3, 3> (s_primal_ctx_exp_1(_S1345), 0.0f, 0.0f, 0.0f, s_primal_ctx_exp_1(_S1346), 0.0f, 0.0f, 0.0f, 1.0f);
    (&_S1349)->differential_0 = _S1348;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1350;
    (&_S1350)->primal_0 = _S1347;
    (&_S1350)->differential_0 = _S1348;
    s_bwd_prop_mul_1(&_S1349, &_S1350, _s_dOut_4);
    Matrix<float, 3, 3>  _S1351 = transpose_0(_S1350.differential_0);
    DiffPair_float_0 _S1352;
    (&_S1352)->primal_0 = _S1346;
    (&_S1352)->differential_0 = 0.0f;
    s_bwd_prop_exp_1(&_S1352, _S1349.differential_0.rows[int(1)].y);
    float _S1353 = - _S1352.differential_0;
    DiffPair_float_0 _S1354;
    (&_S1354)->primal_0 = _S1345;
    (&_S1354)->differential_0 = 0.0f;
    s_bwd_prop_exp_1(&_S1354, _S1349.differential_0.rows[int(0)].x);
    Matrix<float, 3, 3>  _S1355 = transpose_0(_S1351);
    float _S1356 = 2.0f * - _S1355.rows[int(2)].z;
    float _S1357 = 2.0f * _S1355.rows[int(2)].y;
    float _S1358 = 2.0f * _S1355.rows[int(2)].x;
    float _S1359 = 2.0f * _S1355.rows[int(1)].z;
    float _S1360 = 2.0f * - _S1355.rows[int(1)].y;
    float _S1361 = 2.0f * _S1355.rows[int(1)].x;
    float _S1362 = 2.0f * _S1355.rows[int(0)].z;
    float _S1363 = 2.0f * _S1355.rows[int(0)].y;
    float _S1364 = 2.0f * - _S1355.rows[int(0)].x;
    float _S1365 = - _S1361 + _S1363;
    float _S1366 = _S1358 + - _S1362;
    float _S1367 = - _S1357 + _S1359;
    float _S1368 = _S1357 + _S1359;
    float _S1369 = _S1358 + _S1362;
    float _S1370 = _S1361 + _S1363;
    float _S1371 = (*dpquat_1).primal_0.w * (_S1360 + _S1364);
    float _S1372 = (*dpquat_1).primal_0.z * (_S1356 + _S1364);
    float _S1373 = (*dpquat_1).primal_0.y * (_S1356 + _S1360);
    float _S1374 = (*dpquat_1).primal_0.x * _S1365 + (*dpquat_1).primal_0.z * _S1368 + (*dpquat_1).primal_0.y * _S1369 + _S1371 + _S1371;
    float _S1375 = (*dpquat_1).primal_0.x * _S1366 + (*dpquat_1).primal_0.w * _S1368 + (*dpquat_1).primal_0.y * _S1370 + _S1372 + _S1372;
    float _S1376 = (*dpquat_1).primal_0.x * _S1367 + (*dpquat_1).primal_0.w * _S1369 + (*dpquat_1).primal_0.z * _S1370 + _S1373 + _S1373;
    float _S1377 = (*dpquat_1).primal_0.w * _S1365 + (*dpquat_1).primal_0.z * _S1366 + (*dpquat_1).primal_0.y * _S1367;
    float3  _S1378 = make_float3 (- _S1354.differential_0, _S1353, 0.0f);
    dpscale_1->primal_0 = (*dpscale_1).primal_0;
    dpscale_1->differential_0 = _S1378;
    float4  _S1379 = make_float4 (0.0f);
    *&((&_S1379)->w) = _S1374;
    *&((&_S1379)->z) = _S1375;
    *&((&_S1379)->y) = _S1376;
    *&((&_S1379)->x) = _S1377;
    dpquat_1->primal_0 = (*dpquat_1).primal_0;
    dpquat_1->differential_0 = _S1379;
    return;
}

inline __device__ void s_bwd_compute_2dgs_iscl_rot_0(DiffPair_vectorx3Cfloatx2C4x3E_0 * _S1380, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1381, Matrix<float, 3, 3>  _S1382)
{
    s_bwd_prop_compute_2dgs_iscl_rot_0(_S1380, _S1381, _S1382);
    return;
}

inline __device__ void compute_2dgs_iscl_rot_vjp(float4  quat_8, float3  scale_7, Matrix<float, 3, 3>  v_iscl_rot_4, float4  * v_quat_1, float3  * v_scale_1)
{
    float4  _S1383 = make_float4 (0.0f);
    DiffPair_vectorx3Cfloatx2C4x3E_0 dp_quat_1;
    (&dp_quat_1)->primal_0 = quat_8;
    (&dp_quat_1)->differential_0 = _S1383;
    float3  _S1384 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_scale_1;
    (&dp_scale_1)->primal_0 = scale_7;
    (&dp_scale_1)->differential_0 = _S1384;
    s_bwd_compute_2dgs_iscl_rot_0(&dp_quat_1, &dp_scale_1, v_iscl_rot_4);
    *v_quat_1 = dp_quat_1.differential_0;
    *v_scale_1 = dp_scale_1.differential_0;
    return;
}

inline __device__ float evaluate_alpha_2dgs(float3  mean_6, Matrix<float, 3, 3>  iscl_rot_6, float opacity_6, float3  ray_o_6, float3  ray_d_6)
{
    SurfelHit_0 h_0;
    float3  gro_0 = mul_2(iscl_rot_6, ray_o_6 - mean_6);
    float3  grd_3 = mul_2(iscl_rot_6, ray_d_6);
    float _S1385 = grd_3.z;
    if((_S1385 * _S1385) > (9.99999997475242708e-07f * dot_1(ray_d_6, ray_d_6)))
    {
        float _S1386 = - gro_0.z / _S1385;
        (&h_0)->t_hit_0 = _S1386;
        float2  uv_34 = float2 {gro_0.x, gro_0.y} + make_float2 (_S1386) * float2 {grd_3.x, grd_3.y};
        (&h_0)->g_0 = (F32_exp((-0.5f * dot_0(uv_34, uv_34))));
    }
    else
    {
        (&h_0)->t_hit_0 = 0.0f;
        (&h_0)->g_0 = 0.0f;
    }
    SurfelHit_0 _S1387 = h_0;
    return opacity_6 * _S1387.g_0;
}

inline __device__ float evaluate_2dgs(float3  mean_7, Matrix<float, 3, 3>  iscl_rot_7, float opacity_7, float3  rgb_4, float3  ray_o_7, float3  ray_d_7, float3  * out_rgb_2, float * depth_2, float3  * normal_1)
{
    *out_rgb_2 = rgb_4;
    SurfelHit_0 h_1;
    float3  gro_1 = mul_2(iscl_rot_7, ray_o_7 - mean_7);
    float3  grd_4 = mul_2(iscl_rot_7, ray_d_7);
    float _S1388 = grd_4.z;
    if((_S1388 * _S1388) > (9.99999997475242708e-07f * dot_1(ray_d_7, ray_d_7)))
    {
        float _S1389 = - gro_1.z / _S1388;
        (&h_1)->t_hit_0 = _S1389;
        float2  uv_35 = float2 {gro_1.x, gro_1.y} + make_float2 (_S1389) * float2 {grd_4.x, grd_4.y};
        (&h_1)->g_0 = (F32_exp((-0.5f * dot_0(uv_35, uv_35))));
    }
    else
    {
        (&h_1)->t_hit_0 = 0.0f;
        (&h_1)->g_0 = 0.0f;
    }
    SurfelHit_0 _S1390 = h_1;
    *depth_2 = _S1390.t_hit_0;
    float3  n_4 = make_float3 (iscl_rot_7.rows[int(2)].x, iscl_rot_7.rows[int(2)].y, iscl_rot_7.rows[int(2)].z);
    float3  _S1391;
    if((dot_1(n_4, ray_d_7)) > 0.0f)
    {
        _S1391 = - n_4;
    }
    else
    {
        _S1391 = n_4;
    }
    *normal_1 = _S1391;
    return opacity_7 * _S1390.g_0;
}

inline __device__ void evaluate_color_2dgs(float3  mean_8, Matrix<float, 3, 3>  iscl_rot_8, float opacity_8, float3  rgb_5, float3  ray_o_8, float3  ray_d_8, float3  * out_rgb_3, float * depth_3, float3  * normal_2)
{
    float _S1392 = evaluate_2dgs(mean_8, iscl_rot_8, opacity_8, rgb_5, ray_o_8, ray_d_8, out_rgb_3, depth_3, normal_2);
    return;
}

inline __device__ float s_primal_ctx_dot_1(float2  _S1393, float2  _S1394)
{
    return dot_0(_S1393, _S1394);
}

inline __device__ void s_bwd_prop_dot_1(DiffPair_vectorx3Cfloatx2C2x3E_0 * _S1395, DiffPair_vectorx3Cfloatx2C2x3E_0 * _S1396, float _S1397)
{
    _d_dot_0(_S1395, _S1396, _S1397);
    return;
}

inline __device__ void s_bwd_prop_evaluate_2dgs_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpmean_3, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * dpiscl_rot_3, DiffPair_float_0 * dpopacity_3, DiffPair_vectorx3Cfloatx2C3x3E_0 * dprgb_2, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpray_o_3, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpray_d_3, float3  dpout_rgb_2, float dpdepth_2, float3  dpnormal_1, float _s_dOut_5)
{
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1398 = *dpiscl_rot_3;
    DiffPair_float_0 _S1399 = *dpopacity_3;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1400 = *dpray_d_3;
    float2  _S1401 = make_float2 (0.0f);
    float3  _S1402 = (*dpray_o_3).primal_0 - (*dpmean_3).primal_0;
    float3  _S1403 = s_primal_ctx_mul_0((*dpiscl_rot_3).primal_0, _S1402);
    float3  _S1404 = s_primal_ctx_mul_0((*dpiscl_rot_3).primal_0, (*dpray_d_3).primal_0);
    float _S1405 = _S1404.z;
    float _S1406 = _S1405 * _S1405;
    bool _S1407 = _S1406 > (9.99999997475242708e-07f * s_primal_ctx_dot_0((*dpray_d_3).primal_0, (*dpray_d_3).primal_0));
    SurfelHit_0 h_2;
    float _S1408;
    float _S1409;
    float _S1410;
    float2  _S1411;
    float2  _S1412;
    float2  _S1413;
    if(_S1407)
    {
        float _S1414 = - _S1403.z;
        float _S1415 = _S1414 / _S1405;
        float2  _S1416 = make_float2 (_S1415);
        float2  _S1417 = float2 {_S1404.x, _S1404.y};
        float2  uv_36 = float2 {_S1403.x, _S1403.y} + make_float2 (_S1415) * _S1417;
        float _S1418 = -0.5f * s_primal_ctx_dot_1(uv_36, uv_36);
        (&h_2)->g_0 = s_primal_ctx_exp_1(_S1418);
        (&h_2)->t_hit_0 = _S1415;
        _S1408 = _S1418;
        _S1411 = uv_36;
        _S1412 = _S1416;
        _S1413 = _S1417;
        _S1409 = _S1406;
        _S1410 = _S1414;
    }
    else
    {
        (&h_2)->g_0 = 0.0f;
        (&h_2)->t_hit_0 = 0.0f;
        _S1408 = 0.0f;
        _S1411 = _S1401;
        _S1412 = _S1401;
        _S1413 = _S1401;
        _S1409 = 0.0f;
        _S1410 = 0.0f;
    }
    float3  n_5 = make_float3 (_S1398.primal_0.rows[int(2)].x, _S1398.primal_0.rows[int(2)].y, _S1398.primal_0.rows[int(2)].z);
    bool _S1419 = (s_primal_ctx_dot_0(n_5, _S1400.primal_0)) > 0.0f;
    float3  _S1420 = make_float3 (0.0f);
    SurfelHit_0 _S1421 = SurfelHit_x24_syn_dzero_0();
    float _S1422 = _S1399.primal_0 * _s_dOut_5;
    float _S1423 = h_2.g_0 * _s_dOut_5;
    SurfelHit_0 _S1424 = _S1421;
    (&_S1424)->g_0 = _S1422;
    SurfelHit_0 _S1425 = _S1421;
    SurfelHit_0 _S1426 = _S1424;
    SurfelHit_0 _S1427 = SurfelHit_x24_syn_dadd_0(&_S1425, &_S1426);
    float3  _S1428;
    if(_S1419)
    {
        _S1428 = - dpnormal_1;
    }
    else
    {
        _S1428 = dpnormal_1;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1429;
    (&_S1429)->primal_0 = n_5;
    (&_S1429)->differential_0 = _S1420;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1430;
    (&_S1430)->primal_0 = _S1400.primal_0;
    (&_S1430)->differential_0 = _S1420;
    s_bwd_prop_dot_0(&_S1429, &_S1430, 0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1431 = _S1430;
    float3  _S1432 = _S1429.differential_0 + _S1428;
    float3  _S1433 = _S1420;
    *&((&_S1433)->z) = _S1432.z;
    *&((&_S1433)->y) = _S1432.y;
    *&((&_S1433)->x) = _S1432.x;
    Matrix<float, 3, 3>  _S1434 = makeMatrix<float, 3, 3> (0.0f);
    Matrix<float, 3, 3>  _S1435 = _S1434;
    _S1435[int(2)] = _S1433;
    Matrix<float, 3, 3>  _S1436 = _S1435;
    SurfelHit_0 _S1437 = _S1421;
    (&_S1437)->t_hit_0 = dpdepth_2;
    SurfelHit_0 _S1438 = _S1427;
    SurfelHit_0 _S1439 = _S1437;
    SurfelHit_0 _S1440 = SurfelHit_x24_syn_dadd_0(&_S1438, &_S1439);
    float3  _S1441;
    if(_S1407)
    {
        DiffPair_float_0 _S1442;
        (&_S1442)->primal_0 = _S1408;
        (&_S1442)->differential_0 = 0.0f;
        s_bwd_prop_exp_1(&_S1442, _S1440.g_0);
        float _S1443 = -0.5f * _S1442.differential_0;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S1444;
        (&_S1444)->primal_0 = _S1411;
        (&_S1444)->differential_0 = _S1401;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S1445;
        (&_S1445)->primal_0 = _S1411;
        (&_S1445)->differential_0 = _S1401;
        s_bwd_prop_dot_1(&_S1444, &_S1445, _S1443);
        float2  _S1446 = _S1445.differential_0 + _S1444.differential_0;
        float2  _S1447 = _S1412 * _S1446;
        float2  _S1448 = _S1413 * _S1446;
        float _S1449 = (_S1440.t_hit_0 + _S1448.x + _S1448.y) / _S1409;
        float3  _S1450 = make_float3 (_S1446.x, _S1446.y, - (_S1405 * _S1449));
        float3  _S1451 = make_float3 (_S1447.x, _S1447.y, 0.0f);
        _S1408 = _S1410 * - _S1449;
        _S1428 = _S1451;
        _S1441 = _S1450;
    }
    else
    {
        _S1408 = 0.0f;
        _S1428 = _S1420;
        _S1441 = _S1420;
    }
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1452;
    (&_S1452)->primal_0 = _S1400.primal_0;
    (&_S1452)->differential_0 = _S1420;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1453;
    (&_S1453)->primal_0 = _S1400.primal_0;
    (&_S1453)->differential_0 = _S1420;
    s_bwd_prop_dot_0(&_S1452, &_S1453, 0.0f);
    float3  _S1454 = _S1428 + make_float3 (0.0f, 0.0f, _S1408);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1455;
    (&_S1455)->primal_0 = _S1398.primal_0;
    (&_S1455)->differential_0 = _S1434;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1456;
    (&_S1456)->primal_0 = _S1400.primal_0;
    (&_S1456)->differential_0 = _S1420;
    s_bwd_prop_mul_2(&_S1455, &_S1456, _S1454);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S1457;
    (&_S1457)->primal_0 = _S1398.primal_0;
    (&_S1457)->differential_0 = _S1434;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S1458;
    (&_S1458)->primal_0 = _S1402;
    (&_S1458)->differential_0 = _S1420;
    s_bwd_prop_mul_2(&_S1457, &_S1458, _S1441);
    float3  _S1459 = - _S1458.differential_0;
    float3  _S1460 = _S1453.differential_0 + _S1452.differential_0 + _S1456.differential_0 + _S1431.differential_0;
    dpray_d_3->primal_0 = (*dpray_d_3).primal_0;
    dpray_d_3->differential_0 = _S1460;
    dpray_o_3->primal_0 = (*dpray_o_3).primal_0;
    dpray_o_3->differential_0 = _S1458.differential_0;
    dprgb_2->primal_0 = (*dprgb_2).primal_0;
    dprgb_2->differential_0 = dpout_rgb_2;
    dpopacity_3->primal_0 = (*dpopacity_3).primal_0;
    dpopacity_3->differential_0 = _S1423;
    Matrix<float, 3, 3>  _S1461 = _S1455.differential_0 + _S1457.differential_0 + _S1436;
    dpiscl_rot_3->primal_0 = (*dpiscl_rot_3).primal_0;
    dpiscl_rot_3->differential_0 = _S1461;
    dpmean_3->primal_0 = (*dpmean_3).primal_0;
    dpmean_3->differential_0 = _S1459;
    return;
}

inline __device__ void s_bwd_evaluate_2dgs_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1462, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S1463, DiffPair_float_0 * _S1464, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1465, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1466, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S1467, float3  _S1468, float _S1469, float3  _S1470, float _S1471)
{
    s_bwd_prop_evaluate_2dgs_0(_S1462, _S1463, _S1464, _S1465, _S1466, _S1467, _S1468, _S1469, _S1470, _S1471);
    return;
}

inline __device__ void evaluate_2dgs_vjp(float3  mean_9, Matrix<float, 3, 3>  iscl_rot_9, float opacity_9, float3  rgb_6, float3  ray_o_9, float3  ray_d_9, float v_alpha_1, float3  v_out_rgb_2, float v_depth_2, float3  v_normal_1, float3  * v_mean_3, Matrix<float, 3, 3>  * v_iscl_rot_5, float * v_opacity_3, float3  * v_rgb_2, float3  * v_ray_o_4, float3  * v_ray_d_4)
{
    float3  _S1472 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_mean_3;
    (&dp_mean_3)->primal_0 = mean_9;
    (&dp_mean_3)->differential_0 = _S1472;
    Matrix<float, 3, 3>  _S1473 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 dp_iscl_rot_3;
    (&dp_iscl_rot_3)->primal_0 = iscl_rot_9;
    (&dp_iscl_rot_3)->differential_0 = _S1473;
    DiffPair_float_0 dp_opacity_3;
    (&dp_opacity_3)->primal_0 = opacity_9;
    (&dp_opacity_3)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_rgb_2;
    (&dp_rgb_2)->primal_0 = rgb_6;
    (&dp_rgb_2)->differential_0 = _S1472;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ray_o_3;
    (&dp_ray_o_3)->primal_0 = ray_o_9;
    (&dp_ray_o_3)->differential_0 = _S1472;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ray_d_3;
    (&dp_ray_d_3)->primal_0 = ray_d_9;
    (&dp_ray_d_3)->differential_0 = _S1472;
    s_bwd_evaluate_2dgs_0(&dp_mean_3, &dp_iscl_rot_3, &dp_opacity_3, &dp_rgb_2, &dp_ray_o_3, &dp_ray_d_3, v_out_rgb_2, v_depth_2, v_normal_1, v_alpha_1);
    *v_mean_3 = dp_mean_3.differential_0;
    *v_iscl_rot_5 = dp_iscl_rot_3.differential_0;
    *v_opacity_3 = dp_opacity_3.differential_0;
    *v_rgb_2 = dp_rgb_2.differential_0;
    *v_ray_o_4 = dp_ray_o_3.differential_0;
    *v_ray_d_4 = dp_ray_d_3.differential_0;
    return;
}

inline __device__ float3  max_0(float3  x_30, float3  y_3)
{
    float3  result_14;
    int i_9 = int(0);
    for(;;)
    {
        if(i_9 < int(3))
        {
        }
        else
        {
            break;
        }
        *_slang_vector_get_element_ptr(&result_14, i_9) = (F32_max((_slang_vector_get_element(x_30, i_9)), (_slang_vector_get_element(y_3, i_9))));
        i_9 = i_9 + int(1);
    }
    return result_14;
}

struct FootprintDisc_0
{
    float3  c0_0;
    float3  a_1;
    float3  b_1;
    bool inside_0;
};

inline __device__ bool footprint_planar_cov(float3  mean_10, float4  quat_9, float3  log_scale_0, float opacity_10, Matrix<float, 3, 3>  R_5, float3  t_2, float4  intrins_26, bool surfel_0, int planar_0, float2  * centre_0, float3  * cov_0)
{
    bool ok_3;
    if(planar_0 == int(2))
    {
        float3  scale_8 = exp_0(log_scale_0);
        if(surfel_0)
        {
            *&((&scale_8)->z) = 0.0f;
        }
        float4  _S1474 = normalize_1(quat_9);
        float x_31 = _S1474.y;
        float x2_9 = x_31 * x_31;
        float y2_9 = _S1474.z * _S1474.z;
        float z2_9 = _S1474.w * _S1474.w;
        float xy_9 = _S1474.y * _S1474.z;
        float xz_9 = _S1474.y * _S1474.w;
        float yz_9 = _S1474.z * _S1474.w;
        float wx_9 = _S1474.x * _S1474.y;
        float wy_9 = _S1474.x * _S1474.z;
        float wz_9 = _S1474.x * _S1474.w;
        Matrix<float, 3, 3>  M_3 = mul_1(transpose_0(makeMatrix<float, 3, 3> (1.0f - 2.0f * (y2_9 + z2_9), 2.0f * (xy_9 + wz_9), 2.0f * (xz_9 - wy_9), 2.0f * (xy_9 - wz_9), 1.0f - 2.0f * (x2_9 + z2_9), 2.0f * (yz_9 + wx_9), 2.0f * (xz_9 + wy_9), 2.0f * (yz_9 - wx_9), 1.0f - 2.0f * (x2_9 + y2_9))), makeMatrix<float, 3, 3> (scale_8.x, 0.0f, 0.0f, 0.0f, scale_8.y, 0.0f, 0.0f, 0.0f, scale_8.z));
        Matrix<float, 3, 3>  covar_c_0 = mul_1(mul_1(R_5, mul_1(M_3, transpose_0(M_3))), transpose_0(R_5));
        float3  mean_c_0 = mul_2(R_5, mean_10) + t_2;
        float r2_4 = 2.0f * (F32_log((opacity_10 / 0.00392156885936856f)));
        float fx_10 = intrins_26.x;
        float fy_10 = intrins_26.y;
        *centre_0 = make_float2 (fx_10 * mean_c_0.x + intrins_26.z, fy_10 * mean_c_0.y + intrins_26.w);
        *cov_0 = make_float3 (r2_4) * make_float3 (fx_10 * fx_10 * covar_c_0.rows[int(0)].x, fx_10 * fy_10 * covar_c_0.rows[int(0)].y, fy_10 * fy_10 * covar_c_0.rows[int(1)].y);
        ok_3 = true;
    }
    else
    {
        FootprintDisc_0 _S1475;
        for(;;)
        {
            float4  _S1476 = normalize_1(quat_9);
            float x_32 = _S1476.y;
            float x2_10 = x_32 * x_32;
            float y2_10 = _S1476.z * _S1476.z;
            float z2_10 = _S1476.w * _S1476.w;
            float xy_10 = _S1476.y * _S1476.z;
            float xz_10 = _S1476.y * _S1476.w;
            float yz_10 = _S1476.z * _S1476.w;
            float wx_10 = _S1476.x * _S1476.y;
            float wy_10 = _S1476.x * _S1476.z;
            float wz_10 = _S1476.x * _S1476.w;
            Matrix<float, 3, 3>  axes_0 = transpose_0(mul_1(R_5, transpose_0(makeMatrix<float, 3, 3> (1.0f - 2.0f * (y2_10 + z2_10), 2.0f * (xy_10 + wz_10), 2.0f * (xz_10 - wy_10), 2.0f * (xy_10 - wz_10), 1.0f - 2.0f * (x2_10 + z2_10), 2.0f * (yz_10 + wx_10), 2.0f * (xz_10 + wy_10), 2.0f * (yz_10 - wx_10), 1.0f - 2.0f * (x2_10 + y2_10)))));
            float3  _S1477 = mul_2(R_5, mean_10) + t_2;
            float3  _S1478 = exp_0(log_scale_0);
            float _S1479 = 2.0f * (F32_log((opacity_10 / 0.00392156885936856f)));
            for(;;)
            {
                FootprintDisc_0 d_0;
                (&d_0)->inside_0 = false;
                if(surfel_0)
                {
                    float r_86 = (F32_sqrt((_S1479)));
                    (&d_0)->c0_0 = _S1477;
                    (&d_0)->a_1 = make_float3 (r_86 * _S1478.x) * axes_0.rows[int(0)];
                    (&d_0)->b_1 = make_float3 (r_86 * _S1478.y) * axes_0.rows[int(1)];
                    _S1475 = d_0;
                    break;
                }
                float3  _S1480 = max_0(_S1478, make_float3 (0.00100000004749745f * (F32_max((_S1478.x), ((F32_max((_S1478.y), (_S1478.z))))))));
                float3  h_3 = mul_2(axes_0, _S1477) / _S1480;
                float hh_0 = dot_1(h_3, h_3);
                if(!(hh_0 > (1.0010000467300415f * _S1479)))
                {
                    (&d_0)->inside_0 = true;
                    (&d_0)->c0_0 = _S1477;
                    float3  _S1481 = make_float3 (0.0f);
                    (&d_0)->a_1 = _S1481;
                    (&d_0)->b_1 = _S1481;
                    _S1475 = d_0;
                    break;
                }
                float _S1482 = 1.0f - _S1479 / hh_0;
                float rho_0 = (F32_sqrt((_S1479 * _S1482)));
                float3  h_4 = h_3 * make_float3 ((F32_rsqrt((hh_0))));
                float _S1483 = h_4.z;
                float sg_0;
                if(_S1483 >= 0.0f)
                {
                    sg_0 = 1.0f;
                }
                else
                {
                    sg_0 = -1.0f;
                }
                float p_0 = -1.0f / (sg_0 + _S1483);
                float _S1484 = h_4.x;
                float _S1485 = h_4.y;
                float o_0 = _S1484 * _S1485 * p_0;
                float3  _S1486 = make_float3 (rho_0) * _S1480;
                float3  u1_0 = _S1486 * make_float3 (1.0f + sg_0 * _S1484 * _S1484 * p_0, sg_0 * o_0, - sg_0 * _S1484);
                float3  u2_0 = _S1486 * make_float3 (o_0, sg_0 + _S1485 * _S1485 * p_0, - _S1485);
                (&d_0)->c0_0 = _S1477 * make_float3 (_S1482);
                (&d_0)->a_1 = mul_3(u1_0, axes_0);
                (&d_0)->b_1 = mul_3(u2_0, axes_0);
                _S1475 = d_0;
                break;
            }
            break;
        }
        for(;;)
        {
            *centre_0 = make_float2 (0.0f);
            *cov_0 = make_float3 (0.0f);
            FootprintDisc_0 _S1487 = _S1475;
            float _S1488 = _S1475.c0_0.z;
            FootprintDisc_0 _S1489 = _S1475;
            float _S1490 = _S1475.a_1.z;
            FootprintDisc_0 _S1491 = _S1475;
            float _S1492 = _S1475.b_1.z;
            float den_0 = _S1488 * _S1488 - _S1490 * _S1490 - _S1492 * _S1492;
            if(_S1475.inside_0)
            {
                ok_3 = true;
            }
            else
            {
                if(_S1488 > 0.0f)
                {
                    ok_3 = den_0 > (9.99999997475242708e-07f * _S1488 * _S1488);
                }
                else
                {
                    ok_3 = false;
                }
                ok_3 = !ok_3;
            }
            if(ok_3)
            {
                break;
            }
            float inv_0 = 1.0f / den_0;
            float _S1493 = _S1487.c0_0.x;
            float _S1494 = _S1489.a_1.x;
            float _S1495 = _S1491.b_1.x;
            float _S1496 = _S1487.c0_0.y;
            float _S1497 = _S1489.a_1.y;
            float _S1498 = _S1491.b_1.y;
            float2  ctr_0 = make_float2 (_S1493 * _S1488 - _S1494 * _S1490 - _S1495 * _S1492, _S1496 * _S1488 - _S1497 * _S1490 - _S1498 * _S1492) * make_float2 (inv_0);
            float ab_x_0 = _S1494 * _S1492 - _S1490 * _S1495;
            float ab_y_0 = _S1497 * _S1492 - _S1490 * _S1498;
            float ca_x_0 = _S1493 * _S1490 - _S1488 * _S1494;
            float cb_x_0 = _S1493 * _S1492 - _S1488 * _S1495;
            float ca_y_0 = _S1496 * _S1490 - _S1488 * _S1497;
            float cb_y_0 = _S1496 * _S1492 - _S1488 * _S1498;
            float fx_11 = intrins_26.x;
            float fy_11 = intrins_26.y;
            *centre_0 = make_float2 (fx_11 * ctr_0.x + intrins_26.z, fy_11 * ctr_0.y + intrins_26.w);
            *cov_0 = make_float3 (fx_11 * fx_11 * (F32_max((ca_x_0 * ca_x_0 + cb_x_0 * cb_x_0 - ab_x_0 * ab_x_0), (0.0f))) * inv_0 * inv_0, fx_11 * fy_11 * (ca_x_0 * ca_y_0 + cb_x_0 * cb_y_0 - ab_x_0 * ab_y_0) * inv_0 * inv_0, fy_11 * fy_11 * (F32_max((ca_y_0 * ca_y_0 + cb_y_0 * cb_y_0 - ab_y_0 * ab_y_0), (0.0f))) * inv_0 * inv_0);
            break;
        }
        ok_3 = !ok_3;
    }
    *&(cov_0->x) = *&(cov_0->x) + 0.00999999977648258f;
    *&(cov_0->z) = *&(cov_0->z) + 0.00999999977648258f;
    return ok_3;
}

struct FootprintRay_0
{
    float3  c_3;
    float3  w_0;
    float f_0;
};

inline __device__ int footprint_rays_coverage(float3  mean_11, Matrix<float, 3, 3>  M_4, float opacity_11, bool surfel_1, float3  o_1, float3  d0_0, float3  d1_0, float3  d2_0, float3  d3_0)
{
    bool _S1499;
    if((dot_1(d0_0, d0_0)) == 0.0f)
    {
        _S1499 = true;
    }
    else
    {
        _S1499 = (dot_1(d1_0, d1_0)) == 0.0f;
    }
    if(_S1499)
    {
        _S1499 = true;
    }
    else
    {
        _S1499 = (dot_1(d2_0, d2_0)) == 0.0f;
    }
    if(_S1499)
    {
        _S1499 = true;
    }
    else
    {
        _S1499 = (dot_1(d3_0, d3_0)) == 0.0f;
    }
    if(_S1499)
    {
        return int(1);
    }
    int _S1500;
    float3  _S1501 = mul_2(M_4, o_1 - mean_11);
    float3  _S1502 = mul_2(M_4, d0_0);
    float3  _S1503 = mul_2(M_4, d1_0);
    float3  _S1504 = mul_2(M_4, d2_0);
    float3  _S1505 = mul_2(M_4, d3_0);
    float _S1506 = 2.0f * (F32_log((opacity_11 / 0.00392156885936856f)));
    for(;;)
    {
        float3  _S1507 = _S1502 * make_float3 ((F32_rsqrt(((F32_max((dot_1(_S1502, _S1502)), (9.99999991097578965e-38f)))))));
        FootprintRay_0 k_15;
        if(surfel_1)
        {
            float _S1508 = _S1507.z;
            float _S1509 = _S1501.z;
            (&k_15)->c_3 = make_float3 (_S1501.x * _S1508 - _S1509 * _S1507.x, _S1501.y * _S1508 - _S1509 * _S1507.y, 0.0f);
            (&k_15)->w_0 = make_float3 (_S1508, 0.0f, 0.0f);
            (&k_15)->f_0 = - _S1509 * _S1508;
        }
        else
        {
            (&k_15)->c_3 = cross_0(_S1501, _S1507);
            (&k_15)->w_0 = _S1507;
            (&k_15)->f_0 = - dot_1(_S1501, _S1507);
        }
        FootprintRay_0 _S1510 = k_15;
        float3  _S1511 = _S1503 * make_float3 ((F32_rsqrt(((F32_max((dot_1(_S1503, _S1503)), (9.99999991097578965e-38f)))))));
        FootprintRay_0 k_16;
        if(surfel_1)
        {
            float _S1512 = _S1511.z;
            float _S1513 = _S1501.z;
            (&k_16)->c_3 = make_float3 (_S1501.x * _S1512 - _S1513 * _S1511.x, _S1501.y * _S1512 - _S1513 * _S1511.y, 0.0f);
            (&k_16)->w_0 = make_float3 (_S1512, 0.0f, 0.0f);
            (&k_16)->f_0 = - _S1513 * _S1512;
        }
        else
        {
            (&k_16)->c_3 = cross_0(_S1501, _S1511);
            (&k_16)->w_0 = _S1511;
            (&k_16)->f_0 = - dot_1(_S1501, _S1511);
        }
        FootprintRay_0 _S1514 = k_16;
        float3  _S1515 = _S1504 * make_float3 ((F32_rsqrt(((F32_max((dot_1(_S1504, _S1504)), (9.99999991097578965e-38f)))))));
        FootprintRay_0 k_17;
        if(surfel_1)
        {
            float _S1516 = _S1515.z;
            float _S1517 = _S1501.z;
            (&k_17)->c_3 = make_float3 (_S1501.x * _S1516 - _S1517 * _S1515.x, _S1501.y * _S1516 - _S1517 * _S1515.y, 0.0f);
            (&k_17)->w_0 = make_float3 (_S1516, 0.0f, 0.0f);
            (&k_17)->f_0 = - _S1517 * _S1516;
        }
        else
        {
            (&k_17)->c_3 = cross_0(_S1501, _S1515);
            (&k_17)->w_0 = _S1515;
            (&k_17)->f_0 = - dot_1(_S1501, _S1515);
        }
        FootprintRay_0 _S1518 = k_17;
        float3  _S1519 = _S1505 * make_float3 ((F32_rsqrt(((F32_max((dot_1(_S1505, _S1505)), (9.99999991097578965e-38f)))))));
        FootprintRay_0 k_18;
        if(surfel_1)
        {
            float _S1520 = _S1519.z;
            float _S1521 = _S1501.z;
            (&k_18)->c_3 = make_float3 (_S1501.x * _S1520 - _S1521 * _S1519.x, _S1501.y * _S1520 - _S1521 * _S1519.y, 0.0f);
            (&k_18)->w_0 = make_float3 (_S1520, 0.0f, 0.0f);
            (&k_18)->f_0 = - _S1521 * _S1520;
        }
        else
        {
            (&k_18)->c_3 = cross_0(_S1501, _S1519);
            (&k_18)->w_0 = _S1519;
            (&k_18)->f_0 = - dot_1(_S1501, _S1519);
        }
        FootprintRay_0 _S1522 = k_18;
        float3  _S1523 = _S1510.c_3;
        float3  _S1524 = _S1510.w_0;
        float3  _S1525 = _S1514.c_3;
        float3  _S1526 = _S1514.w_0;
        float _S1527 = dot_1(_S1525, _S1525) - _S1506 * dot_1(_S1526, _S1526);
        float3  _S1528 = _S1518.c_3;
        float3  _S1529 = _S1518.w_0;
        float _S1530 = dot_1(_S1528, _S1528) - _S1506 * dot_1(_S1529, _S1529);
        float3  _S1531 = _S1522.c_3;
        float3  _S1532 = _S1522.w_0;
        float _S1533 = dot_1(_S1531, _S1531) - _S1506 * dot_1(_S1532, _S1532);
        if((dot_1(_S1523, _S1523) - _S1506 * dot_1(_S1524, _S1524)) <= 0.0f)
        {
            _S1499 = (_S1510.f_0) > 0.0f;
        }
        else
        {
            _S1499 = false;
        }
        int _S1534 = int(_S1499);
        if(_S1527 <= 0.0f)
        {
            _S1499 = (_S1514.f_0) > 0.0f;
        }
        else
        {
            _S1499 = false;
        }
        int _S1535 = _S1534 + int(_S1499);
        if(_S1530 <= 0.0f)
        {
            _S1499 = (_S1518.f_0) > 0.0f;
        }
        else
        {
            _S1499 = false;
        }
        int _S1536 = _S1535 + int(_S1499);
        if(_S1533 <= 0.0f)
        {
            _S1499 = (_S1522.f_0) > 0.0f;
        }
        else
        {
            _S1499 = false;
        }
        int inside_1 = _S1536 + int(_S1499);
        if(inside_1 != int(0))
        {
            if(inside_1 == int(4))
            {
                _S1500 = int(2);
            }
            else
            {
                _S1500 = int(1);
            }
            break;
        }
        for(;;)
        {
            float3  dc_0 = _S1514.c_3 - _S1510.c_3;
            float3  dw_0 = _S1514.w_0 - _S1510.w_0;
            float lin_0 = dot_1(_S1510.c_3, dc_0) - _S1506 * dot_1(_S1510.w_0, dw_0);
            float quad_0 = dot_1(dc_0, dc_0) - _S1506 * dot_1(dw_0, dw_0);
            if(lin_0 < 0.0f)
            {
                _S1499 = (- lin_0) < quad_0;
            }
            else
            {
                _S1499 = false;
            }
            if(!_S1499)
            {
                _S1499 = false;
                break;
            }
            float s_12 = - lin_0 / quad_0;
            float3  c_4 = _S1510.c_3 + make_float3 (s_12) * dc_0;
            float3  w_1 = _S1510.w_0 + make_float3 (s_12) * dw_0;
            if((dot_1(c_4, c_4) - _S1506 * dot_1(w_1, w_1)) <= 0.0f)
            {
                _S1499 = (_S1510.f_0 + s_12 * (_S1514.f_0 - _S1510.f_0)) > 0.0f;
            }
            else
            {
                _S1499 = false;
            }
            break;
        }
        if(_S1499)
        {
            _S1499 = true;
        }
        else
        {
            for(;;)
            {
                float3  dc_1 = _S1518.c_3 - _S1514.c_3;
                float3  dw_1 = _S1518.w_0 - _S1514.w_0;
                float lin_1 = dot_1(_S1514.c_3, dc_1) - _S1506 * dot_1(_S1514.w_0, dw_1);
                float quad_1 = dot_1(dc_1, dc_1) - _S1506 * dot_1(dw_1, dw_1);
                if(lin_1 < 0.0f)
                {
                    _S1499 = (- lin_1) < quad_1;
                }
                else
                {
                    _S1499 = false;
                }
                if(!_S1499)
                {
                    _S1499 = false;
                    break;
                }
                float s_13 = - lin_1 / quad_1;
                float3  c_5 = _S1514.c_3 + make_float3 (s_13) * dc_1;
                float3  w_2 = _S1514.w_0 + make_float3 (s_13) * dw_1;
                if((dot_1(c_5, c_5) - _S1506 * dot_1(w_2, w_2)) <= 0.0f)
                {
                    _S1499 = (_S1514.f_0 + s_13 * (_S1518.f_0 - _S1514.f_0)) > 0.0f;
                }
                else
                {
                    _S1499 = false;
                }
                break;
            }
        }
        if(_S1499)
        {
            _S1499 = true;
        }
        else
        {
            for(;;)
            {
                float3  dc_2 = _S1522.c_3 - _S1518.c_3;
                float3  dw_2 = _S1522.w_0 - _S1518.w_0;
                float lin_2 = dot_1(_S1518.c_3, dc_2) - _S1506 * dot_1(_S1518.w_0, dw_2);
                float quad_2 = dot_1(dc_2, dc_2) - _S1506 * dot_1(dw_2, dw_2);
                if(lin_2 < 0.0f)
                {
                    _S1499 = (- lin_2) < quad_2;
                }
                else
                {
                    _S1499 = false;
                }
                if(!_S1499)
                {
                    _S1499 = false;
                    break;
                }
                float s_14 = - lin_2 / quad_2;
                float3  c_6 = _S1518.c_3 + make_float3 (s_14) * dc_2;
                float3  w_3 = _S1518.w_0 + make_float3 (s_14) * dw_2;
                if((dot_1(c_6, c_6) - _S1506 * dot_1(w_3, w_3)) <= 0.0f)
                {
                    _S1499 = (_S1518.f_0 + s_14 * (_S1522.f_0 - _S1518.f_0)) > 0.0f;
                }
                else
                {
                    _S1499 = false;
                }
                break;
            }
        }
        if(_S1499)
        {
            _S1499 = true;
        }
        else
        {
            for(;;)
            {
                float3  dc_3 = _S1510.c_3 - _S1522.c_3;
                float3  dw_3 = _S1510.w_0 - _S1522.w_0;
                float lin_3 = dot_1(_S1522.c_3, dc_3) - _S1506 * dot_1(_S1522.w_0, dw_3);
                float quad_3 = dot_1(dc_3, dc_3) - _S1506 * dot_1(dw_3, dw_3);
                if(lin_3 < 0.0f)
                {
                    _S1499 = (- lin_3) < quad_3;
                }
                else
                {
                    _S1499 = false;
                }
                if(!_S1499)
                {
                    _S1499 = false;
                    break;
                }
                float s_15 = - lin_3 / quad_3;
                float3  c_7 = _S1522.c_3 + make_float3 (s_15) * dc_3;
                float3  w_4 = _S1522.w_0 + make_float3 (s_15) * dw_3;
                if((dot_1(c_7, c_7) - _S1506 * dot_1(w_4, w_4)) <= 0.0f)
                {
                    _S1499 = (_S1522.f_0 + s_15 * (_S1510.f_0 - _S1522.f_0)) > 0.0f;
                }
                else
                {
                    _S1499 = false;
                }
                break;
            }
        }
        if(_S1499)
        {
            _S1500 = int(1);
            break;
        }
        if((dot_1(cross_0(_S1502, _S1503), _S1501)) <= 0.0f)
        {
            _S1499 = (dot_1(cross_0(_S1503, _S1504), _S1501)) <= 0.0f;
        }
        else
        {
            _S1499 = false;
        }
        if(_S1499)
        {
            _S1499 = (dot_1(cross_0(_S1504, _S1505), _S1501)) <= 0.0f;
        }
        else
        {
            _S1499 = false;
        }
        if(_S1499)
        {
            _S1499 = (dot_1(cross_0(_S1505, _S1502), _S1501)) <= 0.0f;
        }
        else
        {
            _S1499 = false;
        }
        _S1500 = int(_S1499);
        break;
    }
    return _S1500;
}

inline __device__ bool aabb_reaches_micro_tile(float4  aabb_0, float bx0_0, float by0_0, float size_0)
{
    bool _S1537;
    if((aabb_0.x) <= (bx0_0 + size_0 - 0.5f))
    {
        _S1537 = (aabb_0.z) >= (bx0_0 + 0.5f);
    }
    else
    {
        _S1537 = false;
    }
    if(_S1537)
    {
        _S1537 = (aabb_0.y) <= (by0_0 + size_0 - 0.5f);
    }
    else
    {
        _S1537 = false;
    }
    if(_S1537)
    {
        _S1537 = (aabb_0.w) >= (by0_0 + 0.5f);
    }
    else
    {
        _S1537 = false;
    }
    return _S1537;
}

inline __device__ float2  abs_0(float2  x_33)
{
    float2  result_15;
    int i_10 = int(0);
    for(;;)
    {
        if(i_10 < int(2))
        {
        }
        else
        {
            break;
        }
        *_slang_vector_get_element_ptr(&result_15, i_10) = (F32_abs((_slang_vector_get_element(x_33, i_10))));
        i_10 = i_10 + int(1);
    }
    return result_15;
}

inline __device__ bool slab_misses_box(float3  slab_0, float2  centre_1, float2  half_extent_0)
{
    float2  _S1538 = float2 {slab_0.x, slab_0.y};
    return (F32_abs((dot_0(_S1538, centre_1) - slab_0.z))) > (1.0f + dot_0(abs_0(_S1538), half_extent_0));
}

inline __device__ float view_radius_3dgs(float3  mean_12, float3  log_scale_1, float logit_opacity_0, float3  campos_0)
{
    float radius_0 = (F32_exp(((F32_max((log_scale_1.x), ((F32_max((log_scale_1.y), (log_scale_1.z))))))))) * (F32_sqrt((2.0f * (F32_log(((F32_max((255.0f / (1.0f + (F32_exp((- logit_opacity_0))))), (1.0f)))))))));
    float dist_0 = length_1(mean_12 - campos_0);
    return radius_0 / ((F32_max((dist_0), (radius_0))) + (F32_sqrt(((F32_max((dist_0 * dist_0 - radius_0 * radius_0), (0.0f)))))));
}

