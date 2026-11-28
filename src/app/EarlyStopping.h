#pragma once

// ValidationTracker -- tracks held-out validation PSNR/SSIM and early-stop state.
// Stays pure CPU so host unit tests can exercise patience, min-delta, coarse
// progressive-resolution resets, and non-finite inputs without a GPU device.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace spirula {

struct ValidationMetrics {
    int   step = 0;
    float psnr = 0.0f;
    float ssim = 0.0f;
    float l1 = 0.0f;
    int   views = 0;
};

struct ValidationUpdate {
    bool tracked = false;
    bool improved = false;
    bool should_stop = false;
    bool coarse_reset = false;
};

// Track best full-resolution validation PSNR and patience-based early stopping.
class ValidationTracker {
public:
    ValidationTracker(int patience = 0, float min_delta = 0.0f) {
        reset(patience, min_delta);
    }

    void reset(int patience, float min_delta) {
        _patience = std::max(0, patience);
        _min_delta = (std::isfinite(min_delta) && min_delta > 0.0f) ? min_delta : 0.0f;
        has_best = false;
        best_step = 0;
        best_psnr = -std::numeric_limits<float>::infinity();
        best_ssim = 0.0f;
        stale_checks = 0;
        stopped = false;
        last_divisor = 0;
        last_psnr = 0.0f;
        last_ssim = 0.0f;
        has_last = false;
    }

    int patience() const { return _patience; }
    float min_delta() const { return _min_delta; }

    ValidationUpdate observe(int step, float psnr, float ssim, int divisor = 1) {
        ValidationUpdate u;
        last_psnr = psnr;
        last_ssim = ssim;
        has_last = true;

        // Progressive resolution scales training images down early; only compare
        // and count patience once training reaches full resolution (divisor <= 1).
        if (divisor > 1) {
            stale_checks = 0;
            last_divisor = divisor;
            return u;
        }
        if (last_divisor > 1) {
            stale_checks = 0;
            u.coarse_reset = true;
        }
        last_divisor = divisor;
        u.tracked = true;

        bool improved = false;
        if (!std::isnan(psnr)) {
            if (!has_best) {
                improved = true;
            } else if (std::isinf(psnr)) {
                improved = (psnr > 0.0f) && !std::isinf(best_psnr);
            } else if (!std::isinf(best_psnr)) {
                improved = _min_delta <= 0.0f
                    ? (psnr > best_psnr)
                    : (psnr - best_psnr >= _min_delta);
            }
        }

        if (improved) {
            has_best = true;
            best_step = step;
            best_psnr = psnr;
            best_ssim = std::isnan(ssim) ? 0.0f : ssim;
            stale_checks = 0;
            u.improved = true;
            return u;
        }

        if (_patience > 0) {
            ++stale_checks;
            if (stale_checks >= _patience) {
                stopped = true;
                u.should_stop = true;
            }
        }
        return u;
    }

    bool  has_best = false;
    int   best_step = 0;
    float best_psnr = -std::numeric_limits<float>::infinity();
    float best_ssim = 0.0f;
    int   stale_checks = 0;
    bool  stopped = false;
    int   last_divisor = 0;
    float last_psnr = 0.0f;
    float last_ssim = 0.0f;
    bool  has_last = false;

private:
    int   _patience = 0;
    float _min_delta = 0.0f;
};

// Map an engine GT alpha slice [mH, mW] onto [rH, rW] for validation metrics.
// Sets *all_masked_out = true when every pixel is excluded.
inline const uint8_t* prepare_validation_mask(
    const uint8_t* src, int mH, int mW, int rH, int rW,
    std::vector<uint8_t>& resized, bool* all_masked_out = nullptr)
{
    if (all_masked_out) *all_masked_out = false;
    if (!src || mH <= 0 || mW <= 0 || rH <= 0 || rW <= 0) return nullptr;
    if (mH == 1 && mW == 1) {
        if (src[0] == 0 && all_masked_out) *all_masked_out = true;
        return nullptr;
    }
    const uint8_t* mask = src;
    if (mH != rH || mW != rW) {
        resized.resize((size_t)rH * (size_t)rW);
        for (int y = 0; y < rH; ++y) {
            const int sy = (int)(((int64_t)y * mH) / rH);
            for (int x = 0; x < rW; ++x) {
                const int sx = (int)(((int64_t)x * mW) / rW);
                resized[(size_t)y * (size_t)rW + (size_t)x] =
                    src[(size_t)sy * (size_t)mW + (size_t)sx];
            }
        }
        mask = resized.data();
    }
    const size_t n = (size_t)rH * (size_t)rW;
    bool any = false;
    for (size_t i = 0; i < n; ++i) {
        if (mask[i] != 0) { any = true; break; }
    }
    if (!any && all_masked_out) *all_masked_out = true;
    return mask;
}

}  // namespace spirula
