// frame_pick -- which chosen frames are left out (app/FramePick.h) and the
// measure they are left out by (app/FrameSharpness.h, edge_width_px). A wrong
// answer here is silent: a dataset simply comes out with other frames. So the
// rule's corners are asserted -- the threshold itself, the cap on frames
// skipped in a row, a frame both blurred and still, the first frame, a frame
// nothing could be measured on -- and the measure is held to telling a
// smeared picture from a sharp one whichever way the smear runs.

#include "app/FramePick.h"

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

// The verdicts a rule gives a run of edge widths, as "K", "B", "S" a frame.
std::string walk(app::PickRule rule, const std::vector<float>& widths,
                 const std::vector<uint64_t>& hashes = {}) {
    std::string out;
    for (size_t i = 0; i < widths.size(); i++) {
        app::FramePick p;
        p.index = (int64_t)i;
        p.width = widths[i];
        p.hash = i < hashes.size() ? hashes[i] : 0x0123456789abcdefull * (i + 1);
        out += "KBS"[(int)rule.judge(p)];
    }
    return out;
}

// A chequerboard smeared across `smear` pixels along x (or y): squares of
// unequal contrast, since the measure takes the strongest edges each way.
std::vector<uint8_t> squares(int w, int h, int smear, bool along_y) {
    std::vector<uint8_t> px((size_t)w * h * 3);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int sum = 0;
            for (int k = 0; k < smear; k++) {
                const int cx = (along_y ? x : x + k) / 40, cy = (along_y ? y + k : y) / 40;
                sum += (cx + cy) % 2 ? 110 + 15 * ((cx + 3 * cy) % 8) : 30;
            }
            for (int c = 0; c < 3; c++) px[((size_t)y * w + x) * 3 + c] = (uint8_t)(sum / smear);
        }
    return px;
}

// Brightness rising (or falling) left to right: every dHash bit the same.
std::vector<uint8_t> ramp(int w, int h, bool rising) {
    std::vector<uint8_t> px((size_t)w * h * 3);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const uint8_t v = (uint8_t)((rising ? x : w - 1 - x) * 255 / (w - 1));
            for (int c = 0; c < 3; c++) px[((size_t)y * w + x) * 3 + c] = v;
        }
    return px;
}

int bits(uint64_t x) {
    int n = 0;
    for (; x; x &= x - 1) n++;
    return n;
}

void test_rule() {
    const std::vector<float> run{3, 3, 20, 3, 14, 3};
    expect(!app::PickRule{}.on() && walk(app::PickRule{}, run) == "KKKKKK",
           "off: nothing is left out, however wide its edges");
    expect(walk(app::PickRule{8.0f, true, 0}, run) == "KKBKBK",
           "8 px: the 20 px and 14 px frames go");
    expect(walk(app::PickRule{16.0f, true, 0}, run) == "KKBKKK",
           "16 px: the 14 px frame, 13.6 px past the lens, stays");

    // No lens share to take off (the sharpest frame is 0 wide), so blur is
    // the width and the threshold can be met exactly.
    app::PickRule exact{8.0f, true, 0};
    app::FramePick zero, at, over;
    zero.width = 0.0f;
    at.width = 8.0f;
    over.width = 8.25f;
    exact.judge(zero);
    expect(exact.judge(at) == app::PickVerdict::Kept && at.blur == 8.0f,
           "a frame blurred exactly the threshold is kept");
    expect(exact.judge(over) == app::PickVerdict::Blurry, "and one just past it is not");

    const std::vector<float> stretch{0, 20, 20, 20, 20, 20, 0};
    expect(walk(app::PickRule{8.0f, true, 0}, stretch) == "KBBKBBK",
           "neighbour: never more than two left out in a row");
    expect(walk(app::PickRule{8.0f, false, 0}, stretch) == "KBBBBBK",
           "without it a bad stretch goes whole");
    expect(walk(app::PickRule{8.0f, true, 0}, {0, 20, 20}) == "KBB",
           "the last frames of a sequence can be left out like any other");

    expect(walk(app::PickRule{8.0f, true, 0}, {}).empty(), "no frames: nothing to rule on");
    expect(walk(app::PickRule{8.0f, true, 0}, {40}) == "K",
           "one frame is its own measure of sharp, and is kept");
    expect(walk(app::PickRule{1.0f, true, 0}, {40, 40, 40}) == "KKK",
           "frames all equally soft are the capture's lens, not blur");

    app::PickRule gap{8.0f, true, 0};
    app::FramePick none;
    expect(gap.judge(none) == app::PickVerdict::Kept && none.blur < 0.0f && gap.widths.empty(),
           "a frame nothing could be measured on is kept, and teaches the rule nothing");
}

