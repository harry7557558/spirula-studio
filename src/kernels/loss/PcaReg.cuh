#pragma once

// The local-PCA regularizer's launchers (shaders/pca_reg.slang). The engine
// picks the method and orders the calls: engine/EnginePcaReg.cpp. What they
// return is in the RasterBwd arena (POOL_ALIAS_TABLE), valid until it closes.

#include <core/Tensor.h>

#include <cstdint>
#include <tuple>

// Must match kPcaMode* and kPcaMaxNeighbors in shaders/pca_reg.slang.
enum class PcaRegMode : int { Tree = 0, Window = 1, Best = 2, List = 3 };
enum class PcaRegCovariance : int { Centers = 0, OpacityCenters = 1, GaussianMixture = 2 };
inline constexpr int kPcaRegMaxNeighbors = 32;

inline uint32_t pca_reg_terms(float normal, float surfel, float erank, float transparency) {
    return (normal != 0 ? 1u : 0u) | (surfel != 0 ? 2u : 0u) |
           (erank != 0 ? 4u : 0u) | (transparency != 0 ? 8u : 0u);
}

// The compiled neighbour capacities, kPcaRegMaxNeighbors the largest: the
// pass for k others runs the smallest that holds them.
inline int pca_reg_capacity(int k) { return k <= 8 ? 8 : (k <= 16 ? 16 : 32); }

// The window must fit one workgroup's slab: kPcaMaxHalf in shaders/pca_reg.slang.
inline bool pca_reg_uses_slab(int mode, int k, int window, float w_transparency) {
    return mode == (int)PcaRegMode::Best && (window > k + 1 ? window : k + 1) / 2 <= 64 &&
           w_transparency == 0;
}


/* == AUTO HEADER GENERATOR - DO NOT EDIT THIS LINE OR ANYTHING BELOW THIS LINE == */



std::tuple<DeviceVector<int32_t>, DeviceVector<float4>, DeviceVector<uint32_t>>
pca_reg_order_tensor(
    int64_t num_splats,
    DeviceVector<float3> means,     // [N]
    DeviceVector<int32_t> keep      // [N] or empty: also receives the order
);


DeviceVector<float4> pca_reg_gather_tensor(
    int64_t num_splats,
    DeviceVector<float3> means,     // [N]
    DeviceVector<int32_t> perm      // [N] rank -> splat
);


std::tuple<DeviceVector<float4>, DeviceVector<int4>, DeviceVector<int32_t>> pca_reg_tree_tensor(
    int64_t num_splats,             // >= 2
    DeviceVector<float4> sorted,    // [N] Morton-ordered centres
    DeviceVector<uint32_t> box      // [6] the order's box
);


void pca_reg_neighborhood_tensor(
    int64_t num_splats,
    int mode,                       // PcaRegMode
    int k,                          // neighbours besides the splat, < kPcaRegMaxNeighbors
    int window,                     // Best: candidates in Morton order
    DeviceVector<float3> means,
    DeviceVector<float4> quats,
    DeviceVector<float3> scales,
    DeviceVector<float> opacities,
    int covariance,                // PcaRegCovariance
    DeviceVector<int32_t> perm,     // [N] rank -> splat
    DeviceVector<float4> sorted,    // [N] by rank; unused by List
    DeviceVector<float4> nodes,     // Tree only: pca_reg_tree_tensor's
    DeviceVector<int4> meta,
    DeviceVector<int32_t> ranges,
    DeviceVector<int32_t> nbrs,     // [N, k] splat ids by rank: List reads, store_list writes
    bool store_list,
    DeviceVector<float> radii,      // [N] this step's screen radii, or empty
    bool gate_means,                // means of splats with radius 0 get nothing
    bool scale_agnostic,            // the terms relative to the spread and largest scale
    DeviceVector<float3> v_means,   // accumulated
    DeviceVector<float4> v_quats,
    DeviceVector<float3> v_scales,
    DeviceVector<float> v_opacities,
    float w_plane,
    float w_tangent,
    float w_normal,
    float w_surfel,
    float w_erank,
    float w_transparency,
    DeviceVector<float> loss_sums   // [5] accumulated, unweighted sums
);
