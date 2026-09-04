#include "nn/Tensor.h"

#include "nn/core/Error.h"
#include "nn/core/Half.h"
#include "nn/core/Log.h"
#include "nn/vk/Context.h"
#include "nn/vk/Stream.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace nn {

namespace {
int infer_ndim(int ndim, int64_t d1, int64_t d2, int64_t d3) {
    if (ndim > 0) return ndim;
    if (d3 != 1) return 4;
    if (d2 != 1) return 3;
    if (d1 != 1) return 2;
    return 1;
}

// Measured on NVIDIA: writing element 2^30 of a 4 GiB f32 buffer lands at
// element 0, and widening the shader's index to 64 bits does not change it.
int64_t g_max_span = 1ll << 32;
WideIndex g_wide = WideIndex::Auto;

}  // namespace

int64_t max_span_bytes() { return g_max_span; }

void set_max_span_bytes(int64_t bytes) { g_max_span = bytes > 0 ? bytes : (1ll << 32); }

int64_t span_rows(const char* op, int64_t row_bytes) {
    if (row_bytes <= 0) return g_max_span;
    NN_CHECK(row_bytes <= g_max_span,
             "%s: one row spans %lld bytes, past the %lld a kernel can index from one "
             "pointer", op, (long long)row_bytes, (long long)g_max_span);
    return g_max_span / row_bytes;
}

int64_t span_rows_even(const char* op, int64_t row_bytes) {
    return std::max<int64_t>(2, span_rows(op, row_bytes) & ~1ll);
}

void check_span(const char* op, std::initializer_list<Tensor> ts) {
    for (const Tensor& t : ts)
        NN_CHECK(!t.valid() || t.bytes() <= g_max_span,
                 "%s: a [%lld, %lld] %s tensor spans %lld bytes, past the %lld a kernel "
                 "can index from one pointer",
                 op, (long long)t.rows(), (long long)t.cols(), dtype_name(t.dtype),
                 (long long)t.bytes(), (long long)g_max_span);
}

void set_wide_index(WideIndex mode) { g_wide = mode; }

int64_t span_reach(DType dtype) {
    if (g_wide == WideIndex::Off || !vk::Context::get().hasInt64()) return g_max_span;
    return std::max<int64_t>(g_max_span, (1ll << 32) * dtype_size(dtype));
}

KernelName span_entry(const char* entry, std::initializer_list<Tensor> ts) {
    KernelName out{};
    const size_t len = std::strlen(entry);
    NN_CHECK(len + 5 < sizeof(out.text), "entry name '%s' is too long", entry);
    int64_t bytes = 0, elems = 0;
    for (const Tensor& t : ts)
        if (t.valid()) {
            bytes = std::max(bytes, t.bytes());
            elems = std::max(elems, t.numel());
        }
    if (bytes <= g_max_span && g_wide != WideIndex::Force) {
        std::memcpy(out.text, entry, len + 1);
        return out;
    }

    NN_CHECK(g_wide != WideIndex::Off && vk::Context::get().hasInt64(),
             "%s: a %lld-byte tensor needs the 64-bit addressing kernel, which this "
             "device does not offer (no shaderInt64)", entry, (long long)bytes);
    // The wide kernel still indexes with a uint, which is the next wall.
    NN_CHECK(elems <= (1ll << 32),
             "%s: a tensor of %lld elements is past the 2^32 a kernel can index",
             entry, (long long)elems);

    const char* dot = std::strchr(entry, '.');
    NN_CHECK(dot != nullptr, "%s: entry name has no module prefix", entry);
    std::snprintf(out.text, sizeof(out.text), "%.*s_wide%s", (int)(dot - entry), entry,
                  dot);
    return out;
}

Tensor arena_tensor(vk::Arena& a, DType t, int64_t d0, int64_t d1, int64_t d2, int64_t d3,
                    int ndim) {
    Tensor out;
    out.dtype = t;
    out.ndim = infer_ndim(ndim, d1, d2, d3);
    out.shape[0] = d0; out.shape[1] = d1; out.shape[2] = d2; out.shape[3] = d3;
    out.ptr = a.alloc((vk::DevicePtr)out.numel() * dtype_size(t));
    return out;
}

