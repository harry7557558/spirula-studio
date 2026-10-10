// Progressive alignment's settings (sfm/Progressive.h): the error ladder, and
// which tolerance verification and the mapper are left holding.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "sfm/SfmConfig.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;

static void check(bool condition, const std::string& message, int& fails) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", message.c_str());
    fails++;
}

static int cmdProgressiveTest(int, char**) {
    int fails = 0;

    SfmConfig off;
    check(off.finalize(CMD_AUTO).empty() && off.progressiveErrors().empty(), "off: no ladder",
          fails);
    check(off.twoview.ransac.max_error == 3.0 && off.mapper.max_reproj_error == 3.0,
          "off: one tolerance", fails);

    SfmConfig on;
    on.progressive = true;
    check(on.finalize(CMD_AUTO).empty(), "on: finalizes", fails);
    const std::vector<double> e = on.progressiveErrors();
    check(e.size() == 5 && e.front() == 20.0 && e.back() == 3.0, "20 to 3 in 5", fails);
    bool falling = true, even = true;
    for (size_t k = 1; k < e.size(); k++) {
        falling = falling && e[k] < e[k - 1];
        even = even && std::fabs(e[k] / e[k - 1] - e[1] / e[0]) < 1e-9;
    }
    check(falling && even, "geometric steps, falling", fails);
    check(on.twoview.ransac.max_error == 20.0, "matches verified at the start", fails);
    check(on.mapper.max_reproj_error == 3.0, "the mapper holds the end", fails);

    SfmConfig end;
    end.progressive = true;
    end.max_error = 4.0;
    end.progressive_error_end = 2.0;
    end.progressive_error_steps = 2;
    check(end.finalize(CMD_AUTO).empty() && end.progressiveErrors() == std::vector<double>{20, 2},
          "an explicit end beats --max-error", fails);

    SfmConfig bad;
    bad.progressive = true;
    bad.progressive_error_start = 2.0;
    check(!bad.finalize(CMD_AUTO).empty(), "a start below the end is refused", fails);

    std::printf("%s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, cmdProgressiveTest); }
