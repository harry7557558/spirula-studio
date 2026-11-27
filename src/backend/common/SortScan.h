#pragma once
// Device sort / scan / stream-compaction primitives shared by backend
// implementations. This sits BELOW the launch API: the engine never calls it
// directly — kernel-launch implementations (tile intersection, densify,
// visualizer, meshing, packed projection) do, replacing their direct
// cub::Device* calls so both backends share one implementation per primitive.
//
// CUDA backend: thin wrappers over CUB (cub::DeviceRadixSort::SortPairs,
// cub::DeviceScan::Inclusive/ExclusiveSum, cub::DeviceSelect::Flagged),
// preserving the current CUB_WRAPPER temp-storage handling.
// Vulkan backend: onesweep radix sort (vendored GLSL, MIT, jaesung-cs — same
// one used in VkSplat) + multi-phase prefix-sum compute kernels.
//
// Instantiations required by current call sites (audited 2026-07):
//   sort_pairs<int64_t, int32_t>   — tile intersection keys. DECIDED: the
//                                    Vulkan sort must support 64-bit keys
//                                    (32-bit packing is only safe up to
//                                    Mip-NeRF-360-sized scenes; large-scale
//                                    real-world datasets overflow it). The
//                                    vendored onesweep sort is 32-bit-key —
//                                    extend it to 64-bit (wide-key passes or
//                                    hi/lo double-sort), exploiting
//                                    begin_bit/end_bit to skip unused passes.
//   sort_pairs<int32_t, int32_t>   — densify / packed projection / visualizer
//                                    (densify weighted sampling sorts float
//                                    keys through the order-preserving
//                                    uint32 flip transform on this overload)
//   inclusive_sum / exclusive_sum  — int32 and int64 counters
//   select_flagged                 — int32 ids
// Callers pass begin_bit/end_bit exactly as with CUB to skip radix passes.

#include <cstdint>

#include "backend/api/BackendRuntime.h"

namespace backend {

// Mirrors cub::DoubleBuffer: two ping-pong device buffers; after a sort,
// selector() tells which one holds the results.
template <typename T>
struct DoubleBuffer {
    T* buffers[2];
    int selector = 0;

    DoubleBuffer(T* current, T* alternate) : buffers{current, alternate} {}
    T* current() const { return buffers[selector]; }
    T* alternate() const { return buffers[selector ^ 1]; }
};

template <typename KeyT, typename ValueT>
void sort_pairs(DoubleBuffer<KeyT>& keys, DoubleBuffer<ValueT>& values,
                int64_t num_items, int begin_bit, int end_bit,
                Stream stream = kDefaultStream);

// in may equal out, as with CUB.
template <typename T>
void inclusive_sum(const T* in, T* out, int64_t num_items,
                   Stream stream = kDefaultStream);

template <typename T>
void exclusive_sum(const T* in, T* out, int64_t num_items,
                   Stream stream = kDefaultStream);

// Copies in[i] where flags[i] != 0 into out (stable); returns the number of
// selected items (synchronizes on the internal count readback, as the CUB
// call sites do today).
template <typename T>
int64_t select_flagged(const T* in, const uint8_t* flags, T* out,
                       int64_t num_items, Stream stream = kDefaultStream);

}  // namespace backend
