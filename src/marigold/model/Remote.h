#pragma once
// A byte range of a published file, read through the system curl as a stream,
// so a 10 GB shard is converted as it arrives and never stored. A thread keeps
// the connection draining while the caller converts what has arrived, and a
// broken connection resumes where it stopped, on the other host.

#include "marigold/model/Fetch.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace marigold {

// Thrown when `cancel` is set; the caller keeps what it has and resumes later.
struct Cancelled : std::exception {
    const char* what() const noexcept override { return "cancelled"; }
};

class RemoteRange {
public:
    // Bytes [begin, end) of `f`. `cancel` may be null.
    RemoteRange(const RemoteFile& f, uint64_t begin, uint64_t end,
                const std::atomic<bool>* cancel);
    ~RemoteRange();
    RemoteRange(const RemoteRange&) = delete;
    RemoteRange& operator=(const RemoteRange&) = delete;

    // Exactly `n` bytes, or throws: nn::Error naming both hosts, or Cancelled.
    void read(void* dst, size_t n);
    uint64_t pos() const { return pos_; }

private:
    void fetch();

    const RemoteFile& f_;
    uint64_t pos_, end_;
    const std::atomic<bool>* cancel_;
    std::atomic<bool> stop_{false};
    std::mutex mu_;
    std::condition_variable cv_;
    std::deque<std::vector<uint8_t>> chunks_;
    size_t head_ = 0;         // bytes of chunks_.front() already handed out
    uint64_t buffered_ = 0;
    bool done_ = false;
    std::exception_ptr error_;
    std::thread worker_;
};

// The safetensors header of `f`: the JSON, and where the data section starts.
std::string remote_safetensors_header(const RemoteFile& f, uint64_t* data_offset,
                                      const std::atomic<bool>* cancel);

}  // namespace marigold
