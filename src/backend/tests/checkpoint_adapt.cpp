// CPU checkpoint-layout conversion and payload-boundary checks.

#include "checkpoint/Adapt.h"

#include "core/CheckpointIO.h"
#include "core/Tensor.h"
#include "data/Json.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_fail = 0;
#define SS_CHECK(cond, ...)                                                \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::fprintf(stderr, "  FAIL: " __VA_ARGS__);                  \
            std::fprintf(stderr, "\n");                                    \
            ++g_fail;                                                      \
        }                                                                  \
    } while (0)

constexpr int kK = 15;  // SH3 REST: DC is stored separately.
constexpr int kCells = 3 * kK;
constexpr int64_t kBlock = 256;
constexpr float kQ8 = 255.0f;
constexpr float kQ16 = 65535.0f;

constexpr int kValue8 = 8;
constexpr int kValue16 = 16;

struct Member {
    std::string name;          // tar base name, without the ".npy" suffix
    std::string descr;
    std::vector<uint8_t> bytes;
};

size_t elem_size(const std::string& d) { return (size_t)(d[2] - '0'); }

ShQuantAddr addr_of(bool fpbo, int K) {
    return sh_quant_addr((uint32_t)K, fpbo ? 0 : 256);
}

int64_t cell_count(bool fpbo, int64_t rows, int K) {
    return fpbo ? sh_fpbo_cells(rows, (uint32_t)K) : rows * 3 * (int64_t)K;
}

int64_t bounds_stride_of(bool fpbo, int K) {
    return fpbo ? addr_of(true, K).bounds_stride : kBlock;
}

// FPBO addresses whole 256-splat blocks, so its row extent is block-rounded.
int64_t filled_rows(bool fpbo, int64_t rows) {
    return fpbo ? ((rows + kBlock - 1) / kBlock) * kBlock : rows;
}

int64_t num_blocks(int64_t n, int64_t bbc) { return (n + bbc - 1) / bbc; }

int64_t value_bpc(int bits) { return bits == kValue16 ? 2 : 1; }


// ===========================================================================
// Synthetic checkpoint I/O
// ===========================================================================

void write_state_tar(const fs::path& path, const std::string& state_json,
                     const std::vector<Member>& members) {
    std::ofstream out(path.string(), std::ios::binary);
    if (!out) throw std::runtime_error("cannot write " + path.string());
    ckpt::tar_write_bytes(out, "state.json", state_json.data(), state_json.size());
    for (const Member& m : members) {
        std::string hdr = ckpt::npy_header(
            m.descr.c_str(), m.bytes.size() / elem_size(m.descr));
        size_t member = hdr.size() + m.bytes.size();
        ckpt::tar_header(out, m.name + ".npy", member);
        out.write(hdr.data(), (std::streamsize)hdr.size());
        if (!m.bytes.empty())
            out.write((const char*)m.bytes.data(),
                      (std::streamsize)m.bytes.size());
        ckpt::tar_pad(out, member);
    }
    ckpt::tar_finish(out);
}

std::map<std::string, Member> read_state_tar(const fs::path& path) {
    std::ifstream in(path.string(), std::ios::binary);
    std::map<std::string, Member> out;
    for (const auto& tm : ckpt::tar_index(in)) {
        if (tm.name.size() < 4 ||
            tm.name.compare(tm.name.size() - 4, 4, ".npy") != 0) continue;
        ckpt::NpyInfo info = ckpt::npy_locate(in, tm.data_offset, tm.size);
        Member m;
        m.descr = info.descr;
        m.bytes.resize((size_t)info.data_bytes);
        in.clear();
        in.seekg((std::streamoff)info.data_offset, std::ios::beg);
        in.read((char*)m.bytes.data(), (std::streamsize)info.data_bytes);
        out.emplace(tm.name.substr(0, tm.name.size() - 4), std::move(m));
    }
    return out;
}

std::string read_state_json(const fs::path& path) {
    std::ifstream in(path.string(), std::ios::binary);
    for (const auto& m : ckpt::tar_index(in))
        if (m.name == "state.json") return ckpt::tar_read_member(in, m);
    return {};
}

