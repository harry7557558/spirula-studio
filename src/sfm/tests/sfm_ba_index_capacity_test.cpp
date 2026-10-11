#include <cstdio>
#include <limits>
#include <stdexcept>
#include <type_traits>

#include "sfm/ba/Problem.h"
#include "sfm/tests/TestMain.h"

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static void testScalarBoundaries() {
    static_assert(!std::is_base_of<BAOverBudget, BAIndexCapacity>::value);
    checkBAIndexCapacity(UINT32_MAX, UINT32_MAX, "Jc pool");
    checkBAIndexCapacity(0, 0, "empty pool");
    for (uint64_t need : {uint64_t{UINT32_MAX} + 1, std::numeric_limits<uint64_t>::max()}) {
        bool rejected = false;
        try {
            checkBAIndexCapacity(need, std::numeric_limits<uint64_t>::max(), "Jc pool");
        } catch (const BAIndexCapacity& e) {
            rejected = e.need_elements == need && e.limit_elements == UINT32_MAX &&
                       e.pool == "Jc pool" && std::string(e.what()).find("memory") == std::string::npos;
        }
        require(rejected, "32-bit capacity failure lost its count or became a memory error");
    }
    bool rejected = false;
    try {
        checkBAIndexCapacity(1, 0, "empty pool");
    } catch (const BAIndexCapacity& e) {
        rejected = e.need_elements == 1 && e.limit_elements == 0;
    }
    require(rejected, "zero capacity accepted a non-empty pool");
}

static BAProblem mixedProblem(uint64_t limit) {
    BAProblem P;
    P.num_images = P.num_frames = 2;
    P.num_obs = 3;
    P.index_limit = limit;
    P.image_group = {0, 1};
    P.groups = {{0, 0, 4, 5}, {4, 4, 12, 7}};
    P.image_frame = {0, 1};
    P.image_member = {kNoMember, 0};
    P.members = {{0, 12, 6}};
    P.obs_image = {0, 1, 1};
    return P;
}

static void testFinalizeCapacity() {
    BAProblem exact = mixedProblem(116);
    finalizeTables(exact);
    require(exact.jc_total == 116 && exact.jc_off == std::vector<uint32_t>({0, 20, 68}),
            "mixed plain/rig Jacobian offsets are incorrect");
    require(exact.model_obs == std::vector<uint32_t>({0, 1, 2}), "observation order changed");
    require(exact.model_ranges.size() == 2 && !exact.model_ranges[0].rig &&
            exact.model_ranges[1].rig, "plain/rig dispatch buckets changed");
    BAProblem too_small = mixedProblem(115);
    bool rejected = false;
    try {
        finalizeTables(too_small);
    } catch (const BAIndexCapacity& e) {
        rejected = e.need_elements == 116 && e.limit_elements == 115 && e.pool == "Jc pool";
    }
    require(rejected, "finalizeTables accepted an oversized Jacobian pool");
    require(too_small.model_obs.empty() && too_small.jc_off.empty(),
            "finalizeTables allocated observation tables before rejecting capacity");
}

static int runTests(int, char**) {
    testScalarBoundaries();
    testFinalizeCapacity();
    std::puts("sfm_ba_index_capacity_test: PASS");
    return 0;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, runTests); }
