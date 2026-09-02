// Several splat sets resident at once, one bound to the world buffers.
// See the "Viewer scenes" section of Engine.h.

#include "engine/Engine.h"
#include "engine/EngineCommon.h"
#include "engine/EngineInternal.h"
#include "engine/EngineState.h"

#include <array>
#include <stdexcept>
#include <string>

namespace {

struct Scene {
    bool loaded = false;
    WorldSplats world;
    int64_t cur_num_splats = 0;
    int64_t max_num_splats = 0;
    int num_sh = 0;
    bool cs_enabled = false;
    int  cs_transfer = 0;
    bool cs_is_linear = false;
    std::vector<float> cs_matrix;
    uint64_t cs_gen = 1;
};

std::array<Scene, kMaxEngineScenes>& scenes() {
    static std::array<Scene, kMaxEngineScenes> s;
    return s;
}

// Which slot's colour space the engine currently carries, and which revision
// of it. The engine has one colour-space slot and re-uploading its matrix per
// render would be a synchronous H2D per panel.
int g_cs_slot = -1;
uint64_t g_cs_gen = 0;

Scene& at(int slot) {
    if (slot < 0 || slot >= kMaxEngineScenes)
        throw std::runtime_error("engine scene: slot out of range");
    return scenes()[slot];
}

std::string buffer_name(int slot, const char* field) {
    return "scene" + std::to_string(slot) + "." + field;
}

// Slot 1 must not be freed by slot 10's prefix, hence the trailing dot.
std::string slot_prefix(int slot) {
    return "scene" + std::to_string(slot) + ".";
}

template<typename T>
DeviceVector<T> upload_dv(const std::string& name, const TorchTensorView& src) {
    DeviceVector<T> dv;
    if (std::get<0>(src) == 0) return dv;
    const int64_t n = std::get<2>(src)[0];
    dv.resize_dynamic(VramCategory::Splat, name, n);
    backend::memcpy_sync(dv.data_ptr(), (void*)std::get<0>(src),
                         (size_t)n * sizeof(T), backend::MemcpyKind::HostToDevice);
    return dv;
}

template<typename T>
DeviceTensor2D<T> upload_dt2d(const std::string& name, const TorchTensorView& src) {
    DeviceTensor2D<T> dt;
    if (std::get<0>(src) == 0) return dt;
    const auto& shape = std::get<2>(src);
    const int64_t n0 = shape[0], n1 = shape[1];
    dt.resize_dynamic(VramCategory::Splat, name, n0, n1);
    backend::memcpy_sync(dt.data_ptr(), (void*)std::get<0>(src),
                         (size_t)(n0 * n1) * sizeof(T),
                         backend::MemcpyKind::HostToDevice);
    return dt;
}

}  // namespace


void engine_scene_set_data_3dgs(
    int slot,
    int64_t num_splats,
    TorchTensorView means,
    TorchTensorView quats,
    TorchTensorView scales,
    TorchTensorView opacities,
    TorchTensorView features_dc,
    TorchTensorView features_sh
) {
    Scene& s = at(slot);
    const int64_t max_num_splats = std::get<2>(means)[0];
    if (std::get<2>(quats)[0] != max_num_splats ||
        std::get<2>(scales)[0] != max_num_splats ||
        std::get<2>(opacities)[0] != max_num_splats ||
        std::get<2>(features_dc)[0] != max_num_splats ||
        std::get<2>(features_sh)[0] != max_num_splats)
        throw std::runtime_error("engine_scene_set_data_3dgs: max_num_splats mismatch across splat tensors");
    if (max_num_splats < num_splats)
        throw std::runtime_error("engine_scene_set_data_3dgs: tensor size smaller than num_splats");

    engine_scene_free(slot);

    s.world.means       = upload_dv<float3>(buffer_name(slot, "means"), means);
    s.world.quats       = upload_dv<float4>(buffer_name(slot, "quats"), quats);
    s.world.scales      = upload_dv<float3>(buffer_name(slot, "scales"), scales);
    s.world.opacities   = upload_dv<float>(buffer_name(slot, "opacities"), opacities);
    s.world.features_dc = upload_dv<float3>(buffer_name(slot, "features_dc"), features_dc);
    s.world.features_sh = upload_dt2d<float3>(buffer_name(slot, "features_sh"), features_sh);
    s.world.initialized = true;
    s.world.sh_value_bits = 32;

    const auto& sh_shape = std::get<2>(features_sh);
    s.num_sh = (sh_shape.size() >= 2) ? (int)sh_shape[1] : 0;
    s.cur_num_splats = num_splats;
    s.max_num_splats = max_num_splats;
    s.loaded = true;
}


void engine_scene_set_color_space(int slot, bool enabled, int transfer,
                                  bool is_linear,
                                  std::vector<float> color_matrix) {
    Scene& s = at(slot);
    s.cs_enabled = enabled;
    s.cs_transfer = transfer;
    s.cs_is_linear = is_linear;
    s.cs_matrix = std::move(color_matrix);
    s.cs_gen++;
}


void engine_scene_activate(int slot) {
    Scene& s = at(slot);
    if (!s.loaded)
        throw std::runtime_error("engine_scene_activate: slot holds no model");
    engine().world = s.world;
    engine().cur_num_splats = s.cur_num_splats;
    engine().max_num_splats = s.max_num_splats;
    engine().num_sh = s.num_sh;
    if (g_cs_slot != slot || g_cs_gen != s.cs_gen) {
        engine_init_color_space(s.cs_enabled, s.cs_transfer, s.cs_is_linear,
                                s.cs_matrix, false, 0, false,
                                std::vector<float>{});
        g_cs_slot = slot;
        g_cs_gen = s.cs_gen;
    }
}


bool engine_scene_loaded(int slot) {
    return slot >= 0 && slot < kMaxEngineScenes && scenes()[slot].loaded;
}


// Idempotent, and it does NOT test `loaded`: an upload that threw half way
// leaves buffers behind under this slot's name and nothing else frees them.
void engine_scene_free(int slot) {
    Scene& s = at(slot);
    // Whatever is bound right now may be this slot's buffers, which are about
    // to be freed.
    if (s.world.means.data_ptr() &&
        engine().world.means.data_ptr() == s.world.means.data_ptr())
        engine().world = WorldSplats{};
    DevicePool::global().free_dynamic_prefix(slot_prefix(slot));
    s = Scene{};
    if (g_cs_slot == slot) g_cs_slot = -1;
}


void engine_scenes_forget() {
    for (auto& s : scenes()) s = Scene{};
    g_cs_slot = -1;
}
