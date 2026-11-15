// The Metal path where the build has none (OpMetal.mm is macOS-only).
#if !SS_HAVE_NN_METAL
#include "nn/OpMetal.h"
#include "nn/vk/MetalInterop.h"

namespace nn {

bool metal_enabled() { return false; }
void set_metal_enabled(bool) {}
std::string metal_status() { return "off: not built with SS_NN_METAL"; }
MetalCounts metal_counts() { return {}; }

namespace {
bool g_fp16 = false;
}
Fp16Scope::Fp16Scope(bool on) : prev_(g_fp16) { g_fp16 = g_fp16 || on; }
Fp16Scope::~Fp16Scope() { g_fp16 = prev_; }
bool fp16_active() { return g_fp16; }

bool metal_attention(const Tensor&, const Tensor&, const Tensor&, const Tensor&, int64_t,
                     int64_t, const AttnOpts&) {
    return false;
}
bool metal_gemm(const Tensor&, const Tensor&, const Tensor&, const Tensor&, const Tensor&, Act,
                float, int64_t, float) {
    return false;
}

namespace vk {
namespace metal {

bool available() { return false; }
const std::string& reason() {
    static const std::string r = "not built with SS_NN_METAL";
    return r;
}
void close() {}
Stats stats() { return {}; }
void check_range(DevicePtr, uint64_t, const char*) {}

}  // namespace metal
}  // namespace vk

}  // namespace nn
#endif
