#pragma once

#include "generated/slang.cuh"

inline __device__ Matrix<float, 3, 3>  transpose_0(Matrix<float, 3, 3>  x_0)
{
    Matrix<float, 3, 3>  result_0;
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
            *_slang_vector_get_element_ptr(((&result_0)->rows + (r_0)), c_0) = _slang_vector_get_element(x_0.rows[c_0], r_0);
            c_0 = c_0 + int(1);
        }
        r_0 = r_0 + int(1);
    }
    return result_0;
}

inline __device__ Matrix<float, 2, 2>  transpose_1(Matrix<float, 2, 2>  x_1)
{
    Matrix<float, 2, 2>  result_1;
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
            *_slang_vector_get_element_ptr(((&result_1)->rows + (r_1)), c_1) = _slang_vector_get_element(x_1.rows[c_1], r_1);
            c_1 = c_1 + int(1);
        }
        r_1 = r_1 + int(1);
    }
    return result_1;
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
    Matrix<float, 3, 3>  result_2;
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
            *_slang_vector_get_element_ptr(((&result_2)->rows + (r_2)), c_2) = sum_0;
            c_2 = c_2 + int(1);
        }
        r_2 = r_2 + int(1);
    }
    return result_2;
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
    float3  result_3;
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
        *_slang_vector_get_element_ptr(&result_3, i_1) = sum_8;
        i_1 = i_1 + int(1);
    }
    return result_3;
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

inline __device__ void _d_dot_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_2, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpy_1, float dOut_2)
{
    float3  x_d_result_0;
    *&((&x_d_result_0)->x) = (*dpy_1).primal_0.x * dOut_2;
    float3  y_d_result_0;
    *&((&y_d_result_0)->x) = (*dpx_2).primal_0.x * dOut_2;
    *&((&x_d_result_0)->y) = (*dpy_1).primal_0.y * dOut_2;
    *&((&y_d_result_0)->y) = (*dpx_2).primal_0.y * dOut_2;
    *&((&x_d_result_0)->z) = (*dpy_1).primal_0.z * dOut_2;
    *&((&y_d_result_0)->z) = (*dpx_2).primal_0.z * dOut_2;
    dpx_2->primal_0 = (*dpx_2).primal_0;
    dpx_2->differential_0 = x_d_result_0;
    dpy_1->primal_0 = (*dpy_1).primal_0;
    dpy_1->differential_0 = y_d_result_0;
    return;
}

inline __device__ float dot_0(float2  x_7, float2  y_0)
{
    int i_2 = int(0);
    float result_4 = 0.0f;
    for(;;)
    {
        if(i_2 < int(2))
        {
        }
        else
        {
            break;
        }
        float result_5 = result_4 + _slang_vector_get_element(x_7, i_2) * _slang_vector_get_element(y_0, i_2);
        i_2 = i_2 + int(1);
        result_4 = result_5;
    }
    return result_4;
}

