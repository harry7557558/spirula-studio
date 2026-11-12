// Several splat sets resident at once, one bound to the world buffers.
// See the "Viewer scenes" section of Engine.h.

#include "engine/Engine.h"
#include "backend/api/BackendRuntime.h"
#include "engine/EngineCommon.h"
#include "engine/EngineInternal.h"
#include "engine/EngineState.h"

#include <algorithm>
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
    int sb_lobes = 0;
    bool cs_enabled = false;
    int  cs_transfer = 0;
    bool cs_is_linear = false;
    std::vector<float> cs_matrix;
    uint64_t cs_gen = 1;
    bool has_bg_sh = false;
    int bg_sh_degree = 0;
    int bg_transfer = 0;
    bool bg_is_linear = false;
    DeviceVector<float3> bg_sh_coeffs;
    int64_t rows_missing = 0;  // engine_scene_upload_3dgs rows still owed
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

// Rows [first, first + rows of src) of a slot buffer; `width` elements a row.
template<typename T>
void upload_rows(T* dst, int64_t first, const TorchTensorView& src, int64_t width = 1) {
    if (!dst || std::get<0>(src) == 0) return;
    const int64_t n = std::get<2>(src)[0];
    backend::memcpy_sync(dst + first * width, (void*)std::get<0>(src),
                         (size_t)(n * width) * sizeof(T), backend::MemcpyKind::HostToDevice);
}

// A slot keeps its SH in the AoS packed layout, 256 cells per bound.
template<int BITS>
void alloc_sh_quant(QuantizedTensor<BITS, 256>& q, int slot, int64_t n, int num_sh) {
    q.n_cells = n * 3 * num_sh;
    q.n_bounds = (q.n_cells + 255) / 256;
    q.packed.resize_dynamic(VramCategory::Splat, buffer_name(slot, "features_sh.q"),
                            q.n_cells * QuantizedTensor<BITS, 256>::kBytesPerCell);
    q.bounds.resize_dynamic(VramCategory::Splat, buffer_name(slot, "features_sh.qb"),
                            q.n_bounds);
}

// What one render adds per splat in view: projection outputs, tile keys and
// their sort. 99 B with all of 10M and 55M small SH2 splats in a 1080p view,
// 105 B for a 30M and a 50M trained SH3 model at 1600x800, 16 px bins.
constexpr uint64_t kRenderBytesPerSplat = 110;

// 16 bits is what training keeps SH at (quantization_level 1), so a slot
// loses nothing; 8 where 16 would not leave room to render plus an eighth
// spare (closer views add tile pairs, bigger viewports images).
int auto_sh_value_bits(int64_t n, int num_sh) {
    const backend::MemoryUsage m = backend::memory_usage();
    if (!m.has_total || !m.has_used) return 16;
    uint64_t avail = m.total_bytes > m.used_bytes ? m.total_bytes - m.used_bytes : 0;
    if (m.has_commit) avail = std::min(avail, m.commit_free_bytes);
    const uint64_t cells = (uint64_t)n * 3 * (uint64_t)num_sh;
    const uint64_t need = (uint64_t)n * kRenderBytesPerSplat + cells * 2 +
                          (cells / 256 + 1) * sizeof(float2);
    return need <= avail - avail / 8 ? 16 : 8;
}

}  // namespace


void engine_scene_begin_3dgs(int slot, int64_t num_splats, int num_sh, int sb_lobes,
                             int sh_value_bits) {
    Scene& s = at(slot);
    if (num_splats < 0 || num_sh < 0)
        throw std::runtime_error("engine_scene_begin_3dgs: negative size");
    if (sb_lobes < 0 || 2 * sb_lobes > num_sh)
        throw std::runtime_error("engine_scene_begin_3dgs: a features_sh row of " +
                                 std::to_string(num_sh) + " coefficients cannot hold " +
                                 std::to_string(sb_lobes) + " lobes");
    if (sh_value_bits != 0 && sh_value_bits != 8 && sh_value_bits != 16 &&
        sh_value_bits != 32)
        throw std::runtime_error("engine_scene_begin_3dgs: sh_value_bits must be "
                                 "0, 8, 16 or 32; got " + std::to_string(sh_value_bits));

    engine_scene_free(slot);
    auto alloc = [&](auto& dv, const char* field) {
        dv.resize_dynamic(VramCategory::Splat, buffer_name(slot, field), num_splats);
    };
    alloc(s.world.means, "means");
    alloc(s.world.quats, "quats");
    alloc(s.world.scales, "scales");
    alloc(s.world.opacities, "opacities");
    alloc(s.world.features_dc, "features_dc");

    // After those: `used` counts them, and Vulkan has no `used` until a device exists.
    if (num_sh == 0) sh_value_bits = 32;
    else if (sh_value_bits == 0) sh_value_bits = auto_sh_value_bits(num_splats, num_sh);
    if (sh_value_bits == 32) {
        s.world.features_sh.resize_dynamic(VramCategory::Splat,
                                           buffer_name(slot, "features_sh"), num_splats, num_sh);
    } else {
        s.world.features_sh.set_shape_no_alloc(num_splats, num_sh);
        if (sh_value_bits == 8) alloc_sh_quant(s.world.features_sh_quant8, slot, num_splats, num_sh);
        else alloc_sh_quant(s.world.features_sh_quant16, slot, num_splats, num_sh);
    }
    s.world.sh_value_bits = sh_value_bits;
    s.world.initialized = true;
    s.num_sh = num_sh;
    s.sb_lobes = sb_lobes;
    s.cur_num_splats = num_splats;
    s.max_num_splats = num_splats;
    s.rows_missing = num_splats;
    s.loaded = num_splats == 0;
}


