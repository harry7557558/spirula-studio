#include "app/InferenceWorker.h"

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <memory>
#include <mutex>
#include <thread>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace app {
namespace {
constexpr uint32_t kMaxFrame = 1024 * 1024;
FILE* protocol_out = nullptr;

template<class Read> bool receive(Read read, std::string& text) {
    auto exact = [&](void* data, size_t size) {
        auto* p = static_cast<unsigned char*>(data);
        while (size) {
            const size_t n = read(p, size);
            if (!n || n > size) return false;
            p += n;
            size -= n;
        }
        return true;
    };
    std::array<unsigned char, 4> header;
    if (!exact(header.data(), header.size())) return false;
    uint32_t size = 0;
    for (unsigned i = 0; i < 4; ++i) size |= uint32_t(header[i]) << (8 * i);
    if (!size || size > kMaxFrame) return false;
    text.resize(size);
    return exact(text.data(), size);
}

template<class Write> bool send(Write write, const std::string& text) {
    if (text.empty() || text.size() > kMaxFrame) return false;
    std::array<unsigned char, 4> header;
    const auto size = static_cast<uint32_t>(text.size());
    for (unsigned i = 0; i < 4; ++i) header[i] = static_cast<unsigned char>(size >> (8 * i));
    return write(header.data(), header.size()) && write(text.data(), text.size());
}
}  // namespace

bool worker_begin() {
    if (protocol_out) return false;
    std::fflush(stdout);
#ifdef _WIN32
    const int fd = _dup(_fileno(stdout));
    if (fd < 0) return false;
    // _fdopen does not change the text mode inherited by _dup.
    if (_setmode(fd, _O_BINARY) < 0) { _close(fd); return false; }
    protocol_out = _fdopen(fd, "wb");
    if (!protocol_out) { _close(fd); return false; }
    if (_setmode(_fileno(stdin), _O_BINARY) < 0 ||
        _dup2(_fileno(stderr), _fileno(stdout)) < 0) return false;
#else
    const int fd = dup(fileno(stdout));
    if (fd < 0) return false;
    protocol_out = fdopen(fd, "wb");
    if (!protocol_out) { close(fd); return false; }
    if (dup2(fileno(stderr), fileno(stdout)) < 0) return false;
#endif
    return true;
}

bool worker_read_frame(std::string& frame) {
    return receive([](void* p, size_t n) { return std::fread(p, 1, n, stdin); }, frame);
}

bool worker_write_frame(const std::string& frame) {
    return protocol_out && send([](const void* p, size_t n) {
        return std::fwrite(p, 1, n, protocol_out) == n;
    }, frame) && std::fflush(protocol_out) == 0;
}

bool run_inference_workers(
    const std::vector<std::vector<std::string>>& commands,
    const std::vector<WorkerTask>& tasks, uint64_t max_host_bytes,
    const std::atomic<bool>& cancel,
    const std::function<bool(size_t, const std::string&, std::string&)>& accept,
    const std::function<void(const std::string&)>& log, std::string& error) {
    if (tasks.empty()) return true;
    if (commands.empty() || !max_host_bytes) { error = "No inference worker resources admitted"; return false; }
    for (const auto& task : tasks) {
        if (!task.host_bytes || task.host_bytes > max_host_bytes || task.request.empty() || task.request.size() > kMaxFrame) {
            error = "Inference task exceeds its host-byte or protocol budget";
            return false;
        }
    }
    std::mutex mutex, log_mutex;
    std::condition_variable space;
    std::atomic<bool> failed{false}, finished{false};
    std::vector<std::unique_ptr<gui::ProcessDuplex>> children;
    auto fail = [&](const std::string& why) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!failed.exchange(true)) error = why;
        space.notify_all();
    };
    for (size_t i = 0; i < commands.size() && i < tasks.size(); ++i) {
        auto child = std::make_unique<gui::ProcessDuplex>();
        if (!child->start(commands[i], [&](const std::string& line) {
            std::lock_guard<std::mutex> lock(log_mutex);
            if (log) log(line);
        })) { error = "Could not start inference worker"; return false; }
        children.push_back(std::move(child));
    }
    std::vector<std::thread> workers;
    workers.reserve(children.size());
    std::thread cancellation([&] {
        while (!finished.load()) {
            if (cancel.load() || failed.load()) {
                for (auto& child : children) child->kill();
                space.notify_all();
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });
    size_t next = 0;
    uint64_t resident = 0;
    try {
    for (auto& owned : children) {
        auto* child = owned.get();
        workers.emplace_back([&, child] {
            try {
                for (;;) {
                    size_t index;
                    {
                        std::unique_lock<std::mutex> lock(mutex);
                        space.wait(lock, [&] { return cancel.load() || failed.load() || next == tasks.size() ||
                            tasks[next].host_bytes <= max_host_bytes - resident; });
                        if (cancel.load() || failed.load() || next == tasks.size()) break;
                        index = next++;
                        resident += tasks[index].host_bytes;
                    }
                    std::string response;
                    if (!send([&](const void* p, size_t n) { return child->write(p, n); }, tasks[index].request) ||
                        !receive([&](void* p, size_t n) { return child->read(p, n); }, response)) {
                        fail("Inference worker died or returned an invalid/incomplete frame");
                        break;
                    }
                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        if (!cancel.load() && !failed.load()) {
                            std::string why;
                            if (!accept(index, response, why)) {
                                failed = true;
                                error = why.empty() ? "Inference output family was rejected" : why;
                            }
                        }
                        resident -= tasks[index].host_bytes;
                    }
                    space.notify_all();
                }
                if (child->finish() != 0 && !cancel.load()) fail("Inference worker exited unsuccessfully");
            } catch (const std::exception& e) { fail(e.what()); }
        });
    }
    } catch (const std::exception& e) {
        fail(e.what());
    }
    for (auto& worker : workers) worker.join();
    finished = true;
    cancellation.join();
    if (cancel.load()) { error = "Inference canceled"; return false; }
    return !failed.load();
}
}  // namespace app
