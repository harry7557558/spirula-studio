// xbin_frame_rate -- which of an XGRIDS camera's frames a run keeps at a rate
// (app/XbinAnchors.h). A wrong answer is silent: the dataset just gets other
// photographs. Times are the PortalCam's: about 3 Hz, a few ms of jitter, and
// the four cameras some tens of microseconds apart.

#include "app/XbinAnchors.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void expect(bool ok, const std::string& what) {
    std::printf("%s  %s\n", ok ? "ok  " : "BAD ", what.c_str());
    if (!ok) g_failures++;
}

// `n` frames at 3.006 Hz from `start` us, with a jitter of up to 3 ms.
std::vector<int64_t> camera(int n, int64_t start) {
    std::vector<int64_t> t;
    for (int i = 0; i < n; i++) t.push_back(start + (int64_t)(i * 332668.0) + (i * 7919) % 3000);
    return t;
}

std::vector<int64_t> kept(const std::vector<int64_t>& times, float fps) {
    app::lidar::FrameRate rate(fps);
    std::vector<int64_t> out;
    for (int64_t t : times)
        if (rate.keep(t)) out.push_back(t);
    return out;
}

}  // namespace

int main() {
    const std::vector<int64_t> cam0 = camera(330, 1700000000000032);
    const double seconds = (cam0.back() - cam0.front()) * 1e-6;

    expect(kept(cam0, 0.0f).size() == cam0.size(), "rate 0 keeps every frame");
    expect(kept(cam0, 10.0f).size() == cam0.size(), "a rate above the camera's keeps every frame");

    const std::vector<int64_t> one = kept(cam0, 1.0f);
    expect(one.size() >= 109 && one.size() <= 111,
           "1 fps of 3 Hz over " + std::to_string(seconds) + " s keeps " + std::to_string(one.size()));
    bool spaced = true;
    for (size_t i = 1; i < one.size(); i++)
        spaced = spaced && one[i] - one[i - 1] >= 660000 && one[i] - one[i - 1] <= 1340000;
    expect(spaced, "1 fps keeps one frame a second, never two in one");
    expect(!one.empty() && one.front() == cam0.front(), "the first frame is kept");

    const std::vector<int64_t> two = kept(cam0, 2.0f);
    expect(two.size() >= 218 && two.size() <= 221, "2 fps of 3 Hz keeps " + std::to_string(two.size()));

    for (float fps : {0.5f, 1.0f, 1.5f, 2.0f}) {
        const int64_t bound = app::lidar::frames_at_rate((int64_t)cam0.size(), seconds, fps);
        expect((int64_t)kept(cam0, fps).size() <= bound,
               "frames_at_rate bounds what " + std::to_string(fps) + " fps keeps");
    }
    expect(app::lidar::frames_at_rate(330, seconds, 0.0f) == 330, "frames_at_rate at rate 0 is every frame");
    expect(app::lidar::frames_at_rate(330, 0.0, 1.0f) == 330, "frames_at_rate without a time range is every frame");

    // camera_2 runs 32 us ahead of camera_0: the same rate keeps the same instants.
    std::vector<int64_t> cam2 = camera(330, 1700000000000000);
    const std::vector<int64_t> one2 = kept(cam2, 1.0f);
    bool same = one2.size() == one.size();
    for (size_t i = 0; same && i < one.size(); i++) same = one[i] - one2[i] == 32;
    expect(same, "two cameras at one rate keep the same instants");

    std::printf("%s\n", g_failures ? "FAILED" : "PASSED");
    return g_failures;
}
