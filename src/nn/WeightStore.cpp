#include "nn/WeightStore.h"

#include "nn/core/Error.h"
#include "nn/vk/Memory.h"
#include "nn/vk/Stream.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>

namespace nn {
namespace {

constexpr uint64_t kAlign = 256;
// NVIDIA's Windows driver refuses a single 1 GiB device-local allocation on an
// 8 GiB laptop part with 7 GiB free.
constexpr uint64_t kChunkBytes = 256ull << 20;

uint64_t align_up(uint64_t v, uint64_t a) { return (v + a - 1) / a * a; }

}  // namespace

WeightStore::~WeightStore() { release(); }

void WeightStore::release() {
    for (DevicePtr p : chunks_) vk::device_free(p);
    chunks_.clear();
    tensors_.clear();
    staged_.clear();
    device_bytes_ = 0;
}

void WeightStore::stage(const std::string& name, std::vector<int64_t> shape,
                        std::vector<float> data, bool f16) {
    NN_CHECK(shape.size() <= 4, "weight '%s' has rank %zu; nn::Tensor holds 4", name.c_str(),
             shape.size());
    int64_t n = 1;
    for (int64_t d : shape) n *= d;
    NN_CHECK(n == (int64_t)data.size(), "weight '%s': shape holds %lld, data %zu",
             name.c_str(), (long long)n, data.size());
    for (Staged& s : staged_)
        if (s.name == name) {
            s = Staged{name, std::move(shape), std::move(data), f16};
            return;
        }
    staged_.push_back(Staged{name, std::move(shape), std::move(data), f16});
}

void WeightStore::upload(const char* tag) {
    std::vector<uint64_t> chunk_sizes;
    std::vector<std::pair<uint32_t, uint64_t>> placement(staged_.size());
    uint64_t used = 0;
    for (size_t i = 0; i < staged_.size(); ++i) {
        const uint64_t bytes = align_up((uint64_t)staged_[i].data.size() *
                                            (staged_[i].f16 ? 2u : 4u), 4);
        used = align_up(used, kAlign);
        if (used != 0 && used + bytes > kChunkBytes) {
            chunk_sizes.push_back(used);
            used = 0;
        }
        placement[i] = {(uint32_t)chunk_sizes.size(), used};
        used += bytes;
        device_bytes_ += bytes;
    }
    if (used) chunk_sizes.push_back(used);
    const size_t first = chunks_.size();
    for (uint64_t sz : chunk_sizes) chunks_.push_back(vk::device_alloc(sz, tag));

    for (size_t i = 0; i < staged_.size(); ++i) {
        Staged& s = staged_[i];
        Tensor t;
        t.ptr = chunks_[first + placement[i].first] + placement[i].second;
        t.dtype = s.f16 ? DType::F16 : DType::F32;
        t.ndim = (int32_t)std::max<size_t>(s.shape.size(), 1);
        for (size_t d = 0; d < s.shape.size(); ++d) t.shape[d] = s.shape[d];
        if (s.shape.empty()) t.shape[0] = 1;
        tensor_from_host(t, s.data.data(), (int64_t)s.data.size());
        tensors_[s.name] = t;
    }
    vk::Stream::get().sync();
    staged_.clear();
}

Tensor WeightStore::get(const std::string& name) const {
    auto it = tensors_.find(name);
    if (it != tensors_.end()) return it->second;
    std::string hints;
    const size_t dot = name.rfind('.');
    const std::string stem = name.substr(0, dot == std::string::npos ? 0 : dot);
    int shown = 0;
    for (const auto& kv : tensors_)
        if (!stem.empty() && kv.first.compare(0, stem.size(), stem) == 0 && shown++ < 8)
            hints += "\n  " + kv.first;
    fail("weight '%s' is not in the checkpoint%s%s", name.c_str(),
         hints.empty() ? "" : "; near misses:", hints.c_str());
}

Tensor WeightStore::getf(const char* fmt, ...) const {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return get(buf);
}

}  // namespace nn
