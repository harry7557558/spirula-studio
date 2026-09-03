#include "nn/vk/Context.h"

#include "nn/core/Error.h"
#include "nn/core/Log.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include "core/Env.h"

namespace nn {
namespace vk {

// ================
// Result names
// ================

const char* Context::resultName(VkResult r) {
    switch (r) {
        case VK_SUCCESS:                        return "VK_SUCCESS";
        case VK_NOT_READY:                      return "VK_NOT_READY";
        case VK_TIMEOUT:                        return "VK_TIMEOUT";
        case VK_INCOMPLETE:                     return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY:       return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "VK_ERROR_OUT_OF_DEVICE_MEMORY (the GPU ran out of memory -- "
                   "try a smaller --img-size, a smaller model, or a GPU not "
                   "shared with another job)";
        case VK_ERROR_INITIALIZATION_FAILED:    return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST:
            return "VK_ERROR_DEVICE_LOST (a kernel faulted or hung; rerun with "
                   "SS_NN_DEBUG_SYNC=1 to bisect it to one dispatch)";
        case VK_ERROR_MEMORY_MAP_FAILED:        return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT:        return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT:    return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT:      return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER:      return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS:         return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:     return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_FRAGMENTED_POOL:          return "VK_ERROR_FRAGMENTED_POOL";
        case VK_ERROR_UNKNOWN:                  return "VK_ERROR_UNKNOWN";
        // Video decoding rejects an unsupported profile through these, and the
        // name is the whole diagnosis -- "this GPU cannot decode 4:2:2 HEVC".
        case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR:
            return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
        case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR:
            return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
        case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR:
            return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
        case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR:
            return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
        case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR:
            return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
        default:                                return "VkResult";
    }
}

// ================
// Enumeration
// ================

namespace {

const char* deviceTypeName(VkPhysicalDeviceType t) {
    switch (t) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return "discrete";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return "virtual";
        case VK_PHYSICAL_DEVICE_TYPE_CPU:            return "cpu";
        default:                                     return "other";
    }
}

int typeScore(const char* type) {
    if (std::strcmp(type, "discrete") == 0)   return 4;
    if (std::strcmp(type, "integrated") == 0) return 3;
    if (std::strcmp(type, "virtual") == 0)    return 2;
    if (std::strcmp(type, "cpu") == 0)        return 1;
    return 0;
}

uint64_t deviceLocalHeap(VkPhysicalDevice pd) {
    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(pd, &mp);
    uint64_t best = 0;
    for (uint32_t i = 0; i < mp.memoryHeapCount; ++i)
        if (mp.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            best = std::max<uint64_t>(best, mp.memoryHeaps[i].size);
    return best;
}

// Fills `usable` / `unusable_reason` against the Vulkan 1.2 baseline.
void probeBaseline(VkPhysicalDevice pd, DeviceInfo& out) {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(pd, &props);
    out.name = props.deviceName;
    out.type = deviceTypeName(props.deviceType);
    out.vram_bytes = deviceLocalHeap(pd);

    if (props.apiVersion < VK_API_VERSION_1_2) {
        out.unusable_reason = "driver reports Vulkan < 1.2";
        return;
    }

    VkPhysicalDeviceVulkan12Features f12{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    f2.pNext = &f12;
    vkGetPhysicalDeviceFeatures2(pd, &f2);

    if (!f12.bufferDeviceAddress) {
        out.unusable_reason = "no bufferDeviceAddress";
        return;
    }
    if (!f12.timelineSemaphore) {
        out.unusable_reason = "no timelineSemaphore";
        return;
    }

    uint32_t nq = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &nq, nullptr);
    std::vector<VkQueueFamilyProperties> qf(nq);
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &nq, qf.data());
    bool compute = false;
    for (auto& q : qf)
        if (q.queueFlags & VK_QUEUE_COMPUTE_BIT) compute = true;
    if (!compute) {
        out.unusable_reason = "no compute queue";
        return;
    }
    out.usable = true;
}

VkInstance createInstance(bool validation, VkDebugUtilsMessengerEXT* messenger);

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*) {
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        NN_LOG_WARN("[vk-validation] %s\n", data->pMessage);
    return VK_FALSE;
}

VkInstance createInstance(bool validation, VkDebugUtilsMessengerEXT* messenger) {
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "ssam";
    app.apiVersion = VK_API_VERSION_1_3;  // request 1.3, we only *require* 1.2

    std::vector<const char*> layers;
    std::vector<const char*> exts;
    if (validation) {
        uint32_t n = 0;
        vkEnumerateInstanceLayerProperties(&n, nullptr);
        std::vector<VkLayerProperties> props(n);
        vkEnumerateInstanceLayerProperties(&n, props.data());
        for (auto& p : props)
            if (std::strcmp(p.layerName, "VK_LAYER_KHRONOS_validation") == 0)
                layers.push_back("VK_LAYER_KHRONOS_validation");
        if (layers.empty())
            NN_LOG_WARN("[vk] validation requested but the layer is not installed\n");
        else
            exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;

    // A driver implementing only a subset of Vulkan (MoltenVK, the sole driver
    // on macOS) stays hidden from vkEnumeratePhysicalDevices unless the
    // instance opts in, and vkCreateInstance fails with
    // VK_ERROR_INCOMPATIBLE_DRIVER when it is the only one installed. Where no
    // such driver exists the extension is absent and this does nothing.
    {
        uint32_t n = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &n, nullptr);
        std::vector<VkExtensionProperties> avail(n);
        vkEnumerateInstanceExtensionProperties(nullptr, &n, avail.data());
        for (auto& e : avail)
            if (std::strcmp(e.extensionName,
                            VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
                exts.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
                ici.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
                break;
            }
    }

    ici.enabledLayerCount = (uint32_t)layers.size();
    ici.ppEnabledLayerNames = layers.data();
    ici.enabledExtensionCount = (uint32_t)exts.size();
    ici.ppEnabledExtensionNames = exts.data();

    VkInstance inst = VK_NULL_HANDLE;
    VkResult r = vkCreateInstance(&ici, nullptr, &inst);
    if (r == VK_ERROR_INCOMPATIBLE_DRIVER) {
        // Retry at 1.2 for loaders that reject a 1.3 request outright.
        app.apiVersion = VK_API_VERSION_1_2;
        r = vkCreateInstance(&ici, nullptr, &inst);
    }
    NN_CHECK(r == VK_SUCCESS,
               "vkCreateInstance failed: %s (%d). Is a Vulkan driver installed?",
               Context::resultName(r), (int)r);

    if (messenger && !layers.empty()) {
        auto fn = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            inst, "vkCreateDebugUtilsMessengerEXT");
        if (fn) {
            VkDebugUtilsMessengerCreateInfoEXT dci{
                VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
            dci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            dci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            dci.pfnUserCallback = debugCallback;
            fn(inst, &dci, nullptr, messenger);
        }
    }
    return inst;
}

// A knob that is on unless explicitly switched off. (The other direction is
// spirula::env_on.)
bool envFlagDefaultOn(const char* suffix) {
    const char* v = spirula::env(suffix);
    return !v || !v[0] || std::strcmp(v, "0") != 0;
}

#ifdef VK_KHR_cooperative_matrix
// Does the device advertise the one cooperative-matrix shape the fp16 GEMM is
// written against? 16x16x16 with fp16 operands and an fp32 accumulator, at
// subgroup scope. fp16 accumulate is deliberately not accepted as a substitute:
// it would change the numerics of every Linear in the model to buy a few
// registers, and the accumulator is not where the arithmetic is.
bool hasCoopMatShape(VkInstance inst, VkPhysicalDevice pd) {
    auto fn = (PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR)
        vkGetInstanceProcAddr(inst, "vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR");
    if (!fn) return false;
    uint32_t n = 0;
    if (fn(pd, &n, nullptr) != VK_SUCCESS || n == 0) return false;
    std::vector<VkCooperativeMatrixPropertiesKHR> props(n);
    for (auto& p : props) p = {VK_STRUCTURE_TYPE_COOPERATIVE_MATRIX_PROPERTIES_KHR};
    if (fn(pd, &n, props.data()) != VK_SUCCESS) return false;
    for (uint32_t i = 0; i < n; ++i) {
        const auto& p = props[i];
        if (p.MSize == 16 && p.NSize == 16 && p.KSize == 16 &&
            p.AType == VK_COMPONENT_TYPE_FLOAT16_KHR &&
            p.BType == VK_COMPONENT_TYPE_FLOAT16_KHR &&
            p.CType == VK_COMPONENT_TYPE_FLOAT32_KHR &&
            p.ResultType == VK_COMPONENT_TYPE_FLOAT32_KHR &&
            p.saturatingAccumulation == VK_FALSE &&
            p.scope == VK_SCOPE_SUBGROUP_KHR)
            return true;
    }
    return false;
}
#endif

std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

}  // namespace

std::vector<DeviceInfo> enumerate_devices() {
    std::vector<DeviceInfo> out;
    VkInstance inst = createInstance(false, nullptr);
    uint32_t n = 0;
    vkEnumeratePhysicalDevices(inst, &n, nullptr);
    std::vector<VkPhysicalDevice> devs(n);
    if (n) vkEnumeratePhysicalDevices(inst, &n, devs.data());
    for (uint32_t i = 0; i < n; ++i) {
        DeviceInfo di;
        di.index = (int)i;
        probeBaseline(devs[i], di);
        out.push_back(std::move(di));
    }
    vkDestroyInstance(inst, nullptr);
    return out;
}

// ================
// Lifetime
// ================

namespace {
Context*              g_ctx = nullptr;
std::mutex            g_ctx_mu;
std::atomic<uint64_t> g_generation{0};
}  // namespace

bool Context::initialized() { return g_ctx != nullptr; }

uint64_t Context::generation() { return g_generation.load(std::memory_order_acquire); }

Context& Context::get(const ContextOptions& opts) {
    std::lock_guard<std::mutex> lock(g_ctx_mu);
    if (!g_ctx) {
        // Bumped before init() so that anything resolving entry points from
        // inside device creation caches them against the right generation.
        g_generation.fetch_add(1, std::memory_order_release);
        g_ctx = new Context();
        g_ctx->init(opts);
    }
    return *g_ctx;
}

void Context::shutdown() {
    std::lock_guard<std::mutex> lock(g_ctx_mu);
    delete g_ctx;
    g_ctx = nullptr;
}

Context::~Context() {
    if (device_) {
        vkDeviceWaitIdle(device_);
        vkDestroyDevice(device_, nullptr);
    }
    if (debug_messenger_) {
        auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance_, "vkDestroyDebugUtilsMessengerEXT");
        if (fn) fn(instance_, debug_messenger_, nullptr);
    }
    if (instance_) vkDestroyInstance(instance_, nullptr);
}

