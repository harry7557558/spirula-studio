// Stochastic viewer renders: docs/notes/stochastic-splats.md. forward_3dgs
// draws the frame; this keeps each channel's running average.

#include "engine/Engine.h"

#include "engine/EngineCommon.h"
#include "engine/EngineInternal.h"
#include "engine/EngineState.h"

#include "core/Env.h"

#include <algorithm>
#include <cstdlib>
#include <string>

namespace {
uint32_t env_count(const char* name, uint32_t fallback, uint32_t lo, uint32_t hi) {
    const char* v = spirula::env(name);
    if (!v || !*v) return fallback;
    return (uint32_t)std::clamp<long>(std::strtol(v, nullptr, 10), (long)lo, (long)hi);
}

uint64_t mix(uint64_t h, uint64_t v) {
    h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    return h;
}
}  // namespace

bool engine_stochastic_enabled() {
    static const bool on = [] {
        const char* v = spirula::env("VIEWER_STOCHASTIC");
        return v && v[0] && v[0] != '0';
    }();
    return on;
}

void engine_arm_stochastic(int channel, uint64_t key) {
    auto& st = engine().stochastic;
    st.armed = true;
    st.arm_channel = channel;
    st.arm_key = key;
}

uint32_t engine_stochastic_frames() { return engine().stochastic.last_frames; }

uint32_t engine_stochastic_frame_target() {
    static const uint32_t n = env_count("VIEWER_STOCHASTIC_FRAMES", 64, 1, 1u << 20);
    return n;
}

bool engine_stochastic_stratified() {
    return env_count("VIEWER_STOCHASTIC_STRATIFIED", 1, 0, 1) != 0;
}

uint32_t engine_stochastic_spp() {
    return env_count("VIEWER_STOCHASTIC_SPP", 4, 1, kStochasticMaxSpp);
}

bool _engine_stochastic_depth_sorted() {
    return env_count("VIEWER_STOCHASTIC_SORTED", 1, 0, 1) != 0;
}

StochasticFrame _engine_stochastic_frame(const std::string& primitive, int sh_degree,
                                         bool output_normal, bool depth_sorted) {
    auto& st = engine().stochastic;
    const auto& cam = engine().camera;
    uint64_t key = mix(st.arm_key, st.revision);
    key = mix(key, std::hash<std::string>{}(primitive));
    key = mix(key, ((uint64_t)(uint32_t)sh_degree << 1) | (output_normal ? 1u : 0u));
    key = mix(key, ((uint64_t)cam.width << 32) | (uint32_t)cam.height);
    key = mix(key, (uint64_t)engine().cur_num_splats);
    key = mix(key, (uint64_t)(uintptr_t)engine().world.features_dc.data_ptr());

    auto& sum = st.sums[st.arm_channel];
    StochasticFrame f;
    f.frame = sum.key == key ? sum.frames : 0u;
    f.spp = engine_stochastic_spp();
    f.winner = env_count("VIEWER_STOCHASTIC_WINNER", 0, 0, 1) != 0;
    f.depth_sorted = depth_sorted;
    f.stratified = engine_stochastic_stratified();
    DeviceVector<float> buf;
    buf.resize_dynamic(VramCategory::Viewer, "stochastic.sum." + std::to_string(st.arm_channel),
                       (int64_t)cam.num * cam.height * cam.width * kStochasticSum);
    f.sum = buf.data_ptr();
    sum.key = key;
    sum.frames = f.frame + 1;
    st.last_frames = sum.frames;
    return f;
}
