// A viewer slot's packed SH (engine_scene_set_data_3dgs' sh_value_bits)
// renders like its fp32 rows, and the shared encoder keeps the codec's bounds.
#include "engine/Engine.h"
#include "engine/EngineInternal.h"
#include "engine/EngineState.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}

TorchTensorView view(std::vector<float>& d, std::vector<int64_t> s) {
    return {(uint64_t)d.data(), 4, std::move(s)};
}

constexpr int64_t kN = 20000;
constexpr int kDeg = 3;
constexpr int kK = (kDeg + 1) * (kDeg + 1) - 1;
constexpr int kW = 256, kH = 192;

struct Model {
    std::vector<float> means, quats, scales, op, dc, sh;
};

Model make_model(uint32_t seed) {
    Model m;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> u(-1.f, 1.f);
    std::normal_distribution<float> g(0.f, 0.3f);
    m.means.resize(kN * 3);
    m.quats.assign(kN * 4, 0.f);
    m.scales.resize(kN * 3);
    m.op.resize(kN);
    m.dc.resize(kN * 3);
    m.sh.resize(kN * kK * 3);
    for (int64_t i = 0; i < kN; i++) {
        for (int a = 0; a < 3; a++) m.means[3 * i + a] = u(rng);
        m.quats[4 * i] = 1;
        for (int a = 0; a < 3; a++) m.scales[3 * i + a] = -4.0f + 0.5f * u(rng);
        m.op[i] = u(rng);
        for (int a = 0; a < 3; a++) m.dc[3 * i + a] = u(rng);
    }
    for (auto& v : m.sh) v = g(rng);
    return m;
}

void upload(int slot, Model& m, int bits) {
    engine_scene_set_data_3dgs(slot, kN, view(m.means, {kN, 3}), view(m.quats, {kN, 4}),
                               view(m.scales, {kN, 3}), view(m.op, {kN, 1}),
                               view(m.dc, {kN, 3}), view(m.sh, {kN, kK, 3}), 0, bits);
}

std::vector<float> render(int slot) {
    std::vector<float> vm{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 3, 0, 0, 0, 1};
    std::vector<float> intr{200, 200, kW / 2.f, kH / 2.f}, dist(8, 0.f);
    std::vector<float> rgb((size_t)kW * kH * 3), depth((size_t)kW * kH), ts((size_t)kW * kH);
    engine_scene_activate(slot);
    set_camera_params(kW, kH, "PINHOLE", "NONE", view(vm, {1, 4, 4}), view(intr, {1, 4}),
                      view(dist, {1, 8}));
    forward_3dgs("3dgs", kDeg, true, false, 0);
    engine_copy_render_to_host(view(rgb, {1, kH, kW, 3}), view(depth, {1, kH, kW, 1}),
                               view(ts, {1, kH, kW, 1}), {}, {});
    return rgb;
}

double max_diff(const std::vector<float>& a, const std::vector<float>& b) {
    double m = 0;
    for (size_t i = 0; i < a.size(); i++) m = std::max(m, (double)std::fabs(a[i] - b[i]));
    return m;
}

size_t slot_bytes(const std::string& key) {
    for (auto& [k, used, cap] : engine_get_pool_breakdown())
        if (k == key) return used;
    return 0;
}

template <int BITS>
void check_encoder(int64_t n, int k, bool fpbo) {
    std::mt19937 rng(11 + (uint32_t)n);
    std::normal_distribution<float> g(0.f, 0.5f);
    std::vector<float> rows((size_t)(n * k * 3));
    for (auto& v : rows) v = g(rng);
    const ShQuantAddr a = sh_quant_addr((uint32_t)k, fpbo ? 0 : 256);
    QuantizedTensor<BITS, 256> q;
    q.n_cells = fpbo ? sh_fpbo_cells(n, (uint32_t)k) : n * 3 * k;
    q.n_bounds = fpbo ? (n + 255) / 256 : (q.n_cells + 255) / 256;
    q.packed.resize_dynamic(VramCategory::Other, "enc_test.q",
                            q.n_cells * QuantizedTensor<BITS, 256>::kBytesPerCell);
    q.bounds.resize_dynamic(VramCategory::Other, "enc_test.qb", q.n_bounds);
    encode_sh_rows(q, rows.data(), n, k, fpbo);
    std::vector<uint8_t> packed((size_t)q.packed_bytes());
    std::vector<float2> bounds((size_t)q.n_bounds);
    backend::memcpy_sync(packed.data(), q.packed_ptr(), packed.size(),
                         backend::MemcpyKind::DeviceToHost);
    backend::memcpy_sync(bounds.data(), q.bounds_ptr(), bounds.size() * sizeof(float2),
                         backend::MemcpyKind::DeviceToHost);
    DevicePool::global().free_dynamic_prefix("enc_test.");
    std::vector<float2> want(bounds.size(), float2{1e30f, -1e30f});
    const int64_t r = 3 * (int64_t)k;
    for (int64_t i = 0; i < n; i++)
        for (int c = 0; c < r; c++) {
            const int64_t cell = a.cell(a.base(i), c);
            float2& w = want[(size_t)(cell / a.bounds_stride)];
            w.x = std::min(w.x, rows[i * r + c]);
            w.y = std::max(w.y, rows[i * r + c]);
        }
    for (int64_t i = 0; i < n; i++)
        for (int c = 0; c < r; c++) {
            const int64_t cell = a.cell(a.base(i), c);
            const float2 mm = bounds[(size_t)(cell / a.bounds_stride)];
            const float2 w = want[(size_t)(cell / a.bounds_stride)];
            require(mm.x == w.x && mm.y == w.y, "encoder bounds are not the block's min/max");
            const float v = QuantizedTensor<BITS, 256>::decode_v(packed.data(), cell, mm);
            const float half_step = 0.5f * (mm.y - mm.x) / QuantizedTensor<BITS, 256>::kQMax;
            const float ulps = 8 * FLT_EPSILON * std::max(1.0f, std::fabs(v));
            require(std::fabs(v - rows[i * r + c]) <= half_step + ulps,
                    "encoded cell is off by more than half a step");
        }
}
}  // namespace

