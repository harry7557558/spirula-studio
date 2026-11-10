// CUDA launchers of the local-PCA regularizer; the per-thread work is
// shaders/pca_reg.slang, shared with backend/vulkan/kernels/PcaReg.cpp.

#include "kernels/loss/PcaReg.cuh"

#include "core/Common.cuh"

#include <cub/cub.cuh>

#include "generated/slang.cuh"
namespace SlangPcaReg {
#include "generated/set_namespace.cuh"
#include "generated/pca_reg.cuh"
}

namespace {

constexpr int kBoxBlock = 256;
constexpr int kOwnerBlock = 128;

// Min then max per axis, as order keys (pca_order_key).
__global__ void pca_reg_box_kernel(uint32_t n, const float* __restrict__ means,
                                   uint32_t* __restrict__ box) {
    __shared__ uint32_t red[6][kBoxBlock / 32];
    uint32_t lo[3] = {0xffffffffu, 0xffffffffu, 0xffffffffu};
    uint32_t hi[3] = {0u, 0u, 0u};
    for (uint32_t i = blockIdx.x * blockDim.x + threadIdx.x; i < n;
         i += gridDim.x * blockDim.x) {
        for (int c = 0; c < 3; c++) {
            const float v = means[3 * i + c];
            if (!isfinite(v)) continue;
            const uint32_t key = SlangPcaReg::pca_order_key(v);
            lo[c] = min(lo[c], key);
            hi[c] = max(hi[c], key);
        }
    }
    for (int o = 16; o > 0; o >>= 1)
        for (int c = 0; c < 3; c++) {
            lo[c] = min(lo[c], __shfl_xor_sync(0xffffffffu, lo[c], o));
            hi[c] = max(hi[c], __shfl_xor_sync(0xffffffffu, hi[c], o));
        }
    const int warp = threadIdx.x / 32, lane = threadIdx.x % 32;
    if (lane == 0)
        for (int c = 0; c < 3; c++) {
            red[c][warp] = lo[c];
            red[3 + c][warp] = hi[c];
        }
    __syncthreads();
    if (threadIdx.x < 6) {
        const int c = threadIdx.x;
        uint32_t v = red[c][0];
        for (int w = 1; w < kBoxBlock / 32; w++)
            v = c < 3 ? min(v, red[c][w]) : max(v, red[c][w]);
        if (c < 3) atomicMin(box + c, v);
        else atomicMax(box + c, v);
    }
}

__global__ void pca_reg_keys_kernel(uint32_t n, float* __restrict__ means,
                                    uint32_t* __restrict__ box,
                                    int64_t* __restrict__ keys, int32_t* __restrict__ vals) {
    const uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    const uint2 code = SlangPcaReg::pca_reg_key(
        make_float3(means[3 * i], means[3 * i + 1], means[3 * i + 2]), box);
    keys[i] = (int64_t)(((uint64_t)code.y << 32) | (uint64_t)code.x);
    vals[i] = (int32_t)i;
}

__global__ void pca_reg_gather_kernel(uint32_t n, int32_t* perm, float* means,
                                      float4* sorted, int32_t* perm_out,
                                      uint32_t write_perm) {
    const uint32_t r = blockIdx.x * blockDim.x + threadIdx.x;
    if (r >= n) return;
    SlangPcaReg::pca_reg_gather(r, perm, means, sorted, perm_out, write_perm);
}

__global__ void pca_reg_build_kernel(uint32_t n, float4* sorted, uint32_t* box,
                                     float4* nodes, int4* meta, int32_t* ranges) {
    const uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    SlangPcaReg::pca_reg_build(i, n, sorted, box, nodes, (uint32_t*)nodes, meta, ranges);
}

// red: [5][kOwnerBlock / 32] of __shared__ floats, the caller's.
__device__ __forceinline__ void pca_reg_reduce_losses(float3 l, float2 extra, float* red,
                                                      float* loss_sums) {
    for (int o = 16; o > 0; o >>= 1) {
        l.x += __shfl_xor_sync(0xffffffffu, l.x, o);
        l.y += __shfl_xor_sync(0xffffffffu, l.y, o);
        l.z += __shfl_xor_sync(0xffffffffu, l.z, o);
        extra.x += __shfl_xor_sync(0xffffffffu, extra.x, o);
        extra.y += __shfl_xor_sync(0xffffffffu, extra.y, o);
    }
    const int warps = kOwnerBlock / 32, warp = threadIdx.x / 32, lane = threadIdx.x % 32;
    if (lane == 0) {
        red[0 * warps + warp] = l.x;
        red[1 * warps + warp] = l.y;
        red[2 * warps + warp] = l.z;
        red[3 * warps + warp] = extra.x;
        red[4 * warps + warp] = extra.y;
    }
    __syncthreads();
    if (threadIdx.x < 5) {
        float v = 0.f;
        for (int w = 0; w < warps; w++) v += red[threadIdx.x * warps + w];
        atomicAdd(loss_sums + threadIdx.x, v);
    }
}

template <int K, bool SA>
__device__ float3 pca_reg_owner_k(uint32_t r, uint32_t mode, uint32_t n, uint32_t k,
                                  uint32_t window, float* means, float* quats, float* scales, float* opacities, uint32_t covariance, uint32_t terms,
                                  int32_t* perm, float4* sorted, float4* nodes, int4* meta,
                                  int32_t* ranges, int32_t* nbrs, uint32_t store_list,
                                  float* radii, uint32_t gate_means,
                                  float* v_means, float* v_quats, float* v_scales, float* v_opacities,
                                  float w_plane, float w_tangent, float w_normal,
                                  float w_surfel, float w_erank, float w_transparency, float2* extra) {
#define SS_PCA_REG_PASS                                                                    \
    r, mode, n, k, window, means, quats, scales, opacities, covariance, terms, perm, sorted, nodes, meta, ranges,   \
        nbrs, store_list, radii, gate_means, v_means, v_quats, v_scales, v_opacities, w_plane,      \
        w_tangent, w_normal, w_surfel, w_erank, w_transparency, extra
    if constexpr (SA) {
        if constexpr (K == 8) return SlangPcaReg::pca_reg_owner_k8_sa(SS_PCA_REG_PASS);
        else if constexpr (K == 16) return SlangPcaReg::pca_reg_owner_k16_sa(SS_PCA_REG_PASS);
        else return SlangPcaReg::pca_reg_owner_k32_sa(SS_PCA_REG_PASS);
    } else {
        if constexpr (K == 8) return SlangPcaReg::pca_reg_owner_k8(SS_PCA_REG_PASS);
        else if constexpr (K == 16) return SlangPcaReg::pca_reg_owner_k16(SS_PCA_REG_PASS);
        else return SlangPcaReg::pca_reg_owner_k32(SS_PCA_REG_PASS);
    }
#undef SS_PCA_REG_PASS
}

template <int K, bool SA, bool Extended>
__global__ void pca_reg_owner_kernel(
    uint32_t n, uint32_t mode, uint32_t k, uint32_t window, float* means, float* quats,
    float* scales, float* opacities, uint32_t covariance, uint32_t terms, int32_t* perm, float4* sorted, float4* nodes, int4* meta,
    int32_t* ranges, int32_t* nbrs, uint32_t store_list, float* radii, uint32_t gate_means,
    float* v_means, float* v_quats, float* v_scales, float* v_opacities,
    float w_plane, float w_tangent, float w_normal, float w_surfel, float w_erank, float w_transparency,
    float* __restrict__ loss_sums) {
    __shared__ float red[5][kOwnerBlock / 32];
    const uint32_t r = blockIdx.x * blockDim.x + threadIdx.x;
    // Every lane runs the search, whose votes take the full warp; a lane past
    // n repeats the last rank and writes nothing.
    const bool valid = r < n;
    const float on = valid ? 1.0f : 0.0f;
    float2 extra;
    float3 l = pca_reg_owner_k<K, SA>(
        valid ? r : n - 1, mode, n, k, window, means, quats, scales, opacities,
        Extended ? covariance : 0u, Extended ? terms : (terms & 3u), perm, sorted, nodes, meta,
        ranges, nbrs, valid ? store_list : 0u, radii, gate_means, v_means, v_quats, v_scales, v_opacities,
        on * w_plane,
        on * w_tangent, on * w_normal, on * w_surfel,
        Extended ? on * w_erank : 0.0f, Extended ? on * w_transparency : 0.0f, &extra);
    if (!valid) { l = make_float3(0.f, 0.f, 0.f); extra = make_float2(0.f, 0.f); }
    pca_reg_reduce_losses(l, extra, (float*)red, loss_sums);
}

template <int K, bool SA>
__global__ void pca_reg_slab_kernel(
    uint32_t n, uint32_t k, uint32_t window, float* quats, float* scales, float* opacities,
    uint32_t covariance, uint32_t terms, int32_t* perm, float4* sorted, int32_t* nbrs,
    uint32_t store_list, float* radii, uint32_t gate_means, float* v_means, float* v_quats,
    float* v_scales, float* v_opacities, float w_plane, float w_tangent, float w_normal,
    float w_surfel, float w_erank, float* __restrict__ loss_sums) {
    __shared__ float red[5][kOwnerBlock / 32];
    float2 extra;
#define SS_PCA_REG_PASS                                                                      \
    threadIdx.x, blockIdx.x * blockDim.x, n, k, window, quats, scales, opacities, covariance, \
        terms, perm, sorted, nbrs, store_list, radii, gate_means, v_means, v_quats, v_scales, \
        v_opacities, w_plane, w_tangent, w_normal, w_surfel, w_erank, &extra
    float3 l;
    if constexpr (SA) {
        if constexpr (K == 8) l = SlangPcaReg::pca_reg_slab_k8_sa(SS_PCA_REG_PASS);
        else if constexpr (K == 16) l = SlangPcaReg::pca_reg_slab_k16_sa(SS_PCA_REG_PASS);
        else l = SlangPcaReg::pca_reg_slab_k32_sa(SS_PCA_REG_PASS);
    } else {
        if constexpr (K == 8) l = SlangPcaReg::pca_reg_slab_k8(SS_PCA_REG_PASS);
        else if constexpr (K == 16) l = SlangPcaReg::pca_reg_slab_k16(SS_PCA_REG_PASS);
        else l = SlangPcaReg::pca_reg_slab_k32(SS_PCA_REG_PASS);
    }
#undef SS_PCA_REG_PASS
    pca_reg_reduce_losses(l, extra, (float*)red, loss_sums);
}

}  // namespace

