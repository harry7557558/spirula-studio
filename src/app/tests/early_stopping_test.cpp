// early_stopping_test.cpp -- CPU unit tests for ValidationTracker and mask alignment.

#include "app/EarlyStopping.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

static int failures = 0;

static void check(bool ok, const char* what) {
    if (ok) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
}

static void test_monitor_only_never_stops() {
    spirula::ValidationTracker t(/*patience=*/0, /*min_delta=*/0.05f);
    check(!t.observe(500, 24.0f, 0.80f).should_stop, "first check never stops");
    check(t.has_best && t.best_step == 500 && t.best_psnr == 24.0f, "records best");
    for (int step = 1000; step <= 4000; step += 500) {
        const auto u = t.observe(step, 23.5f, 0.78f);
        check(!u.improved && !u.should_stop, "patience=0 never stops early");
    }
    check(!t.stopped && t.best_step == 500, "monitor-only keeps running");
}

static void test_patience_and_min_delta() {
    spirula::ValidationTracker t(/*patience=*/3, /*min_delta=*/0.05f);
    auto u = t.observe(500, 25.00f, 0.82f);
    check(u.tracked && u.improved && !u.should_stop, "initial check sets best");

    // Gains smaller than min_delta must count against patience.
    u = t.observe(1000, 25.04f, 0.83f);
    check(!u.improved && !u.should_stop && t.stale_checks == 1, "sub-delta gain stale 1");

    // Gains >= min_delta reset patience and advance best_step.
    u = t.observe(1500, 25.06f, 0.84f);
    check(u.improved && !u.should_stop && t.stale_checks == 0 && t.best_step == 1500,
          "delta >= min_delta resets patience");

    u = t.observe(2000, 25.00f, 0.83f);
    check(!u.improved && !u.should_stop && t.stale_checks == 1, "drop 1");
    u = t.observe(2500, 24.90f, 0.82f);
    check(!u.improved && !u.should_stop && t.stale_checks == 2, "drop 2");
    u = t.observe(3000, 24.80f, 0.81f);
    check(!u.improved && u.should_stop && t.stopped && t.best_step == 1500,
          "3rd stale check triggers early stop with preserved best_step");
}

static void test_progressive_resolution_coarse_reset() {
    spirula::ValidationTracker t(/*patience=*/2, /*min_delta=*/0.01f);
    for (int step = 500; step <= 2500; step += 500) {
        const auto u = t.observe(step, 20.0f, 0.70f, /*divisor=*/4);
        check(!u.tracked && !u.improved && !u.should_stop,
              "coarse stage does not trigger early stop or claim best");
    }
    check(!t.has_best && t.stale_checks == 0, "no best locked during coarse stage");

    const auto full0 = t.observe(3000, 22.0f, 0.75f, /*divisor=*/1);
    check(full0.coarse_reset && full0.tracked && full0.improved,
          "first full-resolution check resets and records best");
    check(t.best_step == 3000 && t.stale_checks == 0, "best starts at full-res step");
}

static void test_non_finite_values() {
    const float inf = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    spirula::ValidationTracker t(/*patience=*/2, /*min_delta=*/0.01f);

    auto u = t.observe(500, inf, 1.0f);
    check(u.improved && std::isinf(t.best_psnr), "first +inf becomes best");
    u = t.observe(1000, inf, 1.0f);
    check(!u.improved && !u.should_stop && t.stale_checks == 1,
          "repeated +inf does not trigger NaN comparison or false reset");
    u = t.observe(1500, nan, 0.5f);
    check(!u.improved && u.should_stop && std::isinf(t.best_psnr),
          "NaN counts as stale without corrupting best_psnr");
}

static void test_mask_preparation() {
    std::vector<uint8_t> resized;
    bool all_masked = false;

    const uint8_t zero1x1 = 0;
    check(spirula::prepare_validation_mask(&zero1x1, 1, 1, 8, 8, resized, &all_masked) == nullptr &&
              all_masked,
          "1x1 zero mask skips view");

    const uint8_t one1x1 = 1;
    check(spirula::prepare_validation_mask(&one1x1, 1, 1, 8, 8, resized, &all_masked) == nullptr &&
              !all_masked,
          "1x1 non-zero mask treats all pixels valid");

    const uint8_t grid[4] = {0, 1, 2, 0};
    const uint8_t* out = spirula::prepare_validation_mask(grid, 2, 2, 4, 4, resized, &all_masked);
    check(out == resized.data() && !all_masked && resized.size() == 16,
          "mismatched mask resizes cleanly");
    check(resized[0] == 0 && resized[3] == 1 && resized[12] == 2 && resized[15] == 0,
          "nearest-neighbor mask corners preserved");

    const uint8_t zeros[4] = {0, 0, 0, 0};
    spirula::prepare_validation_mask(zeros, 2, 2, 4, 4, resized, &all_masked);
    check(all_masked, "all-zero resized mask marks view masked out");
}

int main() {
    test_monitor_only_never_stops();
    test_patience_and_min_delta();
    test_progressive_resolution_coarse_reset();
    test_non_finite_values();
    test_mask_preparation();
    if (failures) return 1;
    std::puts("early_stopping_test: OK");
    return 0;
}
