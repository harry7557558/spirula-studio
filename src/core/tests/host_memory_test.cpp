// cgroupMemoryLimit against fake /proc/self/cgroup files and cgroup trees, and
// jobMemoryLimit under a real Windows job.

#include "core/HostMemory.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {
int failures = 0;

void expect(const char* name, size_t got, size_t want) {
    if (got == want) return;
    std::fprintf(stderr, "FAIL %s: got %zu, want %zu\n", name, got, want);
    ++failures;
}
}  // namespace

#ifdef __linux__
namespace {
namespace fs = std::filesystem;

void put(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream(path) << text;
}

spirula::MemoryLimit limit(const fs::path& root) {
    return spirula::cgroupMemoryLimit((root / "cgroup").c_str(), (root / "sys").c_str());
}

void cgroups() {
    const fs::path base = fs::temp_directory_path() / ("ss-host-memory-" + std::to_string(::getpid()));
    fs::remove_all(base);

    // v2: the parent's limit binds; its inactive file cache is reclaimable.
    fs::path r = base / "v2";
    put(r / "cgroup", "0::/job/step\n");
    put(r / "sys/job/memory.max", "1000\n");
    put(r / "sys/job/memory.current", "600\n");
    put(r / "sys/job/memory.stat", "anon 400\ninactive_file 100\nactive_file 100\n");
    put(r / "sys/job/step/memory.max", "max\n");
    put(r / "sys/job/step/memory.current", "550\n");
    expect("v2 parent headroom", limit(r).headroom, 500);
    expect("v2 parent limit", limit(r).limit, 1000);
    expect("v2 no swap files", limit(r).swap, SIZE_MAX);

    // v2: the tighter child wins.
    put(r / "sys/job/step/memory.max", "700\n");
    put(r / "sys/job/step/memory.stat", "inactive_file 0\n");
    expect("v2 child headroom", limit(r).headroom, 150);
    expect("v2 child limit", limit(r).limit, 700);

    // memory.high throttles below memory.max, and usage can pass it.
    put(r / "sys/job/step/memory.high", "600\n");
    expect("v2 high headroom", limit(r).headroom, 50);
    expect("v2 high limit", limit(r).limit, 600);
    put(r / "sys/job/step/memory.high", "500\n");
    expect("v2 past high", limit(r).headroom, 0);

    // Swap: the tightest of what each level's memory.swap.max leaves.
    put(r / "sys/job/memory.swap.max", "300\n");
    put(r / "sys/job/memory.swap.current", "100\n");
    put(r / "sys/job/step/memory.swap.max", "max\n");
    put(r / "sys/job/step/memory.swap.current", "100\n");
    expect("v2 parent swap", limit(r).swap, 200);
    put(r / "sys/job/step/memory.swap.max", "0\n");
    expect("v2 no swap", limit(r).swap, 0);

    // v2 with no limit anywhere.
    r = base / "v2-unlimited";
    put(r / "cgroup", "0::/user.slice/app.scope\n");
    for (const char* dir : {"sys/user.slice", "sys/user.slice/app.scope"}) {
        put(r / dir / "memory.max", "max\n");
        put(r / dir / "memory.high", "max\n");
        put(r / dir / "memory.current", "123\n");
        put(r / dir / "memory.swap.max", "max\n");
        put(r / dir / "memory.swap.current", "0\n");
    }
    expect("v2 unlimited headroom", limit(r).headroom, SIZE_MAX);
    expect("v2 unlimited limit", limit(r).limit, SIZE_MAX);
    expect("v2 unlimited swap", limit(r).swap, SIZE_MAX);

    // A container without a cgroup namespace: the listed path is not under the mount.
    r = base / "v2-no-namespace";
    put(r / "cgroup", "0::/system.slice/docker-abc.scope\n");
    put(r / "sys/memory.max", "4000\n");
    put(r / "sys/memory.current", "1000\n");
    expect("v2 mount root", limit(r).headroom, 3000);

    // Usage past the limit leaves nothing.
    put(r / "sys/memory.current", "5000\n");
    expect("v2 over limit", limit(r).headroom, 0);

    // Hybrid v1 (a Slurm job): the memory controller's hierarchy, its "no limit" sentinel at the root.
    r = base / "v1";
    put(r / "cgroup", "12:cpu,cpuacct:/slurm/job1\n9:memory:/slurm/job1\n0::/user.slice\n");
    put(r / "sys/memory/memory.limit_in_bytes", "9223372036854771712\n");
    put(r / "sys/memory/memory.usage_in_bytes", "99999\n");
    put(r / "sys/memory/memory.memsw.limit_in_bytes", "9223372036854771712\n");
    put(r / "sys/memory/memory.memsw.usage_in_bytes", "99999\n");
    put(r / "sys/memory/slurm/job1/memory.limit_in_bytes", "2000\n");
    put(r / "sys/memory/slurm/job1/memory.usage_in_bytes", "1500\n");
    put(r / "sys/memory/slurm/job1/memory.stat", "inactive_file 50\ntotal_inactive_file 300\n");
    expect("v1 job headroom", limit(r).headroom, 800);
    expect("v1 job limit", limit(r).limit, 2000);
    expect("v1 no memsw", limit(r).swap, SIZE_MAX);

    // memsw caps RAM plus swap: 500 of swap allowed, 100 of it used.
    put(r / "sys/memory/slurm/job1/memory.memsw.limit_in_bytes", "2500\n");
    put(r / "sys/memory/slurm/job1/memory.memsw.usage_in_bytes", "1600\n");
    expect("v1 memsw swap", limit(r).swap, 400);

    // No cgroup file at all.
    const spirula::MemoryLimit none = spirula::cgroupMemoryLimit((base / "none").c_str(), "/nonexistent");
    expect("missing headroom", none.headroom, SIZE_MAX);
    expect("missing limit", none.limit, SIZE_MAX);

    fs::remove_all(base);
}
}  // namespace
#endif

