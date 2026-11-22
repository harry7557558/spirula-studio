// Streamed PLY export and native training-state archives; docs/training-resume.md.

#include "engine/Engine.h"
#include "engine/EngineInternal.h"
#include "engine/EngineState.h"

#include "checkpoint/BackgroundExport.h"
#include "checkpoint/SplatPly.h"
#include "checkpoint/SplatYUp.h"
#include "checkpoint/Resume.h"
#include "core/BufferedFile.h"
#include <future>
#include "core/AtomicFile.h"
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
#ifdef _OPENMP
#include <omp.h>
#endif


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

// Runtime scalars and layout validation; run configuration lives in config.json.
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
    j << "  \"sb_lobes\": " << s.sb_lobes << ",\n";
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
    j << "  \"background\": {\"mode\": "
      << (s.background.mode == EngineBackground::Mode::Sh ? "\"sh\"" : "\"other\"")
      << ", \"sh_degree\": " << s.background.sh_degree
      << ", \"splat_transfer\": " << s.background.splat_transfer
      << ", \"splat_is_linear\": " << b(s.background.splat_is_linear)
      << "},\n";
    j << "  \"calib_warp\": {\"enabled\": " << b(s.calib_warp.enabled);
    if (s.calib_warp.enabled)
        j << ", \"grid\": " << s.calib_warp.grid_n << ", \"D\": " << s.calib_warp.D;
    j << "},\n";
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

// Interrupted writes must not look complete to viewers or --resume.
// Files keep a .tmp suffix until every stream has closed successfully.
struct StagedFiles {
    std::vector<fs::path> finals;

    static fs::path temp_of(const fs::path& p) { return fs::path(p).concat(".tmp"); }
    fs::path add(const fs::path& final_path) {
        finals.push_back(final_path);
        return temp_of(final_path);
    }
    void publish() {
        for (const fs::path& p : finals) spirula::replace_file(temp_of(p), p);
        finals.clear();
    }
    ~StagedFiles() {
        std::error_code ec;
        for (const fs::path& p : finals) fs::remove(temp_of(p), ec);
    }
};

} // anon namespace


