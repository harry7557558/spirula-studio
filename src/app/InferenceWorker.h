#pragma once

#include "app/gui/Subprocess.h"
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace app {

// Each child has one outstanding task; acceptance belongs to the coordinator.
struct WorkerTask {
    std::string request;
    uint64_t host_bytes = 0;
};

// Call before model initialization: preserves a binary protocol stdout and sends
// ordinary library stdout logging to stderr. False means the worker cannot run.
bool worker_begin();
bool worker_read_frame(std::string& frame);
bool worker_write_frame(const std::string& frame);

bool run_inference_workers(
    const std::vector<std::vector<std::string>>& commands,
    const std::vector<WorkerTask>& tasks, uint64_t max_host_bytes,
    const std::atomic<bool>& cancel,
    const std::function<bool(size_t, const std::string&, std::string&)>& accept,
    const std::function<void(const std::string&)>& log, std::string& error);

}  // namespace app