inline __device__ float dot_1(float3  x_8, float3  y_1)
{
    int i_3 = int(0);
    float result_6 = 0.0f;
    for(;;)
    {
        if(i_3 < int(3))
        {
        }
        else
        {
            break;
        }
        float result_7 = result_6 + _slang_vector_get_element(x_8, i_3) * _slang_vector_get_element(y_1, i_3);
        i_3 = i_3 + int(1);
        result_6 = result_7;
    }
    return result_6;
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

struct DiffPair_vectorx3Cfloatx2C2x3E_0
{
    float2  primal_0;
    float2  differential_0;
};

inline __device__ DiffPair_float_0 s_fwd_length_impl_0(DiffPair_vectorx3Cfloatx2C2x3E_0 * dpx_3)
{
    float _S7 = *&((&dpx_3->differential_0)->x) * *&((&dpx_3->primal_0)->x);
    float _S8 = *&((&dpx_3->differential_0)->y) * *&((&dpx_3->primal_0)->y);
    float s_diff_len_0 = _S7 + _S7 + (_S8 + _S8);
    DiffPair_float_0 _S9;
    (&_S9)->primal_0 = *&((&dpx_3->primal_0)->x) * *&((&dpx_3->primal_0)->x) + *&((&dpx_3->primal_0)->y) * *&((&dpx_3->primal_0)->y);
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

inline __device__ DiffPair_float_0 _d_sin_0(DiffPair_float_0 * dpx_4)
{
    DiffPair_float_0 _S57 = { (F32_sin((dpx_4->primal_0))), (F32_cos((dpx_4->primal_0))) * dpx_4->differential_0 };
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
        i_4 = i_4 + int(1);
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
        i_5 = i_5 + int(1);
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

inline __device__ float3  normalize_0(float3  x_11)
{
    return x_11 / make_float3 (length_1(x_11));
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

inline __device__ bool is_valid_distortion_opencv(float2  uv_16, FixedArray<float, 4>  dist_coeffs_14)
{
    float2  _S282 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S283;
    (&_S283)->primal_0 = uv_16;
    (&_S283)->differential_0 = _S282;
    FixedArray<float, 4>  _S284 = dist_coeffs_14;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S285 = s_fwd_DistOpenCV_distort_0(&_S283, &_S284);
    float2  _S286 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S287;
    (&_S287)->primal_0 = uv_16;
    (&_S287)->differential_0 = _S286;
    FixedArray<float, 4>  _S288 = dist_coeffs_14;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S289 = s_fwd_DistOpenCV_distort_0(&_S287, &_S288);
    Matrix<float, 2, 2>  _S290 = transpose_1(makeMatrix<float, 2, 2> (_S285.differential_0, _S289.differential_0));
    float _S291 = (F32_min((determinant_0(_S290)), ((F32_min((_S290.rows[int(0)].x), (_S290.rows[int(1)].y))))));
    bool _S292;
    if(_S291 > 0.25f)
    {
        _S292 = _S291 < 4.0f;
    }
    else
    {
        _S292 = false;
    }
    if(_S292)
    {
        FixedArray<float, 4>  _S293 = dist_coeffs_14;
        float2  _S294 = DistOpenCV_distort_0(uv_16, &_S293);
        _S292 = (dot_0(uv_16, _S294)) >= 0.0f;
    }
    else
    {
        _S292 = false;
    }
    return _S292;
}

inline __device__ bool persp_proj_nav_opencv(float3  p_view_8, float4  intrins_8, FixedArray<float, 4>  dist_coeffs_15, float2  * uv_17)
{
    bool _S295;
    for(;;)
    {
        float2  _S296 = float2 {p_view_8.x, p_view_8.y};
        float _S297 = p_view_8.z;
        float2  uv0_1 = _S296 / make_float2 (_S297);
        if(_S297 < 0.0f)
        {
            _S295 = true;
        }
        else
        {
            float2  _S298 = make_float2 (1.0f, 0.0f);
            DiffPair_vectorx3Cfloatx2C2x3E_0 _S299;
            (&_S299)->primal_0 = uv0_1;
            (&_S299)->differential_0 = _S298;
            FixedArray<float, 4>  _S300 = dist_coeffs_15;
            DiffPair_vectorx3Cfloatx2C2x3E_0 _S301 = s_fwd_DistOpenCV_distort_0(&_S299, &_S300);
            float2  _S302 = make_float2 (0.0f, 1.0f);
            DiffPair_vectorx3Cfloatx2C2x3E_0 _S303;
            (&_S303)->primal_0 = uv0_1;
            (&_S303)->differential_0 = _S302;
            FixedArray<float, 4>  _S304 = dist_coeffs_15;
            DiffPair_vectorx3Cfloatx2C2x3E_0 _S305 = s_fwd_DistOpenCV_distort_0(&_S303, &_S304);
            Matrix<float, 2, 2>  _S306 = transpose_1(makeMatrix<float, 2, 2> (_S301.differential_0, _S305.differential_0));
            float _S307 = (F32_min((determinant_0(_S306)), ((F32_min((_S306.rows[int(0)].x), (_S306.rows[int(1)].y))))));
            if(_S307 > 0.25f)
            {
                _S295 = _S307 < 4.0f;
            }
            else
            {
                _S295 = false;
            }
            if(_S295)
            {
                FixedArray<float, 4>  _S308 = dist_coeffs_15;
                float2  _S309 = DistOpenCV_distort_0(uv0_1, &_S308);
                _S295 = (dot_0(uv0_1, _S309)) >= 0.0f;
            }
            else
            {
                _S295 = false;
            }
            _S295 = !_S295;
        }
        if(_S295)
        {
            *uv_17 = uv0_1;
            _S295 = false;
            break;
        }
        float2  uv_18 = _S296 / make_float2 (_S297);
        FixedArray<float, 4>  _S310 = dist_coeffs_15;
        float2  _S311 = DistOpenCV_distort_0(uv_18, &_S310);
        *uv_17 = make_float2 (intrins_8.x * _S311.x + intrins_8.z, intrins_8.y * _S311.y + intrins_8.w);
        _S295 = true;
        break;
    }
    return _S295;
}

inline __device__ bool fisheye_proj_nav_opencv(float3  p_view_9, float4  intrins_9, FixedArray<float, 4>  dist_coeffs_16, float2  * uv_19)
{
    bool _S312;
    for(;;)
    {
        float2  _S313 = float2 {p_view_9.x, p_view_9.y};
        float r_15 = length_0(_S313);
        float _S314 = p_view_9.z;
        float theta_4 = (F32_atan2((r_15), (_S314)));
        bool _S315 = theta_4 < 0.00100000004749745f;
        float k_5;
        if(_S315)
        {
            k_5 = (1.0f - theta_4 * theta_4 / 3.0f) / _S314;
        }
        else
        {
            k_5 = theta_4 / r_15;
        }
        float2  _S316 = _S313 * make_float2 (k_5);
        float2  _S317 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S318;
        (&_S318)->primal_0 = _S316;
        (&_S318)->differential_0 = _S317;
        FixedArray<float, 4>  _S319 = dist_coeffs_16;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S320 = s_fwd_DistOpenCV_distort_0(&_S318, &_S319);
        float2  _S321 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S322;
        (&_S322)->primal_0 = _S316;
        (&_S322)->differential_0 = _S321;
        FixedArray<float, 4>  _S323 = dist_coeffs_16;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S324 = s_fwd_DistOpenCV_distort_0(&_S322, &_S323);
        Matrix<float, 2, 2>  _S325 = transpose_1(makeMatrix<float, 2, 2> (_S320.differential_0, _S324.differential_0));
        float _S326 = (F32_min((determinant_0(_S325)), ((F32_min((_S325.rows[int(0)].x), (_S325.rows[int(1)].y))))));
        if(_S326 > 0.25f)
        {
            _S312 = _S326 < 4.0f;
        }
        else
        {
            _S312 = false;
        }
        if(_S312)
        {
            FixedArray<float, 4>  _S327 = dist_coeffs_16;
            float2  _S328 = DistOpenCV_distort_0(_S316, &_S327);
            _S312 = (dot_0(_S316, _S328)) >= 0.0f;
        }
        else
        {
            _S312 = false;
        }
        if(!_S312)
        {
            *uv_19 = _S316;
            _S312 = false;
            break;
        }
        if(_S315)
        {
            k_5 = (1.0f - theta_4 * theta_4 / 3.0f) / _S314;
        }
        else
        {
            k_5 = theta_4 / r_15;
        }
        float2  _S329 = _S313 * make_float2 (k_5);
        FixedArray<float, 4>  _S330 = dist_coeffs_16;
        float2  _S331 = DistOpenCV_distort_0(_S329, &_S330);
        *uv_19 = make_float2 (intrins_9.x * _S331.x + intrins_9.z, intrins_9.y * _S331.y + intrins_9.w);
        _S312 = true;
        break;
    }
    return _S312;
}

inline __device__ bool equisolid_proj_nav_opencv(float3  p_view_10, float4  intrins_10, FixedArray<float, 4>  dist_coeffs_17, float2  * uv_20)
{
    bool _S332;
    for(;;)
    {
        float2  _S333 = float2 {p_view_10.x, p_view_10.y};
        float r_16 = length_0(_S333);
        float _S334 = p_view_10.z;
        float theta_5 = (F32_atan2((r_16), (_S334)));
        bool _S335 = r_16 < 9.99999997475242708e-07f;
        float k_6;
        if(_S335)
        {
            k_6 = (1.0f - theta_5 * theta_5 / 24.0f) / _S334;
        }
        else
        {
            k_6 = 2.0f * (F32_sin((0.5f * theta_5))) / r_16;
        }
        float2  _S336 = _S333 * make_float2 (k_6);
        float2  _S337 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S338;
        (&_S338)->primal_0 = _S336;
        (&_S338)->differential_0 = _S337;
        FixedArray<float, 4>  _S339 = dist_coeffs_17;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S340 = s_fwd_DistOpenCV_distort_0(&_S338, &_S339);
        float2  _S341 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S342;
        (&_S342)->primal_0 = _S336;
        (&_S342)->differential_0 = _S341;
        FixedArray<float, 4>  _S343 = dist_coeffs_17;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S344 = s_fwd_DistOpenCV_distort_0(&_S342, &_S343);
        Matrix<float, 2, 2>  _S345 = transpose_1(makeMatrix<float, 2, 2> (_S340.differential_0, _S344.differential_0));
        float _S346 = (F32_min((determinant_0(_S345)), ((F32_min((_S345.rows[int(0)].x), (_S345.rows[int(1)].y))))));
        if(_S346 > 0.25f)
        {
            _S332 = _S346 < 4.0f;
        }
        else
        {
            _S332 = false;
        }
        if(_S332)
        {
            FixedArray<float, 4>  _S347 = dist_coeffs_17;
            float2  _S348 = DistOpenCV_distort_0(_S336, &_S347);
            _S332 = (dot_0(_S336, _S348)) >= 0.0f;
        }
        else
        {
            _S332 = false;
        }
        if(!_S332)
        {
            *uv_20 = _S336;
            _S332 = false;
            break;
        }
        if(_S335)
        {
            k_6 = (1.0f - theta_5 * theta_5 / 24.0f) / _S334;
        }
        else
        {
            k_6 = 2.0f * (F32_sin((0.5f * theta_5))) / r_16;
        }
        float2  _S349 = _S333 * make_float2 (k_6);
        FixedArray<float, 4>  _S350 = dist_coeffs_17;
        float2  _S351 = DistOpenCV_distort_0(_S349, &_S350);
        *uv_20 = make_float2 (intrins_10.x * _S351.x + intrins_10.z, intrins_10.y * _S351.y + intrins_10.w);
        _S332 = true;
        break;
    }
    return _S332;
}

inline __device__ Matrix<float, 2, 3>  persp_proj_jac_opencv(float3  p_view_11, float4  intrins_11, FixedArray<float, 4>  dist_coeffs_18)
{
    float2  _S352 = float2 {p_view_11.x, p_view_11.y};
    float _S353 = p_view_11.z;
    float2  _S354 = _S352 * make_float2 (0.0f);
    float _S355 = _S353 * _S353;
    float2  s_diff_uv_3 = (make_float2 (1.0f, 0.0f) * make_float2 (_S353) - _S354) / make_float2 (_S355);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S356;
    (&_S356)->primal_0 = _S352 / make_float2 (_S353);
    (&_S356)->differential_0 = s_diff_uv_3;
    FixedArray<float, 4>  _S357 = dist_coeffs_18;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S358 = s_fwd_DistOpenCV_distort_0(&_S356, &_S357);
    float fx_4 = intrins_11.x;
    float fy_4 = intrins_11.y;
    float _S359 = _S358.differential_0.y * fy_4;
    Matrix<float, 2, 3>  J_4;
    *&(((&J_4)->rows + (int(0)))->x) = _S358.differential_0.x * fx_4;
    *&(((&J_4)->rows + (int(1)))->x) = _S359;
    float2  s_diff_uv_4 = (make_float2 (0.0f, 1.0f) * make_float2 (_S353) - _S354) / make_float2 (_S355);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S360;
    (&_S360)->primal_0 = _S352 / make_float2 (_S353);
    (&_S360)->differential_0 = s_diff_uv_4;
    FixedArray<float, 4>  _S361 = dist_coeffs_18;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S362 = s_fwd_DistOpenCV_distort_0(&_S360, &_S361);
    float _S363 = _S362.differential_0.y * fy_4;
    *&(((&J_4)->rows + (int(0)))->y) = _S362.differential_0.x * fx_4;
    *&(((&J_4)->rows + (int(1)))->y) = _S363;
    float2  s_diff_uv_5 = (make_float2 (0.0f, 0.0f) * make_float2 (_S353) - _S352) / make_float2 (_S355);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S364;
    (&_S364)->primal_0 = _S352 / make_float2 (_S353);
    (&_S364)->differential_0 = s_diff_uv_5;
    FixedArray<float, 4>  _S365 = dist_coeffs_18;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S366 = s_fwd_DistOpenCV_distort_0(&_S364, &_S365);
    float _S367 = _S366.differential_0.y * fy_4;
    *&(((&J_4)->rows + (int(0)))->z) = _S366.differential_0.x * fx_4;
    *&(((&J_4)->rows + (int(1)))->z) = _S367;
    return J_4;
}

inline __device__ Matrix<float, 2, 3>  fisheye_proj_jac_opencv(float3  p_view_12, float4  intrins_12, FixedArray<float, 4>  dist_coeffs_19)
{
    Matrix<float, 2, 3>  J_5;
    float2  _S368 = float2 {p_view_12.x, p_view_12.y};
    float2  _S369 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S370;
    (&_S370)->primal_0 = _S368;
    (&_S370)->differential_0 = _S369;
    DiffPair_float_0 _S371 = s_fwd_length_impl_0(&_S370);
    float _S372 = p_view_12.z;
    DiffPair_float_0 _S373;
    (&_S373)->primal_0 = _S371.primal_0;
    (&_S373)->differential_0 = _S371.differential_0;
    DiffPair_float_0 _S374;
    (&_S374)->primal_0 = _S372;
    (&_S374)->differential_0 = 0.0f;
    DiffPair_float_0 _S375 = _d_atan2_0(&_S373, &_S374);
    float k_7;
    float s_diff_k_2;
    if((_S375.primal_0) < 0.00100000004749745f)
    {
        float _S376 = _S375.differential_0 * _S375.primal_0;
        float _S377 = 1.0f - _S375.primal_0 * _S375.primal_0 / 3.0f;
        float _S378 = ((0.0f - (_S376 + _S376) * 0.3333333432674408f) * _S372 - _S377 * 0.0f) / (_S372 * _S372);
        k_7 = _S377 / _S372;
        s_diff_k_2 = _S378;
    }
    else
    {
        float _S379 = (_S375.differential_0 * _S371.primal_0 - _S375.primal_0 * _S371.differential_0) / (_S371.primal_0 * _S371.primal_0);
        k_7 = _S375.primal_0 / _S371.primal_0;
        s_diff_k_2 = _S379;
    }
    float2  _S380 = _S368 * make_float2 (k_7);
    float2  _S381 = _S369 * make_float2 (k_7) + make_float2 (s_diff_k_2) * _S368;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S382;
    (&_S382)->primal_0 = _S380;
    (&_S382)->differential_0 = _S381;
    FixedArray<float, 4>  _S383 = dist_coeffs_19;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S384 = s_fwd_DistOpenCV_distort_0(&_S382, &_S383);
    float fx_5 = intrins_12.x;
    float fy_5 = intrins_12.y;
    float _S385 = _S384.differential_0.y * fy_5;
    *&(((&J_5)->rows + (int(0)))->x) = _S384.differential_0.x * fx_5;
    *&(((&J_5)->rows + (int(1)))->x) = _S385;
    float2  _S386 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S387;
    (&_S387)->primal_0 = _S368;
    (&_S387)->differential_0 = _S386;
    DiffPair_float_0 _S388 = s_fwd_length_impl_0(&_S387);
    DiffPair_float_0 _S389;
    (&_S389)->primal_0 = _S388.primal_0;
    (&_S389)->differential_0 = _S388.differential_0;
    DiffPair_float_0 _S390;
    (&_S390)->primal_0 = _S372;
    (&_S390)->differential_0 = 0.0f;
    DiffPair_float_0 _S391 = _d_atan2_0(&_S389, &_S390);
    if((_S391.primal_0) < 0.00100000004749745f)
    {
        float _S392 = _S391.differential_0 * _S391.primal_0;
        float _S393 = 1.0f - _S391.primal_0 * _S391.primal_0 / 3.0f;
        float _S394 = ((0.0f - (_S392 + _S392) * 0.3333333432674408f) * _S372 - _S393 * 0.0f) / (_S372 * _S372);
        k_7 = _S393 / _S372;
        s_diff_k_2 = _S394;
    }
    else
    {
        float _S395 = (_S391.differential_0 * _S388.primal_0 - _S391.primal_0 * _S388.differential_0) / (_S388.primal_0 * _S388.primal_0);
        k_7 = _S391.primal_0 / _S388.primal_0;
        s_diff_k_2 = _S395;
    }
    float2  _S396 = _S368 * make_float2 (k_7);
    float2  _S397 = _S386 * make_float2 (k_7) + make_float2 (s_diff_k_2) * _S368;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S398;
    (&_S398)->primal_0 = _S396;
    (&_S398)->differential_0 = _S397;
    FixedArray<float, 4>  _S399 = dist_coeffs_19;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S400 = s_fwd_DistOpenCV_distort_0(&_S398, &_S399);
    float _S401 = _S400.differential_0.y * fy_5;
    *&(((&J_5)->rows + (int(0)))->y) = _S400.differential_0.x * fx_5;
    *&(((&J_5)->rows + (int(1)))->y) = _S401;
    float2  _S402 = make_float2 (0.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S403;
    (&_S403)->primal_0 = _S368;
    (&_S403)->differential_0 = _S402;
    DiffPair_float_0 _S404 = s_fwd_length_impl_0(&_S403);
    DiffPair_float_0 _S405;
    (&_S405)->primal_0 = _S404.primal_0;
    (&_S405)->differential_0 = _S404.differential_0;
    DiffPair_float_0 _S406;
    (&_S406)->primal_0 = _S372;
    (&_S406)->differential_0 = 1.0f;
    DiffPair_float_0 _S407 = _d_atan2_0(&_S405, &_S406);
    if((_S407.primal_0) < 0.00100000004749745f)
    {
        float _S408 = _S407.differential_0 * _S407.primal_0;
        float _S409 = 1.0f - _S407.primal_0 * _S407.primal_0 / 3.0f;
        float _S410 = ((0.0f - (_S408 + _S408) * 0.3333333432674408f) * _S372 - _S409) / (_S372 * _S372);
        k_7 = _S409 / _S372;
        s_diff_k_2 = _S410;
    }
    else
    {
        float _S411 = (_S407.differential_0 * _S404.primal_0 - _S407.primal_0 * _S404.differential_0) / (_S404.primal_0 * _S404.primal_0);
        k_7 = _S407.primal_0 / _S404.primal_0;
        s_diff_k_2 = _S411;
    }
    float2  _S412 = _S368 * make_float2 (k_7);
    float2  _S413 = _S402 * make_float2 (k_7) + make_float2 (s_diff_k_2) * _S368;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S414;
    (&_S414)->primal_0 = _S412;
    (&_S414)->differential_0 = _S413;
    FixedArray<float, 4>  _S415 = dist_coeffs_19;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S416 = s_fwd_DistOpenCV_distort_0(&_S414, &_S415);
    float _S417 = _S416.differential_0.y * fy_5;
    *&(((&J_5)->rows + (int(0)))->z) = _S416.differential_0.x * fx_5;
    *&(((&J_5)->rows + (int(1)))->z) = _S417;
    return J_5;
}

inline __device__ Matrix<float, 2, 3>  equisolid_proj_jac_opencv(float3  p_view_13, float4  intrins_13, FixedArray<float, 4>  dist_coeffs_20)
{
    Matrix<float, 2, 3>  J_6;
    float2  _S418 = float2 {p_view_13.x, p_view_13.y};
    float2  _S419 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S420;
    (&_S420)->primal_0 = _S418;
    (&_S420)->differential_0 = _S419;
    DiffPair_float_0 _S421 = s_fwd_length_impl_0(&_S420);
    float _S422 = p_view_13.z;
    DiffPair_float_0 _S423;
    (&_S423)->primal_0 = _S421.primal_0;
    (&_S423)->differential_0 = _S421.differential_0;
    DiffPair_float_0 _S424;
    (&_S424)->primal_0 = _S422;
    (&_S424)->differential_0 = 0.0f;
    DiffPair_float_0 _S425 = _d_atan2_0(&_S423, &_S424);
    float k_8;
    float s_diff_k_3;
    if((_S421.primal_0) < 9.99999997475242708e-07f)
    {
        float _S426 = _S425.differential_0 * _S425.primal_0;
        float _S427 = 1.0f - _S425.primal_0 * _S425.primal_0 / 24.0f;
        float _S428 = ((0.0f - (_S426 + _S426) * 0.0416666679084301f) * _S422 - _S427 * 0.0f) / (_S422 * _S422);
        k_8 = _S427 / _S422;
        s_diff_k_3 = _S428;
    }
    else
    {
        float _S429 = _S425.differential_0 * 0.5f;
        DiffPair_float_0 _S430;
        (&_S430)->primal_0 = 0.5f * _S425.primal_0;
        (&_S430)->differential_0 = _S429;
        DiffPair_float_0 _S431 = _d_sin_0(&_S430);
        float _S432 = 2.0f * _S431.primal_0;
        float _S433 = (_S431.differential_0 * 2.0f * _S421.primal_0 - _S432 * _S421.differential_0) / (_S421.primal_0 * _S421.primal_0);
        k_8 = _S432 / _S421.primal_0;
        s_diff_k_3 = _S433;
    }
    float2  _S434 = _S418 * make_float2 (k_8);
    float2  _S435 = _S419 * make_float2 (k_8) + make_float2 (s_diff_k_3) * _S418;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S436;
    (&_S436)->primal_0 = _S434;
    (&_S436)->differential_0 = _S435;
    FixedArray<float, 4>  _S437 = dist_coeffs_20;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S438 = s_fwd_DistOpenCV_distort_0(&_S436, &_S437);
    float fx_6 = intrins_13.x;
    float fy_6 = intrins_13.y;
    float _S439 = _S438.differential_0.y * fy_6;
    *&(((&J_6)->rows + (int(0)))->x) = _S438.differential_0.x * fx_6;
    *&(((&J_6)->rows + (int(1)))->x) = _S439;
    float2  _S440 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S441;
    (&_S441)->primal_0 = _S418;
    (&_S441)->differential_0 = _S440;
    DiffPair_float_0 _S442 = s_fwd_length_impl_0(&_S441);
    DiffPair_float_0 _S443;
    (&_S443)->primal_0 = _S442.primal_0;
    (&_S443)->differential_0 = _S442.differential_0;
    DiffPair_float_0 _S444;
    (&_S444)->primal_0 = _S422;
    (&_S444)->differential_0 = 0.0f;
    DiffPair_float_0 _S445 = _d_atan2_0(&_S443, &_S444);
    if((_S442.primal_0) < 9.99999997475242708e-07f)
    {
        float _S446 = _S445.differential_0 * _S445.primal_0;
        float _S447 = 1.0f - _S445.primal_0 * _S445.primal_0 / 24.0f;
        float _S448 = ((0.0f - (_S446 + _S446) * 0.0416666679084301f) * _S422 - _S447 * 0.0f) / (_S422 * _S422);
        k_8 = _S447 / _S422;
        s_diff_k_3 = _S448;
    }
    else
    {
        float _S449 = _S445.differential_0 * 0.5f;
        DiffPair_float_0 _S450;
        (&_S450)->primal_0 = 0.5f * _S445.primal_0;
        (&_S450)->differential_0 = _S449;
        DiffPair_float_0 _S451 = _d_sin_0(&_S450);
        float _S452 = 2.0f * _S451.primal_0;
        float _S453 = (_S451.differential_0 * 2.0f * _S442.primal_0 - _S452 * _S442.differential_0) / (_S442.primal_0 * _S442.primal_0);
        k_8 = _S452 / _S442.primal_0;
        s_diff_k_3 = _S453;
    }
    float2  _S454 = _S418 * make_float2 (k_8);
    float2  _S455 = _S440 * make_float2 (k_8) + make_float2 (s_diff_k_3) * _S418;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S456;
    (&_S456)->primal_0 = _S454;
    (&_S456)->differential_0 = _S455;
    FixedArray<float, 4>  _S457 = dist_coeffs_20;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S458 = s_fwd_DistOpenCV_distort_0(&_S456, &_S457);
    float _S459 = _S458.differential_0.y * fy_6;
    *&(((&J_6)->rows + (int(0)))->y) = _S458.differential_0.x * fx_6;
    *&(((&J_6)->rows + (int(1)))->y) = _S459;
    float2  _S460 = make_float2 (0.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S461;
    (&_S461)->primal_0 = _S418;
    (&_S461)->differential_0 = _S460;
    DiffPair_float_0 _S462 = s_fwd_length_impl_0(&_S461);
    DiffPair_float_0 _S463;
    (&_S463)->primal_0 = _S462.primal_0;
    (&_S463)->differential_0 = _S462.differential_0;
    DiffPair_float_0 _S464;
    (&_S464)->primal_0 = _S422;
    (&_S464)->differential_0 = 1.0f;
    DiffPair_float_0 _S465 = _d_atan2_0(&_S463, &_S464);
    if((_S462.primal_0) < 9.99999997475242708e-07f)
    {
        float _S466 = _S465.differential_0 * _S465.primal_0;
        float _S467 = 1.0f - _S465.primal_0 * _S465.primal_0 / 24.0f;
        float _S468 = ((0.0f - (_S466 + _S466) * 0.0416666679084301f) * _S422 - _S467) / (_S422 * _S422);
        k_8 = _S467 / _S422;
        s_diff_k_3 = _S468;
    }
    else
    {
        float _S469 = _S465.differential_0 * 0.5f;
        DiffPair_float_0 _S470;
        (&_S470)->primal_0 = 0.5f * _S465.primal_0;
        (&_S470)->differential_0 = _S469;
        DiffPair_float_0 _S471 = _d_sin_0(&_S470);
        float _S472 = 2.0f * _S471.primal_0;
        float _S473 = (_S471.differential_0 * 2.0f * _S462.primal_0 - _S472 * _S462.differential_0) / (_S462.primal_0 * _S462.primal_0);
        k_8 = _S472 / _S462.primal_0;
        s_diff_k_3 = _S473;
    }
    float2  _S474 = _S418 * make_float2 (k_8);
    float2  _S475 = _S460 * make_float2 (k_8) + make_float2 (s_diff_k_3) * _S418;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S476;
    (&_S476)->primal_0 = _S474;
    (&_S476)->differential_0 = _S475;
    FixedArray<float, 4>  _S477 = dist_coeffs_20;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S478 = s_fwd_DistOpenCV_distort_0(&_S476, &_S477);
    float _S479 = _S478.differential_0.y * fy_6;
    *&(((&J_6)->rows + (int(0)))->z) = _S478.differential_0.x * fx_6;
    *&(((&J_6)->rows + (int(1)))->z) = _S479;
    return J_6;
}

inline __device__ float2  distort_point_opencv(float2  uv_21, int camera_model_4, FixedArray<float, 4>  dist_coeffs_21)
{
    float2  _S480;
    for(;;)
    {
        if(camera_model_4 == int(3))
        {
            _S480 = uv_21;
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
            _S480 = uv_21 * make_float2 (k_9);
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
                _S480 = uv_21 * make_float2 (k_9);
            }
            else
            {
                _S480 = uv_21;
            }
        }
        FixedArray<float, 4>  _S481 = dist_coeffs_21;
        float2  _S482 = DistOpenCV_distort_0(_S480, &_S481);
        _S480 = _S482;
        break;
    }
    return _S480;
}

inline __device__ bool undistort_point_opencv(float2  uv_22, int camera_model_5, FixedArray<float, 4>  dist_coeffs_22, float2  * uv_undist_4)
{
    bool _S483;
    for(;;)
    {
        *uv_undist_4 = make_float2 (0.0f);
        if(camera_model_5 == int(3))
        {
            float lon_2 = uv_22.x;
            float lat_3 = uv_22.y;
            float cl_3 = (F32_cos((lat_3)));
            *uv_undist_4 = make_float2 (cl_3 * (F32_sin((lon_2))), (F32_sin((lat_3)))) / make_float2 ((F32_max((cl_3 * (F32_cos((lon_2)))), (9.999999960041972e-13f))));
            _S483 = true;
            break;
        }
        FixedArray<float, 4>  _S484 = dist_coeffs_22;
        float2  uv_u_3;
        bool _S485 = undistort_point_1(uv_22, &_S484, int(8), &uv_u_3);
        if(!_S485)
        {
            _S483 = false;
            break;
        }
        float2  _S486 = uv_u_3;
        float3  raydir_3;
        if(camera_model_5 == int(1))
        {
            float r_19 = length_0(_S486);
            float s_3;
            if(r_19 < 0.00100000004749745f)
            {
                s_3 = 1.0f - r_19 * r_19 / 6.0f;
            }
            else
            {
                s_3 = (F32_sin((r_19))) / r_19;
            }
            raydir_3 = make_float3 ((_S486 * make_float2 (s_3)).x, (_S486 * make_float2 (s_3)).y, (F32_cos((r_19))));
        }
        else
        {
            if(camera_model_5 == int(2))
            {
                float r_20 = length_0(_S486);
                raydir_3 = make_float3 ((_S486 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_20 * r_20)))))))).x, (_S486 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_20 * r_20)))))))).y, 1.0f - 0.5f * r_20 * r_20);
            }
            else
            {
                raydir_3 = make_float3 (_S486.x, _S486.y, 1.0f);
            }
        }
        *uv_undist_4 = float2 {raydir_3.x, raydir_3.y} / make_float2 ((F32_max((raydir_3.z), (9.999999960041972e-13f))));
        _S483 = true;
        break;
    }
    return _S483;
}