std::string state_json(int64_t cur, int64_t maxn, int K, bool fused,
                       int value_bits) {
    std::string s = "{\n";
    s += "  \"format_version\": 1,\n";
    s += "  \"full_resume\": 1,\n";
    s += "  \"cur_num_splats\": " + std::to_string(cur) + ",\n";
    s += "  \"max_num_splats\": " + std::to_string(maxn) + ",\n";
    s += "  \"num_sh\": " + std::to_string(K) + ",\n";
    s += "  \"sh_degree\": " + std::to_string((int)std::sqrt(K + 1) - 1) + ",\n";
    s += "  \"sh_optim_bits\": 8,\n";
    s += "  \"sh_value_bits\": " + std::to_string(value_bits) + ",\n";
    s += "  \"non_sh_optim_bits\": 32,\n";
    s += "  \"packed\": 1,\n";
    s += "  \"use_fused_proj_bwd_optim\": " + std::to_string(fused ? 1 : 0) + ",\n";
    s += "  \"bilagrid_rgb\": {\"enabled\": 0},\n";
    s += "  \"bilagrid_depth\": {\"enabled\": 0},\n";
    s += "  \"bilagrid_normal\": {\"enabled\": 0},\n";
    s += "  \"ppisp\": {\"enabled\": 0}\n";
    s += "}\n";
    return s;
}


// ===========================================================================
// Payload builders, through the engine's own host codecs
// ===========================================================================

// A scattered signal covering every cell; strictly positive so a zeroed or
// dropped payload cannot pass, and varying so each block has a real range.
std::vector<float> value_cells(bool fpbo, int64_t rows) {
    const ShQuantAddr a = addr_of(fpbo, kK);
    std::vector<float> v((size_t)cell_count(fpbo, rows, kK), 0.0f);
    for (int64_t r = 0; r < filled_rows(fpbo, rows); r++)
        for (int c = 0; c < kCells; c++)
            v[(size_t)a.cell(a.base(r), c)] =
                0.25f + 0.5f * (float)((r * 7 + c) % 17) / 17.0f;
    return v;
}

void encode_value(int bits, const std::vector<float>& v, int64_t bbc,
                  std::vector<uint8_t>& pk, std::vector<float>& bnd) {
    const int64_t n = (int64_t)v.size();
    const int64_t nb = num_blocks(n, bbc);
    bnd.assign((size_t)(nb * 2), 0.0f);
    for (int64_t b = 0; b < nb; b++) {
        int64_t lo = b * bbc, hi = std::min(n, lo + bbc);
        float mn = v[(size_t)lo], mx = mn;
        for (int64_t i = lo + 1; i < hi; i++) {
            mn = std::min(mn, v[(size_t)i]);
            mx = std::max(mx, v[(size_t)i]);
        }
        bnd[(size_t)(b * 2 + 0)] = mn;
        bnd[(size_t)(b * 2 + 1)] = mx;
    }
    pk.assign((size_t)(n * value_bpc(bits)), 0);
    for (int64_t i = 0; i < n; i++) {
        int64_t b = i / bbc;
        float2 mm{bnd[(size_t)(b * 2)], bnd[(size_t)(b * 2 + 1)]};
        if (bits == kValue16)
            QuantizedTensor<16>::encode_v(pk.data(), i, v[(size_t)i], mm);
        else
            QuantizedTensor<8>::encode_v(pk.data(), i, v[(size_t)i], mm);
    }
}

// Two moment levels alternating per splat, so every block of either layout
// sees the same (min, max) pair and the codec round-trips them exactly.
void adam_moments(int64_t row, float& g1, float& g2) {
    g1 = (row & 1) ? -0.25f : 0.25f;
    g2 = (row & 1) ?  2.25f : 1.0f;
}

