#include "backend/vulkan/VulkanContext.h"
#include "i18n/catalog/Data.h"

#include "backend/api/BackendRuntime.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <thread>
#include <vector>
#include "core/Env.h"

namespace backend {
namespace vk {

// --- sticky error (definition of backend::last_error lives in
// VulkanRuntime.cpp; it reads these) ---
std::mutex g_error_mutex;
std::string g_error;

// Live device_malloc byte total, defined in VulkanRuntime.cpp; read below by
// backend::memory_usage() for the process VRAM figure.
extern std::atomic<uint64_t> g_device_bytes;

void set_error(const char* what, VkResult result) {
    std::lock_guard<std::mutex> lock(g_error_mutex);
    if (!g_error.empty()) return;  // sticky: keep the FIRST error
    g_error = what;
    if (result != VK_SUCCESS) {
        g_error += " (VkResult ";
        g_error += std::to_string((int)result);
        g_error += ")";
    }
}

namespace {

const char* device_type_name(VkPhysicalDeviceType t) {
    switch (t) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "discrete";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "integrated";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "virtual";
        case VK_PHYSICAL_DEVICE_TYPE_CPU: return "cpu";
        default: return "other";
    }
}

int device_type_score(VkPhysicalDeviceType t) {
    switch (t) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 3;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 2;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 1;
        case VK_PHYSICAL_DEVICE_TYPE_CPU: return 0;
        default: return 0;
    }
}

VkDeviceSize device_local_vram(VkPhysicalDevice pd) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(pd, &mp);
    VkDeviceSize total = 0;
    for (uint32_t i = 0; i < mp.memoryHeapCount; i++)
        if (mp.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            total += mp.memoryHeaps[i].size;
    return total;
}

// Compute queue family with timestamp support preferred.
uint32_t pick_compute_queue_family(VkPhysicalDevice pd) {
    uint32_t n = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &n, nullptr);
    std::vector<VkQueueFamilyProperties> qf(n);
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &n, qf.data());
    uint32_t best = UINT32_MAX;
    for (uint32_t i = 0; i < n; i++) {
        if (!(qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) continue;
        if (best == UINT32_MAX) best = i;
        if (qf[i].timestampValidBits > 0) return i;
    }
    return best;
}

struct DeviceProbe {
    bool required_ok = false;
    bool atomic_float = false;
    bool shader_int64 = false;
    bool shader_int8 = false;
    uint32_t queue_family = UINT32_MAX;
    VkPhysicalDeviceProperties props{};
    VkPhysicalDeviceSubgroupProperties subgroup{};
};

