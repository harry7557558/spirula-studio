#pragma once
// How much memory the machine has, or 0 when the platform will not say.
// Callers that need a working number must supply their own fallback.

#include <cstddef>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN   // rpcndr.h's `#define small char` reaches far from here
#endif
#include <windows.h>
#include <psapi.h>
#else
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#endif

namespace spirula {

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

// What a memory-limited cgroup (a container, a Slurm job) still lets this process
// take, which MemAvailable does not see: the tightest limit - (usage - inactive file
// cache) over the process's cgroup and its ancestors. SIZE_MAX with no limit.
inline size_t cgroupRamHeadroomBytes(const char* self_cgroup = "/proc/self/cgroup",
                                     const char* mount = "/sys/fs/cgroup") {
    std::string root, path;
    const char *limit_file = "memory.max", *usage_file = "memory.current", *cache_key = "inactive_file";
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
                limit_file = "memory.limit_in_bytes";
                usage_file = "memory.usage_in_bytes";
                cache_key = "total_inactive_file";
                break;
            }
            if (controllers == ",," && std::strncmp(line, "0:", 2) == 0) {
                root = mount;
                path = second + 1;
            }
        }
        std::fclose(file);
    }
    if (root.empty()) return SIZE_MAX;
    std::string dir = root + path;
    while (dir.size() > root.size() && dir.back() == '/') dir.pop_back();
    unsigned long long probe = 0;
    // Without its own cgroup namespace a container sees a host path it cannot open.
    if (!detail::cgroupNumber(dir + "/" + usage_file, nullptr, probe)) dir = root;
    size_t headroom = SIZE_MAX;
    for (;;) {
        unsigned long long limit = 0, usage = 0, cache = 0;
        // v1 writes "no limit" as a number near 2^63.
        if (detail::cgroupNumber(dir + "/" + limit_file, nullptr, limit) && limit < (1ull << 62) &&
            detail::cgroupNumber(dir + "/" + usage_file, nullptr, usage)) {
            detail::cgroupNumber(dir + "/memory.stat", cache_key, cache);
            const unsigned long long working = usage > cache ? usage - cache : 0;
            const size_t left = limit > working ? (size_t)(limit - working) : 0;
            if (left < headroom) headroom = left;
        }
        if (dir.size() <= root.size()) break;
        dir.erase(dir.rfind('/'));
    }
    return headroom;
}
#endif

inline size_t availableRamBytes() {
#if defined(_WIN32)
    MEMORYSTATUSEX st{};
    st.dwLength = sizeof(st);
    if (GlobalMemoryStatusEx(&st)) return (size_t)st.ullAvailPhys;
#elif defined(__linux__)
    // Not sysconf(_SC_AVPHYS_PAGES): glibc answers MemFree, which leaves out the page cache.
    return meminfoBytes("MemAvailable: %llu kB");
#elif defined(_SC_AVPHYS_PAGES) && defined(_SC_PAGESIZE)
    long pages = sysconf(_SC_AVPHYS_PAGES), page = sysconf(_SC_PAGESIZE);
    if (pages > 0 && page > 0) return (size_t)pages * (size_t)page;
#endif
    return 0;
}

// What new allocations can still take: Windows refuses them at the commit limit, which
// a small page file puts well below free RAM, and charges Vulkan device memory to it too.
inline size_t availableCommitBytes() {
#if defined(_WIN32)
    MEMORYSTATUSEX st{};
    st.dwLength = sizeof(st);
    if (GlobalMemoryStatusEx(&st)) return (size_t)st.ullAvailPageFile;
    return 0;
#else
    return availableRamBytes();
#endif
}

// Free swap on Linux. 0 elsewhere: Windows already counts the page file in availableCommitBytes.
inline size_t availableSwapBytes() {
#if defined(__linux__)
    return meminfoBytes("SwapFree: %llu kB");
#else
    return 0;
#endif
}

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