void Context::init(const ContextOptions& opts) {
    const bool validation = opts.validation || spirula::env_on("VK_VALIDATION");
    profiling_ = opts.profile || spirula::env_on("PROFILE");

    instance_ = createInstance(validation, &debug_messenger_);
    pickPhysicalDevice(opts);
    createDevice(opts);

    NN_LOG_INFO("[vk] device: %s (%s, %.1f GiB)\n", info_.name.c_str(), info_.type,
                  info_.vram_bytes / (1024.0 * 1024.0 * 1024.0));
}

void Context::pickPhysicalDevice(const ContextOptions& opts) {
    uint32_t n = 0;
    vkEnumeratePhysicalDevices(instance_, &n, nullptr);
    NN_CHECK(n > 0, "no Vulkan physical devices found");
    std::vector<VkPhysicalDevice> devs(n);
    vkEnumeratePhysicalDevices(instance_, &n, devs.data());

    std::vector<DeviceInfo> infos(n);
    for (uint32_t i = 0; i < n; ++i) {
        infos[i].index = (int)i;
        probeBaseline(devs[i], infos[i]);
    }

    // Explicit selection: option, then $SS_VK_DEVICE (index or name substring).
    int chosen = -1;
    std::string match = opts.device_match;
    int want_index = opts.device_index;
    if (const char* env = spirula::env("VK_DEVICE")) {
        char* end = nullptr;
        long v = std::strtol(env, &end, 10);
        if (end && *end == '\0') want_index = (int)v;
        else match = env;
    }
    if (want_index >= 0) {
        NN_CHECK(want_index < (int)n, "SS_VK_DEVICE index %d out of range (%u devices)",
                   want_index, n);
        chosen = want_index;
    } else if (!match.empty()) {
        std::string needle = lower(match);
        for (uint32_t i = 0; i < n; ++i)
            if (lower(infos[i].name).find(needle) != std::string::npos) {
                chosen = (int)i;
                break;
            }
        NN_CHECK(chosen >= 0, "no Vulkan device name contains '%s'", match.c_str());
    } else {
        int best_score = -1;
        uint64_t best_vram = 0;
        for (uint32_t i = 0; i < n; ++i) {
            if (!infos[i].usable) continue;
            int s = typeScore(infos[i].type);
            if (s > best_score || (s == best_score && infos[i].vram_bytes > best_vram)) {
                best_score = s;
                best_vram = infos[i].vram_bytes;
                chosen = (int)i;
            }
        }
        NN_CHECK(chosen >= 0,
                   "no Vulkan device meets the baseline (1.2 + bufferDeviceAddress "
                   "+ timelineSemaphore); first device said: %s",
                   infos[0].unusable_reason.c_str());
    }

    NN_CHECK(infos[chosen].usable, "device %d (%s) is unusable: %s", chosen,
               infos[chosen].name.c_str(), infos[chosen].unusable_reason.c_str());

    physical_ = devs[chosen];
    info_ = infos[chosen];
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physical_, &props);
    limits_ = props.limits;
    vkGetPhysicalDeviceMemoryProperties(physical_, &mem_props_);
}

