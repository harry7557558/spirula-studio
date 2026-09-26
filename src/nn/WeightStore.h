#pragma once
// Named device weights: staged on the host, then uploaded into a few chunked
// allocations. A model's loader does its own renaming, fusing and folding while
// staging; everything after that -- placement, dtype, lookup with near misses
// in the error -- is the same for every model and lives here.

#include "nn/Tensor.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace nn {

class WeightStore {
public:
    WeightStore() = default;
    ~WeightStore();
    WeightStore(const WeightStore&) = delete;
    WeightStore& operator=(const WeightStore&) = delete;

    // `f16` stores the tensor as packed halves -- what the GEMM wants for a
    // matrix. A name staged twice keeps the second.
    void stage(const std::string& name, std::vector<int64_t> shape, std::vector<float> data,
               bool f16);
    // Chunked upload of everything staged; the host copies are dropped.
    void upload(const char* tag);
    // Frees the device memory and forgets every name.
    void release();

    bool has(const std::string& name) const { return tensors_.count(name) != 0; }
    // Throws when absent, listing the names that share its prefix.
    Tensor get(const std::string& name) const;
    Tensor getf(const char* fmt, ...) const;
    uint64_t deviceBytes() const { return device_bytes_; }

private:
    struct Staged {
        std::string          name;
        std::vector<int64_t> shape;
        std::vector<float>   data;
        bool                 f16 = false;
    };
    std::vector<Staged>                     staged_;
    std::unordered_map<std::string, Tensor> tensors_;
    std::vector<DevicePtr>                  chunks_;
    uint64_t                                device_bytes_ = 0;
};

}  // namespace nn