void engine_save_checkpoint(
    std::string output_dir,
    bool full_dump,
    int step,
    int background_export_height,
    std::string basename,
    bool y_up,
    std::string region_camera_hash
) {
    if (background_export_height < 1 || background_export_height > 8192)
        throw std::runtime_error("background_export_height must be in [1, 8192]");
    EngineState& s = engine();

    fs::path out_root(output_dir);
    _ckpt_mkdir(out_root);

    backend::device_synchronize();

    const int64_t N = s.cur_num_splats;
    const int K = s.num_sh;
    ColorBasis basis{0, s.sb_lobes};
    while (ColorBasis::sh_coeffs_of(basis.sh_degree + 1) <= K - basis.sb_coeffs())
        basis.sh_degree++;
    if (basis.coeffs() != K || basis.sh_degree > spirula::ShRotation::kMaxDegree)
        throw std::runtime_error("engine_save_checkpoint: invalid colour basis");
    const bool quant = K > 0 && s.world.sh_value_quantize_enabled();
    const int bits = s.world.sh_value_bits;
    const uint8_t* packed = nullptr;
    const float2* bounds = nullptr;
    bool fpbo = false;
    auto use_quant = [&](const auto& q, bool fused) {
        if (!q.initialized()) return;
        packed = q.packed.data_ptr();
        bounds = q.bounds.data_ptr();
        fpbo = fused;
    };
    if (quant) {
        if (bits == 8) {
            use_quant(s.world.features_sh_quant8, false);
            use_quant(s.world.features_sh_quant8_fpbo, true);
        } else {
            use_quant(s.world.features_sh_quant16, false);
            use_quant(s.world.features_sh_quant16_fpbo, true);
        }
        if (!packed || !bounds) throw std::runtime_error("checkpoint: missing quantized colour buffers");
    }
    const ShQuantAddr addr = sh_quant_addr((uint32_t)K, fpbo ? 0 : 256);
    const int row_floats = spirula::splat_ply_row_floats(basis);
    // Both quantization layouts restart on a 256-splat boundary.
    const int64_t chunk_rows = std::max<size_t>(256,
        spirula::kFileBufferBytes / (row_floats * sizeof(float)) / 256 * 256);
    StagedFiles staged;
    const fs::path ply_path = staged.add(out_root / (basename.empty() ? "splat.ply" : basename + ".ply"));
    {
        spirula::BufferedOutputFile ply(ply_path, std::ios::binary);
        if (!ply) throw std::runtime_error("Failed to open PLY for write: " + ply_path.string());
        ply << spirula::splat_ply_header(0, basis, s.primitive, y_up, region_camera_hash);
        const spirula::SplatYUp turn;
        std::vector<float3> means, scales, dc, sh;
        std::vector<float4> quats;
        std::vector<float> opacity, buf(chunk_rows * row_floats), writing(buf.size());
        std::vector<uint8_t> values;
        std::vector<float2> limits;
        std::vector<int64_t> indices(chunk_rows);
        std::future<void> pending;
        int64_t kept = 0;
        auto copy = [](auto& host, const auto* device, size_t count) {
            host.resize(count);
            if (count) backend::memcpy_sync(host.data(), device, count * sizeof(*device),
                                           backend::MemcpyKind::DeviceToHost);
        };
        for (int64_t first = 0; first < N; first += chunk_rows) {
            const int64_t n = std::min(chunk_rows, N-first);
            copy(means, s.world.means.data_ptr()+first, n);
            copy(quats, s.world.quats.data_ptr()+first, n);
            copy(scales, s.world.scales.data_ptr()+first, n);
            copy(opacity, s.world.opacities.data_ptr()+first, n);
            copy(dc, s.world.features_dc.data_ptr()+first, n);
            if (quant) {
                const int64_t cells = fpbo ? sh_fpbo_cells(n, K) : n*3*K;
                const int64_t start = fpbo ? sh_fpbo_cells(first, K) : first*3*K;
                copy(values, packed+start*(bits/8), cells*(bits/8));
                copy(limits, bounds+(fpbo ? first : start)/256,
                     ((fpbo ? n : cells)+255)/256);
            } else if (K > 0) {
                copy(sh, s.world.features_sh.data_ptr()+first*K, n*K);
            }
            auto finite3 = [](const float3& v) {
                return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
            };
            int rows = 0;
            for (int64_t i = 0; i < n; ++i) {
                const auto& q = quats[i];
                bool valid = finite3(means[i]) && finite3(scales[i]) && finite3(dc[i]) &&
                    std::isfinite(opacity[i]) && std::isfinite(q.x) && std::isfinite(q.y) &&
                    std::isfinite(q.z) && std::isfinite(q.w);
                if (!quant)
                    for (int j = 0; valid && j < K; ++j) valid = finite3(sh[i*K+j]);
                if (valid && opacity[i] >= -5.5373f &&
                    std::max({scales[i].x, scales[i].y, scales[i].z}) > -40.0f)
                    indices[rows++] = i;
            }
#pragma omp parallel num_threads(8) if(rows >= 4096)
            {
                std::vector<float> rest(3*K);
#pragma omp for schedule(static)
                for (int r = 0; r < rows; ++r) {
                    const int64_t i = indices[r];
                    for (int j = 0; j < 3*K; ++j) {
                        if (quant) {
                            const int64_t cell = addr.cell(addr.base(i), j);
                            const auto mm = limits[fpbo ? i/256 : cell/256];
                            uint16_t q = values[cell];
                            if (bits == 16) std::memcpy(&q, values.data()+2*cell, 2);
                            rest[j] = mm.x + (mm.y-mm.x) * (float(q) / (bits == 8 ? 255.f : 65535.f));
                        } else rest[j] = (&sh[i*K].x)[j];
                    }
                    float mean[3] = {means[i].x, means[i].y, means[i].z};
                    float quat[4] = {quats[i].x, quats[i].y, quats[i].z, quats[i].w};
                    if (y_up) turn.apply(mean, quat, K ? rest.data() : nullptr, basis);
                    float* row = buf.data() + size_t(r)*row_floats;
                    std::copy(mean, mean+3, row);
                    std::fill(row+3, row+6, 0.f);
                    row[6] = dc[i].x; row[7] = dc[i].y; row[8] = dc[i].z;
                    if (K) spirula::splat_ply_put_rest(rest.data(), basis, row+9);
                    int p = 9+3*K;
                    row[p++] = opacity[i];
                    row[p++] = scales[i].x; row[p++] = scales[i].y; row[p++] = scales[i].z;
                    std::copy(quat, quat+4, row+p);
                }
            }
            if (pending.valid()) pending.get();
            buf.swap(writing);
            pending = std::async(std::launch::async, [&, rows] {
                ply.write(reinterpret_cast<const char*>(writing.data()),
                          std::streamsize(rows)*row_floats*sizeof(float));
                if (!ply) throw std::runtime_error("Failed to write " + ply_path.string());
            });
            kept += rows;
        }
        if (pending.valid()) pending.get();
        // The fixed-width vertex count lets filtering finish in a single pass.
        ply.seekp(0);
        ply << spirula::splat_ply_header(kept, basis, s.primitive, y_up, region_camera_hash);
        ply.close();
        if (!ply) throw std::runtime_error("Failed to write " + ply_path.string());
    }

    // --- state.tar: metadata-driven resume payload (see file header) ---------
    const SaveClass save_min = full_dump ? SaveClass::Resume : SaveClass::Always;

    const fs::path tar_path = staged.add(out_root / (basename.empty() ? "state.tar" : basename + ".tar"));
    spirula::BufferedOutputFile tar(tar_path, std::ios::binary);
    if (!tar)
        throw std::runtime_error("Failed to open state.tar for write: " + tar_path.string());

    {
        std::string sj = _build_state_json(s, step, full_dump);
        ckpt::tar_write_bytes(tar, "state.json", sj.data(), sj.size());
    }

    std::vector<char> host_stage;
    for (const auto& sl : DevicePool::global().saved(save_min)) {
        ckpt::tar_write_npy_device(tar, sl.name + ".npy",
                                   sl.ptr, sl.nbytes, sl.dtype_tag, host_stage);
    }
    ckpt::tar_finish(tar);
    tar.close();
    if (!tar) throw std::runtime_error("Failed to write " + tar_path.string());

    if (s.background.mode == EngineBackground::Mode::Sh && s.background.sh_coeffs.size() > 0) {
        std::vector<float3> h_bg((size_t)s.background.sh_coeffs.size());
        backend::memcpy_sync(h_bg.data(), s.background.sh_coeffs.data_ptr(),
                             h_bg.size() * sizeof(float3),
                             backend::MemcpyKind::DeviceToHost);
        std::vector<float> matrix;
        if (s.color_space.splat_enabled)
            matrix = _ckpt_d2h(reinterpret_cast<const float*>(s.color_space.splat_color_matrix.data_ptr()), 9);
        const fs::path webp_path = staged.add(out_root / "background.webp");
        if (!ckpt::export_background_sh_webp(webp_path, h_bg.data(),
                                        s.background.sh_degree, 2 * background_export_height, background_export_height,
                                        s.background.splat_transfer,
                                        s.background.splat_is_linear,
                                        matrix.empty() ? nullptr : matrix.data()))
            throw std::runtime_error("failed to write " + webp_path.string());
    }
    staged.publish();
}


