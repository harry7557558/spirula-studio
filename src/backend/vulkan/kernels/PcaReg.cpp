// Vulkan implementation of kernels/loss/PcaReg.cu. Device work:
// shaders/pca_reg.slang (entries) over the shared src/shaders/pca_reg.slang.

#include <kernels/loss/PcaReg.cuh>

#include "backend/vulkan/kernels/KernelCommon.h"

namespace {

// Each mirrors the struct of the same name in shaders/pca_reg.slang.
struct PcaBoxParams {
    uint64_t means, box;
    uint32_t n, stride;
};
static_assert(sizeof(PcaBoxParams) == 2 * 8 + 2 * 4, "params layout must match the slang struct");

struct PcaKeysParams {
    uint64_t means, box, keys, vals;
    uint32_t n, wgs_per_row;
};
static_assert(sizeof(PcaKeysParams) == 4 * 8 + 2 * 4, "params layout must match the slang struct");

struct PcaGatherParams {
    uint64_t perm, means, sorted, perm_out;
    uint32_t n, write_perm, wgs_per_row, _pad0;
};
static_assert(sizeof(PcaGatherParams) == 4 * 8 + 4 * 4, "params layout must match the slang struct");

struct PcaBuildParams {
    uint64_t sorted, box, nodes, node_bits, meta, ranges;
    uint32_t n, wgs_per_row;
};
static_assert(sizeof(PcaBuildParams) == 6 * 8 + 2 * 4, "params layout must match the slang struct");

struct PcaOwnerParams {
    uint64_t means, quats, scales, perm, sorted, nodes, meta, ranges, nbrs, radii, v_means,
        v_quats, v_scales, loss_sums, opacities, v_opacities;
    uint32_t n, mode, k, window;
    uint32_t store_list, wgs_per_row;
    float w_plane, w_tangent, w_normal, w_surfel;
    uint32_t gate_means, _pad0;
    float w_erank, w_transparency;
};
static_assert(sizeof(PcaOwnerParams) == 16 * 8 + 14 * 4, "params layout must match the slang struct");

constexpr uint32_t kBoxGroup = 256;
constexpr uint32_t kOwnerGroup = 128;

void gather(int64_t n, const int32_t* perm, const float3* means, float4* sorted,
            int32_t* perm_out) {
    PcaGatherParams p{};
    p.perm = (uint64_t)perm;
    p.means = (uint64_t)means;
    p.sorted = (uint64_t)sorted;
    p.perm_out = vkk::or_fallback(perm_out);
    p.n = (uint32_t)n;
    p.write_perm = perm_out ? 1u : 0u;
    vkk::dispatch_flat("pca_reg.pca_gather", {}, n, 256, &p, sizeof(p), &p.wgs_per_row);
}

}  // namespace

std::tuple<DeviceVector<int32_t>, DeviceVector<float4>, DeviceVector<uint32_t>>
pca_reg_order_tensor(int64_t num_splats, DeviceVector<float3> means, DeviceVector<int32_t> keep) {
    if (num_splats <= 0) return {};
    DeviceVector<uint32_t> box;
    box.resize(PoolSlot::PcaRegBox, 6);
    backend::memset_async(box.data_ptr(), 0xff, 3 * sizeof(uint32_t), backend::kDefaultStream);
    backend::memset_async(box.data_ptr() + 3, 0, 3 * sizeof(uint32_t), backend::kDefaultStream);
    {
        const uint32_t groups =
            (uint32_t)std::min<int64_t>((num_splats + kBoxGroup - 1) / kBoxGroup, 1024);
        PcaBoxParams p{};
        p.means = (uint64_t)means.data_ptr();
        p.box = (uint64_t)box.data_ptr();
        p.n = (uint32_t)num_splats;
        p.stride = groups * kBoxGroup;
        vkk::dispatch("pca_reg.pca_box", {}, groups, 1, 1, &p, sizeof(p));
    }

    // The sorted centres take the two key buffers' place once the sort is done.
    DeviceVector<int64_t> keys;
    DeviceVector<int32_t> vals;
    keys.resize(PoolSlot::PcaRegKeys, 2 * num_splats);
    vals.resize(PoolSlot::PcaRegVals, 2 * num_splats);
    {
        PcaKeysParams p{};
        p.means = (uint64_t)means.data_ptr();
        p.box = (uint64_t)box.data_ptr();
        p.keys = (uint64_t)keys.data_ptr();
        p.vals = (uint64_t)vals.data_ptr();
        p.n = (uint32_t)num_splats;
        vkk::dispatch_flat("pca_reg.pca_keys", {}, num_splats, 256, &p, sizeof(p),
                           &p.wgs_per_row);
    }
    backend::DoubleBuffer<int64_t> d_keys(keys.data_ptr(), keys.data_ptr() + num_splats);
    backend::DoubleBuffer<int32_t> d_vals(vals.data_ptr(), vals.data_ptr() + num_splats);
    backend::sort_pairs<int64_t, int32_t>(d_keys, d_vals, num_splats, 0, 63);
    float4* sorted = (float4*)keys.data_ptr();
    gather(num_splats, d_vals.current(), means.data_ptr(), sorted, keep.data_ptr());
    return {DeviceVector<int32_t>(TorchTensorView((uint64_t)d_vals.current(), 4, {num_splats, 1})),
            DeviceVector<float4>(TorchTensorView((uint64_t)sorted, 16, {num_splats, 1})), box};
}

