// Minimal Vulkan compute context: device setup, storage buffers with chunked
// staging upload/download, a single shared descriptor set, named compute
// pipelines from one SPIR-V module, and command recording helpers.
#pragma once

#include <atomic>

#include <chrono>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include "core/Env.h"
#include "core/SourcePath.h"

#include "sfm/core/Log.h"
#include "i18n/catalog/Sfm.h"

// The result codes worth naming: the ones a user can act on. Everything else
// prints its number. Out-of-memory in particular used to surface as a bare
// "Vulkan error -2" from a buffer allocation, which says nothing about the
// 3681-image model or the other job sharing the GPU that caused it.
inline const char* vkResultName(int r) {
    switch (r) {
        case -1: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case -2: return "VK_ERROR_OUT_OF_DEVICE_MEMORY (the GPU ran out of memory -- "
                        "a smaller --quality, a --vram-budget, or not sharing the device "
                        "with another job)";
        case -3: return "VK_ERROR_INITIALIZATION_FAILED";
        case -4: return "VK_ERROR_DEVICE_LOST";
        case -5: return "VK_ERROR_MEMORY_MAP_FAILED";
        case -8: return "VK_ERROR_FEATURE_NOT_PRESENT";
        case -9: return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case -11: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        default: return "";
    }
}

inline bool hasDeviceExtension(VkPhysicalDevice phys, const char* name) {
    uint32_t n = 0;
    vkEnumerateDeviceExtensionProperties(phys, nullptr, &n, nullptr);
    std::vector<VkExtensionProperties> exts(n);
    vkEnumerateDeviceExtensionProperties(phys, nullptr, &n, exts.data());
    for (const auto& e : exts)
        if (std::strcmp(e.extensionName, name) == 0) return true;
    return false;
}

// A driver implementing only a subset of Vulkan (MoltenVK, the sole driver on
// macOS) stays hidden from vkEnumeratePhysicalDevices unless the instance opts
// in, and vkCreateInstance fails with VK_ERROR_INCOMPATIBLE_DRIVER when it is
// the only one installed. Where no such driver exists the extension is absent
// and this does nothing. `exts` must outlive the vkCreateInstance call.
inline void vkEnablePortability(VkInstanceCreateInfo& ici,
                                std::vector<const char*>& exts) {
    uint32_t n = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &n, nullptr);
    std::vector<VkExtensionProperties> avail(n);
    vkEnumerateInstanceExtensionProperties(nullptr, &n, avail.data());
    for (const auto& e : avail) {
        if (std::strcmp(e.extensionName,
                        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) != 0)
            continue;
        exts.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        ici.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        ici.enabledExtensionCount = (uint32_t)exts.size();
        ici.ppEnabledExtensionNames = exts.data();
        return;
    }
}

// A failed call, thrown rather than exited on: a lost device or a refused
// allocation ends one bundle adjustment, not the reconstruction, and
// sfm/map/Bundle.h re-runs the solve on the host.
struct VkError : std::runtime_error {
    VkError(int r, const std::string& what) : std::runtime_error(what), result(r) {}
    int result;
};

// Losing the device, or being refused memory, says nothing about whether the
// same work fits on the host; every other code is a bug in the caller.
inline bool vkErrorIsResourceFailure(int r) {
    return r == VK_ERROR_DEVICE_LOST || r == VK_ERROR_OUT_OF_DEVICE_MEMORY ||
           r == VK_ERROR_OUT_OF_HOST_MEMORY;
}

inline std::string vkErrorText(int r, const char* file, int line) {
    char buf[512];
    snprintf(buf, sizeof buf, "Vulkan error %d %s at %s:%d", r, vkResultName(r), file, line);
    return buf;
}

#define VK_CHECK(x) do { VkResult _r = (x); if (_r != VK_SUCCESS) \
    throw VkError((int)_r, vkErrorText((int)_r, SS_FILE, __LINE__)); } while (0)

// Must match the push constant block in sfm/shaders/ba/ba.slang.
struct Push {
    uint32_t u0 = 0, u1 = 0, u2 = 0, u3 = 0;
    float f0 = 0, f1 = 0, f2 = 0, f3 = 0;
    // Appended, so the first 32 bytes keep the layout every existing shader
    // declares; the matcher needs more than four integer slots. Vulkan
    // guarantees at least 128 bytes of push constants, and a shader may declare
    // a smaller range than the pipeline layout provides.
    uint32_t u4 = 0, u5 = 0, u6 = 0, u7 = 0;
};

struct GpuBuffer {
    VkBuffer buf = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
};