inline __device__ bool unproject_point_opencv(float2  uv_23, int camera_model_6, FixedArray<float, 4>  dist_coeffs_23, float3  * raydir_4)
{
    bool _S487;
    for(;;)
    {
        if(camera_model_6 == int(4))
        {
            *raydir_4 = make_float3 (0.0f, 0.0f, 1.0f);
            _S487 = true;
            break;
        }
        int3  _S488 = make_int3 (int(0));
        float3  _S489 = make_float3 ((float)_S488.x, (float)_S488.y, (float)_S488.z);
        *raydir_4 = _S489;
        if(camera_model_6 == int(3))
        {
            float lon_3 = uv_23.x;
            float lat_4 = uv_23.y;
            float cl_4 = (F32_cos((lat_4)));
            *raydir_4 = make_float3 (cl_4 * (F32_sin((lon_3))), (F32_sin((lat_4))), cl_4 * (F32_cos((lon_3))));
            _S487 = true;
            break;
        }
        FixedArray<float, 4>  _S490 = dist_coeffs_23;
        float2  uv_u_4;
        bool _S491 = undistort_point_1(uv_23, &_S490, int(8), &uv_u_4);
        if(!_S491)
        {
            _S487 = false;
            break;
        }
        float2  _S492 = uv_u_4;
        if(camera_model_6 == int(1))
        {
            float r_21 = length_0(_S492);
            float s_4;
            if(r_21 < 0.00100000004749745f)
            {
                s_4 = 1.0f - r_21 * r_21 / 6.0f;
            }
            else
            {
                s_4 = (F32_sin((r_21))) / r_21;
            }
            *raydir_4 = make_float3 ((_S492 * make_float2 (s_4)).x, (_S492 * make_float2 (s_4)).y, (F32_cos((r_21))));
        }
        else
        {
            if(camera_model_6 == int(2))
            {
                float r_22 = length_0(_S492);
                *raydir_4 = make_float3 ((_S492 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_22 * r_22)))))))).x, (_S492 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_22 * r_22)))))))).y, 1.0f - 0.5f * r_22 * r_22);
            }
            else
            {
                *raydir_4 = make_float3 (_S492.x, _S492.y, 1.0f);
            }
        }
        _S487 = true;
        break;
    }
    return _S487;
}

inline __device__ bool generate_ray_opencv(float2  uv_24, int camera_model_7, FixedArray<float, 4>  dist_coeffs_24, float3  * raydir_5)
{
    bool _S493;
    for(;;)
    {
        if(camera_model_7 == int(4))
        {
            *raydir_5 = make_float3 (0.0f, 0.0f, 1.0f);
            _S493 = true;
            break;
        }
        if(camera_model_7 == int(3))
        {
            float _S494 = uv_24.x;
            if((F32_abs((_S494))) > 3.14159274101257324f)
            {
                _S493 = true;
            }
            else
            {
                _S493 = (F32_abs((uv_24.y))) > 1.57079637050628662f;
            }
            if(_S493)
            {
                int3  _S495 = make_int3 (int(0));
                float3  _S496 = make_float3 ((float)_S495.x, (float)_S495.y, (float)_S495.z);
                *raydir_5 = _S496;
                _S493 = false;
                break;
            }
            float lat_5 = uv_24.y;
            float cl_5 = (F32_cos((lat_5)));
            *raydir_5 = make_float3 (cl_5 * (F32_sin((_S494))), (F32_sin((lat_5))), cl_5 * (F32_cos((_S494))));
            _S493 = true;
            break;
        }
        FixedArray<float, 4>  _S497 = dist_coeffs_24;
        float2  uv_u_5;
        bool _S498 = undistort_point_1(uv_24, &_S497, int(8), &uv_u_5);
        if(!_S498)
        {
            int3  _S499 = make_int3 (int(0));
            float3  _S500 = make_float3 ((float)_S499.x, (float)_S499.y, (float)_S499.z);
            *raydir_5 = _S500;
            _S493 = false;
            break;
        }
        float2  _S501 = uv_u_5;
        if(camera_model_7 == int(1))
        {
            float r_23 = length_0(_S501);
            if(r_23 >= 3.14159274101257324f)
            {
                int3  _S502 = make_int3 (int(0));
                float3  _S503 = make_float3 ((float)_S502.x, (float)_S502.y, (float)_S502.z);
                *raydir_5 = _S503;
                _S493 = false;
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
            *raydir_5 = make_float3 ((_S501 * make_float2 (s_5)).x, (_S501 * make_float2 (s_5)).y, (F32_cos((r_23))));
        }
        else
        {
            if(camera_model_7 == int(2))
            {
                float r_24 = length_0(_S501);
                if(r_24 >= 2.0f)
                {
                    int3  _S504 = make_int3 (int(0));
                    float3  _S505 = make_float3 ((float)_S504.x, (float)_S504.y, (float)_S504.z);
                    *raydir_5 = _S505;
                    _S493 = false;
                    break;
                }
                *raydir_5 = make_float3 ((_S501 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_24 * r_24)))))))).x, (_S501 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_24 * r_24)))))))).y, 1.0f - 0.5f * r_24 * r_24);
            }
            else
            {
                *raydir_5 = make_float3 (_S501.x, _S501.y, 1.0f);
            }
        }
        *raydir_5 = normalize_0(*raydir_5);
        _S493 = true;
        break;
    }
    return _S493;
}

