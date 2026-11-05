#pragma once

// The host memory the dense run's own budgets allow, which is the peak its
// memory report plans for (app/RunReport.h).

#include <cstdint>

namespace spirula::dense {

// The matching stage's host budgets added up; each is a share of `budget` in
// app/DenseProcessing.cpp and src/dense/Reconstruction.cpp, and must follow them.
uint64_t planned_host_bytes(uint64_t budget, bool rectified);

}  // namespace spirula::dense
