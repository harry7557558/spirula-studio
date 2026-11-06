#pragma once

// Worker-pool sizing for every subsystem. On Windows
// std::thread::hardware_concurrency() is answered by the installed msvcp140.dll,
// and before 14.50 that counts only the calling thread's processor group (at most
// 64 logical processors, microsoft/STL#5453), sizing pools to one group's share.
// Header-only like core/Env.h, and without <windows.h>, whose near/far macros
// break code that includes this.

#include <thread>

#ifdef _WIN32
extern "C" __declspec(dllimport) unsigned long __stdcall GetActiveProcessorCount(unsigned short);
#endif

namespace spirula {

// Logical processors across all groups; 0 when unknown, as hardware_concurrency().
// Windows 10 and Server 2019 keep a process in one group, where the extra workers
// share it; spreading them with SetThreadGroupAffinity is the fix if that matters.
inline unsigned hardware_threads() {
#ifdef _WIN32
    if (const unsigned long n = GetActiveProcessorCount(0xffff)) return (unsigned)n;  // ALL_PROCESSOR_GROUPS
#endif
    return std::thread::hardware_concurrency();
}

}  // namespace spirula
