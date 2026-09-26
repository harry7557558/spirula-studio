#pragma once
// Per-stage .npy dumps for a parity check against a reference implementation:
// SS_<MODEL>_DUMP=<dir> turns them on, and each model's tools/<model>/
// compare script reads them. Off, a dump is one pointer test.

#include "nn/Tensor.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nn {

class StageDump {
public:
    // `env_suffix` is read through spirula::env, e.g. "BIREFNET_DUMP".
    explicit StageDump(const char* env_suffix);
    bool on() const { return !dir_.empty(); }
    void tensor(const char* name, const Tensor& t, const std::vector<int64_t>& shape) const;
    void host(const char* name, const float* data, const std::vector<int64_t>& shape) const;

private:
    std::string dir_;
};

// A .npy of float32 into `data`; false when unreadable.
bool read_npy_f32(const std::string& path, std::vector<float>& data,
                  std::vector<int64_t>& shape);
void write_npy_f32(const std::string& path, const float* data,
                   const std::vector<int64_t>& shape);

}  // namespace nn