struct VkContextOptions {
    bool needFloat64 = false;
    bool needFloatAtomics = false;   // VK_EXT_shader_atomic_float (f32 add, and f64 add if needFloat64)
    bool needInt64Atomics = false;   // buffer int64 atomics (emulated-double CAS)
    bool needIntDotProduct = false;  // VK_KHR_shader_integer_dot_product (packed uint8x4, matcher)
    int deviceIndex = -1;            // -1 = prefer discrete GPU
    bool validate = false;
    bool profile = false;            // per-dispatch GPU timestamps, aggregated by pipeline name
};

// What a device can actually do, as far as this module cares. Integrated parts
// are not a subset of discrete ones: Intel's Xe iGPU has int64 buffer atomics
// but neither fp64 nor a float32 atomic add, which is the exact opposite of
// what a "smaller GPU has fewer features" reading would predict. So the BA
// scalar type has to be chosen from a probe (see pickRealForDevice), not assumed
// -- vkCreateDevice returns VK_ERROR_FEATURE_NOT_PRESENT for anything asked
// for and missing, and that used to abort a reconstruction at the first solve.
struct VkDeviceCaps {
    bool float64 = false;           // shaderFloat64
    bool float32AtomicAdd = false;  // VK_EXT_shader_atomic_float, buffer f32 add
    bool float64AtomicAdd = false;  // ... and its f64 counterpart
    bool int64Atomics = false;      // shaderInt64 + shaderBufferInt64Atomics
    bool intDotProduct = false;     // VK_KHR_shader_integer_dot_product
    // ... and whether the packed uint8x4 form is a hardware instruction. A
    // driver may emulate it (MoltenVK does, worse than our own spelling: 114 vs
    // 43 ms per 8192x8192 pair on an M2), so this is what picks the blob.
    bool intDotProductFast = false;
};

class VkContext {
public:
    VkContext() = default;
    // Owners hold VkContext by value and hand out GpuBuffers freely; a copy would
    // double-destroy every handle.
    VkContext(const VkContext&) = delete;
    VkContext& operator=(const VkContext&) = delete;

