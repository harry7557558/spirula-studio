#pragma once
// Builds the installed form of Marigold v2 from the published checkpoints as
// they stream in (model/Remote.h). Each bf16 tensor is quantized to NF4, or
// spent on one of the constants the network only ever sees one value of, the
// moment it arrives: the disk holds the 7 GB result and never a 10 GB shard,
// and an interrupted install resumes at the tensor it stopped on.
// src/marigold/README.md has what the files hold.

#include "marigold/model/Fetch.h"

#include <atomic>
#include <cstdint>
#include <functional>

namespace marigold {

bool base_installed();
bool task_installed(Task t);

// Left free past what an install writes: the rest of the system needs a disk too.
constexpr uint64_t kDiskSpare = 1ull << 30;

// What an install of these tasks still has to do, against what the disk has.
struct InstallNeeds {
    uint64_t download = 0;
    uint64_t disk = 0;
    uint64_t free = 0;
    bool fits() const;
};
InstallNeeds install_needs(bool depth, bool normals);

// Bytes streamed so far, of the total this call streams.
using InstallProgress = std::function<void(uint64_t done, uint64_t total)>;

// Throws nn::Error, or Cancelled once `cancel` is set (model/Remote.h); either
// way what was converted stays, and the next call continues from it. Asking
// first is the caller's: SS_NO_AUTO_FETCH is not consulted here.
void install(bool depth, bool normals, const InstallProgress& progress,
             const std::atomic<bool>* cancel);

}  // namespace marigold