void encode_adam(const std::vector<float>& g1, const std::vector<float>& g2,
                 int64_t bbc, std::vector<uint8_t>& pk, std::vector<float>& bnd) {
    const int64_t n = (int64_t)g1.size();
    std::vector<float> u((size_t)n), s((size_t)n);
    for (int64_t i = 0; i < n; i++) {
        float2 us = QuantizedAdamState<8>::g1g2_to_us(g1[(size_t)i], g2[(size_t)i]);
        u[(size_t)i] = us.x;
        s[(size_t)i] = us.y;
    }
    const int64_t nb = num_blocks(n, bbc);
    bnd.assign((size_t)(nb * 4), 0.0f);
    for (int64_t b = 0; b < nb; b++) {
        int64_t lo = b * bbc, hi = std::min(n, lo + bbc);
        float umn = u[(size_t)lo], umx = umn, smn = s[(size_t)lo], smx = smn;
        for (int64_t i = lo + 1; i < hi; i++) {
            umn = std::min(umn, u[(size_t)i]); umx = std::max(umx, u[(size_t)i]);
            smn = std::min(smn, s[(size_t)i]); smx = std::max(smx, s[(size_t)i]);
        }
        bnd[(size_t)(b * 4 + 0)] = umn; bnd[(size_t)(b * 4 + 1)] = umx;
        bnd[(size_t)(b * 4 + 2)] = smn; bnd[(size_t)(b * 4 + 3)] = smx;
    }
    pk.assign((size_t)(n * 2), 0);
    for (int64_t i = 0; i < n; i++) {
        int64_t b = i / bbc;
        float4 mm{bnd[(size_t)(b * 4 + 0)], bnd[(size_t)(b * 4 + 1)],
                  bnd[(size_t)(b * 4 + 2)], bnd[(size_t)(b * 4 + 3)]};
        QuantizedAdamState<8>::encode_us(pk.data(), i, u[(size_t)i],
                                        s[(size_t)i], mm);
    }
}


// ===========================================================================
// Decoders + bounds introspection
// ===========================================================================

const float* f32(const Member& m) { return (const float*)m.bytes.data(); }

float decode_value(int bits, const Member& pk, const Member& bnd, int64_t cell,
                   int64_t stride) {
    int64_t b = cell / stride;
    float2 mm{f32(bnd)[b * 2], f32(bnd)[b * 2 + 1]};
    return bits == kValue16 ? QuantizedTensor<16>::decode_v(pk.bytes.data(), cell, mm)
                            : QuantizedTensor<8>::decode_v(pk.bytes.data(), cell, mm);
}

void decode_adam(const Member& pk, const Member& bnd, int64_t cell,
                 int64_t stride, float2& us, float& g1, float& g2) {
    int64_t b = cell / stride;
    float4 mm{f32(bnd)[b * 4 + 0], f32(bnd)[b * 4 + 1],
              f32(bnd)[b * 4 + 2], f32(bnd)[b * 4 + 3]};
    us = QuantizedAdamState<8>::decode_us(pk.bytes.data(), cell, mm);
    float2 gg = QuantizedAdamState<8>::decode_g1g2(pk.bytes.data(), cell, mm);
    g1 = gg.x;
    g2 = gg.y;
}

// Widest (max - min) of the primitive in lanes `lo, lo+1` across a bounds
// table. Turns a codec quantum into an absolute error bound.
float widest_range(const Member& bnd, int lanes, int lo) {
    float widest = 0.0f;
    const int64_t nb = (int64_t)bnd.bytes.size() / (size_t)(lanes * sizeof(float));
    for (int64_t b = 0; b < nb; b++) {
        const float* p = f32(bnd) + b * lanes;
        widest = std::max(widest, p[lo + 1] - p[lo]);
    }
    return widest;
}


// ===========================================================================
// Scenario
// ===========================================================================

struct Case {
    const char* name;
    bool    src_fpbo;
    bool    dst_fpbo;
    int     value_bits;
    int64_t cur;
    int64_t max_ck;
    int64_t max_new;
};

std::string value_slot(bool fpbo, int bits) {
    return std::string("eng.world.sh_vq") + (bits == kValue16 ? "16" : "8") +
           (fpbo ? "_fpbo" : "");
}
std::string adam_slot(bool fpbo) {
    return fpbo ? "eng.sh_quant_fpbo" : "eng.sh_quant";
}

