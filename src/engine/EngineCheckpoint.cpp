// Engine checkpoint save.
//   * splat.ply -- inference artifact (filtered, dequantized Gaussians).
//   * state.tar -- resume payload: a zero-dep ustar of state.json + one flat
//     typed .npy per saved pool buffer, selected by SaveClass metadata:
//       full_dump=false -> Always slots (appearance/inference params)
//       full_dump=true  -> Always + Resume (world raw + all optimizer state)
//     The dump is metadata-driven (DevicePool::saved) so it can never fall out
//     of sync with the buffer set, and it copies device->host in chunks with no
//     extra device memory.

#include "engine/Engine.h"
#include "engine/EngineInternal.h"
#include "engine/EngineState.h"

#include "core/CheckpointIO.h"
#include "external/npy.hpp"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>


namespace {

namespace fs = std::filesystem;

static void _ckpt_mkdir(const fs::path& p) {
    std::error_code ec;
    fs::create_directories(p, ec);
    if (ec) throw std::runtime_error("create_directories failed: " + p.string() + " (" + ec.message() + ")");
}

// D->H copy into a freshly sized host buffer.
template<typename T>
static std::vector<T> _ckpt_d2h(const T* d_ptr, size_t n) {
    std::vector<T> host(n);
    if (n > 0 && d_ptr != nullptr) {
        backend::memcpy_sync(host.data(), d_ptr, n * sizeof(T), backend::MemcpyKind::DeviceToHost);
    }
    return host;
}

// Save a device-side buffer of `numel` elements as a .npy with the given shape.
template<typename Scalar>
static void _ckpt_save_npy(const fs::path& path, const Scalar* d_ptr, size_t numel,
                           const npy::shape_t& shape) {
    if (d_ptr == nullptr || numel == 0) return;
    auto host = _ckpt_d2h<Scalar>(d_ptr, numel);
    npy::npy_data_ptr<Scalar> data{host.data(), shape, false};
    npy::write_npy(path.string(), data);
}

template<typename T>
static npy::shape_t _ckpt_shape_5d(const DeviceTensor5D<T>& t) {
    return {
        (unsigned long)t.template size<0>(),
        (unsigned long)t.template size<1>(),
        (unsigned long)t.template size<2>(),
        (unsigned long)t.template size<3>(),
        (unsigned long)t.template size<4>(),
    };
}

// Runtime + validation manifest embedded in state.tar. JSON. Holds only what
// the loader needs that config.json cannot supply (cur_num_splats, active SH
// bands, allocated appearance dims) plus fields to fail-fast on a config /
// checkpoint mismatch. Deliberately NOT a config duplicate.
static std::string _build_state_json(EngineState& s, int step, bool full) {
    std::ostringstream j;
    auto b = [](bool v) { return v ? 1 : 0; };
    j << "{\n";
    j << "  \"format_version\": 1,\n";
    j << "  \"full_resume\": " << b(full) << ",\n";
    j << "  \"quant_codec\": \"joint_u_log_sqrt_g2_v1\",\n";
    j << "  \"quant_block_size\": 256,\n";
    j << "  \"quant_eps\": 1e-15,\n";
    j << "  \"step\": " << step << ",\n";
    j << "  \"primitive\": \"" << s.primitive << "\",\n";
    j << "  \"cur_num_splats\": " << s.cur_num_splats << ",\n";
    j << "  \"max_num_splats\": " << s.max_num_splats << ",\n";
    j << "  \"num_sh\": " << s.num_sh << ",\n";
    j << "  \"sh_degree\": " << s.sh_degree << ",\n";
    j << "  \"packed\": " << b(s.packed) << ",\n";
    j << "  \"sh_optim_bits\": " << s.optim.sh_optim_bits << ",\n";
    j << "  \"sh_value_bits\": " << s.world.sh_value_bits << ",\n";
    j << "  \"non_sh_optim_bits\": " << s.optim.non_sh_optim_bits << ",\n";
    // Reflects the ACTUAL allocated optimizer-state layout (FPBO vs regular).
    // The loader sets this before _ensure_optim_state so the restore targets
    // match the saved slot set.
    j << "  \"use_fused_proj_bwd_optim\": " << b(s.optim.fused_state_active) << ",\n";
    j << "  \"use_per_splat_bias_correction\": "
      << b(s.optim.use_per_splat_bias_correction) << ",\n";
    j << "  \"camera\": {\"num\": " << s.camera.num
      << ", \"width\": " << s.camera.width
      << ", \"height\": " << s.camera.height
      << ", \"model\": \"" << s.camera.model_str << "\"},\n";
    // Bilagrid channels. `type`/`C` only exist on the RGB struct, so they are
    // passed in explicitly (nullptr for depth/normal) to keep the lambda generic.
    auto emit_bg = [&](const char* key, auto& bg, const std::string* type, int C) {
        j << "  \"" << key << "\": {\"enabled\": " << b(bg.enabled);
        if (bg.enabled) {
            if (type) j << ", \"type\": \"" << *type << "\", \"C\": " << C;
            j << ", \"L\": " << bg.grids.template size<1>()
              << ", \"H\": " << bg.grids.template size<2>()
              << ", \"W\": " << bg.grids.template size<3>()
              << ", \"optim_bits\": " << bg.optim_bits
              << ", \"value_bits\": " << bg.value_bits
              << ", \"use_adagrad\": " << b(bg.use_adagrad);
        }
        j << "},\n";
    };
    emit_bg("bilagrid_rgb",    s.bilagrid_rgb,    &s.bilagrid_rgb.type, s.bilagrid_rgb.C);
    emit_bg("bilagrid_depth",  s.bilagrid_depth,  nullptr, 0);
    emit_bg("bilagrid_normal", s.bilagrid_normal, nullptr, 0);
    j << "  \"ppisp\": {\"enabled\": " << b(s.ppisp.enabled);
    if (s.ppisp.enabled)
        j << ", \"param_type\": \"" << s.ppisp.param_type << "\""
          << ", \"num_params\": " << s.ppisp.num_params
          << ", \"use_adagrad\": " << b(s.ppisp.use_adagrad);
    j << "}\n}\n";
    return j.str();
}

// --- Minimal extractors for our own flat state.json (top-level keys only,
//     which are unique, so a first-match find is sufficient). ---
static bool _json_find(const std::string& s, const std::string& key, size_t& vpos) {
    std::string pat = "\"" + key + "\"";
    size_t k = s.find(pat);
    if (k == std::string::npos) return false;
    size_t c = s.find(':', k + pat.size());
    if (c == std::string::npos) return false;
    vpos = c + 1;
    while (vpos < s.size() &&
           (s[vpos] == ' ' || s[vpos] == '\t' || s[vpos] == '\n' || s[vpos] == '\r'))
        ++vpos;
    return true;
}
static int64_t _json_int(const std::string& s, const std::string& key, int64_t def) {
    size_t v;
    if (!_json_find(s, key, v)) return def;
    return (int64_t)std::strtoll(s.c_str() + v, nullptr, 10);
}
static std::string _json_str(const std::string& s, const std::string& key) {
    size_t v;
    if (!_json_find(s, key, v) || s[v] != '"') return "";
    size_t e = s.find('"', v + 1);
    return (e == std::string::npos) ? "" : s.substr(v + 1, e - v - 1);
}

} // anon namespace


