#pragma once
// How much memory this process can use. A memory limit -- a cgroup on Linux (a
// container, a Slurm job, systemd-run), a job object on Windows -- caps what the
// machine has, and every number here except physicalRamBytes and processRamBytes
// counts it. 0 when the platform will not say: callers that need a working
// number must supply their own fallback.

#include <algorithm>
#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN   // rpcndr.h's `#define small char` reaches far from here
#endif
#include <windows.h>
#include <psapi.h>
#else
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#endif

namespace spirula {

// A memory limit as this process meets it; SIZE_MAX where none applies.
struct MemoryLimit {
    size_t limit = SIZE_MAX;     // the most the process and the rest of its group may hold
    size_t headroom = SIZE_MAX;  // what new allocations may still take
    size_t swap = SIZE_MAX;      // swap the group may still use (Linux)
};

// The machine's RAM, to describe the machine; a budget wants totalRamBytes.
inline size_t physicalRamBytes() {
#if defined(_WIN32)
    MEMORYSTATUSEX st{};
    st.dwLength = sizeof(st);
    if (GlobalMemoryStatusEx(&st)) return (size_t)st.ullTotalPhys;
#elif defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
    long pages = sysconf(_SC_PHYS_PAGES), page = sysconf(_SC_PAGESIZE);
    if (pages > 0 && page > 0) return (size_t)pages * (size_t)page;
#endif
    return 0;
}

#if defined(__linux__)
// One /proc/meminfo field, e.g. "SwapFree: %llu kB"; 0 when absent.
inline size_t meminfoBytes(const char* format) {
    size_t bytes = 0;
    if (FILE* file = std::fopen("/proc/meminfo", "r")) {
        char line[128];
        unsigned long long kb = 0;
        bool found = false;
        while (!found && std::fgets(line, sizeof line, file))
            found = std::sscanf(line, format, &kb) == 1;
        std::fclose(file);
        if (found) bytes = (size_t)kb * 1024;
    }
    return bytes;
}

namespace detail {
// The number in a cgroup file, or after "<key> " in one; false when absent or "max".
inline bool cgroupNumber(const std::string& path, const char* key, unsigned long long& out) {
    FILE* file = std::fopen(path.c_str(), "r");
    if (!file) return false;
    const size_t key_len = key ? std::strlen(key) : 0;
    char line[256];
    bool found = false;
    while (!found && std::fgets(line, sizeof line, file)) {
        if (key && (std::strncmp(line, key, key_len) != 0 || line[key_len] != ' ')) continue;
        found = std::sscanf(line + (key ? key_len + 1 : 0), "%llu", &out) == 1;
        if (!key) break;
    }
    std::fclose(file);
    return found;
}
}  // namespace detail

// The tightest limit over this process's cgroup and its ancestors: memory.max, or
// memory.high past which the kernel throttles, less usage net of inactive file cache,
// and what memory.swap.max leaves; v1's limit_in_bytes and memsw.limit_in_bytes alike.
inline MemoryLimit cgroupMemoryLimit(const char* self_cgroup = "/proc/self/cgroup",
                                     const char* mount = "/sys/fs/cgroup") {
    std::string root, path;
    bool v1 = false;
    if (FILE* file = std::fopen(self_cgroup, "r")) {
        char line[4096];
        while (std::fgets(line, sizeof line, file)) {
            line[std::strcspn(line, "\n")] = 0;
            char* first = std::strchr(line, ':');
            char* second = first ? std::strchr(first + 1, ':') : nullptr;
            if (!second) continue;
            const std::string controllers = "," + std::string(first + 1, second) + ",";
            // A hybrid host lists both; only the v1 hierarchy then has the memory controller.
            if (controllers.find(",memory,") != std::string::npos) {
                root = std::string(mount) + "/memory";
                path = second + 1;
                v1 = true;
                break;
            }
            if (controllers == ",," && std::strncmp(line, "0:", 2) == 0) {
                root = mount;
                path = second + 1;
            }
        }
        std::fclose(file);
    }
    MemoryLimit m;
    if (root.empty()) return m;
    std::string dir = root + path;
    while (dir.size() > root.size() && dir.back() == '/') dir.pop_back();
    const char* usage_file = v1 ? "memory.usage_in_bytes" : "memory.current";
    unsigned long long probe = 0;
    // Without its own cgroup namespace a container sees a host path it cannot open.
    if (!detail::cgroupNumber(dir + "/" + usage_file, nullptr, probe)) dir = root;
    const auto left = [](unsigned long long limit, unsigned long long used) {
        return limit > used ? (size_t)(limit - used) : (size_t)0;
    };
    for (;;) {
        // v1 writes "no limit" as a number near 2^63.
        const auto read = [&](const char* file, unsigned long long& out) {
            return detail::cgroupNumber(dir + "/" + file, nullptr, out) && out < (1ull << 62);
        };
        unsigned long long limit = 0, high = 0, used = 0, cache = 0, swap_limit = 0, swap_used = 0;
        bool limited = read(v1 ? "memory.limit_in_bytes" : "memory.max", limit);
        if (!v1 && read("memory.high", high) && (!limited || high < limit)) limit = high, limited = true;
        if (limited && read(usage_file, used)) {
            detail::cgroupNumber(dir + "/memory.stat", v1 ? "total_inactive_file" : "inactive_file", cache);
            m.limit = std::min(m.limit, (size_t)limit);
            m.headroom = std::min(m.headroom, left(limit, used > cache ? used - cache : 0));
            // v1 caps RAM plus swap; what it leaves past the RAM limit is the swap.
            if (v1 && read("memory.memsw.limit_in_bytes", swap_limit) &&
                read("memory.memsw.usage_in_bytes", swap_used))
                m.swap = std::min(m.swap, left(swap_limit - std::min(swap_limit, limit),
                                               swap_used - std::min(swap_used, used)));
        }
        if (!v1 && read("memory.swap.max", swap_limit) && read("memory.swap.current", swap_used))
            m.swap = std::min(m.swap, left(swap_limit, swap_used));
        if (dir.size() <= root.size()) break;
        dir.erase(dir.rfind('/'));
    }
    return m;
}
#endif

#if defined(_WIN32)
// This process's job object's commit limit, which Windows enforces, on device memory
// too, but GlobalMemoryStatusEx does not report. A nested job hides the limits of the
// jobs around it, so a process the app starts in its own job sees none.
inline MemoryLimit jobMemoryLimit() {
    MemoryLimit m;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
    if (!QueryInformationJobObject(nullptr, JobObjectExtendedLimitInformation, &info, sizeof info, nullptr))
        return m;
    PROCESS_MEMORY_COUNTERS_EX self{};
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&self, sizeof self);
    const auto bound = [&](size_t limit, size_t used) {
        m.limit = std::min(m.limit, limit);
        m.headroom = std::min(m.headroom, limit > used ? limit - used : 0);
    };
    const DWORD flags = info.BasicLimitInformation.LimitFlags;
    if (flags & JOB_OBJECT_LIMIT_PROCESS_MEMORY) bound(info.ProcessMemoryLimit, self.PrivateUsage);
    if (flags & JOB_OBJECT_LIMIT_JOB_MEMORY) {
        JOBOBJECT_LIMIT_VIOLATION_INFORMATION job{};
        // JobMemory is the job's commit now, whether or not the limit was hit.
        QueryInformationJobObject(nullptr, JobObjectLimitViolationInformation, &job, sizeof job, nullptr);
        bound(info.JobMemoryLimit, std::max<size_t>((size_t)job.JobMemory, self.PrivateUsage));
    }
    return m;
}
#endif

