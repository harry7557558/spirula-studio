// Streamed PLY export and native training-state archives; docs/training-resume.md.

#include "engine/Engine.h"
#include "engine/EngineInternal.h"
#include "engine/EngineState.h"

#include "checkpoint/BackgroundExport.h"
#include "checkpoint/SplatPly.h"
#include "checkpoint/SplatYUp.h"
#include "checkpoint/Resume.h"
#include "checkpoint/StagedFiles.h"
#include "core/CheckpointIO.h"
#include "core/HostMemory.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>
#ifdef _OPENMP
#include <omp.h>
#endif


namespace {

namespace fs = std::filesystem;

std::function<void()> g_checkpoint_writer_gate;


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

constexpr size_t kNoRange = std::numeric_limits<size_t>::max();
constexpr uint64_t kSnapshotHeadroom = uint64_t{1} << 30;
constexpr size_t kSnapshotAlignment = alignof(float4);

struct SnapshotDelete {
    void operator()(char* p) const { ::operator delete(p, std::align_val_t{kSnapshotAlignment}); }
};
using SnapshotBytes = std::unique_ptr<char[], SnapshotDelete>;

struct CheckpointData {
    struct Range { size_t offset, bytes; };
    struct Member { std::string name; uint8_t dtype; };
    std::vector<Range> ranges;
    std::vector<Member> members;
    SnapshotBytes host;
    size_t bytes = 0;
    std::string state_json, primitive;
    int64_t splats = 0;
    ColorBasis basis{};
    int sh_bits = 32;
    bool quant = false, fpbo = false;
    int bg_degree = 0, bg_transfer = 0;
    bool bg_linear = false;
    size_t means = kNoRange, quats = kNoRange, scales = kNoRange, opacity = kNoRange, dc = kNoRange;
    size_t sh = kNoRange, values = kNoRange, bounds = kNoRange;
    size_t bg = kNoRange, matrix = kNoRange, bg_count = 0;

    int64_t chunk_rows() const {
        // Both quantization layouts restart on a 256-splat boundary.
        return std::max<size_t>(256, spirula::kFileBufferBytes /
            (spirula::splat_ply_row_floats(basis) * sizeof(float)) / 256 * 256);
    }

    uint64_t writer_bytes(int height) const {
        const size_t rows = size_t(std::min(splats, chunk_rows()));
        const size_t ply = rows * (spirula::splat_ply_row_floats(basis) * sizeof(float) + sizeof(int64_t));
        // RGB, predictor and encoder storage, including vector growth and encoded bytes.
        const uint64_t webp = bg == kNoRange ? 0 : uint64_t{64} * height * (2 * height);
        return spirula::kFileBufferBytes + std::max(uint64_t(ply), webp) + (16u << 20);
    }

    const char* source(size_t i, const std::vector<const void*>* device) const {
        return device ? static_cast<const char*>((*device)[i]) : host.get() + ranges[i].offset;
    }
};

struct Capture {
    CheckpointData data;
    std::vector<const void*> device;

    size_t add(const void* ptr, size_t bytes) {
        if (!bytes) return kNoRange;
        if (!ptr) throw std::runtime_error("checkpoint: missing device buffer");
        constexpr size_t mask = kSnapshotAlignment - 1;
        if (bytes > std::numeric_limits<size_t>::max() - data.bytes - mask)
            throw std::length_error("checkpoint snapshot size overflow");
        const size_t index = device.size();
        device.push_back(ptr);
        data.ranges.push_back({data.bytes, bytes});
        data.bytes += (bytes + mask) & ~mask;
        return index;
    }

