#pragma once

// What a child process says about itself while it runs, for the GUI that
// started it: how far it is, the memory it holds, and the peak it plans for.
// One JSON file in the child's --progress-dir, replaced whole twice a second
// by RunReporter. Dense and meshing write it; app/gui/RunMemoryView.h reads it.

#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <string>
#include <thread>

namespace spirula {

// Bytes; a mean of 0 is no plan. A `ceiling` bounds the peak rather than
// predicting it: budgets that may never fill.
struct PlannedPeak {
    double mean = 0.0, sigma = 0.0;
    bool ceiling = false;
};

struct RunReport {
    uint64_t sequence = 0;
    std::string phase;
    double work = -1.0;          // 0..1 of the run, -1 when it cannot say
    uint64_t host_process = 0, host_available = 0, host_total = 0, host_commit_free = 0;
    PlannedPeak host_peak;       // of host_process
    bool device_known = false;
    uint64_t device_ours = 0, device_others = 0, device_capacity = 0;
    PlannedPeak device_peak;     // of device_ours
};

std::filesystem::path run_report_path(const std::filesystem::path& dir);
void write_run_report(const std::filesystem::path& dir, const RunReport& r);
bool read_run_report(const std::filesystem::path& dir, RunReport& r);

// Samples this process's RAM twice a second and writes the report; the owner
// sets everything else. A no-op with an empty directory.
class RunReporter {
public:
    struct Device { uint64_t ours = 0, others = 0, capacity = 0; };
    // `device`, when given, is asked on the reporter's thread before every write.
    explicit RunReporter(const std::string& dir, std::function<bool(Device&)> device = {});
    ~RunReporter() { stop(); }
    RunReporter(const RunReporter&) = delete;
    RunReporter& operator=(const RunReporter&) = delete;

    void phase(const char* name);
    void work(double w);
    void host_peak(PlannedPeak p);
    void device(const Device& d);
    void device_peak(PlannedPeak p);
    void stop();

private:
    void run();

    std::filesystem::path _dir;
    std::function<bool(Device&)> _device;
    RunReport _r;
    std::mutex _mu;
    std::condition_variable _wake;
    bool _quit = false;
    std::thread _thread;
};

}  // namespace spirula