inline __device__ bool is_valid_distortion_prism(float2  uv_25, FixedArray<float, 8>  dist_coeffs_25)
{
    float2  _S506 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S507;
    (&_S507)->primal_0 = uv_25;
    (&_S507)->differential_0 = _S506;
    FixedArray<float, 8>  _S508 = dist_coeffs_25;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S509 = s_fwd_DistThinPrism_distort_0(&_S507, &_S508);
    float2  _S510 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S511;
    (&_S511)->primal_0 = uv_25;
    (&_S511)->differential_0 = _S510;
    FixedArray<float, 8>  _S512 = dist_coeffs_25;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S513 = s_fwd_DistThinPrism_distort_0(&_S511, &_S512);
    Matrix<float, 2, 2>  _S514 = transpose_1(makeMatrix<float, 2, 2> (_S509.differential_0, _S513.differential_0));
    float _S515 = (F32_min((determinant_0(_S514)), ((F32_min((_S514.rows[int(0)].x), (_S514.rows[int(1)].y))))));
    bool _S516;
    if(_S515 > 0.25f)
    {
        _S516 = _S515 < 4.0f;
    }
    else
    {
        _S516 = false;
    }
    if(_S516)
    {
        FixedArray<float, 8>  _S517 = dist_coeffs_25;
        float2  _S518 = DistThinPrism_distort_0(uv_25, &_S517);
        _S516 = (dot_0(uv_25, _S518)) >= 0.0f;
    }
    else
    {
        _S516 = false;
    }
    return _S516;
}

inline __device__ bool persp_proj_nav_prism(float3  p_view_14, float4  intrins_14, FixedArray<float, 8>  dist_coeffs_26, float2  * uv_26)
{
    bool _S519;
    for(;;)
    {
        float2  _S520 = float2 {p_view_14.x, p_view_14.y};
        float _S521 = p_view_14.z;
        float2  uv0_2 = _S520 / make_float2 (_S521);
        if(_S521 < 0.0f)
        {
            _S519 = true;
        }
        else
        {
            float2  _S522 = make_float2 (1.0f, 0.0f);
            DiffPair_vectorx3Cfloatx2C2x3E_0 _S523;
            (&_S523)->primal_0 = uv0_2;
            (&_S523)->differential_0 = _S522;
            FixedArray<float, 8>  _S524 = dist_coeffs_26;
            DiffPair_vectorx3Cfloatx2C2x3E_0 _S525 = s_fwd_DistThinPrism_distort_0(&_S523, &_S524);
            float2  _S526 = make_float2 (0.0f, 1.0f);
            DiffPair_vectorx3Cfloatx2C2x3E_0 _S527;
            (&_S527)->primal_0 = uv0_2;
            (&_S527)->differential_0 = _S526;
            FixedArray<float, 8>  _S528 = dist_coeffs_26;
            DiffPair_vectorx3Cfloatx2C2x3E_0 _S529 = s_fwd_DistThinPrism_distort_0(&_S527, &_S528);
            Matrix<float, 2, 2>  _S530 = transpose_1(makeMatrix<float, 2, 2> (_S525.differential_0, _S529.differential_0));
            float _S531 = (F32_min((determinant_0(_S530)), ((F32_min((_S530.rows[int(0)].x), (_S530.rows[int(1)].y))))));
            if(_S531 > 0.25f)
            {
                _S519 = _S531 < 4.0f;
            }
            else
            {
                _S519 = false;
            }
            if(_S519)
            {
                FixedArray<float, 8>  _S532 = dist_coeffs_26;
                float2  _S533 = DistThinPrism_distort_0(uv0_2, &_S532);
                _S519 = (dot_0(uv0_2, _S533)) >= 0.0f;
            }
            else
            {
                _S519 = false;
            }
            _S519 = !_S519;
        }
        if(_S519)
        {
            *uv_26 = uv0_2;
            _S519 = false;
            break;
        }
        float2  uv_27 = _S520 / make_float2 (_S521);
        FixedArray<float, 8>  _S534 = dist_coeffs_26;
        float2  _S535 = DistThinPrism_distort_0(uv_27, &_S534);
        *uv_26 = make_float2 (intrins_14.x * _S535.x + intrins_14.z, intrins_14.y * _S535.y + intrins_14.w);
        _S519 = true;
        break;
    }
    return _S519;
}

inline __device__ bool fisheye_proj_nav_prism(float3  p_view_15, float4  intrins_15, FixedArray<float, 8>  dist_coeffs_27, float2  * uv_28)
{
    bool _S536;
    for(;;)
    {
        float2  _S537 = float2 {p_view_15.x, p_view_15.y};
        float r_25 = length_0(_S537);
        float _S538 = p_view_15.z;
        float theta_8 = (F32_atan2((r_25), (_S538)));
        bool _S539 = theta_8 < 0.00100000004749745f;
        float k_10;
        if(_S539)
        {
            k_10 = (1.0f - theta_8 * theta_8 / 3.0f) / _S538;
        }
        else
        {
            k_10 = theta_8 / r_25;
        }
        float2  _S540 = _S537 * make_float2 (k_10);
        float2  _S541 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S542;
        (&_S542)->primal_0 = _S540;
        (&_S542)->differential_0 = _S541;
        FixedArray<float, 8>  _S543 = dist_coeffs_27;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S544 = s_fwd_DistThinPrism_distort_0(&_S542, &_S543);
        float2  _S545 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S546;
        (&_S546)->primal_0 = _S540;
        (&_S546)->differential_0 = _S545;
        FixedArray<float, 8>  _S547 = dist_coeffs_27;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S548 = s_fwd_DistThinPrism_distort_0(&_S546, &_S547);
        Matrix<float, 2, 2>  _S549 = transpose_1(makeMatrix<float, 2, 2> (_S544.differential_0, _S548.differential_0));
        float _S550 = (F32_min((determinant_0(_S549)), ((F32_min((_S549.rows[int(0)].x), (_S549.rows[int(1)].y))))));
        if(_S550 > 0.25f)
        {
            _S536 = _S550 < 4.0f;
        }
        else
        {
            _S536 = false;
        }
        if(_S536)
        {
            FixedArray<float, 8>  _S551 = dist_coeffs_27;
            float2  _S552 = DistThinPrism_distort_0(_S540, &_S551);
            _S536 = (dot_0(_S540, _S552)) >= 0.0f;
        }
        else
        {
            _S536 = false;
        }
        if(!_S536)
        {
            *uv_28 = _S540;
            _S536 = false;
            break;
        }
        if(_S539)
        {
            k_10 = (1.0f - theta_8 * theta_8 / 3.0f) / _S538;
        }
        else
        {
            k_10 = theta_8 / r_25;
        }
        float2  _S553 = _S537 * make_float2 (k_10);
        FixedArray<float, 8>  _S554 = dist_coeffs_27;
        float2  _S555 = DistThinPrism_distort_0(_S553, &_S554);
        *uv_28 = make_float2 (intrins_15.x * _S555.x + intrins_15.z, intrins_15.y * _S555.y + intrins_15.w);
        _S536 = true;
        break;
    }
    return _S536;
}