inline MemoryLimit memoryLimit() {
#if defined(__linux__)
    return cgroupMemoryLimit();
#elif defined(_WIN32)
    return jobMemoryLimit();
#else
    return {};
#endif
}

namespace detail {
// The smaller of what the machine says and what the limit allows. 0 means "cannot
// say" to callers, so a spent limit leaves one byte rather than none.
inline size_t capped(size_t machine, size_t limit) {
    if (limit == SIZE_MAX) return machine;
    return std::max<size_t>(1, machine ? std::min(machine, limit) : limit);
}
}  // namespace detail

// What this process can use, from one look at the machine and its memory limit.
struct HostMemory {
    size_t total = 0;      // the most RAM it can hold: physical, or a lower limit
    size_t available = 0;  // RAM new allocations can still take
    size_t commit = 0;     // what Windows will still commit, device memory included; else available
    size_t swap = 0;       // free swap on Linux; Windows counts the page file in commit
};

inline HostMemory hostMemory() {
    const MemoryLimit limit = memoryLimit();
    size_t available = 0, commit = 0, swap = 0;
#if defined(_WIN32)
    MEMORYSTATUSEX st{};
    st.dwLength = sizeof(st);
    if (GlobalMemoryStatusEx(&st)) available = (size_t)st.ullAvailPhys, commit = (size_t)st.ullAvailPageFile;
#elif defined(__linux__)
    // Not sysconf(_SC_AVPHYS_PAGES): glibc answers MemFree, which leaves out the page cache.
    available = commit = meminfoBytes("MemAvailable: %llu kB");
    swap = std::min(meminfoBytes("SwapFree: %llu kB"), limit.swap);
#elif defined(_SC_AVPHYS_PAGES) && defined(_SC_PAGESIZE)
    long pages = sysconf(_SC_AVPHYS_PAGES), page = sysconf(_SC_PAGESIZE);
    if (pages > 0 && page > 0) available = commit = (size_t)pages * (size_t)page;
#endif
    HostMemory h;
    h.total = detail::capped(physicalRamBytes(), limit.limit);
    h.available = detail::capped(available, limit.headroom);
    h.commit = detail::capped(commit, limit.headroom);
    h.swap = swap;
    return h;
}

inline size_t totalRamBytes() { return hostMemory().total; }
inline size_t availableRamBytes() { return hostMemory().available; }
// What new allocations can still take: Windows refuses them at the commit limit, which
// a small page file puts well below free RAM, and charges Vulkan device memory to it too.
inline size_t availableCommitBytes() { return hostMemory().commit; }
inline size_t availableSwapBytes() { return hostMemory().swap; }

// This process's resident set: the physical memory it holds right now.
inline size_t processRamBytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof counters)) return (size_t)counters.WorkingSetSize;
#elif defined(__linux__)
    if (FILE* file = std::fopen("/proc/self/statm", "r")) {
        unsigned long size = 0, resident = 0;
        const int read = std::fscanf(file, "%lu %lu", &size, &resident);
        std::fclose(file);
        const long page = sysconf(_SC_PAGESIZE);
        if (read == 2 && page > 0) return (size_t)resident * (size_t)page;
    }
#endif
    return 0;
}

}  // namespace spirula