DeviceProbe probe_device(VkPhysicalDevice pd) {
    DeviceProbe out;

    VkPhysicalDeviceVulkan12Features f12{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceShaderAtomicFloatFeaturesEXT fatomic{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT};
    f12.pNext = &fatomic;
    VkPhysicalDeviceFeatures2 f2{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    f2.pNext = &f12;
    vkGetPhysicalDeviceFeatures2(pd, &f2);

    out.subgroup.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
    VkPhysicalDeviceProperties2 p2{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    p2.pNext = &out.subgroup;
    vkGetPhysicalDeviceProperties2(pd, &p2);
    out.props = p2.properties;

    out.queue_family = pick_compute_queue_family(pd);
    out.required_ok =
        out.props.apiVersion >= VK_API_VERSION_1_2 &&
        f12.bufferDeviceAddress &&
        f12.timelineSemaphore &&
        out.queue_family != UINT32_MAX;
    out.atomic_float = fatomic.shaderBufferFloat32AtomicAdd;
    // Optional: without shaderInt64 the pipeline layer loads the ".noint64"
    // blob variants (32-bit index emulation); with shaderInt8 (+ 8-bit
    // storage, near-universally paired) it loads the native ".int8" ones.
    out.shader_int64 = f2.features.shaderInt64;
    out.shader_int8 = f12.shaderInt8 && f12.storageBuffer8BitAccess;
    return out;
}

bool has_extension(VkPhysicalDevice pd, const char* name) {
    uint32_t n = 0;
    vkEnumerateDeviceExtensionProperties(pd, nullptr, &n, nullptr);
    std::vector<VkExtensionProperties> exts(n);
    vkEnumerateDeviceExtensionProperties(pd, nullptr, &n, exts.data());
    for (const auto& e : exts)
        if (std::strcmp(e.extensionName, name) == 0) return true;
    return false;
}

bool has_instance_layer(const char* name) {
    uint32_t n = 0;
    vkEnumerateInstanceLayerProperties(&n, nullptr);
    std::vector<VkLayerProperties> layers(n);
    vkEnumerateInstanceLayerProperties(&n, layers.data());
    for (const auto& l : layers)
        if (std::strcmp(l.layerName, name) == 0) return true;
    return false;
}

bool has_instance_extension(const char* name) {
    static const std::vector<VkExtensionProperties> exts = [] {
        uint32_t n = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &n, nullptr);
        std::vector<VkExtensionProperties> v(n);
        vkEnumerateInstanceExtensionProperties(nullptr, &n, v.data());
        return v;
    }();
    for (const auto& e : exts)
        if (std::strcmp(e.extensionName, name) == 0) return true;
    return false;
}

// A driver that only implements a subset of Vulkan (MoltenVK, the sole
// driver on macOS) is hidden from vkEnumeratePhysicalDevices unless the
// instance opts in, and vkCreateInstance fails outright with
// VK_ERROR_INCOMPATIBLE_DRIVER when it is the only one installed. Opting in
// costs nothing where no such driver exists: the extension is simply absent.
//
// Both instances this file creates -- the throwaway one in
// enumerate_devices() and the real one in Context::init() -- go through here,
// so they see the same device list.
void enable_portability(VkInstanceCreateInfo& ici,
                        std::vector<const char*>& exts) {
    if (!has_instance_extension(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
        return;
    exts.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    ici.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    ici.enabledExtensionCount = (uint32_t)exts.size();
    ici.ppEnabledExtensionNames = exts.data();
}

// MoltenVK tuning, on every instance this file creates. The names reach a
// layer no other driver answers to, so this is inert everywhere else.
//
// Fast math: MoltenVK hands the Metal compiler -ffast-math for every shader
// unless told otherwise, which is a wider licence than the CUDA build takes.
// With it on, msloss_parity, optimgeo_parity and meshing_parity all exceed
// their tolerance against the CUDA reference, and all three pass with it off.
// Correctness first; SS_VK_FAST_MATH=1 puts it back for A/B timing.
//
// Log level: MoltenVK narrates instance and device lifetime at Info (2). The
// enumeration instance alone would print two lines per run, so hold it to
// errors (1) unless SS_VK_VERBOSE asks for the rest.
struct MvkSettings {
    int32_t fast_math;
    int32_t log_level;
    VkLayerSettingEXT items[2];
    VkLayerSettingsCreateInfoEXT create_info{
        VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT};

    MvkSettings() {
        const char* fm = spirula::env("VK_FAST_MATH");
        fast_math = fm && fm[0] == '1' ? 1 : 0;
        log_level = spirula::env("VK_VERBOSE") ? 2 : 1;
        items[0] = {"MoltenVK", "MVK_CONFIG_FAST_MATH_ENABLED",
                    VK_LAYER_SETTING_TYPE_INT32_EXT, 1, &fast_math};
        items[1] = {"MoltenVK", "MVK_CONFIG_LOG_LEVEL",
                    VK_LAYER_SETTING_TYPE_INT32_EXT, 1, &log_level};
        create_info.settingCount = (uint32_t)std::size(items);
        create_info.pSettings = items;
    }
};

// `s` must outlive the vkCreateInstance call it is attached to.
void enable_mvk_settings(VkInstanceCreateInfo& ici,
                         std::vector<const char*>& exts, MvkSettings& s) {
    if (!has_instance_extension(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME)) return;
    exts.push_back(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME);
    ici.enabledExtensionCount = (uint32_t)exts.size();
    ici.ppEnabledExtensionNames = exts.data();
    s.create_info.pNext = ici.pNext;
    ici.pNext = &s.create_info;
}

// --- device enumeration / selection (backs the backend::device_* API) ---
// Physical devices are addressed by their vkEnumeratePhysicalDevices index;
// enumeration uses a throwaway instance so listing devices never initializes
// the Context singleton (or its VkDevice). The index is assumed stable
// across instances of the same loader — standard practice (VkSplat does the
// same).

struct EnumeratedDevice {
    std::string name;
    VkPhysicalDeviceType type = VK_PHYSICAL_DEVICE_TYPE_OTHER;
    uint64_t vram = 0;
    bool usable = false;
};

std::atomic<int> g_requested_device{-1};  // backend::device_select
std::atomic<bool> g_context_created{false};
std::atomic<int> g_context_device{-1};

const std::vector<EnumeratedDevice>& enumerate_devices() {
    static const std::vector<EnumeratedDevice> list = [] {
        std::vector<EnumeratedDevice> out;
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName = "Spirula Studio";
        app.apiVersion = VK_API_VERSION_1_2;
        VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        ici.pApplicationInfo = &app;
        std::vector<const char*> inst_exts;
        enable_portability(ici, inst_exts);
        MvkSettings mvk;
        enable_mvk_settings(ici, inst_exts, mvk);
        VkInstance inst = VK_NULL_HANDLE;
        if (vkCreateInstance(&ici, nullptr, &inst) != VK_SUCCESS) return out;
        uint32_t n = 0;
        vkEnumeratePhysicalDevices(inst, &n, nullptr);
        std::vector<VkPhysicalDevice> devices(n);
        vkEnumeratePhysicalDevices(inst, &n, devices.data());
        for (uint32_t i = 0; i < n; i++) {
            DeviceProbe p = probe_device(devices[i]);
            EnumeratedDevice d;
            d.name = p.props.deviceName;
            d.type = p.props.deviceType;
            d.vram = (uint64_t)device_local_vram(devices[i]);
            d.usable = p.required_ok;
            out.push_back(std::move(d));
        }
        vkDestroyInstance(inst, nullptr);
        return out;
    }();
    return list;
}

// Selection precedence: backend::device_select > SS_VK_DEVICE env
// (index or name substring) > auto-score (discrete > integrated > others,
// VRAM tie-break). -1 if nothing usable matches.
int resolve_device_index() {
    const std::vector<EnumeratedDevice>& list = enumerate_devices();
    int req = g_requested_device.load();
    if (req >= 0)
        return (req < (int)list.size() && list[req].usable) ? req : -1;
    if (const char* want = spirula::env("VK_DEVICE");
        want && want[0]) {
        char* end = nullptr;
        long idx = std::strtol(want, &end, 10);
        bool numeric = end && *end == '\0';
        for (int i = 0; i < (int)list.size(); i++) {
            bool matches = numeric
                ? (idx == (long)i)
                : (list[i].name.find(want) != std::string::npos);
            if (!matches) continue;
            if (!list[i].usable) {
                std::fprintf(stderr, "[spirula-vk] %s\n",
                    spirula::i18n::format(
                        spirula::i18n::msg::data::vk_device_lacks_features,
                        {list[i].name}).c_str());
                continue;
            }
            return i;
        }
        return -1;
    }
    int best = -1;
    long best_score = -1;
    for (int i = 0; i < (int)list.size(); i++) {
        if (!list[i].usable) continue;
        long score = device_type_score(list[i].type) * 1000000 +
                     (long)(list[i].vram >> 30);
        if (score > best_score) {
            best_score = score;
            best = i;
        }
    }
    return best;
}

}  // namespace

Context& Context::get() {
    static Context ctx;
    return ctx;
}

Context::Context() { init(); }

void Context::init() {
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "Spirula Studio";
    app.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    std::vector<const char*> inst_exts;
    enable_portability(ici, inst_exts);
    const char* validation = "VK_LAYER_KHRONOS_validation";
    if (const char* env = spirula::env("VK_VALIDATION");
        env && env[0] == '1') {
        if (has_instance_layer(validation)) {
            ici.enabledLayerCount = 1;
            ici.ppEnabledLayerNames = &validation;
        } else {
            std::fprintf(stderr,
                "[spirula-vk] SS_VK_VALIDATION=1 ignored: %s is not installed "
                "(it comes with the Vulkan SDK)\n", validation);
        }
    }

    MvkSettings mvk;
    enable_mvk_settings(ici, inst_exts, mvk);

    VkResult r = vkCreateInstance(&ici, nullptr, &_instance);
    if (r != VK_SUCCESS) {
        set_error("vkCreateInstance failed", r);
        return;
    }

    uint32_t n = 0;
    vkEnumeratePhysicalDevices(_instance, &n, nullptr);
    std::vector<VkPhysicalDevice> devices(n);
    vkEnumeratePhysicalDevices(_instance, &n, devices.data());
    if (n == 0) {
        set_error("no Vulkan devices found", VK_SUCCESS);
        return;
    }

    const int best = resolve_device_index();
    if (best < 0 || best >= (int)n) {
        set_error("no viable Vulkan device (need Vulkan 1.2 + "
                  "bufferDeviceAddress + timelineSemaphore)", VK_SUCCESS);
        return;
    }

    _physical = devices[best];
    const DeviceProbe probe = probe_device(_physical);
    if (!probe.required_ok) {  // paranoia: index drifted across instances
        set_error("selected Vulkan device lost required features", VK_SUCCESS);
        return;
    }
    _queue_family = probe.queue_family;
    _device_name = probe.props.deviceName;
    vkGetPhysicalDeviceMemoryProperties(_physical, &_mem_props);

    // Queue family timestamp support (for event timing).
    {
        uint32_t qn = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(_physical, &qn, nullptr);
        std::vector<VkQueueFamilyProperties> qf(qn);
        vkGetPhysicalDeviceQueueFamilyProperties(_physical, &qn, qf.data());
        _caps.timestamps = qf[_queue_family].timestampValidBits > 0;
    }
    _caps.timestamp_period_ns = probe.props.limits.timestampPeriod;
    _caps.subgroup_size = probe.subgroup.subgroupSize;
    _caps.max_push_constants = probe.props.limits.maxPushConstantsSize;
    _caps.max_workgroup_invocations =
        probe.props.limits.maxComputeWorkGroupInvocations;
    _caps.max_shared_memory = probe.props.limits.maxComputeSharedMemorySize;
    _caps.non_coherent_atom_size = probe.props.limits.nonCoherentAtomSize;
    _caps.float32_atomic_add = probe.atomic_float;
    _caps.shader_int64 = probe.shader_int64;
    _caps.shader_int8 = probe.shader_int8;

    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = _queue_family;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;

    VkPhysicalDeviceVulkan12Features f12{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    f12.bufferDeviceAddress = VK_TRUE;
    f12.timelineSemaphore = VK_TRUE;
    // Optional: native byte access for the ".int8" blob variants (both
    // features are core-optional in 1.2, no extension needed).
    if (_caps.shader_int8) {
        f12.shaderInt8 = VK_TRUE;
        f12.storageBuffer8BitAccess = VK_TRUE;
    }

    VkPhysicalDeviceShaderAtomicFloatFeaturesEXT fatomic{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT};
    fatomic.shaderBufferFloat32AtomicAdd = VK_TRUE;

    // SS_VK_NATIVE_ATOMICS=0 forces the CAS-loop shader variants on a
    // native-capable device (A/B timing, exercising the fallback blobs).
    // SS_VK_NATIVE_INT64=0 / SS_VK_NATIVE_INT8=0 likewise force the
    // ".noint64" emulation / word-packed byte-access blobs; the device
    // features stay enabled, only blob selection changes.
    if (const char* env = spirula::env("VK_NATIVE_ATOMICS");
        env && env[0] == '0')
        _caps.float32_atomic_add = false;
    if (const char* env = spirula::env("VK_NATIVE_INT64");
        env && env[0] == '0')
        _caps.shader_int64 = false;
    if (const char* env = spirula::env("VK_NATIVE_INT8");
        env && env[0] == '0')
        _caps.shader_int8 = false;

    std::vector<const char*> extensions;
    // Enabling this one is mandatory, not optional: the spec forbids creating
    // a device from a physical device that advertises it without it in the
    // list. Spelled out rather than using the header macro, which lives
    // behind VK_ENABLE_BETA_EXTENSIONS.
    if (has_extension(_physical, "VK_KHR_portability_subset"))
        extensions.push_back("VK_KHR_portability_subset");
    // Shader blobs carry NonSemantic debug info (slangc -g2, for
    // profiler source correlation); a 1.2 device must enable this extension
    // for SPV_KHR_non_semantic_info to be legal (core in 1.3). Universally
    // present on current drivers; stripped-debug fallback not needed.
    if (has_extension(_physical,
                      VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME))
        extensions.push_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
    if (_caps.float32_atomic_add &&
        has_extension(_physical, VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME)) {
        extensions.push_back(VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME);
        f12.pNext = &fatomic;
    } else {
        _caps.float32_atomic_add = false;
    }

    // VK_EXT_memory_budget: lets memory_usage() report system-wide usage and
    // budget (no feature struct, just enable it). Widely supported; absence
    // just leaves the "in use" figure unavailable.
    if (has_extension(_physical, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME)) {
        extensions.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
        _caps.memory_budget = true;
    }

    // Pin the compute subgroup size when the device lets us (see
    // Capabilities::required_subgroup_size).
    VkPhysicalDeviceSubgroupSizeControlFeaturesEXT fsgc{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT};
    if (has_extension(_physical,
                      VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME)) {
        VkPhysicalDeviceSubgroupSizeControlFeaturesEXT probe_sgc{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT};
        VkPhysicalDeviceFeatures2 probe_f2{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        probe_f2.pNext = &probe_sgc;
        vkGetPhysicalDeviceFeatures2(_physical, &probe_f2);

        VkPhysicalDeviceSubgroupSizeControlPropertiesEXT sgc_props{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES_EXT};
        VkPhysicalDeviceProperties2 sgc_p2{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
        sgc_p2.pNext = &sgc_props;
        vkGetPhysicalDeviceProperties2(_physical, &sgc_p2);

        if (probe_sgc.subgroupSizeControl && probe_sgc.computeFullSubgroups &&
            (sgc_props.requiredSubgroupSizeStages &
             VK_SHADER_STAGE_COMPUTE_BIT)) {
            uint32_t want = _caps.subgroup_size;
            if (want < sgc_props.minSubgroupSize)
                want = sgc_props.minSubgroupSize;
            if (want > sgc_props.maxSubgroupSize)
                want = sgc_props.maxSubgroupSize;
            if (want > 32)  // TODO: not supported by shaders
                want = 32;
            _caps.required_subgroup_size = want;
            extensions.push_back(VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME);
            fsgc.subgroupSizeControl = VK_TRUE;
            fsgc.computeFullSubgroups = VK_TRUE;
            fsgc.pNext = f12.pNext;
            f12.pNext = &fsgc;
        }
    }
    // Several kernels index subgroups as tid / WaveGetLaneCount() against a
    // 32-wide workgroup, so a wider unpinned subgroup makes the count 0 and
    // the result silently wrong (rasterize_bwd's survivor compaction).
    if (!_caps.required_subgroup_size && _caps.subgroup_size > 32) {
        std::fprintf(stderr,
            "[spirula-vk] warning: %s reports subgroup size %u and does not "
            "support VK_EXT_subgroup_size_control, which this build needs to "
            "pin it to 32. Training results on this device are not trusted.\n",
            _device_name.c_str(), _caps.subgroup_size);
    }

    VkPhysicalDeviceFeatures features{};
    features.shaderInt64 = probe.shader_int64 ? VK_TRUE : VK_FALSE;

    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.pNext = &f12;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.pEnabledFeatures = &features;
    dci.enabledExtensionCount = (uint32_t)extensions.size();
    dci.ppEnabledExtensionNames = extensions.data();

    r = vkCreateDevice(_physical, &dci, nullptr, &_device);
    if (r != VK_SUCCESS) {
        set_error("vkCreateDevice failed", r);
        _device = VK_NULL_HANDLE;
        return;
    }
    vkGetDeviceQueue(_device, _queue_family, 0, &_queue);

    VkSemaphoreTypeCreateInfo tci{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
    tci.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    tci.initialValue = 0;
    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    sci.pNext = &tci;
    r = vkCreateSemaphore(_device, &sci, nullptr, &_timeline);
    if (r != VK_SUCCESS) {
        set_error("vkCreateSemaphore (timeline) failed", r);
        vkDestroyDevice(_device, nullptr);
        _device = VK_NULL_HANDLE;
        return;
    }

    // Poll-based waits by default on real GPUs; SS_VK_POLL_WAIT=0/1
    // forces either mode (mainly for A/B timing).
    _poll_waits =
        probe.props.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU;
    if (const char* env = spirula::env("VK_POLL_WAIT"); env && env[0])
        _poll_waits = env[0] != '0';

    g_context_device.store(best);
    g_context_created.store(true);

    if (spirula::env("VK_VERBOSE")) {
        // The pinned size is what the shaders actually run at; printing the
        // device default alone reads as "the pin did not happen".
        char subgroup[48];
        if (_caps.required_subgroup_size == _caps.subgroup_size)
            std::snprintf(subgroup, sizeof(subgroup), "%u (pinned)",
                          _caps.required_subgroup_size);
        else if (_caps.required_subgroup_size)
            std::snprintf(subgroup, sizeof(subgroup),
                          "%u (pinned, device default %u)",
                          _caps.required_subgroup_size, _caps.subgroup_size);
        else
            std::snprintf(subgroup, sizeof(subgroup), "%u (unpinned)",
                          _caps.subgroup_size);
        std::fprintf(stderr,
            "[spirula-vk] using %s (%s), subgroup %s, push %uB, "
            "float-atomic-add %s, int64 %s, int8 %s, timestamps %s\n",
            _device_name.c_str(), device_type_name(probe.props.deviceType),
            subgroup, _caps.max_push_constants,
            _caps.float32_atomic_add ? "native" : "EMULATED",
            _caps.shader_int64 ? "native" : "EMULATED",
            _caps.shader_int8 ? "native" : "emulated",
            _caps.timestamps ? "yes" : "no");
    }
}

Context::~Context() {
    if (_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(_device);
        if (_shutdown_hook) _shutdown_hook();
        vkDestroySemaphore(_device, _timeline, nullptr);
        vkDestroyDevice(_device, nullptr);
    }
    if (_instance != VK_NULL_HANDLE) vkDestroyInstance(_instance, nullptr);
}

uint64_t Context::submit(VkCommandBuffer cb) {
    std::lock_guard<std::mutex> lock(_submit_mutex);
    uint64_t value = _last_value + 1;

    VkTimelineSemaphoreSubmitInfo tsi{
        VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
    tsi.signalSemaphoreValueCount = 1;
    tsi.pSignalSemaphoreValues = &value;

    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.pNext = &tsi;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &_timeline;

    VkResult r = vkQueueSubmit(_queue, 1, &si, VK_NULL_HANDLE);
    if (r != VK_SUCCESS) {
        set_error("vkQueueSubmit failed", r);
        return 0;
    }
    _last_value = value;
    return value;
}

bool Context::wait(uint64_t value) {
    if (value == 0) return true;
    // Spin on the counter before the blocking wait to save the park/wake
    // round trip. It cannot replace that wait: reading the counter is a query,
    // not the host domain operation that makes device writes visible.
    if (_poll_waits) {
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds(100);
        do {
            uint64_t current = 0;
            VkResult r =
                vkGetSemaphoreCounterValue(_device, _timeline, &current);
            if (r != VK_SUCCESS) {
                set_error("vkGetSemaphoreCounterValue failed", r);
                return false;
            }
            if (current >= value) break;
            std::this_thread::yield();
        } while (std::chrono::steady_clock::now() < deadline);
    }
    VkSemaphoreWaitInfo wi{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
    wi.semaphoreCount = 1;
    wi.pSemaphores = &_timeline;
    wi.pValues = &value;
    VkResult r = vkWaitSemaphores(_device, &wi, UINT64_MAX);
    if (r != VK_SUCCESS) {
        set_error("vkWaitSemaphores failed", r);
        return false;
    }
    return true;
}

uint32_t Context::find_memory_type(uint32_t type_bits,
                                   VkMemoryPropertyFlags required) const {
    for (uint32_t i = 0; i < _mem_props.memoryTypeCount; i++) {
        if (!(type_bits & (1u << i))) continue;
        if ((_mem_props.memoryTypes[i].propertyFlags & required) == required)
            return i;
    }
    return UINT32_MAX;
}

}  // namespace vk

// --- backend::device_* (BackendRuntime.h) ---

int device_count() { return (int)vk::enumerate_devices().size(); }

DeviceInfo device_info(int index) {
    DeviceInfo info{};
    info.type = "other";
    const auto& list = vk::enumerate_devices();
    if (index < 0 || index >= (int)list.size()) return info;
    const auto& d = list[index];
    std::snprintf(info.name, sizeof(info.name), "%s", d.name.c_str());
    info.type = vk::device_type_name(d.type);
    info.vram_bytes = d.vram;
    info.usable = d.usable;
    return info;
}

bool device_select(int index) {
    const auto& list = vk::enumerate_devices();
    if (index < 0 || index >= (int)list.size() || !list[index].usable)
        return false;
    // After the context exists the device cannot change; selecting the one
    // already in use is a no-op success.
    if (vk::g_context_created.load())
        return vk::g_context_device.load() == index;
    vk::g_requested_device.store(index);
    return true;
}

int device_current() {
    if (vk::g_context_created.load()) return vk::g_context_device.load();
    return vk::resolve_device_index();
}

MemoryUsage memory_usage() {
    MemoryUsage m;
    const int idx = device_current();
    const auto& list = vk::enumerate_devices();
    if (idx >= 0 && idx < (int)list.size() && list[idx].vram > 0) {
        m.total_bytes = list[idx].vram;
        m.has_total = true;
    }
    m.process_bytes = vk::g_device_bytes.load(std::memory_order_relaxed);
    m.has_process = true;

    // System-wide "in use" needs a live device with VK_EXT_memory_budget.
    // heapBudget already discounts memory held by other applications, so
    //   system_free ~= sum(budget - usage) over device-local heaps
    //   system_used  = total - system_free
    // (an estimate; Vulkan exposes no exact system-wide counter). Never call
    // Context::get() before it exists — that would create the device just to
    // read a status number.
    if (vk::g_context_created.load() && vk::Context::get().ok() &&
        vk::Context::get().caps().memory_budget && m.has_total) {
        VkPhysicalDeviceMemoryBudgetPropertiesEXT budget{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
        VkPhysicalDeviceMemoryProperties2 mp2{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
        mp2.pNext = &budget;
        vkGetPhysicalDeviceMemoryProperties2(vk::Context::get().physical(),
                                             &mp2);
        uint64_t free_head = 0;
        const auto& mp = mp2.memoryProperties;
        for (uint32_t i = 0; i < mp.memoryHeapCount; i++) {
            if (!(mp.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT))
                continue;
            const uint64_t b = budget.heapBudget[i], u = budget.heapUsage[i];
            if (b > u) free_head += b - u;
        }
        if (free_head <= m.total_bytes) {
            m.used_bytes = m.total_bytes - free_head;
            m.has_used = true;
        }
    }
    return m;
}

}  // namespace backend