int main() {
    try {
        for (bool fpbo : {false, true})
            for (int64_t n : {1, 300, 70001})
                for (int k : {3, 11}) {
                    check_encoder<8>(n, k, fpbo);
                    check_encoder<16>(n, k, fpbo);
                }

        engine_reset();
        Model m = make_model(1);
        upload(0, m, 32);
        upload(1, m, 16);
        upload(2, m, 8);
        upload(3, m, 0);
        const size_t cells = (size_t)kN * 3 * kK;
        require(slot_bytes("scene0.features_sh") == cells * 4 &&
                    slot_bytes("scene0.features_sh.q") == 0,
                "32-bit slot did not keep fp32 rows");
        require(slot_bytes("scene1.features_sh") == 0 &&
                    slot_bytes("scene1.features_sh.q") == cells * 2,
                "16-bit slot is not packed at 2 bytes a cell");
        require(slot_bytes("scene2.features_sh.q") == cells, "8-bit slot is not packed");
        require(slot_bytes("scene3.features_sh.q") == cells * 2,
                "a small model did not get 16-bit SH by default");

        const auto ref = render(0);
        const auto r16 = render(1);
        require(engine().world.sh_value_bits == 16, "activating slot 1 did not bind its packed SH");
        const auto r8 = render(2);
        const double d16 = max_diff(r16, ref), d8 = max_diff(r8, ref);
        std::printf("max |rgb - fp32|: 16-bit %.3g, 8-bit %.3g\n", d16, d8);
        require(d16 < 1e-4, "16-bit SH renders differently from fp32");
        require(d8 > 0 && d8 < 2.0f / 255.0f, "8-bit SH error is out of range");
        require(max_diff(render(0), ref) == 0, "fp32 slot changed after the others rendered");

        // A streamed upload, in runs that only start on a multiple of 256, is
        // the one-shot upload.
        for (int bits : {32, 16, 8}) {
            upload(5, m, bits);
            engine_scene_begin_3dgs(6, kN, kK, 0, bits);
            for (int64_t first = 0; first < kN; first += 256 * 7) {
                const int64_t n = std::min<int64_t>(256 * 7, kN - first);
                auto run = [&](std::vector<float>& v, int64_t w) {
                    return TorchTensorView{(uint64_t)(uintptr_t)(v.data() + first * w), 4,
                                           {n, w}};
                };
                require(!engine_scene_loaded(6), "a slot activated before its last row");
                engine_scene_upload_3dgs(6, first, run(m.means, 3), run(m.quats, 4),
                                         run(m.scales, 3), run(m.op, 1), run(m.dc, 3),
                                         TorchTensorView{(uint64_t)(uintptr_t)(m.sh.data() +
                                                                               first * kK * 3),
                                                         4, {n, kK, 3}});
            }
            require(max_diff(render(5), render(6)) == 0,
                    "a streamed " + std::to_string(bits) + "-bit upload renders differently");
        }

        // In-place SH update re-encodes into the packed store.
        Model m2 = make_model(2);
        m2.means = m.means;
        m2.quats = m.quats;
        m2.scales = m.scales;
        m2.op = m.op;
        m2.dc = m.dc;
        engine_scene_update(1, "features_sh", view(m2.sh, {kN, kK, 3}));
        upload(4, m2, 16);
        require(max_diff(render(1), render(4)) == 0, "features_sh update differs from a fresh upload");
        require(max_diff(render(1), ref) > 1e-3, "features_sh update did not change the render");

        for (int s = 0; s < 7; s++) engine_scene_free(s);
        require(slot_bytes("scene1.features_sh.q") == 0 && slot_bytes("scene1.features_sh.qb") == 0,
                "freeing a slot left its packed SH behind");
        engine_reset();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "scene_sh_storage: %s\n", e.what());
        return 1;
    }
    std::puts("scene_sh_storage: OK");
}
