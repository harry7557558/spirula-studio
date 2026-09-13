#pragma once
// Vulkan device context for the backend (see README.md in this directory).
// One instance, one physical device, one compute queue, one queue-level
// timeline semaphore. Lazily initialized on first use; init failure is
// reported through ok() + backend::last_error(), never by throwing across
// the backend API surface.

#include "backend/api/BackendRuntime.h"
#include <vulkan/vulkan.h>

#include <cstdint>
#include <mutex>
#include <string>

namespace backend {
namespace vk {

// Optional capabilities probed per device. Required features (Vulkan 1.2,
// bufferDeviceAddress, timelineSemaphore) are preconditions of ok() and
// have no flags here. The shader-facing flags below select between embedded
// SPIR-V blob variants at pipeline creation (VulkanPipelines.cpp); each
// kernel behaves identically on either path.
struct Capabilities {
    bool float32_atomic_add = false;  // VK_EXT_shader_atomic_float
    bool shader_int64 = false;        // shaderInt64 (else ".noint64" blobs)
    bool shader_int8 = false;         // shaderInt8 + storageBuffer8BitAccess
                                      // (picks the native ".int8" blobs)
    bool memory_budget = false;       // VK_EXT_memory_budget (usage reporting)
    bool timestamps = false;
    double timestamp_period_ns = 0.0;
    uint32_t subgroup_size = 0;       // advertised default; never assumed 32
    // Non-zero when VK_EXT_subgroup_size_control is enabled: every compute
    // pipeline is created with this REQUIRED subgroup size + full subgroups,
    // pinning WaveGetLaneCount() to a launch-time constant. Essential on
    // Intel (ANV), whose default is a VARYING SIMD width that breaks
    // tid/WaveGetLaneCount() subgroup indexing.
    uint32_t required_subgroup_size = 0;
    uint32_t max_push_constants = 128;
    uint32_t max_workgroup_invocations = 0;
    uint32_t max_shared_memory = 0;
    VkDeviceSize non_coherent_atom_size = 64;
};
struct HeapBudget {
    BudgetStatus status = BudgetStatus::Unavailable;
    uint64_t total_bytes = 0;
    uint64_t available_bytes = 0;
    uint64_t budget_bytes = 0;
    uint64_t usage_bytes = 0;
};
bool context_created();

class Context {
public:
    // Lazily creates the context on first call. Selection: required features
    // filtered, then discrete > integrated > others, VRAM as tie-break.
    // Override with SS_VK_DEVICE=<index|name substring>.
    static Context& get();

    bool ok() const { return _device != VK_NULL_HANDLE; }
    BudgetStatus init_status() const { return _init_status; }

    VkInstance instance() const { return _instance; }
    VkPhysicalDevice physical() const { return _physical; }
    VkDevice device() const { return _device; }
    uint32_t queue_family() const { return _queue_family; }
    const Capabilities& caps() const { return _caps; }
    const std::string& device_name() const { return _device_name; }

    // Submits `cb` on the compute queue, signaling the queue timeline with
    // the returned (monotonically increasing) value. Thread-safe.
    // Returns 0 on failure (and sets the sticky error).
    uint64_t submit(VkCommandBuffer cb);

    // Blocks until the queue timeline reaches `value`. Returns false on
    // device loss / wait failure (sticky error set). On GPU devices this
    // spin-polls vkGetSemaphoreCounterValue briefly before falling back to
    // the (slower) blocking vkWaitSemaphores.
    bool wait(uint64_t value);

    // Last value handed out by submit(). wait(last_submitted()) is a full
    // device sync for work submitted through this context.
    uint64_t last_submitted() const { return _last_value; }

    // Memory-type index with all `required` property flags for `type_bits`,
    // or UINT32_MAX.
    uint32_t find_memory_type(uint32_t type_bits,
                              VkMemoryPropertyFlags required) const;
    // Memory-type index to heap index mapping, and heap property helpers.
    uint32_t memory_type_heap(uint32_t type_index) const;
    bool is_heap_device_local(uint32_t heap_index) const;
    int default_device_local_heap() const;

    // Queries one heap's EXT memory budget.
    HeapBudget query_heap_budget(uint32_t heap_index) const;

    // Called by the runtime layer so its device children (command pools,
    // staging buffer, query pool, leaked allocations) are destroyed inside
    // ~Context, after the device-idle wait but before vkDestroyDevice.
    // The runtime's namespace-scope statics outlive the function-local
    // Context singleton (reverse construction order), so the hook may
    // safely touch them.
    void set_shutdown_hook(void (*hook)()) { _shutdown_hook = hook; }

    ~Context();
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

private:
    Context();
    void init();

    VkInstance _instance = VK_NULL_HANDLE;
    VkPhysicalDevice _physical = VK_NULL_HANDLE;
    VkDevice _device = VK_NULL_HANDLE;
    VkQueue _queue = VK_NULL_HANDLE;
    uint32_t _queue_family = 0;
    VkSemaphore _timeline = VK_NULL_HANDLE;
    uint64_t _last_value = 0;
    bool _poll_waits = false;
    std::mutex _submit_mutex;
    Capabilities _caps;
    std::string _device_name;
    VkPhysicalDeviceMemoryProperties _mem_props{};
    BudgetStatus _init_status = BudgetStatus::QueryError;
    void (*_shutdown_hook)() = nullptr;
};

// Sets the sticky backend error (returned once by backend::last_error()).
// `result` may be VK_SUCCESS for non-VkResult failures.
void set_error(const char* what, VkResult result);

}  // namespace vk
}  // namespace backend
