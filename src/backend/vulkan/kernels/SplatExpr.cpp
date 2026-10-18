// Vulkan implementation of splat_expr_eval_tensor (kernels/visualize/
// Visualizer.cu): shaders/splat_expr.slang, with the program's variables and
// code staged in the kernel-params ring.

#include <kernels/visualize/Visualizer.cuh>

#include "backend/vulkan/kernels/KernelCommon.h"

#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

// Mirrors SplatExprParams in shaders/splat_expr.slang.
struct SplatExprParams {
    uint64_t out, code, vars;
    uint32_t num_splats, wgs_per_row, code_len, _pad0;
};
static_assert(sizeof(SplatExprParams) == 3 * 8 + 4 * 4, "params layout must match the slang struct");
static_assert(sizeof(SplatExprVar) == 48, "SplatExprVar must match ExprVar in shaders/splat_expr.slang");

}  // namespace

void splat_expr_eval_tensor(int64_t num_splats, const std::vector<SplatExprVar>& vars,
                            const std::vector<uint32_t>& code, float* out) {
    if (num_splats <= 0 || code.empty()) return;
    const size_t var_bytes = vars.size() * sizeof(SplatExprVar);
    const size_t code_bytes = code.size() * sizeof(uint32_t);
    uint64_t addr = 0;
    void* mapped = nullptr;
    if (!backend::vk::params_alloc((uint32_t)(var_bytes + code_bytes), &addr, &mapped))
        throw std::runtime_error("splat_expr_eval_tensor: params ring failed");
    std::memcpy(mapped, vars.data(), var_bytes);
    std::memcpy((char*)mapped + var_bytes, code.data(), code_bytes);

    SplatExprParams p{};
    p.out = (uint64_t)out;
    p.vars = addr;
    p.code = addr + var_bytes;
    p.num_splats = (uint32_t)num_splats;
    p.code_len = (uint32_t)code.size();
    vkk::dispatch_flat("splat_expr.splat_expr_eval", {}, num_splats, 256, &p, sizeof(p),
                       &p.wgs_per_row);
}
