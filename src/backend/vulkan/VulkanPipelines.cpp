#include "backend/vulkan/VulkanPipelines.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include "core/Env.h"

namespace backend {
namespace vk {

// Defined by the generated embed TU (see shaders/spirv_tool.cpp).
struct SpirvBlob {
    const char* name;
    const uint32_t* data;
    size_t size_bytes;
};
extern const SpirvBlob g_spirv_blobs[];
extern const size_t g_spirv_blob_count;

namespace {

std::mutex g_pipeline_mutex;
struct Module {
    VkShaderModule handle = VK_NULL_HANDLE;
    uint32_t local_x = 0;  // workgroup X, 0 when not a literal
};
std::map<std::string, Module> g_modules;                 // by blob name
std::map<uint32_t, VkPipelineLayout> g_layouts;          // by push size
std::map<std::string, VkPipeline> g_pipelines;           // by full key

const SpirvBlob* find_blob(const char* name) {
    for (size_t i = 0; i < g_spirv_blob_count; i++)
        if (std::strcmp(g_spirv_blobs[i].name, name) == 0)
            return &g_spirv_blobs[i];
    return nullptr;
}

// Entries whose source uses an adaptive feature embed variant blobs under
// dotted suffixes (see shaders/spirv_tool.cpp): ".atomicadd" (native
// OpAtomicFAddEXT instead of the CAS loop), ".int8" (native byte access
// instead of u32-word packing), ".noint64" (no Int64 capability, for
// devices WITHOUT shaderInt64). spirv_tool.cpp compiles every subset of an
// entry's applicable features, so the largest device-desired subset that
// exists is exactly desired-intersect-applicable — probing subsets from
// largest to smallest and taking the first hit is correct. The choice is
// per-device-fixed, so caching modules under the base name stays correct.
constexpr const char* kFeatureSuffixes[] = {".atomicadd", ".int8",
                                            ".noint64"};
constexpr uint32_t kNumFeatures = 3;

// [vk::constant_id] of kXchgFloatAdd in shaders/atomic_float.slang.
constexpr uint32_t kXchgFloatAddSpecId = 1000;

// Entries that run at 64 lanes where the device offers 32 and 64 (RDNA); the
// rest run at 32. Each is GPU time at 64 over 32, paired within 1500-step
// runs of four presets on a Ryzen 7000 iGPU (RADV RDNA2, 2026-10-08).
constexpr const char* kWave64Entries[] = {
    "appearance_chain.appearance_fwd",          // 0.57-0.66
    "appearance_chain.appearance_bwd",          // 0.81-0.95
    "multi_scale_loss.ppl_bwd",                 // 0.90
    "rasterize_fwd.rasterize_fwd_2d",           // 0.86-0.99
    "fused_ssim.ssim_mask_cov",                 // 0.94
    "background_sh.background_sh_bwd_reduce",   // 0.53
    "intersect_tile.intersect_tile_count",      // 0.80
    "intersect_tile.intersect_offset",          // 0.65-0.76
    "sort_scan.sort_hist_u32",                  // 0.88-0.91
    "sort_scan.sort_hist_u64",                  // 0.86-0.90
    "sort_scan.sort_spine_local",               // 0.67
    "sort_scan.scan_add_i32",                   // 0.81
    "optimizer.increment_i32",                  // 0.60
    "projection_qgrad.qgrad_camera_id_bounds",  // 0.62
    "projection_qgrad.qgrad_iota",              // 0.77
    "densify.normalize_clip_map",               // 0.71
    "densify.densify_update_weight",            // 0.69
    "densify.densify_accum_finalize",           // 0.66
    "densify.densify_efraimidis_keys",          // 0.73-0.75
};

// The width `entry` requires, or 0 to leave it to the device: its default
// capped at 32, or 64 for the entries above.
uint32_t subgroup_width(const std::string& entry) {
    const Capabilities& caps = Context::get().caps();
    if (!caps.subgroup_max) return 0;
    uint32_t want = caps.subgroup_force;
    if (!want) {
        want = std::min(caps.subgroup_size, 32u);
        if (caps.subgroup_max >= 64)
            for (const char* e : kWave64Entries)
                if (entry == e) want = 64;
    }
    return std::min(std::max(want, caps.subgroup_min), caps.subgroup_max);
}

const SpirvBlob* find_variant_blob(const std::string& name) {
    const Capabilities& caps = Context::get().caps();
    const bool desired[kNumFeatures] = {
        caps.float32_atomic_add,  // .atomicadd
        caps.shader_int8,         // .int8
        !caps.shader_int64,       // .noint64
    };
    uint32_t desired_mask = 0;
    for (uint32_t i = 0; i < kNumFeatures; i++)
        if (desired[i]) desired_mask |= 1u << i;

    auto bit_count = [](uint32_t v) {
        int n = 0;
        for (; v; v &= v - 1) n++;
        return n;
    };
    for (int size = (int)kNumFeatures; size >= 0; size--) {
        for (uint32_t sub = 0; sub < (1u << kNumFeatures); sub++) {
            if ((sub & desired_mask) != sub) continue;
            if (bit_count(sub) != size) continue;
            std::string full = name;
            for (uint32_t i = 0; i < kNumFeatures; i++)
                if (sub & (1u << i)) full += kFeatureSuffixes[i];
            if (const SpirvBlob* blob = find_blob(full.c_str())) return blob;
        }
    }
    return nullptr;
}

// The X of OpExecutionMode LocalSize; 0 for LocalSizeId or none.
uint32_t spirv_local_size_x(const uint32_t* words, size_t n) {
    constexpr uint32_t kOpExecutionMode = 16, kLocalSize = 17;
    for (size_t i = 5; i < n;) {
        const uint32_t len = words[i] >> 16, op = words[i] & 0xffffu;
        if (len == 0 || i + len > n) break;
        if (op == kOpExecutionMode && len >= 6 && words[i + 2] == kLocalSize)
            return words[i + 3];
        i += len;
    }
    return 0;
}

const Module* get_module(const std::string& name) {
    auto it = g_modules.find(name);
    if (it != g_modules.end()) return &it->second;
    const SpirvBlob* blob = find_variant_blob(name);
    if (!blob) {
        set_error("no embedded SPIR-V blob with this entry name (rerun "
                  "shaders/spirv_tool.cpp?)", VK_SUCCESS);
        return nullptr;
    }
    // Driver shader compilers can crash outright on a blob they dislike;
    // under SS_VK_VERBOSE the last line printed names the culprit.
    if (spirula::env("VK_VERBOSE")) {
        std::fprintf(stderr, "[spirula-vk] compiling %s\n", blob->name);
        std::fflush(stderr);
    }
    VkShaderModuleCreateInfo sci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    sci.codeSize = blob->size_bytes;
    sci.pCode = blob->data;
    Module m;
    VkResult r = vkCreateShaderModule(Context::get().device(), &sci, nullptr,
                                      &m.handle);
    if (r != VK_SUCCESS) {
        set_error("vkCreateShaderModule failed", r);
        return nullptr;
    }
    m.local_x = spirv_local_size_x(blob->data, blob->size_bytes / 4);
    return &(g_modules[name] = m);
}

VkPipelineLayout get_layout(uint32_t push_size) {
    auto it = g_layouts.find(push_size);
    if (it != g_layouts.end()) return it->second;
    VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT, 0, push_size};
    VkPipelineLayoutCreateInfo lci{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    if (push_size > 0) {
        lci.pushConstantRangeCount = 1;
        lci.pPushConstantRanges = &range;
    }
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkResult r = vkCreatePipelineLayout(Context::get().device(), &lci,
                                        nullptr, &layout);
    if (r != VK_SUCCESS) {
        set_error("vkCreatePipelineLayout failed", r);
        return VK_NULL_HANDLE;
    }
    g_layouts[push_size] = layout;
    return layout;
}

// `dispatch_base` builds the variant vkCmdDispatchBase may run; plain launches
// keep the pipeline without it, so a launch that never splits is unchanged.
VkPipeline get_pipeline(const std::string& blob_name, const SpecList& spec,
                        uint32_t push_size, bool dispatch_base,
                        VkPipelineLayout* out_layout) {
    std::string key = blob_name;
    key += '|';
    key += std::to_string(push_size);
    for (uint32_t i = 0; i < spec.count; i++) {
        key += '|';
        key += std::to_string(spec.values[i]);
    }
    if (dispatch_base) key += "|base";

    std::lock_guard<std::mutex> lock(g_pipeline_mutex);
    VkPipelineLayout layout = get_layout(push_size);
    if (layout == VK_NULL_HANDLE) return VK_NULL_HANDLE;
    *out_layout = layout;

    auto it = g_pipelines.find(key);
    if (it != g_pipelines.end()) return it->second;

    const Module* module = get_module(blob_name);
    if (!module) return VK_NULL_HANDLE;

    VkSpecializationMapEntry entries[SpecList::kMax + 1];
    uint32_t values[SpecList::kMax + 1];
    uint32_t n = spec.count;
    for (uint32_t i = 0; i < n; i++) {
        entries[i] = {i, i * 4u, 4u};
        values[i] = spec.values[i];
    }
    // Every module, whether it declares the constant or not: an entry for an
    // ID the shader lacks is ignored (VkSpecializationInfo).
    if (Context::get().caps().xchg_float_add) {
        entries[n] = {kXchgFloatAddSpecId, n * 4u, 4u};
        values[n++] = VK_TRUE;
    }
    VkSpecializationInfo spec_info{};
    spec_info.mapEntryCount = n;
    spec_info.pMapEntries = entries;
    spec_info.dataSize = n * 4u;
    spec_info.pData = values;

    VkComputePipelineCreateInfo pci{
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pci.stage.module = module->handle;
    // slangc emits each single-entry blob with the entry renamed to "main".
    pci.stage.pName = "main";
    VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT sgsz{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT};
    if (uint32_t width = subgroup_width(blob_name)) {
        sgsz.requiredSubgroupSize = width;
        pci.stage.pNext = &sgsz;
        // A 32-wide workgroup on a wave64-only device is one half-full wave.
        if (module->local_x && module->local_x % width == 0)
            pci.stage.flags |=
                VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT_EXT;
    }
    if (n) pci.stage.pSpecializationInfo = &spec_info;
    pci.layout = layout;
    if (dispatch_base) pci.flags |= VK_PIPELINE_CREATE_DISPATCH_BASE_BIT;

    if (spirula::env("VK_VERBOSE")) {
        std::fprintf(stderr, "[spirula-vk] pipeline %s\n", key.c_str());
        std::fflush(stderr);
    }
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult r = vkCreateComputePipelines(Context::get().device(),
                                          VK_NULL_HANDLE, 1, &pci, nullptr,
                                          &pipeline);
    if (r != VK_SUCCESS) {
        set_error("vkCreateComputePipelines failed", r);
        return VK_NULL_HANDLE;
    }
    g_pipelines[key] = pipeline;
    return pipeline;
}

}  // namespace

namespace {

// Shared by dispatch() and dispatch_range(): `base` null is a plain launch.
bool record_dispatch(Stream stream, const char* entry_name,
                     const SpecList& spec, const uint32_t* base,
                     uint32_t groups_x, uint32_t groups_y, uint32_t groups_z,
                     const void* params, uint32_t params_size) {
    Context& ctx = Context::get();
    if (!ctx.ok()) return false;
    if (groups_x == 0 || groups_y == 0 || groups_z == 0) return true;
    if (params_size > ctx.caps().max_push_constants) {
        set_error("dispatch: params exceed maxPushConstantsSize (use a "
                  "params-ring indirection for this kernel)", VK_SUCCESS);
        return false;
    }
    // Vulkan guarantees only 65535 workgroups per dimension; a kernel that
    // can exceed it must fold its grid (vkk::fold_1d).
    if (groups_x > 65535 || groups_y > 65535 || groups_z > 65535) {
        set_error("dispatch: workgroup count exceeds 65535 (kernel needs a "
                  "folded grid)", VK_SUCCESS);
        return false;
    }

    uint32_t push_size = (params_size + 3u) / 4u * 4u;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline =
        get_pipeline(entry_name, spec, push_size, base != nullptr, &layout);
    if (pipeline == VK_NULL_HANDLE) return false;

    StreamImpl* s = stream_impl(stream);
    VkCommandBuffer cb = stream_begin(s);
    if (cb == VK_NULL_HANDLE) return false;
    if (submit_log_ms() > 0) s->recorded.push_back(entry_name);

    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    if (params_size > 0) {
        if (push_size == params_size) {
            vkCmdPushConstants(cb, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                               params_size, params);
        } else {  // pad the trailing bytes to the 4-byte range size
            unsigned char padded[256] = {};
            std::memcpy(padded, params, params_size);
            vkCmdPushConstants(cb, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                               push_size, padded);
        }
    }
    int ts = gpu_ts_begin(cb, entry_name);  // GPU-timestamp timing (profile)
    if (base)
        vkCmdDispatchBase(cb, base[0], base[1], base[2], groups_x, groups_y,
                          groups_z);
    else
        vkCmdDispatch(cb, groups_x, groups_y, groups_z);
    gpu_ts_end(cb, ts);
    stream_barrier(cb);
    return true;
}

}  // namespace

bool dispatch(Stream stream, const char* entry_name, const SpecList& spec,
              uint32_t groups_x, uint32_t groups_y, uint32_t groups_z,
              const void* params, uint32_t params_size) {
    return record_dispatch(stream, entry_name, spec, nullptr, groups_x,
                           groups_y, groups_z, params, params_size);
}

bool dispatch_range(Stream stream, const char* entry_name,
                    const SpecList& spec, const uint32_t base[3],
                    const uint32_t count[3], const void* params,
                    uint32_t params_size) {
    return record_dispatch(stream, entry_name, spec, base, count[0], count[1],
                           count[2], params, params_size);
}

void pipelines_shutdown() {
    VkDevice dev = Context::get().device();
    std::lock_guard<std::mutex> lock(g_pipeline_mutex);
    for (auto& kv : g_pipelines) vkDestroyPipeline(dev, kv.second, nullptr);
    for (auto& kv : g_layouts)
        vkDestroyPipelineLayout(dev, kv.second, nullptr);
    for (auto& kv : g_modules)
        vkDestroyShaderModule(dev, kv.second.handle, nullptr);
    g_pipelines.clear();
    g_layouts.clear();
    g_modules.clear();
}

}  // namespace vk
}  // namespace backend
