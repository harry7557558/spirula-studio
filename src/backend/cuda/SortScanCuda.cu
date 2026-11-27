// CUDA implementation of the shared sort / scan / compaction primitives
// declared in backend/common/SortScan.h -- thin wrappers over CUB, with the
// codebase's usual CUB_WRAPPER temp-storage handling (one growing global
// scratch buffer rather than an allocation per call).
//
// These exist so code ABOVE the backend seam can sort without naming CUB:
// the meshing pipeline's LBVH builds are host-side orchestration compiled once
// for both backends (mesh/OccupancyEvaluator.cpp, mesh/MeshingRasterHost.cpp),
// and they reach the device sort through here on CUDA and through
// backend/vulkan/SortScanVulkan.cpp on Vulkan. Kernel launchers that are
// already CUDA-only (IntersectTile.cu, DensifySampling.cu, ...) keep calling
// CUB directly; nothing is duplicated, because this file IS the CUB call.

#include "backend/common/SortScan.h"

#include <cub/cub.cuh>

#include <core/Common.cuh>
#include <core/Tensor.h>

namespace backend {

template <typename KeyT, typename ValueT>
void sort_pairs(DoubleBuffer<KeyT>& keys, DoubleBuffer<ValueT>& values,
                int64_t num_items, int begin_bit, int end_bit, Stream stream) {
    if (num_items <= 0) return;
    cub::DoubleBuffer<KeyT> k(keys.current(), keys.alternate());
    cub::DoubleBuffer<ValueT> v(values.current(), values.alternate());
    CUB_WRAPPER(cub::DeviceRadixSort::SortPairs, k, v, num_items, begin_bit,
                end_bit, (cudaStream_t)stream);
    // CUB's selector is relative to the buffers we handed it, which are
    // already in the caller's current/alternate order.
    keys.selector ^= k.selector;
    values.selector ^= v.selector;
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

template <typename T>
void inclusive_sum(const T* in, T* out, int64_t num_items, Stream stream) {
    if (num_items <= 0) return;
    CUB_WRAPPER(cub::DeviceScan::InclusiveSum, in, out, num_items,
                (cudaStream_t)stream);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

template <typename T>
void exclusive_sum(const T* in, T* out, int64_t num_items, Stream stream) {
    if (num_items <= 0) return;
    CUB_WRAPPER(cub::DeviceScan::ExclusiveSum, in, out, num_items,
                (cudaStream_t)stream);
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

template <typename T>
int64_t select_flagged(const T* in, const uint8_t* flags, T* out,
                       int64_t num_items, Stream stream) {
    if (num_items <= 0) return 0;
    // DeviceScratch hands back ONE buffer and CUB's temp storage already owns
    // it, so the selected-count word gets its own (permanent, 4-byte)
    // allocation rather than a pool slot.
    static int32_t* d_count =
        (int32_t*)device_malloc_checked(sizeof(int32_t), "select_flagged");
    CUB_WRAPPER(cub::DeviceSelect::Flagged, in, flags, out, d_count, num_items,
                (cudaStream_t)stream);
    int32_t h_count = 0;
    memcpy_sync(&h_count, d_count, sizeof(int32_t), MemcpyKind::DeviceToHost);
    CHECK_DEVICE_ERROR(cudaGetLastError());
    return (int64_t)h_count;
}

// Instantiations required by current call sites (see SortScan.h).
template void sort_pairs<int64_t, int32_t>(DoubleBuffer<int64_t>&,
                                           DoubleBuffer<int32_t>&, int64_t,
                                           int, int, Stream);
template void sort_pairs<int32_t, int32_t>(DoubleBuffer<int32_t>&,
                                           DoubleBuffer<int32_t>&, int64_t,
                                           int, int, Stream);
template void inclusive_sum<int32_t>(const int32_t*, int32_t*, int64_t,
                                     Stream);
template void inclusive_sum<int64_t>(const int64_t*, int64_t*, int64_t,
                                     Stream);
template void exclusive_sum<int32_t>(const int32_t*, int32_t*, int64_t,
                                     Stream);
template void exclusive_sum<int64_t>(const int64_t*, int64_t*, int64_t,
                                     Stream);
template int64_t select_flagged<int32_t>(const int32_t*, const uint8_t*,
                                         int32_t*, int64_t, Stream);

}  // namespace backend
