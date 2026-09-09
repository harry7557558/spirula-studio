// The mask margin: sam::dilate_radius_px / accumulate_dilated / compose_hit.
//
// No model, no device -- this is geometry over a binary mask, so it runs
// anywhere. Every case checks the disc against an exact ground truth rather
// than checking that something got bigger; a wrong radius is a wrong picture,
// not a smaller one.

#include "sam/Masking.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("  %-4s %s\n", ok ? "ok" : "FAIL", what.c_str());
    if (!ok) ++g_failures;
}

// A solid rectangle, inclusive on both corners, in a w*h mask of 0 / 255.
sam::Mask rect_mask(int w, int h, int x0, int y0, int x1, int y1) {
    sam::Mask m;
    m.width = w;
    m.height = h;
    m.data.assign((size_t)w * h, 0);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) m.data[(size_t)y * w + x] = 255;
    return m;
}

sam::Detection detection(sam::Mask m, sam::Box box) {
    sam::Detection d;
    d.box = box;
    d.mask = std::move(m);
    return d;
}

// Distance from (x, y) to the nearest pixel of the inclusive rectangle.
double rect_distance(int x, int y, int x0, int y0, int x1, int y1) {
    const double dx = (double)std::max(0, std::max(x0 - x, x - x1));
    const double dy = (double)std::max(0, std::max(y0 - y, y - y1));
    return std::sqrt(dx * dx + dy * dy);
}

size_t count_set(const std::vector<uint8_t>& v) {
    size_t n = 0;
    for (uint8_t b : v) n += (b != 0);
    return n;
}

// The union the code took before there was a margin, kept here so the
// `dilate_ratio = 0` path can be compared byte for byte rather than by eye.
std::vector<uint8_t> plain_union(const std::vector<const sam::Mask*>& masks,
                                 size_t n) {
    std::vector<uint8_t> hit(n, 0);
    for (const sam::Mask* m : masks)
        for (size_t i = 0; i < n; ++i)
            if (m->data[i] > 127) hit[i] = 1;
    return hit;
}

// ---------------------------------------------------------------------------

void test_radius_formula() {
    std::printf("\nRadius\n");
    // 15% of the mean side, halved into a radius: an 80-wide box gains 5 on
    // each side, so its width grows by 10 -- an eighth of itself, near enough
    // to the eighth the ratio asks for once the kernel is forced odd.
    check(sam::dilate_radius_px({60, 60, 139, 139}, 0.15f) == 5,
          "0.15 of an 80 px box is 5 px");
    check(sam::dilate_radius_px({0, 0, 399, 399}, 0.15f) == 29,
          "0.15 of a 400 px box is 29 px");
    check(sam::dilate_radius_px({60, 60, 139, 139}, 0.0f) == 0,
          "0 asks for no margin at all");
    check(sam::dilate_radius_px({60, 60, 139, 139}, -0.5f) == 0,
          "a negative ratio is not a shrink, it is off");
    // A wide, short box: the only shape that tells the mean of the two sides
    // apart from the longer one (14 px) or the shorter one (3 px).
    check(sam::dilate_radius_px({0, 0, 199, 49}, 0.15f) == 9,
          "a 200x50 box takes the mean of its sides, not either one");
    check(sam::dilate_radius_px({10, 10, 10, 10}, 0.15f) == 1,
          "a degenerate box still grows by the 3 px kernel floor");
    check(sam::dilate_radius_px({100, 100, 20, 20}, 0.15f) == 1,
          "an inverted box does not produce a huge or negative radius");
}

void test_default() {
    std::printf("\nDefault\n");
    // Pinned because the value is duplicated into three GUI structs that cannot
    // include this header, and because the help text quotes these two numbers.
    const sam::MaskOptions o;
    check(o.dilate_ratio == 0.05f, "the library default margin is 5 percent");
    check(o.dilate_ratio > 0.0f, "and it is on, not opt-in");
    check(sam::dilate_radius_px({0, 0, 400, 400}, o.dilate_ratio) == 10,
          "so a 400 px object grows 10 px on each side, 20 across");
}

