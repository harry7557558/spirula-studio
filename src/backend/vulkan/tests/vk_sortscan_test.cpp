// Parity tests for the Vulkan SortScan implementation against CPU
// references. Covers: 64-bit and 32-bit key radix sort (stability verified
// via std::stable_sort on the masked key), begin_bit/end_bit sub-ranges
// including non-multiples of 8 (the tile-intersection pattern
// [0, 32 + tile_n_bits)), inclusive/exclusive scans (int32/int64), and
// select_flagged.

#include "backend/common/SortScan.h"
#include "core/SourcePath.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <random>
#include <vector>

static int g_failures = 0;

#define CHECK_MSG(cond, ...)                                              \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::printf("FAIL %s:%d: ", SS_FILE, __LINE__);               \
            std::printf(__VA_ARGS__);                                     \
            std::printf("\n");                                            \
            g_failures++;                                                 \
        }                                                                 \
    } while (0)

#define CHECK_NO_ERROR()                                                  \
    do {                                                                  \
        const char* err_ = backend::last_error();                         \
        if (err_) {                                                       \
            std::printf("FAIL %s:%d: backend error: %s\n", SS_FILE,       \
                        __LINE__, err_);                                  \
            g_failures++;                                                 \
        }                                                                 \
    } while (0)

using backend::MemcpyKind;

template <typename T>
T* dev_upload(const std::vector<T>& host) {
    T* d = (T*)backend::device_malloc(host.size() * sizeof(T));
    backend::memcpy_sync(d, host.data(), host.size() * sizeof(T),
                         MemcpyKind::HostToDevice);
    return d;
}

template <typename T>
std::vector<T> dev_download(const T* d, size_t n) {
    std::vector<T> host(n);
    backend::memcpy_sync(host.data(), d, n * sizeof(T),
                         MemcpyKind::DeviceToHost);
    return host;
}

template <typename KeyT>
void test_sort(int64_t n, int begin_bit, int end_bit, uint64_t key_range,
               std::mt19937_64& rng) {
    std::vector<KeyT> keys(n);
    std::vector<int32_t> vals(n);
    for (int64_t i = 0; i < n; i++) {
        keys[i] = (KeyT)(rng() % key_range);
        vals[i] = (int32_t)i;
    }

    // CPU reference: stable sort of indices by the masked key.
    auto masked = [&](KeyT k) -> uint64_t {
        uint64_t u = (uint64_t)(typename std::make_unsigned<KeyT>::type)k;
        uint64_t width = (uint64_t)(end_bit - begin_bit);
        uint64_t mask =
            width >= 64 ? ~0ull : ((1ull << width) - 1ull);
        return (u >> begin_bit) & mask;
    };
    std::vector<int32_t> ref_order(n);
    std::iota(ref_order.begin(), ref_order.end(), 0);
    std::stable_sort(ref_order.begin(), ref_order.end(),
                     [&](int32_t a, int32_t b) {
                         return masked(keys[a]) < masked(keys[b]);
                     });

    KeyT* d_keys_a = dev_upload(keys);
    KeyT* d_keys_b = (KeyT*)backend::device_malloc(n * sizeof(KeyT));
    int32_t* d_vals_a = dev_upload(vals);
    int32_t* d_vals_b = (int32_t*)backend::device_malloc(n * sizeof(int32_t));

    backend::DoubleBuffer<KeyT> kb(d_keys_a, d_keys_b);
    backend::DoubleBuffer<int32_t> vb(d_vals_a, d_vals_b);
    backend::sort_pairs(kb, vb, n, begin_bit, end_bit);
    backend::device_synchronize();
    CHECK_NO_ERROR();

    auto got_keys = dev_download(kb.current(), n);
    auto got_vals = dev_download(vb.current(), n);
    int bad = 0;
    for (int64_t i = 0; i < n; i++) {
        int32_t ri = ref_order[i];
        if ((got_vals[i] != ri || got_keys[i] != keys[ri]) && ++bad <= 3)
            std::printf("  sort mismatch n=%lld bits=[%d,%d) @%lld: "
                        "got (v=%d) want (v=%d)\n",
                        (long long)n, begin_bit, end_bit, (long long)i,
                        got_vals[i], ri);
    }
    CHECK_MSG(bad == 0, "sort<%zu-bit> n=%lld bits=[%d,%d): %d mismatches",
              sizeof(KeyT) * 8, (long long)n, begin_bit, end_bit, bad);

    backend::device_free(d_keys_a);
    backend::device_free(d_keys_b);
    backend::device_free(d_vals_a);
    backend::device_free(d_vals_b);
}

