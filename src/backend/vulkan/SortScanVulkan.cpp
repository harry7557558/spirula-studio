// Vulkan implementation of backend/common/SortScan.h over the kernels in
// shaders/sort_scan.slang (embedded SPIR-V). Design notes in README.md.
//
// Scratch is a global grow-only pool (mirroring DeviceScratch's single-
// stream assumption): concurrent sort/scan calls on different streams are
// not supported, matching how the engine uses these primitives today.

#include "backend/common/SortScan.h"
#include "backend/vulkan/VulkanPipelines.h"

#include <algorithm>
#include <cstring>
#include <mutex>

namespace backend {

namespace {

using vk::SpecList;

// Must mirror shaders/sort_scan.slang.
constexpr uint32_t kSortPart = 2048;
constexpr uint32_t kRadix = 256;
constexpr uint32_t kScanBlock = 2048;
constexpr uint32_t kMinSubgroup = 8;  // MAX_NSG bound in the shader

struct GrowBuffer {
    void* ptr = nullptr;
    size_t cap = 0;
    void* acquire(size_t bytes) {
        if (bytes > cap) {
            if (ptr) device_free(ptr);  // syncs in-flight work per contract
            ptr = device_malloc(bytes);
            cap = ptr ? bytes : 0;
        }
        return ptr;
    }
};

std::mutex g_scratch_mutex;
GrowBuffer g_part_hist;
GrowBuffer g_digit_total;
GrowBuffer g_global_base;
GrowBuffer g_block_sums;
GrowBuffer g_select_scan;

struct Grid {
    uint32_t x, y, per_row;
};

Grid fold_grid(uint32_t groups) {
    uint32_t per_row = std::min(groups, 65535u);
    return {per_row, (groups + per_row - 1) / per_row, per_row};
}

// elem_at() in the shaders builds a 64-bit byte offset, but only where
// gindex_t is 64-bit; the ".noint64" blobs index in 32 bits and so cannot
// reach past 4 GiB of any one buffer.
bool span_ok(int64_t num_items, size_t elem_bytes, const char* what) {
    if (vk::Context::get().caps().shader_int64) return true;
    if ((uint64_t)num_items * elem_bytes <= 0xffffffffull) return true;
    vk::set_error(what, VK_SUCCESS);
    return false;
}

bool subgroup_ok() {
    const auto& caps = vk::Context::get().caps();
    if (caps.subgroup_size < kMinSubgroup) {
        vk::set_error("SortScan: device subgroup size < 8 is unsupported",
                      VK_SUCCESS);
        return false;
    }
    return true;
}

// --- radix sort -------------------------------------------------------------

struct HistParams {
    uint64_t keys, part_hist;
    uint32_t n, shift, digit_mask, num_parts, parts_per_row;
};
struct SpineLocalParams {
    uint64_t part_hist, digit_total;
    uint32_t num_parts;
};
struct SpineGlobalParams {
    uint64_t digit_total, global_base;
};
struct DownsweepParams {
    uint64_t keys_in, keys_out, vals_in, vals_out, part_offsets, global_base;
    uint32_t n, shift, digit_mask, num_parts, parts_per_row;
};

template <typename KeyT>
void sort_pairs_impl(DoubleBuffer<KeyT>& keys, DoubleBuffer<int32_t>& values,
                     int64_t num_items, int begin_bit, int end_bit,
                     Stream stream) {
    static_assert(sizeof(KeyT) == 4 || sizeof(KeyT) == 8, "key width");
    if (num_items <= 0) return;
    if (num_items > INT32_MAX) {
        vk::set_error("sort_pairs: > 2^31 items unsupported", VK_SUCCESS);
        return;
    }
    if (!span_ok(num_items, sizeof(KeyT),
                 "sort_pairs: keys over 4 GiB need shaderInt64") ||
        !span_ok(num_items, sizeof(int32_t),
                 "sort_pairs: values over 4 GiB need shaderInt64"))
        return;
    if (!subgroup_ok()) return;
    begin_bit = std::max(begin_bit, 0);
    end_bit = std::min(end_bit, (int)sizeof(KeyT) * 8);
    if (begin_bit >= end_bit) return;

    const char* hist_entry = sizeof(KeyT) == 8 ? "sort_scan.sort_hist_u64"
                                               : "sort_scan.sort_hist_u32";
    const char* down_entry = sizeof(KeyT) == 8
                                 ? "sort_scan.sort_downsweep_u64"
                                 : "sort_scan.sort_downsweep_u32";

    uint32_t n = (uint32_t)num_items;
    uint32_t num_parts = (n + kSortPart - 1) / kSortPart;
    Grid grid = fold_grid(num_parts);

    std::lock_guard<std::mutex> lock(g_scratch_mutex);
    uint64_t part_hist =
        (uint64_t)g_part_hist.acquire((size_t)num_parts * kRadix * 4);
    uint64_t digit_total = (uint64_t)g_digit_total.acquire(kRadix * 4);
    uint64_t global_base = (uint64_t)g_global_base.acquire(kRadix * 4);
    if (!part_hist || !digit_total || !global_base) return;

    for (int b = begin_bit; b < end_bit; b += 8) {
        uint32_t width = (uint32_t)std::min(8, end_bit - b);
        uint32_t mask = (1u << width) - 1u;

        HistParams hp{(uint64_t)keys.current(), part_hist,
                      n, (uint32_t)b, mask, num_parts, grid.per_row};
        if (!vk::dispatch(stream, hist_entry, SpecList{}, grid.x, grid.y, 1,
                          &hp, sizeof(hp)))
            return;
        SpineLocalParams slp{part_hist, digit_total, num_parts};
        if (!vk::dispatch(stream, "sort_scan.sort_spine_local", SpecList{},
                          kRadix, 1, 1, &slp, sizeof(slp)))
            return;
        SpineGlobalParams sgp{digit_total, global_base};
        if (!vk::dispatch(stream, "sort_scan.sort_spine_global", SpecList{},
                          1, 1, 1, &sgp, sizeof(sgp)))
            return;
        DownsweepParams dp{(uint64_t)keys.current(),
                           (uint64_t)keys.alternate(),
                           (uint64_t)values.current(),
                           (uint64_t)values.alternate(),
                           part_hist, global_base,
                           n, (uint32_t)b, mask, num_parts, grid.per_row};
        if (!vk::dispatch(stream, down_entry, SpecList{}, grid.x, grid.y, 1,
                          &dp, sizeof(dp)))
            return;

        keys.selector ^= 1;
        values.selector ^= 1;
    }
}

// --- scans ------------------------------------------------------------------

struct ScanParams {
    uint64_t input, output, block_sums;
    uint32_t n, blocks_per_row;
};
struct ScanSpineParams {
    uint64_t block_sums;
    uint32_t num_blocks;
};
struct ScanAddParams {
    uint64_t output, block_sums;
    uint32_t n, blocks_per_row;
};

// blocks_entry reads `input` elements (int32/int64/uint8 flags) and writes
// `elem_bytes`-wide partial scans to `output`.
void scan_impl(const void* in, void* out, int64_t num_items, bool inclusive,
               const char* blocks_entry, const char* spine_entry,
               const char* add_entry, size_t elem_bytes, Stream stream) {
    if (num_items <= 0) return;
    if (num_items > INT32_MAX) {
        vk::set_error("scan: > 2^31 items unsupported", VK_SUCCESS);
        return;
    }
    if (!span_ok(num_items, elem_bytes,
                 "scan: buffer over 4 GiB needs shaderInt64"))
        return;
    uint32_t n = (uint32_t)num_items;
    uint32_t blocks = (n + kScanBlock - 1) / kScanBlock;
    Grid grid = fold_grid(blocks);

    std::lock_guard<std::mutex> lock(g_scratch_mutex);
    uint64_t sums = (uint64_t)g_block_sums.acquire(blocks * elem_bytes);
    if (!sums) return;

    SpecList spec{inclusive ? 1u : 0u};
    ScanParams sp{(uint64_t)in, (uint64_t)out, sums, n, grid.per_row};
    if (!vk::dispatch(stream, blocks_entry, spec, grid.x, grid.y, 1, &sp,
                      sizeof(sp)))
        return;
    if (blocks > 1) {
        ScanSpineParams ssp{sums, blocks};
        if (!vk::dispatch(stream, spine_entry, SpecList{}, 1, 1, 1, &ssp,
                          sizeof(ssp)))
            return;
        ScanAddParams sap{(uint64_t)out, sums, n, grid.per_row};
        vk::dispatch(stream, add_entry, SpecList{}, grid.x, grid.y, 1, &sap,
                     sizeof(sap));
    }
}

template <typename T>
const char* scan_blocks_entry() {
    return sizeof(T) == 8 ? "sort_scan.scan_blocks_i64"
                          : "sort_scan.scan_blocks_i32";
}
template <>
const char* scan_blocks_entry<float>() {
    return "sort_scan.scan_blocks_f32";
}
template <typename T>
const char* scan_spine_entry() {
    return sizeof(T) == 8 ? "sort_scan.scan_spine_i64"
                          : "sort_scan.scan_spine_i32";
}
template <>
const char* scan_spine_entry<float>() {
    return "sort_scan.scan_spine_f32";
}
template <typename T>
const char* scan_add_entry() {
    return sizeof(T) == 8 ? "sort_scan.scan_add_i64"
                          : "sort_scan.scan_add_i32";
}
template <>
const char* scan_add_entry<float>() {
    return "sort_scan.scan_add_f32";
}

// --- select ------------------------------------------------------------------

struct ScatterFlaggedParams {
    uint64_t input, flags, scan, output;
    uint32_t n, blocks_per_row;
};

}  // namespace

// --- public API (backend/common/SortScan.h) ---------------------------------

template <typename KeyT, typename ValueT>
void sort_pairs(DoubleBuffer<KeyT>& keys, DoubleBuffer<ValueT>& values,
                int64_t num_items, int begin_bit, int end_bit,
                Stream stream) {
    static_assert(sizeof(ValueT) == 4, "sort_pairs: 4-byte values only");
    sort_pairs_impl(keys, reinterpret_cast<DoubleBuffer<int32_t>&>(values),
                    num_items, begin_bit, end_bit, stream);
}

template <typename T>
void inclusive_sum(const T* in, T* out, int64_t num_items, Stream stream) {
    static_assert(sizeof(T) == 4 || sizeof(T) == 8, "element width");
    scan_impl(in, out, num_items, /*inclusive=*/true, scan_blocks_entry<T>(),
              scan_spine_entry<T>(), scan_add_entry<T>(), sizeof(T), stream);
}

template <typename T>
void exclusive_sum(const T* in, T* out, int64_t num_items, Stream stream) {
    static_assert(sizeof(T) == 4 || sizeof(T) == 8, "element width");
    scan_impl(in, out, num_items, /*inclusive=*/false, scan_blocks_entry<T>(),
              scan_spine_entry<T>(), scan_add_entry<T>(), sizeof(T), stream);
}

template <typename T>
int64_t select_flagged(const T* in, const uint8_t* flags, T* out,
                       int64_t num_items, Stream stream) {
    static_assert(sizeof(T) == 4, "select_flagged: 4-byte elements only");
    if (num_items <= 0) return 0;
    if (num_items > INT32_MAX) {
        vk::set_error("select_flagged: > 2^31 items unsupported", VK_SUCCESS);
        return 0;
    }
    if (!span_ok(num_items, sizeof(T),
                 "select_flagged: buffer over 4 GiB needs shaderInt64"))
        return 0;
    uint32_t n = (uint32_t)num_items;
    uint32_t blocks = (n + kScanBlock - 1) / kScanBlock;
    Grid grid = fold_grid(blocks);

    void* scan_buf;
    {
        std::lock_guard<std::mutex> lock(g_scratch_mutex);
        scan_buf = g_select_scan.acquire((size_t)n * 4);
    }
    if (!scan_buf) return 0;

    // Exclusive scan of the flags into scan_buf (int32).
    scan_impl(flags, scan_buf, num_items, /*inclusive=*/false,
              "sort_scan.scan_blocks_flags", scan_spine_entry<int32_t>(),
              scan_add_entry<int32_t>(), 4, stream);

    ScatterFlaggedParams sp{(uint64_t)in, (uint64_t)flags,
                            (uint64_t)scan_buf, (uint64_t)out, n,
                            grid.per_row};
    if (!vk::dispatch(stream, "sort_scan.scatter_flagged_u32", SpecList{},
                      grid.x, grid.y, 1, &sp, sizeof(sp)))
        return 0;

    // Count = last exclusive prefix + last flag (synchronizes, as the CUB
    // call sites do today — documented in SortScan.h).
    int32_t last_scan = 0;
    uint8_t last_flag = 0;
    memcpy_sync(&last_scan, (const char*)scan_buf + (size_t)(n - 1) * 4, 4,
                MemcpyKind::DeviceToHost);
    memcpy_sync(&last_flag, flags + (n - 1), 1, MemcpyKind::DeviceToHost);
    return (int64_t)last_scan + (last_flag ? 1 : 0);
}

// Instantiations audited in SortScan.h.
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
template void inclusive_sum<float>(const float*, float*, int64_t, Stream);
template void exclusive_sum<int32_t>(const int32_t*, int32_t*, int64_t,
                                     Stream);
template void exclusive_sum<int64_t>(const int64_t*, int64_t*, int64_t,
                                     Stream);
template int64_t select_flagged<int32_t>(const int32_t*, const uint8_t*,
                                         int32_t*, int64_t, Stream);

}  // namespace backend