void test_scale_awareness() {
    std::printf("\nScale awareness\n");
    // The whole point of a per-detection ratio: two objects in one frame, and
    // the far one must not get the near one's margin.
    const int near_px = sam::dilate_radius_px({200, 200, 359, 359}, 0.15f);
    const int far_px = sam::dilate_radius_px({20, 20, 59, 59}, 0.15f);
    check(near_px == 11, "a 160 px object gets 11 px");
    check(far_px == 2, "a 40 px object in the same frame gets 2 px");
    check(near_px != far_px, "the margin is not a fixed number of pixels");
    // Each grows its own box by about the ratio asked for. The bounds are wide
    // because the kernel is forced odd, which costs up to half a pixel of
    // radius and matters most on the smallest boxes.
    const double near_growth = (160.0 + 2 * near_px) / 160.0;
    const double far_growth = (40.0 + 2 * far_px) / 40.0;
    check(near_growth > 1.05 && near_growth < 1.20,
          "the near object grew by roughly 15% of itself");
    check(far_growth > 1.05 && far_growth < 1.20,
          "and so did the far one, in its own terms");
}

void test_resolution_invariance() {
    std::printf("\nResolution invariance\n");
    // Masks are computed at `max_size`, so an object measured there is the
    // source object divided by the downscale factor. The margin has to be the
    // same fraction of the object either way, or it lands wrong in the file.
    const int full = sam::dilate_radius_px({0, 0, 399, 399}, 0.15f);
    const int quarter = sam::dilate_radius_px({0, 0, 99, 99}, 0.15f);
    check(full > 0 && quarter > 0, "both resolutions ask for a margin");
    check(std::abs(full - 4 * quarter) <= 2,
          "a 4x downscale asks for a quarter of the radius, +-2 px of rounding");
    // A radius taken from the image rather than the object would be identical
    // at both, and one scaled by the downscale factor a second time would be
    // 16x out; both are excluded by the bound above only if the two differ.
    check(full != quarter, "the two resolutions do not ask for the same radius");
}

void test_disc_geometry() {
    std::printf("\nDisc geometry\n");
    // 140 px square, so the radius here (10) is not the 5 the radius cases use:
    // an implementation that hard-codes one of them fails the other.
    const int W = 240, H = 240, X0 = 40, Y0 = 40, X1 = 179, Y1 = 179;
    const sam::Mask m = rect_mask(W, H, X0, Y0, X1, Y1);
    const int r = sam::dilate_radius_px({(float)X0, (float)Y0, (float)X1, (float)Y1},
                                        0.15f);
    std::vector<uint8_t> hit((size_t)W * H, 0);
    sam::accumulate_dilated(m, r, hit);

    size_t wrong = 0;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const bool want = rect_distance(x, y, X0, Y0, X1, Y1) <= (double)r;
            if ((hit[(size_t)y * W + x] != 0) != want) ++wrong;
        }
    check(r > 0, "the fixture asked for a margin");
    check(wrong == 0, "every pixel is set exactly when it is within r of the object");
    size_t object_px = 0;
    for (uint8_t b : m.data) object_px += (b > 127);
    check(count_set(hit) > object_px, "the result is strictly larger than the object");
    check(count_set(hit) < (size_t)W * H, "and has not swallowed the frame");
}

void test_zero_is_a_no_op() {
    std::printf("\nNo margin\n");
    const sam::Mask a = rect_mask(120, 90, 10, 10, 40, 40);
    const sam::Mask b = rect_mask(120, 90, 70, 50, 100, 80);
    const size_t n = (size_t)120 * 90;

    std::vector<uint8_t> hit(n, 0);
    sam::accumulate_dilated(a, 0, hit);
    sam::accumulate_dilated(b, 0, hit);
    const std::vector<uint8_t> want = plain_union({&a, &b}, n);
    check(hit == want, "radius 0 is byte for byte the union that came before");

    // ... and the comparison is worth making, because a margin does move it.
    std::vector<uint8_t> grown(n, 0);
    sam::accumulate_dilated(a, sam::dilate_radius_px({10, 10, 40, 40}, 0.15f), grown);
    sam::accumulate_dilated(b, sam::dilate_radius_px({70, 50, 100, 80}, 0.15f), grown);
    check(grown != want, "and the default ratio does not produce that same union");

    sam::Mask empty = rect_mask(120, 90, 10, 10, 10, 10);
    empty.data.assign(n, 0);
    std::vector<uint8_t> untouched(n, 0);
    sam::accumulate_dilated(empty, 7, untouched);
    check(count_set(untouched) == 0, "a mask with nothing in it grows nothing");
}