    template<class T>
    size_t view(const T* ptr, size_t count) {
        const size_t bytes = count * sizeof(T);
        if (!bytes) return kNoRange;
        for (size_t i = 0; i < device.size(); ++i)
            if (device[i] == ptr && bytes <= data.ranges[i].bytes) return i;
        return add(ptr, bytes);
    }
};

static void check_device_copy() {
    if (const char* err = backend::last_error())
        throw std::runtime_error(std::string("checkpoint device transfer: ") + err);
}

static void validate_height(int height) {
    if (height < 1 || height > 8192)
        throw std::runtime_error("background_export_height must be in [1, 8192]");
}

static Capture describe_checkpoint(bool full) {
    auto& s = engine();
    Capture capture;
    auto& d = capture.data;
    d.primitive = s.primitive;
    d.splats = s.cur_num_splats;
    const size_t n = size_t(d.splats);
    const int k = s.num_sh;
    d.basis = {0, s.sb_lobes};
    while (ColorBasis::sh_coeffs_of(d.basis.sh_degree + 1) <= k - d.basis.sb_coeffs())
        d.basis.sh_degree++;
    if (d.basis.coeffs() != k || d.basis.sh_degree > spirula::ShRotation::kMaxDegree)
        throw std::runtime_error("engine_save_checkpoint: invalid colour basis");

    auto slots = DevicePool::global().saved(full ? SaveClass::Resume : SaveClass::Always);
    capture.device.reserve(slots.size() + 9);
    d.ranges.reserve(slots.size() + 9);
    d.members.reserve(slots.size());
    for (auto& slot : slots) {
        capture.add(slot.ptr, slot.nbytes);
        d.members.push_back({std::move(slot.name), slot.dtype_tag});
    }

    d.means = capture.view(s.world.means.data_ptr(), n);
    d.quats = capture.view(s.world.quats.data_ptr(), n);
    d.scales = capture.view(s.world.scales.data_ptr(), n);
    d.opacity = capture.view(s.world.opacities.data_ptr(), n);
    d.dc = capture.view(s.world.features_dc.data_ptr(), n);
    d.quant = k > 0 && s.world.sh_value_quantize_enabled();
    d.sh_bits = s.world.sh_value_bits;
    if (d.quant) {
        const uint8_t* packed = nullptr;
        const float2* bounds = nullptr;
        auto use_quant = [&](const auto& q, bool fused) {
            if (!q.initialized()) return;
            packed = q.packed.data_ptr();
            bounds = q.bounds.data_ptr();
            d.fpbo = fused;
        };
        if (d.sh_bits == 8) {
            use_quant(s.world.features_sh_quant8, false);
            use_quant(s.world.features_sh_quant8_fpbo, true);
        } else {
            use_quant(s.world.features_sh_quant16, false);
            use_quant(s.world.features_sh_quant16_fpbo, true);
        }
        if (!packed || !bounds) throw std::runtime_error("checkpoint: missing quantized colour buffers");
        const size_t cells = d.fpbo ? size_t(sh_fpbo_cells(d.splats, k)) : n * 3 * k;
        d.values = capture.view(packed, cells * (d.sh_bits / 8));
        d.bounds = capture.view(bounds, ((d.fpbo ? n : cells) + 255) / 256);
    } else if (k > 0) {
        d.sh = capture.view(s.world.features_sh.data_ptr(), n * k);
    }

    if (s.background.mode == EngineBackground::Mode::Sh && s.background.sh_coeffs.size() > 0) {
        d.bg_count = size_t(s.background.sh_coeffs.size());
        d.bg = capture.view(s.background.sh_coeffs.data_ptr(), d.bg_count);
        d.bg_degree = s.background.sh_degree;
        d.bg_transfer = s.background.splat_transfer;
        d.bg_linear = s.background.splat_is_linear;
        if (s.color_space.splat_enabled)
            d.matrix = capture.view(reinterpret_cast<const float*>(s.color_space.splat_color_matrix.data_ptr()), 9);
    }
    return capture;
}

static const char* snapshot_rejection(double peak, uint64_t bytes, uint64_t scratch) {
    const auto mem = backend::memory_usage();
#ifdef _WIN32
    const uint64_t free = mem.has_commit ? mem.commit_free_bytes : 0;
#else
    const uint64_t free = spirula::availableRamBytes();
#endif
    if (!free) return "headroom_unavailable";
    const double growth = mem.has_process && std::isfinite(peak) ? peak - double(mem.process_bytes) : 0;
    if (bytes > free || scratch > free - bytes ||
        double(free - bytes - scratch) < std::max(double(kSnapshotHeadroom), growth))
        return "insufficient_headroom";
    return nullptr;
}

template<class T>
static const T* checkpoint_view(const CheckpointData& d, const std::vector<const void*>* device,
                                std::vector<T>& scratch, size_t range, size_t first, size_t count) {
    const char* src = d.source(range, device) + first * sizeof(T);
    if (!device) return reinterpret_cast<const T*>(src);
    scratch.resize(count);
    backend::memcpy_sync(scratch.data(), src, count * sizeof(T), backend::MemcpyKind::DeviceToHost);
    check_device_copy();
    return scratch.data();
}

static void write_ply(std::ostream& ply, const CheckpointData& d,
                      const std::vector<const void*>* device, bool y_up, const std::string& camera_hash) {
    const int64_t N = d.splats;
    const ColorBasis basis = d.basis;
    const int K = basis.coeffs();
    const bool quant = d.quant, fpbo = d.fpbo;
    const int bits = d.sh_bits;
    const ShQuantAddr addr = sh_quant_addr((uint32_t)K, fpbo ? 0 : 256);
    const int row_floats = spirula::splat_ply_row_floats(basis);
    const int64_t chunk_rows = d.chunk_rows();
    const size_t max_rows = size_t(std::min(N, chunk_rows));
    ply << spirula::splat_ply_header(0, basis, d.primitive, y_up, camera_hash);
    const spirula::SplatYUp turn;
    std::vector<float3> h_means, h_scales, h_dc, h_sh;
    std::vector<float4> h_quats;
    std::vector<float> h_opacity, buf(max_rows * row_floats), writing(device ? buf.size() : 0);
    std::vector<uint8_t> h_values;
    std::vector<float2> h_limits;
    std::vector<int64_t> indices(max_rows);
    std::future<void> pending;
    int64_t kept = 0;
    for (int64_t first = 0; first < N; first += chunk_rows) {
        const int64_t n = std::min(chunk_rows, N-first);
        auto view = [&](auto& scratch, size_t range, size_t start, size_t count) {
            return checkpoint_view(d, device, scratch, range, start, count);
        };
        const auto* means = view(h_means, d.means, first, n);
        const auto* quats = view(h_quats, d.quats, first, n);
        const auto* scales = view(h_scales, d.scales, first, n);
        const auto* opacity = view(h_opacity, d.opacity, first, n);
        const auto* dc = view(h_dc, d.dc, first, n);
        const uint8_t* values = nullptr;
        const float2* limits = nullptr;
        const float3* sh = nullptr;
        if (quant) {
            const int64_t cells = fpbo ? sh_fpbo_cells(n, K) : n*3*K;
            const int64_t start = fpbo ? sh_fpbo_cells(first, K) : first*3*K;
            values = view(h_values, d.values, start*(bits/8), cells*(bits/8));
            limits = view(h_limits, d.bounds, (fpbo ? first : start)/256,
                          ((fpbo ? n : cells)+255)/256);
        } else if (K > 0) {
            sh = view(h_sh, d.sh, first*K, n*K);
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
                        uint16_t q;
                        if (bits == 16) std::memcpy(&q, values+2*cell, 2);
                        else q = values[cell];
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
        const auto write = [&ply, rows, row_floats](const float* data) {
            if (!ply.write(reinterpret_cast<const char*>(data),
                           std::streamsize(rows)*row_floats*sizeof(float)))
                throw std::runtime_error("failed to write checkpoint PLY");
        };
        if (device) {
            if (pending.valid()) pending.get();
            buf.swap(writing);
            pending = std::async(std::launch::async, [write, &writing] { write(writing.data()); });
        } else {
            write(buf.data());
        }
        kept += rows;
    }
    if (pending.valid()) pending.get();
    // The fixed-width vertex count lets filtering finish in a single pass.
    ply.seekp(0);
    ply << spirula::splat_ply_header(kept, basis, d.primitive, y_up, camera_hash);
}

static void write_checkpoint(const CheckpointData& d, const std::vector<const void*>* device,
                             const fs::path& root, int height, const std::string& basename,
                             bool y_up, const std::string& camera_hash) {
    fs::create_directories(root);
    ckpt::StagedFiles staged;
    staged.write(root / (basename.empty() ? "splat.ply" : basename + ".ply"),
                 [&](std::ostream& ply) { write_ply(ply, d, device, y_up, camera_hash); });
    if (d.bg != kNoRange) {
        staged.write(root / "background.webp", [&](std::ostream& webp) {
            std::vector<float3> coeffs;
            std::vector<float> matrix;
            const auto* bg = checkpoint_view(d, device, coeffs, d.bg, 0, d.bg_count);
            const auto* transform = d.matrix == kNoRange ? nullptr :
                checkpoint_view(d, device, matrix, d.matrix, 0, 9);
            if (!ckpt::export_background_sh_webp(webp, bg, d.bg_degree, 2 * height, height,
                                                 d.bg_transfer, d.bg_linear, transform))
                throw std::runtime_error("failed to write checkpoint background.webp");
        });
    }
    staged.write(root / (basename.empty() ? "state.tar" : basename + ".tar"), [&](std::ostream& tar) {
        ckpt::tar_write_bytes(tar, "state.json", d.state_json.data(), d.state_json.size());
        std::vector<char> scratch;
        for (size_t i = 0; i < d.members.size(); ++i) {
            const auto& m = d.members[i];
            ckpt::tar_write_npy(tar, m.name + ".npy", d.source(i, device), d.ranges[i].bytes,
                                m.dtype, device ? &scratch : nullptr);
            if (device) check_device_copy();
        }
        ckpt::tar_finish(tar);
    });
    staged.publish();
}

}  // namespace

void engine_save_checkpoint(std::string output_dir, bool full_dump, int step,
                            int background_export_height, std::string basename,
                            bool y_up, std::string region_camera_hash) {
    validate_height(background_export_height);
    backend::device_synchronize();
    check_device_copy();
    auto c = describe_checkpoint(full_dump);
    c.data.state_json = _build_state_json(engine(), step, full_dump);
    write_checkpoint(c.data, &c.device, output_dir, background_export_height,
                     basename, y_up, region_camera_hash);
}

CheckpointSubmission engine_try_save_checkpoint_async(
    std::mutex& engine_mutex, double projected_device_peak,
    const std::string& output_dir, bool full_dump, int step, int background_export_height,
    const std::string& basename, bool y_up, const std::string& region_camera_hash) try {
    validate_height(background_export_height);
    std::unique_lock<std::mutex> lock(engine_mutex);
    const auto sizing = [&] {
        const auto c = describe_checkpoint(full_dump);
        return std::make_pair(c.data.bytes, c.data.writer_bytes(background_export_height));
    }();
    const auto [bytes, scratch] = sizing;
    lock.unlock();
    if (const char* reason = snapshot_rejection(projected_device_peak, bytes, scratch))
        return {{}, reason};
    SnapshotBytes host(static_cast<char*>(::operator new(bytes, std::align_val_t{kSnapshotAlignment})));
    // Touch at least once per page before rechecking headroom.
    volatile char* pages = host.get();
    for (size_t off = 0; off < bytes; off += 4096) pages[off] = 0;
    if (bytes) pages[bytes - 1] = 0;

    lock.lock();
    backend::device_synchronize();
    check_device_copy();
    auto c = describe_checkpoint(full_dump);
    const auto actual_scratch = c.data.writer_bytes(background_export_height);
    if (c.data.bytes != bytes || actual_scratch > scratch) {
        lock.unlock();
        return {{}, "layout_changed"};
    }
    if (const char* reason = snapshot_rejection(projected_device_peak, 0, actual_scratch)) {
        lock.unlock();
        return {{}, reason};
    }
    c.data.state_json = _build_state_json(engine(), step, full_dump);
    for (size_t i = 0; i < c.device.size(); ++i) {
        const auto& range = c.data.ranges[i];
        backend::memcpy_sync(host.get() + range.offset, c.device[i], range.bytes,
                             backend::MemcpyKind::DeviceToHost);
        check_device_copy();
    }
    lock.unlock();
    c.data.host = std::move(host);
    try {
        auto pending = std::async(std::launch::async, [snapshot = std::move(c.data), root = fs::absolute(output_dir),
                          background_export_height, basename, y_up, region_camera_hash,
                          writer_gate = g_checkpoint_writer_gate]() mutable {
            const auto data = std::move(snapshot);
            if (writer_gate) writer_gate();
            write_checkpoint(data, nullptr, root, background_export_height,
                             basename, y_up, region_camera_hash);
        });
        return {std::move(pending), "snapshot"};
    } catch (const std::system_error& e) {
        if (e.code() != std::errc::resource_unavailable_try_again) throw;
        return {{}, "worker_unavailable"};
    }
} catch (const std::bad_alloc&) {
    return {{}, "allocation_failed"};
}

void engine_set_checkpoint_writer_gate(std::function<void()> gate) {
    g_checkpoint_writer_gate = std::move(gate);
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
    }

    backend::device_synchronize();
    return step;
}