Tensor pool_tensor(vk::PoolSlot slot, uint32_t sub, DType t, int64_t d0, int64_t d1,
                   int64_t d2, int64_t d3, int ndim) {
    Tensor out;
    out.dtype = t;
    out.ndim = infer_ndim(ndim, d1, d2, d3);
    out.shape[0] = d0; out.shape[1] = d1; out.shape[2] = d2; out.shape[3] = d3;
    out.ptr = vk::VramPool::get().acquire(slot, sub, (VkDeviceSize)out.bytes());
    return out;
}

void tensor_to_host(const Tensor& t, float* dst, int64_t count) {
    NN_CHECK(t.valid(), "tensor_to_host on an empty tensor");
    NN_CHECK(count <= t.numel(), "tensor_to_host: %lld > %lld elements",
               (long long)count, (long long)t.numel());
    if (t.dtype == DType::F32) {
        vk::Stream::get().download(dst, t.ptr, (VkDeviceSize)count * 4);
    } else if (t.dtype == DType::F16) {
        std::vector<uint16_t> tmp((size_t)count);
        vk::Stream::get().download(tmp.data(), t.ptr, (VkDeviceSize)count * 2);
        for (int64_t i = 0; i < count; ++i) dst[i] = half_to_float(tmp[(size_t)i]);
    } else if (t.dtype == DType::I32) {
        std::vector<int32_t> tmp((size_t)count);
        vk::Stream::get().download(tmp.data(), t.ptr, (VkDeviceSize)count * 4);
        for (int64_t i = 0; i < count; ++i) dst[i] = (float)tmp[(size_t)i];
    } else {
        std::vector<uint8_t> tmp((size_t)count);
        vk::Stream::get().download(tmp.data(), t.ptr, (VkDeviceSize)count);
        for (int64_t i = 0; i < count; ++i) dst[i] = (float)tmp[(size_t)i];
    }
}

void tensor_from_host(const Tensor& t, const float* src, int64_t count) {
    NN_CHECK(t.valid(), "tensor_from_host on an empty tensor");
    NN_CHECK(count <= t.numel(), "tensor_from_host: %lld > %lld elements",
               (long long)count, (long long)t.numel());
    if (t.dtype == DType::F32) {
        vk::Stream::get().upload(t.ptr, src, (VkDeviceSize)count * 4);
    } else if (t.dtype == DType::F16) {
        std::vector<uint16_t> tmp((size_t)count);
        for (int64_t i = 0; i < count; ++i) tmp[(size_t)i] = float_to_half(src[i]);
        vk::Stream::get().upload(t.ptr, tmp.data(), (VkDeviceSize)count * 2);
    } else {
        fail("tensor_from_host: unsupported dtype %s", dtype_name(t.dtype));
    }
}

void tensor_debug_dump(const char* label, const Tensor& t, int max_values) {
    if (log_level() < 3 || !t.valid()) return;
    const int64_t n = t.numel();
    std::vector<float> v((size_t)n);
    tensor_to_host(t, v.data(), n);
    double sum = 0, sq = 0, mn = 1e30, mx = -1e30;
    int64_t nan_count = 0;
    for (int64_t i = 0; i < n; ++i) {
        float x = v[(size_t)i];
        if (std::isnan(x) || std::isinf(x)) { ++nan_count; continue; }
        sum += x; sq += (double)x * x;
        mn = std::fmin(mn, x); mx = std::fmax(mx, x);
    }
    double mean = sum / (double)n;
    NN_LOG_DEBUG("[dbg] %-28s [", label);
    for (int i = 0; i < t.ndim; ++i)
        NN_LOG_DEBUG("%lld%s", (long long)t.shape[i], i + 1 < t.ndim ? "," : "");
    NN_LOG_DEBUG("] %s mean=%.5f std=%.5f min=%.5f max=%.5f%s\n      ",
                   dtype_name(t.dtype), mean,
                   std::sqrt(std::fmax(sq / (double)n - mean * mean, 0.0)), mn, mx,
                   nan_count ? " HAS-NAN" : "");
    for (int i = 0; i < max_values && i < n; ++i)
        NN_LOG_DEBUG("%.5f ", v[(size_t)i]);
    NN_LOG_DEBUG("\n");
}

}  // namespace nn
