#include "dense/MemoryPlan.h"

namespace spirula::dense {

uint64_t planned_host_bytes(uint64_t budget, bool rectified) {
    // Decoded pixels (1/2), queued predictions (1/4), pending tiles (1/8), reference workers (1/4),
    // the paged job table (1/32), and for rectified runs the warp-plan cache (1/4).
    return budget / 2 + budget / 4 + budget / 8 + budget / 4 + budget / 32 + (rectified ? budget / 4 : 0);
}

}  // namespace spirula::dense
