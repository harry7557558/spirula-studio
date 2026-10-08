#pragma once

// When a wait on a GPU timeline gives up. After a driver reset an unbounded
// wait can block forever instead of reporting the lost device, so waits run in
// one-second slices and fail once no submission has finished for a minute.
// Header-only: the training backend and nn/ both wait through it.

#include "core/Env.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>

namespace spirula {

class GpuStallWatch {
public:
    static constexpr uint64_t kSliceNs = 1000000000ull;

    // A GPU's watchdog resets any submission past ~2 s (Windows TDR, amdgpu); a
    // CPU device has none and may spend minutes on one, so it never gives up.
    // SS_GPU_STALL_SECONDS replaces the minute for either; 0 never gives up.
    explicit GpuStallWatch(bool cpu_device) : limit_s_(cpu_device ? 0.0 : 60.0) {
        if (const char* v = env("GPU_STALL_SECONDS"); v && *v) limit_s_ = std::max(0.0, std::atof(v));
    }

    double limit_seconds() const { return limit_s_; }

    // After a slice timed out with the timeline at `value`; true once it has not
    // moved for the limit. A slice counts at most two seconds, so a suspended
    // machine or a starved waiting thread does not read as a stalled GPU.
    bool stalled(uint64_t value) {
        const auto now = std::chrono::steady_clock::now();
        const double slice = std::min(2.0, std::chrono::duration<double>(now - last_).count());
        last_ = now;
        if (value != seen_) {
            seen_ = value;
            stalled_s_ = 0.0;
            return false;
        }
        stalled_s_ += slice;
        return limit_s_ > 0.0 && stalled_s_ > limit_s_;
    }

private:
    double limit_s_;
    double stalled_s_ = 0.0;
    uint64_t seen_ = 0;
    std::chrono::steady_clock::time_point last_ = std::chrono::steady_clock::now();
};

}  // namespace spirula