void Context::createDevice(const ContextOptions& opts) {
    uint32_t nq = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_, &nq, nullptr);
    std::vector<VkQueueFamilyProperties2> qf2(nq);
    std::vector<VkQueueFamilyVideoPropertiesKHR> qvid(nq);
    for (uint32_t i = 0; i < nq; ++i) {
        qf2[i] = {VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2};
        qvid[i] = {VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR};
        qf2[i].pNext = &qvid[i];
    }
    vkGetPhysicalDeviceQueueFamilyProperties2(physical_, &nq, qf2.data());

    // Prefer a compute family without graphics (async compute) when one exists.
    for (uint32_t i = 0; i < nq; ++i) {
        const auto& p = qf2[i].queueFamilyProperties;
        if ((p.queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            !(p.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            queue_family_ = i;
            break;
        }
    }
    if (queue_family_ == UINT32_MAX)
        for (uint32_t i = 0; i < nq; ++i)
            if (qf2[i].queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT) {
                queue_family_ = i;
                break;
            }
    NN_CHECK(queue_family_ != UINT32_MAX, "no compute queue family");

    // ---- optional extensions -------------------------------------------
    uint32_t ne = 0;
    vkEnumerateDeviceExtensionProperties(physical_, nullptr, &ne, nullptr);
    std::vector<VkExtensionProperties> avail(ne);
    vkEnumerateDeviceExtensionProperties(physical_, nullptr, &ne, avail.data());
    auto has = [&](const char* name) {
        for (auto& e : avail)
            if (std::strcmp(e.extensionName, name) == 0) return true;
        return false;
    };

    std::vector<const char*> exts;
    // Mandatory where advertised: the spec forbids creating a device from a
    // portability physical device without it. Spelled out rather than using
    // the macro, which is behind VK_ENABLE_BETA_EXTENSIONS.
    if (has("VK_KHR_portability_subset"))
        exts.push_back("VK_KHR_portability_subset");
    subgroup_size_control_ = has(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME);
    if (subgroup_size_control_) {
        exts.push_back(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME);
        VkPhysicalDeviceSubgroupSizeControlPropertiesEXT sp{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT};
        VkPhysicalDeviceProperties2 p2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
        p2.pNext = &sp;
        vkGetPhysicalDeviceProperties2(physical_, &p2);
        // Pin the widest size the device offers that our 64-wide-X kernels can
        // still fill; Intel/ANV's varying width is the failure this avoids.
        preferred_subgroup_ = std::min<uint32_t>(sp.maxSubgroupSize, 64u);
        preferred_subgroup_ = std::max<uint32_t>(preferred_subgroup_, sp.minSubgroupSize);
        NN_LOG_DEBUG("[vk] subgroup size pinned to %u (range %u..%u)\n",
                     preferred_subgroup_, sp.minSubgroupSize, sp.maxSubgroupSize);
    }

    // ---- 64-bit integers -------------------------------------------------
    // Only the `*_wide` modules use them, to address past 4 GiB. Contained like
    // the coop-matrix module: false keeps Int64 off the device entirely.
    {
        VkPhysicalDeviceFeatures pf{};
        vkGetPhysicalDeviceFeatures(physical_, &pf);
        int64_ = pf.shaderInt64 == VK_TRUE;
    }
    NN_LOG_DEBUG("[vk] shaderInt64: %s\n", int64_ ? "yes" : "no");

    // ---- cooperative matrix ---------------------------------------------
    // Everything the fp16 GEMM's tensor-core path needs, resolved to one flag.
    // The kernels that use it live in their own SPIR-V module: a module
    // declaring CooperativeMatrixKHR must never be handed to a device without
    // it, and Pipelines only creates a VkShaderModule when an entry from that
    // module is first acquired -- so leaving the flag false is enough to keep
    // the capability off the device entirely.
#ifdef VK_KHR_cooperative_matrix
    if (!envFlagDefaultOn("NN_COOPMAT")) {
        coopmat_reason_ = "disabled by SS_NN_COOPMAT=0";
    } else if (!has(VK_KHR_COOPERATIVE_MATRIX_EXTENSION_NAME)) {
        coopmat_reason_ = "driver does not expose VK_KHR_cooperative_matrix";
    } else {
        VkPhysicalDeviceCooperativeMatrixFeaturesKHR pcm{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_KHR};
        VkPhysicalDeviceVulkan12Features p12{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        pcm.pNext = &p12;
        VkPhysicalDeviceFeatures2 pf2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        pf2.pNext = &pcm;
        vkGetPhysicalDeviceFeatures2(physical_, &pf2);

        VkPhysicalDeviceCooperativeMatrixPropertiesKHR pcmp{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_PROPERTIES_KHR};
        VkPhysicalDeviceProperties2 pp2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
        pp2.pNext = &pcmp;
        vkGetPhysicalDeviceProperties2(physical_, &pp2);

        // Reported in the order the shader would trip over them.
        if (!pcm.cooperativeMatrix)
            coopmat_reason_ = "cooperativeMatrix feature not supported";
        else if (!(pcmp.cooperativeMatrixSupportedStages & VK_SHADER_STAGE_COMPUTE_BIT))
            coopmat_reason_ = "cooperative matrix is not supported in compute shaders";
        else if (!p12.shaderFloat16)
            coopmat_reason_ = "no shaderFloat16";
        else if (!p12.vulkanMemoryModel || !p12.vulkanMemoryModelDeviceScope)
            coopmat_reason_ = "no vulkanMemoryModel (device scope)";
        else if (!subgroup_size_control_)
            coopmat_reason_ = "no VK_EXT_subgroup_size_control, so the subgroup "
                              "width the kernel tiles against cannot be pinned";
        else if (preferred_subgroup_ != 32 && preferred_subgroup_ != 64)
            coopmat_reason_ = "subgroup width is neither 32 nor 64";
        else if (!hasCoopMatShape(instance_, physical_))
            coopmat_reason_ = "no 16x16x16 fp16 shape with an fp32 accumulator";
        else
            coopmat_ = true;

        if (coopmat_) {
            coopmat_reason_.clear();
            exts.push_back(VK_KHR_COOPERATIVE_MATRIX_EXTENSION_NAME);
        }
    }
#else
    coopmat_reason_ = "built against Vulkan headers without VK_KHR_cooperative_matrix";
#endif
    NN_LOG_DEBUG("[vk] cooperative matrix: %s\n",
                 coopmat_ ? "16x16x16 fp16/fp32" : coopmat_reason_.c_str());

    // ---- video decode ---------------------------------------------------
    bool want_video = opts.want_video;
#ifdef SS_HAVE_VIDEO
    want_video = true;  // always probe; the decoder is created lazily
#endif
    if (want_video) {
        const bool base = has(VK_KHR_VIDEO_QUEUE_EXTENSION_NAME) &&
                          has(VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME);
        if (!base) {
            video_reason_ = "driver does not expose VK_KHR_video_queue / "
                            "VK_KHR_video_decode_queue";
        } else {
            for (uint32_t i = 0; i < nq; ++i) {
                if (!(qf2[i].queueFamilyProperties.queueFlags &
                      VK_QUEUE_VIDEO_DECODE_BIT_KHR))
                    continue;
                video_queue_family_ = i;
                video_codec_ops_ = qvid[i].videoCodecOperations;
                break;
            }
            if (video_queue_family_ == UINT32_MAX) {
                video_reason_ = "no queue family advertises VIDEO_DECODE";
            } else {
                exts.push_back(VK_KHR_VIDEO_QUEUE_EXTENSION_NAME);
                exts.push_back(VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME);
                if (has(VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME))
                    exts.push_back(VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME);
                else
                    video_codec_ops_ &= ~(uint32_t)VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR;
                if (has(VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME))
                    exts.push_back(VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME);
                else
                    video_codec_ops_ &= ~(uint32_t)VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR;
#ifdef VK_KHR_video_decode_av1
                if (has(VK_KHR_VIDEO_DECODE_AV1_EXTENSION_NAME))
                    exts.push_back(VK_KHR_VIDEO_DECODE_AV1_EXTENSION_NAME);
                else
                    video_codec_ops_ &= ~(uint32_t)VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR;
#endif
                if (video_codec_ops_ == 0) {
                    video_reason_ = "video-decode queue exists but no supported codec "
                                    "(H.264/H.265/AV1) is exposed";
                    video_queue_family_ = UINT32_MAX;
                    // Drop the extensions we just added: nothing will use them.
                    while (!exts.empty() &&
                           std::strncmp(exts.back(), "VK_KHR_video", 12) == 0)
                        exts.pop_back();
                } else {
                    video_reason_.clear();
                }
            }
        }
    }

    // ---- queues ---------------------------------------------------------
    const float prio = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> qcis;
    {
        VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        q.queueFamilyIndex = queue_family_;
        q.queueCount = 1;
        q.pQueuePriorities = &prio;
        qcis.push_back(q);
        if (video_queue_family_ != UINT32_MAX && video_queue_family_ != queue_family_) {
            q.queueFamilyIndex = video_queue_family_;
            qcis.push_back(q);
        }
    }

    VkPhysicalDeviceVulkan12Features f12{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    f12.bufferDeviceAddress = VK_TRUE;
    f12.timelineSemaphore = VK_TRUE;

    VkPhysicalDeviceVulkan11Features f11{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    f11.pNext = &f12;

#ifdef VK_KHR_cooperative_matrix
    // Exactly the capability list `spirv-dis gemm_coop.spv` prints:
    // CooperativeMatrixKHR, Float16, VulkanMemoryModel and its device scope.
    // Requested together or not at all -- the probe above has already
    // established the device has all of them.
    VkPhysicalDeviceCooperativeMatrixFeaturesKHR fcm{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_KHR};
    if (coopmat_) {
        fcm.cooperativeMatrix = VK_TRUE;
        f12.shaderFloat16 = VK_TRUE;
        f12.vulkanMemoryModel = VK_TRUE;
        f12.vulkanMemoryModelDeviceScope = VK_TRUE;
    }
#endif

    // f11 -> f12 is fixed; the optional structs prepend to it.
    void* chain = &f11;
#ifdef VK_KHR_cooperative_matrix
    if (coopmat_) {
        fcm.pNext = chain;
        chain = &fcm;
    }
#endif
    // Decoded pictures are multi-planar YCbCr images. We never sample them --
    // the planes are copied to buffers and unpacked in a shader -- but creating
    // an image in one of those formats is gated on this feature.
    if (video_queue_family_ != UINT32_MAX) f11.samplerYcbcrConversion = VK_TRUE;

    VkPhysicalDeviceSubgroupSizeControlFeaturesEXT fsg{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT};
    if (subgroup_size_control_) {
        fsg.subgroupSizeControl = VK_TRUE;
        fsg.computeFullSubgroups = VK_TRUE;
        fsg.pNext = chain;
        chain = &fsg;
    }

    VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    f2.pNext = chain;
    if (int64_) f2.features.shaderInt64 = VK_TRUE;

    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.pNext = &f2;
    dci.queueCreateInfoCount = (uint32_t)qcis.size();
    dci.pQueueCreateInfos = qcis.data();
    dci.enabledExtensionCount = (uint32_t)exts.size();
    dci.ppEnabledExtensionNames = exts.data();
    NN_VK_CHECK(vkCreateDevice(physical_, &dci, nullptr, &device_));

    vkGetDeviceQueue(device_, queue_family_, 0, &queue_);
    if (video_queue_family_ != UINT32_MAX)
        vkGetDeviceQueue(device_, video_queue_family_,
                         video_queue_family_ == queue_family_ ? 0 : 0, &video_queue_);

}

bool Context::hasVideoCodec(VkVideoCodecOperationFlagBitsKHR op) const {
    return (video_codec_ops_ & (uint32_t)op) != 0;
}

PFN_vkVoidFunction Context::deviceProc(const char* name) const {
    return vkGetDeviceProcAddr(device_, name);
}

}  // namespace vk
}  // namespace nn