void engine_save_checkpoint(
    std::string output_dir,
    bool full_dump,
    int step
) {
    EngineState& s = engine();

    fs::path out_root(output_dir);
    _ckpt_mkdir(out_root);

    backend::device_synchronize();

    const int64_t N = s.cur_num_splats;
    const int K = s.num_sh;

    // --- D->H copy world splat tensors at s.cur_num_splats ---
    auto h_means        = _ckpt_d2h<float3>(s.world.means.data_ptr(),        (size_t)N);
    auto h_quats        = _ckpt_d2h<float4>(s.world.quats.data_ptr(),        (size_t)N);
    auto h_scales       = _ckpt_d2h<float3>(s.world.scales.data_ptr(),       (size_t)N);
    auto h_opacities    = _ckpt_d2h<float >(s.world.opacities.data_ptr(),    (size_t)N);
    auto h_features_dc  = _ckpt_d2h<float3>(s.world.features_dc.data_ptr(),  (size_t)N);
    // features_sh is stored [max_N, K] contiguous; copying first N rows = N*K float3.
    // When sh_value_bits != 32 the canonical store is the packed buffer
    // (`features_sh_quant{8,16}{,_fpbo}`); decode it on the host into the
    // PLY row directly so we don't pay the ~N*K*12B fp32 staging allocation.
    // The packed copy is ~N*3*K bytes (8-bit) or 2x (16-bit), much smaller
    // than the fp32 staging would be.
    const bool quant_sh_value = (K > 0) && s.world.sh_value_quantize_enabled();
    const int  sh_value_bits  = s.world.sh_value_bits;

    std::vector<float3> h_features_sh;            // fp32 path only
    std::vector<uint8_t> h_value_packed;          // quant path only
    std::vector<float2>  h_value_bounds;          // quant path only
    ShQuantAddr sh_addr{256, 0, 3 * (int64_t)K};  // quant path: cell-block vs fpbo
    if (K > 0 && !quant_sh_value) {
        h_features_sh = _ckpt_d2h<float3>(s.world.features_sh.data_ptr(), (size_t)(N * K));
    } else if (quant_sh_value) {
        // Pick the populated layout. FPBO (per-splat-block) takes precedence
        // since that's what `_ensure_optim_state` allocates when FPBO is on.
        bool fpbo_layout = false;
        const uint8_t* dev_packed = nullptr;
        const float2*  dev_bounds = nullptr;
        int64_t        dev_n_bounds = 0;
        if (sh_value_bits == 8) {
            if (s.world.features_sh_quant8_fpbo.initialized()) {
                dev_packed = s.world.features_sh_quant8_fpbo.packed.data_ptr();
                dev_bounds = s.world.features_sh_quant8_fpbo.bounds.data_ptr();
                dev_n_bounds = s.world.features_sh_quant8_fpbo.n_bounds;
                fpbo_layout = true;
            } else if (s.world.features_sh_quant8.initialized()) {
                dev_packed = s.world.features_sh_quant8.packed.data_ptr();
                dev_bounds = s.world.features_sh_quant8.bounds.data_ptr();
                dev_n_bounds = s.world.features_sh_quant8.n_bounds;
            }
        } else /* sh_value_bits == 16 */ {
            if (s.world.features_sh_quant16_fpbo.initialized()) {
                dev_packed = s.world.features_sh_quant16_fpbo.packed.data_ptr();
                dev_bounds = s.world.features_sh_quant16_fpbo.bounds.data_ptr();
                dev_n_bounds = s.world.features_sh_quant16_fpbo.n_bounds;
                fpbo_layout = true;
            } else if (s.world.features_sh_quant16.initialized()) {
                dev_packed = s.world.features_sh_quant16.packed.data_ptr();
                dev_bounds = s.world.features_sh_quant16.bounds.data_ptr();
                dev_n_bounds = s.world.features_sh_quant16.n_bounds;
            }
        }
        if (dev_packed != nullptr && dev_bounds != nullptr) {
            // Copy only the bytes needed for the first N splats (skip the
            // tail of the over-allocated max_N buffer).
            const int64_t cells_needed =
                fpbo_layout ? sh_fpbo_cells(N, (uint32_t)K) : (int64_t)N * 3 * K;
            const int bytes_per_cell = (sh_value_bits == 8) ? 1 : 2;
            h_value_packed.resize((size_t)(cells_needed * bytes_per_cell));
            backend::memcpy_sync(h_value_packed.data(), dev_packed,
                       (size_t)(cells_needed * bytes_per_cell),
                       backend::MemcpyKind::DeviceToHost);
            sh_addr = sh_quant_addr((uint32_t)K, fpbo_layout ? 0 : 256);
            int64_t n_bounds_needed;
            if (fpbo_layout) {
                n_bounds_needed = (N + 255) / 256;
            } else {
                n_bounds_needed = (cells_needed + 255) / 256;
            }
            // Don't run off the allocated bounds array.
            n_bounds_needed = std::min(n_bounds_needed, dev_n_bounds);
            h_value_bounds.resize((size_t)n_bounds_needed);
            backend::memcpy_sync(h_value_bounds.data(), dev_bounds,
                       (size_t)n_bounds_needed * sizeof(float2),
                       backend::MemcpyKind::DeviceToHost);
        }
    }

    // Decode one SH cell value on the host. Inlined into the PLY write loop
    // below; safe no-op (returns 0) if quant buffers weren't populated.
    auto decode_sh = [&](int64_t splat_i, int j, int ch) -> float {
        if (h_value_packed.empty() || h_value_bounds.empty()) return 0.0f;
        const int64_t cell = sh_addr.cell(sh_addr.base(splat_i), 3 * j + ch);
        const int64_t b_idx = cell / sh_addr.bounds_stride;
        if (b_idx >= (int64_t)h_value_bounds.size()) return 0.0f;
        const float2 mm = h_value_bounds[(size_t)b_idx];
        if (sh_value_bits == 8) {
            const uint8_t q = h_value_packed[(size_t)cell];
            return mm.x + (mm.y - mm.x) * ((float)q * (1.0f / 255.0f));
        } else {
            const uint16_t* p16 = reinterpret_cast<const uint16_t*>(h_value_packed.data());
            const uint16_t q = p16[(size_t)cell];
            return mm.x + (mm.y - mm.x) * ((float)q * (1.0f / 65535.0f));
        }
    };

    // --- Filter mask: NaN/Inf + low-opacity (logit < logit(1/255) ~= -5.5373)
    // + dead scale (densify recycles those, but it stops before the run ends)
    const float OPA_MIN = -5.5373f;
    const float SCALE_MIN = -40.0f;  // == kDeadLogScale, shaders/densify.slang
    auto fin1 = [](float v) { return std::isfinite(v); };
    auto fin3 = [&](const float3& v) { return fin1(v.x) && fin1(v.y) && fin1(v.z); };
    auto fin4 = [&](const float4& v) { return fin1(v.x) && fin1(v.y) && fin1(v.z) && fin1(v.w); };

    std::vector<char> keep((size_t)N, 1);
    int64_t kept = 0, nan_dropped = 0, lowopa_dropped = 0, deadscale_dropped = 0;
    for (int64_t i = 0; i < N; i++) {
        bool ok = fin3(h_means[i]) && fin4(h_quats[i]) && fin3(h_scales[i])
                  && fin1(h_opacities[i]) && fin3(h_features_dc[i]);
        // Quant-decoded SH values are finite by construction (byte to fp32
        // linear interp from finite bounds), so skip the NaN check on that
        // path. The fp32 path still validates each coef.
        if (ok && K > 0 && !quant_sh_value) {
            for (int j = 0; j < K; j++) {
                if (!fin3(h_features_sh[i*K + j])) { ok = false; break; }
            }
        }
        if (!ok) { keep[i] = 0; nan_dropped++; continue; }
        if (h_opacities[i] < OPA_MIN) { keep[i] = 0; lowopa_dropped++; continue; }
        const float3& sc = h_scales[i];
        if (std::max(std::max(sc.x, sc.y), sc.z) <= SCALE_MIN) {
            keep[i] = 0; deadscale_dropped++; continue;
        }
        kept++;
    }

    // --- PLY (binary little-endian) ---
    const int vertex_floats = 3 + 3 + 3 + 3 * K + 1 + 3 + 4;  // pos + normal + dc + sh + opa + scale + rot
    fs::path ply_path = out_root / "splat.ply";
    {
        std::ofstream ply(ply_path, std::ios::binary);
        if (!ply) throw std::runtime_error("Failed to open PLY for write: " + ply_path.string());
        ply << "ply\n";
        ply << "format binary_little_endian 1.0\n";
        ply << "element vertex " << kept << "\n";
        ply << "property float x\nproperty float y\nproperty float z\n";
        ply << "property float nx\nproperty float ny\nproperty float nz\n";
        ply << "property float f_dc_0\nproperty float f_dc_1\nproperty float f_dc_2\n";
        for (int j = 0; j < 3 * K; j++) ply << "property float f_rest_" << j << "\n";
        ply << "property float opacity\n";
        ply << "property float scale_0\nproperty float scale_1\nproperty float scale_2\n";
        ply << "property float rot_0\nproperty float rot_1\nproperty float rot_2\nproperty float rot_3\n";
        ply << "end_header\n";

        // Buffer multiple rows per write for throughput.
        constexpr int ROWS_PER_FLUSH = 1024;
        std::vector<float> buf((size_t)vertex_floats * ROWS_PER_FLUSH);
        int rows_in_buf = 0;
        auto flush = [&]() {
            if (rows_in_buf == 0) return;
            ply.write(reinterpret_cast<const char*>(buf.data()),
                      (std::streamsize)rows_in_buf * vertex_floats * sizeof(float));
            rows_in_buf = 0;
        };
        for (int64_t i = 0; i < N; i++) {
            if (!keep[i]) continue;
            float* row = buf.data() + (size_t)rows_in_buf * vertex_floats;
            int p = 0;
            row[p++] = h_means[i].x; row[p++] = h_means[i].y; row[p++] = h_means[i].z;
            row[p++] = 0.f; row[p++] = 0.f; row[p++] = 0.f;  // nx, ny, nz
            row[p++] = h_features_dc[i].x; row[p++] = h_features_dc[i].y; row[p++] = h_features_dc[i].z;
            // SH rest in (3, K) order: r0..r_{K-1}, g0..g_{K-1}, b0..b_{K-1}.
            // Branch hoisted out of the inner loops so the codec decode only
            // runs when value-quant is active.
            if (quant_sh_value) {
                for (int j = 0; j < K; j++) row[p++] = decode_sh(i, j, 0);
                for (int j = 0; j < K; j++) row[p++] = decode_sh(i, j, 1);
                for (int j = 0; j < K; j++) row[p++] = decode_sh(i, j, 2);
            } else {
                for (int j = 0; j < K; j++) row[p++] = h_features_sh[i*K + j].x;
                for (int j = 0; j < K; j++) row[p++] = h_features_sh[i*K + j].y;
                for (int j = 0; j < K; j++) row[p++] = h_features_sh[i*K + j].z;
            }
            row[p++] = h_opacities[i];
            row[p++] = h_scales[i].x; row[p++] = h_scales[i].y; row[p++] = h_scales[i].z;
            row[p++] = h_quats[i].x; row[p++] = h_quats[i].y; row[p++] = h_quats[i].z; row[p++] = h_quats[i].w;
            rows_in_buf++;
            if (rows_in_buf == ROWS_PER_FLUSH) flush();
        }
        flush();
    }

    // --- state.tar: metadata-driven resume payload (see file header) ---------
    // Which buffers to serialize: Always (base) or Always+Resume (full resume).
    const SaveClass save_min = full_dump ? SaveClass::Resume : SaveClass::Always;

    std::ofstream tar((out_root / "state.tar").string(), std::ios::binary);
    if (!tar)
        throw std::runtime_error("Failed to open state.tar for write: "
                                 + (out_root / "state.tar").string());

    // state.json -- runtime + validation manifest (config lives in config.json).
    {
        std::string sj = _build_state_json(s, step, full_dump);
        ckpt::tar_write_bytes(tar, "state.json", sj.data(), sj.size());
    }

    // One flat typed .npy per saved pool slot. Chunked D->H copy through a small
    // reusable host buffer -- bounded host RAM, ZERO extra device memory.
    std::vector<char> host_stage;
    for (const auto& sl : DevicePool::global().saved(save_min)) {
        ckpt::tar_write_npy_device(tar, sl.name + ".npy",
                                   sl.ptr, sl.nbytes, sl.dtype_tag, host_stage);
    }
    ckpt::tar_finish(tar);
}