inline __device__ bool equisolid_proj_nav_prism(float3  p_view_16, float4  intrins_16, FixedArray<float, 8>  dist_coeffs_28, float2  * uv_29)
{
    bool _S556;
    for(;;)
    {
        float2  _S557 = float2 {p_view_16.x, p_view_16.y};
        float r_26 = length_0(_S557);
        float _S558 = p_view_16.z;
        float theta_9 = (F32_atan2((r_26), (_S558)));
        bool _S559 = r_26 < 9.99999997475242708e-07f;
        float k_11;
        if(_S559)
        {
            k_11 = (1.0f - theta_9 * theta_9 / 24.0f) / _S558;
        }
        else
        {
            k_11 = 2.0f * (F32_sin((0.5f * theta_9))) / r_26;
        }
        float2  _S560 = _S557 * make_float2 (k_11);
        float2  _S561 = make_float2 (1.0f, 0.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S562;
        (&_S562)->primal_0 = _S560;
        (&_S562)->differential_0 = _S561;
        FixedArray<float, 8>  _S563 = dist_coeffs_28;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S564 = s_fwd_DistThinPrism_distort_0(&_S562, &_S563);
        float2  _S565 = make_float2 (0.0f, 1.0f);
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S566;
        (&_S566)->primal_0 = _S560;
        (&_S566)->differential_0 = _S565;
        FixedArray<float, 8>  _S567 = dist_coeffs_28;
        DiffPair_vectorx3Cfloatx2C2x3E_0 _S568 = s_fwd_DistThinPrism_distort_0(&_S566, &_S567);
        Matrix<float, 2, 2>  _S569 = transpose_1(makeMatrix<float, 2, 2> (_S564.differential_0, _S568.differential_0));
        float _S570 = (F32_min((determinant_0(_S569)), ((F32_min((_S569.rows[int(0)].x), (_S569.rows[int(1)].y))))));
        if(_S570 > 0.25f)
        {
            _S556 = _S570 < 4.0f;
        }
        else
        {
            _S556 = false;
        }
        if(_S556)
        {
            FixedArray<float, 8>  _S571 = dist_coeffs_28;
            float2  _S572 = DistThinPrism_distort_0(_S560, &_S571);
            _S556 = (dot_0(_S560, _S572)) >= 0.0f;
        }
        else
        {
            _S556 = false;
        }
        if(!_S556)
        {
            *uv_29 = _S560;
            _S556 = false;
            break;
        }
        if(_S559)
        {
            k_11 = (1.0f - theta_9 * theta_9 / 24.0f) / _S558;
        }
        else
        {
            k_11 = 2.0f * (F32_sin((0.5f * theta_9))) / r_26;
        }
        float2  _S573 = _S557 * make_float2 (k_11);
        FixedArray<float, 8>  _S574 = dist_coeffs_28;
        float2  _S575 = DistThinPrism_distort_0(_S573, &_S574);
        *uv_29 = make_float2 (intrins_16.x * _S575.x + intrins_16.z, intrins_16.y * _S575.y + intrins_16.w);
        _S556 = true;
        break;
    }
    return _S556;
}

inline __device__ Matrix<float, 2, 3>  persp_proj_jac_prism(float3  p_view_17, float4  intrins_17, FixedArray<float, 8>  dist_coeffs_29)
{
    float2  _S576 = float2 {p_view_17.x, p_view_17.y};
    float _S577 = p_view_17.z;
    float2  _S578 = _S576 * make_float2 (0.0f);
    float _S579 = _S577 * _S577;
    float2  s_diff_uv_6 = (make_float2 (1.0f, 0.0f) * make_float2 (_S577) - _S578) / make_float2 (_S579);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S580;
    (&_S580)->primal_0 = _S576 / make_float2 (_S577);
    (&_S580)->differential_0 = s_diff_uv_6;
    FixedArray<float, 8>  _S581 = dist_coeffs_29;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S582 = s_fwd_DistThinPrism_distort_0(&_S580, &_S581);
    float fx_7 = intrins_17.x;
    float fy_7 = intrins_17.y;
    float _S583 = _S582.differential_0.y * fy_7;
    Matrix<float, 2, 3>  J_7;
    *&(((&J_7)->rows + (int(0)))->x) = _S582.differential_0.x * fx_7;
    *&(((&J_7)->rows + (int(1)))->x) = _S583;
    float2  s_diff_uv_7 = (make_float2 (0.0f, 1.0f) * make_float2 (_S577) - _S578) / make_float2 (_S579);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S584;
    (&_S584)->primal_0 = _S576 / make_float2 (_S577);
    (&_S584)->differential_0 = s_diff_uv_7;
    FixedArray<float, 8>  _S585 = dist_coeffs_29;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S586 = s_fwd_DistThinPrism_distort_0(&_S584, &_S585);
    float _S587 = _S586.differential_0.y * fy_7;
    *&(((&J_7)->rows + (int(0)))->y) = _S586.differential_0.x * fx_7;
    *&(((&J_7)->rows + (int(1)))->y) = _S587;
    float2  s_diff_uv_8 = (make_float2 (0.0f, 0.0f) * make_float2 (_S577) - _S576) / make_float2 (_S579);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S588;
    (&_S588)->primal_0 = _S576 / make_float2 (_S577);
    (&_S588)->differential_0 = s_diff_uv_8;
    FixedArray<float, 8>  _S589 = dist_coeffs_29;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S590 = s_fwd_DistThinPrism_distort_0(&_S588, &_S589);
    float _S591 = _S590.differential_0.y * fy_7;
    *&(((&J_7)->rows + (int(0)))->z) = _S590.differential_0.x * fx_7;
    *&(((&J_7)->rows + (int(1)))->z) = _S591;
    return J_7;
}

inline __device__ Matrix<float, 2, 3>  fisheye_proj_jac_prism(float3  p_view_18, float4  intrins_18, FixedArray<float, 8>  dist_coeffs_30)
{
    Matrix<float, 2, 3>  J_8;
    float2  _S592 = float2 {p_view_18.x, p_view_18.y};
    float2  _S593 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S594;
    (&_S594)->primal_0 = _S592;
    (&_S594)->differential_0 = _S593;
    DiffPair_float_0 _S595 = s_fwd_length_impl_0(&_S594);
    float _S596 = p_view_18.z;
    DiffPair_float_0 _S597;
    (&_S597)->primal_0 = _S595.primal_0;
    (&_S597)->differential_0 = _S595.differential_0;
    DiffPair_float_0 _S598;
    (&_S598)->primal_0 = _S596;
    (&_S598)->differential_0 = 0.0f;
    DiffPair_float_0 _S599 = _d_atan2_0(&_S597, &_S598);
    float k_12;
    float s_diff_k_4;
    if((_S599.primal_0) < 0.00100000004749745f)
    {
        float _S600 = _S599.differential_0 * _S599.primal_0;
        float _S601 = 1.0f - _S599.primal_0 * _S599.primal_0 / 3.0f;
        float _S602 = ((0.0f - (_S600 + _S600) * 0.3333333432674408f) * _S596 - _S601 * 0.0f) / (_S596 * _S596);
        k_12 = _S601 / _S596;
        s_diff_k_4 = _S602;
    }
    else
    {
        float _S603 = (_S599.differential_0 * _S595.primal_0 - _S599.primal_0 * _S595.differential_0) / (_S595.primal_0 * _S595.primal_0);
        k_12 = _S599.primal_0 / _S595.primal_0;
        s_diff_k_4 = _S603;
    }
    float2  _S604 = _S592 * make_float2 (k_12);
    float2  _S605 = _S593 * make_float2 (k_12) + make_float2 (s_diff_k_4) * _S592;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S606;
    (&_S606)->primal_0 = _S604;
    (&_S606)->differential_0 = _S605;
    FixedArray<float, 8>  _S607 = dist_coeffs_30;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S608 = s_fwd_DistThinPrism_distort_0(&_S606, &_S607);
    float fx_8 = intrins_18.x;
    float fy_8 = intrins_18.y;
    float _S609 = _S608.differential_0.y * fy_8;
    *&(((&J_8)->rows + (int(0)))->x) = _S608.differential_0.x * fx_8;
    *&(((&J_8)->rows + (int(1)))->x) = _S609;
    float2  _S610 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S611;
    (&_S611)->primal_0 = _S592;
    (&_S611)->differential_0 = _S610;
    DiffPair_float_0 _S612 = s_fwd_length_impl_0(&_S611);
    DiffPair_float_0 _S613;
    (&_S613)->primal_0 = _S612.primal_0;
    (&_S613)->differential_0 = _S612.differential_0;
    DiffPair_float_0 _S614;
    (&_S614)->primal_0 = _S596;
    (&_S614)->differential_0 = 0.0f;
    DiffPair_float_0 _S615 = _d_atan2_0(&_S613, &_S614);
    if((_S615.primal_0) < 0.00100000004749745f)
    {
        float _S616 = _S615.differential_0 * _S615.primal_0;
        float _S617 = 1.0f - _S615.primal_0 * _S615.primal_0 / 3.0f;
        float _S618 = ((0.0f - (_S616 + _S616) * 0.3333333432674408f) * _S596 - _S617 * 0.0f) / (_S596 * _S596);
        k_12 = _S617 / _S596;
        s_diff_k_4 = _S618;
    }
    else
    {
        float _S619 = (_S615.differential_0 * _S612.primal_0 - _S615.primal_0 * _S612.differential_0) / (_S612.primal_0 * _S612.primal_0);
        k_12 = _S615.primal_0 / _S612.primal_0;
        s_diff_k_4 = _S619;
    }
    float2  _S620 = _S592 * make_float2 (k_12);
    float2  _S621 = _S610 * make_float2 (k_12) + make_float2 (s_diff_k_4) * _S592;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S622;
    (&_S622)->primal_0 = _S620;
    (&_S622)->differential_0 = _S621;
    FixedArray<float, 8>  _S623 = dist_coeffs_30;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S624 = s_fwd_DistThinPrism_distort_0(&_S622, &_S623);
    float _S625 = _S624.differential_0.y * fy_8;
    *&(((&J_8)->rows + (int(0)))->y) = _S624.differential_0.x * fx_8;
    *&(((&J_8)->rows + (int(1)))->y) = _S625;
    float2  _S626 = make_float2 (0.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S627;
    (&_S627)->primal_0 = _S592;
    (&_S627)->differential_0 = _S626;
    DiffPair_float_0 _S628 = s_fwd_length_impl_0(&_S627);
    DiffPair_float_0 _S629;
    (&_S629)->primal_0 = _S628.primal_0;
    (&_S629)->differential_0 = _S628.differential_0;
    DiffPair_float_0 _S630;
    (&_S630)->primal_0 = _S596;
    (&_S630)->differential_0 = 1.0f;
    DiffPair_float_0 _S631 = _d_atan2_0(&_S629, &_S630);
    if((_S631.primal_0) < 0.00100000004749745f)
    {
        float _S632 = _S631.differential_0 * _S631.primal_0;
        float _S633 = 1.0f - _S631.primal_0 * _S631.primal_0 / 3.0f;
        float _S634 = ((0.0f - (_S632 + _S632) * 0.3333333432674408f) * _S596 - _S633) / (_S596 * _S596);
        k_12 = _S633 / _S596;
        s_diff_k_4 = _S634;
    }
    else
    {
        float _S635 = (_S631.differential_0 * _S628.primal_0 - _S631.primal_0 * _S628.differential_0) / (_S628.primal_0 * _S628.primal_0);
        k_12 = _S631.primal_0 / _S628.primal_0;
        s_diff_k_4 = _S635;
    }
    float2  _S636 = _S592 * make_float2 (k_12);
    float2  _S637 = _S626 * make_float2 (k_12) + make_float2 (s_diff_k_4) * _S592;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S638;
    (&_S638)->primal_0 = _S636;
    (&_S638)->differential_0 = _S637;
    FixedArray<float, 8>  _S639 = dist_coeffs_30;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S640 = s_fwd_DistThinPrism_distort_0(&_S638, &_S639);
    float _S641 = _S640.differential_0.y * fy_8;
    *&(((&J_8)->rows + (int(0)))->z) = _S640.differential_0.x * fx_8;
    *&(((&J_8)->rows + (int(1)))->z) = _S641;
    return J_8;
}

inline __device__ Matrix<float, 2, 3>  equisolid_proj_jac_prism(float3  p_view_19, float4  intrins_19, FixedArray<float, 8>  dist_coeffs_31)
{
    Matrix<float, 2, 3>  J_9;
    float2  _S642 = float2 {p_view_19.x, p_view_19.y};
    float2  _S643 = make_float2 (1.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S644;
    (&_S644)->primal_0 = _S642;
    (&_S644)->differential_0 = _S643;
    DiffPair_float_0 _S645 = s_fwd_length_impl_0(&_S644);
    float _S646 = p_view_19.z;
    DiffPair_float_0 _S647;
    (&_S647)->primal_0 = _S645.primal_0;
    (&_S647)->differential_0 = _S645.differential_0;
    DiffPair_float_0 _S648;
    (&_S648)->primal_0 = _S646;
    (&_S648)->differential_0 = 0.0f;
    DiffPair_float_0 _S649 = _d_atan2_0(&_S647, &_S648);
    float k_13;
    float s_diff_k_5;
    if((_S645.primal_0) < 9.99999997475242708e-07f)
    {
        float _S650 = _S649.differential_0 * _S649.primal_0;
        float _S651 = 1.0f - _S649.primal_0 * _S649.primal_0 / 24.0f;
        float _S652 = ((0.0f - (_S650 + _S650) * 0.0416666679084301f) * _S646 - _S651 * 0.0f) / (_S646 * _S646);
        k_13 = _S651 / _S646;
        s_diff_k_5 = _S652;
    }
    else
    {
        float _S653 = _S649.differential_0 * 0.5f;
        DiffPair_float_0 _S654;
        (&_S654)->primal_0 = 0.5f * _S649.primal_0;
        (&_S654)->differential_0 = _S653;
        DiffPair_float_0 _S655 = _d_sin_0(&_S654);
        float _S656 = 2.0f * _S655.primal_0;
        float _S657 = (_S655.differential_0 * 2.0f * _S645.primal_0 - _S656 * _S645.differential_0) / (_S645.primal_0 * _S645.primal_0);
        k_13 = _S656 / _S645.primal_0;
        s_diff_k_5 = _S657;
    }
    float2  _S658 = _S642 * make_float2 (k_13);
    float2  _S659 = _S643 * make_float2 (k_13) + make_float2 (s_diff_k_5) * _S642;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S660;
    (&_S660)->primal_0 = _S658;
    (&_S660)->differential_0 = _S659;
    FixedArray<float, 8>  _S661 = dist_coeffs_31;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S662 = s_fwd_DistThinPrism_distort_0(&_S660, &_S661);
    float fx_9 = intrins_19.x;
    float fy_9 = intrins_19.y;
    float _S663 = _S662.differential_0.y * fy_9;
    *&(((&J_9)->rows + (int(0)))->x) = _S662.differential_0.x * fx_9;
    *&(((&J_9)->rows + (int(1)))->x) = _S663;
    float2  _S664 = make_float2 (0.0f, 1.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S665;
    (&_S665)->primal_0 = _S642;
    (&_S665)->differential_0 = _S664;
    DiffPair_float_0 _S666 = s_fwd_length_impl_0(&_S665);
    DiffPair_float_0 _S667;
    (&_S667)->primal_0 = _S666.primal_0;
    (&_S667)->differential_0 = _S666.differential_0;
    DiffPair_float_0 _S668;
    (&_S668)->primal_0 = _S646;
    (&_S668)->differential_0 = 0.0f;
    DiffPair_float_0 _S669 = _d_atan2_0(&_S667, &_S668);
    if((_S666.primal_0) < 9.99999997475242708e-07f)
    {
        float _S670 = _S669.differential_0 * _S669.primal_0;
        float _S671 = 1.0f - _S669.primal_0 * _S669.primal_0 / 24.0f;
        float _S672 = ((0.0f - (_S670 + _S670) * 0.0416666679084301f) * _S646 - _S671 * 0.0f) / (_S646 * _S646);
        k_13 = _S671 / _S646;
        s_diff_k_5 = _S672;
    }
    else
    {
        float _S673 = _S669.differential_0 * 0.5f;
        DiffPair_float_0 _S674;
        (&_S674)->primal_0 = 0.5f * _S669.primal_0;
        (&_S674)->differential_0 = _S673;
        DiffPair_float_0 _S675 = _d_sin_0(&_S674);
        float _S676 = 2.0f * _S675.primal_0;
        float _S677 = (_S675.differential_0 * 2.0f * _S666.primal_0 - _S676 * _S666.differential_0) / (_S666.primal_0 * _S666.primal_0);
        k_13 = _S676 / _S666.primal_0;
        s_diff_k_5 = _S677;
    }
    float2  _S678 = _S642 * make_float2 (k_13);
    float2  _S679 = _S664 * make_float2 (k_13) + make_float2 (s_diff_k_5) * _S642;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S680;
    (&_S680)->primal_0 = _S678;
    (&_S680)->differential_0 = _S679;
    FixedArray<float, 8>  _S681 = dist_coeffs_31;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S682 = s_fwd_DistThinPrism_distort_0(&_S680, &_S681);
    float _S683 = _S682.differential_0.y * fy_9;
    *&(((&J_9)->rows + (int(0)))->y) = _S682.differential_0.x * fx_9;
    *&(((&J_9)->rows + (int(1)))->y) = _S683;
    float2  _S684 = make_float2 (0.0f, 0.0f);
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S685;
    (&_S685)->primal_0 = _S642;
    (&_S685)->differential_0 = _S684;
    DiffPair_float_0 _S686 = s_fwd_length_impl_0(&_S685);
    DiffPair_float_0 _S687;
    (&_S687)->primal_0 = _S686.primal_0;
    (&_S687)->differential_0 = _S686.differential_0;
    DiffPair_float_0 _S688;
    (&_S688)->primal_0 = _S646;
    (&_S688)->differential_0 = 1.0f;
    DiffPair_float_0 _S689 = _d_atan2_0(&_S687, &_S688);
    if((_S686.primal_0) < 9.99999997475242708e-07f)
    {
        float _S690 = _S689.differential_0 * _S689.primal_0;
        float _S691 = 1.0f - _S689.primal_0 * _S689.primal_0 / 24.0f;
        float _S692 = ((0.0f - (_S690 + _S690) * 0.0416666679084301f) * _S646 - _S691) / (_S646 * _S646);
        k_13 = _S691 / _S646;
        s_diff_k_5 = _S692;
    }
    else
    {
        float _S693 = _S689.differential_0 * 0.5f;
        DiffPair_float_0 _S694;
        (&_S694)->primal_0 = 0.5f * _S689.primal_0;
        (&_S694)->differential_0 = _S693;
        DiffPair_float_0 _S695 = _d_sin_0(&_S694);
        float _S696 = 2.0f * _S695.primal_0;
        float _S697 = (_S695.differential_0 * 2.0f * _S686.primal_0 - _S696 * _S686.differential_0) / (_S686.primal_0 * _S686.primal_0);
        k_13 = _S696 / _S686.primal_0;
        s_diff_k_5 = _S697;
    }
    float2  _S698 = _S642 * make_float2 (k_13);
    float2  _S699 = _S684 * make_float2 (k_13) + make_float2 (s_diff_k_5) * _S642;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S700;
    (&_S700)->primal_0 = _S698;
    (&_S700)->differential_0 = _S699;
    FixedArray<float, 8>  _S701 = dist_coeffs_31;
    DiffPair_vectorx3Cfloatx2C2x3E_0 _S702 = s_fwd_DistThinPrism_distort_0(&_S700, &_S701);
    float _S703 = _S702.differential_0.y * fy_9;
    *&(((&J_9)->rows + (int(0)))->z) = _S702.differential_0.x * fx_9;
    *&(((&J_9)->rows + (int(1)))->z) = _S703;
    return J_9;
}

inline __device__ float2  distort_point_prism(float2  uv_30, int camera_model_8, FixedArray<float, 8>  dist_coeffs_32)
{
    float2  _S704;
    for(;;)
    {
        if(camera_model_8 == int(3))
        {
            _S704 = uv_30;
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
            _S704 = uv_30 * make_float2 (k_14);
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
                _S704 = uv_30 * make_float2 (k_14);
            }
            else
            {
                _S704 = uv_30;
            }
        }
        FixedArray<float, 8>  _S705 = dist_coeffs_32;
        float2  _S706 = DistThinPrism_distort_0(_S704, &_S705);
        _S704 = _S706;
        break;
    }
    return _S704;
}

inline __device__ bool undistort_point_prism(float2  uv_31, int camera_model_9, FixedArray<float, 8>  dist_coeffs_33, float2  * uv_undist_5)
{
    bool _S707;
    for(;;)
    {
        *uv_undist_5 = make_float2 (0.0f);
        if(camera_model_9 == int(3))
        {
            float lon_4 = uv_31.x;
            float lat_6 = uv_31.y;
            float cl_6 = (F32_cos((lat_6)));
            *uv_undist_5 = make_float2 (cl_6 * (F32_sin((lon_4))), (F32_sin((lat_6)))) / make_float2 ((F32_max((cl_6 * (F32_cos((lon_4)))), (9.999999960041972e-13f))));
            _S707 = true;
            break;
        }
        FixedArray<float, 8>  _S708 = dist_coeffs_33;
        float2  uv_u_6;
        bool _S709 = undistort_point_2(uv_31, &_S708, int(8), &uv_u_6);
        if(!_S709)
        {
            _S707 = false;
            break;
        }
        float2  _S710 = uv_u_6;
        float3  raydir_6;
        if(camera_model_9 == int(1))
        {
            float r_29 = length_0(_S710);
            float s_6;
            if(r_29 < 0.00100000004749745f)
            {
                s_6 = 1.0f - r_29 * r_29 / 6.0f;
            }
            else
            {
                s_6 = (F32_sin((r_29))) / r_29;
            }
            raydir_6 = make_float3 ((_S710 * make_float2 (s_6)).x, (_S710 * make_float2 (s_6)).y, (F32_cos((r_29))));
        }
        else
        {
            if(camera_model_9 == int(2))
            {
                float r_30 = length_0(_S710);
                raydir_6 = make_float3 ((_S710 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_30 * r_30)))))))).x, (_S710 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_30 * r_30)))))))).y, 1.0f - 0.5f * r_30 * r_30);
            }
            else
            {
                raydir_6 = make_float3 (_S710.x, _S710.y, 1.0f);
            }
        }
        *uv_undist_5 = float2 {raydir_6.x, raydir_6.y} / make_float2 ((F32_max((raydir_6.z), (9.999999960041972e-13f))));
        _S707 = true;
        break;
    }
    return _S707;
}