#ifdef _WIN32
namespace {
// A job's commit limit, which GlobalMemoryStatusEx leaves out.
void job() {
    constexpr size_t kLimit = 3ull << 30, kBlock = 1ull << 30, kSlack = 16ull << 20;
    HANDLE handle = CreateJobObjectA(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_JOB_MEMORY;
    info.JobMemoryLimit = kLimit;
    if (!handle || !SetInformationJobObject(handle, JobObjectExtendedLimitInformation, &info, sizeof info) ||
        !AssignProcessToJobObject(handle, GetCurrentProcess())) {
        std::fprintf(stderr, "FAIL job setup: error %lu\n", GetLastError());
        ++failures;
        return;
    }
    expect("job limit", spirula::jobMemoryLimit().limit, kLimit);
    expect("job total", spirula::totalRamBytes(), std::min(kLimit, spirula::physicalRamBytes()));
    const size_t before = spirula::jobMemoryLimit().headroom;
    void* block = VirtualAlloc(nullptr, kBlock, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    const size_t after = spirula::jobMemoryLimit().headroom, commit = spirula::availableCommitBytes();
    if (!block || before < after + kBlock - kSlack || before > after + kBlock + kSlack || commit > after) {
        std::fprintf(stderr, "FAIL job headroom: %zu before, %zu after a 1 GiB commit, commit %zu\n",
                     before, after, commit);
        ++failures;
    }
    if (block) VirtualFree(block, 0, MEM_RELEASE);
}
}  // namespace
#endif

int main() {
#ifdef __linux__
    cgroups();
#endif
#ifdef _WIN32
    job();
#endif
    // 0 is "cannot say" to callers, so a spent limit must not read as one.
    using spirula::detail::capped;
    expect("no limit", capped(100, SIZE_MAX), 100);
    expect("unknown, no limit", capped(0, SIZE_MAX), 0);
    expect("limit binds", capped(100, 40), 40);
    expect("limit spent", capped(100, 0), 1);
    expect("unknown, limited", capped(0, 70), 70);

#ifndef _WIN32
    const spirula::HostMemory host = spirula::hostMemory();
    expect("commit is available", host.commit, host.available);
#endif
    const spirula::MemoryLimit here = spirula::memoryLimit();
    if (here.limit == SIZE_MAX) std::printf("this process: no memory limit\n");
    else std::printf("this process: %.2f GiB free under a %.2f GiB memory limit\n",
                     here.headroom / 1073741824.0, here.limit / 1073741824.0);
    std::printf("total %.2f GiB, available %.2f GiB, commit %.2f GiB, swap %.2f GiB\n",
                spirula::totalRamBytes() / 1073741824.0, spirula::availableRamBytes() / 1073741824.0,
                spirula::availableCommitBytes() / 1073741824.0, spirula::availableSwapBytes() / 1073741824.0);
    std::printf(failures ? "host_memory_test: %d failures\n" : "host_memory_test: passed\n", failures);
    return failures ? 1 : 0;
}