/*[AutoHeaderGeneratorExport]*/
std::tuple<DeviceVector<int32_t>, DeviceVector<float4>, DeviceVector<uint32_t>>
pca_reg_order_tensor(
    int64_t num_splats,
    DeviceVector<float3> means,     // [N]
    DeviceVector<int32_t> keep      // [N] or empty: also receives the order
) {
    if (num_splats <= 0) return {};
    const uint32_t n = (uint32_t)num_splats;
    DeviceVector<uint32_t> box;
    box.resize(PoolSlot::PcaRegBox, 6);
    backend::memset_async(box.data_ptr(), 0xff, 3 * sizeof(uint32_t), backend::kDefaultStream);
    backend::memset_async(box.data_ptr() + 3, 0, 3 * sizeof(uint32_t), backend::kDefaultStream);
    const int box_blocks = (int)std::min<int64_t>(_CEIL_DIV(num_splats, kBoxBlock), 1024);
    pca_reg_box_kernel<<<box_blocks, kBoxBlock>>>(n, (const float*)means.data_ptr(),
                                                  box.data_ptr());
    CHECK_DEVICE_ERROR(cudaGetLastError());

    // The sorted centres take the two key buffers' place once the sort is done.
    DeviceVector<int64_t> keys;
    DeviceVector<int32_t> vals;
    keys.resize(PoolSlot::PcaRegKeys, 2 * num_splats);
    vals.resize(PoolSlot::PcaRegVals, 2 * num_splats);
    pca_reg_keys_kernel<<<_LAUNCH_ARGS_1D(num_splats, 256)>>>(
        n, (float*)means.data_ptr(), box.data_ptr(), keys.data_ptr(), vals.data_ptr());
    CHECK_DEVICE_ERROR(cudaGetLastError());
    cub::DoubleBuffer<int64_t> d_keys(keys.data_ptr(), keys.data_ptr() + num_splats);
    cub::DoubleBuffer<int32_t> d_vals(vals.data_ptr(), vals.data_ptr() + num_splats);
    CUB_WRAPPER(cub::DeviceRadixSort::SortPairs, d_keys, d_vals, (int)num_splats, 0, 63);
    CHECK_DEVICE_ERROR(cudaGetLastError());

    float4* sorted = (float4*)keys.data_ptr();
    pca_reg_gather_kernel<<<_LAUNCH_ARGS_1D(num_splats, 256)>>>(
        n, d_vals.Current(), (float*)means.data_ptr(), sorted,
        keep.data_ptr() ? keep.data_ptr() : d_vals.Current(), keep.data_ptr() ? 1u : 0u);
    CHECK_DEVICE_ERROR(cudaGetLastError());
    return {DeviceVector<int32_t>(TorchTensorView((uint64_t)d_vals.Current(), 4, {num_splats, 1})),
            DeviceVector<float4>(TorchTensorView((uint64_t)sorted, 16, {num_splats, 1})), box};
}

