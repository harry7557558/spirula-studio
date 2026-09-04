#pragma once
// SS_LOMA_DUMP=<dir>: one .npy per stage, for tools/loma/compare_ort.py.
//
// A forward pass must never round-trip through the host, so nothing here runs
// unless the variable is set.

#include "nn/Tensor.h"

#include <vector>

namespace loma {

bool dump_enabled();
void dump_tensor(const char* name, const nn::Tensor& t, const std::vector<int64_t>& shape);
void dump_host(const char* name, const float* data, const std::vector<int64_t>& shape);

}  // namespace loma