std::vector<Member> source_members(const Case& c) {
    std::vector<Member> out;

    // state.tar contains full logical slots, not PLY's live-row staging.
    std::vector<float> v = value_cells(c.src_fpbo, c.max_ck);
    std::vector<uint8_t> vpk; std::vector<float> vbd;
    encode_value(c.value_bits, v, bounds_stride_of(c.src_fpbo, kK), vpk, vbd);

    const ShQuantAddr a = addr_of(c.src_fpbo, kK);
    const int64_t adam_cells = cell_count(c.src_fpbo, c.max_ck, kK);
    std::vector<float> g1((size_t)adam_cells), g2((size_t)adam_cells);
    for (int64_t r = 0; r < filled_rows(c.src_fpbo, c.max_ck); r++) {
        float m1, m2;
        adam_moments(r, m1, m2);
        for (int cc = 0; cc < kCells; cc++) {
            int64_t cell = a.cell(a.base(r), cc);
            g1[(size_t)cell] = m1;
            g2[(size_t)cell] = m2;
        }
    }
    std::vector<uint8_t> apk; std::vector<float> abd;
    encode_adam(g1, g2, bounds_stride_of(c.src_fpbo, kK), apk, abd);

    // Capacity-only retargeting keeps live row indices unchanged.
    std::vector<float> opac((size_t)c.max_ck);
    for (int64_t i = 0; i < c.max_ck; i++)
        opac[(size_t)i] = 0.1f + 0.8f * (float)i / (float)c.max_ck;

    auto add = [&out](const std::string& n, const char* descr,
                      const void* p, size_t bytes) {
        Member m; m.name = n; m.descr = descr;
        m.bytes.assign((const uint8_t*)p, (const uint8_t*)p + bytes);
        out.push_back(std::move(m));
    };
    add("world.opacities", "<f4", opac.data(), opac.size() * sizeof(float));
    add(value_slot(c.src_fpbo, c.value_bits) + ".q", "|u1", vpk.data(), vpk.size());
    add(value_slot(c.src_fpbo, c.value_bits) + ".qb", "<f4",
        vbd.data(), vbd.size() * sizeof(float));
    add(adam_slot(c.src_fpbo) + ".q", "|u1", apk.data(), apk.size());
    add(adam_slot(c.src_fpbo) + ".qb", "<f4", abd.data(), abd.size() * sizeof(float));
    return out;
}

const Member* need(const std::map<std::string, Member>& m, const std::string& n,
                   const char* case_name, const char* which) {
    auto it = m.find(n);
    SS_CHECK(it != m.end(), "%s: %s member '%s' missing", case_name, which, n.c_str());
    return it == m.end() ? nullptr : &it->second;
}