/*[AutoHeaderGeneratorExport]*/
DeviceVector<float4> pca_reg_gather_tensor(
    int64_t num_splats,
    DeviceVector<float3> means,     // [N]
    DeviceVector<int32_t> perm      // [N] rank -> splat
) {
    if (num_splats <= 0) return {};
    DeviceVector<int64_t> keys;
    keys.resize(PoolSlot::PcaRegKeys, 2 * num_splats);
    float4* sorted = (float4*)keys.data_ptr();
    pca_reg_gather_kernel<<<_LAUNCH_ARGS_1D(num_splats, 256)>>>(
        (uint32_t)num_splats, perm.data_ptr(), (float*)means.data_ptr(), sorted,
        perm.data_ptr(), 0u);
    CHECK_DEVICE_ERROR(cudaGetLastError());
    return DeviceVector<float4>(TorchTensorView((uint64_t)sorted, 16, {num_splats, 1}));
}

/*[AutoHeaderGeneratorExport]*/
std::tuple<DeviceVector<float4>, DeviceVector<int4>, DeviceVector<int32_t>> pca_reg_tree_tensor(
    int64_t num_splats,             // >= 2
    DeviceVector<float4> sorted,    // [N] Morton-ordered centres
    DeviceVector<uint32_t> box      // [6] the order's box
) {
    if (num_splats < 2) return {};
    DeviceVector<float4> nodes;
    DeviceVector<int4> meta;
    DeviceVector<int32_t> ranges;
    nodes.resize(PoolSlot::PcaRegNodes, 2 * (num_splats - 1));
    meta.resize(PoolSlot::PcaRegMeta, num_splats - 1);
    ranges.resize(PoolSlot::PcaRegRanges, num_splats);
    backend::memset_async(ranges.data_ptr(), 0xff, num_splats * sizeof(int32_t),
                          backend::kDefaultStream);
    pca_reg_build_kernel<<<_LAUNCH_ARGS_1D(num_splats, 256)>>>(
        (uint32_t)num_splats, sorted.data_ptr(), box.data_ptr(), nodes.data_ptr(),
        meta.data_ptr(), ranges.data_ptr());
    CHECK_DEVICE_ERROR(cudaGetLastError());
    return {nodes, meta, ranges};
}

