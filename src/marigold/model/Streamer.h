#pragma once
// The 60 transformer blocks on the device. As many stay resident as the VRAM
// plan allows; the rest are streamed through two slots, each read from disk
// into pinned memory by a thread and copied on the transfer queue
// (nn/vk/Transfer.h) while the block before it computes. A block is its NF4
// weights from the base file plus the LoRA of the task in hand, one buffer.

#include "marigold/model/Fetch.h"
#include "marigold/model/Layout.h"
#include "nn/Tensor.h"
#include "nn/io/Safetensors.h"
#include "nn/vk/Transfer.h"

#include <condition_variable>
#include <cstdio>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace marigold {

struct BlockLin {
    nn::Tensor q4, absmax, bias, lora_a, lora_b;
};

struct BlockView {
    BlockLin lin[kLinCount];
    nn::Tensor norm[4];   // kBlockNorms order
};

class Streamer {
public:
    Streamer();
    ~Streamer();

    void addBase(const nn::SafetensorsFile& base);
    void addTask(Task t, const nn::SafetensorsFile& task);

    // One block's buffer: its NF4 part and the larger of the tasks' LoRA parts.
    uint64_t unitBytes() const { return base_bytes_ + lora_bytes_; }
    // Keeps as many blocks resident as `bytes` holds beside the two streaming
    // slots; frees or uploads the difference. Returns the bytes now held.
    uint64_t plan(uint64_t bytes);
    int resident() const { return resident_; }
    uint64_t deviceBytes() const;

    // A pass runs blocks 0..59 in order: acquire before a block's first
    // dispatch, release after its last.
    void beginPass(Task t);
    BlockView acquire(int i);
    void release(int i);

private:
    struct Part {
        std::FILE* file = nullptr;
        uint64_t offset = 0, bytes = 0;   // in the file
    };
    struct Job {
        int block = -1, staging = -1;
        bool ready = false;
        uint64_t ticket = 0;              // the transfer's, once issued
    };

    void openFile(const std::string& path, std::FILE** f);
    void layout(const nn::SafetensorsFile& f, bool lora, Part* parts);
    BlockView view(nn::DevicePtr unit, Task t, int i) const;
    void readUnit(int i, Task t, uint8_t* dst, bool base, bool lora);
    void loader();
    void issue(size_t s);

    std::FILE* base_file_ = nullptr;
    std::FILE* task_file_[2] = {};
    Part base_parts_[kLayers], lora_parts_[2][kLayers];
    // Tensor offsets inside a unit, by name, for each source.
    std::vector<std::pair<std::string, uint64_t>> base_names_[kLayers];
    std::vector<std::pair<std::string, uint64_t>> lora_names_[2][kLayers];
    uint64_t base_bytes_ = 0, lora_bytes_ = 0;

    std::vector<nn::DevicePtr> resident_units_;
    int resident_ = 0;
    Task resident_task_ = Task::Depth;
    bool resident_lora_ = false;

    std::unique_ptr<nn::vk::Transfer> transfer_;
    nn::DevicePtr slots_[2] = {}, staging_[2] = {};
    uint8_t* staging_map_[2] = {};
    uint64_t slot_free_after_[2] = {}, staging_free_after_[2] = {};

    // The streamed blocks of the passes begun, in order.
    std::mutex mu_;
    std::condition_variable cv_;
    std::deque<Job> jobs_;
    size_t jobs_base_ = 0;    // sequence number of jobs_.front()
    size_t next_read_ = 0, next_issue_ = 0, next_acquire_ = 0;
    Task pass_task_ = Task::Depth;
    std::vector<Task> job_task_;
    bool stop_ = false;
    std::thread thread_;
};

}  // namespace marigold
