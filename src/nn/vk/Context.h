#pragma once
// The Vulkan device this library runs on.
//
// One device per process, picked lazily on first use. Enumeration is
// side-effect free (it spins up a throwaway VkInstance) so an application can
// list devices and choose before any allocation happens -- the same contract
// the engine's backend::device_* calls provide, so the two converge when
// merged.
//
// Baseline: **Vulkan 1.2 core** with `bufferDeviceAddress` and
// `timelineSemaphore`. Everything else is optional and probed:
//
//   VK_EXT_subgroup_size_control   pin the subgroup width (Intel/ANV defaults
//                                  to a varying SIMD width, which silently
//                                  breaks any `tid / WaveGetLaneCount()`
//                                  indexing)
//   VK_KHR_video_decode_queue      video/ decoder; absent -> descriptive error
//   VK_KHR_cooperative_matrix      tensor cores for the fp16 GEMM; absent ->
//                                  the fp32 kernels run unchanged
//   shaderFloat16 / 16-bit storage NOT required by the fp32 kernels: fp16
//                                  weights are read as packed uint words (see
//                                  AGENTS.md). The cooperative-matrix path does
//                                  need them, and is enabled only when it can
//                                  have all of them at once.

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/SourcePath.h"

namespace nn {
namespace vk {

struct DeviceInfo {
    int         index = -1;
    std::string name;
    const char* type = "other";  // discrete|integrated|virtual|cpu|other
    uint64_t    vram_bytes = 0;
    bool        usable = false;   // meets the Vulkan 1.2 baseline
    std::string unusable_reason;
};

// Side-effect free: does not create or touch the process Context.
std::vector<DeviceInfo> enumerate_devices();

struct ContextOptions {
    // -1 = auto (discrete > integrated > cpu, VRAM tie-break). Overridden by
    // $SS_VK_DEVICE, which accepts an index or a case-insensitive substring of
    // the device name.
    int         device_index = -1;
    std::string device_match;
    bool        validation = false;   // or $SS_VK_VALIDATION=1
    bool        profile = false;      // or $SS_PROFILE=1
    bool        want_video = false;   // request the video-decode queue family
};

class Context {
public:
    // Creates the process context if needed and returns it. Thread-safe.
    static Context& get(const ContextOptions& opts = {});
    // True once get() has run; lets callers avoid forcing device creation.
    static bool     initialized();
    // Releases the device. Every buffer, pipeline and pool must already be
    // gone; call only at shutdown.
    static void     shutdown();
    // 0 before the first get(), then bumped on every context creation.
    // shutdown() is not the end of the process -- the GUI hands the GPU back
    // between jobs and creates a second context later -- so anything that
    // caches device-derived state for the life of the process (an extension
    // entry-point table, above all) must compare against this and re-resolve
    // when it changes. A table resolved from the previous device points at
    // loader trampolines that are gone; calling one segfaults.
    static uint64_t generation();

    VkInstance         instance()      const { return instance_; }
    VkPhysicalDevice   physical()      const { return physical_; }
    VkDevice           device()        const { return device_; }
    VkQueue            queue()         const { return queue_; }
    uint32_t           queueFamily()   const { return queue_family_; }
    const DeviceInfo&  info()          const { return info_; }

    const VkPhysicalDeviceMemoryProperties& memoryProps() const { return mem_props_; }
    const VkPhysicalDeviceLimits&           limits()      const { return limits_; }

    // Feature flags resolved at device creation.
    bool     hasSubgroupSizeControl() const { return subgroup_size_control_; }
    uint32_t preferredSubgroupSize()  const { return preferred_subgroup_; }
    uint32_t maxPushConstantsSize()   const { return limits_.maxPushConstantsSize; }
    bool     profiling()              const { return profiling_; }
    float    timestampPeriod()        const { return limits_.timestampPeriod; }

    // Cooperative matrix (tensor cores), resolved at device creation. Exactly
    // one configuration is ever used -- 16x16x16, fp16 operands, fp32
    // accumulate, subgroup scope -- so this is a flag and not a table: a kernel
    // written against a shape the device does not advertise would fail to
    // compile in the driver, not degrade. `coopMatReason()` says why it is off,
    // which is the first thing to look at when a machine is unexpectedly slow.
    //
    // $SS_NN_COOPMAT=0 forces it off, for bisecting a numerical difference.
    bool               hasCoopMat()    const { return coopmat_; }
    const std::string& coopMatReason() const { return coopmat_reason_; }

    // 64-bit integer arithmetic, which only the `*_wide` shader modules use --
    // to address past 4 GiB. Contained the same way the coop-matrix module is.
    bool               hasInt64()      const { return int64_; }

    // Video decode support, resolved at device creation. `videoQueueFamily()`
    // is UINT32_MAX when unavailable; `videoUnavailableReason()` then explains
    // why, for the error video/VideoDecoder.cpp raises.
    bool        hasVideoDecode()          const { return video_queue_family_ != UINT32_MAX; }
    uint32_t    videoQueueFamily()        const { return video_queue_family_; }
    VkQueue     videoQueue()              const { return video_queue_; }
    bool        hasVideoCodec(VkVideoCodecOperationFlagBitsKHR op) const;
    const std::string& videoUnavailableReason() const { return video_reason_; }

    // Loaded per-device entry points for the extensions we use dynamically.
    // (Vulkan-Headers ships the prototypes but not all loaders export them.)
    PFN_vkVoidFunction deviceProc(const char* name) const;

    // Human-readable VkResult, used by every VK_CHECK site.
    static const char* resultName(VkResult r);

private:
    Context() = default;
    ~Context();
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    void init(const ContextOptions& opts);
    void pickPhysicalDevice(const ContextOptions& opts);
    void createDevice(const ContextOptions& opts);

    VkInstance       instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_ = VK_NULL_HANDLE;
    VkDevice         device_   = VK_NULL_HANDLE;
    VkQueue          queue_    = VK_NULL_HANDLE;
    uint32_t         queue_family_ = UINT32_MAX;
    VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;

    VkQueue   video_queue_ = VK_NULL_HANDLE;
    uint32_t  video_queue_family_ = UINT32_MAX;
    uint32_t  video_codec_ops_ = 0;
    std::string video_reason_ = "not requested";

    VkPhysicalDeviceMemoryProperties mem_props_{};
    VkPhysicalDeviceLimits           limits_{};
    DeviceInfo info_;

    bool     subgroup_size_control_ = false;
    uint32_t preferred_subgroup_ = 32;
    bool     profiling_ = false;

    bool        int64_ = false;
    bool        coopmat_ = false;
    std::string coopmat_reason_ = "not probed";
};

}  // namespace vk
}  // namespace nn

// Every Vulkan call goes through this. Unlike a research prototype we never
// compile the check out: a silent VK_ERROR_OUT_OF_DEVICE_MEMORY three calls
// upstream is the hardest class of bug in this codebase.
#define NN_VK_CHECK(expr)                                                    \
    do {                                                                       \
        VkResult _r = (expr);                                                  \
        if (_r != VK_SUCCESS)                                                  \
            ::nn::fail("%s failed: %s (%d) at %s:%d", #expr,                 \
                         ::nn::vk::Context::resultName(_r), (int)_r,         \
                         SS_FILE, __LINE__);                                   \
    } while (0)
