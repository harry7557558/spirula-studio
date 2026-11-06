#pragma once
// Compute-pipeline cache + dispatch helper over the embedded SPIR-V registry
// (generated from the build-tree spirv/ directory; see README "Shader
// build"). Internal to backend/vulkan.
//
// Kernel ABI (see README.md): `params` is the byte image of the entry
// point's `uniform` struct — device addresses + scalars — delivered as push
// constants. Value-parameter variants (camera model, SH degree, ...) are
// 4-byte specialization constants with IDs 0..n-1.

#include "backend/vulkan/VulkanInternal.h"

#include <cassert>
#include <initializer_list>

namespace backend {
namespace vk {

struct SpecList {
    // Sized for the widest kernel (fpbo: 11 spec axes). The assert exists
    // because overflowing this array once corrupted the adjacent `count`
    // field and crashed the NVIDIA driver at pipeline creation -- raise kMax
    // when a kernel needs more axes.
    static constexpr uint32_t kMax = 16;
    uint32_t values[kMax] = {};
    uint32_t count = 0;
    SpecList() = default;
    SpecList(std::initializer_list<uint32_t> v) {
        assert(v.size() <= kMax && "SpecList: too many spec constants");
        for (uint32_t x : v) {
            if (count >= kMax) break;
            values[count++] = x;
        }
    }
};

// Includes the pinned subgroup limit for a one-dimensional workgroup.
bool supports_workgroup_size(const char* entry_name, uint32_t threads);

// Records into `stream`'s open batch: bind the (cached) pipeline for
// `entry_name` ("<file-stem>.<entry>") specialized with `spec`, push
// `params`, dispatch the workgroup grid, then the conservative global
// barrier (README: CUDA stream ordering). Returns false on failure with the
// sticky error set. params_size must be within the device push-constant
// limit; larger param structs go behind a pushed device address instead.
bool dispatch(Stream stream, const char* entry_name, const SpecList& spec,
              uint32_t groups_x, uint32_t groups_y, uint32_t groups_z,
              const void* params, uint32_t params_size);

// Workgroups [base, base + count) of a grid, through vkCmdDispatchBase: the
// kernel sees SV_GroupID with the base added, so it needs no change.
bool dispatch_range(Stream stream, const char* entry_name,
                    const SpecList& spec, const uint32_t base[3],
                    const uint32_t count[3], const void* params,
                    uint32_t params_size);

// dispatch() split along its longest axis into submissions of about
// core/SubmitBudget.h's target; its workgroups must be independent. `work` is
// its size in units compared only under one `key` (default: the entry).
bool dispatch_budgeted(Stream stream, const char* entry_name,
                       const SpecList& spec, uint32_t groups_x,
                       uint32_t groups_y, uint32_t groups_z,
                       const void* params, uint32_t params_size, double work,
                       const char* key = nullptr);

// Destroys cached pipelines/layouts/modules. Called by runtime_shutdown().
void pipelines_shutdown();

}  // namespace vk
}  // namespace backend
