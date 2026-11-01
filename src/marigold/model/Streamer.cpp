#include "marigold/model/Streamer.h"

#include "marigold/Common.h"
#include "nn/vk/Memory.h"
#include "nn/vk/Stream.h"

#include <algorithm>

namespace marigold {
namespace {

constexpr uint64_t kAlign = 256;

uint64_t align_up(uint64_t v) { return (v + kAlign - 1) / kAlign * kAlign; }

bool seek(std::FILE* f, uint64_t off) {
#ifdef _WIN32
    return _fseeki64(f, (long long)off, SEEK_SET) == 0;
#else
    return fseeko(f, (off_t)off, SEEK_SET) == 0;
#endif
}

std::vector<std::string> base_tensor_names(int i) {
    std::vector<std::string> v;
    for (const BlockLinear& L : kBlockLinears)
        for (const char* s : {".q4", ".absmax", ".bias"}) v.push_back(block_tensor(i, L.name, s));
    for (const char* n : kBlockNorms) v.push_back(block_tensor(i, n));
    return v;
}

std::vector<std::string> lora_tensor_names(int i) {
    std::vector<std::string> v;
    for (const BlockLinear& L : kBlockLinears)
        for (const char* s : {".lora_a", ".lora_b"}) v.push_back(block_tensor(i, L.name, s));
    return v;
}

}  // namespace

Streamer::Streamer() = default;

Streamer::~Streamer() {
    {
        std::lock_guard<std::mutex> lk(mu_);
        stop_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
    transfer_.reset();
    for (nn::DevicePtr p : resident_units_) vk::Allocator::get().free(p);
    for (int k = 0; k < 2; ++k) {
        vk::Allocator::get().free(slots_[k]);
        vk::Allocator::get().free(staging_[k]);
    }
    for (std::FILE* f : {base_file_, task_file_[0], task_file_[1]})
        if (f) std::fclose(f);
}

void Streamer::openFile(const std::string& path, std::FILE** f) {
    if (*f) std::fclose(*f);
    *f = std::fopen(path.c_str(), "rb");
    NN_CHECK(*f, "cannot open '%s'", path.c_str());
}

void Streamer::layout(const nn::SafetensorsFile& f, bool lora, Part* parts) {
    for (int i = 0; i < kLayers; ++i) {
        const std::vector<std::string> names = lora ? lora_tensor_names(i) : base_tensor_names(i);
        uint64_t lo = UINT64_MAX, hi = 0, sum = 0;
        for (const std::string& n : names) {
            const nn::SafetensorsFile::Entry& e = f.entry(n);
            lo = std::min(lo, e.begin);
            hi = std::max(hi, e.end);
            sum += align_up(e.end - e.begin);
        }
        // Contiguous, with nothing but alignment filler between: one read, one copy.
        NN_CHECK(hi - lo <= sum, "%s: block %d is not stored contiguously", f.path().c_str(), i);
        parts[i] = {nullptr, f.dataOffset() + lo, hi - lo};
        auto& offs = lora ? lora_names_[f.metadata().at("task") == "depth" ? 0 : 1][i]
                          : base_names_[i];
        offs.clear();
        for (const std::string& n : names) offs.emplace_back(n, f.entry(n).begin - lo);
    }
}

void Streamer::addBase(const nn::SafetensorsFile& base) {
    openFile(base.path(), &base_file_);
    layout(base, false, base_parts_);
    base_bytes_ = 0;
    for (const Part& p : base_parts_) base_bytes_ = std::max(base_bytes_, align_up(p.bytes));
    if (!transfer_) transfer_ = std::make_unique<nn::vk::Transfer>();
    if (!thread_.joinable()) thread_ = std::thread([this] { loader(); });
}

void Streamer::addTask(Task t, const nn::SafetensorsFile& task) {
    const int k = (int)t;
    openFile(task.path(), &task_file_[k]);
    layout(task, true, lora_parts_[k]);
    for (const Part& p : lora_parts_[k]) lora_bytes_ = std::max(lora_bytes_, align_up(p.bytes));
}

BlockView Streamer::view(nn::DevicePtr unit, Task t, int i) const {
    std::unordered_map<std::string, nn::DevicePtr> at;
    for (const auto& [n, off] : base_names_[i]) at[n] = unit + off;
    for (const auto& [n, off] : lora_names_[(int)t][i]) at[n] = unit + base_bytes_ + off;
    BlockView v;
    for (int l = 0; l < kLinCount; ++l) {
        const BlockLinear& L = kBlockLinears[l];
        BlockLin& b = v.lin[l];
        b.q4 = nn::Tensor(at.at(block_tensor(i, L.name, ".q4")), nn::DType::U8, L.N * L.K / 2);
        b.absmax = nn::Tensor(at.at(block_tensor(i, L.name, ".absmax")), nn::DType::F16,
                              L.N * L.K / 64);
        b.bias = nn::Tensor(at.at(block_tensor(i, L.name, ".bias")), nn::DType::F32, L.N);
        b.lora_a = nn::Tensor(at.at(block_tensor(i, L.name, ".lora_a")), nn::DType::F16,
                              L.parts * L.K, kRank);
        b.lora_b = nn::Tensor(at.at(block_tensor(i, L.name, ".lora_b")), nn::DType::F16, L.N,
                              kRank);
    }
    for (int n = 0; n < 4; ++n)
        v.norm[n] = nn::Tensor(at.at(block_tensor(i, kBlockNorms[n])), nn::DType::F32, kHeadDim);
    return v;
}

void Streamer::readUnit(int i, Task t, uint8_t* dst, bool base, bool lora) {
    auto rd = [&](std::FILE* f, const Part& p, uint8_t* out) {
        NN_CHECK(f && seek(f, p.offset) && std::fread(out, 1, (size_t)p.bytes, f) == p.bytes,
                 "short read of transformer block %d", i);
    };
    if (base) rd(base_file_, base_parts_[i], dst);
    if (lora) rd(task_file_[(int)t], lora_parts_[(int)t][i], dst + base_bytes_);
}

uint64_t Streamer::plan(uint64_t bytes) {
    const uint64_t unit = unitBytes();
    int want = (int)std::min<uint64_t>(kLayers, bytes / unit);
    if (want < kLayers) want = (int)std::max<int64_t>(0, (int64_t)(bytes / unit) - 2);
    NN_CHECK(want == kLayers || bytes >= 2 * unit,
             "Marigold v2 needs %.0f MB of GPU memory for its weights alone, and the plan "
             "left %.0f MB", 2.0 * unit / 1048576.0, bytes / 1048576.0);
    while ((int)resident_units_.size() > want) {
        vk::Allocator::get().free(resident_units_.back());
        resident_units_.pop_back();
    }
    if ((int)resident_units_.size() < want) {
        std::vector<uint8_t> host((size_t)unit);
        for (int i = (int)resident_units_.size(); i < want; ++i) {
            const nn::DevicePtr p = vk::Allocator::get().allocDevice(unit, "marigold.block");
            readUnit(i, resident_task_, host.data(), true, resident_lora_);
            vk::Stream::get().upload(p, host.data(), resident_lora_ ? unit : base_bytes_);
            resident_units_.push_back(p);
        }
    }
    resident_ = want;
    if (resident_ < kLayers && !slots_[0])
        for (int k = 0; k < 2; ++k) {
            slots_[k] = vk::Allocator::get().allocDeviceShared(unit, "marigold.slot");
            void* map = nullptr;
            staging_[k] = vk::Allocator::get().allocHost(unit, &map, "marigold.staging");
            staging_map_[k] = static_cast<uint8_t*>(map);
        }
    NN_LOG_INFO("[marigold] %d of %d blocks resident (%.1f GB), %s\n", resident_, kLayers,
                resident_ * (double)unit / 1e9,
                resident_ == kLayers ? "nothing streamed"
                : transfer_->overlaps() ? "the rest streamed beside the compute"
                                        : "the rest streamed between blocks: no transfer queue");
    return deviceBytes();
}

uint64_t Streamer::deviceBytes() const {
    return (resident_units_.size() + (slots_[0] ? 2 : 0)) * unitBytes();
}

void Streamer::beginPass(Task t) {
    if (resident_ && (!resident_lora_ || resident_task_ != t)) {
        std::vector<uint8_t> host((size_t)unitBytes());
        for (int i = 0; i < resident_; ++i) {
            readUnit(i, t, host.data(), false, true);
            vk::Stream::get().upload(resident_units_[(size_t)i] + base_bytes_,
                                     host.data() + base_bytes_, lora_bytes_);
        }
        resident_lora_ = true;
    }
    resident_task_ = t;
    pass_task_ = t;
    std::lock_guard<std::mutex> lk(mu_);
    for (int i = resident_; i < kLayers; ++i) {
        Job j;
        j.block = i;
        j.staging = (int)((jobs_base_ + jobs_.size()) % 2);
        jobs_.push_back(j);
        job_task_.push_back(t);
    }
    cv_.notify_all();
}

void Streamer::loader() {
    for (;;) {
        size_t s;
        int block;
        Task task;
        uint64_t wait_ticket = 0;
        {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait(lk, [&] {
                return stop_ || (next_read_ < jobs_base_ + jobs_.size() &&
                                 (next_read_ < 2 || next_issue_ > next_read_ - 2));
            });
            if (stop_) return;
            s = next_read_;
            block = jobs_[s - jobs_base_].block;
            task = job_task_[s - jobs_base_];
            wait_ticket = staging_free_after_[s % 2];
        }
        try {
            if (wait_ticket) transfer_->wait(wait_ticket);
            readUnit(block, task, staging_map_[s % 2], true, true);
        } catch (const std::exception& e) {
            NN_LOG_ERROR("[marigold] %s\n", e.what());
            std::lock_guard<std::mutex> lk(mu_);
            stop_ = true;
            cv_.notify_all();
            return;
        }
        std::lock_guard<std::mutex> lk(mu_);
        jobs_[s - jobs_base_].ready = true;
        ++next_read_;
        cv_.notify_all();
    }
}

void Streamer::issue(size_t s) {
    Job& j = jobs_[s - jobs_base_];
    const int k = (int)(s % 2);
    j.ticket = transfer_->copy({{slots_[k], staging_[k], unitBytes()}}, slot_free_after_[k]);
    staging_free_after_[k] = j.ticket;
    ++next_issue_;
}

BlockView Streamer::acquire(int i) {
    if (i < resident_) return view(resident_units_[(size_t)i], pass_task_, i);
    std::unique_lock<std::mutex> lk(mu_);
    const size_t s = next_acquire_;
    NN_CHECK(s < jobs_base_ + jobs_.size() && jobs_[s - jobs_base_].block == i,
             "transformer block %d acquired out of order", i);
    while (next_issue_ <= s) {
        cv_.wait(lk, [&] { return stop_ || jobs_[next_issue_ - jobs_base_].ready; });
        NN_CHECK(!stop_, "reading the transformer blocks failed (see above)");
        issue(next_issue_);
    }
    cv_.notify_all();
    const uint64_t ticket = jobs_[s - jobs_base_].ticket;
    const Task t = job_task_[s - jobs_base_];
    lk.unlock();
    transfer_->streamWaits(ticket);
    return view(slots_[s % 2], t, i);
}

void Streamer::release(int i) {
    if (i < resident_) return;
    vk::Stream::get().flush();
    std::lock_guard<std::mutex> lk(mu_);
    const size_t s = next_acquire_++;
    slot_free_after_[s % 2] = vk::Stream::get().lastSubmitted();
    // The next block into this slot can start copying as soon as it is read.
    while (next_issue_ < jobs_base_ + jobs_.size() && next_issue_ <= s + 2 &&
           jobs_[next_issue_ - jobs_base_].ready)
        issue(next_issue_);
    while (!jobs_.empty() && jobs_base_ + 2 < next_issue_ && jobs_base_ < s) {
        jobs_.pop_front();
        job_task_.erase(job_task_.begin());
        ++jobs_base_;
    }
    cv_.notify_all();
}

}  // namespace marigold