void engine_scene_upload_3dgs(int slot, int64_t first, TorchTensorView means,
                              TorchTensorView quats, TorchTensorView scales,
                              TorchTensorView opacities, TorchTensorView features_dc,
                              TorchTensorView features_sh) {
    Scene& s = at(slot);
    const int64_t n = std::get<2>(means)[0];
    const auto& sh_shape = std::get<2>(features_sh);
    if (std::get<2>(quats)[0] != n || std::get<2>(scales)[0] != n ||
        std::get<2>(opacities)[0] != n || std::get<2>(features_dc)[0] != n ||
        (s.num_sh > 0 && (sh_shape.size() < 2 || sh_shape[0] != n || sh_shape[1] != s.num_sh)))
        throw std::runtime_error("engine_scene_upload_3dgs: row counts or SH width "
                                 "disagree with each other or the slot");
    if (first < 0 || first + n > s.max_num_splats)
        throw std::runtime_error("engine_scene_upload_3dgs: rows past the slot's end");
    upload_rows(s.world.means.data_ptr(), first, means);
    upload_rows(s.world.quats.data_ptr(), first, quats);
    upload_rows(s.world.scales.data_ptr(), first, scales);
    upload_rows(s.world.opacities.data_ptr(), first, opacities);
    upload_rows(s.world.features_dc.data_ptr(), first, features_dc);
    const float* sh_rows = (const float*)std::get<0>(features_sh);
    if (s.num_sh > 0 && sh_rows) {
        if (s.world.sh_value_bits == 32)
            upload_rows(s.world.features_sh.data_ptr(), first, features_sh, s.num_sh);
        else if (s.world.sh_value_bits == 8)
            encode_sh_rows(s.world.features_sh_quant8, sh_rows, s.max_num_splats, s.num_sh,
                           false, first, n);
        else
            encode_sh_rows(s.world.features_sh_quant16, sh_rows, s.max_num_splats, s.num_sh,
                           false, first, n);
    }
    s.rows_missing -= n;
    if (s.rows_missing <= 0) s.loaded = true;
}