inline __device__ bool unproject_point_prism(float2  uv_32, int camera_model_10, FixedArray<float, 8>  dist_coeffs_34, float3  * raydir_7)
{
    bool _S711;
    for(;;)
    {
        if(camera_model_10 == int(4))
        {
            *raydir_7 = make_float3 (0.0f, 0.0f, 1.0f);
            _S711 = true;
            break;
        }
        int3  _S712 = make_int3 (int(0));
        float3  _S713 = make_float3 ((float)_S712.x, (float)_S712.y, (float)_S712.z);
        *raydir_7 = _S713;
        if(camera_model_10 == int(3))
        {
            float lon_5 = uv_32.x;
            float lat_7 = uv_32.y;
            float cl_7 = (F32_cos((lat_7)));
            *raydir_7 = make_float3 (cl_7 * (F32_sin((lon_5))), (F32_sin((lat_7))), cl_7 * (F32_cos((lon_5))));
            _S711 = true;
            break;
        }
        FixedArray<float, 8>  _S714 = dist_coeffs_34;
        float2  uv_u_7;
        bool _S715 = undistort_point_2(uv_32, &_S714, int(8), &uv_u_7);
        if(!_S715)
        {
            _S711 = false;
            break;
        }
        float2  _S716 = uv_u_7;
        if(camera_model_10 == int(1))
        {
            float r_31 = length_0(_S716);
            float s_7;
            if(r_31 < 0.00100000004749745f)
            {
                s_7 = 1.0f - r_31 * r_31 / 6.0f;
            }
            else
            {
                s_7 = (F32_sin((r_31))) / r_31;
            }
            *raydir_7 = make_float3 ((_S716 * make_float2 (s_7)).x, (_S716 * make_float2 (s_7)).y, (F32_cos((r_31))));
        }
        else
        {
            if(camera_model_10 == int(2))
            {
                float r_32 = length_0(_S716);
                *raydir_7 = make_float3 ((_S716 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_32 * r_32)))))))).x, (_S716 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_32 * r_32)))))))).y, 1.0f - 0.5f * r_32 * r_32);
            }
            else
            {
                *raydir_7 = make_float3 (_S716.x, _S716.y, 1.0f);
            }
        }
        _S711 = true;
        break;
    }
    return _S711;
}

inline __device__ bool generate_ray_prism(float2  uv_33, int camera_model_11, FixedArray<float, 8>  dist_coeffs_35, float3  * raydir_8)
{
    bool _S717;
    for(;;)
    {
        if(camera_model_11 == int(4))
        {
            *raydir_8 = make_float3 (0.0f, 0.0f, 1.0f);
            _S717 = true;
            break;
        }
        if(camera_model_11 == int(3))
        {
            float _S718 = uv_33.x;
            if((F32_abs((_S718))) > 3.14159274101257324f)
            {
                _S717 = true;
            }
            else
            {
                _S717 = (F32_abs((uv_33.y))) > 1.57079637050628662f;
            }
            if(_S717)
            {
                int3  _S719 = make_int3 (int(0));
                float3  _S720 = make_float3 ((float)_S719.x, (float)_S719.y, (float)_S719.z);
                *raydir_8 = _S720;
                _S717 = false;
                break;
            }
            float lat_8 = uv_33.y;
            float cl_8 = (F32_cos((lat_8)));
            *raydir_8 = make_float3 (cl_8 * (F32_sin((_S718))), (F32_sin((lat_8))), cl_8 * (F32_cos((_S718))));
            _S717 = true;
            break;
        }
        FixedArray<float, 8>  _S721 = dist_coeffs_35;
        float2  uv_u_8;
        bool _S722 = undistort_point_2(uv_33, &_S721, int(8), &uv_u_8);
        if(!_S722)
        {
            int3  _S723 = make_int3 (int(0));
            float3  _S724 = make_float3 ((float)_S723.x, (float)_S723.y, (float)_S723.z);
            *raydir_8 = _S724;
            _S717 = false;
            break;
        }
        float2  _S725 = uv_u_8;
        if(camera_model_11 == int(1))
        {
            float r_33 = length_0(_S725);
            if(r_33 >= 3.14159274101257324f)
            {
                int3  _S726 = make_int3 (int(0));
                float3  _S727 = make_float3 ((float)_S726.x, (float)_S726.y, (float)_S726.z);
                *raydir_8 = _S727;
                _S717 = false;
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
            *raydir_8 = make_float3 ((_S725 * make_float2 (s_8)).x, (_S725 * make_float2 (s_8)).y, (F32_cos((r_33))));
        }
        else
        {
            if(camera_model_11 == int(2))
            {
                float r_34 = length_0(_S725);
                if(r_34 >= 2.0f)
                {
                    int3  _S728 = make_int3 (int(0));
                    float3  _S729 = make_float3 ((float)_S728.x, (float)_S728.y, (float)_S728.z);
                    *raydir_8 = _S729;
                    _S717 = false;
                    break;
                }
                *raydir_8 = make_float3 ((_S725 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_34 * r_34)))))))).x, (_S725 * make_float2 ((F32_sqrt(((F32_max((0.0f), (1.0f - 0.25f * r_34 * r_34)))))))).y, 1.0f - 0.5f * r_34 * r_34);
            }
            else
            {
                *raydir_8 = make_float3 (_S725.x, _S725.y, 1.0f);
            }
        }
        *raydir_8 = normalize_0(*raydir_8);
        _S717 = true;
        break;
    }
    return _S717;
}

inline __device__ void _d_mul_1(DiffPair_vectorx3Cfloatx2C3x3E_0 * left_4, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * right_4, float3  dOut_3)
{
    float _S730 = (*right_4).primal_0.rows[int(0)].x * dOut_3.x;
    Matrix<float, 3, 3>  right_d_result_2;
    *&(((&right_d_result_2)->rows + (int(0)))->x) = (*left_4).primal_0.x * dOut_3.x;
    float sum_10 = _S730 + (*right_4).primal_0.rows[int(0)].y * dOut_3.y;
    *&(((&right_d_result_2)->rows + (int(0)))->y) = (*left_4).primal_0.x * dOut_3.y;
    float sum_11 = sum_10 + (*right_4).primal_0.rows[int(0)].z * dOut_3.z;
    *&(((&right_d_result_2)->rows + (int(0)))->z) = (*left_4).primal_0.x * dOut_3.z;
    float3  left_d_result_2;
    *&((&left_d_result_2)->x) = sum_11;
    float _S731 = (*right_4).primal_0.rows[int(1)].x * dOut_3.x;
    *&(((&right_d_result_2)->rows + (int(1)))->x) = (*left_4).primal_0.y * dOut_3.x;
    float sum_12 = _S731 + (*right_4).primal_0.rows[int(1)].y * dOut_3.y;
    *&(((&right_d_result_2)->rows + (int(1)))->y) = (*left_4).primal_0.y * dOut_3.y;
    float sum_13 = sum_12 + (*right_4).primal_0.rows[int(1)].z * dOut_3.z;
    *&(((&right_d_result_2)->rows + (int(1)))->z) = (*left_4).primal_0.y * dOut_3.z;
    *&((&left_d_result_2)->y) = sum_13;
    float _S732 = (*right_4).primal_0.rows[int(2)].x * dOut_3.x;
    *&(((&right_d_result_2)->rows + (int(2)))->x) = (*left_4).primal_0.z * dOut_3.x;
    float sum_14 = _S732 + (*right_4).primal_0.rows[int(2)].y * dOut_3.y;
    *&(((&right_d_result_2)->rows + (int(2)))->y) = (*left_4).primal_0.z * dOut_3.y;
    float sum_15 = sum_14 + (*right_4).primal_0.rows[int(2)].z * dOut_3.z;
    *&(((&right_d_result_2)->rows + (int(2)))->z) = (*left_4).primal_0.z * dOut_3.z;
    *&((&left_d_result_2)->z) = sum_15;
    left_4->primal_0 = (*left_4).primal_0;
    left_4->differential_0 = left_d_result_2;
    right_4->primal_0 = (*right_4).primal_0;
    right_4->differential_0 = right_d_result_2;
    return;
}

inline __device__ float3  mul_3(float3  left_5, Matrix<float, 3, 3>  right_5)
{
    float3  result_8;
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
        *_slang_vector_get_element_ptr(&result_8, j_1) = sum_16;
        j_1 = j_1 + int(1);
    }
    return result_8;
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

inline __device__ void s_bwd_prop_mul_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S733, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S734, float3  _S735)
{
    _d_mul_1(_S733, _S734, _S735);
    return;
}

inline __device__ void s_bwd_prop_transform_ray_o_0(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * dpR_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpt_0, float3  _s_dOut_0)
{
    float3  _S736 = - _s_dOut_0;
    float3  _S737 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S738;
    (&_S738)->primal_0 = (*dpt_0).primal_0;
    (&_S738)->differential_0 = _S737;
    Matrix<float, 3, 3>  _S739 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S740;
    (&_S740)->primal_0 = (*dpR_0).primal_0;
    (&_S740)->differential_0 = _S739;
    s_bwd_prop_mul_0(&_S738, &_S740, _S736);
    dpt_0->primal_0 = (*dpt_0).primal_0;
    dpt_0->differential_0 = _S738.differential_0;
    dpR_0->primal_0 = (*dpR_0).primal_0;
    dpR_0->differential_0 = _S740.differential_0;
    return;
}

inline __device__ void s_bwd_transform_ray_o_0(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S741, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S742, float3  _S743)
{
    s_bwd_prop_transform_ray_o_0(_S741, _S742, _S743);
    return;
}

inline __device__ void transform_ray_o_vjp(Matrix<float, 3, 3>  R_3, float3  t_1, float3  v_ray_o_0, Matrix<float, 3, 3>  * v_R_0, float3  * v_t_0)
{
    Matrix<float, 3, 3>  _S744 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 dp_R_0;
    (&dp_R_0)->primal_0 = R_3;
    (&dp_R_0)->differential_0 = _S744;
    float3  _S745 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_t_0;
    (&dp_t_0)->primal_0 = t_1;
    (&dp_t_0)->differential_0 = _S745;
    s_bwd_transform_ray_o_0(&dp_R_0, &dp_t_0, v_ray_o_0);
    *v_R_0 = dp_R_0.differential_0;
    *v_t_0 = dp_t_0.differential_0;
    return;
}

