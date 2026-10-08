// gpu_stall_test -- when a GPU timeline wait gives up (core/GpuStall.h): only
// after the limit without progress, counting at most two seconds per slice,
// never on a CPU device unless SS_GPU_STALL_SECONDS asks. Host only, ~5 s.

#include "core/GpuStall.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

void set_limit(const char* seconds) {
#ifdef _WIN32
    _putenv_s("SS_GPU_STALL_SECONDS", seconds ? seconds : "");
#else
    if (seconds) setenv("SS_GPU_STALL_SECONDS", seconds, 1);
    else unsetenv("SS_GPU_STALL_SECONDS");
#endif
}

void wait_s(double s) { std::this_thread::sleep_for(std::chrono::duration<double>(s)); }

}  // namespace

int main() {
    set_limit("0.5");
    {
        spirula::GpuStallWatch watch(false);
        bool early = watch.stalled(7);
        wait_s(0.3);
        early = early || watch.stalled(7);
        wait_s(0.3);
        check(!early && watch.stalled(7), "gives up once the timeline has not moved for the limit");
    }
    {
        spirula::GpuStallWatch watch(false);
        bool any = watch.stalled(1);
        for (uint64_t v = 2; v < 6; ++v) {
            wait_s(0.2);
            any = any || watch.stalled(v);
        }
        check(!any, "progress between slices restarts the count");
    }
    set_limit("2.5");
    {
        spirula::GpuStallWatch watch(false);
        watch.stalled(3);
        wait_s(3.0);
        check(!watch.stalled(3), "a slice that took longer (a suspend) counts two seconds");
    }
    check(spirula::GpuStallWatch(true).limit_seconds() == 2.5, "SS_GPU_STALL_SECONDS applies to a CPU device too");
    set_limit(nullptr);
    check(spirula::GpuStallWatch(false).limit_seconds() == 60.0 && spirula::GpuStallWatch(true).limit_seconds() == 0.0,
          "a GPU gives up after a minute, a CPU device never");
    std::printf(g_failures ? "FAIL %d\n" : "PASS gpu stall watch\n", g_failures);
    return g_failures ? 1 : 0;
}