void engine_scene_set_data_3dgs(
    int slot,
    int64_t num_splats,
    TorchTensorView means,
    TorchTensorView quats,
    TorchTensorView scales,
    TorchTensorView opacities,
    TorchTensorView features_dc,
    TorchTensorView features_sh,
    int sb_lobes,
    int sh_value_bits
) {
    const int64_t max_num_splats = std::get<2>(means)[0];
    if (std::get<2>(quats)[0] != max_num_splats ||
        std::get<2>(scales)[0] != max_num_splats ||
        std::get<2>(opacities)[0] != max_num_splats ||
        std::get<2>(features_dc)[0] != max_num_splats ||
        std::get<2>(features_sh)[0] != max_num_splats)
        throw std::runtime_error("engine_scene_set_data_3dgs: max_num_splats mismatch across splat tensors");
    if (max_num_splats < num_splats)
        throw std::runtime_error("engine_scene_set_data_3dgs: tensor size smaller than num_splats");
    const auto& sh_shape = std::get<2>(features_sh);
    const int num_sh = (sh_shape.size() >= 2) ? (int)sh_shape[1] : 0;
    engine_scene_begin_3dgs(slot, max_num_splats, num_sh, sb_lobes,
                            std::get<0>(features_sh) ? sh_value_bits : 32);
    engine_scene_upload_3dgs(slot, 0, means, quats, scales, opacities, features_dc,
                             features_sh);
    at(slot).cur_num_splats = num_splats;
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


void engine_scene_set_background_sh(int slot, int sh_degree,
                                   TorchTensorView coeffs, int transfer, bool is_linear) {
    Scene& s = at(slot);
    if (std::get<0>(coeffs) == 0) {
        s.has_bg_sh = false;
        s.bg_sh_degree = 0;
        s.bg_sh_coeffs = DeviceVector<float3>{};
        return;
    }
    const auto& shape = std::get<2>(coeffs);
    if (sh_degree < 0 || sh_degree > 4 || std::get<1>(coeffs) != sizeof(float) ||
        shape.size() != 2 || shape[0] != (sh_degree + 1) * (sh_degree + 1) || shape[1] != 3)
        throw std::runtime_error("engine_scene_set_background_sh: invalid coefficient shape");
    s.bg_transfer = transfer;
    s.bg_is_linear = is_linear;
    s.has_bg_sh = true;
    s.bg_sh_degree = sh_degree;
    s.bg_sh_coeffs = upload_dv<float3>(buffer_name(slot, "bg_sh_coeffs"), coeffs);
}


namespace {

// Bytes per element of a slot attribute, and where it lives, by the name
// engine_scene_update takes. Returns null for a name that is not one.
void* scene_field(Scene& s, const std::string& field, size_t& elem_bytes,
                  int64_t& count) {
    count = s.max_num_splats;
    if (field == "means")       { elem_bytes = sizeof(float3); return s.world.means.data_ptr(); }
    if (field == "quats")       { elem_bytes = sizeof(float4); return s.world.quats.data_ptr(); }
    if (field == "scales")      { elem_bytes = sizeof(float3); return s.world.scales.data_ptr(); }
    if (field == "opacities")   { elem_bytes = sizeof(float);  return s.world.opacities.data_ptr(); }
    if (field == "features_dc") { elem_bytes = sizeof(float3); return s.world.features_dc.data_ptr(); }
    if (field == "features_sh") {
        elem_bytes = sizeof(float3) * (size_t)std::max(s.num_sh, 0);
        return s.world.features_sh.data_ptr();
    }
    return nullptr;
}

}  // namespace


void engine_scene_update(int slot, const std::string& field,
                         TorchTensorView data) {
    Scene& s = at(slot);
    if (!s.loaded)
        throw std::runtime_error("engine_scene_update: slot holds no model");
    if (field == "features_sh" && s.world.sh_value_bits != 32) {
        if (std::get<0>(data) == 0) return;
        if (std::get<2>(data)[0] != s.max_num_splats)
            throw std::runtime_error("engine_scene_update: features_sh row count "
                                     "does not match the slot");
        const float* rows = (const float*)std::get<0>(data);
        if (s.world.sh_value_bits == 8)
            encode_sh_rows(s.world.features_sh_quant8, rows, s.max_num_splats, s.num_sh, false);
        else
            encode_sh_rows(s.world.features_sh_quant16, rows, s.max_num_splats, s.num_sh, false);
        return;
    }
    size_t elem_bytes = 0;
    int64_t count = 0;
    void* dst = scene_field(s, field, elem_bytes, count);
    if (!dst)
        throw std::runtime_error("engine_scene_update: no such field: " + field);
    if (std::get<0>(data) == 0 || elem_bytes == 0) return;
    if (std::get<2>(data)[0] != count)
        throw std::runtime_error("engine_scene_update: " + field +
                                 " row count does not match the slot");
    backend::memcpy_sync(dst, (void*)std::get<0>(data),
                         (size_t)count * elem_bytes,
                         backend::MemcpyKind::HostToDevice);
}


void engine_scene_activate(int slot) {
    Scene& s = at(slot);
    if (!s.loaded)
        throw std::runtime_error("engine_scene_activate: slot holds no model");
    engine().world = s.world;
    engine().cur_num_splats = s.cur_num_splats;
    engine().max_num_splats = s.max_num_splats;
    DevicePool::global().set_splat_counts(engine().cur_num_splats, engine().max_num_splats);
    engine().num_sh = s.num_sh;
    engine().sb_lobes = s.sb_lobes;
    if (g_cs_slot != slot || g_cs_gen != s.cs_gen) {
        engine_init_color_space(s.cs_enabled, s.cs_transfer, s.cs_is_linear,
                                s.cs_matrix, false, 0, false,
                                std::vector<float>{});
        g_cs_slot = slot;
        g_cs_gen = s.cs_gen;
    }
    if (s.has_bg_sh && s.bg_sh_coeffs.size() > 0) {
        engine().background.mode = EngineBackground::Mode::Sh;
        engine().background.enabled = true;
        engine().background.splat_transfer = s.bg_transfer;
        engine().background.splat_is_linear = s.bg_is_linear;
        engine().background.sh_degree = s.bg_sh_degree;
        engine().background.sh_coeffs = s.bg_sh_coeffs;
    } else {
        engine().background.enabled = false;
        engine().background.mode = EngineBackground::Mode::None;
        engine().background.sh_coeffs = DeviceVector<float3>{};
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
    if (s.bg_sh_coeffs.data_ptr() &&
        engine().background.sh_coeffs.data_ptr() == s.bg_sh_coeffs.data_ptr()) {
        engine().background.enabled = false;
        engine().background.mode = EngineBackground::Mode::None;
        engine().background.sh_coeffs = DeviceVector<float3>{};
    }
    DevicePool::global().free_dynamic_prefix(slot_prefix(slot));
    s = Scene{};
    if (g_cs_slot == slot) g_cs_slot = -1;
}


void engine_scenes_forget() {
    for (auto& s : scenes()) s = Scene{};
    g_cs_slot = -1;
}
