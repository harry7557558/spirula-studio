#pragma once
// How much physical memory the machine has, or 0 when the platform will not
// say. Callers that need a working number must supply their own fallback.

#include <cstddef>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN   // rpcndr.h's `#define small char` reaches far from here
#endif
#include <windows.h>
#include <psapi.h>
#else
#include <cstdio>
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

inline size_t availableRamBytes() {
#if defined(_WIN32)
    MEMORYSTATUSEX st{};
    st.dwLength = sizeof(st);
    if (GlobalMemoryStatusEx(&st)) return (size_t)st.ullAvailPhys;
#elif defined(__linux__)
    // Not sysconf(_SC_AVPHYS_PAGES): glibc answers MemFree, which leaves out the page cache.
    if (FILE* file = std::fopen("/proc/meminfo", "r")) {
        char line[128];
        unsigned long long kb = 0;
        bool found = false;
        while (!found && std::fgets(line, sizeof line, file))
            found = std::sscanf(line, "MemAvailable: %llu kB", &kb) == 1;
        std::fclose(file);
        if (found) return (size_t)kb * 1024;
    }
#elif defined(_SC_AVPHYS_PAGES) && defined(_SC_PAGESIZE)
    long pages = sysconf(_SC_AVPHYS_PAGES), page = sysconf(_SC_PAGESIZE);
    if (pages > 0 && page > 0) return (size_t)pages * (size_t)page;
#endif
    return 0;
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