void test_border_clips_and_does_not_wrap() {
    std::printf("\nBorder\n");
    // A panorama's masks routinely touch the left and right edges; a wrap here
    // would paint the far side of the frame and nobody would look for it.
    const int W = 100, H = 100;
    const sam::Mask right = rect_mask(W, H, 80, 20, 99, 79);
    const int r = sam::dilate_radius_px({80, 20, 99, 79}, 0.15f);
    check(r >= 2, "the edge object asks for a margin worth testing");

    std::vector<uint8_t> hit((size_t)W * H, 0);
    sam::accumulate_dilated(right, r, hit);

    bool wrapped = false;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < r + 1; ++x)
            if (hit[(size_t)y * W + x]) wrapped = true;
    check(!wrapped, "nothing appeared on the opposite edge");
    check(hit[(size_t)20 * W + 99] && hit[(size_t)(20 - r) * W + 99],
          "the margin still grew along the edge it could");
    check(hit[(size_t)20 * W + (80 - r)] && !hit[(size_t)20 * W + (80 - r - 1)],
          "and inward by exactly r");

    const sam::Mask left = rect_mask(W, H, 0, 20, 19, 79);
    std::vector<uint8_t> hit2((size_t)W * H, 0);
    sam::accumulate_dilated(left, sam::dilate_radius_px({0, 20, 19, 79}, 0.15f), hit2);
    bool wrapped2 = false;
    for (int y = 0; y < H; ++y)
        for (int x = W - 4; x < W; ++x)
            if (hit2[(size_t)y * W + x]) wrapped2 = true;
    check(!wrapped2, "and the same at the left edge");
}

void test_negative_beats_the_margin() {
    std::printf("\nComposition order\n");
    const int W = 200, H = 200;
    sam::Result pos, neg, none;
    pos.detections.push_back(
        detection(rect_mask(W, H, 60, 60, 139, 139), {60, 60, 139, 139}));
    // Sits entirely outside the object and entirely inside its margin.
    neg.detections.push_back(
        detection(rect_mask(W, H, 140, 60, 160, 139), {140, 60, 160, 139}));

    std::vector<uint8_t> with_neg((size_t)W * H, 0);
    sam::compose_hit(pos, neg, 0.15f, with_neg);
    std::vector<uint8_t> without((size_t)W * H, 0);
    sam::compose_hit(pos, none, 0.15f, without);

    check(without[(size_t)100 * W + 142] != 0,
          "the margin does reach past the object into the negative's ground");
    check(with_neg[(size_t)100 * W + 142] == 0,
          "a negative phrase carves the margin back out");
    check(with_neg[(size_t)100 * W + 139] != 0, "the object itself is untouched");
    check(with_neg[(size_t)100 * W + 55] != 0,
          "and the margin survives where no negative covers it");
}

void test_compose_matches_the_old_union() {
    std::printf("\nCompose at ratio 0\n");
    const int W = 120, H = 90;
    const size_t n = (size_t)W * H;
    sam::Result pos, neg;
    pos.detections.push_back(
        detection(rect_mask(W, H, 10, 10, 40, 40), {10, 10, 40, 40}));
    pos.detections.push_back(
        detection(rect_mask(W, H, 70, 50, 100, 80), {70, 50, 100, 80}));
    neg.detections.push_back(
        detection(rect_mask(W, H, 20, 20, 30, 30), {20, 20, 30, 30}));

    std::vector<uint8_t> hit(n, 0);
    sam::compose_hit(pos, neg, 0.0f, hit);

    std::vector<uint8_t> want =
        plain_union({&pos.detections[0].mask, &pos.detections[1].mask}, n);
    for (size_t i = 0; i < n; ++i)
        if (neg.detections[0].mask.data[i] > 127) want[i] = 0;
    check(hit == want, "compose_hit at ratio 0 is the union it replaced");
    check(count_set(hit) > 0 && count_set(hit) < n,
          "and the fixture is not trivially empty or trivially full");
}

}  // namespace

int main() {
    test_radius_formula();
    test_default();
    test_scale_awareness();
    test_resolution_invariance();
    test_disc_geometry();
    test_zero_is_a_no_op();
    test_border_clips_and_does_not_wrap();
    test_negative_beats_the_margin();
    test_compose_matches_the_old_union();

    std::printf("\n%d failures\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
