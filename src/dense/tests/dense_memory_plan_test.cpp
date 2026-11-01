#include "dense/MemoryPlan.h"

#include <cstdio>
#include <stdexcept>

namespace {

using namespace spirula::dense;

void check(bool valid, const char* message) {
    if (!valid) throw std::runtime_error(message);
}

}  // namespace

int main() {
    try {
        constexpr uint64_t GiB = 1ull << 30;
        check(planned_host_bytes(32 * GiB, false) == 16 * GiB + 8 * GiB + 4 * GiB + 8 * GiB + GiB, "source plan does not add the matching budgets");
        check(planned_host_bytes(32 * GiB, true) == planned_host_bytes(32 * GiB, false) + 8 * GiB, "rectified plan omits the warp cache");
    } catch (const std::exception& e) { std::printf("FAIL %s\n", e.what()); return 1; }
    std::printf("PASS dense memory plan\n");
    return 0;
}