void compare_semantics(const Case& c, const std::map<std::string, Member>& src,
                       const std::map<std::string, Member>& dst) {
    const std::string vs = value_slot(c.src_fpbo, c.value_bits);
    const std::string vd = value_slot(c.dst_fpbo, c.value_bits);
    const std::string as = adam_slot(c.src_fpbo);
    const std::string ad = adam_slot(c.dst_fpbo);

    const Member* sq = need(src, vs + ".q",  c.name, "source");
    const Member* sb = need(src, vs + ".qb", c.name, "source");
    const Member* dq = need(dst, vd + ".q",  c.name, "target");
    const Member* db = need(dst, vd + ".qb", c.name, "target");
    const Member* aq = need(src, as + ".q",  c.name, "source");
    const Member* ab = need(src, as + ".qb", c.name, "source");
    const Member* cq = need(dst, ad + ".q",  c.name, "target");
    const Member* cb = need(dst, ad + ".qb", c.name, "target");
    if (!sq || !sb || !dq || !db || !aq || !ab || !cq || !cb) return;

    // The engine loader requires exact logical slot sizes.
    const int64_t tgt_cells = cell_count(c.dst_fpbo, c.max_new, kK);
    const int64_t tgt_bbc = bounds_stride_of(c.dst_fpbo, kK);
    const int64_t tgt_bounds = num_blocks(tgt_cells, tgt_bbc);
    const int64_t vbytes = tgt_cells * value_bpc(c.value_bits);
    const int failures_before_sizes = g_fail;
    SS_CHECK((int64_t)dq->bytes.size() == vbytes,
          "%s: value .q is %lld bytes, expected %lld", c.name,
          (long long)dq->bytes.size(), (long long)vbytes);
    SS_CHECK((int64_t)db->bytes.size() == tgt_bounds * 8,
          "%s: value .qb is %lld bytes, expected %lld", c.name,
          (long long)db->bytes.size(), (long long)(tgt_bounds * 8));
    SS_CHECK((int64_t)cq->bytes.size() == tgt_cells * 2,
          "%s: adam .q is %lld bytes, expected %lld", c.name,
          (long long)cq->bytes.size(), (long long)(tgt_cells * 2));
    SS_CHECK((int64_t)cb->bytes.size() == tgt_bounds * 16,
          "%s: adam .qb is %lld bytes, expected %lld", c.name,
          (long long)cb->bytes.size(), (long long)(tgt_bounds * 16));
    if (g_fail != failures_before_sizes) return;
    SS_CHECK(dst.count(vs + ".q") == 0 && dst.count(as + ".q") == 0,
          "%s: obsolete source-layout slots survived", c.name);

    const ShQuantAddr sa = addr_of(c.src_fpbo, kK);
    const ShQuantAddr da = addr_of(c.dst_fpbo, kK);
    const int64_t sbbc = bounds_stride_of(c.src_fpbo, kK);
    const int64_t dbbc = bounds_stride_of(c.dst_fpbo, kK);

    // Re-encoding may move a value by one target codec quantum.
    const float vqmax = c.value_bits == kValue16 ? kQ16 : kQ8;
    const float vtol = std::max(widest_range(*sb, 2, 0),
                                widest_range(*db, 2, 0)) / vqmax + 1e-5f;
    const float eps_u = std::max(widest_range(*ab, 4, 0),
                                 widest_range(*cb, 4, 0)) / kQ8;
    const float eps_s = std::max(widest_range(*ab, 4, 2),
                                 widest_range(*cb, 4, 2)) / kQ8;
    const float utol = eps_u + 1e-6f;
    const float stol = eps_s + 1e-6f;

    const int64_t live_rows = std::min(c.cur, c.max_new);

    float worst_value = 0.0f, worst_u = 0.0f, worst_s = 0.0f;
    float max_sqrt = 0.0f, max_abs_u = 0.0f, worst_g1 = 0.0f, worst_g2 = 0.0f;
    int64_t nz_value = 0, nz_moments = 0;
    for (int64_t r = 0; r < live_rows; r++) {
        for (int cc = 0; cc < kCells; cc++) {
            const int64_t sc = sa.cell(sa.base(r), cc);
            const int64_t dc = da.cell(da.base(r), cc);

            const float v_s = decode_value(c.value_bits, *sq, *sb, sc, sbbc);
            const float v_d = decode_value(c.value_bits, *dq, *db, dc, dbbc);
            if (!std::isfinite(v_s) || !std::isfinite(v_d)) {
                SS_CHECK(false, "%s: non-finite SH value", c.name);
                return;
            }
            worst_value = std::max(worst_value, std::fabs(v_s - v_d));
            if (v_s != 0.0f) nz_value++;

            float2 us_s, us_d; float g1_s, g2_s, g1_d, g2_d;
            decode_adam(*aq, *ab, sc, sbbc, us_s, g1_s, g2_s);
            decode_adam(*cq, *cb, dc, dbbc, us_d, g1_d, g2_d);
            for (float value : {us_s.x, us_s.y, us_d.x, us_d.y,
                                g1_s, g2_s, g1_d, g2_d}) {
                if (!std::isfinite(value)) {
                    SS_CHECK(false, "%s: non-finite Adam state", c.name);
                    return;
                }
            }
            worst_u = std::max(worst_u, std::fabs(us_s.x - us_d.x));
            worst_s = std::max(worst_s, std::fabs(us_s.y - us_d.y));
            worst_g1 = std::max(worst_g1, std::fabs(g1_s - g1_d));
            worst_g2 = std::max(worst_g2, std::fabs(g2_s - g2_d));

            max_sqrt  = std::max(max_sqrt, std::sqrt(std::max(g2_s, 0.0f)));
            max_abs_u = std::max(max_abs_u, std::fabs(us_s.x));
            if (g2_s > 0.0f) nz_moments++;
        }
    }
    const float sqrt_delta = QuantizedAdamState<8>::inverse_sqrt_g2(
        QuantizedAdamState<8>::forward_sqrt_g2(max_sqrt) + eps_s) - max_sqrt;
    const float tol_g1 = eps_u * (max_sqrt + sqrt_delta + 1e-15f) +
                         max_abs_u * sqrt_delta + 1e-5f;
    const float tol_g2 = sqrt_delta * (2.0f * max_sqrt + sqrt_delta) + 1e-5f;

    const int64_t live = live_rows * kCells;
    SS_CHECK(nz_value == live,
          "%s: source SH values were not all non-zero (%lld/%lld)", c.name,
          (long long)nz_value, (long long)live);
    SS_CHECK(nz_moments == live,
          "%s: source moments were not all non-zero (%lld/%lld)", c.name,
          (long long)nz_moments, (long long)live);
    SS_CHECK(worst_value <= vtol,
          "%s: decoded SH value drift %.6g exceeds codec tolerance %.6g",
          c.name, (double)worst_value, (double)vtol);
    SS_CHECK(worst_u <= utol,
          "%s: decoded Adam u drift %.6g exceeds %.6g", c.name,
          (double)worst_u, (double)utol);
    SS_CHECK(worst_s <= stol,
          "%s: decoded Adam log_s drift %.6g exceeds %.6g", c.name,
          (double)worst_s, (double)stol);
    SS_CHECK(worst_g1 <= tol_g1,
          "%s: decoded g1 drift %.6g exceeds derived bound %.6g", c.name,
          (double)worst_g1, (double)tol_g1);
    SS_CHECK(worst_g2 <= tol_g2,
          "%s: decoded g2 drift %.6g exceeds derived bound %.6g", c.name,
          (double)worst_g2, (double)tol_g2);
}