inline __device__ void s_bwd_prop_transform_ray_d_0(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * dpR_1, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpraydir_0, float3  _s_dOut_1)
{
    float3  _S746 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S747;
    (&_S747)->primal_0 = (*dpraydir_0).primal_0;
    (&_S747)->differential_0 = _S746;
    Matrix<float, 3, 3>  _S748 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S749;
    (&_S749)->primal_0 = (*dpR_1).primal_0;
    (&_S749)->differential_0 = _S748;
    s_bwd_prop_mul_0(&_S747, &_S749, _s_dOut_1);
    dpraydir_0->primal_0 = (*dpraydir_0).primal_0;
    dpraydir_0->differential_0 = _S747.differential_0;
    dpR_1->primal_0 = (*dpR_1).primal_0;
    dpR_1->differential_0 = _S749.differential_0;
    return;
}

inline __device__ void s_bwd_transform_ray_d_0(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S750, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S751, float3  _S752)
{
    s_bwd_prop_transform_ray_d_0(_S750, _S751, _S752);
    return;
}

inline __device__ void transform_ray_d_vjp(Matrix<float, 3, 3>  R_4, float3  raydir_11, float3  v_ray_d_0, Matrix<float, 3, 3>  * v_R_1, float3  * v_raydir_0)
{
    Matrix<float, 3, 3>  _S753 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 dp_R_1;
    (&dp_R_1)->primal_0 = R_4;
    (&dp_R_1)->differential_0 = _S753;
    float3  _S754 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_raydir_0;
    (&dp_raydir_0)->primal_0 = raydir_11;
    (&dp_raydir_0)->differential_0 = _S754;
    s_bwd_transform_ray_d_0(&dp_R_1, &dp_raydir_0, v_ray_d_0);
    *v_R_1 = dp_R_1.differential_0;
    *v_raydir_0 = dp_raydir_0.differential_0;
    return;
}

inline __device__ void _d_exp_0(DiffPair_float_0 * dpx_5, float dOut_4)
{
    float _S755 = (F32_exp(((*dpx_5).primal_0))) * dOut_4;
    dpx_5->primal_0 = (*dpx_5).primal_0;
    dpx_5->differential_0 = _S755;
    return;
}

inline __device__ float3  exp_0(float3  x_12)
{
    float3  result_9;
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
        *_slang_vector_get_element_ptr(&result_9, i_7) = (F32_exp((_slang_vector_get_element(x_12, i_7))));
        i_7 = i_7 + int(1);
    }
    return result_9;
}

inline __device__ void _d_exp_vector_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpx_6, float3  dOut_5)
{
    float3  _S756 = exp_0((*dpx_6).primal_0) * dOut_5;
    dpx_6->primal_0 = (*dpx_6).primal_0;
    dpx_6->differential_0 = _S756;
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
    float3  _S757 = exp_0(- scale_4);
    return mul_1(makeMatrix<float, 3, 3> (_S757.x, 0.0f, 0.0f, 0.0f, _S757.y, 0.0f, 0.0f, 0.0f, _S757.z), transpose_0(transpose_0(makeMatrix<float, 3, 3> (1.0f - 2.0f * (y2_5 + z2_5), 2.0f * (xy_5 + wz_5), 2.0f * (xz_5 - wy_5), 2.0f * (xy_5 - wz_5), 1.0f - 2.0f * (x2_5 + z2_5), 2.0f * (yz_5 + wx_5), 2.0f * (xz_5 + wy_5), 2.0f * (yz_5 - wx_5), 1.0f - 2.0f * (x2_5 + y2_5)))));
}

struct DiffPair_vectorx3Cfloatx2C4x3E_0
{
    float4  primal_0;
    float4  differential_0;
};

inline __device__ float3  s_primal_ctx_exp_0(float3  _S758)
{
    return exp_0(_S758);
}

inline __device__ void s_bwd_prop_mul_1(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S759, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S760, Matrix<float, 3, 3>  _S761)
{
    mul_0(_S759, _S760, _S761);
    return;
}

inline __device__ void s_bwd_prop_exp_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S762, float3  _S763)
{
    _d_exp_vector_0(_S762, _S763);
    return;
}

inline __device__ void s_bwd_prop_compute_3dgut_iscl_rot_0(DiffPair_vectorx3Cfloatx2C4x3E_0 * dpquat_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpscale_0, Matrix<float, 3, 3>  _s_dOut_2)
{
    float _S764 = (*dpquat_0).primal_0.y;
    float x2_6 = _S764 * _S764;
    float y2_6 = (*dpquat_0).primal_0.z * (*dpquat_0).primal_0.z;
    float z2_6 = (*dpquat_0).primal_0.w * (*dpquat_0).primal_0.w;
    float xy_6 = (*dpquat_0).primal_0.y * (*dpquat_0).primal_0.z;
    float xz_6 = (*dpquat_0).primal_0.y * (*dpquat_0).primal_0.w;
    float yz_6 = (*dpquat_0).primal_0.z * (*dpquat_0).primal_0.w;
    float wx_6 = (*dpquat_0).primal_0.x * (*dpquat_0).primal_0.y;
    float wy_6 = (*dpquat_0).primal_0.x * (*dpquat_0).primal_0.z;
    float wz_6 = (*dpquat_0).primal_0.x * (*dpquat_0).primal_0.w;
    float3  _S765 = - (*dpscale_0).primal_0;
    float3  _S766 = s_primal_ctx_exp_0(_S765);
    Matrix<float, 3, 3>  _S767 = transpose_0(transpose_0(makeMatrix<float, 3, 3> (1.0f - 2.0f * (y2_6 + z2_6), 2.0f * (xy_6 + wz_6), 2.0f * (xz_6 - wy_6), 2.0f * (xy_6 - wz_6), 1.0f - 2.0f * (x2_6 + z2_6), 2.0f * (yz_6 + wx_6), 2.0f * (xz_6 + wy_6), 2.0f * (yz_6 - wx_6), 1.0f - 2.0f * (x2_6 + y2_6))));
    Matrix<float, 3, 3>  _S768 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S769;
    (&_S769)->primal_0 = makeMatrix<float, 3, 3> (_S766.x, 0.0f, 0.0f, 0.0f, _S766.y, 0.0f, 0.0f, 0.0f, _S766.z);
    (&_S769)->differential_0 = _S768;
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S770;
    (&_S770)->primal_0 = _S767;
    (&_S770)->differential_0 = _S768;
    s_bwd_prop_mul_1(&_S769, &_S770, _s_dOut_2);
    Matrix<float, 3, 3>  _S771 = transpose_0(_S770.differential_0);
    float3  _S772 = make_float3 (_S769.differential_0.rows[int(0)].x, _S769.differential_0.rows[int(1)].y, _S769.differential_0.rows[int(2)].z);
    float3  _S773 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S774;
    (&_S774)->primal_0 = _S765;
    (&_S774)->differential_0 = _S773;
    s_bwd_prop_exp_0(&_S774, _S772);
    float3  _S775 = - _S774.differential_0;
    Matrix<float, 3, 3>  _S776 = transpose_0(_S771);
    float _S777 = 2.0f * - _S776.rows[int(2)].z;
    float _S778 = 2.0f * _S776.rows[int(2)].y;
    float _S779 = 2.0f * _S776.rows[int(2)].x;
    float _S780 = 2.0f * _S776.rows[int(1)].z;
    float _S781 = 2.0f * - _S776.rows[int(1)].y;
    float _S782 = 2.0f * _S776.rows[int(1)].x;
    float _S783 = 2.0f * _S776.rows[int(0)].z;
    float _S784 = 2.0f * _S776.rows[int(0)].y;
    float _S785 = 2.0f * - _S776.rows[int(0)].x;
    float _S786 = - _S782 + _S784;
    float _S787 = _S779 + - _S783;
    float _S788 = - _S778 + _S780;
    float _S789 = _S778 + _S780;
    float _S790 = _S779 + _S783;
    float _S791 = _S782 + _S784;
    float _S792 = (*dpquat_0).primal_0.w * (_S781 + _S785);
    float _S793 = (*dpquat_0).primal_0.z * (_S777 + _S785);
    float _S794 = (*dpquat_0).primal_0.y * (_S777 + _S781);
    float _S795 = (*dpquat_0).primal_0.x * _S786 + (*dpquat_0).primal_0.z * _S789 + (*dpquat_0).primal_0.y * _S790 + _S792 + _S792;
    float _S796 = (*dpquat_0).primal_0.x * _S787 + (*dpquat_0).primal_0.w * _S789 + (*dpquat_0).primal_0.y * _S791 + _S793 + _S793;
    float _S797 = (*dpquat_0).primal_0.x * _S788 + (*dpquat_0).primal_0.w * _S790 + (*dpquat_0).primal_0.z * _S791 + _S794 + _S794;
    float _S798 = (*dpquat_0).primal_0.w * _S786 + (*dpquat_0).primal_0.z * _S787 + (*dpquat_0).primal_0.y * _S788;
    dpscale_0->primal_0 = (*dpscale_0).primal_0;
    dpscale_0->differential_0 = _S775;
    float4  _S799 = make_float4 (0.0f);
    *&((&_S799)->w) = _S795;
    *&((&_S799)->z) = _S796;
    *&((&_S799)->y) = _S797;
    *&((&_S799)->x) = _S798;
    dpquat_0->primal_0 = (*dpquat_0).primal_0;
    dpquat_0->differential_0 = _S799;
    return;
}

inline __device__ void s_bwd_compute_3dgut_iscl_rot_0(DiffPair_vectorx3Cfloatx2C4x3E_0 * _S800, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S801, Matrix<float, 3, 3>  _S802)
{
    s_bwd_prop_compute_3dgut_iscl_rot_0(_S800, _S801, _S802);
    return;
}

inline __device__ void compute_3dgut_iscl_rot_vjp(float4  quat_6, float3  scale_5, Matrix<float, 3, 3>  v_iscl_rot_0, float4  * v_quat_0, float3  * v_scale_0)
{
    float4  _S803 = make_float4 (0.0f);
    DiffPair_vectorx3Cfloatx2C4x3E_0 dp_quat_0;
    (&dp_quat_0)->primal_0 = quat_6;
    (&dp_quat_0)->differential_0 = _S803;
    float3  _S804 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_scale_0;
    (&dp_scale_0)->primal_0 = scale_5;
    (&dp_scale_0)->differential_0 = _S804;
    s_bwd_compute_3dgut_iscl_rot_0(&dp_quat_0, &dp_scale_0, v_iscl_rot_0);
    *v_quat_0 = dp_quat_0.differential_0;
    *v_scale_0 = dp_scale_0.differential_0;
    return;
}

inline __device__ void _d_cross_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * a_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * b_0, float3  dOut_6)
{
    float _S805 = dOut_6.y;
    float _S806 = dOut_6.z;
    float _S807 = dOut_6.x;
    float _S808 = (*a_0).primal_0.z * _S805 + - (*a_0).primal_0.y * _S806;
    float _S809 = - (*a_0).primal_0.z * _S807 + (*a_0).primal_0.x * _S806;
    float _S810 = (*a_0).primal_0.y * _S807 + - (*a_0).primal_0.x * _S805;
    float3  _S811 = make_float3 (- (*b_0).primal_0.z * _S805 + (*b_0).primal_0.y * _S806, (*b_0).primal_0.z * _S807 + - (*b_0).primal_0.x * _S806, - (*b_0).primal_0.y * _S807 + (*b_0).primal_0.x * _S805);
    a_0->primal_0 = (*a_0).primal_0;
    a_0->differential_0 = _S811;
    float3  _S812 = make_float3 (_S808, _S809, _S810);
    b_0->primal_0 = (*b_0).primal_0;
    b_0->differential_0 = _S812;
    return;
}

inline __device__ float3  cross_0(float3  left_6, float3  right_6)
{
    float _S813 = left_6.y;
    float _S814 = right_6.z;
    float _S815 = left_6.z;
    float _S816 = right_6.y;
    float _S817 = right_6.x;
    float _S818 = left_6.x;
    return make_float3 (_S813 * _S814 - _S815 * _S816, _S815 * _S817 - _S818 * _S814, _S818 * _S816 - _S813 * _S817);
}

inline __device__ float evaluate_alpha_3dgs(float3  mean_0, Matrix<float, 3, 3>  iscl_rot_0, float opacity_0, float3  ray_o_0, float3  ray_d_0)
{
    float3  grd_0 = mul_2(iscl_rot_0, ray_d_0);
    float3  gcrod_0 = cross_0(grd_0, mul_2(iscl_rot_0, ray_o_0 - mean_0));
    return opacity_0 * (F32_exp((-0.5f * dot_1(gcrod_0, gcrod_0) / dot_1(grd_0, grd_0))));
}

inline __device__ float3  s_primal_ctx_mul_0(Matrix<float, 3, 3>  _S819, float3  _S820)
{
    return mul_2(_S819, _S820);
}

inline __device__ float3  s_primal_ctx_cross_0(float3  _S821, float3  _S822)
{
    return cross_0(_S821, _S822);
}