// Restore engine state from a checkpoint's state.tar for resuming training.
//
// Preconditions (established by the resume flow from config.json): the engine
// skeleton is configured -- world buffers seeded/allocated at max_num_splats
// (set_data_3dgs), and any bilagrid/PPISP channels present in the checkpoint
// initialized (engine_init_bilagrid_*/engine_init_ppisp). This function then:
//   1. reads state.json and installs runtime scalars + the optimizer-state
//      layout flags (quant bits, FPBO, bias correction),
//   2. force-allocates optimizer + appearance-optimizer buffers to that layout
//      (no training step is run; the _ensure_* calls only allocate + zero, and
//      no-op for absent components),
//   3. memcpies every saved .npy back into its pool slot by name (chunked
//      H->D, no extra device memory), validating byte sizes,
//   4. leaves *_initialized flags set so the lazy guards won't re-zero data.
// Returns the saved training `step`.
int engine_load_checkpoint(std::string input_dir) {
    EngineState& s = engine();

    fs::path tarpath = fs::path(input_dir) / "state.tar";
    std::ifstream in(tarpath.string(), std::ios::binary);
    if (!in)
        throw std::runtime_error("engine_load_checkpoint: cannot open " + tarpath.string());

    auto members = ckpt::tar_index(in);
    std::string sj;
    for (auto& m : members)
        if (m.name == "state.json") { sj = ckpt::tar_read_member(in, m); break; }
    if (sj.empty())
        throw std::runtime_error("engine_load_checkpoint: state.json missing in "
                                 + tarpath.string());

    const int     step          = (int)_json_int(sj, "step", 0);
    const int64_t cur_n         = _json_int(sj, "cur_num_splats", 0);
    const int64_t max_n         = _json_int(sj, "max_num_splats", 0);
    const int     num_sh        = (int)_json_int(sj, "num_sh", 0);
    const int     sh_degree     = (int)_json_int(sj, "sh_degree", 0);
    const int     packed        = (int)_json_int(sj, "packed", 0);
    const int     sh_opt_bits   = (int)_json_int(sj, "sh_optim_bits", 32);
    const int     sh_val_bits   = (int)_json_int(sj, "sh_value_bits", 32);
    const int     non_sh_bits   = (int)_json_int(sj, "non_sh_optim_bits", 32);
    const int     bias          = (int)_json_int(sj, "use_per_splat_bias_correction", 0);
    const int     fused         = (int)_json_int(sj, "use_fused_proj_bwd_optim", 0);
    const std::string primitive = _json_str(sj, "primitive");

    // Capacity must match the skeleton, or per-slot byte counts won't line up.
    if (s.max_num_splats != 0 && s.max_num_splats != max_n)
        throw std::runtime_error(
            "engine_load_checkpoint: max_num_splats mismatch (engine "
            + std::to_string(s.max_num_splats) + " vs checkpoint "
            + std::to_string(max_n) + ")");

    // Install runtime scalars + optimizer layout from the checkpoint.
    s.cur_num_splats = cur_n;
    s.max_num_splats = max_n;
    s.num_sh         = num_sh;
    s.sh_degree      = sh_degree;
    s.packed         = (packed != 0);
    if (!primitive.empty()) s.primitive = primitive;
    s.optim.use_per_splat_bias_correction = (bias != 0);
    s.optim.use_fused_proj_bwd_optim      = (fused != 0);

    // Force-allocate optimizer + appearance-optimizer state so the restore
    // targets exist. No training step; no-ops for absent components.
    engine_ensure_optim_state(sh_opt_bits, sh_val_bits, non_sh_bits, bias != 0);
    _ensure_bilagrid_optim_state();
    _ensure_ppisp_optim_state();

    // Restore each saved .npy into its pool slot by name.
    std::vector<char> host_stage;
    int restored = 0;
    for (auto& m : members) {
        const std::string& fn = m.name;
        if (fn.size() < 4 || fn.compare(fn.size() - 4, 4, ".npy") != 0) continue;
        std::string base = fn.substr(0, fn.size() - 4);

        PoolSlot slot; uint32_t sub;
        if (!parse_saved_name(base, slot, sub))
            throw std::runtime_error("engine_load_checkpoint: unknown slot '" + base + "'");

        void*  ptr    = nullptr;
        size_t nbytes = 0;
        if (!DevicePool::global().lookup(slot, sub, ptr, nbytes))
            throw std::runtime_error(
                "engine_load_checkpoint: target slot not allocated for '" + base
                + "' (config/checkpoint mismatch?)");

        auto info = ckpt::npy_locate(in, m.data_offset, m.size);
        if (info.data_bytes != nbytes)
            throw std::runtime_error(
                "engine_load_checkpoint: size mismatch for '" + base + "': checkpoint "
                + std::to_string(info.data_bytes) + " vs slot " + std::to_string(nbytes));

        ckpt::read_into_device(in, info.data_offset, ptr, nbytes, host_stage);
        ++restored;
    }

    backend::device_synchronize();
    return step;
}