struct TempDir {
    fs::path path;
    TempDir() {
        const fs::path base = fs::temp_directory_path();
        for (uint64_t attempt = 0;; ++attempt) {
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            path = base / ("spirula_ckpt_adapt_" + std::to_string(stamp) +
                           "_" + std::to_string(attempt));
            if (fs::create_directory(path)) return;
        }
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

ckpt::TargetLayout target_for(const Case& c) {
    ckpt::TargetLayout t;
    t.max_num_splats = c.max_new;
    t.num_sh = kK;
    t.num_images = 1;
    t.fused_proj_bwd_optim = c.dst_fpbo;
    return t;
}

void run_case(const Case& c, bool stale_layout = false) {
    TempDir tmp;
    const fs::path& root = tmp.path;
    fs::create_directories(root / "src");
    fs::create_directories(root / "out");

    std::vector<Member> members = source_members(c);
    if (stale_layout) {
        Case inactive = c;
        inactive.src_fpbo = !c.src_fpbo;
        for (Member& m : source_members(inactive)) {
            if (m.name == "world.opacities") continue;
            std::fill(m.bytes.begin(), m.bytes.end(), 0);
            members.push_back(std::move(m));
        }
    }
    write_state_tar(root / "src" / "state.tar",
                    state_json(c.cur, c.max_ck, kK, c.src_fpbo, c.value_bits),
                    members);

    const ckpt::TargetLayout t = target_for(c);

    const bool wrote = ckpt::adapt_checkpoint(root / "src", t, root / "out");
    SS_CHECK(wrote, "%s: adapt_checkpoint adapted nothing", c.name);
    if (!wrote) return;

    JsonValue os = json_parse(read_state_json(root / "out" / "state.tar"));
    SS_CHECK((int64_t)os.get_double("max_num_splats", -1) == c.max_new,
          "%s: max_num_splats not retargeted", c.name);
    SS_CHECK((int64_t)os.get_double("cur_num_splats", -1) == c.cur,
          "%s: cur_num_splats not retargeted", c.name);
    SS_CHECK((int)os.get_double("num_sh", -1) == kK,
          "%s: num_sh not retargeted", c.name);
    SS_CHECK((os.get_double("use_fused_proj_bwd_optim", -1) != 0) == c.dst_fpbo,
          "%s: saved use_fused_proj_bwd_optim not rewritten to the target",
          c.name);

    compare_semantics(c, read_state_tar(root / "src" / "state.tar"),
                      read_state_tar(root / "out" / "state.tar"));
}


// A truncated unused tail is still an invalid state.tar slot.
void run_short_payload_checks() {
    const Case c{"short", true, false, kValue8, 200, 512, 300};
    const int64_t source_cells = cell_count(c.src_fpbo, c.max_ck, kK);

    struct Cut {
        const char* name;
        std::string member;
        int64_t     keep_bytes;
    };
    const Cut cuts[] = {
        {"value .q",  value_slot(c.src_fpbo, c.value_bits) + ".q",
         source_cells * value_bpc(c.value_bits) - 1},
        {"value .qb", value_slot(c.src_fpbo, c.value_bits) + ".qb", 1},
        {"adam .q",   adam_slot(c.src_fpbo) + ".q", source_cells * 2 - 1},
        {"adam .qb",  adam_slot(c.src_fpbo) + ".qb", 1},
    };

    for (const Cut& cut : cuts) {
        TempDir tmp;
        const fs::path& root = tmp.path;
        fs::create_directories(root / "src");
        fs::create_directories(root / "out");

        std::vector<Member> members = source_members(c);
        bool sliced = false;
        for (Member& m : members) {
            if (m.name != cut.member) continue;
            SS_CHECK(cut.keep_bytes >= 0 &&
                      cut.keep_bytes < (int64_t)m.bytes.size(),
                  "short %s: cut of %lld is not below the %lld-byte payload",
                  cut.name, (long long)cut.keep_bytes,
                  (long long)m.bytes.size());
            m.bytes.resize((size_t)std::max<int64_t>(cut.keep_bytes, 0));
            sliced = true;
        }
        SS_CHECK(sliced, "short %s: member '%s' not present", cut.name,
              cut.member.c_str());

        write_state_tar(root / "src" / "state.tar",
                        state_json(c.cur, c.max_ck, kK, c.src_fpbo, c.value_bits),
                        members);

        bool threw = false;
        try {
            ckpt::adapt_checkpoint(root / "src", target_for(c), root / "out");
        } catch (const std::exception&) {
            threw = true;
        }
        SS_CHECK(threw, "short %s (kept %lld bytes) was not refused", cut.name,
              (long long)cut.keep_bytes);
    }
}

void run_zero_sh_checks() {
    const Case c{"zero SH", true, true, kValue8, 200, 512, 768};
    for (bool empty_source : {true, false}) {
        TempDir tmp;
        fs::create_directories(tmp.path / "src");
        fs::create_directories(tmp.path / "out");
        std::vector<Member> members = source_members(c);
        if (empty_source) {
            members.erase(std::remove_if(members.begin(), members.end(),
                [](const Member& m) { return m.name != "world.opacities"; }),
                members.end());
            members.push_back({value_slot(true, kValue8) + ".qb", "<f4",
                               std::vector<uint8_t>(2 * sizeof(float2))});
            members.push_back({adam_slot(true) + ".qb", "<f4",
                               std::vector<uint8_t>(2 * sizeof(float4))});
        }
        write_state_tar(tmp.path / "src" / "state.tar",
                        state_json(c.cur, c.max_ck, empty_source ? 0 : kK,
                                   true, kValue8), members);
        ckpt::TargetLayout target = target_for(c);
        target.num_sh = empty_source ? kK : 0;
        const bool wrote = ckpt::adapt_checkpoint(
            tmp.path / "src", target, tmp.path / "out");
        SS_CHECK(wrote, "zero SH adaptation was skipped");
        if (!wrote) continue;
        const auto result = read_state_tar(tmp.path / "out" / "state.tar");
        SS_CHECK(result.size() == 1 && result.count("world.opacities") == 1,
                 "zero SH conversion failed to preserve only the non-SH state");
        const auto state = json_parse(read_state_json(tmp.path / "out" / "state.tar"));
        SS_CHECK((int)state.get_double("num_sh", -1) == target.num_sh,
                 "zero SH conversion lost the target degree");
    }
}

}  // namespace

int main() {
    const Case cases[] = {
        {"fpbo->cell q8",  true,  false, kValue8,  200, 512, 200},
        {"cell->fpbo q8",  false, true,  kValue8,  255, 256, 512},
        {"fpbo->cell q16", true,  false, kValue16, 300, 512, 400},
        {"cell->fpbo q16", false, true,  kValue16, 257, 512, 768},
    };
    for (const Case& c : cases) run_case(c);
    run_case({"stale fpbo slots", false, true, kValue16, 257, 512, 768}, true);
    run_short_payload_checks();
    run_zero_sh_checks();
    std::printf("%s\n", g_fail ? "FAILURES" : "checkpoint_adapt ok");
    return g_fail ? 1 : 0;
}