/*[AutoHeaderGeneratorExport]*/
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
) {
    if (num_splats <= 0 || k <= 0) return;
    const int cap = pca_reg_capacity(k);
    const uint32_t terms = pca_reg_terms(w_normal, w_surfel, w_erank, w_transparency);
    if (pca_reg_uses_slab(mode, k, window, w_transparency)) {
        auto slab = scale_agnostic
            ? (cap == 8 ? pca_reg_slab_kernel<8, true>
               : cap == 16 ? pca_reg_slab_kernel<16, true> : pca_reg_slab_kernel<32, true>)
            : (cap == 8 ? pca_reg_slab_kernel<8, false>
               : cap == 16 ? pca_reg_slab_kernel<16, false> : pca_reg_slab_kernel<32, false>);
        slab<<<_LAUNCH_ARGS_1D(num_splats, kOwnerBlock)>>>(
            (uint32_t)num_splats, (uint32_t)k, (uint32_t)window, (float*)quats.data_ptr(),
            (float*)scales.data_ptr(), opacities.data_ptr(), (uint32_t)covariance, terms,
            perm.data_ptr(), sorted.data_ptr(), nbrs.data_ptr(), store_list ? 1u : 0u,
            radii.data_ptr(), gate_means && radii.data_ptr() ? 1u : 0u, (float*)v_means.data_ptr(),
            (float*)v_quats.data_ptr(), (float*)v_scales.data_ptr(), v_opacities.data_ptr(),
            w_plane, w_tangent, w_normal, w_surfel, w_erank, loss_sums.data_ptr());
        CHECK_DEVICE_ERROR(cudaGetLastError());
        return;
    }
    auto kernel = scale_agnostic
        ? (cap == 8 ? pca_reg_owner_kernel<8, true, true>
           : cap == 16 ? pca_reg_owner_kernel<16, true, true> : pca_reg_owner_kernel<32, true, true>)
        : (cap == 8 ? pca_reg_owner_kernel<8, false, true>
           : cap == 16 ? pca_reg_owner_kernel<16, false, true> : pca_reg_owner_kernel<32, false, true>);
    if (covariance == 0 && w_erank == 0 && w_transparency == 0)
        kernel = scale_agnostic
            ? (cap == 8 ? pca_reg_owner_kernel<8, true, false>
               : cap == 16 ? pca_reg_owner_kernel<16, true, false> : pca_reg_owner_kernel<32, true, false>)
            : (cap == 8 ? pca_reg_owner_kernel<8, false, false>
               : cap == 16 ? pca_reg_owner_kernel<16, false, false> : pca_reg_owner_kernel<32, false, false>);
    kernel<<<_LAUNCH_ARGS_1D(num_splats, kOwnerBlock)>>>(
        (uint32_t)num_splats, (uint32_t)mode, (uint32_t)k, (uint32_t)window,
        (float*)means.data_ptr(), (float*)quats.data_ptr(), (float*)scales.data_ptr(),
        opacities.data_ptr(), (uint32_t)covariance, terms,
        perm.data_ptr(), sorted.data_ptr(), nodes.data_ptr(), meta.data_ptr(),
        ranges.data_ptr(), nbrs.data_ptr(), store_list ? 1u : 0u, radii.data_ptr(),
        gate_means && radii.data_ptr() ? 1u : 0u, (float*)v_means.data_ptr(),
        (float*)v_quats.data_ptr(), (float*)v_scales.data_ptr(), v_opacities.data_ptr(), w_plane, w_tangent,
        w_normal, w_surfel, w_erank, w_transparency,
        loss_sums.data_ptr());
    CHECK_DEVICE_ERROR(cudaGetLastError());
}