    // Full teardown. Every buffer ever created through this context is freed
    // here (createBufferRaw tracks them), so a scoped VkContext -- the mapper's
    // per-global-BA solver, the prefilter's matcher -- returns its VRAM when
    // it dies instead of leaking it for the life of the process. Before this,
    // a 1363-image run OOMed: each of the mapper's periodic global BAs leaked
    // a device plus problem-sized buffers.
    ~VkContext() {
        if (device_ == VK_NULL_HANDLE) {
            if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);
            return;
        }
        vkDeviceWaitIdle(device_);
        for (auto& p : pipelines_) vkDestroyPipeline(device_, p.second, nullptr);
        if (shaderModule_ != VK_NULL_HANDLE) vkDestroyShaderModule(device_, shaderModule_, nullptr);
        if (descPool_ != VK_NULL_HANDLE) vkDestroyDescriptorPool(device_, descPool_, nullptr);
        if (pipeLayout_ != VK_NULL_HANDLE) vkDestroyPipelineLayout(device_, pipeLayout_, nullptr);
        if (setLayout_ != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device_, setLayout_, nullptr);
        if (queryPool_ != VK_NULL_HANDLE) vkDestroyQueryPool(device_, queryPool_, nullptr);
        if (stagingPtr_) vkUnmapMemory(device_, staging_.mem);
        if (stagingDlPtr_) vkUnmapMemory(device_, stagingDl_.mem);
        for (auto& a : allocations_) {
            vkDestroyBuffer(device_, a.first, nullptr);
            vkFreeMemory(device_, a.second, nullptr);
        }
        if (fence_ != VK_NULL_HANDLE) vkDestroyFence(device_, fence_, nullptr);
        if (cmdPool_ != VK_NULL_HANDLE) vkDestroyCommandPool(device_, cmdPool_, nullptr);
        vkDestroyDevice(device_, nullptr);
        vkDestroyInstance(instance_, nullptr);
    }

    // The device this context runs on, as probed at init(). Zeroed before then.
    const VkDeviceCaps& caps() const { return caps_; }

    // Same features, without owning a context: creates a throwaway instance,
    // picks the device `deviceIndex` would pick and asks it what it has. For
    // callers that must choose a code path (a BA scalar type, say) before any
    // context exists. Returns all-false if there is no usable device.
    static VkDeviceCaps probeCaps(int deviceIndex) {
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName = "vk_ba_probe";
        app.apiVersion = VK_API_VERSION_1_2;
        VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        ici.pApplicationInfo = &app;
        std::vector<const char*> instExts;
        vkEnablePortability(ici, instExts);
        VkInstance inst = VK_NULL_HANDLE;
        if (vkCreateInstance(&ici, nullptr, &inst) != VK_SUCCESS) return {};
        VkPhysicalDevice phys = choosePhysical(inst, deviceIndex);
        VkDeviceCaps c = phys == VK_NULL_HANDLE ? VkDeviceCaps{} : queryCaps(phys);
        vkDestroyInstance(inst, nullptr);
        return c;
    }

    void init(const VkContextOptions& opt) {
        // ---- instance ----
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.pApplicationName = "vk_ba";
        app.apiVersion = VK_API_VERSION_1_2;
        VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        ici.pApplicationInfo = &app;
        std::vector<const char*> instExts;
        vkEnablePortability(ici, instExts);
        const char* layers[] = {"VK_LAYER_KHRONOS_validation"};
        if (opt.validate) {
            ici.enabledLayerCount = 1;
            ici.ppEnabledLayerNames = layers;
        }
        VK_CHECK(vkCreateInstance(&ici, nullptr, &instance_));

        // ---- physical device ----
        phys_ = choosePhysical(instance_, opt.deviceIndex);
        if (phys_ == VK_NULL_HANDLE) throw std::runtime_error("no Vulkan devices");
        caps_ = queryCaps(phys_);
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(phys_, &props);
        // Once per process. The atom workers of a bottom-up run create a
        // context each, and eight identical banner lines in the middle of a
        // reconstruction say nothing the first one did not.
        static std::atomic<bool> announced{false};
        if (!announced.exchange(true))
            sfm::slog::out(sfm::slog::Tag::Device,
                           spirula::i18n::msg::sfm::device_using, {props.deviceName});

        // ---- queue family ----
        uint32_t qn = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(phys_, &qn, nullptr);
        std::vector<VkQueueFamilyProperties> qf(qn);
        vkGetPhysicalDeviceQueueFamilyProperties(phys_, &qn, qf.data());
        queueFamily_ = ~0u;
        for (uint32_t i = 0; i < qn; i++)
            if (qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { queueFamily_ = i; break; }
        if (queueFamily_ == ~0u) throw std::runtime_error("no compute queue");

        // ---- device ----
        // Everything asked for below has to be there: vkCreateDevice fails the
        // whole call with VK_ERROR_FEATURE_NOT_PRESENT otherwise. So report a
        // missing feature as a missing feature, by name, instead of letting the
        // driver turn it into an unattributable error code.
        auto require = [&](bool want, bool have, const char* what) {
            if (want && !have)
                throw std::runtime_error(
                    std::string("device '") + props.deviceName +
                    "' does not support " + what +
                    ", which this stage needs (pick another device with "
                    "--device, or see `spirula sfm auto --help` for the "
                    "scalar-type flags)");
        };
        require(opt.needFloat64, caps_.float64, "fp64 shader arithmetic");
        require(opt.needFloatAtomics, caps_.float32AtomicAdd,
                "buffer float32 atomic add");
        require(opt.needFloatAtomics && opt.needFloat64, caps_.float64AtomicAdd,
                "buffer float64 atomic add");
        require(opt.needInt64Atomics, caps_.int64Atomics,
                "buffer int64 atomics");
        require(opt.needIntDotProduct, caps_.intDotProduct,
                "integer dot product");

        float prio = 1.0f;
        VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        qci.queueFamilyIndex = queueFamily_;
        qci.queueCount = 1;
        qci.pQueuePriorities = &prio;

        VkPhysicalDeviceFeatures2 feat2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        feat2.features.shaderFloat64 = opt.needFloat64 ? VK_TRUE : VK_FALSE;
        feat2.features.shaderInt64 = opt.needInt64Atomics ? VK_TRUE : VK_FALSE;

        VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        f12.shaderBufferInt64Atomics = opt.needInt64Atomics ? VK_TRUE : VK_FALSE;
        feat2.pNext = &f12;

        VkPhysicalDeviceShaderAtomicFloatFeaturesEXT fAtom{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT};
        std::vector<const char*> exts;
        // Mandatory where advertised: the spec forbids creating a device from
        // a portability physical device without it. Spelled out rather than
        // using the macro, which is behind VK_ENABLE_BETA_EXTENSIONS.
        if (hasDeviceExtension(phys_, "VK_KHR_portability_subset"))
            exts.push_back("VK_KHR_portability_subset");
        if (opt.needFloatAtomics) {
            fAtom.shaderBufferFloat32Atomics = VK_TRUE;
            fAtom.shaderBufferFloat32AtomicAdd = VK_TRUE;
            if (opt.needFloat64) {
                fAtom.shaderBufferFloat64Atomics = VK_TRUE;
                fAtom.shaderBufferFloat64AtomicAdd = VK_TRUE;
            }
            fAtom.pNext = feat2.pNext;
            feat2.pNext = &fAtom;
            exts.push_back(VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME);
        }

        // Packed uint8x4 dot product for the descriptor matcher (D21). Core in
        // Vulkan 1.3; we ask for 1.2, so take it as an extension.
        VkPhysicalDeviceShaderIntegerDotProductFeatures fDot{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_FEATURES};
        if (opt.needIntDotProduct) {
            fDot.shaderIntegerDotProduct = VK_TRUE;
            fDot.pNext = feat2.pNext;
            feat2.pNext = &fDot;
            exts.push_back(VK_KHR_SHADER_INTEGER_DOT_PRODUCT_EXTENSION_NAME);
        }

        VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        dci.pNext = &feat2;
        dci.queueCreateInfoCount = 1;
        dci.pQueueCreateInfos = &qci;
        dci.enabledExtensionCount = (uint32_t)exts.size();
        dci.ppEnabledExtensionNames = exts.data();
        VK_CHECK(vkCreateDevice(phys_, &dci, nullptr, &device_));
        vkGetDeviceQueue(device_, queueFamily_, 0, &queue_);

        vkGetPhysicalDeviceMemoryProperties(phys_, &memProps_);

        VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cpi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cpi.queueFamilyIndex = queueFamily_;
        VK_CHECK(vkCreateCommandPool(device_, &cpi, nullptr, &cmdPool_));

        VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        VK_CHECK(vkCreateFence(device_, &fci, nullptr, &fence_));

        // Two staging buffers, because the two directions want opposite memory.
        // The first HOST_VISIBLE|HOST_COHERENT type on a discrete GPU is
        // write-combined: excellent to write through, and *uncached to read* --
        // a memcpy out of it runs at a small fraction of RAM speed. Every
        // readback in the pipeline goes through this path (match results, SIFT
        // keypoints and descriptors, BA parameters), so downloads get their own
        // buffer that asks for HOST_CACHED, and fall back to the shared type on
        // a device that has no such heap.
        staging_ = createBufferRaw(kStagingSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VK_CHECK(vkMapMemory(device_, staging_.mem, 0, kStagingSize, 0, &stagingPtr_));
        stagingDl_ = createBufferRaw(kStagingSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
        VK_CHECK(vkMapMemory(device_, stagingDl_.mem, 0, kStagingSize, 0, &stagingDlPtr_));

        if (opt.profile) {
            profiling_ = true;
            timestampPeriod_ = props.limits.timestampPeriod;
            VkQueryPoolCreateInfo qpi{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
            qpi.queryType = VK_QUERY_TYPE_TIMESTAMP;
            qpi.queryCount = kMaxQueries;
            VK_CHECK(vkCreateQueryPool(device_, &qpi, nullptr, &queryPool_));
        }
    }

    GpuBuffer createBuffer(VkDeviceSize size) {
        GpuBuffer b = createBufferRaw(size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        totalAllocated_ += size;
        return b;
    }

    // Free one buffer early (a long-lived context whose users come and go --
    // the persistent BA solver -- must not defer everything to ~VkContext). Safe
    // immediately after any submit(): submits are fenced synchronous, so no
    // GPU work can still reference the buffer.
    void destroyBuffer(GpuBuffer& b) {
        if (b.buf == VK_NULL_HANDLE) return;
        vkDestroyBuffer(device_, b.buf, nullptr);
        vkFreeMemory(device_, b.mem, nullptr);
        for (size_t i = 0; i < allocations_.size(); i++)
            if (allocations_[i].first == b.buf) {
                allocations_.erase(allocations_.begin() + i);
                break;
            }
        totalAllocated_ -= b.size;
        b = GpuBuffer{};
    }

    bool initialized() const { return device_ != VK_NULL_HANDLE; }
    bool hasPipeline(const std::string& name) const { return pipelines_.count(name) != 0; }

    void upload(const GpuBuffer& dst, const void* src, VkDeviceSize size, VkDeviceSize dstOff = 0) {
        const uint8_t* p = (const uint8_t*)src;
        for (VkDeviceSize off = 0; off < size; off += kStagingSize) {
            VkDeviceSize chunk = std::min<VkDeviceSize>(kStagingSize, size - off);
            memcpy(stagingPtr_, p + off, chunk);
            VkCommandBuffer cb = begin();
            VkBufferCopy c{0, dstOff + off, chunk};
            vkCmdCopyBuffer(cb, staging_.buf, dst.buf, 1, &c);
            submit(cb);
        }
    }

    // Several host->device copies in one submit.
    //
    // `upload` fences per call, and a bundle-adjustment problem uploads
    // seventeen buffers. Uncontended that is a few milliseconds; with a solver
    // per thread on one device -- the atom phase of a bottom-up run -- the
    // fences serialize against each other and it becomes most of a small
    // solve's cost (measured: 3 ms of upload per solve at one thread, 69 ms at
    // eight). Everything that fits in the staging buffer goes in one command
    // buffer; the rest flushes and starts a new one, so an item larger than
    // the staging buffer still works, by the chunked path.
    struct UploadItem {
        const GpuBuffer* dst;
        const void* src;
        VkDeviceSize size;
    };
    void uploadMany(const UploadItem* items, size_t n) {
        VkCommandBuffer cb = VK_NULL_HANDLE;
        VkDeviceSize used = 0;
        std::vector<VkBufferCopy> copies;
        std::vector<VkBuffer> dsts;
        auto flush = [&] {
            if (cb == VK_NULL_HANDLE) return;
            for (size_t i = 0; i < copies.size(); i++)
                vkCmdCopyBuffer(cb, staging_.buf, dsts[i], 1, &copies[i]);
            submit(cb);
            cb = VK_NULL_HANDLE;
            used = 0;
            copies.clear();
            dsts.clear();
        };
        for (size_t i = 0; i < n; i++) {
            const UploadItem& it = items[i];
            if (!it.dst || !it.size) continue;
            if (it.size > kStagingSize) {  // does not fit whole: the chunked path
                flush();
                upload(*it.dst, it.src, it.size);
                continue;
            }
            if (used + it.size > kStagingSize) flush();
            if (cb == VK_NULL_HANDLE) cb = begin();
            memcpy((uint8_t*)stagingPtr_ + used, it.src, it.size);
            copies.push_back(VkBufferCopy{used, 0, it.size});
            dsts.push_back(it.dst->buf);
            // Keep every source offset 16-byte aligned: cheap, and it stops a
            // driver from taking a slow path on an unaligned copy.
            used = (used + it.size + 15) & ~(VkDeviceSize)15;
        }
        flush();
    }

    void download(const GpuBuffer& src, void* dst, VkDeviceSize size, VkDeviceSize srcOff = 0) {
        uint8_t* p = (uint8_t*)dst;
        for (VkDeviceSize off = 0; off < size; off += kStagingSize) {
            VkDeviceSize chunk = std::min<VkDeviceSize>(kStagingSize, size - off);
            VkCommandBuffer cb = begin();
            VkBufferCopy c{srcOff + off, 0, chunk};
            vkCmdCopyBuffer(cb, src.buf, stagingDl_.buf, 1, &c);
            submit(cb);
            memcpy(p + off, stagingDlPtr_, chunk);
        }
    }

    // Fold a readback into a command buffer the caller is already recording:
    // record the copy with recordDownload(), submit once, then take the bytes
    // with stagingDownloadPtr(). Saves the extra submit-and-fence that a
    // separate download() costs after every dispatch batch. `size` must not
    // exceed stagingCapacity().
    void recordDownload(VkCommandBuffer cb, const GpuBuffer& src, VkDeviceSize size,
                        VkDeviceSize srcOff = 0, VkDeviceSize dstOff = 0) {
        VkBufferCopy c{srcOff, dstOff, size};
        vkCmdCopyBuffer(cb, src.buf, stagingDl_.buf, 1, &c);
    }
    const void* stagingDownloadPtr() const { return stagingDlPtr_; }
    static constexpr VkDeviceSize stagingCapacity() { return kStagingSize; }

    // ---- descriptor set (one set of N storage buffers shared by all pipelines) ----
    // Idempotent: the first call builds layout/pool/set, later calls (same
    // binding count) just rewrite the buffer bindings -- how a persistent
    // context re-targets the pipelines at a new problem's buffers. Safe
    // between submits (fenced synchronous, never while recording).
    void createDescriptors(const std::vector<VkBuffer>& buffers) {
        uint32_t nb = (uint32_t)buffers.size();
        if (setLayout_ != VK_NULL_HANDLE) {
            if (nb != descBindingCount_)
                throw std::runtime_error("createDescriptors: binding count changed");
            writeDescriptors(buffers);
            return;
        }
        descBindingCount_ = nb;
        std::vector<VkDescriptorSetLayoutBinding> binds(nb);
        for (uint32_t i = 0; i < nb; i++)
            binds[i] = {i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        li.bindingCount = nb;
        li.pBindings = binds.data();
        VK_CHECK(vkCreateDescriptorSetLayout(device_, &li, nullptr, &setLayout_));

        VkPushConstantRange pcr{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push)};
        VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pli.setLayoutCount = 1;
        pli.pSetLayouts = &setLayout_;
        pli.pushConstantRangeCount = 1;
        pli.pPushConstantRanges = &pcr;
        VK_CHECK(vkCreatePipelineLayout(device_, &pli, nullptr, &pipeLayout_));

        VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nb};
        VkDescriptorPoolCreateInfo dpi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        dpi.maxSets = 1;
        dpi.poolSizeCount = 1;
        dpi.pPoolSizes = &ps;
        VK_CHECK(vkCreateDescriptorPool(device_, &dpi, nullptr, &descPool_));

        VkDescriptorSetAllocateInfo dai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        dai.descriptorPool = descPool_;
        dai.descriptorSetCount = 1;
        dai.pSetLayouts = &setLayout_;
        VK_CHECK(vkAllocateDescriptorSets(device_, &dai, &descSet_));
        writeDescriptors(buffers);
    }

    void writeDescriptors(const std::vector<VkBuffer>& buffers) {
        uint32_t nb = (uint32_t)buffers.size();
        std::vector<VkDescriptorBufferInfo> infos(nb);
        std::vector<VkWriteDescriptorSet> writes(nb);
        for (uint32_t i = 0; i < nb; i++) {
            infos[i] = {buffers[i], 0, VK_WHOLE_SIZE};
            writes[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[i].dstSet = descSet_;
            writes[i].dstBinding = i;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[i].pBufferInfo = &infos[i];
        }
        vkUpdateDescriptorSets(device_, nb, writes.data(), 0, nullptr);
    }

    void loadPipelines(const std::string& spvPath, const std::vector<std::string>& entries) {
        std::ifstream f(spvPath, std::ios::binary | std::ios::ate);
        if (!f) throw std::runtime_error("cannot open " + spvPath);
        size_t sz = (size_t)f.tellg();
        f.seekg(0);
        std::vector<char> code(sz);
        f.read(code.data(), sz);
        loadPipelines((const uint32_t*)code.data(), sz, entries);
    }

    // One module holding every entry point (slangc -fvk-use-entrypoint-name),
    // either read from disk above or taken from the embedded blobs. Entry
    // points already created are skipped, so a caller may come back for more
    // as it discovers it needs them -- which is the point: compiling one is
    // ~90 ms on a cold driver cache, and the bundle-adjustment module has
    // thirty-odd, most of them camera models a given problem never uses.
    void loadPipelines(const uint32_t* code, size_t codeBytes,
                       const std::vector<std::string>& entries) {
        if (shaderModule_ == VK_NULL_HANDLE) {
            VkShaderModuleCreateInfo smi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            smi.codeSize = codeBytes;
            smi.pCode = code;
            VK_CHECK(vkCreateShaderModule(device_, &smi, nullptr, &shaderModule_));
        }

        // Compiling the BA module is seconds of wall clock on a cold driver
        // cache, and it is not evenly spread: SS_SFM_MAP_PROF names the
        // entry points that cost more than 100 ms, which is how you find out
        // that one kernel is carrying the whole bill.
        const bool prof = spirula::env("SFM_MAP_PROF") != nullptr;
        for (const auto& e : entries) {
            if (pipelines_.count(e)) continue;
            auto t0 = std::chrono::steady_clock::now();
            VkComputePipelineCreateInfo cpi{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            cpi.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            cpi.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            cpi.stage.module = shaderModule_;
            cpi.stage.pName = e.c_str();
            cpi.layout = pipeLayout_;
            VkPipeline p;
            VK_CHECK(vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &cpi, nullptr, &p));
            pipelines_[e] = p;
            if (prof) {
                double dt = std::chrono::duration<double>(
                                std::chrono::steady_clock::now() - t0).count();
                if (dt > 0.1)
                    fprintf(stderr, "[prof]   pipeline %s: %.2f s\n", e.c_str(), dt);
            }
        }
    }

    // ---- recording ----
    VkCommandBuffer begin() {
        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool = cmdPool_;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        VkCommandBuffer cb;
        VK_CHECK(vkAllocateCommandBuffers(device_, &ai, &cb));
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cb, &bi));
        if (descSet_ != VK_NULL_HANDLE)
            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeLayout_, 0, 1,
                                    &descSet_, 0, nullptr);
        if (profiling_) {
            vkCmdResetQueryPool(cb, queryPool_, 0, kMaxQueries);
            queryNames_.clear();
        }
        return cb;
    }

    void dispatch(VkCommandBuffer cb, const std::string& name, uint32_t gx, const Push& push,
                  uint32_t gy = 1) {
        auto it = pipelines_.find(name);
        if (it == pipelines_.end()) throw std::runtime_error("unknown pipeline " + name);
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, it->second);
        vkCmdPushConstants(cb, pipeLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push), &push);
        bool prof = profiling_ && 2 * queryNames_.size() + 2 <= kMaxQueries;
        if (prof)
            vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool_,
                                (uint32_t)(2 * queryNames_.size()));
        vkCmdDispatch(cb, gx, gy, 1);
        if (prof) {
            vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool_,
                                (uint32_t)(2 * queryNames_.size() + 1));
            queryNames_.push_back(name);
        }
    }

    void barrier(VkCommandBuffer cb,
                 VkPipelineStageFlags src = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                 VkPipelineStageFlags dst = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) {
        VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
                           VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cb, src | VK_PIPELINE_STAGE_TRANSFER_BIT,
                             dst | VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr);
    }

    void fillZero(VkCommandBuffer cb, const GpuBuffer& b) {
        vkCmdFillBuffer(cb, b.buf, 0, VK_WHOLE_SIZE, 0);
    }

    // offset and size must be 4-byte multiples (vkCmdFillBuffer's rule)
    void fillZero(VkCommandBuffer cb, const GpuBuffer& b, VkDeviceSize offset, VkDeviceSize size) {
        vkCmdFillBuffer(cb, b.buf, offset, size, 0);
    }

    void copy(VkCommandBuffer cb, const GpuBuffer& src, const GpuBuffer& dst, VkDeviceSize size) {
        VkBufferCopy c{0, 0, size};
        vkCmdCopyBuffer(cb, src.buf, dst.buf, 1, &c);
    }

    void submit(VkCommandBuffer cb) {
        VK_CHECK(vkEndCommandBuffer(cb));
        VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cb;
        VK_CHECK(vkQueueSubmit(queue_, 1, &si, fence_));
        VK_CHECK(vkWaitForFences(device_, 1, &fence_, VK_TRUE, ~0ull));
        VK_CHECK(vkResetFences(device_, 1, &fence_));
        vkFreeCommandBuffers(device_, cmdPool_, 1, &cb);
        if (profiling_ && !queryNames_.empty()) {
            std::vector<uint64_t> ts(2 * queryNames_.size());
            VK_CHECK(vkGetQueryPoolResults(device_, queryPool_, 0, (uint32_t)ts.size(),
                                           ts.size() * 8, ts.data(), 8,
                                           VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
            for (size_t i = 0; i < queryNames_.size(); i++) {
                auto& s = profStats_[queryNames_[i]];
                s.first += 1;
                s.second += (ts[2 * i + 1] - ts[2 * i]) * (double)timestampPeriod_ * 1e-6;
            }
            queryNames_.clear();
        }
    }

    void printProfile(FILE* f = stderr) const {
        if (!profiling_) return;
        std::vector<std::pair<std::string, std::pair<uint64_t, double>>> v(profStats_.begin(),
                                                                           profStats_.end());
        std::sort(v.begin(), v.end(),
                  [](auto& a, auto& b) { return a.second.second > b.second.second; });
        double total = 0;
        for (auto& e : v) total += e.second.second;
        fprintf(f, "[profile] %-18s %10s %8s %10s\n", "kernel", "total_ms", "count", "avg_us");
        for (auto& e : v)
            fprintf(f, "[profile] %-18s %10.2f %8llu %10.1f\n", e.first.c_str(), e.second.second,
                    (unsigned long long)e.second.first, 1e3 * e.second.second / e.second.first);
        fprintf(f, "[profile] %-18s %10.2f\n", "TOTAL", total);
    }

    VkDevice device() const { return device_; }
    double totalAllocatedMB() const { return totalAllocated_ / (1024.0 * 1024.0); }
    double deviceLocalHeapMB() const {
        VkDeviceSize best = 0;
        for (uint32_t i = 0; i < memProps_.memoryHeapCount; i++)
            if (memProps_.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                best = std::max(best, memProps_.memoryHeaps[i].size);
        return best / (1024.0 * 1024.0);
    }

private:
    static constexpr VkDeviceSize kStagingSize = 64ull << 20;

    // `deviceIndex` < 0 prefers the first discrete GPU, else device 0.
    static VkPhysicalDevice choosePhysical(VkInstance inst, int deviceIndex) {
        uint32_t n = 0;
        vkEnumeratePhysicalDevices(inst, &n, nullptr);
        if (n == 0) return VK_NULL_HANDLE;
        std::vector<VkPhysicalDevice> devs(n);
        vkEnumeratePhysicalDevices(inst, &n, devs.data());
        int pick = deviceIndex;
        if (pick < 0) {
            pick = 0;
            for (uint32_t i = 0; i < n; i++) {
                VkPhysicalDeviceProperties p;
                vkGetPhysicalDeviceProperties(devs[i], &p);
                if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) { pick = (int)i; break; }
            }
        }
        if (pick >= (int)n) return VK_NULL_HANDLE;
        return devs[pick];
    }

    static VkDeviceCaps queryCaps(VkPhysicalDevice phys) {
        // The atomic-float features live behind an extension, so ask only if
        // the device advertises it -- chaining an unsupported feature struct
        // into vkGetPhysicalDeviceFeatures2 is undefined behaviour, not a
        // false answer.
        uint32_t en = 0;
        vkEnumerateDeviceExtensionProperties(phys, nullptr, &en, nullptr);
        std::vector<VkExtensionProperties> exts(en);
        vkEnumerateDeviceExtensionProperties(phys, nullptr, &en, exts.data());
        auto has_ext = [&](const char* name) {
            for (const auto& e : exts)
                if (std::strcmp(e.extensionName, name) == 0) return true;
            return false;
        };

        VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        f2.pNext = &f12;
        VkPhysicalDeviceShaderAtomicFloatFeaturesEXT fAtom{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT};
        const bool atomic_float_ext = has_ext(VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME);
        if (atomic_float_ext) { fAtom.pNext = f2.pNext; f2.pNext = &fAtom; }
        VkPhysicalDeviceShaderIntegerDotProductFeatures fDot{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_FEATURES};
        const bool dot_ext = has_ext(VK_KHR_SHADER_INTEGER_DOT_PRODUCT_EXTENSION_NAME);
        if (dot_ext) { fDot.pNext = f2.pNext; f2.pNext = &fDot; }
        vkGetPhysicalDeviceFeatures2(phys, &f2);

        VkPhysicalDeviceShaderIntegerDotProductProperties pDot{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_INTEGER_DOT_PRODUCT_PROPERTIES};
        if (dot_ext) {
            VkPhysicalDeviceProperties2 p2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
            p2.pNext = &pDot;
            vkGetPhysicalDeviceProperties2(phys, &p2);
        }

        VkDeviceCaps c;
        c.float64 = f2.features.shaderFloat64 == VK_TRUE;
        c.float32AtomicAdd = atomic_float_ext &&
                             fAtom.shaderBufferFloat32Atomics == VK_TRUE &&
                             fAtom.shaderBufferFloat32AtomicAdd == VK_TRUE;
        c.float64AtomicAdd = atomic_float_ext &&
                             fAtom.shaderBufferFloat64Atomics == VK_TRUE &&
                             fAtom.shaderBufferFloat64AtomicAdd == VK_TRUE;
        c.int64Atomics = f2.features.shaderInt64 == VK_TRUE &&
                         f12.shaderBufferInt64Atomics == VK_TRUE;
        c.intDotProduct = dot_ext && fDot.shaderIntegerDotProduct == VK_TRUE;
        c.intDotProductFast =
            c.intDotProduct &&
            pDot.integerDotProduct4x8BitPackedUnsignedAccelerated == VK_TRUE;
        return c;
    }

    // `preferFlags` are tried on top of `memFlags` and dropped if no memory
    // type offers both.
    GpuBuffer createBufferRaw(VkDeviceSize size, VkBufferUsageFlags usage,
                              VkMemoryPropertyFlags memFlags,
                              VkMemoryPropertyFlags preferFlags = 0) {
        GpuBuffer b;
        b.size = size;
        VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bci.size = size;
        bci.usage = usage;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK(vkCreateBuffer(device_, &bci, nullptr, &b.buf));
        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(device_, b.buf, &req);
        uint32_t typeIdx = ~0u;
        for (int pass = preferFlags ? 0 : 1; pass < 2 && typeIdx == ~0u; pass++) {
            const VkMemoryPropertyFlags want = pass == 0 ? (memFlags | preferFlags) : memFlags;
            for (uint32_t i = 0; i < memProps_.memoryTypeCount; i++)
                if ((req.memoryTypeBits & (1u << i)) &&
                    (memProps_.memoryTypes[i].propertyFlags & want) == want) {
                    typeIdx = i;
                    break;
                }
        }
        if (typeIdx == ~0u) throw std::runtime_error("no suitable memory type");
        VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mai.allocationSize = req.size;
        mai.memoryTypeIndex = typeIdx;
        VK_CHECK(vkAllocateMemory(device_, &mai, nullptr, &b.mem));
        VK_CHECK(vkBindBufferMemory(device_, b.buf, b.mem, 0));
        allocations_.emplace_back(b.buf, b.mem);
        return b;
    }

    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice phys_ = VK_NULL_HANDLE;
    VkDeviceCaps caps_{};
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue queue_ = VK_NULL_HANDLE;
    uint32_t queueFamily_ = 0;
    VkPhysicalDeviceMemoryProperties memProps_{};
    VkCommandPool cmdPool_ = VK_NULL_HANDLE;
    VkFence fence_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout setLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout pipeLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descPool_ = VK_NULL_HANDLE;
    VkDescriptorSet descSet_ = VK_NULL_HANDLE;
    uint32_t descBindingCount_ = 0;
    VkShaderModule shaderModule_ = VK_NULL_HANDLE;
    std::map<std::string, VkPipeline> pipelines_;
    GpuBuffer staging_, stagingDl_;
    void* stagingPtr_ = nullptr;
    void* stagingDlPtr_ = nullptr;
    VkDeviceSize totalAllocated_ = 0;
    std::vector<std::pair<VkBuffer, VkDeviceMemory>> allocations_;  // freed in ~VkContext

    static constexpr uint32_t kMaxQueries = 16384;
    bool profiling_ = false;
    float timestampPeriod_ = 1.0f;
    VkQueryPool queryPool_ = VK_NULL_HANDLE;
    std::vector<std::string> queryNames_;
    std::map<std::string, std::pair<uint64_t, double>> profStats_;  // name -> (count, total ms)
};
