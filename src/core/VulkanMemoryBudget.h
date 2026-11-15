#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstring>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <pdh.h>
#include <cwchar>
#pragma comment(lib, "pdh.lib")
#endif

namespace spirula::vkmemory {

struct Budget {
    uint64_t budget_bytes = 0, usage_bytes = 0, available_bytes = 0;
};

inline Budget queryBudget(VkPhysicalDevice device) {
    VkPhysicalDeviceMemoryBudgetPropertiesEXT budget{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
    VkPhysicalDeviceMemoryProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
    properties.pNext = &budget;
    vkGetPhysicalDeviceMemoryProperties2(device, &properties);
    Budget out;
    for (uint32_t i = 0; i < properties.memoryProperties.memoryHeapCount; ++i) {
        if (!(properties.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)) continue;
        out.budget_bytes += budget.heapBudget[i];
        out.usage_bytes += budget.heapUsage[i];
        if (budget.heapBudget[i] > budget.heapUsage[i]) out.available_bytes += budget.heapBudget[i] - budget.heapUsage[i];
    }
    return out;
}

// Device-local bytes other programs hold, 0 if unknown. On Windows the driver's
// budget ignores them (two R9700s holding 1.69 GB and 0.14 GB reported equal
// budgets), so read the adapter counter Task Manager shows instead.
inline uint64_t othersLocalBytes(VkPhysicalDevice device, const VkPhysicalDeviceIDProperties& id) {
#ifdef _WIN32
    (void)device;
    if (!id.deviceLUIDValid) return 0;
    LUID luid;
    std::memcpy(&luid, id.deviceLUID, sizeof luid);
    wchar_t path[96];
    std::swprintf(path, 96, L"\\GPU Adapter Memory(luid_0x%08lx_0x%08lx_phys_0)\\Dedicated Usage",
                  (unsigned long)luid.HighPart, luid.LowPart);
    PDH_HQUERY query = nullptr;
    if (PdhOpenQueryW(nullptr, 0, &query) != ERROR_SUCCESS) return 0;
    PDH_HCOUNTER counter = nullptr;
    PDH_FMT_COUNTERVALUE value{};
    const bool ok = PdhAddEnglishCounterW(query, path, 0, &counter) == ERROR_SUCCESS &&
                    PdhCollectQueryData(query) == ERROR_SUCCESS &&
                    PdhGetFormattedCounterValue(counter, PDH_FMT_LARGE, nullptr, &value) == ERROR_SUCCESS &&
                    value.CStatus == ERROR_SUCCESS;
    PdhCloseQuery(query);
    return ok ? (uint64_t)value.largeValue : 0;
#else
    (void)id;
    uint32_t n = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &n, nullptr);
    std::vector<VkExtensionProperties> extensions(n);
    if (n) vkEnumerateDeviceExtensionProperties(device, nullptr, &n, extensions.data());
    bool supported = false;
    for (const VkExtensionProperties& e : extensions)
        supported |= std::strcmp(e.extensionName, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME) == 0;
    if (!supported) return 0;  // the budget struct may not be chained then
    VkPhysicalDeviceMemoryProperties memory;
    vkGetPhysicalDeviceMemoryProperties(device, &memory);
    uint64_t heap = 0;
    for (uint32_t i = 0; i < memory.memoryHeapCount; ++i)
        if (memory.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) heap += memory.memoryHeaps[i].size;
    const uint64_t budget = queryBudget(device).budget_bytes;  // RADV: heap minus other processes' usage
    return heap > budget ? heap - budget : 0;
#endif
}

}  // namespace spirula::vkmemory
