#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace backend::vk {

// On GTX 1060 / Windows 571.96, WorkgroupId omits dispatch bases.
// GlobalInvocationId includes them; recover group IDs for split pipelines.
inline bool spirv_dispatch_base(const uint32_t* words, size_t count,
                                std::vector<uint32_t>& out) {
    enum : uint32_t {
        OpExecutionMode = 16, OpTypeVector = 23, OpConstant = 43,
        OpConstantComposite = 44, OpFunction = 54, OpLoad = 61,
        OpDecorate = 71, OpUDiv = 134,
        LocalSize = 17, BuiltIn = 11, WorkgroupId = 26, GlobalInvocationId = 28
    };
    if (count < 5 || words[0] != 0x07230203u) return false;
    uint32_t group = 0, global = 0, vector_type = 0, scalar_type = 0;
    uint32_t local[3] = {};
    size_t decoration = 0;
    for (size_t i = 5; i < count;) {
        uint32_t len = words[i] >> 16, op = words[i] & 0xffffu;
        if (!len || i + len > count) return false;
        if (op == OpExecutionMode && len == 6 && words[i + 2] == LocalSize)
            for (int d = 0; d < 3; ++d) local[d] = words[i + 3 + d];
        if (op == OpDecorate && len == 4 && words[i + 2] == BuiltIn) {
            if (words[i + 3] == WorkgroupId) {
                group = words[i + 1];
                decoration = i;
            }
            if (words[i + 3] == GlobalInvocationId) global = words[i + 1];
        }
        i += len;
    }
    if (!group) { out.assign(words, words + count); return true; }
    for (size_t i = 5; i < count; i += words[i] >> 16)
        if ((words[i] & 0xffffu) == OpLoad && (words[i] >> 16) >= 4 &&
            words[i + 3] == group) vector_type = words[i + 1];
    if (!vector_type) { out.assign(words, words + count); return true; }
    for (size_t i = 5; i < count; i += words[i] >> 16)
        if ((words[i] & 0xffffu) == OpTypeVector && (words[i] >> 16) == 4 &&
            words[i + 1] == vector_type && words[i + 3] == 3)
            scalar_type = words[i + 2];
    if (!scalar_type || !local[0] || !local[1] || !local[2]) return false;

    uint32_t next = words[3];
    uint32_t sizes[3] = {next++, next++, next++};
    uint32_t size_vector = next++;
    bool constants = false;
    out.assign(words, words + 5);
    for (size_t i = 5; i < count;) {
        uint32_t len = words[i] >> 16, op = words[i] & 0xffffu;
        if (op == OpFunction && !constants) {
            for (int d = 0; d < 3; ++d)
                out.insert(out.end(), {(4u << 16) | OpConstant,
                                       scalar_type, sizes[d], local[d]});
            out.insert(out.end(), {(6u << 16) | OpConstantComposite, vector_type, size_vector,
                                   sizes[0], sizes[1], sizes[2]});
            constants = true;
        }
        size_t start = out.size();
        out.insert(out.end(), words + i, words + i + len);
        if (i == decoration && !global) out[start + 3] = GlobalInvocationId;
        if (op == OpLoad && len >= 4 && words[i + 3] == group) {
            uint32_t loaded = next++;
            out[start + 2] = loaded;
            out[start + 3] = global ? global : group;
            out.insert(out.end(), {(5u << 16) | OpUDiv, vector_type,
                                   words[i + 2], loaded, size_vector});
        }
        i += len;
    }
    out[3] = next;
    return constants;
}

}  // namespace backend::vk
