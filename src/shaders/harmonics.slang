// Based on https://github.com/nerfstudio-project/gsplat/blob/main/gsplat/cuda/csrc/SphericalHarmonicsCUDA.cu

/*
 * SPDX-FileCopyrightText: Copyright 2025 the Regents of the University of California, Nerfstudio Team and contributors. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


// We've noticed poor register pressure when using Slang autodiff for SH
// So we write SH (as well as other harmonics) manually


// ----------------------------------------------------------------------------
// Block-level reduce + atomic add (BLOCK_SIZE = 512 = 16 warps * 32 lanes).
//
// Mirrors common_utils.cuh `blockAtomicAdd<512>(float3*, float3)`: each warp
// shfl-reduces its lane values, lane 0 writes one slot per channel into a
// 16x3 groupshared table, a single syncthreads, then warps 0..2 each take
// one channel, warp-reduce across the 16 partials, and lane 0 of that warp
// issues a single global atomicAdd. Used by inplace=3 mode of the SH vjp to
// coalesce per-pixel writes (otherwise every pixel hammers the same 25 global
// SH coefficient slots with one atomicAdd each -> severe serialization).
//
// REQUIREMENTS for the caller:
//   - Launch the kernel with EXACTLY 512 threads/block.
//   - Every thread in the block must reach EVERY call to this helper (no
//     early returns / warp-divergent skips); threads with no contribution
//     should just pass val = 0.
//   - thread_id is the flat thread index within the block:
//       threadIdx.x + threadIdx.y*blockDim.x + threadIdx.z*blockDim.x*blockDim.y
//
// Single GroupMemoryBarrierWithGroupSync per call (matches the C++ helper).
// No trailing barrier: subsequent calls write disjoint slots, and warps 0-2
// have already read into registers before the next call's write occurs.
// ----------------------------------------------------------------------------

static const uint _SH_BLOCK_SIZE = 512u;
static const uint _SH_WARP_SIZE  = 32u;
static const uint _SH_NUM_WARPS  = _SH_BLOCK_SIZE / _SH_WARP_SIZE;  // 16

groupshared float _sh_block_reduce_shared[3u * _SH_NUM_WARPS];

[ForceInline]
void sh_block_atomic_add_f3_512(float3 *dst, uint idx, float3 val, uint thread_id) {
    val.x = isfinite(val.x) ? val.x : 0.0f;
    val.y = isfinite(val.y) ? val.y : 0.0f;
    val.z = isfinite(val.z) ? val.z : 0.0f;

    // Warp-reduce each channel.
    val.x = WaveActiveSum(val.x);
    val.y = WaveActiveSum(val.y);
    val.z = WaveActiveSum(val.z);

    uint warp_id = thread_id / _SH_WARP_SIZE;
    uint lane_id = thread_id % _SH_WARP_SIZE;

    if (lane_id == 0u) {
        _sh_block_reduce_shared[warp_id + 0u * _SH_NUM_WARPS] = val.x;
        _sh_block_reduce_shared[warp_id + 1u * _SH_NUM_WARPS] = val.y;
        _sh_block_reduce_shared[warp_id + 2u * _SH_NUM_WARPS] = val.z;
    }
    GroupMemoryBarrierWithGroupSync();

    if (warp_id < 3u) {
        float v = lane_id < _SH_NUM_WARPS
            ? _sh_block_reduce_shared[lane_id + warp_id * _SH_NUM_WARPS] : 0.0f;
        v = WaveActiveSum(v);
        if (lane_id == 0u && v != 0.0f) {
            if (warp_id == 0u)      InterlockedAdd(dst[idx].x, v);
            else if (warp_id == 1u) InterlockedAdd(dst[idx].y, v);
            else                    InterlockedAdd(dst[idx].z, v);
        }
    }
}


// ----------------------------------------------------------------------------
// SH value-quantization load helpers (q8 / q16). Cell addressing and both
// layouts: docs/notes/sh-quant-layout.md -- callers pass the
// (bounds_stride, pair_pitch) pair of whichever buffer they hold.
// ----------------------------------------------------------------------------

// Cell index of coefficient `c` of the splat whose cells start at `base`.
[ForceInline]
int64_t sh_cell_at(int64_t base, int c, int64_t pair_pitch) {
    // Branch-free (per cell): AoS folds to (c >> 0) * 1 + (c & 0).
    int pair = int(min(pair_pitch, int64_t(1)));
    int64_t pitch = pair_pitch + int64_t(1 - pair);
    return base + (int64_t)(c >> pair) * pitch + (int64_t)(c & pair);
}

// First cell of splat `gid` in the FPBO layout (pair_pitch 512).
[ForceInline]
int64_t sh_fpbo_base(int64_t gid, int cells_per_splat) {
    return (gid >> 8) * (int64_t)((cells_per_splat + 1) >> 1) * 512
           + (gid & 255) * 2;
}

[ForceInline]
float3 _sh_load_q8(uint8_t* packed, float2* bounds, int64_t base, int j, int64_t bounds_stride, int64_t pair_pitch) {
    int64_t cell0 = sh_cell_at(base, 3*j, pair_pitch);
    int64_t cell1 = sh_cell_at(base, 3*j+1, pair_pitch);
    int64_t cell2 = sh_cell_at(base, 3*j+2, pair_pitch);
    float2 mm0 = bounds[(int)(cell0 / bounds_stride)];
    float2 mm1 = bounds[(int)(cell1 / bounds_stride)];
    float2 mm2 = bounds[(int)(cell2 / bounds_stride)];
    const float inv = 1.0f / 255.0f;
    return float3(
        mm0.x + (mm0.y - mm0.x) * (float(packed[cell0]) * inv),
        mm1.x + (mm1.y - mm1.x) * (float(packed[cell1]) * inv),
        mm2.x + (mm2.y - mm2.x) * (float(packed[cell2]) * inv)
    );
}

[ForceInline]
float3 _sh_load_q16(uint16_t* packed, float2* bounds, int64_t base, int j, int64_t bounds_stride, int64_t pair_pitch) {
    int64_t cell0 = sh_cell_at(base, 3*j, pair_pitch);
    int64_t cell1 = sh_cell_at(base, 3*j+1, pair_pitch);
    int64_t cell2 = sh_cell_at(base, 3*j+2, pair_pitch);
    float2 mm0 = bounds[(int)(cell0 / bounds_stride)];
    float2 mm1 = bounds[(int)(cell1 / bounds_stride)];
    float2 mm2 = bounds[(int)(cell2 / bounds_stride)];
    const float inv = 1.0f / 65535.0f;
    return float3(
        mm0.x + (mm0.y - mm0.x) * (float(packed[cell0]) * inv),
        mm1.x + (mm1.y - mm1.x) * (float(packed[cell1]) * inv),
        mm2.x + (mm2.y - mm2.x) * (float(packed[cell2]) * inv)
    );
}


[ForceInline]
[Differentiable]
float3 mean_viewmat_to_unnormalized_viewdir(float3 mean, float3x3 R, float3 t) {
    // float3 campos = -mul(transpose(R), t);
    // return mean - campos;
    return mean + mul(transpose(R), t);
}


// [CudaDeviceExport]
[ForceInline]
float3 sh_coeffs_to_color(
    const int degree,
    const float3 viewdir,
    float3 coeff_dc,
    float3* coeffs
) {
    float3 colors = 0.2820947917738781f * coeff_dc;

    if (degree <= 0)
        return colors;

    float inv_norm = rsqrt(
        viewdir.x * viewdir.x + viewdir.y * viewdir.y + viewdir.z * viewdir.z
    );
    float x = viewdir.x * inv_norm;
    float y = viewdir.y * inv_norm;
    float z = viewdir.z * inv_norm;

    float fTmp0A = 0.48860251190292f;
    colors += fTmp0A *
            (-y * coeffs[0] +
            z * coeffs[1] -
            x * coeffs[2]);

    if (degree <= 1)
        return colors;

    float z2 = z * z;
    float fTmp0B = -1.092548430592079f * z;
    float fTmp1A = 0.5462742152960395f;
    float fC1 = x * x - y * y;
    float fS1 = 2.f * x * y;
    float pSH6 = (0.9461746957575601f * z2 - 0.3153915652525201f);
    float pSH7 = fTmp0B * x;
    float pSH5 = fTmp0B * y;
    float pSH8 = fTmp1A * fC1;
    float pSH4 = fTmp1A * fS1;
    colors +=
        pSH4 * coeffs[3] + pSH5 * coeffs[4] +
        pSH6 * coeffs[5] + pSH7 * coeffs[6] +
        pSH8 * coeffs[7];

    if (degree <= 2)
        return colors;

    float fTmp0C = -2.285228997322329f * z2 + 0.4570457994644658f;
    float fTmp1B = 1.445305721320277f * z;
    float fTmp2A = -0.5900435899266435f;
    float fC2 = x * fC1 - y * fS1;
    float fS2 = x * fS1 + y * fC1;
    float pSH12 = z * (1.865881662950577f * z2 - 1.119528997770346f);
    float pSH13 = fTmp0C * x;
    float pSH11 = fTmp0C * y;
    float pSH14 = fTmp1B * fC1;
    float pSH10 = fTmp1B * fS1;
    float pSH15 = fTmp2A * fC2;
    float pSH9  = fTmp2A * fS2;
    colors += pSH9  * coeffs[8] +
            pSH10 * coeffs[9] +
            pSH11 * coeffs[10] +
            pSH12 * coeffs[11] +
            pSH13 * coeffs[12] +
            pSH14 * coeffs[13] +
            pSH15 * coeffs[14];

    if (degree <= 3)
        return colors;

    float fTmp0D =
        z * (-4.683325804901025f * z2 + 2.007139630671868f);
    float fTmp1C = 3.31161143515146f * z2 - 0.47308734787878f;
    float fTmp2B = -1.770130769779931f * z;
    float fC3 = x * fC2 - y * fS2;
    float fS3 = x * fS2 + y * fC2;
    float pSH20 =
        (1.984313483298443f * z * pSH12 -
            1.006230589874905f * pSH6);
    float pSH21 = fTmp0D * x;
    float pSH19 = fTmp0D * y;
    float pSH22 = fTmp1C * fC1;
    float pSH18 = fTmp1C * fS1;
    float pSH23 = fTmp2B * fC2;
    float pSH17 = fTmp2B * fS2;
    float pSH24 = 0.6258357354491763f * fC3;
    float pSH16 = 0.6258357354491763f * fS3;

    colors += pSH16 * coeffs[15] +
                pSH17 * coeffs[16] +
                pSH18 * coeffs[17] +
                pSH19 * coeffs[18] +
                pSH20 * coeffs[19] +
                pSH21 * coeffs[20] +
                pSH22 * coeffs[21] +
                pSH23 * coeffs[22] +
                pSH24 * coeffs[23];

    return colors;
}


[ForceInline]
float sh_coeffs_to_color_c<let c: int>(
    const int degree,
    const float3 viewdir,
    float3 coeff_dc,
    float* coeffs
) {
    float colors = 0.2820947917738781f * (
        c == 0 ? coeff_dc.x : c == 1 ? coeff_dc.y : coeff_dc.z
    );

    if (degree <= 0)
        return colors;

    float inv_norm = rsqrt(
        viewdir.x * viewdir.x + viewdir.y * viewdir.y + viewdir.z * viewdir.z
    );
    float x = viewdir.x * inv_norm;
    float y = viewdir.y * inv_norm;
    float z = viewdir.z * inv_norm;

    float fTmp0A = 0.48860251190292f;
    colors += fTmp0A *
            (-y * coeffs[3*0+c] +
            z * coeffs[3*1+c] -
            x * coeffs[3*2+c]);

    if (degree <= 1)
        return colors;

    float z2 = z * z;
    float fTmp0B = -1.092548430592079f * z;
    float fTmp1A = 0.5462742152960395f;
    float fC1 = x * x - y * y;
    float fS1 = 2.f * x * y;
    float pSH6 = (0.9461746957575601f * z2 - 0.3153915652525201f);
    float pSH7 = fTmp0B * x;
    float pSH5 = fTmp0B * y;
    float pSH8 = fTmp1A * fC1;
    float pSH4 = fTmp1A * fS1;
    colors +=
        pSH4 * coeffs[3*3+c] + pSH5 * coeffs[3*4+c] +
        pSH6 * coeffs[3*5+c] + pSH7 * coeffs[3*6+c] +
        pSH8 * coeffs[3*7+c];

    if (degree <= 2)
        return colors;

    float fTmp0C = -2.285228997322329f * z2 + 0.4570457994644658f;
    float fTmp1B = 1.445305721320277f * z;
    float fTmp2A = -0.5900435899266435f;
    float fC2 = x * fC1 - y * fS1;
    float fS2 = x * fS1 + y * fC1;
    float pSH12 = z * (1.865881662950577f * z2 - 1.119528997770346f);
    float pSH13 = fTmp0C * x;
    float pSH11 = fTmp0C * y;
    float pSH14 = fTmp1B * fC1;
    float pSH10 = fTmp1B * fS1;
    float pSH15 = fTmp2A * fC2;
    float pSH9  = fTmp2A * fS2;
    colors += pSH9  * coeffs[3*8+c] +
            pSH10 * coeffs[3*9+c] +
            pSH11 * coeffs[3*10+c] +
            pSH12 * coeffs[3*11+c] +
            pSH13 * coeffs[3*12+c] +
            pSH14 * coeffs[3*13+c] +
            pSH15 * coeffs[3*14+c];

    if (degree <= 3)
        return colors;

    float fTmp0D =
        z * (-4.683325804901025f * z2 + 2.007139630671868f);
    float fTmp1C = 3.31161143515146f * z2 - 0.47308734787878f;
    float fTmp2B = -1.770130769779931f * z;
    float fC3 = x * fC2 - y * fS2;
    float fS3 = x * fS2 + y * fC2;
    float pSH20 =
        (1.984313483298443f * z * pSH12 -
            1.006230589874905f * pSH6);
    float pSH21 = fTmp0D * x;
    float pSH19 = fTmp0D * y;
    float pSH22 = fTmp1C * fC1;
    float pSH18 = fTmp1C * fS1;
    float pSH23 = fTmp2B * fC2;
    float pSH17 = fTmp2B * fS2;
    float pSH24 = 0.6258357354491763f * fC3;
    float pSH16 = 0.6258357354491763f * fS3;

    colors += pSH16 * coeffs[3*15+c] +
                pSH17 * coeffs[3*16+c] +
                pSH18 * coeffs[3*17+c] +
                pSH19 * coeffs[3*18+c] +
                pSH20 * coeffs[3*19+c] +
                pSH21 * coeffs[3*20+c] +
                pSH22 * coeffs[3*21+c] +
                pSH23 * coeffs[3*22+c] +
                pSH24 * coeffs[3*23+c];

    return colors;
}

// [CudaDeviceExport]
[ForceInline]
float3 sh_coeffs_to_color(
    const int degree,
    const float3 viewdir,
    float3 coeff_dc,
    float* coeffs
) {
    return float3(
        sh_coeffs_to_color_c<0>(degree, viewdir, coeff_dc, coeffs),
        sh_coeffs_to_color_c<1>(degree, viewdir, coeff_dc, coeffs),
        sh_coeffs_to_color_c<2>(degree, viewdir, coeff_dc, coeffs)
    );
}


[ForceInline]
void _sh_coeffs_to_color_vjp<let inplace: int>(
    const int degree,
    const float3 dir,
    float3 coeff_dc,
    float3 *coeffs,
    const float3 v_colors,
    inout float3 v_coeff_dc,
    float3 *v_coeffs,
    inout float3 v_dir,
    // Flat thread index in block; only consulted when inplace == 3 (block-
    // reduced atomic). Caller passes 0u for inplace 0/1/2.
    uint thread_id
) {
    // inplace:
    // 0: assign
    // 1: add
    // 2: atomic add (per-thread; one atomicAdd per call per channel)
    // 3: block-reduced atomic add (one atomicAdd per block per channel via
    //    sh_block_atomic_add_f3_512); requires BLOCK_SIZE == 512 and every
    //    thread to participate in every call.

    #define sv(i, v) \
        if (inplace == 0) v_coeffs[i] = (v); \
        else if (inplace == 1) v_coeffs[i] += (v); \
        else if (inplace == 2) { \
            float3 temp = (v); \
            if (isfinite(dot(temp, temp)) && dot(temp, temp) != 0.0) { \
                InterlockedAdd(v_coeffs[i].x, temp.x); \
                InterlockedAdd(v_coeffs[i].y, temp.y); \
                InterlockedAdd(v_coeffs[i].z, temp.z); \
            } \
        } \
        else if (inplace == 3) { \
            sh_block_atomic_add_f3_512(v_coeffs, (uint)(i), (v), thread_id); \
        }

    if (inplace != 0) {
        v_coeff_dc += 0.2820947917738781f * v_colors;
    } else {
        v_coeff_dc = 0.2820947917738781f * v_colors;
    }
    if (degree <= 0) {
        if (inplace == 0) v_dir = float3(0);
        return;
    }

    float inorm = rsqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    float x = dir.x * inorm;
    float y = dir.y * inorm;
    float z = dir.z * inorm;
    float v_x = 0.f, v_y = 0.f, v_z = 0.f;

    sv(0, -0.48860251190292f * y * v_colors);
    sv(1, 0.48860251190292f * z * v_colors);
    sv(2, -0.48860251190292f * x * v_colors);

    v_x += -0.48860251190292f * dot(coeffs[2], v_colors);
    v_y += -0.48860251190292f * dot(coeffs[0], v_colors);
    v_z += 0.48860251190292f * dot(coeffs[1], v_colors);

    if (degree <= 1) {
        float3 dir_n = float3(x, y, z);
        float3 v_dir_n = float3(v_x, v_y, v_z);
        if (inplace != 0)
            v_dir += (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        else
            v_dir = (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        return;
    }

    float z2 = z * z;
    float fTmp0B = -1.092548430592079f * z;
    float fC1 = x * x - y * y;
    float fS1 = 2.f * x * y;
    float pSH6 = (0.9461746957575601f * z2 - 0.3153915652525201f);
    float pSH7 = fTmp0B * x;
    float pSH5 = fTmp0B * y;
    float pSH8 = 0.5462742152960395f * fC1;
    float pSH4 = 0.5462742152960395f * fS1;
    sv(3, pSH4 * v_colors);
    sv(4, pSH5 * v_colors);
    sv(5, pSH6 * v_colors);
    sv(6, pSH7 * v_colors);
    sv(7, pSH8 * v_colors);

    float fTmp0B_z, fC1_x, fC1_y, fS1_x, fS1_y, pSH6_z, pSH7_x, pSH7_z, pSH5_y,
        pSH5_z, pSH8_x, pSH8_y, pSH4_x, pSH4_y;

    fTmp0B_z = -1.092548430592079f;
    fC1_x = 2.f * x;
    fC1_y = -2.f * y;
    fS1_x = 2.f * y;
    fS1_y = 2.f * x;
    pSH6_z = 2.f * 0.9461746957575601f * z;
    pSH7_x = fTmp0B;
    pSH7_z = fTmp0B_z * x;
    pSH5_y = fTmp0B;
    pSH5_z = fTmp0B_z * y;
    pSH8_x = 0.5462742152960395f * fC1_x;
    pSH8_y = 0.5462742152960395f * fC1_y;
    pSH4_x = 0.5462742152960395f * fS1_x;
    pSH4_y = 0.5462742152960395f * fS1_y;

    v_x += dot(v_colors,
            pSH4_x * coeffs[3] + pSH8_x * coeffs[7] +
            pSH7_x * coeffs[6]);
    v_y += dot(v_colors,
            pSH4_y * coeffs[3] + pSH8_y * coeffs[7] +
            pSH5_y * coeffs[4]);
    v_z += dot(v_colors,
            pSH6_z * coeffs[5] + pSH7_z * coeffs[6] +
            pSH5_z * coeffs[4]);

    if (degree <= 2) {
        float3 dir_n = float3(x, y, z);
        float3 v_dir_n = float3(v_x, v_y, v_z);
        if (inplace != 0)
            v_dir += (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        else
            v_dir = (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        return;
    }

    float fTmp0C = -2.285228997322329f * z2 + 0.4570457994644658f;
    float fTmp1B = 1.445305721320277f * z;
    float fC2 = x * fC1 - y * fS1;
    float fS2 = x * fS1 + y * fC1;
    float pSH12 = z * (1.865881662950577f * z2 - 1.119528997770346f);
    float pSH13 = fTmp0C * x;
    float pSH11 = fTmp0C * y;
    float pSH14 = fTmp1B * fC1;
    float pSH10 = fTmp1B * fS1;
    float pSH15 = -0.5900435899266435f * fC2;
    float pSH9 = -0.5900435899266435f * fS2;
    sv(8, pSH9 * v_colors);
    sv(9, pSH10 * v_colors);
    sv(10, pSH11 * v_colors);
    sv(11, pSH12 * v_colors);
    sv(12, pSH13 * v_colors);
    sv(13, pSH14 * v_colors);
    sv(14, pSH15 * v_colors);

    float fTmp0C_z, fTmp1B_z, fC2_x, fC2_y, fS2_x, fS2_y, pSH12_z, pSH13_x,
        pSH13_z, pSH11_y, pSH11_z, pSH14_x, pSH14_y, pSH14_z, pSH10_x, pSH10_y,
        pSH10_z, pSH15_x, pSH15_y, pSH9_x, pSH9_y;

    fTmp0C_z = -2.285228997322329f * 2.f * z;
    fTmp1B_z = 1.445305721320277f;
    fC2_x = fC1 + x * fC1_x - y * fS1_x;
    fC2_y = x * fC1_y - fS1 - y * fS1_y;
    fS2_x = fS1 + x * fS1_x + y * fC1_x;
    fS2_y = x * fS1_y + fC1 + y * fC1_y;
    pSH12_z = 3.f * 1.865881662950577f * z2 - 1.119528997770346f;
    pSH13_x = fTmp0C;
    pSH13_z = fTmp0C_z * x;
    pSH11_y = fTmp0C;
    pSH11_z = fTmp0C_z * y;
    pSH14_x = fTmp1B * fC1_x;
    pSH14_y = fTmp1B * fC1_y;
    pSH14_z = fTmp1B_z * fC1;
    pSH10_x = fTmp1B * fS1_x;
    pSH10_y = fTmp1B * fS1_y;
    pSH10_z = fTmp1B_z * fS1;
    pSH15_x = -0.5900435899266435f * fC2_x;
    pSH15_y = -0.5900435899266435f * fC2_y;
    pSH9_x = -0.5900435899266435f * fS2_x;
    pSH9_y = -0.5900435899266435f * fS2_y;

    v_x += dot(v_colors,
            pSH9_x * coeffs[8] + pSH15_x * coeffs[14] +
            pSH10_x * coeffs[9] + pSH14_x * coeffs[13] +
            pSH13_x * coeffs[12]);

    v_y += dot(v_colors,
            pSH9_y * coeffs[8] + pSH15_y * coeffs[14] +
            pSH10_y * coeffs[9] + pSH14_y * coeffs[13] +
            pSH11_y * coeffs[10]);

    v_z += dot(v_colors,
            pSH12_z * coeffs[11] + pSH13_z * coeffs[12] +
            pSH11_z * coeffs[10] + pSH14_z * coeffs[13] +
            pSH10_z * coeffs[9]);

    if (degree <= 3) {
        float3 dir_n = float3(x, y, z);
        float3 v_dir_n = float3(v_x, v_y, v_z);
        if (inplace != 0)
            v_dir += (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        else
            v_dir = (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        return;
    }

    float fTmp0D = z * (-4.683325804901025f * z2 + 2.007139630671868f);
    float fTmp1C = 3.31161143515146f * z2 - 0.47308734787878f;
    float fTmp2B = -1.770130769779931f * z;
    float fC3 = x * fC2 - y * fS2;
    float fS3 = x * fS2 + y * fC2;
    float pSH20 = (1.984313483298443f * z * pSH12 + -1.006230589874905f * pSH6);
    float pSH21 = fTmp0D * x;
    float pSH19 = fTmp0D * y;
    float pSH22 = fTmp1C * fC1;
    float pSH18 = fTmp1C * fS1;
    float pSH23 = fTmp2B * fC2;
    float pSH17 = fTmp2B * fS2;
    float pSH24 = 0.6258357354491763f * fC3;
    float pSH16 = 0.6258357354491763f * fS3;
    sv(15, pSH16 * v_colors);
    sv(16, pSH17 * v_colors);
    sv(17, pSH18 * v_colors);
    sv(18, pSH19 * v_colors);
    sv(19, pSH20 * v_colors);
    sv(20, pSH21 * v_colors);
    sv(21, pSH22 * v_colors);
    sv(22, pSH23 * v_colors);
    sv(23, pSH24 * v_colors);

    float fTmp0D_z, fTmp1C_z, fTmp2B_z, fC3_x, fC3_y, fS3_x, fS3_y, pSH20_z,
        pSH21_x, pSH21_z, pSH19_y, pSH19_z, pSH22_x, pSH22_y, pSH22_z, pSH18_x,
        pSH18_y, pSH18_z, pSH23_x, pSH23_y, pSH23_z, pSH17_x, pSH17_y, pSH17_z,
        pSH24_x, pSH24_y, pSH16_x, pSH16_y;

    fTmp0D_z = 3.f * -4.683325804901025f * z2 + 2.007139630671868f;
    fTmp1C_z = 2.f * 3.31161143515146f * z;
    fTmp2B_z = -1.770130769779931f;
    fC3_x = fC2 + x * fC2_x - y * fS2_x;
    fC3_y = x * fC2_y - fS2 - y * fS2_y;
    fS3_x = fS2 + y * fC2_x + x * fS2_x;
    fS3_y = x * fS2_y + fC2 + y * fC2_y;
    pSH20_z = 1.984313483298443f * (pSH12 + z * pSH12_z) +
                -1.006230589874905f * pSH6_z;
    pSH21_x = fTmp0D;
    pSH21_z = fTmp0D_z * x;
    pSH19_y = fTmp0D;
    pSH19_z = fTmp0D_z * y;
    pSH22_x = fTmp1C * fC1_x;
    pSH22_y = fTmp1C * fC1_y;
    pSH22_z = fTmp1C_z * fC1;
    pSH18_x = fTmp1C * fS1_x;
    pSH18_y = fTmp1C * fS1_y;
    pSH18_z = fTmp1C_z * fS1;
    pSH23_x = fTmp2B * fC2_x;
    pSH23_y = fTmp2B * fC2_y;
    pSH23_z = fTmp2B_z * fC2;
    pSH17_x = fTmp2B * fS2_x;
    pSH17_y = fTmp2B * fS2_y;
    pSH17_z = fTmp2B_z * fS2;
    pSH24_x = 0.6258357354491763f * fC3_x;
    pSH24_y = 0.6258357354491763f * fC3_y;
    pSH16_x = 0.6258357354491763f * fS3_x;
    pSH16_y = 0.6258357354491763f * fS3_y;

    v_x += dot(v_colors,
            pSH16_x * coeffs[15] + pSH24_x * coeffs[23] +
            pSH17_x * coeffs[16] + pSH23_x * coeffs[22] +
            pSH18_x * coeffs[17] + pSH22_x * coeffs[21] +
            pSH21_x * coeffs[20]);
    v_y += dot(v_colors,
            pSH16_y * coeffs[15] + pSH24_y * coeffs[23] +
            pSH17_y * coeffs[16] + pSH23_y * coeffs[22] +
            pSH18_y * coeffs[17] + pSH22_y * coeffs[21] +
            pSH19_y * coeffs[18]);
    v_z += dot(v_colors,
            pSH20_z * coeffs[19] + pSH21_z * coeffs[20] +
            pSH19_z * coeffs[18] + pSH22_z * coeffs[21] +
            pSH18_z * coeffs[17] + pSH23_z * coeffs[22] +
            pSH17_z * coeffs[16]);

    float3 dir_n = float3(x, y, z);
    float3 v_dir_n = float3(v_x, v_y, v_z);
    if (inplace != 0)
        v_dir += (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
    else
        v_dir = (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;

    #undef sv
}


[ForceInline]
void _sh_coeffs_to_color_c_vjp<let c: int, let inplace: int>(
    const int degree,
    const float3 dir,
    float3 coeff_dc,
    float *coeffs,
    const float3 v_colors_3,
    inout float3 v_coeff_dc,
    float *v_coeffs,
    inout float3 v_dir
) {
    // inplace:
    // 0: assign
    // 1: add
    // 2: atomic add

    #define sv(i, v) \
        if (inplace == 0) v_coeffs[3*(i)+c] = (v); \
        else if (inplace == 1) v_coeffs[3*(i)+c] += (v); \
        else if (inplace == 2) { \
            float temp = (v); \
            if (isfinite(temp) && temp != 0.0) { \
                InterlockedAdd(v_coeffs[3*(i)+c], temp); \
            } \
        }
    
    float v_colors = (c == 0 ? v_colors_3.x : c == 1 ? v_colors_3.y : v_colors_3.z);

    if (inplace != 0) {
        v_coeff_dc[c] += 0.2820947917738781f * v_colors;
    } else {
        v_coeff_dc[c] = 0.2820947917738781f * v_colors;
    }
    if (degree <= 0) {
        if (inplace == 0) v_dir = float3(0);
        return;
    }

    float inorm = rsqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    float x = dir.x * inorm;
    float y = dir.y * inorm;
    float z = dir.z * inorm;
    float v_x = 0.f, v_y = 0.f, v_z = 0.f;

    sv(0, -0.48860251190292f * y * v_colors);
    sv(1, 0.48860251190292f * z * v_colors);
    sv(2, -0.48860251190292f * x * v_colors);

    v_x += -0.48860251190292f * coeffs[2*3+c] * v_colors;
    v_y += -0.48860251190292f * coeffs[0*3+c] * v_colors;
    v_z += 0.48860251190292f * coeffs[1*3+c] * v_colors;

    if (degree <= 1) {
        float3 dir_n = float3(x, y, z);
        float3 v_dir_n = float3(v_x, v_y, v_z);
        if (inplace != 0)
            v_dir += (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        else
            v_dir = (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        return;
    }

    float z2 = z * z;
    float fTmp0B = -1.092548430592079f * z;
    float fC1 = x * x - y * y;
    float fS1 = 2.f * x * y;
    float pSH6 = (0.9461746957575601f * z2 - 0.3153915652525201f);
    float pSH7 = fTmp0B * x;
    float pSH5 = fTmp0B * y;
    float pSH8 = 0.5462742152960395f * fC1;
    float pSH4 = 0.5462742152960395f * fS1;
    sv(3, pSH4 * v_colors);
    sv(4, pSH5 * v_colors);
    sv(5, pSH6 * v_colors);
    sv(6, pSH7 * v_colors);
    sv(7, pSH8 * v_colors);

    float fTmp0B_z, fC1_x, fC1_y, fS1_x, fS1_y, pSH6_z, pSH7_x, pSH7_z, pSH5_y,
        pSH5_z, pSH8_x, pSH8_y, pSH4_x, pSH4_y;

    fTmp0B_z = -1.092548430592079f;
    fC1_x = 2.f * x;
    fC1_y = -2.f * y;
    fS1_x = 2.f * y;
    fS1_y = 2.f * x;
    pSH6_z = 2.f * 0.9461746957575601f * z;
    pSH7_x = fTmp0B;
    pSH7_z = fTmp0B_z * x;
    pSH5_y = fTmp0B;
    pSH5_z = fTmp0B_z * y;
    pSH8_x = 0.5462742152960395f * fC1_x;
    pSH8_y = 0.5462742152960395f * fC1_y;
    pSH4_x = 0.5462742152960395f * fS1_x;
    pSH4_y = 0.5462742152960395f * fS1_y;

    v_x += v_colors * (
            pSH4_x * coeffs[3*3+c] + pSH8_x * coeffs[3*7+c] +
            pSH7_x * coeffs[3*6+c]);
    v_y += v_colors * (
            pSH4_y * coeffs[3*3+c] + pSH8_y * coeffs[3*7+c] +
            pSH5_y * coeffs[3*4+c]);
    v_z += v_colors * (
            pSH6_z * coeffs[3*5+c] + pSH7_z * coeffs[3*6+c] +
            pSH5_z * coeffs[3*4+c]);

    if (degree <= 2) {
        float3 dir_n = float3(x, y, z);
        float3 v_dir_n = float3(v_x, v_y, v_z);
        if (inplace != 0)
            v_dir += (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        else
            v_dir = (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        return;
    }

    float fTmp0C = -2.285228997322329f * z2 + 0.4570457994644658f;
    float fTmp1B = 1.445305721320277f * z;
    float fC2 = x * fC1 - y * fS1;
    float fS2 = x * fS1 + y * fC1;
    float pSH12 = z * (1.865881662950577f * z2 - 1.119528997770346f);
    float pSH13 = fTmp0C * x;
    float pSH11 = fTmp0C * y;
    float pSH14 = fTmp1B * fC1;
    float pSH10 = fTmp1B * fS1;
    float pSH15 = -0.5900435899266435f * fC2;
    float pSH9 = -0.5900435899266435f * fS2;
    sv(8, pSH9 * v_colors);
    sv(9, pSH10 * v_colors);
    sv(10, pSH11 * v_colors);
    sv(11, pSH12 * v_colors);
    sv(12, pSH13 * v_colors);
    sv(13, pSH14 * v_colors);
    sv(14, pSH15 * v_colors);

    float fTmp0C_z, fTmp1B_z, fC2_x, fC2_y, fS2_x, fS2_y, pSH12_z, pSH13_x,
        pSH13_z, pSH11_y, pSH11_z, pSH14_x, pSH14_y, pSH14_z, pSH10_x, pSH10_y,
        pSH10_z, pSH15_x, pSH15_y, pSH9_x, pSH9_y;

    fTmp0C_z = -2.285228997322329f * 2.f * z;
    fTmp1B_z = 1.445305721320277f;
    fC2_x = fC1 + x * fC1_x - y * fS1_x;
    fC2_y = x * fC1_y - fS1 - y * fS1_y;
    fS2_x = fS1 + x * fS1_x + y * fC1_x;
    fS2_y = x * fS1_y + fC1 + y * fC1_y;
    pSH12_z = 3.f * 1.865881662950577f * z2 - 1.119528997770346f;
    pSH13_x = fTmp0C;
    pSH13_z = fTmp0C_z * x;
    pSH11_y = fTmp0C;
    pSH11_z = fTmp0C_z * y;
    pSH14_x = fTmp1B * fC1_x;
    pSH14_y = fTmp1B * fC1_y;
    pSH14_z = fTmp1B_z * fC1;
    pSH10_x = fTmp1B * fS1_x;
    pSH10_y = fTmp1B * fS1_y;
    pSH10_z = fTmp1B_z * fS1;
    pSH15_x = -0.5900435899266435f * fC2_x;
    pSH15_y = -0.5900435899266435f * fC2_y;
    pSH9_x = -0.5900435899266435f * fS2_x;
    pSH9_y = -0.5900435899266435f * fS2_y;

    v_x += v_colors * (
            pSH9_x * coeffs[3*8+c] + pSH15_x * coeffs[3*14+c] +
            pSH10_x * coeffs[3*9+c] + pSH14_x * coeffs[3*13+c] +
            pSH13_x * coeffs[3*12+c]);

    v_y += v_colors * (
            pSH9_y * coeffs[3*8+c] + pSH15_y * coeffs[3*14+c] +
            pSH10_y * coeffs[3*9+c] + pSH14_y * coeffs[3*13+c] +
            pSH11_y * coeffs[3*10+c]);

    v_z += v_colors * (
            pSH12_z * coeffs[3*11+c] + pSH13_z * coeffs[3*12+c] +
            pSH11_z * coeffs[3*10+c] + pSH14_z * coeffs[3*13+c] +
            pSH10_z * coeffs[3*9+c]);

    if (degree <= 3) {
        float3 dir_n = float3(x, y, z);
        float3 v_dir_n = float3(v_x, v_y, v_z);
        if (inplace != 0)
            v_dir += (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        else
            v_dir = (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        return;
    }

    float fTmp0D = z * (-4.683325804901025f * z2 + 2.007139630671868f);
    float fTmp1C = 3.31161143515146f * z2 - 0.47308734787878f;
    float fTmp2B = -1.770130769779931f * z;
    float fC3 = x * fC2 - y * fS2;
    float fS3 = x * fS2 + y * fC2;
    float pSH20 = (1.984313483298443f * z * pSH12 + -1.006230589874905f * pSH6);
    float pSH21 = fTmp0D * x;
    float pSH19 = fTmp0D * y;
    float pSH22 = fTmp1C * fC1;
    float pSH18 = fTmp1C * fS1;
    float pSH23 = fTmp2B * fC2;
    float pSH17 = fTmp2B * fS2;
    float pSH24 = 0.6258357354491763f * fC3;
    float pSH16 = 0.6258357354491763f * fS3;
    sv(15, pSH16 * v_colors);
    sv(16, pSH17 * v_colors);
    sv(17, pSH18 * v_colors);
    sv(18, pSH19 * v_colors);
    sv(19, pSH20 * v_colors);
    sv(20, pSH21 * v_colors);
    sv(21, pSH22 * v_colors);
    sv(22, pSH23 * v_colors);
    sv(23, pSH24 * v_colors);

    float fTmp0D_z, fTmp1C_z, fTmp2B_z, fC3_x, fC3_y, fS3_x, fS3_y, pSH20_z,
        pSH21_x, pSH21_z, pSH19_y, pSH19_z, pSH22_x, pSH22_y, pSH22_z, pSH18_x,
        pSH18_y, pSH18_z, pSH23_x, pSH23_y, pSH23_z, pSH17_x, pSH17_y, pSH17_z,
        pSH24_x, pSH24_y, pSH16_x, pSH16_y;

    fTmp0D_z = 3.f * -4.683325804901025f * z2 + 2.007139630671868f;
    fTmp1C_z = 2.f * 3.31161143515146f * z;
    fTmp2B_z = -1.770130769779931f;
    fC3_x = fC2 + x * fC2_x - y * fS2_x;
    fC3_y = x * fC2_y - fS2 - y * fS2_y;
    fS3_x = fS2 + y * fC2_x + x * fS2_x;
    fS3_y = x * fS2_y + fC2 + y * fC2_y;
    pSH20_z = 1.984313483298443f * (pSH12 + z * pSH12_z) +
                -1.006230589874905f * pSH6_z;
    pSH21_x = fTmp0D;
    pSH21_z = fTmp0D_z * x;
    pSH19_y = fTmp0D;
    pSH19_z = fTmp0D_z * y;
    pSH22_x = fTmp1C * fC1_x;
    pSH22_y = fTmp1C * fC1_y;
    pSH22_z = fTmp1C_z * fC1;
    pSH18_x = fTmp1C * fS1_x;
    pSH18_y = fTmp1C * fS1_y;
    pSH18_z = fTmp1C_z * fS1;
    pSH23_x = fTmp2B * fC2_x;
    pSH23_y = fTmp2B * fC2_y;
    pSH23_z = fTmp2B_z * fC2;
    pSH17_x = fTmp2B * fS2_x;
    pSH17_y = fTmp2B * fS2_y;
    pSH17_z = fTmp2B_z * fS2;
    pSH24_x = 0.6258357354491763f * fC3_x;
    pSH24_y = 0.6258357354491763f * fC3_y;
    pSH16_x = 0.6258357354491763f * fS3_x;
    pSH16_y = 0.6258357354491763f * fS3_y;

    v_x += v_colors * (
            pSH16_x * coeffs[3*15+c] + pSH24_x * coeffs[3*23+c] +
            pSH17_x * coeffs[3*16+c] + pSH23_x * coeffs[3*22+c] +
            pSH18_x * coeffs[3*17+c] + pSH22_x * coeffs[3*21+c] +
            pSH21_x * coeffs[3*20+c]);
    v_y += v_colors * (
            pSH16_y * coeffs[3*15+c] + pSH24_y * coeffs[3*23+c] +
            pSH17_y * coeffs[3*16+c] + pSH23_y * coeffs[3*22+c] +
            pSH18_y * coeffs[3*17+c] + pSH22_y * coeffs[3*21+c] +
            pSH19_y * coeffs[3*18+c]);
    v_z += v_colors * (
            pSH20_z * coeffs[3*19+c] + pSH21_z * coeffs[3*20+c] +
            pSH19_z * coeffs[3*18+c] + pSH22_z * coeffs[3*21+c] +
            pSH18_z * coeffs[3*17+c] + pSH23_z * coeffs[3*22+c] +
            pSH17_z * coeffs[3*16+c]);

    float3 dir_n = float3(x, y, z);
    float3 v_dir_n = float3(v_x, v_y, v_z);
    if (inplace != 0)
        v_dir += (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
    else
        v_dir = (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;

    #undef sv
}

[ForceInline]
void _sh_coeffs_to_color_vjp<let inplace: int>(
    const int degree,
    const float3 dir,
    float3 coeff_dc,
    float *coeffs,
    const float3 v_colors_3,
    inout float3 v_coeff_dc,
    float *v_coeffs,
    inout float3 v_dir
) {
    _sh_coeffs_to_color_c_vjp<0, inplace>(degree, dir, coeff_dc, coeffs, v_colors_3, v_coeff_dc, v_coeffs, v_dir);
    _sh_coeffs_to_color_c_vjp<1, inplace>(degree, dir, coeff_dc, coeffs, v_colors_3, v_coeff_dc, v_coeffs, v_dir);
    _sh_coeffs_to_color_c_vjp<2, inplace>(degree, dir, coeff_dc, coeffs, v_colors_3, v_coeff_dc, v_coeffs, v_dir);
}


// [CudaDeviceExport]
[ForceInline]
void sh_coeffs_to_color_vjp(
    const int degree,
    const float3 dir,
    float3 coeff_dc,
    float3 *coeffs,
    const float3 v_colors,
    inout float3 v_coeff_dc,
    float3 *v_coeffs,
    inout float3 v_dir
) {
    return _sh_coeffs_to_color_vjp<0>
        (degree, dir, coeff_dc, coeffs, v_colors, v_coeff_dc, v_coeffs, v_dir, 0u);
}

// [CudaDeviceExport]
[ForceInline]
void sh_coeffs_to_color_vjp_inplace(
    const int degree,
    const float3 dir,
    float3 coeff_dc,
    float3 *coeffs,
    const float3 v_colors,
    inout float3 v_coeff_dc,
    float3 *v_coeffs,
    inout float3 v_dir
) {
    return _sh_coeffs_to_color_vjp<1>
        (degree, dir, coeff_dc, coeffs, v_colors, v_coeff_dc, v_coeffs, v_dir, 0u);
}

// [CudaDeviceExport]
[ForceInline]
void sh_coeffs_to_color_vjp_atomic(
    const int degree,
    const float3 dir,
    float3 coeff_dc,
    float3 *coeffs,
    const float3 v_colors,
    inout float3 v_coeff_dc,
    float3 *v_coeffs,
    inout float3 v_dir
) {
    return _sh_coeffs_to_color_vjp<2>
        (degree, dir, coeff_dc, coeffs, v_colors, v_coeff_dc, v_coeffs, v_dir, 0u);
}

// [CudaDeviceExport]
[ForceInline]
void sh_coeffs_to_color_vjp_block_atomic(
    const int degree,
    const float3 dir,
    float3 coeff_dc,
    float3 *coeffs,
    const float3 v_colors,
    inout float3 v_coeff_dc,
    float3 *v_coeffs,
    inout float3 v_dir,
    uint thread_id
) {
    return _sh_coeffs_to_color_vjp<3>
        (degree, dir, coeff_dc, coeffs, v_colors, v_coeff_dc, v_coeffs, v_dir, thread_id);
}

// [CudaDeviceExport]
[ForceInline]
void sh_coeffs_to_color_vjp(
    const int degree,
    const float3 dir,
    float3 coeff_dc,
    float *coeffs,
    const float3 v_colors,
    inout float3 v_coeff_dc,
    float *v_coeffs,
    inout float3 v_dir
) {
    return _sh_coeffs_to_color_vjp<0>
        (degree, dir, coeff_dc, coeffs, v_colors, v_coeff_dc, v_coeffs, v_dir);
}

// [CudaDeviceExport]
[ForceInline]
void sh_coeffs_to_color_vjp_inplace(
    const int degree,
    const float3 dir,
    float3 coeff_dc,
    float *coeffs,
    const float3 v_colors,
    inout float3 v_coeff_dc,
    float *v_coeffs,
    inout float3 v_dir
) {
    return _sh_coeffs_to_color_vjp<1>
        (degree, dir, coeff_dc, coeffs, v_colors, v_coeff_dc, v_coeffs, v_dir);
}

// [CudaDeviceExport]
[ForceInline]
void sh_coeffs_to_color_vjp_atomic(
    const int degree,
    const float3 dir,
    float3 coeff_dc,
    float *coeffs,
    const float3 v_colors,
    inout float3 v_coeff_dc,
    float *v_coeffs,
    inout float3 v_dir
) {
    return _sh_coeffs_to_color_vjp<2>
        (degree, dir, coeff_dc, coeffs, v_colors, v_coeff_dc, v_coeffs, v_dir);
}


// [CudaDeviceExport]
[ForceInline]
float3 sh_coeffs_to_color(
    const int degree,
    const float3 mean,
    const float3x3 R,
    const float3 t,
    float3 coeff_dc,
    float3* coeffs
) {
    float3 viewdir = mean_viewmat_to_unnormalized_viewdir(mean, R, t);
    float3 rgb = sh_coeffs_to_color(degree, viewdir, coeff_dc, coeffs);
    return max(rgb + 0.5, 0.0);
}

// [CudaDeviceExport]
[ForceInline]
float3 sh_coeffs_to_color(
    const int degree,
    const float3 mean,
    const float3x3 R,
    const float3 t,
    float3 coeff_dc,
    float* coeffs
) {
    float3 viewdir = mean_viewmat_to_unnormalized_viewdir(mean, R, t);
    float3 rgb = sh_coeffs_to_color(degree, viewdir, coeff_dc, coeffs);
    return max(rgb + 0.5, 0.0);
}


// v_mean/v_R/v_t are inout: with inplace != 0 the vjp accumulates onto the
// caller's running gradient (inplace == 0 overwrites).
[ForceInline]
void _sh_coeffs_to_color_vjp<let inplace: int>(
    const int degree,
    const float3 mean,
    const float3x3 R,
    const float3 t,
    float3 coeff_dc,
    float3 *coeffs,
    float3 v_colors,
    out float3 v_coeff_dc,
    float3 *v_coeffs,
    inout float3 v_mean,
    inout float3x3 v_R,
    inout float3 v_t
) {
    float3 viewdir = mean_viewmat_to_unnormalized_viewdir(mean, R, t);
    float3 rgb = sh_coeffs_to_color(degree, viewdir, coeff_dc, coeffs);  // TODO: pre-compute
    v_colors *= float3(float(rgb.x >= -0.5), float(rgb.y >= -0.5), float(rgb.z >= -0.5));

    float3 v_viewdir;
    // The (mean, R, t) wrappers never use the block-atomic mode (which needs
    // a per-block thread_id), so pass 0u here — the float3 vjp ignores
    // thread_id for inplace 0/1/2.
    _sh_coeffs_to_color_vjp<inplace>(degree, viewdir, coeff_dc, coeffs, v_colors, v_coeff_dc, v_coeffs, v_viewdir, 0u);

    DifferentialPair<float3> dp_mean = diffPair(mean);
    DifferentialPair<float3x3> dp_R = diffPair(R);
    DifferentialPair<float3> dp_t = diffPair(t);
    bwd_diff(mean_viewmat_to_unnormalized_viewdir)(
        dp_mean, dp_R, dp_t, v_viewdir
    );
    if (inplace == 0) {
        v_mean = dp_mean.d;
        v_R = dp_R.d;
        v_t = dp_t.d;
    } else {
        v_mean += dp_mean.d;
        v_R += dp_R.d;
        v_t += dp_t.d;
    }
}

// [CudaDeviceExport]
[ForceInline]
void sh_coeffs_to_color_vjp(
    const int degree,
    const float3 mean,
    const float3x3 R,
    const float3 t,
    float3 coeff_dc,
    float3 *coeffs,
    float3 v_colors,
    out float3 v_coeff_dc,
    float3 *v_coeffs,
    out float3 v_mean,
    out float3x3 v_R,
    out float3 v_t
) {
    v_mean = float3(0.0f);
    v_R = float3x3(0.0f);
    v_t = float3(0.0f);
    return _sh_coeffs_to_color_vjp<0>
        (degree, mean, R, t, coeff_dc, coeffs, v_colors, v_coeff_dc, v_coeffs, v_mean, v_R, v_t);
}

// [CudaDeviceExport]
[ForceInline]
void sh_coeffs_to_color_vjp_inplace(
    const int degree,
    const float3 mean,
    const float3x3 R,
    const float3 t,
    float3 coeff_dc,
    float3 *coeffs,
    float3 v_colors,
    out float3 v_coeff_dc,
    float3 *v_coeffs,
    inout float3 v_mean,
    inout float3x3 v_R,
    inout float3 v_t
) {
    return _sh_coeffs_to_color_vjp<1>
        (degree, mean, R, t, coeff_dc, coeffs, v_colors, v_coeff_dc, v_coeffs, v_mean, v_R, v_t);
}

// [CudaDeviceExport]
[ForceInline]
void sh_coeffs_to_color_vjp_atomic(
    const int degree,
    const float3 mean,
    const float3x3 R,
    const float3 t,
    float3 coeff_dc,
    float3 *coeffs,
    float3 v_colors,
    out float3 v_coeff_dc,
    float3 *v_coeffs,
    inout float3 v_mean,
    inout float3x3 v_R,
    inout float3 v_t
) {
    return _sh_coeffs_to_color_vjp<2>
        (degree, mean, R, t, coeff_dc, coeffs, v_colors, v_coeff_dc, v_coeffs, v_mean, v_R, v_t);
}


// v_mean/v_R/v_t inout: see the float3-coeffs overload above.
[ForceInline]
void _sh_coeffs_to_color_vjp<let inplace: int>(
    const int degree,
    const float3 mean,
    const float3x3 R,
    const float3 t,
    float3 coeff_dc,
    float *coeffs,
    float3 v_colors,
    out float3 v_coeff_dc,
    float *v_coeffs,
    inout float3 v_mean,
    inout float3x3 v_R,
    inout float3 v_t
) {
    float3 viewdir = mean_viewmat_to_unnormalized_viewdir(mean, R, t);
    float3 rgb = sh_coeffs_to_color(degree, viewdir, coeff_dc, coeffs);  // TODO: pre-compute
    v_colors *= float3(float(rgb.x >= -0.5), float(rgb.y >= -0.5), float(rgb.z >= -0.5));

    float3 v_viewdir;
    _sh_coeffs_to_color_vjp<inplace>(degree, viewdir, coeff_dc, coeffs, v_colors, v_coeff_dc, v_coeffs, v_viewdir);

    DifferentialPair<float3> dp_mean = diffPair(mean);
    DifferentialPair<float3x3> dp_R = diffPair(R);
    DifferentialPair<float3> dp_t = diffPair(t);
    bwd_diff(mean_viewmat_to_unnormalized_viewdir)(
        dp_mean, dp_R, dp_t, v_viewdir
    );
    if (inplace == 0) {
        v_mean = dp_mean.d;
        v_R = dp_R.d;
        v_t = dp_t.d;
    } else {
        v_mean += dp_mean.d;
        v_R += dp_R.d;
        v_t += dp_t.d;
    }
}

// [CudaDeviceExport]
[ForceInline]
void sh_coeffs_to_color_vjp(
    const int degree,
    const float3 mean,
    const float3x3 R,
    const float3 t,
    float3 coeff_dc,
    float *coeffs,
    float3 v_colors,
    out float3 v_coeff_dc,
    float *v_coeffs,
    out float3 v_mean,
    out float3x3 v_R,
    out float3 v_t
) {
    v_mean = float3(0.0f);
    v_R = float3x3(0.0f);
    v_t = float3(0.0f);
    return _sh_coeffs_to_color_vjp<0>
        (degree, mean, R, t, coeff_dc, coeffs, v_colors, v_coeff_dc, v_coeffs, v_mean, v_R, v_t);
}

// [CudaDeviceExport]
[ForceInline]
void sh_coeffs_to_color_vjp_inplace(
    const int degree,
    const float3 mean,
    const float3x3 R,
    const float3 t,
    float3 coeff_dc,
    float *coeffs,
    float3 v_colors,
    out float3 v_coeff_dc,
    float *v_coeffs,
    inout float3 v_mean,
    inout float3x3 v_R,
    inout float3 v_t
) {
    return _sh_coeffs_to_color_vjp<1>
        (degree, mean, R, t, coeff_dc, coeffs, v_colors, v_coeff_dc, v_coeffs, v_mean, v_R, v_t);
}

// [CudaDeviceExport]
[ForceInline]
void sh_coeffs_to_color_vjp_atomic(
    const int degree,
    const float3 mean,
    const float3x3 R,
    const float3 t,
    float3 coeff_dc,
    float *coeffs,
    float3 v_colors,
    out float3 v_coeff_dc,
    float *v_coeffs,
    inout float3 v_mean,
    inout float3x3 v_R,
    inout float3 v_t
) {
    return _sh_coeffs_to_color_vjp<2>
        (degree, mean, R, t, coeff_dc, coeffs, v_colors, v_coeff_dc, v_coeffs, v_mean, v_R, v_t);
}


// Slang compiler will eliminate dead code for these as long as there's [ForceInline]
// Original sh_coeffs_to_color(_vjp) remain options for efficient code length

#if 0

#define _DECL(sh_degree, dtype) \
    [CudaDeviceExport] \
    float3 sh##sh_degree##_to_color(const float3 viewdir, float3 coeff_dc, dtype* coeffs) \
        { return sh_coeffs_to_color(sh_degree, viewdir, coeff_dc, coeffs); } \
    [CudaDeviceExport] \
    float3 sh##sh_degree##_to_color(float3 mean, float3x3 R, float3 t, float3 coeff_dc, dtype* coeffs) \
        { return sh_coeffs_to_color(sh_degree, mean, R, t, coeff_dc, coeffs); } \
    [CudaDeviceExport] \
    void sh##sh_degree##_to_color_vjp(float3 dir, float3 coeff_dc, dtype* coeffs, float3 v_colors, out float3 v_coeff_dc, dtype *v_coeffs, out float3 v_dir) \
        { sh_coeffs_to_color_vjp(sh_degree, dir, coeff_dc, coeffs, v_colors, v_coeff_dc, v_coeffs, v_dir); } \
    [CudaDeviceExport] \
    void sh##sh_degree##_to_color_vjp(float3 mean, float3x3 R, float3 t, float3 coeff_dc, dtype* coeffs, float3 v_colors, out float3 v_coeff_dc, dtype *v_coeffs, out float3 v_mean, out float3x3 v_R, out float3 v_t) \
        { sh_coeffs_to_color_vjp(sh_degree, mean, R, t, coeff_dc, coeffs, v_colors, v_coeff_dc, v_coeffs, v_mean, v_R, v_t); } \
    [CudaDeviceExport] \
    void sh##sh_degree##_to_color_vjp_inplace(float3 mean, float3x3 R, float3 t, float3 coeff_dc, dtype* coeffs, float3 v_colors, out float3 v_coeff_dc, dtype *v_coeffs, out float3 v_mean, out float3x3 v_R, out float3 v_t) \
        { sh_coeffs_to_color_vjp_inplace(sh_degree, mean, R, t, coeff_dc, coeffs, v_colors, v_coeff_dc, v_coeffs, v_mean, v_R, v_t); } \
    [CudaDeviceExport] \
    void sh##sh_degree##_to_color_vjp_atomic(float3 mean, float3x3 R, float3 t, float3 coeff_dc, dtype* coeffs, float3 v_colors, out float3 v_coeff_dc, dtype *v_coeffs, out float3 v_mean, out float3x3 v_R, out float3 v_t) \
        { sh_coeffs_to_color_vjp_atomic(sh_degree, mean, R, t, coeff_dc, coeffs, v_colors, v_coeff_dc, v_coeffs, v_mean, v_R, v_t); } \

_DECL(0, float3) _DECL(1, float3) _DECL(2, float3) _DECL(3, float3) _DECL(4, float3)
_DECL(0, float) _DECL(1, float) _DECL(2, float) _DECL(3, float) _DECL(4, float)

#else

// only include necessary ones to speed up code parsing

#define _DECL(sh_degree, dtype) \
    [CudaDeviceExport] \
    float3 sh##sh_degree##_to_color(float3 mean, float3x3 R, float3 t, float3 coeff_dc, dtype* coeffs) \
        { return sh_coeffs_to_color(sh_degree, mean, R, t, coeff_dc, coeffs); } \
    [CudaDeviceExport] \
    void sh##sh_degree##_to_color_vjp_inplace(float3 mean, float3x3 R, float3 t, float3 coeff_dc, dtype* coeffs, float3 v_colors, out float3 v_coeff_dc, dtype *v_coeffs, out float3 v_mean, out float3x3 v_R, out float3 v_t) \
        { sh_coeffs_to_color_vjp_inplace(sh_degree, mean, R, t, coeff_dc, coeffs, v_colors, v_coeff_dc, v_coeffs, v_mean, v_R, v_t); } \
    [CudaDeviceExport] \
    void sh##sh_degree##_to_color_vjp_atomic(float3 mean, float3x3 R, float3 t, float3 coeff_dc, dtype* coeffs, float3 v_colors, out float3 v_coeff_dc, dtype *v_coeffs, out float3 v_mean, out float3x3 v_R, out float3 v_t) \
        { sh_coeffs_to_color_vjp_atomic(sh_degree, mean, R, t, coeff_dc, coeffs, v_colors, v_coeff_dc, v_coeffs, v_mean, v_R, v_t); } \

_DECL(0, float3) _DECL(1, float3) _DECL(2, float3) _DECL(3, float3) _DECL(4, float3)
_DECL(0, float) _DECL(1, float) _DECL(2, float) _DECL(3, float) _DECL(4, float)

#endif

#undef _DECL


// Direction-only entry points (no splat-mean indirection). Used for skybox /
// background SH where the per-pixel world-space ray direction is computed by
// the caller. Forward returns the raw SH-evaluated color (no +0.5 / clamp);
// the caller applies that post-processing and the matching clamp gradient.
//
// _vjp_atomic       : per-thread atomicAdd per channel per coefficient.
// _vjp_block_atomic : block-reduced (512-thread) atomicAdd per call; caller
//                     must launch with BLOCK_SIZE=512 and pass thread_id (see
//                     sh_block_atomic_add_f3_512 above for requirements).
#define _DECL_DIR(sh_degree, dtype) \
    [CudaDeviceExport] \
    float3 sh##sh_degree##_to_color_dir(float3 dir, float3 coeff_dc, dtype* coeffs) \
        { return sh_coeffs_to_color(sh_degree, dir, coeff_dc, coeffs); } \
    [CudaDeviceExport] \
    void sh##sh_degree##_to_color_dir_vjp_inplace(float3 dir, float3 coeff_dc, dtype* coeffs, float3 v_colors, inout float3 v_coeff_dc, dtype *v_coeffs, inout float3 v_dir) \
        { sh_coeffs_to_color_vjp_inplace(sh_degree, dir, coeff_dc, coeffs, v_colors, v_coeff_dc, v_coeffs, v_dir); } \
    [CudaDeviceExport] \
    void sh##sh_degree##_to_color_dir_vjp_atomic(float3 dir, float3 coeff_dc, dtype* coeffs, float3 v_colors, inout float3 v_coeff_dc, dtype *v_coeffs, inout float3 v_dir) \
        { sh_coeffs_to_color_vjp_atomic(sh_degree, dir, coeff_dc, coeffs, v_colors, v_coeff_dc, v_coeffs, v_dir); } \
    [CudaDeviceExport] \
    void sh##sh_degree##_to_color_dir_vjp_block_atomic(float3 dir, float3 coeff_dc, dtype* coeffs, float3 v_colors, inout float3 v_coeff_dc, dtype *v_coeffs, inout float3 v_dir, uint thread_id) \
        { sh_coeffs_to_color_vjp_block_atomic(sh_degree, dir, coeff_dc, coeffs, v_colors, v_coeff_dc, v_coeffs, v_dir, thread_id); } \

_DECL_DIR(0, float3) _DECL_DIR(1, float3) _DECL_DIR(2, float3) _DECL_DIR(3, float3) _DECL_DIR(4, float3)

#undef _DECL_DIR


// ============================================================================
// SH value-quantization parallel forms (q8 / q16).
//
// Mirrors the existing `sh_coeffs_to_color` / `_sh_coeffs_to_color_vjp` family
// but reads coefficients via _sh_load_q{8,16}(packed, bounds, base, j) instead
// of `coeffs[j]`. v_coeffs writes stay fp32 (gradients live in fp32; only
// the param buffer is quantized). All bodies are mechanical copies of the
// fp32 forms with `coeffs[i]` replaced by a local `_C(i)` macro that returns
// float3 via the load helper. The fp32 forms above are untouched -- the
// q8/q16 variants here are parallel functions, dispatched by the caller on
// the compile-time bit-depth template parameter.
//
// Only the (mean, R, t) public form is exported (splat SH path). The direction-
// only form is only used internally as the implementation of the (mean, R, t)
// form's viewdir eval (and by background SH -- background SH stays fp32 per
// scope decision; only splat features_sh gets value-quantized here).
// ============================================================================


// ---- direction-form forward (q8) -------------------------------------------
[ForceInline]
float3 sh_coeffs_to_color_q8(
    const int degree,
    const float3 viewdir,
    float3 coeff_dc,
    uint8_t* packed,
    float2*  bounds,
    int64_t  base,
    int64_t  bounds_stride,
    int64_t  pair_pitch
) {
    #define _C(j) _sh_load_q8(packed, bounds, base, j, bounds_stride, pair_pitch)
    float3 colors = 0.2820947917738781f * coeff_dc;

    if (degree <= 0)
        return colors;

    float inv_norm = rsqrt(
        viewdir.x * viewdir.x + viewdir.y * viewdir.y + viewdir.z * viewdir.z
    );
    float x = viewdir.x * inv_norm;
    float y = viewdir.y * inv_norm;
    float z = viewdir.z * inv_norm;

    float fTmp0A = 0.48860251190292f;
    colors += fTmp0A *
            (-y * _C(0) +
            z * _C(1) -
            x * _C(2));

    if (degree <= 1)
        return colors;

    float z2 = z * z;
    float fTmp0B = -1.092548430592079f * z;
    float fTmp1A = 0.5462742152960395f;
    float fC1 = x * x - y * y;
    float fS1 = 2.f * x * y;
    float pSH6 = (0.9461746957575601f * z2 - 0.3153915652525201f);
    float pSH7 = fTmp0B * x;
    float pSH5 = fTmp0B * y;
    float pSH8 = fTmp1A * fC1;
    float pSH4 = fTmp1A * fS1;
    colors +=
        pSH4 * _C(3) + pSH5 * _C(4) +
        pSH6 * _C(5) + pSH7 * _C(6) +
        pSH8 * _C(7);

    if (degree <= 2)
        return colors;

    float fTmp0C = -2.285228997322329f * z2 + 0.4570457994644658f;
    float fTmp1B = 1.445305721320277f * z;
    float fTmp2A = -0.5900435899266435f;
    float fC2 = x * fC1 - y * fS1;
    float fS2 = x * fS1 + y * fC1;
    float pSH12 = z * (1.865881662950577f * z2 - 1.119528997770346f);
    float pSH13 = fTmp0C * x;
    float pSH11 = fTmp0C * y;
    float pSH14 = fTmp1B * fC1;
    float pSH10 = fTmp1B * fS1;
    float pSH15 = fTmp2A * fC2;
    float pSH9  = fTmp2A * fS2;
    colors += pSH9  * _C(8) +
            pSH10 * _C(9) +
            pSH11 * _C(10) +
            pSH12 * _C(11) +
            pSH13 * _C(12) +
            pSH14 * _C(13) +
            pSH15 * _C(14);

    if (degree <= 3)
        return colors;

    float fTmp0D = z * (-4.683325804901025f * z2 + 2.007139630671868f);
    float fTmp1C = 3.31161143515146f * z2 - 0.47308734787878f;
    float fTmp2B = -1.770130769779931f * z;
    float fC3 = x * fC2 - y * fS2;
    float fS3 = x * fS2 + y * fC2;
    float pSH20 = (1.984313483298443f * z * pSH12 - 1.006230589874905f * pSH6);
    float pSH21 = fTmp0D * x;
    float pSH19 = fTmp0D * y;
    float pSH22 = fTmp1C * fC1;
    float pSH18 = fTmp1C * fS1;
    float pSH23 = fTmp2B * fC2;
    float pSH17 = fTmp2B * fS2;
    float pSH24 = 0.6258357354491763f * fC3;
    float pSH16 = 0.6258357354491763f * fS3;

    colors += pSH16 * _C(15) +
                pSH17 * _C(16) +
                pSH18 * _C(17) +
                pSH19 * _C(18) +
                pSH20 * _C(19) +
                pSH21 * _C(20) +
                pSH22 * _C(21) +
                pSH23 * _C(22) +
                pSH24 * _C(23);

    #undef _C
    return colors;
}


// ---- direction-form forward (q16) ------------------------------------------
[ForceInline]
float3 sh_coeffs_to_color_q16(
    const int degree,
    const float3 viewdir,
    float3 coeff_dc,
    uint16_t* packed,
    float2*   bounds,
    int64_t   base,
    int64_t  bounds_stride,
    int64_t  pair_pitch
) {
    #define _C(j) _sh_load_q16(packed, bounds, base, j, bounds_stride, pair_pitch)
    float3 colors = 0.2820947917738781f * coeff_dc;

    if (degree <= 0)
        return colors;

    float inv_norm = rsqrt(
        viewdir.x * viewdir.x + viewdir.y * viewdir.y + viewdir.z * viewdir.z
    );
    float x = viewdir.x * inv_norm;
    float y = viewdir.y * inv_norm;
    float z = viewdir.z * inv_norm;

    float fTmp0A = 0.48860251190292f;
    colors += fTmp0A *
            (-y * _C(0) +
            z * _C(1) -
            x * _C(2));

    if (degree <= 1)
        return colors;

    float z2 = z * z;
    float fTmp0B = -1.092548430592079f * z;
    float fTmp1A = 0.5462742152960395f;
    float fC1 = x * x - y * y;
    float fS1 = 2.f * x * y;
    float pSH6 = (0.9461746957575601f * z2 - 0.3153915652525201f);
    float pSH7 = fTmp0B * x;
    float pSH5 = fTmp0B * y;
    float pSH8 = fTmp1A * fC1;
    float pSH4 = fTmp1A * fS1;
    colors +=
        pSH4 * _C(3) + pSH5 * _C(4) +
        pSH6 * _C(5) + pSH7 * _C(6) +
        pSH8 * _C(7);

    if (degree <= 2)
        return colors;

    float fTmp0C = -2.285228997322329f * z2 + 0.4570457994644658f;
    float fTmp1B = 1.445305721320277f * z;
    float fTmp2A = -0.5900435899266435f;
    float fC2 = x * fC1 - y * fS1;
    float fS2 = x * fS1 + y * fC1;
    float pSH12 = z * (1.865881662950577f * z2 - 1.119528997770346f);
    float pSH13 = fTmp0C * x;
    float pSH11 = fTmp0C * y;
    float pSH14 = fTmp1B * fC1;
    float pSH10 = fTmp1B * fS1;
    float pSH15 = fTmp2A * fC2;
    float pSH9  = fTmp2A * fS2;
    colors += pSH9  * _C(8) +
            pSH10 * _C(9) +
            pSH11 * _C(10) +
            pSH12 * _C(11) +
            pSH13 * _C(12) +
            pSH14 * _C(13) +
            pSH15 * _C(14);

    if (degree <= 3)
        return colors;

    float fTmp0D = z * (-4.683325804901025f * z2 + 2.007139630671868f);
    float fTmp1C = 3.31161143515146f * z2 - 0.47308734787878f;
    float fTmp2B = -1.770130769779931f * z;
    float fC3 = x * fC2 - y * fS2;
    float fS3 = x * fS2 + y * fC2;
    float pSH20 = (1.984313483298443f * z * pSH12 - 1.006230589874905f * pSH6);
    float pSH21 = fTmp0D * x;
    float pSH19 = fTmp0D * y;
    float pSH22 = fTmp1C * fC1;
    float pSH18 = fTmp1C * fS1;
    float pSH23 = fTmp2B * fC2;
    float pSH17 = fTmp2B * fS2;
    float pSH24 = 0.6258357354491763f * fC3;
    float pSH16 = 0.6258357354491763f * fS3;

    colors += pSH16 * _C(15) +
                pSH17 * _C(16) +
                pSH18 * _C(17) +
                pSH19 * _C(18) +
                pSH20 * _C(19) +
                pSH21 * _C(20) +
                pSH22 * _C(21) +
                pSH23 * _C(22) +
                pSH24 * _C(23);

    #undef _C
    return colors;
}


// ---- direction-form VJP (q8) -----------------------------------------------
// Body is a verbatim copy of _sh_coeffs_to_color_vjp<inplace>(float3* coeffs)
// with `coeffs[i]` replaced by `_C(i)` (q8 codec load) and v_coeffs writes
// kept as fp32 via the same sv(i, v) macro.
[ForceInline]
void _sh_coeffs_to_color_vjp_q8<let inplace: int>(
    const int degree,
    const float3 dir,
    float3 coeff_dc,
    uint8_t* packed,
    float2*  bounds,
    int64_t  base,
    int64_t  bounds_stride,
    int64_t  pair_pitch,
    const float3 v_colors,
    inout float3 v_coeff_dc,
    float3 *v_coeffs,
    inout float3 v_dir,
    uint thread_id
) {
    #define _C(j) _sh_load_q8(packed, bounds, base, j, bounds_stride, pair_pitch)
    #define sv(i, v) \
        if (inplace == 0) v_coeffs[i] = (v); \
        else if (inplace == 1) v_coeffs[i] += (v); \
        else if (inplace == 2) { \
            float3 temp = (v); \
            if (isfinite(dot(temp, temp)) && dot(temp, temp) != 0.0) { \
                InterlockedAdd(v_coeffs[i].x, temp.x); \
                InterlockedAdd(v_coeffs[i].y, temp.y); \
                InterlockedAdd(v_coeffs[i].z, temp.z); \
            } \
        } \
        else if (inplace == 3) { \
            sh_block_atomic_add_f3_512(v_coeffs, (uint)(i), (v), thread_id); \
        }

    if (inplace != 0) {
        v_coeff_dc += 0.2820947917738781f * v_colors;
    } else {
        v_coeff_dc = 0.2820947917738781f * v_colors;
    }
    if (degree <= 0) {
        if (inplace == 0) v_dir = float3(0);
        return;
    }

    float inorm = rsqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    float x = dir.x * inorm;
    float y = dir.y * inorm;
    float z = dir.z * inorm;
    float v_x = 0.f, v_y = 0.f, v_z = 0.f;

    sv(0, -0.48860251190292f * y * v_colors);
    sv(1, 0.48860251190292f * z * v_colors);
    sv(2, -0.48860251190292f * x * v_colors);

    v_x += -0.48860251190292f * dot(_C(2), v_colors);
    v_y += -0.48860251190292f * dot(_C(0), v_colors);
    v_z += 0.48860251190292f * dot(_C(1), v_colors);

    if (degree <= 1) {
        float3 dir_n = float3(x, y, z);
        float3 v_dir_n = float3(v_x, v_y, v_z);
        if (inplace != 0)
            v_dir += (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        else
            v_dir = (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        return;
    }

    float z2 = z * z;
    float fTmp0B = -1.092548430592079f * z;
    float fC1 = x * x - y * y;
    float fS1 = 2.f * x * y;
    float pSH6 = (0.9461746957575601f * z2 - 0.3153915652525201f);
    float pSH7 = fTmp0B * x;
    float pSH5 = fTmp0B * y;
    float pSH8 = 0.5462742152960395f * fC1;
    float pSH4 = 0.5462742152960395f * fS1;
    sv(3, pSH4 * v_colors);
    sv(4, pSH5 * v_colors);
    sv(5, pSH6 * v_colors);
    sv(6, pSH7 * v_colors);
    sv(7, pSH8 * v_colors);

    float fTmp0B_z, fC1_x, fC1_y, fS1_x, fS1_y, pSH6_z, pSH7_x, pSH7_z, pSH5_y,
        pSH5_z, pSH8_x, pSH8_y, pSH4_x, pSH4_y;

    fTmp0B_z = -1.092548430592079f;
    fC1_x = 2.f * x;
    fC1_y = -2.f * y;
    fS1_x = 2.f * y;
    fS1_y = 2.f * x;
    pSH6_z = 2.f * 0.9461746957575601f * z;
    pSH7_x = fTmp0B;
    pSH7_z = fTmp0B_z * x;
    pSH5_y = fTmp0B;
    pSH5_z = fTmp0B_z * y;
    pSH8_x = 0.5462742152960395f * fC1_x;
    pSH8_y = 0.5462742152960395f * fC1_y;
    pSH4_x = 0.5462742152960395f * fS1_x;
    pSH4_y = 0.5462742152960395f * fS1_y;

    v_x += dot(v_colors,
            pSH4_x * _C(3) + pSH8_x * _C(7) +
            pSH7_x * _C(6));
    v_y += dot(v_colors,
            pSH4_y * _C(3) + pSH8_y * _C(7) +
            pSH5_y * _C(4));
    v_z += dot(v_colors,
            pSH6_z * _C(5) + pSH7_z * _C(6) +
            pSH5_z * _C(4));

    if (degree <= 2) {
        float3 dir_n = float3(x, y, z);
        float3 v_dir_n = float3(v_x, v_y, v_z);
        if (inplace != 0)
            v_dir += (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        else
            v_dir = (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        return;
    }

    float fTmp0C = -2.285228997322329f * z2 + 0.4570457994644658f;
    float fTmp1B = 1.445305721320277f * z;
    float fC2 = x * fC1 - y * fS1;
    float fS2 = x * fS1 + y * fC1;
    float pSH12 = z * (1.865881662950577f * z2 - 1.119528997770346f);
    float pSH13 = fTmp0C * x;
    float pSH11 = fTmp0C * y;
    float pSH14 = fTmp1B * fC1;
    float pSH10 = fTmp1B * fS1;
    float pSH15 = -0.5900435899266435f * fC2;
    float pSH9 = -0.5900435899266435f * fS2;
    sv(8, pSH9 * v_colors);
    sv(9, pSH10 * v_colors);
    sv(10, pSH11 * v_colors);
    sv(11, pSH12 * v_colors);
    sv(12, pSH13 * v_colors);
    sv(13, pSH14 * v_colors);
    sv(14, pSH15 * v_colors);

    float fTmp0C_z, fTmp1B_z, fC2_x, fC2_y, fS2_x, fS2_y, pSH12_z, pSH13_x,
        pSH13_z, pSH11_y, pSH11_z, pSH14_x, pSH14_y, pSH14_z, pSH10_x, pSH10_y,
        pSH10_z, pSH15_x, pSH15_y, pSH9_x, pSH9_y;

    fTmp0C_z = -2.285228997322329f * 2.f * z;
    fTmp1B_z = 1.445305721320277f;
    fC2_x = fC1 + x * fC1_x - y * fS1_x;
    fC2_y = x * fC1_y - fS1 - y * fS1_y;
    fS2_x = fS1 + x * fS1_x + y * fC1_x;
    fS2_y = x * fS1_y + fC1 + y * fC1_y;
    pSH12_z = 3.f * 1.865881662950577f * z2 - 1.119528997770346f;
    pSH13_x = fTmp0C;
    pSH13_z = fTmp0C_z * x;
    pSH11_y = fTmp0C;
    pSH11_z = fTmp0C_z * y;
    pSH14_x = fTmp1B * fC1_x;
    pSH14_y = fTmp1B * fC1_y;
    pSH14_z = fTmp1B_z * fC1;
    pSH10_x = fTmp1B * fS1_x;
    pSH10_y = fTmp1B * fS1_y;
    pSH10_z = fTmp1B_z * fS1;
    pSH15_x = -0.5900435899266435f * fC2_x;
    pSH15_y = -0.5900435899266435f * fC2_y;
    pSH9_x = -0.5900435899266435f * fS2_x;
    pSH9_y = -0.5900435899266435f * fS2_y;

    v_x += dot(v_colors,
            pSH9_x * _C(8) + pSH15_x * _C(14) +
            pSH10_x * _C(9) + pSH14_x * _C(13) +
            pSH13_x * _C(12));

    v_y += dot(v_colors,
            pSH9_y * _C(8) + pSH15_y * _C(14) +
            pSH10_y * _C(9) + pSH14_y * _C(13) +
            pSH11_y * _C(10));

    v_z += dot(v_colors,
            pSH12_z * _C(11) + pSH13_z * _C(12) +
            pSH11_z * _C(10) + pSH14_z * _C(13) +
            pSH10_z * _C(9));

    if (degree <= 3) {
        float3 dir_n = float3(x, y, z);
        float3 v_dir_n = float3(v_x, v_y, v_z);
        if (inplace != 0)
            v_dir += (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        else
            v_dir = (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        return;
    }

    float fTmp0D = z * (-4.683325804901025f * z2 + 2.007139630671868f);
    float fTmp1C = 3.31161143515146f * z2 - 0.47308734787878f;
    float fTmp2B = -1.770130769779931f * z;
    float fC3 = x * fC2 - y * fS2;
    float fS3 = x * fS2 + y * fC2;
    float pSH20 = (1.984313483298443f * z * pSH12 + -1.006230589874905f * pSH6);
    float pSH21 = fTmp0D * x;
    float pSH19 = fTmp0D * y;
    float pSH22 = fTmp1C * fC1;
    float pSH18 = fTmp1C * fS1;
    float pSH23 = fTmp2B * fC2;
    float pSH17 = fTmp2B * fS2;
    float pSH24 = 0.6258357354491763f * fC3;
    float pSH16 = 0.6258357354491763f * fS3;
    sv(15, pSH16 * v_colors);
    sv(16, pSH17 * v_colors);
    sv(17, pSH18 * v_colors);
    sv(18, pSH19 * v_colors);
    sv(19, pSH20 * v_colors);
    sv(20, pSH21 * v_colors);
    sv(21, pSH22 * v_colors);
    sv(22, pSH23 * v_colors);
    sv(23, pSH24 * v_colors);

    float fTmp0D_z, fTmp1C_z, fTmp2B_z, fC3_x, fC3_y, fS3_x, fS3_y, pSH20_z,
        pSH21_x, pSH21_z, pSH19_y, pSH19_z, pSH22_x, pSH22_y, pSH22_z, pSH18_x,
        pSH18_y, pSH18_z, pSH23_x, pSH23_y, pSH23_z, pSH17_x, pSH17_y, pSH17_z,
        pSH24_x, pSH24_y, pSH16_x, pSH16_y;

    fTmp0D_z = 3.f * -4.683325804901025f * z2 + 2.007139630671868f;
    fTmp1C_z = 2.f * 3.31161143515146f * z;
    fTmp2B_z = -1.770130769779931f;
    fC3_x = fC2 + x * fC2_x - y * fS2_x;
    fC3_y = x * fC2_y - fS2 - y * fS2_y;
    fS3_x = fS2 + y * fC2_x + x * fS2_x;
    fS3_y = x * fS2_y + fC2 + y * fC2_y;
    pSH20_z = 1.984313483298443f * (pSH12 + z * pSH12_z) +
                -1.006230589874905f * pSH6_z;
    pSH21_x = fTmp0D;
    pSH21_z = fTmp0D_z * x;
    pSH19_y = fTmp0D;
    pSH19_z = fTmp0D_z * y;
    pSH22_x = fTmp1C * fC1_x;
    pSH22_y = fTmp1C * fC1_y;
    pSH22_z = fTmp1C_z * fC1;
    pSH18_x = fTmp1C * fS1_x;
    pSH18_y = fTmp1C * fS1_y;
    pSH18_z = fTmp1C_z * fS1;
    pSH23_x = fTmp2B * fC2_x;
    pSH23_y = fTmp2B * fC2_y;
    pSH23_z = fTmp2B_z * fC2;
    pSH17_x = fTmp2B * fS2_x;
    pSH17_y = fTmp2B * fS2_y;
    pSH17_z = fTmp2B_z * fS2;
    pSH24_x = 0.6258357354491763f * fC3_x;
    pSH24_y = 0.6258357354491763f * fC3_y;
    pSH16_x = 0.6258357354491763f * fS3_x;
    pSH16_y = 0.6258357354491763f * fS3_y;

    v_x += dot(v_colors,
            pSH16_x * _C(15) + pSH24_x * _C(23) +
            pSH17_x * _C(16) + pSH23_x * _C(22) +
            pSH18_x * _C(17) + pSH22_x * _C(21) +
            pSH21_x * _C(20));
    v_y += dot(v_colors,
            pSH16_y * _C(15) + pSH24_y * _C(23) +
            pSH17_y * _C(16) + pSH23_y * _C(22) +
            pSH18_y * _C(17) + pSH22_y * _C(21) +
            pSH19_y * _C(18));
    v_z += dot(v_colors,
            pSH20_z * _C(19) + pSH21_z * _C(20) +
            pSH19_z * _C(18) + pSH22_z * _C(21) +
            pSH18_z * _C(17) + pSH23_z * _C(22) +
            pSH17_z * _C(16));

    float3 dir_n = float3(x, y, z);
    float3 v_dir_n = float3(v_x, v_y, v_z);
    if (inplace != 0)
        v_dir += (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
    else
        v_dir = (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;

    #undef sv
    #undef _C
}


// ---- direction-form VJP (q8) -----------------------------------------------
// Body is a verbatim copy of _sh_coeffs_to_color_vjp<inplace>(float3* coeffs)  // q16 -- body identical to q8 modulo load helper
// with `coeffs[i]` replaced by `_C(i)` (q16 codec load) and v_coeffs writes
// kept as fp32 via the same sv(i, v) macro.
[ForceInline]
void _sh_coeffs_to_color_vjp_q16<let inplace: int>(
    const int degree,
    const float3 dir,
    float3 coeff_dc,
    uint16_t* packed,
    float2*  bounds,
    int64_t  base,
    int64_t  bounds_stride,
    int64_t  pair_pitch,
    const float3 v_colors,
    inout float3 v_coeff_dc,
    float3 *v_coeffs,
    inout float3 v_dir,
    uint thread_id
) {
    #define _C(j) _sh_load_q16(packed, bounds, base, j, bounds_stride, pair_pitch)
    #define sv(i, v) \
        if (inplace == 0) v_coeffs[i] = (v); \
        else if (inplace == 1) v_coeffs[i] += (v); \
        else if (inplace == 2) { \
            float3 temp = (v); \
            if (isfinite(dot(temp, temp)) && dot(temp, temp) != 0.0) { \
                InterlockedAdd(v_coeffs[i].x, temp.x); \
                InterlockedAdd(v_coeffs[i].y, temp.y); \
                InterlockedAdd(v_coeffs[i].z, temp.z); \
            } \
        } \
        else if (inplace == 3) { \
            sh_block_atomic_add_f3_512(v_coeffs, (uint)(i), (v), thread_id); \
        }

    if (inplace != 0) {
        v_coeff_dc += 0.2820947917738781f * v_colors;
    } else {
        v_coeff_dc = 0.2820947917738781f * v_colors;
    }
    if (degree <= 0) {
        if (inplace == 0) v_dir = float3(0);
        return;
    }

    float inorm = rsqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    float x = dir.x * inorm;
    float y = dir.y * inorm;
    float z = dir.z * inorm;
    float v_x = 0.f, v_y = 0.f, v_z = 0.f;

    sv(0, -0.48860251190292f * y * v_colors);
    sv(1, 0.48860251190292f * z * v_colors);
    sv(2, -0.48860251190292f * x * v_colors);

    v_x += -0.48860251190292f * dot(_C(2), v_colors);
    v_y += -0.48860251190292f * dot(_C(0), v_colors);
    v_z += 0.48860251190292f * dot(_C(1), v_colors);

    if (degree <= 1) {
        float3 dir_n = float3(x, y, z);
        float3 v_dir_n = float3(v_x, v_y, v_z);
        if (inplace != 0)
            v_dir += (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        else
            v_dir = (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        return;
    }

    float z2 = z * z;
    float fTmp0B = -1.092548430592079f * z;
    float fC1 = x * x - y * y;
    float fS1 = 2.f * x * y;
    float pSH6 = (0.9461746957575601f * z2 - 0.3153915652525201f);
    float pSH7 = fTmp0B * x;
    float pSH5 = fTmp0B * y;
    float pSH8 = 0.5462742152960395f * fC1;
    float pSH4 = 0.5462742152960395f * fS1;
    sv(3, pSH4 * v_colors);
    sv(4, pSH5 * v_colors);
    sv(5, pSH6 * v_colors);
    sv(6, pSH7 * v_colors);
    sv(7, pSH8 * v_colors);

    float fTmp0B_z, fC1_x, fC1_y, fS1_x, fS1_y, pSH6_z, pSH7_x, pSH7_z, pSH5_y,
        pSH5_z, pSH8_x, pSH8_y, pSH4_x, pSH4_y;

    fTmp0B_z = -1.092548430592079f;
    fC1_x = 2.f * x;
    fC1_y = -2.f * y;
    fS1_x = 2.f * y;
    fS1_y = 2.f * x;
    pSH6_z = 2.f * 0.9461746957575601f * z;
    pSH7_x = fTmp0B;
    pSH7_z = fTmp0B_z * x;
    pSH5_y = fTmp0B;
    pSH5_z = fTmp0B_z * y;
    pSH8_x = 0.5462742152960395f * fC1_x;
    pSH8_y = 0.5462742152960395f * fC1_y;
    pSH4_x = 0.5462742152960395f * fS1_x;
    pSH4_y = 0.5462742152960395f * fS1_y;

    v_x += dot(v_colors,
            pSH4_x * _C(3) + pSH8_x * _C(7) +
            pSH7_x * _C(6));
    v_y += dot(v_colors,
            pSH4_y * _C(3) + pSH8_y * _C(7) +
            pSH5_y * _C(4));
    v_z += dot(v_colors,
            pSH6_z * _C(5) + pSH7_z * _C(6) +
            pSH5_z * _C(4));

    if (degree <= 2) {
        float3 dir_n = float3(x, y, z);
        float3 v_dir_n = float3(v_x, v_y, v_z);
        if (inplace != 0)
            v_dir += (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        else
            v_dir = (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        return;
    }

    float fTmp0C = -2.285228997322329f * z2 + 0.4570457994644658f;
    float fTmp1B = 1.445305721320277f * z;
    float fC2 = x * fC1 - y * fS1;
    float fS2 = x * fS1 + y * fC1;
    float pSH12 = z * (1.865881662950577f * z2 - 1.119528997770346f);
    float pSH13 = fTmp0C * x;
    float pSH11 = fTmp0C * y;
    float pSH14 = fTmp1B * fC1;
    float pSH10 = fTmp1B * fS1;
    float pSH15 = -0.5900435899266435f * fC2;
    float pSH9 = -0.5900435899266435f * fS2;
    sv(8, pSH9 * v_colors);
    sv(9, pSH10 * v_colors);
    sv(10, pSH11 * v_colors);
    sv(11, pSH12 * v_colors);
    sv(12, pSH13 * v_colors);
    sv(13, pSH14 * v_colors);
    sv(14, pSH15 * v_colors);

    float fTmp0C_z, fTmp1B_z, fC2_x, fC2_y, fS2_x, fS2_y, pSH12_z, pSH13_x,
        pSH13_z, pSH11_y, pSH11_z, pSH14_x, pSH14_y, pSH14_z, pSH10_x, pSH10_y,
        pSH10_z, pSH15_x, pSH15_y, pSH9_x, pSH9_y;

    fTmp0C_z = -2.285228997322329f * 2.f * z;
    fTmp1B_z = 1.445305721320277f;
    fC2_x = fC1 + x * fC1_x - y * fS1_x;
    fC2_y = x * fC1_y - fS1 - y * fS1_y;
    fS2_x = fS1 + x * fS1_x + y * fC1_x;
    fS2_y = x * fS1_y + fC1 + y * fC1_y;
    pSH12_z = 3.f * 1.865881662950577f * z2 - 1.119528997770346f;
    pSH13_x = fTmp0C;
    pSH13_z = fTmp0C_z * x;
    pSH11_y = fTmp0C;
    pSH11_z = fTmp0C_z * y;
    pSH14_x = fTmp1B * fC1_x;
    pSH14_y = fTmp1B * fC1_y;
    pSH14_z = fTmp1B_z * fC1;
    pSH10_x = fTmp1B * fS1_x;
    pSH10_y = fTmp1B * fS1_y;
    pSH10_z = fTmp1B_z * fS1;
    pSH15_x = -0.5900435899266435f * fC2_x;
    pSH15_y = -0.5900435899266435f * fC2_y;
    pSH9_x = -0.5900435899266435f * fS2_x;
    pSH9_y = -0.5900435899266435f * fS2_y;

    v_x += dot(v_colors,
            pSH9_x * _C(8) + pSH15_x * _C(14) +
            pSH10_x * _C(9) + pSH14_x * _C(13) +
            pSH13_x * _C(12));

    v_y += dot(v_colors,
            pSH9_y * _C(8) + pSH15_y * _C(14) +
            pSH10_y * _C(9) + pSH14_y * _C(13) +
            pSH11_y * _C(10));

    v_z += dot(v_colors,
            pSH12_z * _C(11) + pSH13_z * _C(12) +
            pSH11_z * _C(10) + pSH14_z * _C(13) +
            pSH10_z * _C(9));

    if (degree <= 3) {
        float3 dir_n = float3(x, y, z);
        float3 v_dir_n = float3(v_x, v_y, v_z);
        if (inplace != 0)
            v_dir += (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        else
            v_dir = (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
        return;
    }

    float fTmp0D = z * (-4.683325804901025f * z2 + 2.007139630671868f);
    float fTmp1C = 3.31161143515146f * z2 - 0.47308734787878f;
    float fTmp2B = -1.770130769779931f * z;
    float fC3 = x * fC2 - y * fS2;
    float fS3 = x * fS2 + y * fC2;
    float pSH20 = (1.984313483298443f * z * pSH12 + -1.006230589874905f * pSH6);
    float pSH21 = fTmp0D * x;
    float pSH19 = fTmp0D * y;
    float pSH22 = fTmp1C * fC1;
    float pSH18 = fTmp1C * fS1;
    float pSH23 = fTmp2B * fC2;
    float pSH17 = fTmp2B * fS2;
    float pSH24 = 0.6258357354491763f * fC3;
    float pSH16 = 0.6258357354491763f * fS3;
    sv(15, pSH16 * v_colors);
    sv(16, pSH17 * v_colors);
    sv(17, pSH18 * v_colors);
    sv(18, pSH19 * v_colors);
    sv(19, pSH20 * v_colors);
    sv(20, pSH21 * v_colors);
    sv(21, pSH22 * v_colors);
    sv(22, pSH23 * v_colors);
    sv(23, pSH24 * v_colors);

    float fTmp0D_z, fTmp1C_z, fTmp2B_z, fC3_x, fC3_y, fS3_x, fS3_y, pSH20_z,
        pSH21_x, pSH21_z, pSH19_y, pSH19_z, pSH22_x, pSH22_y, pSH22_z, pSH18_x,
        pSH18_y, pSH18_z, pSH23_x, pSH23_y, pSH23_z, pSH17_x, pSH17_y, pSH17_z,
        pSH24_x, pSH24_y, pSH16_x, pSH16_y;

    fTmp0D_z = 3.f * -4.683325804901025f * z2 + 2.007139630671868f;
    fTmp1C_z = 2.f * 3.31161143515146f * z;
    fTmp2B_z = -1.770130769779931f;
    fC3_x = fC2 + x * fC2_x - y * fS2_x;
    fC3_y = x * fC2_y - fS2 - y * fS2_y;
    fS3_x = fS2 + y * fC2_x + x * fS2_x;
    fS3_y = x * fS2_y + fC2 + y * fC2_y;
    pSH20_z = 1.984313483298443f * (pSH12 + z * pSH12_z) +
                -1.006230589874905f * pSH6_z;
    pSH21_x = fTmp0D;
    pSH21_z = fTmp0D_z * x;
    pSH19_y = fTmp0D;
    pSH19_z = fTmp0D_z * y;
    pSH22_x = fTmp1C * fC1_x;
    pSH22_y = fTmp1C * fC1_y;
    pSH22_z = fTmp1C_z * fC1;
    pSH18_x = fTmp1C * fS1_x;
    pSH18_y = fTmp1C * fS1_y;
    pSH18_z = fTmp1C_z * fS1;
    pSH23_x = fTmp2B * fC2_x;
    pSH23_y = fTmp2B * fC2_y;
    pSH23_z = fTmp2B_z * fC2;
    pSH17_x = fTmp2B * fS2_x;
    pSH17_y = fTmp2B * fS2_y;
    pSH17_z = fTmp2B_z * fS2;
    pSH24_x = 0.6258357354491763f * fC3_x;
    pSH24_y = 0.6258357354491763f * fC3_y;
    pSH16_x = 0.6258357354491763f * fS3_x;
    pSH16_y = 0.6258357354491763f * fS3_y;

    v_x += dot(v_colors,
            pSH16_x * _C(15) + pSH24_x * _C(23) +
            pSH17_x * _C(16) + pSH23_x * _C(22) +
            pSH18_x * _C(17) + pSH22_x * _C(21) +
            pSH21_x * _C(20));
    v_y += dot(v_colors,
            pSH16_y * _C(15) + pSH24_y * _C(23) +
            pSH17_y * _C(16) + pSH23_y * _C(22) +
            pSH18_y * _C(17) + pSH22_y * _C(21) +
            pSH19_y * _C(18));
    v_z += dot(v_colors,
            pSH20_z * _C(19) + pSH21_z * _C(20) +
            pSH19_z * _C(18) + pSH22_z * _C(21) +
            pSH18_z * _C(17) + pSH23_z * _C(22) +
            pSH17_z * _C(16));

    float3 dir_n = float3(x, y, z);
    float3 v_dir_n = float3(v_x, v_y, v_z);
    if (inplace != 0)
        v_dir += (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;
    else
        v_dir = (v_dir_n - dot(v_dir_n, dir_n) * dir_n) * inorm;

    #undef sv
    #undef _C
}


// ---- direction-form public wrappers (q8) -----------------------------------
[ForceInline]
void sh_coeffs_to_color_vjp_q8(
    const int degree,
    const float3 dir,
    float3 coeff_dc,
    uint8_t* packed, float2* bounds, int64_t base, int64_t bounds_stride, int64_t pair_pitch,
    const float3 v_colors,
    inout float3 v_coeff_dc,
    float3* v_coeffs,
    inout float3 v_dir
) { _sh_coeffs_to_color_vjp_q8<0>(degree, dir, coeff_dc, packed, bounds, base, bounds_stride, pair_pitch, v_colors, v_coeff_dc, v_coeffs, v_dir, 0u); }

[ForceInline]
void sh_coeffs_to_color_vjp_inplace_q8(
    const int degree, const float3 dir, float3 coeff_dc,
    uint8_t* packed, float2* bounds, int64_t base, int64_t bounds_stride, int64_t pair_pitch,
    const float3 v_colors,
    inout float3 v_coeff_dc, float3* v_coeffs, inout float3 v_dir
) { _sh_coeffs_to_color_vjp_q8<1>(degree, dir, coeff_dc, packed, bounds, base, bounds_stride, pair_pitch, v_colors, v_coeff_dc, v_coeffs, v_dir, 0u); }

[ForceInline]
void sh_coeffs_to_color_vjp_atomic_q8(
    const int degree, const float3 dir, float3 coeff_dc,
    uint8_t* packed, float2* bounds, int64_t base, int64_t bounds_stride, int64_t pair_pitch,
    const float3 v_colors,
    inout float3 v_coeff_dc, float3* v_coeffs, inout float3 v_dir
) { _sh_coeffs_to_color_vjp_q8<2>(degree, dir, coeff_dc, packed, bounds, base, bounds_stride, pair_pitch, v_colors, v_coeff_dc, v_coeffs, v_dir, 0u); }


// ---- direction-form public wrappers (q16) ----------------------------------
[ForceInline]
void sh_coeffs_to_color_vjp_q16(
    const int degree, const float3 dir, float3 coeff_dc,
    uint16_t* packed, float2* bounds, int64_t base, int64_t bounds_stride, int64_t pair_pitch,
    const float3 v_colors,
    inout float3 v_coeff_dc, float3* v_coeffs, inout float3 v_dir
) { _sh_coeffs_to_color_vjp_q16<0>(degree, dir, coeff_dc, packed, bounds, base, bounds_stride, pair_pitch, v_colors, v_coeff_dc, v_coeffs, v_dir, 0u); }

[ForceInline]
void sh_coeffs_to_color_vjp_inplace_q16(
    const int degree, const float3 dir, float3 coeff_dc,
    uint16_t* packed, float2* bounds, int64_t base, int64_t bounds_stride, int64_t pair_pitch,
    const float3 v_colors,
    inout float3 v_coeff_dc, float3* v_coeffs, inout float3 v_dir
) { _sh_coeffs_to_color_vjp_q16<1>(degree, dir, coeff_dc, packed, bounds, base, bounds_stride, pair_pitch, v_colors, v_coeff_dc, v_coeffs, v_dir, 0u); }

[ForceInline]
void sh_coeffs_to_color_vjp_atomic_q16(
    const int degree, const float3 dir, float3 coeff_dc,
    uint16_t* packed, float2* bounds, int64_t base, int64_t bounds_stride, int64_t pair_pitch,
    const float3 v_colors,
    inout float3 v_coeff_dc, float3* v_coeffs, inout float3 v_dir
) { _sh_coeffs_to_color_vjp_q16<2>(degree, dir, coeff_dc, packed, bounds, base, bounds_stride, pair_pitch, v_colors, v_coeff_dc, v_coeffs, v_dir, 0u); }


// ---- (mean, R, t) form forward (q8) ----------------------------------------
[ForceInline]
float3 sh_coeffs_to_color_q8(
    const int degree,
    const float3 mean, const float3x3 R, const float3 t,
    float3 coeff_dc,
    uint8_t* packed, float2* bounds, int64_t base, int64_t bounds_stride, int64_t pair_pitch
) {
    float3 viewdir = mean_viewmat_to_unnormalized_viewdir(mean, R, t);
    float3 rgb = sh_coeffs_to_color_q8(degree, viewdir, coeff_dc, packed, bounds, base, bounds_stride, pair_pitch);
    return max(rgb + 0.5, 0.0);
}

// ---- (mean, R, t) form forward (q16) ---------------------------------------
[ForceInline]
float3 sh_coeffs_to_color_q16(
    const int degree,
    const float3 mean, const float3x3 R, const float3 t,
    float3 coeff_dc,
    uint16_t* packed, float2* bounds, int64_t base, int64_t bounds_stride, int64_t pair_pitch
) {
    float3 viewdir = mean_viewmat_to_unnormalized_viewdir(mean, R, t);
    float3 rgb = sh_coeffs_to_color_q16(degree, viewdir, coeff_dc, packed, bounds, base, bounds_stride, pair_pitch);
    return max(rgb + 0.5, 0.0);
}


// ---- (mean, R, t) form VJP (q8) --------------------------------------------
[ForceInline]
void _sh_coeffs_to_color_vjp_q8<let inplace: int>(
    const int degree,
    const float3 mean, const float3x3 R, const float3 t,
    float3 coeff_dc,
    uint8_t* packed, float2* bounds, int64_t base, int64_t bounds_stride, int64_t pair_pitch,
    float3 v_colors,
    out float3 v_coeff_dc,
    float3* v_coeffs,
    inout float3 v_mean, inout float3x3 v_R, inout float3 v_t
) {
    float3 viewdir = mean_viewmat_to_unnormalized_viewdir(mean, R, t);
    float3 rgb = sh_coeffs_to_color_q8(degree, viewdir, coeff_dc, packed, bounds, base, bounds_stride, pair_pitch);
    v_colors *= float3(float(rgb.x >= -0.5), float(rgb.y >= -0.5), float(rgb.z >= -0.5));

    float3 v_viewdir;
    _sh_coeffs_to_color_vjp_q8<inplace>(
        degree, viewdir, coeff_dc, packed, bounds, base, bounds_stride, pair_pitch,
        v_colors, v_coeff_dc, v_coeffs, v_viewdir, 0u);

    DifferentialPair<float3> dp_mean = diffPair(mean);
    DifferentialPair<float3x3> dp_R = diffPair(R);
    DifferentialPair<float3> dp_t = diffPair(t);
    bwd_diff(mean_viewmat_to_unnormalized_viewdir)(dp_mean, dp_R, dp_t, v_viewdir);
    if (inplace == 0) {
        v_mean = dp_mean.d;
        v_R = dp_R.d;
        v_t = dp_t.d;
    } else {
        v_mean += dp_mean.d;
        v_R += dp_R.d;
        v_t += dp_t.d;
    }
}

// ---- (mean, R, t) form VJP (q16) -------------------------------------------
[ForceInline]
void _sh_coeffs_to_color_vjp_q16<let inplace: int>(
    const int degree,
    const float3 mean, const float3x3 R, const float3 t,
    float3 coeff_dc,
    uint16_t* packed, float2* bounds, int64_t base, int64_t bounds_stride, int64_t pair_pitch,
    float3 v_colors,
    out float3 v_coeff_dc,
    float3* v_coeffs,
    inout float3 v_mean, inout float3x3 v_R, inout float3 v_t
) {
    float3 viewdir = mean_viewmat_to_unnormalized_viewdir(mean, R, t);
    float3 rgb = sh_coeffs_to_color_q16(degree, viewdir, coeff_dc, packed, bounds, base, bounds_stride, pair_pitch);
    v_colors *= float3(float(rgb.x >= -0.5), float(rgb.y >= -0.5), float(rgb.z >= -0.5));

    float3 v_viewdir;
    _sh_coeffs_to_color_vjp_q16<inplace>(
        degree, viewdir, coeff_dc, packed, bounds, base, bounds_stride, pair_pitch,
        v_colors, v_coeff_dc, v_coeffs, v_viewdir, 0u);

    DifferentialPair<float3> dp_mean = diffPair(mean);
    DifferentialPair<float3x3> dp_R = diffPair(R);
    DifferentialPair<float3> dp_t = diffPair(t);
    bwd_diff(mean_viewmat_to_unnormalized_viewdir)(dp_mean, dp_R, dp_t, v_viewdir);
    if (inplace == 0) {
        v_mean = dp_mean.d;
        v_R = dp_R.d;
        v_t = dp_t.d;
    } else {
        v_mean += dp_mean.d;
        v_R += dp_R.d;
        v_t += dp_t.d;
    }
}


// ---- (mean, R, t) form public wrappers (q8) --------------------------------
[ForceInline]
void sh_coeffs_to_color_vjp_q8(
    const int degree,
    const float3 mean, const float3x3 R, const float3 t,
    float3 coeff_dc,
    uint8_t* packed, float2* bounds, int64_t base, int64_t bounds_stride, int64_t pair_pitch,
    float3 v_colors,
    out float3 v_coeff_dc, float3* v_coeffs,
    out float3 v_mean, out float3x3 v_R, out float3 v_t
) {
    v_mean = float3(0.0f); v_R = float3x3(0.0f); v_t = float3(0.0f);
    _sh_coeffs_to_color_vjp_q8<0>(degree, mean, R, t, coeff_dc, packed, bounds, base, bounds_stride, pair_pitch, v_colors, v_coeff_dc, v_coeffs, v_mean, v_R, v_t);
}

[ForceInline]
void sh_coeffs_to_color_vjp_inplace_q8(
    const int degree,
    const float3 mean, const float3x3 R, const float3 t,
    float3 coeff_dc,
    uint8_t* packed, float2* bounds, int64_t base, int64_t bounds_stride, int64_t pair_pitch,
    float3 v_colors,
    out float3 v_coeff_dc, float3* v_coeffs,
    inout float3 v_mean, inout float3x3 v_R, inout float3 v_t
) { _sh_coeffs_to_color_vjp_q8<1>(degree, mean, R, t, coeff_dc, packed, bounds, base, bounds_stride, pair_pitch, v_colors, v_coeff_dc, v_coeffs, v_mean, v_R, v_t); }

[ForceInline]
void sh_coeffs_to_color_vjp_atomic_q8(
    const int degree,
    const float3 mean, const float3x3 R, const float3 t,
    float3 coeff_dc,
    uint8_t* packed, float2* bounds, int64_t base, int64_t bounds_stride, int64_t pair_pitch,
    float3 v_colors,
    out float3 v_coeff_dc, float3* v_coeffs,
    inout float3 v_mean, inout float3x3 v_R, inout float3 v_t
) { _sh_coeffs_to_color_vjp_q8<2>(degree, mean, R, t, coeff_dc, packed, bounds, base, bounds_stride, pair_pitch, v_colors, v_coeff_dc, v_coeffs, v_mean, v_R, v_t); }


// ---- (mean, R, t) form public wrappers (q16) -------------------------------
[ForceInline]
void sh_coeffs_to_color_vjp_q16(
    const int degree,
    const float3 mean, const float3x3 R, const float3 t,
    float3 coeff_dc,
    uint16_t* packed, float2* bounds, int64_t base, int64_t bounds_stride, int64_t pair_pitch,
    float3 v_colors,
    out float3 v_coeff_dc, float3* v_coeffs,
    out float3 v_mean, out float3x3 v_R, out float3 v_t
) {
    v_mean = float3(0.0f); v_R = float3x3(0.0f); v_t = float3(0.0f);
    _sh_coeffs_to_color_vjp_q16<0>(degree, mean, R, t, coeff_dc, packed, bounds, base, bounds_stride, pair_pitch, v_colors, v_coeff_dc, v_coeffs, v_mean, v_R, v_t);
}

[ForceInline]
void sh_coeffs_to_color_vjp_inplace_q16(
    const int degree,
    const float3 mean, const float3x3 R, const float3 t,
    float3 coeff_dc,
    uint16_t* packed, float2* bounds, int64_t base, int64_t bounds_stride, int64_t pair_pitch,
    float3 v_colors,
    out float3 v_coeff_dc, float3* v_coeffs,
    inout float3 v_mean, inout float3x3 v_R, inout float3 v_t
) { _sh_coeffs_to_color_vjp_q16<1>(degree, mean, R, t, coeff_dc, packed, bounds, base, bounds_stride, pair_pitch, v_colors, v_coeff_dc, v_coeffs, v_mean, v_R, v_t); }

[ForceInline]
void sh_coeffs_to_color_vjp_atomic_q16(
    const int degree,
    const float3 mean, const float3x3 R, const float3 t,
    float3 coeff_dc,
    uint16_t* packed, float2* bounds, int64_t base, int64_t bounds_stride, int64_t pair_pitch,
    float3 v_colors,
    out float3 v_coeff_dc, float3* v_coeffs,
    inout float3 v_mean, inout float3x3 v_R, inout float3 v_t
) { _sh_coeffs_to_color_vjp_q16<2>(degree, mean, R, t, coeff_dc, packed, bounds, base, bounds_stride, pair_pitch, v_colors, v_coeff_dc, v_coeffs, v_mean, v_R, v_t); }


// ---- exports (q8 / q16, splat path) ----------------------------------------
// Per-degree (mean, R, t) entry points + the two VJP variants used by the
// projection bwd: vjp_inplace (writes into local SH grad buffer) and
// vjp_atomic (atomicAdd into global engine().grad.features_sh). The direction-
// only background-SH path stays fp32 -- this export block only covers the
// splat features_sh value-quant path.
#define _DECL_Q(sh_degree, qbits, dtype) \
    [CudaDeviceExport] \
    float3 sh##sh_degree##_to_color_q##qbits(float3 mean, float3x3 R, float3 t, float3 coeff_dc, dtype* packed, float2* bounds, int64_t base, int64_t bounds_stride, int64_t pair_pitch) \
        { return sh_coeffs_to_color_q##qbits(sh_degree, mean, R, t, coeff_dc, packed, bounds, base, bounds_stride, pair_pitch); } \
    [CudaDeviceExport] \
    void sh##sh_degree##_to_color_q##qbits##_vjp_inplace(float3 mean, float3x3 R, float3 t, float3 coeff_dc, dtype* packed, float2* bounds, int64_t base, int64_t bounds_stride, int64_t pair_pitch, float3 v_colors, out float3 v_coeff_dc, float3 *v_coeffs, inout float3 v_mean, inout float3x3 v_R, inout float3 v_t) \
        { sh_coeffs_to_color_vjp_inplace_q##qbits(sh_degree, mean, R, t, coeff_dc, packed, bounds, base, bounds_stride, pair_pitch, v_colors, v_coeff_dc, v_coeffs, v_mean, v_R, v_t); } \
    [CudaDeviceExport] \
    void sh##sh_degree##_to_color_q##qbits##_vjp_atomic(float3 mean, float3x3 R, float3 t, float3 coeff_dc, dtype* packed, float2* bounds, int64_t base, int64_t bounds_stride, int64_t pair_pitch, float3 v_colors, out float3 v_coeff_dc, float3 *v_coeffs, inout float3 v_mean, inout float3x3 v_R, inout float3 v_t) \
        { sh_coeffs_to_color_vjp_atomic_q##qbits(sh_degree, mean, R, t, coeff_dc, packed, bounds, base, bounds_stride, pair_pitch, v_colors, v_coeff_dc, v_coeffs, v_mean, v_R, v_t); } \

_DECL_Q(0,  8, uint8_t)  _DECL_Q(1,  8, uint8_t)  _DECL_Q(2,  8, uint8_t)  _DECL_Q(3,  8, uint8_t)  _DECL_Q(4,  8, uint8_t)
_DECL_Q(0, 16, uint16_t) _DECL_Q(1, 16, uint16_t) _DECL_Q(2, 16, uint16_t) _DECL_Q(3, 16, uint16_t) _DECL_Q(4, 16, uint16_t)

#undef _DECL_Q
