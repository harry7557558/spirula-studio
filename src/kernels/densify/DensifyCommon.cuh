#pragma once

// Shared preamble for the densification kernels.
//
// Densify.cu was one file (~2.5k lines). It is split by function; the parts
// all declare into the single Densify.cuh via HEADER_SOURCES['Densify'] in
// generate_headers.py. See docs/codegen.md.
//
//   DensifySampling.cu    quantile / median normalization, indexing,
//                         scatter-reduce, weighted sampling w/o replacement
//   DensifyScoring.cu     covariance-based scale init, densify param update
//   Relocation.cu         relocation with long-axis split
//   McmcRelocation.cu     MCMC relocation and MCMC noise
//   DensifySplitFilter.cu long-axis split, image (edge) filters
//
// The quantized optim-state copy/zero helpers shared by the two relocation
// paths live in DensifyQuantCopy.cuh.

#include "kernels/densify/Densify.cuh"

#ifdef __CUDACC__
#include "generated/slang.cuh"
namespace SlangDensify {
#include "generated/set_namespace.cuh"
#include "generated/densify.cuh"
}
namespace SlangProjectionUtils {
#include "generated/set_namespace.cuh"
// #include "generated/projection_utils.cuh"
#include "generated/primitive_3dgs.cuh"
}
#endif

#include "core/Common.cuh"

#include <cub/cub.cuh>


// ================
// Helper: DeviceTensor3D<T> -> TensorView<U, 4>
// ================

template<typename U, typename T>
static TensorView<U, 4> _dt3d_to_tv4(const DeviceTensor3D<T>& dt) {
    static_assert(sizeof(T) % sizeof(U) == 0, "T must be a multiple of U");
    constexpr int C = sizeof(T) / sizeof(U);
    TensorView<U, 4> v;
    v.data = (U*)dt.data_ptr();
    v.shape[0] = dt.template size<0>();
    v.shape[1] = dt.template size<1>();
    v.shape[2] = dt.template size<2>();
    v.shape[3] = C;
    long HW = v.shape[1] * v.shape[2];
    v.strides[0] = HW * C;
    v.strides[1] = v.shape[2] * C;
    v.strides[2] = C;
    v.strides[3] = 1;
    return v;
}

// Build a TensorView<float,4> for [B,H,W,1] from a DeviceTensor3D<float>
inline TensorView<float, 4> _dt3d_float_to_tv4_1ch(const DeviceTensor3D<float>& dt) {
    TensorView<float, 4> v;
    v.data = (float*)dt.data_ptr();
    v.shape[0] = dt.template size<0>();
    v.shape[1] = dt.template size<1>();
    v.shape[2] = dt.template size<2>();
    v.shape[3] = 1;
    long HW = v.shape[1] * v.shape[2];
    v.strides[0] = HW;
    v.strides[1] = v.shape[2];
    v.strides[2] = 1;
    v.strides[3] = 1;
    return v;
}