DeviceVector<float4> pca_reg_gather_tensor(int64_t num_splats, DeviceVector<float3> means,
                                           DeviceVector<int32_t> perm) {
    if (num_splats <= 0) return {};
    DeviceVector<int64_t> keys;
    keys.resize(PoolSlot::PcaRegKeys, 2 * num_splats);
    float4* sorted = (float4*)keys.data_ptr();
    gather(num_splats, perm.data_ptr(), means.data_ptr(), sorted, nullptr);
    return DeviceVector<float4>(TorchTensorView((uint64_t)sorted, 16, {num_splats, 1}));
}

std::tuple<DeviceVector<float4>, DeviceVector<int4>, DeviceVector<int32_t>> pca_reg_tree_tensor(
    int64_t num_splats, DeviceVector<float4> sorted, DeviceVector<uint32_t> box) {
    if (num_splats < 2) return {};
    DeviceVector<float4> nodes;
    DeviceVector<int4> meta;
    DeviceVector<int32_t> ranges;
    nodes.resize(PoolSlot::PcaRegNodes, 2 * (num_splats - 1));
    meta.resize(PoolSlot::PcaRegMeta, num_splats - 1);
    ranges.resize(PoolSlot::PcaRegRanges, num_splats);
    backend::memset_async(ranges.data_ptr(), 0xff, num_splats * sizeof(int32_t),
                          backend::kDefaultStream);
    PcaBuildParams p{};
    p.sorted = (uint64_t)sorted.data_ptr();
    p.box = (uint64_t)box.data_ptr();
    p.nodes = (uint64_t)nodes.data_ptr();
    p.node_bits = (uint64_t)nodes.data_ptr();
    p.meta = (uint64_t)meta.data_ptr();
    p.ranges = (uint64_t)ranges.data_ptr();
    p.n = (uint32_t)num_splats;
    vkk::dispatch_flat("pca_reg.pca_build", {}, num_splats, 256, &p, sizeof(p), &p.wgs_per_row);
    return {nodes, meta, ranges};
}

void pca_reg_neighborhood_tensor(
    int64_t num_splats, int mode, int k, int window, DeviceVector<float3> means,
    DeviceVector<float4> quats, DeviceVector<float3> scales, DeviceVector<float> opacities,
    int covariance, DeviceVector<int32_t> perm,
    DeviceVector<float4> sorted, DeviceVector<float4> nodes, DeviceVector<int4> meta,
    DeviceVector<int32_t> ranges, DeviceVector<int32_t> nbrs, bool store_list,
    DeviceVector<float> radii, bool gate_means, bool scale_agnostic, DeviceVector<float3> v_means,
    DeviceVector<float4> v_quats, DeviceVector<float3> v_scales, DeviceVector<float> v_opacities, float w_plane,
    float w_tangent, float w_normal, float w_surfel, float w_erank, float w_transparency, DeviceVector<float> loss_sums) {
    if (num_splats <= 0 || k <= 0) return;
    PcaOwnerParams p{};
    p.means = (uint64_t)means.data_ptr();
    p.quats = (uint64_t)quats.data_ptr();
    p.scales = (uint64_t)scales.data_ptr();
    p.opacities = vkk::or_fallback(opacities.data_ptr());
    p.v_opacities = vkk::or_fallback(v_opacities.data_ptr());
    p.perm = (uint64_t)perm.data_ptr();
    p.sorted = vkk::or_fallback(sorted.data_ptr());
    p.nodes = vkk::or_fallback(nodes.data_ptr());
    p.meta = vkk::or_fallback(meta.data_ptr());
    p.ranges = vkk::or_fallback(ranges.data_ptr());
    p.nbrs = vkk::or_fallback(nbrs.data_ptr());
    p.radii = vkk::or_fallback(radii.data_ptr());
    p.gate_means = gate_means && radii.data_ptr() ? 1u : 0u;
    p.v_means = (uint64_t)v_means.data_ptr();
    p.v_quats = vkk::or_fallback(v_quats.data_ptr());
    p.v_scales = vkk::or_fallback(v_scales.data_ptr());
    p.loss_sums = (uint64_t)loss_sums.data_ptr();
    p.n = (uint32_t)num_splats;
    p.mode = (uint32_t)mode;
    p.k = (uint32_t)k;
    p.window = (uint32_t)window;
    p.store_list = store_list ? 1u : 0u;
    p.w_plane = w_plane;
    p.w_tangent = w_tangent;
    p.w_normal = w_normal;
    p.w_surfel = w_surfel;
    p.w_erank = w_erank;
    p.w_transparency = w_transparency;
    const vkk::Fold f = vkk::fold_1d(num_splats, kOwnerGroup);
    p.wgs_per_row = f.per_row;
    const int cap = pca_reg_capacity(k);
    static const char* const kEntries[2][3] = {
        {"pca_reg.pca_owner_k8", "pca_reg.pca_owner_k16", "pca_reg.pca_owner_k32"},
        {"pca_reg.pca_owner_k8_sa", "pca_reg.pca_owner_k16_sa", "pca_reg.pca_owner_k32_sa"}};
    const char* entry = kEntries[scale_agnostic ? 1 : 0][cap == 8 ? 0 : (cap == 16 ? 1 : 2)];
    const int terms = (int)pca_reg_terms(w_normal, w_surfel, w_erank, w_transparency) |
                      ((w_plane != 0 || w_tangent != 0) ? 16 : 0);
    vkk::dispatch_ring_budgeted(entry, {covariance, terms, mode}, f.per_row, f.rows, 1,
                                &p, sizeof(p), (double)num_splats * (double)(k + 1), nullptr);
}