void test_similar() {
    const uint64_t a = 0xF0F0F0F0F0F0F0F0ull;
    const uint64_t four = a ^ 0xFull, five = a ^ 0x1Full, eight = four ^ 0xF0ull;
    const app::PickRule rule{0.0f, true, 5};
    expect(rule.on(), "a similarity threshold alone turns the rule on");
    expect(walk(rule, {-1, -1}, {a, a}) == "KS", "the same picture twice: the second goes");
    expect(walk(rule, {-1, -1}, {a, four}) == "KS" && walk(rule, {-1, -1}, {a, five}) == "KK",
           "5 bits: four apart is too alike, five apart is another frame");
    expect(walk(rule, {-1, -1, -1}, {a, four, eight}) == "KSK",
           "compared with the last frame KEPT, not the one before it");
    expect(walk(rule, {-1, -1, -1, -1, -1}, {a, a, a, a, a}) == "KSSSS",
           "a still camera is not a bad stretch: no cap on how many go");
    expect(walk(app::PickRule{8.0f, true, 5}, {0, 20}, {a, a}) == "KS",
           "blurred and still: left out as similar, which does not use up the cap");
    expect(walk(app::PickRule{0.0f, true, 0}, {-1, -1}, {a, a}) == "KK",
           "0 bits: similarity is off");
}

void test_hash() {
    const int W = 640, H = 480;
    const uint64_t up = app::frame_dhash(ramp(W, H, true).data(), W, H);
    const uint64_t down = app::frame_dhash(ramp(W, H, false).data(), W, H);
    expect(up == 0 && down == ~0ull, "dHash: one bit a cell brighter than its right neighbour");
    const uint64_t big = app::frame_dhash(squares(W, H, 1, false).data(), W, H);
    const uint64_t half = app::frame_dhash(squares(W / 2, H / 2, 1, false).data(), W / 2, H / 2);
    expect(big != 0 && bits(big ^ app::frame_dhash(squares(W, H, 1, false).data(), W, H)) == 0,
           "dHash: the same pixels, the same hash");
    expect(bits(big ^ half) >= 16, "dHash: another picture is many bits away (" +
                                       std::to_string(bits(big ^ half)) + ")");
    expect(bits(big ^ app::frame_dhash(squares(W, H, 3, false).data(), W, H)) <= 4,
           "dHash: a slight smear of the same picture is a few (" +
               std::to_string(bits(big ^ app::frame_dhash(squares(W, H, 3, false).data(), W, H))) + ")");
    expect(app::frame_dhash(ramp(8, 8, false).data(), 8, 8) == 0 &&
               app::frame_dhash(nullptr, W, H) == 0,
           "dHash: no pixels, or fewer than its 9x8 cells, is 0");
}

void test_edge_width() {
    const int W = 640, H = 480;
    const float sharp = app::edge_width_px(squares(W, H, 1, false).data(), W, H);
    const float across = app::edge_width_px(squares(W, H, 30, false).data(), W, H);
    const float down = app::edge_width_px(squares(W, H, 30, true).data(), W, H);
    const std::string got = " (" + std::to_string(sharp) + ", " + std::to_string(across) + ", " +
                            std::to_string(down) + ")";
    expect(sharp > 0.0f && sharp < 14.0f, "edge width: a sharp picture's edges are narrow" + got);
    expect(across > sharp + 20.0f && down > sharp + 20.0f,
           "edge width: a 30 px smear widens them by about that, whichever way it runs" + got);
    const std::vector<uint8_t> flat((size_t)W * H * 3, 128);
    expect(app::edge_width_px(flat.data(), W, H) < 0.0f &&
               app::edge_width_px(squares(48, 48, 1, false).data(), 48, 48) < 0.0f &&
               app::edge_width_px(nullptr, W, H) < 0.0f,
           "edge width: no edges, under 64 px a side, or no pixels: not measured");

    app::FramePick only_hash, both;
    app::measure_pick(only_hash, squares(W, H, 1, false).data(), W, H, /*blur=*/false);
    app::measure_pick(both, squares(W, H, 1, false).data(), W, H, /*blur=*/true);
    expect(only_hash.width < 0.0f && only_hash.hash != 0 && both.width == sharp,
           "the edge width is measured only where blur is being judged");

    // The whole way through: five sharp frames and a smeared one.
    app::PickRule rule{12.0f, true, 0};
    std::string verdicts;
    for (int i = 0; i < 6; i++) {
        app::FramePick p;
        app::measure_pick(p, squares(W, H, i == 4 ? 30 : 1, false).data(), W, H, true);
        verdicts += "KBS"[(int)rule.judge(p)];
    }
    expect(verdicts == "KKKKBK", "measured and ruled on: the smeared frame goes, " + verdicts);
}

}  // namespace

int main() {
    test_rule();
    test_similar();
    test_hash();
    test_edge_width();
    std::printf("%s\n", g_failures ? "FAILED" : "PASSED");
    return g_failures;
}