int engine_load_checkpoint(std::string input_dir, bool restore_projective_depth) {
    EngineState& s = engine();

    fs::path tarpath = ckpt::resolve_checkpoint(input_dir).tar_path;
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
    const int     sb_lobes      = (int)_json_int(sj, "sb_lobes", 0);
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
    DevicePool::global().set_splat_counts(cur_n, max_n);
    if (s.num_sh != 0 && (s.num_sh != num_sh || s.sb_lobes != sb_lobes))
        throw std::runtime_error(
            "engine_load_checkpoint: colour row mismatch (engine " +
            std::to_string(s.num_sh) + " coefficients, " +
            std::to_string(s.sb_lobes) + " lobes vs checkpoint " +
            std::to_string(num_sh) + ", " + std::to_string(sb_lobes) + ")");
    s.num_sh         = num_sh;
    s.sh_degree      = sh_degree;
    s.sb_lobes       = sb_lobes;
    s.packed         = (packed != 0);
    if (!primitive.empty()) s.primitive = primitive;
    s.optim.use_per_splat_bias_correction = (bias != 0);
    s.optim.use_fused_proj_bwd_optim      = (fused != 0);

    // Force-allocate optimizer + appearance-optimizer state so the restore
    // targets exist. No training step; no-ops for absent components.
    engine_ensure_optim_state(sh_opt_bits, sh_val_bits, non_sh_bits, bias != 0);
    _ensure_bilagrid_optim_state();
    _ensure_ppisp_optim_state();

    for (const auto& m : members) {
        if (m.name == "eng.bg_sky.sh_coeffs.npy") {
            int deg = 0;
            int bg_transfer = s.background.splat_transfer;
            bool bg_is_linear = s.background.splat_is_linear;
            std::vector<float3> coefficients;
            if (!ckpt::load_background_sh(tarpath, deg, coefficients, bg_transfer, bg_is_linear))
                throw std::runtime_error("engine_load_checkpoint: invalid SH background");
            engine_init_background_sh(deg, bg_transfer, bg_is_linear);
            break;
        }
    }
    _ensure_bg_sh_optim_state();

    // Restore each saved .npy into its pool slot by name.
    std::vector<char> host_stage;
    int restored = 0;
    if (!restore_projective_depth) engine_clear_projective_depth();
    for (auto& m : members) {
        const std::string& fn = m.name;
        if (fn.size() < 4 || fn.compare(fn.size() - 4, 4, ".npy") != 0) continue;
        std::string base = fn.substr(0, fn.size() - 4);

        PoolSlot slot; uint32_t sub;
        if (!parse_saved_name(base, slot, sub))
            throw std::runtime_error("engine_load_checkpoint: unknown slot '" + base + "'");

        if (!restore_projective_depth &&
            (slot == PoolSlot::EngProjectiveState || slot == PoolSlot::EngProjectiveInformation)) continue;
        if (slot == PoolSlot::EngProjectiveState)
            s.projective_state.resize(slot,s.max_num_splats);
        if (slot == PoolSlot::EngProjectiveInformation)
            s.projective_information.resize(slot,s.max_num_splats);

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