inline __device__ float s_primal_ctx_dot_0(float3  _S823, float3  _S824)
{
    return dot_1(_S823, _S824);
}

inline __device__ float s_primal_ctx_exp_1(float _S825)
{
    return (F32_exp((_S825)));
}

inline __device__ void s_bwd_prop_exp_1(DiffPair_float_0 * _S826, float _S827)
{
    _d_exp_0(_S826, _S827);
    return;
}

inline __device__ void s_bwd_prop_dot_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S828, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S829, float _S830)
{
    _d_dot_0(_S828, _S829, _S830);
    return;
}

inline __device__ void s_bwd_prop_cross_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S831, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S832, float3  _S833)
{
    _d_cross_0(_S831, _S832, _S833);
    return;
}

inline __device__ void s_bwd_prop_mul_2(DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S834, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S835, float3  _S836)
{
    _d_mul_0(_S834, _S835, _S836);
    return;
}

inline __device__ void s_bwd_prop_evaluate_alpha_3dgs_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * dpmean_0, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * dpiscl_rot_0, DiffPair_float_0 * dpopacity_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpray_o_0, DiffPair_vectorx3Cfloatx2C3x3E_0 * dpray_d_0, float _s_dOut_3)
{
    float3  _S837 = (*dpray_o_0).primal_0 - (*dpmean_0).primal_0;
    float3  _S838 = s_primal_ctx_mul_0((*dpiscl_rot_0).primal_0, _S837);
    float3  _S839 = s_primal_ctx_mul_0((*dpiscl_rot_0).primal_0, (*dpray_d_0).primal_0);
    float3  _S840 = s_primal_ctx_cross_0(_S839, _S838);
    float _S841 = -0.5f * s_primal_ctx_dot_0(_S840, _S840);
    float _S842 = s_primal_ctx_dot_0(_S839, _S839);
    float _S843 = _S841 / _S842;
    float _S844 = _S842 * _S842;
    float _S845 = (*dpopacity_0).primal_0 * _s_dOut_3;
    float _S846 = s_primal_ctx_exp_1(_S843) * _s_dOut_3;
    DiffPair_float_0 _S847;
    (&_S847)->primal_0 = _S843;
    (&_S847)->differential_0 = 0.0f;
    s_bwd_prop_exp_1(&_S847, _S845);
    float _S848 = _S847.differential_0 / _S844;
    float _S849 = _S841 * - _S848;
    float _S850 = _S842 * _S848;
    float3  _S851 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S852;
    (&_S852)->primal_0 = _S839;
    (&_S852)->differential_0 = _S851;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S853;
    (&_S853)->primal_0 = _S839;
    (&_S853)->differential_0 = _S851;
    s_bwd_prop_dot_0(&_S852, &_S853, _S849);
    float _S854 = -0.5f * _S850;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S855;
    (&_S855)->primal_0 = _S840;
    (&_S855)->differential_0 = _S851;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S856;
    (&_S856)->primal_0 = _S840;
    (&_S856)->differential_0 = _S851;
    s_bwd_prop_dot_0(&_S855, &_S856, _S854);
    float3  _S857 = _S856.differential_0 + _S855.differential_0;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S858;
    (&_S858)->primal_0 = _S839;
    (&_S858)->differential_0 = _S851;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S859;
    (&_S859)->primal_0 = _S838;
    (&_S859)->differential_0 = _S851;
    s_bwd_prop_cross_0(&_S858, &_S859, _S857);
    float3  _S860 = _S853.differential_0 + _S852.differential_0 + _S858.differential_0;
    Matrix<float, 3, 3>  _S861 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S862;
    (&_S862)->primal_0 = (*dpiscl_rot_0).primal_0;
    (&_S862)->differential_0 = _S861;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S863;
    (&_S863)->primal_0 = (*dpray_d_0).primal_0;
    (&_S863)->differential_0 = _S851;
    s_bwd_prop_mul_2(&_S862, &_S863, _S860);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S864;
    (&_S864)->primal_0 = (*dpiscl_rot_0).primal_0;
    (&_S864)->differential_0 = _S861;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S865;
    (&_S865)->primal_0 = _S837;
    (&_S865)->differential_0 = _S851;
    s_bwd_prop_mul_2(&_S864, &_S865, _S859.differential_0);
    float3  _S866 = - _S865.differential_0;
    dpray_d_0->primal_0 = (*dpray_d_0).primal_0;
    dpray_d_0->differential_0 = _S863.differential_0;
    dpray_o_0->primal_0 = (*dpray_o_0).primal_0;
    dpray_o_0->differential_0 = _S865.differential_0;
    dpopacity_0->primal_0 = (*dpopacity_0).primal_0;
    dpopacity_0->differential_0 = _S846;
    Matrix<float, 3, 3>  _S867 = _S862.differential_0 + _S864.differential_0;
    dpiscl_rot_0->primal_0 = (*dpiscl_rot_0).primal_0;
    dpiscl_rot_0->differential_0 = _S867;
    dpmean_0->primal_0 = (*dpmean_0).primal_0;
    dpmean_0->differential_0 = _S866;
    return;
}

inline __device__ void s_bwd_evaluate_alpha_3dgs_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S868, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S869, DiffPair_float_0 * _S870, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S871, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S872, float _S873)
{
    s_bwd_prop_evaluate_alpha_3dgs_0(_S868, _S869, _S870, _S871, _S872, _S873);
    return;
}

inline __device__ void evaluate_alpha_3dgs_vjp(float3  mean_1, Matrix<float, 3, 3>  iscl_rot_1, float opacity_1, float3  ray_o_1, float3  ray_d_1, float v_alpha_0, float3  * v_mean_0, Matrix<float, 3, 3>  * v_iscl_rot_1, float * v_opacity_0, float3  * v_ray_o_1, float3  * v_ray_d_1)
{
    float3  _S874 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_mean_0;
    (&dp_mean_0)->primal_0 = mean_1;
    (&dp_mean_0)->differential_0 = _S874;
    Matrix<float, 3, 3>  _S875 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 dp_iscl_rot_0;
    (&dp_iscl_rot_0)->primal_0 = iscl_rot_1;
    (&dp_iscl_rot_0)->differential_0 = _S875;
    DiffPair_float_0 dp_opacity_0;
    (&dp_opacity_0)->primal_0 = opacity_1;
    (&dp_opacity_0)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ray_o_0;
    (&dp_ray_o_0)->primal_0 = ray_o_1;
    (&dp_ray_o_0)->differential_0 = _S874;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ray_d_0;
    (&dp_ray_d_0)->primal_0 = ray_d_1;
    (&dp_ray_d_0)->differential_0 = _S874;
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
    float3  _S876 = (*dpray_o_1).primal_0 - (*dpmean_1).primal_0;
    float3  _S877 = s_primal_ctx_mul_0((*dpiscl_rot_1).primal_0, _S876);
    float3  _S878 = s_primal_ctx_mul_0((*dpiscl_rot_1).primal_0, (*dpray_d_1).primal_0);
    float _S879 = s_primal_ctx_dot_0(_S878, _S878);
    float _S880 = dpdepth_0 / (_S879 * _S879);
    float _S881 = - s_primal_ctx_dot_0(_S877, _S878) * - _S880;
    float _S882 = _S879 * _S880;
    float3  _S883 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S884;
    (&_S884)->primal_0 = _S878;
    (&_S884)->differential_0 = _S883;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S885;
    (&_S885)->primal_0 = _S878;
    (&_S885)->differential_0 = _S883;
    s_bwd_prop_dot_0(&_S884, &_S885, _S881);
    float _S886 = - _S882;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S887;
    (&_S887)->primal_0 = _S877;
    (&_S887)->differential_0 = _S883;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S888;
    (&_S888)->primal_0 = _S878;
    (&_S888)->differential_0 = _S883;
    s_bwd_prop_dot_0(&_S887, &_S888, _S886);
    float3  _S889 = _S885.differential_0 + _S884.differential_0 + _S888.differential_0;
    Matrix<float, 3, 3>  _S890 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S891;
    (&_S891)->primal_0 = (*dpiscl_rot_1).primal_0;
    (&_S891)->differential_0 = _S890;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S892;
    (&_S892)->primal_0 = (*dpray_d_1).primal_0;
    (&_S892)->differential_0 = _S883;
    s_bwd_prop_mul_2(&_S891, &_S892, _S889);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 _S893;
    (&_S893)->primal_0 = (*dpiscl_rot_1).primal_0;
    (&_S893)->differential_0 = _S890;
    DiffPair_vectorx3Cfloatx2C3x3E_0 _S894;
    (&_S894)->primal_0 = _S876;
    (&_S894)->differential_0 = _S883;
    s_bwd_prop_mul_2(&_S893, &_S894, _S887.differential_0);
    float3  _S895 = - _S894.differential_0;
    dpray_d_1->primal_0 = (*dpray_d_1).primal_0;
    dpray_d_1->differential_0 = _S892.differential_0;
    dpray_o_1->primal_0 = (*dpray_o_1).primal_0;
    dpray_o_1->differential_0 = _S894.differential_0;
    dprgb_0->primal_0 = (*dprgb_0).primal_0;
    dprgb_0->differential_0 = dpout_rgb_0;
    dpopacity_1->primal_0 = (*dpopacity_1).primal_0;
    dpopacity_1->differential_0 = 0.0f;
    Matrix<float, 3, 3>  _S896 = _S891.differential_0 + _S893.differential_0;
    dpiscl_rot_1->primal_0 = (*dpiscl_rot_1).primal_0;
    dpiscl_rot_1->differential_0 = _S896;
    dpmean_1->primal_0 = (*dpmean_1).primal_0;
    dpmean_1->differential_0 = _S895;
    return;
}

inline __device__ void s_bwd_evaluate_color_3dgs_0(DiffPair_vectorx3Cfloatx2C3x3E_0 * _S897, DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 * _S898, DiffPair_float_0 * _S899, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S900, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S901, DiffPair_vectorx3Cfloatx2C3x3E_0 * _S902, float3  _S903, float _S904)
{
    s_bwd_prop_evaluate_color_3dgs_0(_S897, _S898, _S899, _S900, _S901, _S902, _S903, _S904);
    return;
}

inline __device__ void evaluate_color_3dgs_vjp(float3  mean_3, Matrix<float, 3, 3>  iscl_rot_3, float opacity_3, float3  rgb_1, float3  ray_o_3, float3  ray_d_3, float3  v_out_rgb_0, float v_depth_0, float3  * v_mean_1, Matrix<float, 3, 3>  * v_iscl_rot_2, float * v_opacity_1, float3  * v_rgb_0, float3  * v_ray_o_2, float3  * v_ray_d_2)
{
    float3  _S905 = make_float3 (0.0f);
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_mean_1;
    (&dp_mean_1)->primal_0 = mean_3;
    (&dp_mean_1)->differential_0 = _S905;
    Matrix<float, 3, 3>  _S906 = makeMatrix<float, 3, 3> (0.0f);
    DiffPair_matrixx3Cfloatx2C3x2C3x3E_0 dp_iscl_rot_1;
    (&dp_iscl_rot_1)->primal_0 = iscl_rot_3;
    (&dp_iscl_rot_1)->differential_0 = _S906;
    DiffPair_float_0 dp_opacity_1;
    (&dp_opacity_1)->primal_0 = opacity_3;
    (&dp_opacity_1)->differential_0 = 0.0f;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_rgb_0;
    (&dp_rgb_0)->primal_0 = rgb_1;
    (&dp_rgb_0)->differential_0 = _S905;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ray_o_1;
    (&dp_ray_o_1)->primal_0 = ray_o_3;
    (&dp_ray_o_1)->differential_0 = _S905;
    DiffPair_vectorx3Cfloatx2C3x3E_0 dp_ray_d_1;
    (&dp_ray_d_1)->primal_0 = ray_d_3;
    (&dp_ray_d_1)->differential_0 = _S905;
    s_bwd_evaluate_color_3dgs_0(&dp_mean_1, &dp_iscl_rot_1, &dp_opacity_1, &dp_rgb_0, &dp_ray_o_1, &dp_ray_d_1, v_out_rgb_0, v_depth_0);
    *v_mean_1 = dp_mean_1.differential_0;
    *v_iscl_rot_2 = dp_iscl_rot_1.differential_0;
    *v_opacity_1 = dp_opacity_1.differential_0;
    *v_rgb_0 = dp_rgb_0.differential_0;
    *v_ray_o_2 = dp_ray_o_1.differential_0;
    *v_ray_d_2 = dp_ray_d_1.differential_0;
    return;
}

inline __device__ float view_radius_3dgs(float3  mean_4, float3  log_scale_0, float logit_opacity_0, float3  campos_0)
{
    float radius_0 = (F32_exp(((F32_max((log_scale_0.x), ((F32_max((log_scale_0.y), (log_scale_0.z))))))))) * (F32_sqrt((2.0f * (F32_log(((F32_max((255.0f / (1.0f + (F32_exp((- logit_opacity_0))))), (1.0f)))))))));
    float dist_0 = length_1(mean_4 - campos_0);
    return radius_0 / ((F32_max((dist_0), (radius_0))) + (F32_sqrt(((F32_max((dist_0 * dist_0 - radius_0 * radius_0), (0.0f)))))));
}