template <typename T>
void test_scan(int64_t n, bool inclusive, std::mt19937_64& rng) {
    std::vector<T> in(n);
    for (int64_t i = 0; i < n; i++) in[i] = (T)(rng() % 7);
    std::vector<T> ref(n);
    T acc = 0;
    for (int64_t i = 0; i < n; i++) {
        if (inclusive) {
            acc += in[i];
            ref[i] = acc;
        } else {
            ref[i] = acc;
            acc += in[i];
        }
    }
    T* d_in = dev_upload(in);
    T* d_out = (T*)backend::device_malloc(n * sizeof(T));
    if (inclusive)
        backend::inclusive_sum(d_in, d_out, n);
    else
        backend::exclusive_sum(d_in, d_out, n);
    backend::device_synchronize();
    CHECK_NO_ERROR();
    auto got = dev_download(d_out, n);
    int bad = 0;
    for (int64_t i = 0; i < n; i++)
        if (got[i] != ref[i] && ++bad <= 3)
            std::printf("  scan mismatch n=%lld incl=%d @%lld: got %lld "
                        "want %lld\n",
                        (long long)n, (int)inclusive, (long long)i,
                        (long long)got[i], (long long)ref[i]);
    CHECK_MSG(bad == 0, "scan<%zu-bit> n=%lld incl=%d: %d mismatches",
              sizeof(T) * 8, (long long)n, (int)inclusive, bad);
    backend::device_free(d_in);
    backend::device_free(d_out);
}

void test_select(int64_t n, std::mt19937_64& rng) {
    std::vector<int32_t> in(n);
    std::vector<uint8_t> flags(n);
    for (int64_t i = 0; i < n; i++) {
        in[i] = (int32_t)i * 3;
        flags[i] = (rng() % 3) == 0 ? 1 : 0;
    }
    std::vector<int32_t> ref;
    for (int64_t i = 0; i < n; i++)
        if (flags[i]) ref.push_back(in[i]);

    int32_t* d_in = dev_upload(in);
    uint8_t* d_flags = dev_upload(flags);
    int32_t* d_out = (int32_t*)backend::device_malloc(n * sizeof(int32_t));
    int64_t count = backend::select_flagged(d_in, d_flags, d_out, n);
    CHECK_NO_ERROR();
    CHECK_MSG(count == (int64_t)ref.size(),
              "select n=%lld: count %lld want %zu", (long long)n,
              (long long)count, ref.size());
    if (count == (int64_t)ref.size() && count > 0) {
        auto got = dev_download(d_out, count);
        int bad = 0;
        for (int64_t i = 0; i < count; i++)
            if (got[i] != ref[i]) bad++;
        CHECK_MSG(bad == 0, "select n=%lld: %d mismatches", (long long)n,
                  bad);
    }
    backend::device_free(d_in);
    backend::device_free(d_flags);
    backend::device_free(d_out);
}

int main() {
    std::mt19937_64 rng(20260716);

    const int64_t sizes[] = {1, 100, 2048, 4097, 1 << 16, (1 << 20) + 123};

    // 32-bit keys, full range.
    for (int64_t n : sizes) test_sort<int32_t>(n, 0, 32, 1ull << 31, rng);
    // 64-bit keys, full width.
    for (int64_t n : sizes) test_sort<int64_t>(n, 0, 64, ~0ull, rng);
    // Tile-intersection pattern: key = (tile_id << 32) | depth, sorted on
    // [0, 32 + tile_n_bits) with a non-multiple-of-8 end bit.
    for (int64_t n : sizes) {
        test_sort<int64_t>(n, 0, 32 + 12, 1ull << (32 + 12), rng);
        test_sort<int64_t>(n, 0, 32 + 17, 1ull << (32 + 17), rng);
    }
    // Sub-range with duplicate-heavy keys (stability stress): only 8
    // distinct masked values.
    for (int64_t n : sizes) test_sort<int64_t>(n, 8, 11, 1ull << 20, rng);
    // begin_bit > 0, 32-bit.
    for (int64_t n : sizes) test_sort<int32_t>(n, 4, 20, 1ull << 31, rng);

    for (int64_t n : sizes) {
        test_scan<int32_t>(n, true, rng);
        test_scan<int32_t>(n, false, rng);
        test_scan<int64_t>(n, true, rng);
        test_scan<int64_t>(n, false, rng);
    }

    for (int64_t n : sizes) test_select(n, rng);

    // Past the 65535 grid-fold cap (SCAN_BLOCK = 2048): the fold tail
    // launches whole workgroups with no block_sums slot, and an
    // unguarded write there wedges the GPU.
    test_scan<int32_t>(65536LL * 2048 + 1, true, rng);
    test_scan<int32_t>(65536LL * 2048 + 1, false, rng);

    // Empty inputs are no-ops.
    backend::DoubleBuffer<int64_t> kb(nullptr, nullptr);
    backend::DoubleBuffer<int32_t> vb(nullptr, nullptr);
    backend::sort_pairs(kb, vb, 0, 0, 64);
    backend::inclusive_sum<int32_t>(nullptr, nullptr, 0);
    CHECK_NO_ERROR();

    std::printf(g_failures == 0 ? "vk_sortscan_test: ALL PASSED\n"
                                : "vk_sortscan_test: %d FAILURES\n",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}
