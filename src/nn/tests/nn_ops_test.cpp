// GPU op library vs. straightforward CPU references.
//
// Every kernel in src/nn/shaders is exercised here against an independent scalar
// implementation of the same math. This is the gate that lets the model layer
// treat nn::ops as trustworthy: when a mask comes out wrong, the question is
// which module misread the reference, not whether the matmul is broken.

#include "nn/core/Error.h"
#include "nn/core/Half.h"
#include "nn/core/Log.h"
#include "nn/core/Parallel.h"
#include "nn/Ops.h"
#include "nn/vk/Context.h"
#include "nn/vk/MetalInterop.h"
#include "nn/vk/Stream.h"
#include "nn/vk/Transfer.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <stdexcept>
#include <string>
#include <string>
#include <vector>

using namespace nn;

namespace {

int g_failures = 0;
int g_checks = 0;

std::mt19937 g_rng(12345);

std::vector<float> randn(size_t n, float scale = 1.0f) {
    std::normal_distribution<float> d(0.0f, scale);
    std::vector<float> v(n);
    for (auto& x : v) x = d(g_rng);
    return v;
}

// Error relative to max(|want[i]|, a fraction of the tensor's RMS).
//
// A bare relative error is meaningless for an element that happens to land near
// zero -- a GEMM row summing 256 terms to 1e-6 has full fp32 cancellation error
// and no digits left. Scaling the floor by the tensor's own magnitude asks the
// question that matters: is the error small compared to what this tensor
// carries?
void check(const char* name, const std::vector<float>& got, const std::vector<float>& want,
           float tol) {
    ++g_checks;
    if (got.size() != want.size()) {
        std::printf("  FAIL %-28s size %zu != %zu\n", name, got.size(), want.size());
        ++g_failures;
        return;
    }
    double sq = 0.0;
    for (float v : want) sq += (double)v * v;
    const double rms = std::sqrt(sq / std::max<size_t>(want.size(), 1));
    const double floor = std::max(1e-6, 0.05 * rms);

    double worst = 0.0;
    size_t worst_i = 0;
    for (size_t i = 0; i < got.size(); ++i) {
        double denom = std::max(floor, (double)std::fabs(want[i]));
        double e = std::fabs((double)got[i] - (double)want[i]) / denom;
        if (e > worst) { worst = e; worst_i = i; }
    }
    if (worst > tol) {
        std::printf("  FAIL %-28s rel err %.3g at [%zu] (got %.6f want %.6f)\n", name,
                    worst, worst_i, got[worst_i], want[worst_i]);
        ++g_failures;
    } else {
        std::printf("  ok   %-28s rel err %.2g\n", name, worst);
    }
}

Tensor upload_f32(vk::Arena& a, const std::vector<float>& v, int64_t d0, int64_t d1 = 1,
                  int64_t d2 = 1) {
    Tensor t = arena_tensor(a, DType::F32, d0, d1, d2);
    tensor_from_host(t, v.data(), (int64_t)v.size());
    return t;
}

Tensor upload_f16(vk::Arena& a, const std::vector<float>& v, int64_t d0, int64_t d1 = 1) {
    Tensor t = arena_tensor(a, DType::F16, d0, d1);
    tensor_from_host(t, v.data(), (int64_t)v.size());
    return t;
}

std::vector<float> readback(const Tensor& t) {
    std::vector<float> v((size_t)t.numel());
    tensor_to_host(t, v.data(), t.numel());
    return v;
}

// Rounded the way the tensor-core kernels round: their operands are fp16, so
// the reference has to be, or the check measures fp16's dynamic range instead
// of the kernel. Same convention as the GEMM's coop case.
std::vector<float> round_f16(const std::vector<float>& v) {
    std::vector<float> out(v.size());
    for (size_t i = 0; i < v.size(); ++i) out[i] = half_to_float(float_to_half(v[i]));
    return out;
}

// Every attention case is checked on both paths where the device offers both:
// the scalar kernel against an exact reference, the cooperative-matrix one --
// which only takes head dims that are a multiple of 16 -- against the rounded
// one. Running both in one process is what `set_coop_matrix_enabled` is for.
bool coop_attn(int head_dim) {
    return vk::Context::get().hasCoopMat() && head_dim % 16 == 0;
}

float act_ref(float x, Act a) {
    switch (a) {
        case Act::Relu: return std::fmax(x, 0.0f);
        case Act::GeluErf: return 0.5f * x * (1.0f + std::erf(x * 0.70710678118f));
        case Act::GeluTanh: {
            float inner = 0.7978845608028654f * (x + 0.044715f * x * x * x);
            return 0.5f * x * (1.0f + std::tanh(inner));
        }
        case Act::Sigmoid: return 1.0f / (1.0f + std::exp(-x));
        default: return x;
    }
}

// ================
// Pointer span
// ================

// Each launcher that splits a tall tensor is run twice, once with the span
// lowered far enough to force several dispatches. The split is on whole rows,
// so the two runs must agree bit for bit; the math itself is covered above.
void test_span(vk::Arena& arena) {
    const int64_t rows = 300, cols = 7;
    const int64_t row_span = 32 * cols * 4;   // 32 rows of the widest operand

    auto run = [&](const char* name, int64_t span, auto&& body) {
        vk::ArenaScope scope(arena);
        std::vector<float> want = body();
        set_max_span_bytes(span);
        std::vector<float> got = body();
        set_max_span_bytes(0);
        check(name, got, want, 0.0f);
    };
    auto note = [&](const char* name, bool ok, const char* why) {
        ++g_checks;
        if (ok) std::printf("  ok   %-28s\n", name);
        else { std::printf("  FAIL %-28s %s\n", name, why); ++g_failures; }
    };

    set_max_span_bytes(row_span);
    const bool splits = span_rows("test_span", cols * 4) < rows;
    set_max_span_bytes(0);
    note("span forces a split", splits, "the sizes below no longer split");

    const auto x = randn((size_t)rows * cols);
    const auto y = randn((size_t)rows * cols);
    const auto v = randn((size_t)cols);
    const auto w = randn((size_t)cols * cols, 0.1f);

    run("unary split", row_span, [&] {
        Tensor to = arena_tensor(arena, DType::F32, rows, cols);
        unary(to, upload_f32(arena, x, rows, cols), Act::Selu, 0.5f, 0.25f);
        return readback(to);
    });
    run("add split", row_span, [&] {
        Tensor to = arena_tensor(arena, DType::F32, rows, cols);
        add(to, upload_f32(arena, x, rows, cols), upload_f32(arena, y, rows, cols));
        return readback(to);
    });
    run("add per-column split", row_span, [&] {
        Tensor to = arena_tensor(arena, DType::F32, rows, cols);
        add(to, upload_f32(arena, x, rows, cols), upload_f32(arena, v, cols));
        return readback(to);
    });
    run("convert split", row_span, [&] {
        Tensor half = arena_tensor(arena, DType::F16, rows, cols);
        Tensor to = arena_tensor(arena, DType::F32, rows, cols);
        copy(half, upload_f32(arena, x, rows, cols));
        copy(to, half);
        return readback(to);
    });
    // A destination stride wider than the source is ALIKED's aggregation write.
    run("strided_copy split", row_span, [&] {
        Tensor to = arena_tensor(arena, DType::F32, rows, cols * 2);
        fill(to, 0.0f);
        strided_copy(to, upload_f32(arena, x, rows, cols), rows, cols, cols, cols * 2);
        return readback(to);
    });
    run("l2_normalize_rows split", row_span, [&] {
        Tensor to = arena_tensor(arena, DType::F32, rows, cols);
        l2_normalize_rows(to, upload_f32(arena, x, rows, cols));
        return readback(to);
    });
    run("softmax_rows split", row_span, [&] {
        Tensor to = arena_tensor(arena, DType::F32, rows, cols);
        softmax_rows(to, upload_f32(arena, x, rows, cols));
        return readback(to);
    });
    run("layer_norm split", row_span, [&] {
        Tensor to = arena_tensor(arena, DType::F32, rows, cols);
        layer_norm(to, upload_f32(arena, x, rows, cols), upload_f32(arena, v, cols),
                   upload_f32(arena, v, cols), 1e-5f, upload_f32(arena, y, rows, cols));
        return readback(to);
    });
    // The GEMM splits in whole row tiles, so its span is measured in tiles: this
    // is two 64-row tiles or one 128-row one, three or more calls either way.
    run("gemm split", 128 * cols * 4, [&] {
        Tensor to = arena_tensor(arena, DType::F32, rows, cols);
        linear(to, upload_f32(arena, x, rows, cols), upload_f32(arena, w, cols, cols));
        return readback(to);
    });

    {   // An op whose reads are not row-local takes the 64-bit kernel instead,
        // and says so rather than wrapping when the device cannot run one.
        vk::ArenaScope scope(arena);
        const int64_t H = 40, W = 6, C = 3;
        auto src = randn((size_t)H * W * C);
        Tensor in = upload_f32(arena, src, H, W, C);
        Tensor to = arena_tensor(arena, DType::F32, H / 2, W, C);

        set_max_span_bytes(64);
        set_wide_index(WideIndex::Off);
        std::string refused;
        try {
            resize_bilinear(to, in);
        } catch (const nn::Error& e) {
            refused = e.what();
        }
        set_wide_index(WideIndex::Auto);
        set_max_span_bytes(0);
        note("no wide kernel refuses",
             refused.find("shaderInt64") != std::string::npos,
             refused.empty() ? "resize_bilinear wrapped silently" : refused.c_str());

        run("resize_bilinear wide", 64, [&] {
            Tensor o = arena_tensor(arena, DType::F32, H / 2, W, C);
            resize_bilinear(o, upload_f32(arena, src, H, W, C));
            return readback(o);
        });
    }
}

// ================
// GEMM
// ================

void test_gemm(vk::Arena& arena) {
    struct Case { int M, N, K; const char* name; };
    const Case cases[] = {
        {1, 256, 256, "gemm 1x256x256 (thin)"},
        {6, 64, 128, "gemm 6x64x128"},
        {201, 2048, 256, "gemm 201x2048x256"},
        {129, 130, 131, "gemm 129x130x131 (ragged)"},
    };
    for (const auto& c : cases) {
        vk::ArenaScope scope(arena);
        auto x = randn((size_t)c.M * c.K);
        auto w = randn((size_t)c.N * c.K, 0.05f);
        auto b = randn((size_t)c.N);
        auto r = randn((size_t)c.M * c.N);

        Tensor tx = upload_f32(arena, x, c.M, c.K);
        Tensor tw = upload_f32(arena, w, c.N, c.K);
        Tensor tb = upload_f32(arena, b, c.N);
        Tensor tr = upload_f32(arena, r, c.M, c.N);
        Tensor to = arena_tensor(arena, DType::F32, c.M, c.N);

        LinearOpts o;
        o.bias = tb;
        o.residual = tr;
        o.act = Act::GeluErf;
        linear(to, tx, tw, o);

        std::vector<float> want((size_t)c.M * c.N);
        for (int m = 0; m < c.M; ++m)
            for (int n = 0; n < c.N; ++n) {
                double s = 0;
                for (int k = 0; k < c.K; ++k)
                    s += (double)x[(size_t)m * c.K + k] * w[(size_t)n * c.K + k];
                want[(size_t)m * c.N + n] =
                    act_ref((float)s + b[n] + r[(size_t)m * c.N + n], Act::GeluErf);
            }
        // The erf approximation in the shader is good to ~1.5e-7 absolute; the
        // 1e-4 budget is dominated by fp32 summation order over K.
        check(c.name, readback(to), want, 2e-4f);
    }

    // Past 65535 row tiles of 64 the dispatch splits; +3 rows leaves a ragged
    // tail on the far side of the split. A 2048x2048 ALIKED map is this shape.
    {
        vk::ArenaScope scope(arena);
        const int64_t M = 65535ll * 64 + 3;
        const int N = 2, K = 2;
        auto x = randn((size_t)M * K);
        auto w = randn((size_t)N * K, 0.1f);
        auto b = randn((size_t)N);

        Tensor to = arena_tensor(arena, DType::F32, M, N);
        LinearOpts o;
        o.bias = upload_f32(arena, b, N);
        linear(to, upload_f32(arena, x, M, K), upload_f32(arena, w, N, K), o);

        std::vector<float> want((size_t)M * N);
        for (int64_t m = 0; m < M; ++m)
            for (int n = 0; n < N; ++n) {
                double s = 0;
                for (int k = 0; k < K; ++k)
                    s += (double)x[(size_t)m * K + k] * w[(size_t)n * K + k];
                want[(size_t)m * N + n] = (float)s + b[n];
            }
        check("gemm 4.2M rows", readback(to), want, 2e-4f);
    }

    // fp16 weights: the normal path for checkpoint tensors.
    {
        vk::ArenaScope scope(arena);
        const int M = 64, N = 96, K = 128;
        auto x = randn((size_t)M * K);
        auto w = randn((size_t)N * K, 0.1f);
        // Round the reference through fp16 too -- we are testing the kernel,
        // not fp16's dynamic range.
        for (auto& v : w) v = half_to_float(float_to_half(v));

        Tensor tx = upload_f32(arena, x, M, K);
        Tensor tw = upload_f16(arena, w, N, K);
        Tensor to = arena_tensor(arena, DType::F32, M, N);
        linear(to, tx, tw);

        std::vector<float> want((size_t)M * N);
        for (int m = 0; m < M; ++m)
            for (int n = 0; n < N; ++n) {
                double s = 0;
                for (int k = 0; k < K; ++k)
                    s += (double)x[(size_t)m * K + k] * w[(size_t)n * K + k];
                want[(size_t)m * N + n] = (float)s;
            }
        check("gemm fp16 weights", readback(to), want, 2e-4f);
    }

    // Both tilings OpGemm picks between at M, N >= 128 with an fp16 weight, on
    // every device rather than only the one it measured faster. Ragged on all
    // three axes, which is where a staging or tail-guard mistake shows up.
    for (GemmTile tile : {GemmTile::Wide, GemmTile::Narrow}) {
        vk::ArenaScope scope(arena);
        set_gemm_tile(tile);
        const int M = 300, N = 201, K = 141;
        auto x = randn((size_t)M * K);
        auto w = randn((size_t)N * K, 0.1f);
        for (auto& v : w) v = half_to_float(float_to_half(v));
        auto b = randn((size_t)N);

        Tensor to = arena_tensor(arena, DType::F32, M, N);
        LinearOpts o;
        o.bias = upload_f32(arena, b, N);
        o.act = Act::Relu;
        linear(to, upload_f32(arena, x, M, K), upload_f16(arena, w, N, K), o);

        std::vector<float> want((size_t)M * N);
        for (int m = 0; m < M; ++m)
            for (int n = 0; n < N; ++n) {
                double s = 0;
                for (int k = 0; k < K; ++k)
                    s += (double)x[(size_t)m * K + k] * w[(size_t)n * K + k];
                want[(size_t)m * N + n] = act_ref((float)s + b[n], Act::Relu);
            }
        check(tile == GemmTile::Wide ? "gemm 300x201x141 wide tile"
                                     : "gemm 300x201x141 narrow tile",
              readback(to), want, 2e-4f);
    }
    set_gemm_tile(GemmTile::Measured);

    // Cooperative matrix. N and K are multiples of 16 (the shape the tensor-core
    // path requires) while M is not, so the row tail is still exercised; the
    // ragged case above deliberately is not a multiple of 16 and therefore
    // measures the fallback.
    //
    // Both paths run against a double-precision reference, each with the
    // activations rounded the way that path rounds them: fp32 for the fallback,
    // fp16 for the tensor cores, whose A operand is fp16. Same convention as
    // "gemm fp16 weights" above -- what is under test is the kernel, not fp16's
    // dynamic range, and a loose tolerance would hide a real staging bug behind
    // rounding that is expected and quantified. Whether fp16 activations are
    // acceptable *for the model* is a different question, answered by comparing
    // masks end to end, not here.
    {
        vk::ArenaScope scope(arena);
        const int M = 300, N = 208, K = 144;
        auto x = randn((size_t)M * K);
        auto w = randn((size_t)N * K, 0.1f);
        for (auto& v : w) v = half_to_float(float_to_half(v));
        auto b = randn((size_t)N);
        auto r = randn((size_t)M * N);

        Tensor tx = upload_f32(arena, x, M, K);
        Tensor tw = upload_f16(arena, w, N, K);
        Tensor to = arena_tensor(arena, DType::F32, M, N);
        LinearOpts o;
        o.bias = upload_f32(arena, b, N);
        o.residual = upload_f32(arena, r, M, N);
        o.act = Act::Relu;

        auto reference = [&](bool x_f16) {
            std::vector<float> want((size_t)M * N);
            for (int m = 0; m < M; ++m)
                for (int n = 0; n < N; ++n) {
                    double s = 0;
                    for (int k = 0; k < K; ++k) {
                        float xv = x[(size_t)m * K + k];
                        if (x_f16) xv = half_to_float(float_to_half(xv));
                        s += (double)xv * w[(size_t)n * K + k];
                    }
                    want[(size_t)m * N + n] =
                        act_ref((float)s + b[n] + r[(size_t)m * N + n], Act::Relu);
                }
            return want;
        };

        set_coop_matrix_enabled(false);
        linear(to, tx, tw, o);
        check("gemm 300x208x144 fp32 path", readback(to), reference(false), 2e-4f);
        if (vk::Context::get().hasCoopMat()) {
            set_coop_matrix_enabled(true);
            linear(to, tx, tw, o);
            check("gemm 300x208x144 coop matrix", readback(to), reference(true), 2e-4f);
        } else {
            std::printf("  --   cooperative matrix unavailable: %s\n",
                        vk::Context::get().coopMatReason().c_str());
        }
        set_coop_matrix_enabled(true);
    }
}

// ================
// LayerNorm / GroupNorm
// ================

void test_norm(vk::Arena& arena) {
    {
        vk::ArenaScope scope(arena);
        const int R = 37, C = 1024;
        auto x = randn((size_t)R * C, 2.0f);
        auto res = randn((size_t)R * C, 0.5f);
        auto w = randn((size_t)C, 0.3f);
        auto b = randn((size_t)C, 0.3f);

        Tensor to = arena_tensor(arena, DType::F32, R, C);
        layer_norm(to, upload_f32(arena, x, R, C), upload_f32(arena, w, C),
                   upload_f32(arena, b, C), 1e-5f, upload_f32(arena, res, R, C));

        std::vector<float> want((size_t)R * C);
        for (int r = 0; r < R; ++r) {
            double mean = 0, var = 0;
            for (int c = 0; c < C; ++c) mean += x[(size_t)r * C + c] + res[(size_t)r * C + c];
            mean /= C;
            for (int c = 0; c < C; ++c) {
                double d = x[(size_t)r * C + c] + res[(size_t)r * C + c] - mean;
                var += d * d;
            }
            var /= C;
            double inv = 1.0 / std::sqrt(var + 1e-5);
            for (int c = 0; c < C; ++c)
                want[(size_t)r * C + c] =
                    (float)((x[(size_t)r * C + c] + res[(size_t)r * C + c] - mean) * inv) *
                        w[c] + b[c];
        }
        check("layer_norm + residual", readback(to), want, 1e-4f);
    }
    {
        vk::ArenaScope scope(arena);
        const int N = 512, C = 64, G = 8;
        auto x = randn((size_t)N * C, 1.5f);
        auto w = randn((size_t)C, 0.5f);
        auto b = randn((size_t)C, 0.5f);
        Tensor to = arena_tensor(arena, DType::F32, N, C);
        group_norm(arena, to, upload_f32(arena, x, N, C), upload_f32(arena, w, C),
                   upload_f32(arena, b, C), G, 1e-5f, Act::Relu);

        std::vector<float> want((size_t)N * C);
        const int per = C / G;
        for (int g = 0; g < G; ++g) {
            double mean = 0, var = 0;
            for (int n = 0; n < N; ++n)
                for (int i = 0; i < per; ++i) mean += x[(size_t)n * C + g * per + i];
            mean /= (double)N * per;
            for (int n = 0; n < N; ++n)
                for (int i = 0; i < per; ++i) {
                    double d = x[(size_t)n * C + g * per + i] - mean;
                    var += d * d;
                }
            var /= (double)N * per;
            double inv = 1.0 / std::sqrt(var + 1e-5);
            for (int n = 0; n < N; ++n)
                for (int i = 0; i < per; ++i) {
                    int c = g * per + i;
                    want[(size_t)n * C + c] =
                        act_ref((float)((x[(size_t)n * C + c] - mean) * inv) * w[c] + b[c],
                                Act::Relu);
                }
        }
        check("group_norm + relu", readback(to), want, 1e-3f);
    }
}

// ================
// Attention
// ================

void attn_reference(const std::vector<float>& q, const std::vector<float>& k,
                    const std::vector<float>& v, int nq, int nk, int H, int HD,
                    float scale, const std::vector<float>* bias, AttnBias mode,
                    std::vector<float>& out) {
    out.assign((size_t)nq * H * HD, 0.0f);
    std::vector<double> s((size_t)nk);
    for (int h = 0; h < H; ++h)
        for (int i = 0; i < nq; ++i) {
            double mx = -1e300;
            for (int j = 0; j < nk; ++j) {
                double d = 0;
                for (int c = 0; c < HD; ++c)
                    d += (double)q[((size_t)i * H + h) * HD + c] *
                         k[((size_t)j * H + h) * HD + c];
                d *= scale;
                if (mode == AttnBias::PerKey) d += (*bias)[j];
                if (mode == AttnBias::Full)
                    d += (*bias)[((size_t)h * nq + i) * nk + j];
                if (mode == AttnBias::Causal && j > i) d = -1e300;
                s[(size_t)j] = d;
                mx = std::max(mx, d);
            }
            double sum = 0;
            for (int j = 0; j < nk; ++j) {
                s[(size_t)j] = (s[(size_t)j] <= -1e299) ? 0.0 : std::exp(s[(size_t)j] - mx);
                sum += s[(size_t)j];
            }
            for (int c = 0; c < HD; ++c) {
                double acc = 0;
                for (int j = 0; j < nk; ++j)
                    acc += s[(size_t)j] * v[((size_t)j * H + h) * HD + c];
                out[((size_t)i * H + h) * HD + c] = (float)(acc / (sum > 0 ? sum : 1.0));
            }
        }
}

void test_attention(vk::Arena& arena) {
    struct Case { int nq, nk, H, HD; AttnBias mode; const char* name; };
    const Case cases[] = {
        {40, 40, 4, 64, AttnBias::None, "attn 40x40 h4 d64"},
        {70, 33, 8, 32, AttnBias::None, "attn 70x33 h8 d32 (ragged)"},
        {32, 32, 1, 256, AttnBias::None, "attn 32x32 h1 d256 (memory attn)"},
        {17, 96, 2, 16, AttnBias::None, "attn 17x96 h2 d16"},
        // Hiera's head dim divides neither 16 nor the 16-wide staging chunk:
        // 96 for tiny/small, 72 for large/b+. The second one is q-pooled, so
        // nq is a quarter of nk.
        {49, 49, 8, 96, AttnBias::None, "attn h8 d96 (hiera tiny)"},
        {16, 64, 16, 72, AttnBias::None, "attn h16 d72 pooled (hiera large)"},
        {24, 24, 2, 64, AttnBias::Causal, "attn causal (text encoder)"},
        {12, 20, 3, 32, AttnBias::PerKey, "attn per-key bias (padding)"},
        {9, 15, 2, 32, AttnBias::Full, "attn full bias (box RPB)"},
    };
    for (const auto& c : cases) {
        vk::ArenaScope scope(arena);
        const int dim = c.H * c.HD;
        auto q = randn((size_t)c.nq * dim);
        auto k = randn((size_t)c.nk * dim);
        auto v = randn((size_t)c.nk * dim);
        std::vector<float> bias;
        Tensor tbias;
        if (c.mode == AttnBias::PerKey) {
            bias = randn((size_t)c.nk);
            for (int j = c.nk / 2; j < c.nk; ++j) bias[(size_t)j] = -1e9f;  // padding
            tbias = upload_f32(arena, bias, c.nk);
        } else if (c.mode == AttnBias::Full) {
            bias = randn((size_t)c.H * c.nq * c.nk);
            tbias = upload_f32(arena, bias, (int64_t)c.H * c.nq * c.nk);
        }

        Tensor to = arena_tensor(arena, DType::F32, c.nq, dim);
        AttnOpts o;
        o.n_heads = c.H;
        o.head_dim = c.HD;
        o.scale = 1.0f / std::sqrt((float)c.HD);
        o.bias_mode = c.mode;
        o.bias = tbias;
        Tensor tq = upload_f32(arena, q, c.nq, dim);
        Tensor tk = upload_f32(arena, k, c.nk, dim);
        Tensor tv = upload_f32(arena, v, c.nk, dim);

        set_coop_matrix_enabled(false);
        attention(to, tq, tk, tv, c.nq, c.nk, o);
        std::vector<float> want;
        attn_reference(q, k, v, c.nq, c.nk, c.H, c.HD, o.scale, &bias, c.mode, want);
        check(c.name, readback(to), want, 3e-4f);

        if (coop_attn(c.HD)) {
            set_coop_matrix_enabled(true);
            attention(to, tq, tk, tv, c.nq, c.nk, o);
            std::vector<float> want16;
            attn_reference(round_f16(q), round_f16(k), v, c.nq, c.nk, c.H, c.HD, o.scale,
                           &bias, c.mode, want16);
            check((std::string(c.name) + " [coop]").c_str(), readback(to), want16, 3e-4f);
        }
        set_coop_matrix_enabled(true);
    }

    // Batched: the ViT's window attention runs 9 independent 576-token problems.
    {
        vk::ArenaScope scope(arena);
        const int B = 3, nq = 20, H = 2, HD = 32, dim = H * HD;
        auto q = randn((size_t)B * nq * dim);
        auto k = randn((size_t)B * nq * dim);
        auto v = randn((size_t)B * nq * dim);
        Tensor to = arena_tensor(arena, DType::F32, B, nq, dim);
        AttnOpts o;
        o.n_heads = H;
        o.head_dim = HD;
        o.batch = B;
        Tensor tq = upload_f32(arena, q, B, nq, dim);
        Tensor tk = upload_f32(arena, k, B, nq, dim);
        Tensor tv = upload_f32(arena, v, B, nq, dim);
        set_coop_matrix_enabled(false);
        attention(to, tq, tk, tv, nq, nq, o);
        std::vector<float> want((size_t)B * nq * dim);
        for (int b = 0; b < B; ++b) {
            std::vector<float> qb(q.begin() + (size_t)b * nq * dim,
                                  q.begin() + (size_t)(b + 1) * nq * dim);
            std::vector<float> kb(k.begin() + (size_t)b * nq * dim,
                                  k.begin() + (size_t)(b + 1) * nq * dim);
            std::vector<float> vb(v.begin() + (size_t)b * nq * dim,
                                  v.begin() + (size_t)(b + 1) * nq * dim);
            std::vector<float> ob;
            attn_reference(qb, kb, vb, nq, nq, H, HD, 1.0f / std::sqrt((float)HD), nullptr,
                           AttnBias::None, ob);
            std::copy(ob.begin(), ob.end(), want.begin() + (size_t)b * nq * dim);
        }
        check("attn batched (ViT windows)", readback(to), want, 3e-4f);
        set_coop_matrix_enabled(true);
        if (coop_attn(HD)) {
            attention(to, tq, tk, tv, nq, nq, o);
            // Same reference loop, on operands rounded the way the fragments
            // hold them.
            const auto q16 = round_f16(q), k16 = round_f16(k);
            std::vector<float> want16((size_t)B * nq * dim);
            for (int b = 0; b < B; ++b) {
                std::vector<float> qb(q16.begin() + (size_t)b * nq * dim,
                                      q16.begin() + (size_t)(b + 1) * nq * dim);
                std::vector<float> kb(k16.begin() + (size_t)b * nq * dim,
                                      k16.begin() + (size_t)(b + 1) * nq * dim);
                std::vector<float> vb(v.begin() + (size_t)b * nq * dim,
                                      v.begin() + (size_t)(b + 1) * nq * dim);
                std::vector<float> ob;
                attn_reference(qb, kb, vb, nq, nq, H, HD, 1.0f / std::sqrt((float)HD),
                               nullptr, AttnBias::None, ob);
                std::copy(ob.begin(), ob.end(), want16.begin() + (size_t)b * nq * dim);
            }
            check("attn batched (ViT windows) [coop]", readback(to), want16, 3e-4f);
        }
    }

    // Strided q/k/v: pointing straight into a fused [N, 3*E] projection.
    {
        vk::ArenaScope scope(arena);
        const int nq = 24, H = 2, HD = 32, dim = H * HD;
        auto qkv = randn((size_t)nq * dim * 3);
        Tensor tqkv = upload_f32(arena, qkv, nq, dim * 3);
        Tensor to = arena_tensor(arena, DType::F32, nq, dim);
        AttnOpts o;
        o.n_heads = H;
        o.head_dim = HD;
        o.q_stride = o.k_stride = o.v_stride = dim * 3;
        set_coop_matrix_enabled(false);
        attention(to, tqkv, tqkv.offsetElems(dim), tqkv.offsetElems(2 * dim), nq, nq, o);

        std::vector<float> q(nq * dim), k(nq * dim), v(nq * dim);
        for (int i = 0; i < nq; ++i)
            for (int c = 0; c < dim; ++c) {
                q[(size_t)i * dim + c] = qkv[(size_t)i * dim * 3 + c];
                k[(size_t)i * dim + c] = qkv[(size_t)i * dim * 3 + dim + c];
                v[(size_t)i * dim + c] = qkv[(size_t)i * dim * 3 + 2 * dim + c];
            }
        std::vector<float> want;
        attn_reference(q, k, v, nq, nq, H, HD, 1.0f / std::sqrt((float)HD), nullptr,
                       AttnBias::None, want);
        check("attn fused-qkv strides", readback(to), want, 3e-4f);
        set_coop_matrix_enabled(true);
        if (coop_attn(HD)) {
            attention(to, tqkv, tqkv.offsetElems(dim), tqkv.offsetElems(2 * dim), nq, nq,
                      o);
            std::vector<float> want16;
            attn_reference(round_f16(q), round_f16(k), v, nq, nq, H, HD,
                           1.0f / std::sqrt((float)HD), nullptr, AttnBias::None, want16);
            check("attn fused-qkv strides [coop]", readback(to), want16, 3e-4f);
        }
    }

    // Flash-decoding: passing an arena lets the launcher split the key range
    // across workgroups when there are too few queries x heads to fill the
    // device, which is exactly the memory-attention shape. The partial
    // softmaxes then have to merge back to the same answer.
    //
    // The per-key bias marks the tail of the range as padding, so at least one
    // slice sees nothing but masked keys -- the case where a naive combine
    // would inject phantom keys with weight exp(0).
    {
        vk::ArenaScope scope(arena);
        const int nq = 64, nk = 4096, H = 1, HD = 256, dim = H * HD;
        auto q = randn((size_t)nq * dim);
        auto k = randn((size_t)nk * dim);
        auto v = randn((size_t)nk * dim);
        auto bias = std::vector<float>((size_t)nk, 0.0f);
        for (int j = 3000; j < nk; ++j) bias[(size_t)j] = -1e9f;

        Tensor to = arena_tensor(arena, DType::F32, nq, dim);
        AttnOpts o;
        o.n_heads = H;
        o.head_dim = HD;
        o.bias_mode = AttnBias::PerKey;
        o.bias = upload_f32(arena, bias, nk);
        o.arena = &arena;
        Tensor tq = upload_f32(arena, q, nq, dim);
        Tensor tk = upload_f32(arena, k, nk, dim);
        Tensor tv = upload_f32(arena, v, nk, dim);
        set_coop_matrix_enabled(false);
        attention(to, tq, tk, tv, nq, nk, o);

        std::vector<float> want;
        attn_reference(q, k, v, nq, nk, H, HD, 1.0f / std::sqrt((float)HD), &bias,
                       AttnBias::PerKey, want);
        check("attn split-k (memory attn)", readback(to), want, 3e-4f);

        set_coop_matrix_enabled(true);
        if (coop_attn(HD)) {
            attention(to, tq, tk, tv, nq, nk, o);
            std::vector<float> want16;
            attn_reference(round_f16(q), round_f16(k), v, nq, nk, H, HD,
                           1.0f / std::sqrt((float)HD), &bias, AttnBias::PerKey, want16);
            check("attn split-k (memory attn) [coop]", readback(to), want16, 3e-4f);
        }
    }
}

// ================
// Metal (MPSGraph)
// ================

double rel_l2(const std::vector<float>& a, const std::vector<float>& b) {
    double num = 0, den = 0;
    for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
        num += ((double)a[i] - b[i]) * ((double)a[i] - b[i]);
        den += (double)b[i] * b[i];
    }
    return a.size() == b.size() ? std::sqrt(num / std::max(den, 1e-30)) : 1e30;
}

void expect(bool ok, const char* name, const char* detail) {
    ++g_checks;
    std::printf("  %s %-28s %s\n", ok ? "ok  " : "FAIL", name, detail);
    if (!ok) ++g_failures;
}

void test_external_handoff() {
    auto& stream = vk::Stream::get();
    stream.sync();
    const VkDevice device = vk::Context::get().device();
    VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
    type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    VkSemaphoreCreateInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    info.pNext = &type;
    VkSemaphore gate = VK_NULL_HANDLE;
    NN_VK_CHECK(vkCreateSemaphore(device, &info, nullptr, &gate));
    stream.waitOn(gate, 1);
    const auto first = stream.handOff();
    const VkSemaphore timeline = stream.timeline();
    VkSemaphoreWaitInfo wait{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
    wait.semaphoreCount = 1;
    wait.pSemaphores = &timeline;
    wait.pValues = &first.wait;
    expect(vkWaitSemaphores(device, &wait, 1000000) == VK_TIMEOUT,
           "handoff waits for transfer", "");
    VkSemaphoreSignalInfo signal{VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO};
    signal.semaphore = gate;
    signal.value = 1;
    NN_VK_CHECK(vkSignalSemaphore(device, &signal));
    vk::wait_timeline(timeline, first.wait);
    signal.semaphore = timeline;
    signal.value = first.signal;
    NN_VK_CHECK(vkSignalSemaphore(device, &signal));
    const auto second = stream.handOff();
    expect(second.wait > first.signal, "handoff consumes prior wait", "");
    vk::wait_timeline(timeline, second.wait);
    signal.value = second.signal;
    NN_VK_CHECK(vkSignalSemaphore(device, &signal));
    stream.barrierNow();
    stream.sync();
    vkDestroySemaphore(device, gate, nullptr);
}

// How many MPSGraph ops `fn` ran, as (attention, gemm).
template <class F>
std::pair<uint64_t, uint64_t> metal_ops(F&& fn) {
    const MetalCounts a = metal_counts();
    fn();
    const MetalCounts b = metal_counts();
    return {b.attention - a.attention, b.gemm - a.gemm};
}

void test_metal(vk::Arena& arena) {
    char buf[160];
    set_metal_enabled(true);

    // A 4096-token matcher's shapes at a quarter of the tokens, so the fp64
    // reference stays quick. 1e-5 relative L2 is what an fp32 SDPA must reach.
    struct Case { int B, nq, nk, H, HD; const char* name; };
    const Case cases[] = {
        {1, 1024, 1024, 8, 64, "metal attn 1024 h8 d64"},
        {1, 300, 700, 8, 64, "metal attn 300x700 (cross)"},
        {2, 96, 96, 4, 32, "metal attn batched"},
        {1, 200, 200, 2, 128, "metal attn h2 d128"},
        {1, 100, 100, 1, 256, "metal attn d256 stays vulkan"},
    };
    for (const auto& c : cases) {
        vk::ArenaScope scope(arena);
        const int dim = c.H * c.HD;
        auto q = randn((size_t)c.B * c.nq * dim), k = randn((size_t)c.B * c.nk * dim),
             v = randn((size_t)c.B * c.nk * dim);
        // One element into its allocation, so the NDArray offset is not 256-aligned.
        Tensor tq(arena_tensor(arena, DType::F32, (int64_t)q.size() + 1).offsetElems(1).ptr,
                  DType::F32, c.B, c.nq, dim);
        tensor_from_host(tq, q.data(), (int64_t)q.size());
        Tensor tk = upload_f32(arena, k, c.B, c.nk, dim);
        Tensor tv = upload_f32(arena, v, c.B, c.nk, dim);
        Tensor to = arena_tensor(arena, DType::F32, c.B, c.nq, dim);
        AttnOpts o;
        o.n_heads = c.H;
        o.head_dim = c.HD;
        o.batch = c.B;
        const auto n = metal_ops([&] { attention(to, tq, tk, tv, c.nq, c.nk, o); });
        std::vector<float> want((size_t)c.B * c.nq * dim);
        parallel_for(c.B, [&](int64_t b0, int64_t b1) {
            for (int64_t b = b0; b < b1; ++b) {
                auto sl = [&](const std::vector<float>& t, int64_t n) {
                    return std::vector<float>(t.begin() + b * n * dim, t.begin() + (b + 1) * n * dim);
                };
                std::vector<float> ob;
                attn_reference(sl(q, c.nq), sl(k, c.nk), sl(v, c.nk), c.nq, c.nk, c.H, c.HD,
                               1.0f / std::sqrt((float)c.HD), nullptr, AttnBias::None, ob);
                std::copy(ob.begin(), ob.end(), want.begin() + b * c.nq * dim);
            }
        });
        const auto got = readback(to);
        check(c.name, got, want, 3e-4f);
        const double e = rel_l2(got, want);
        std::snprintf(buf, sizeof buf, "rel L2 %.3g (bar 1e-5), %llu MPSGraph op", e,
                      (unsigned long long)n.first);
        // MPS is wrong at d256 (OpMetal.mm), so that one must not reach it.
        const uint64_t want_ops = c.HD <= 128 ? 1 : 0;
        expect(e <= 1e-5 && n.first == want_ops, (std::string(c.name) + " L2").c_str(), buf);
    }

    // Every epilogue term, both weight dtypes, an alpha, ragged axes, and a
    // weight view that does not start its allocation.
    for (DType wt : {DType::F32, DType::F16}) {
        vk::ArenaScope scope(arena);
        const int M = 300, N = 257, K = 129;
        const float alpha = 0.5f;
        auto x = randn((size_t)M * K), w = randn((size_t)N * K, 0.1f), b = randn((size_t)N),
             r = randn((size_t)M * N);
        if (wt == DType::F16) w = round_f16(w);
        Tensor tw = wt == DType::F16 ? upload_f16(arena, w, N, K) : upload_f32(arena, w, N, K);
        Tensor to = arena_tensor(arena, DType::F32, M, N);
        LinearOpts o;
        o.bias = upload_f32(arena, b, N);
        o.residual = upload_f32(arena, r, M, N);
        o.act = Act::GeluErf;
        o.alpha = alpha;
        Tensor tx = upload_f32(arena, x, M, K);
        const auto n = metal_ops([&] { linear(to, tx, tw, o); });
        std::vector<float> want((size_t)M * N);
        for (int m = 0; m < M; ++m)
            for (int j = 0; j < N; ++j) {
                double acc = 0;
                for (int kk = 0; kk < K; ++kk) acc += (double)x[(size_t)m * K + kk] * w[(size_t)j * K + kk];
                want[(size_t)m * N + j] =
                    act_ref((float)(alpha * acc) + b[j] + r[(size_t)m * N + j], Act::GeluErf);
            }
        const char* nm = wt == DType::F16 ? "metal gemm epilogue f16 w" : "metal gemm epilogue f32 w";
        check(nm, readback(to), want, 2e-4f);
        std::snprintf(buf, sizeof buf, "%llu MPSGraph op", (unsigned long long)n.second);
        expect(n.second == 1, (std::string(nm) + " ran").c_str(), buf);
    }

    // x = W h + b + x, the residual FFN: the output aliases an input.
    {
        vk::ArenaScope scope(arena);
        const int M = 256, N = 256, K = 512;
        auto h = randn((size_t)M * K), w = randn((size_t)N * K, 0.05f), b = randn((size_t)N),
             x = randn((size_t)M * N);
        Tensor tx = upload_f32(arena, x, M, N);
        LinearOpts o;
        o.bias = upload_f32(arena, b, N);
        o.residual = tx;
        Tensor th = upload_f32(arena, h, M, K), tw = upload_f32(arena, w, N, K);
        const auto n = metal_ops([&] { linear(tx, th, tw, o); });
        std::vector<float> want((size_t)M * N);
        for (int m = 0; m < M; ++m)
            for (int j = 0; j < N; ++j) {
                double acc = 0;
                for (int kk = 0; kk < K; ++kk) acc += (double)h[(size_t)m * K + kk] * w[(size_t)j * K + kk];
                want[(size_t)m * N + j] = (float)acc + b[j] + x[(size_t)m * N + j];
            }
        check("metal gemm in place", readback(tx), want, 2e-4f);
        expect(n.second == 1, "metal gemm in place ran", "");
    }

    // Vulkan -> Metal -> Vulkan with no host sync between. Each producer runs
    // tens of ms, so a consumer that does not wait reads the zeros the buffers
    // start with. Checked on every 64th row; the reference is fp64.
    {
        vk::ArenaScope scope(arena);
        const int T = 4096, H = 8, D = 64, dim = H * D, N = 4096, step = 64;
        auto q = randn((size_t)T * dim), w = randn((size_t)N * dim, 0.05f);
        Tensor tq = upload_f32(arena, q, T, dim);
        Tensor p = arena_tensor(arena, DType::F32, T, dim);
        Tensor y = arena_tensor(arena, DType::F32, T, N);
        Tensor y2 = arena_tensor(arena, DType::F32, T, N);
        Tensor tw = upload_f32(arena, w, N, dim);
        vk::Stream::get().zero(p.ptr, (uint64_t)T * dim * 4);
        vk::Stream::get().zero(y.ptr, (uint64_t)T * N * 4);
        vk::Stream::get().zero(y2.ptr, (uint64_t)T * N * 4);
        vk::Stream::get().sync();
        AttnOpts o;
        o.n_heads = H;
        o.head_dim = D;
        set_metal_enabled(false);
        for (int i = 0; i < 6; ++i) attention(p, tq, tq, tq, T, T, o);
        set_metal_enabled(true);
        const auto n = metal_ops([&] {
            for (int i = 0; i < 4; ++i) matmul_nt(y, p, tw);
        });
        add(y2, y, y);
        const auto py = readback(p), all = readback(y2);
        std::vector<float> got, want;
        for (int t = 0; t < T; t += step)
            for (int j = 0; j < N; ++j) {
                double acc = 0;
                for (int c = 0; c < dim; ++c) acc += (double)py[(size_t)t * dim + c] * w[(size_t)j * dim + c];
                want.push_back((float)(2 * acc));
                got.push_back(all[(size_t)t * N + j]);
            }
        std::vector<float> pwant;
        attn_reference(std::vector<float>(q.begin(), q.begin() + (size_t)step * dim), q, q, step, T,
                       H, D, 1.0f / std::sqrt((float)D), nullptr, AttnBias::None, pwant);
        check("metal hand-off producer", std::vector<float>(py.begin(), py.begin() + (size_t)step * dim),
              pwant, 3e-4f);
        check("metal hand-off order", got, want, 2e-4f);
        expect(n.second == 4, "metal hand-off ran on metal", "");
    }

    // Metal -> Vulkan -> Metal. The Vulkan add waits on the first section's
    // event; the second section is committed behind it, and only its own wait
    // keeps it from reading the add's output before the add has run.
    {
        vk::ArenaScope scope(arena);
        const int T = 4096, H = 8, D = 64, dim = H * D, N = 4096, step = 64;
        auto q = randn((size_t)T * dim), w = randn((size_t)N * dim, 0.05f);
        Tensor tq = upload_f32(arena, q, T, dim);
        Tensor p = arena_tensor(arena, DType::F32, T, dim);
        Tensor p2 = arena_tensor(arena, DType::F32, T, dim);
        Tensor y = arena_tensor(arena, DType::F32, T, N);
        Tensor tw = upload_f32(arena, w, N, dim);
        vk::Stream::get().zero(p.ptr, (uint64_t)T * dim * 4);
        vk::Stream::get().zero(p2.ptr, (uint64_t)T * dim * 4);
        vk::Stream::get().zero(y.ptr, (uint64_t)T * N * 4);
        vk::Stream::get().sync();
        AttnOpts o;
        o.n_heads = H;
        o.head_dim = D;
        const auto n1 = metal_ops([&] {
            for (int i = 0; i < 8; ++i) attention(p, tq, tq, tq, T, T, o);
        });
        set_metal_enabled(false);
        add(p2, p, p);
        set_metal_enabled(true);
        const auto n2 = metal_ops([&] { matmul_nt(y, p2, tw); });
        const auto pp = readback(p), yy = readback(y);
        std::vector<float> got, want;
        for (int t = 0; t < T; t += step)
            for (int j = 0; j < N; ++j) {
                double acc = 0;
                for (int c = 0; c < dim; ++c)
                    acc += 2.0 * pp[(size_t)t * dim + c] * w[(size_t)j * dim + c];
                want.push_back((float)acc);
                got.push_back(yy[(size_t)t * N + j]);
            }
        check("metal M-V-M order", got, want, 2e-4f);
        expect(n1.first == 8 && n2.second == 1, "metal M-V-M ran on metal", "");
    }

    // The smallest shapes, nq or nk of 1 and 2: MPSGraph aborts the process on
    // some of them, so this is only that they come back right on either path.
    {
        struct E { int B, nq, nk, H, HD; };
        const E es[] = {{1, 1, 1, 1, 64}, {1, 1, 2, 1, 16}, {1, 2, 1, 1, 16}, {1, 2, 2, 1, 16},
                        {2, 1, 1, 1, 16}, {1, 1, 1, 4, 16}, {1, 1, 7, 8, 64}, {1, 2, 2, 8, 128}};
        for (const auto& c : es) {
            vk::ArenaScope scope(arena);
            const int dim = c.H * c.HD;
            auto q = randn((size_t)c.B * c.nq * dim), k = randn((size_t)c.B * c.nk * dim),
                 v = randn((size_t)c.B * c.nk * dim);
            Tensor tq = upload_f32(arena, q, c.B, c.nq, dim), tk = upload_f32(arena, k, c.B, c.nk, dim),
                   tv = upload_f32(arena, v, c.B, c.nk, dim);
            Tensor to = arena_tensor(arena, DType::F32, c.B, c.nq, dim);
            AttnOpts o;
            o.n_heads = c.H;
            o.head_dim = c.HD;
            o.batch = c.B;
            const auto n = metal_ops([&] { attention(to, tq, tk, tv, c.nq, c.nk, o); });
            std::vector<float> want;
            for (int b = 0; b < c.B; ++b) {
                auto sl = [&](const std::vector<float>& t, int64_t m) {
                    return std::vector<float>(t.begin() + b * m * dim, t.begin() + (b + 1) * m * dim);
                };
                std::vector<float> ob;
                attn_reference(sl(q, c.nq), sl(k, c.nk), sl(v, c.nk), c.nq, c.nk, c.H, c.HD,
                               1.0f / std::sqrt((float)c.HD), nullptr, AttnBias::None, ob);
                want.insert(want.end(), ob.begin(), ob.end());
            }
            const double e = rel_l2(readback(to), want);
            std::snprintf(buf, sizeof buf, "B%d nq%d nk%d H%d d%d: rel L2 %.3g, %llu MPSGraph op",
                          c.B, c.nq, c.nk, c.H, c.HD, e, (unsigned long long)n.first);
            const bool single = (int64_t)c.B * c.H * c.nq * c.nk == 1;
            expect(e <= 1e-5 && (!single || n.first == 0), "metal attn tiny", buf);
        }
    }

    // The fp16 tier: against the fp32 reference to fp16 accuracy, and it must
    // actually differ from the fp32 path or the scope did nothing.
    {
        vk::ArenaScope scope(arena);
        const int T = 512, H = 8, D = 64, dim = H * D;
        auto q = randn((size_t)T * dim), k = randn((size_t)T * dim), v = randn((size_t)T * dim);
        Tensor tq = upload_f32(arena, q, T, dim), tk = upload_f32(arena, k, T, dim),
               tv = upload_f32(arena, v, T, dim);
        Tensor to = arena_tensor(arena, DType::F32, T, dim);
        AttnOpts o;
        o.n_heads = H;
        o.head_dim = D;
        attention(to, tq, tk, tv, T, T, o);
        const auto f32 = readback(to);
        std::pair<uint64_t, uint64_t> n;
        {
            Fp16Scope half(true);
            n = metal_ops([&] { attention(to, tq, tk, tv, T, T, o); });
        }
        const auto f16 = readback(to);
        std::vector<float> want;
        attn_reference(q, k, v, T, T, H, D, 1.0f / std::sqrt((float)D), nullptr, AttnBias::None,
                       want);
        const double e16 = rel_l2(f16, want), d = rel_l2(f16, f32);
        std::snprintf(buf, sizeof buf, "rel L2 %.3g vs fp64 (bar 5e-3), %.3g from fp32 (> 1e-5)",
                      e16, d);
        expect(e16 <= 5e-3 && d > 1e-5 && n.first == 1 && !fp16_active(), "metal fp16 attn", buf);
    }

    // x_scale keeps fp16 operands in range: x near 2e5 overflows fp16 unless the
    // fp16 tier scales it down first, as the tensor-core kernels do.
    {
        vk::ArenaScope scope(arena);
        const int M = 256, N = 256, K = 128;
        auto x = randn((size_t)M * K, 2e5f), w = randn((size_t)N * K, 0.05f);
        Tensor to = arena_tensor(arena, DType::F32, M, N);
        LinearOpts lo;
        lo.x_scale = 1.0f / 1024;
        std::pair<uint64_t, uint64_t> n;
        {
            Fp16Scope half(true);
            Tensor tx = upload_f32(arena, x, M, K), tw = upload_f32(arena, w, N, K);
            n = metal_ops([&] { linear(to, tx, tw, lo); });
        }
        std::vector<float> want((size_t)M * N);
        for (int m = 0; m < M; ++m)
            for (int j = 0; j < N; ++j) {
                double acc = 0;
                for (int kk = 0; kk < K; ++kk) acc += (double)x[(size_t)m * K + kk] * w[(size_t)j * K + kk];
                want[(size_t)m * N + j] = (float)acc;
            }
        const double e = rel_l2(readback(to), want);
        std::snprintf(buf, sizeof buf, "rel L2 %.3g (bar 5e-3), %llu MPSGraph op", e,
                      (unsigned long long)n.second);
        expect(e <= 5e-3 && n.second == 1, "metal fp16 gemm x_scale", buf);
    }

    // A range one byte past its allocation is refused; the range ending on the
    // last byte is not. Host-side only: nothing reaches the GPU.
    {
        vk::ArenaScope scope(arena);
        const uint64_t at = (arena.mark() + 255) & ~(uint64_t)255;
        const vk::DevicePtr p = arena_tensor(arena, DType::F32, 1).ptr;
        const uint64_t tail = arena.capacity() - at;
        bool fits = true, refused = false;
        try {
            vk::metal::check_range(p, tail, "test");
        } catch (const std::exception&) {
            fits = false;
        }
        try {
            vk::metal::check_range(p, tail + 1, "test");
        } catch (const std::exception& e) {
            refused = std::strstr(e.what(), "overruns") != nullptr;
        }
        expect(fits && refused, "metal range check", "");
    }

    // Off means off.
    {
        vk::ArenaScope scope(arena);
        set_metal_enabled(false);
        Tensor a = arena_tensor(arena, DType::F32, 256, 256), o = arena_tensor(arena, DType::F32, 256, 256);
        vk::Stream::get().zero(a.ptr, 256 * 256 * 4);
        const auto n = metal_ops([&] { matmul_nt(o, a, a); });
        expect(n.second == 0, "metal off runs vulkan", "");
        set_metal_enabled(true);
    }
    const MetalCounts c = metal_counts();
    std::snprintf(buf, sizeof buf, "%llu sections, %llu rotations",
                  (unsigned long long)c.sections, (unsigned long long)c.rotations);
    expect(c.rotations == 0, "metal sections stay whole", buf);
}

// ================
// RoPE
// ================

void test_rope(vk::Arena& arena) {
    vk::ArenaScope scope(arena);
    const int N = 36, H = 3, HD = 64, dim = H * HD, B = 2, stride = 3 * dim;
    auto x = randn((size_t)B * N * stride);
    auto f = randn((size_t)N * (HD / 2) * 2);
    for (bool split : {false, true}) {
        Tensor tx = upload_f32(arena, x, B * N, stride);
        rope(tx, upload_f32(arena, f, (int64_t)N * (HD / 2) * 2), H, HD, N, B, stride, split);
        std::vector<float> want = x;
        for (int b = 0; b < B; ++b)
            for (int n = 0; n < N; ++n)
                for (int h = 0; h < H; ++h)
                    for (int i = 0; i < HD / 2; ++i) {
                        const float c = f[((size_t)n * (HD / 2) + i) * 2];
                        const float s = f[((size_t)n * (HD / 2) + i) * 2 + 1];
                        const size_t first = ((size_t)b * N + n) * stride + h * HD + (split ? i : i * 2);
                        const size_t second = first + (split ? HD / 2 : 1);
                        want[first] = x[first] * c - x[second] * s;
                        want[second] = x[first] * s + x[second] * c;
                    }
        check(split ? "rope split-half batched stride" : "rope consecutive batched stride", readback(tx), want, 1e-5f);
    }
}

// ================
// Convolutions
// ================

void test_conv(vk::Arena& arena) {
    {
        vk::ArenaScope scope(arena);
        const int Hi = 19, Wi = 23, Ci = 12, Co = 16, kh = 3, kw = 3;
        const int Ho = Hi, Wo = Wi;  // stride 1, pad 1
        auto x = randn((size_t)Hi * Wi * Ci);
        auto w = randn((size_t)Co * Ci * kh * kw, 0.1f);
        auto b = randn((size_t)Co);
        Tensor to = arena_tensor(arena, DType::F32, Ho, Wo, Co);
        ConvOpts o;
        o.pad_y = o.pad_x = 1;
        o.bias = upload_f32(arena, b, Co);
        o.act = Act::Relu;
        conv2d(arena, to, upload_f32(arena, x, Hi, Wi, Ci),
               upload_f32(arena, w, Co, (int64_t)Ci * kh * kw), kh, kw, o);

        std::vector<float> want((size_t)Ho * Wo * Co);
        for (int y = 0; y < Ho; ++y)
            for (int xx = 0; xx < Wo; ++xx)
                for (int co = 0; co < Co; ++co) {
                    double s = b[co];
                    for (int ci = 0; ci < Ci; ++ci)
                        for (int ky = 0; ky < kh; ++ky)
                            for (int kx = 0; kx < kw; ++kx) {
                                int sy = y + ky - 1, sx = xx + kx - 1;
                                if (sy < 0 || sy >= Hi || sx < 0 || sx >= Wi) continue;
                                s += (double)x[((size_t)sy * Wi + sx) * Ci + ci] *
                                     w[(((size_t)co * Ci + ci) * kh + ky) * kw + kx];
                            }
                    want[((size_t)y * Wo + xx) * Co + co] = act_ref((float)s, Act::Relu);
                }
        check("conv2d 3x3 pad1 + relu", readback(to), want, 2e-4f);
    }
    {   // padding_mode='replicate'. Every 3x3 in MoGe's head pads this way, and
        // the difference from zero padding is confined to one ring of pixels --
        // which is exactly the ring a depth map's edge artefacts live on.
        vk::ArenaScope scope(arena);
        const int Hi = 11, Wi = 9, Ci = 8, Co = 6, kh = 3, kw = 3;
        auto x = randn((size_t)Hi * Wi * Ci);
        auto w = randn((size_t)Co * Ci * kh * kw, 0.1f);
        Tensor to = arena_tensor(arena, DType::F32, Hi, Wi, Co);
        ConvOpts o;
        o.pad_y = o.pad_x = 1;
        o.pad_replicate = true;
        conv2d(arena, to, upload_f32(arena, x, Hi, Wi, Ci),
               upload_f32(arena, w, Co, (int64_t)Ci * kh * kw), kh, kw, o);

        std::vector<float> want((size_t)Hi * Wi * Co);
        for (int y = 0; y < Hi; ++y)
            for (int xx = 0; xx < Wi; ++xx)
                for (int co = 0; co < Co; ++co) {
                    double s = 0;
                    for (int ci = 0; ci < Ci; ++ci)
                        for (int ky = 0; ky < kh; ++ky)
                            for (int kx = 0; kx < kw; ++kx) {
                                const int sy = std::min(std::max(y + ky - 1, 0), Hi - 1);
                                const int sx = std::min(std::max(xx + kx - 1, 0), Wi - 1);
                                s += (double)x[((size_t)sy * Wi + sx) * Ci + ci] *
                                     w[(((size_t)co * Ci + ci) * kh + ky) * kw + kx];
                            }
                    want[((size_t)y * Wi + xx) * Co + co] = (float)s;
                }
        check("conv2d 3x3 replicate pad", readback(to), want, 2e-4f);
    }
    {
        vk::ArenaScope scope(arena);
        const int Hi = 16, Wi = 16, C = 24, k = 7;
        auto x = randn((size_t)Hi * Wi * C);
        auto w = randn((size_t)C * k * k, 0.1f);
        auto b = randn((size_t)C);
        Tensor to = arena_tensor(arena, DType::F32, Hi, Wi, C);
        ConvOpts o;
        o.pad_y = o.pad_x = 3;
        o.bias = upload_f32(arena, b, C);
        conv2d_depthwise(to, upload_f32(arena, x, Hi, Wi, C),
                         upload_f32(arena, w, C, (int64_t)k * k), k, k, o);
        std::vector<float> want((size_t)Hi * Wi * C);
        for (int y = 0; y < Hi; ++y)
            for (int xx = 0; xx < Wi; ++xx)
                for (int c = 0; c < C; ++c) {
                    double s = b[c];
                    for (int ky = 0; ky < k; ++ky)
                        for (int kx = 0; kx < k; ++kx) {
                            int sy = y + ky - 3, sx = xx + kx - 3;
                            if (sy < 0 || sy >= Hi || sx < 0 || sx >= Wi) continue;
                            s += (double)x[((size_t)sy * Wi + sx) * C + c] *
                                 w[((size_t)c * k + ky) * k + kx];
                        }
                    want[((size_t)y * Wi + xx) * C + c] = (float)s;
                }
        check("conv2d depthwise 7x7", readback(to), want, 2e-4f);
    }
    for (int patch : {2, 4}) {
        vk::ArenaScope scope(arena);
        const int Hi = 7, Wi = 9, Ci = 10, Co = 6;
        const int taps = patch * patch;
        auto x = randn((size_t)Hi * Wi * Ci);
        auto w = randn((size_t)Ci * Co * taps, 0.2f);
        auto b = randn((size_t)Co);
        std::vector<float> wp((size_t)Co * taps * Ci);
        for (int ci = 0; ci < Ci; ++ci)
            for (int co = 0; co < Co; ++co)
                for (int i = 0; i < patch; ++i)
                    for (int j = 0; j < patch; ++j)
                        wp[((size_t)co * taps + i * patch + j) * Ci + ci] =
                            w[(((size_t)ci * Co + co) * patch + i) * patch + j];

        Tensor to = arena_tensor(arena, DType::F32, Hi * patch, Wi * patch, Co);
        const Tensor ti = upload_f32(arena, x, Hi, Wi, Ci);
        const Tensor tw = upload_f32(arena, wp, (int64_t)Co * taps, Ci);
        const Tensor tb = upload_f32(arena, b, Co);
        if (patch == 2) conv_transpose2x2(arena, to, ti, tw, tb, Act::GeluErf);
        else conv_transpose_patch(arena, to, ti, tw, tb, patch, Act::GeluErf);

        std::vector<float> want((size_t)Hi * patch * Wi * patch * Co);
        for (int y = 0; y < Hi; ++y)
            for (int xx = 0; xx < Wi; ++xx)
                for (int co = 0; co < Co; ++co)
                    for (int i = 0; i < patch; ++i)
                        for (int j = 0; j < patch; ++j) {
                            double s = b[co];
                            for (int ci = 0; ci < Ci; ++ci)
                                s += (double)x[((size_t)y * Wi + xx) * Ci + ci] *
                                     w[(((size_t)ci * Co + co) * patch + i) * patch + j];
                            want[((size_t)(y * patch + i) * (Wi * patch) + xx * patch + j) * Co + co] =
                                act_ref((float)s, Act::GeluErf);
                        }
        check(patch == 2 ? "conv_transpose 2x2 s2" : "conv_transpose 4x4 s4", readback(to), want, 2e-4f);
    }
}

// ================
// Resample / shuffle / gather
// ================

void test_spatial(vk::Arena& arena) {
    {
        vk::ArenaScope scope(arena);
        const int Hi = 8, Wi = 6, C = 3, Ho = 19, Wo = 13;
        auto x = randn((size_t)Hi * Wi * C);
        Tensor to = arena_tensor(arena, DType::F32, Ho, Wo, C);
        resize_bilinear(to, upload_f32(arena, x, Hi, Wi, C));
        std::vector<float> want((size_t)Ho * Wo * C);
        for (int y = 0; y < Ho; ++y)
            for (int xx = 0; xx < Wo; ++xx) {
                float sy = std::fmax((y + 0.5f) * ((float)Hi / Ho) - 0.5f, 0.0f);
                float sx = std::fmax((xx + 0.5f) * ((float)Wi / Wo) - 0.5f, 0.0f);
                int y0 = (int)std::floor(sy), x0 = (int)std::floor(sx);
                float fy = sy - y0, fx = sx - x0;
                int y1 = std::min(y0 + 1, Hi - 1), x1 = std::min(x0 + 1, Wi - 1);
                y0 = std::min(y0, Hi - 1);
                x0 = std::min(x0, Wi - 1);
                for (int c = 0; c < C; ++c) {
                    float v00 = x[((size_t)y0 * Wi + x0) * C + c];
                    float v01 = x[((size_t)y0 * Wi + x1) * C + c];
                    float v10 = x[((size_t)y1 * Wi + x0) * C + c];
                    float v11 = x[((size_t)y1 * Wi + x1) * C + c];
                    want[((size_t)y * Wo + xx) * C + c] =
                        (1 - fy) * ((1 - fx) * v00 + fx * v01) +
                        fy * ((1 - fx) * v10 + fx * v11);
                }
            }
        // 1e-4, not 1e-6: the shader and the reference compute the same
        // coordinate mapping with a different association of the fp32
        // multiply/subtract, which moves a sample weight by an ulp.
        check("resize_bilinear", readback(to), want, 1e-4f);
    }
    {
        // Window partition must round-trip: a 72x72 grid at ws=24 is what the
        // ViT's 28 local-attention blocks do twice each.
        vk::ArenaScope scope(arena);
        const int H = 24, W = 24, C = 5, ws = 8;
        const int nw = (H / ws) * (W / ws);
        auto x = randn((size_t)H * W * C);
        Tensor tx = upload_f32(arena, x, H, W, C);
        Tensor tw = arena_tensor(arena, DType::F32, nw, ws * ws, C);
        Tensor tb = arena_tensor(arena, DType::F32, H, W, C);
        window_partition(tw, tx, H, W, C, ws);
        window_unpartition(tb, tw, H, W, C, ws);
        check("window partition round-trip", readback(tb), x, 1e-6f);
    }
    {
        vk::ArenaScope scope(arena);
        const int V = 50, C = 8, N = 6;
        auto table = randn((size_t)V * C);
        std::vector<int32_t> ids = {3, 0, 49, 7, 7, 12};
        Tensor tt = upload_f32(arena, table, V, C);
        Tensor ti = arena_tensor(arena, DType::I32, N);
        vk::Stream::get().upload(ti.ptr, ids.data(), ids.size() * 4);
        Tensor to = arena_tensor(arena, DType::F32, N, C);
        gather_rows(to, tt, ti);
        std::vector<float> want((size_t)N * C);
        for (int i = 0; i < N; ++i)
            for (int c = 0; c < C; ++c) want[(size_t)i * C + c] = table[(size_t)ids[i] * C + c];
        check("gather_rows", readback(to), want, 1e-6f);
    }
    {
        vk::ArenaScope scope(arena);
        // Restored at the end: the cases after this one keep the inputs their
        // tolerances were set on.
        const std::mt19937 rng_before = g_rng;
        const int N = 40, C = 5, T = 27;
        auto x = randn((size_t)N * C);
        std::uniform_int_distribution<int> pick(-1, N - 1);
        Tensor tx = upload_f32(arena, x, N, C);
        // A sparse convolution against its explicit column matrix on the host,
        // on the 64-tile path and, at 128 output channels, the tensor-core one.
        for (const int Co : {40, 128}) {
            const int Ci = 32, Rc = 300;
            auto xc = randn((size_t)N * Ci);
            auto wc = randn((size_t)Co * T * Ci, 0.05f);
            auto bc = randn((size_t)Co);
            auto rc = randn((size_t)Rc * Co);
            std::vector<int32_t> nc((size_t)Rc * T);
            for (auto& v : nc) v = pick(g_rng);
            Tensor txc = upload_f32(arena, xc, N, Ci);
            Tensor tnc = arena_tensor(arena, DType::I32, Rc, T);
            vk::Stream::get().upload(tnc.ptr, nc.data(), nc.size() * 4);
            Tensor tbc = upload_f32(arena, bc, Co);
            for (bool f16 : {false, true}) {
                const bool coop = f16 && Co >= 128 && coop_matrix_enabled();
                const std::vector<float> wr = f16 ? round_f16(wc) : wc;
                const std::vector<float> xr = coop ? round_f16(xc) : xc;
                Tensor twc = f16 ? upload_f16(arena, wc, Co, (int64_t)T * Ci)
                                 : upload_f32(arena, wc, Co, (int64_t)T * Ci);
                Tensor tout = upload_f32(arena, rc, Rc, Co);
                LinearOpts lo;
                lo.bias = tbc;
                lo.residual = tout;
                lo.act = Act::Silu;
                sparse_conv(tout, txc, tnc, twc, lo);
                std::vector<float> ref((size_t)Rc * Co);
                for (int r = 0; r < Rc; ++r)
                    for (int o = 0; o < Co; ++o) {
                        double acc = bc[(size_t)o] + rc[(size_t)r * Co + o];
                        for (int t = 0; t < T; ++t) {
                            const int id = nc[(size_t)r * T + t];
                            if (id < 0) continue;
                            for (int c = 0; c < Ci; ++c)
                                acc += (double)xr[(size_t)id * Ci + c] *
                                       wr[((size_t)o * T + t) * Ci + c];
                        }
                        ref[(size_t)r * Co + o] = (float)(acc / (1.0 + std::exp(-acc)));
                    }
                const std::string name = std::string("sparse_conv ") + std::to_string(Co) +
                                         (coop ? " coop" : f16 ? " f16 weights" : "");
                check(name.c_str(), readback(tout), ref, coop ? 2e-3f : 1e-4f);
            }
        }

        const int S = 9;
        std::vector<int32_t> off = {0, 3, 3, 7, 8, 12, 20, 21, 30, 40};
        std::vector<int32_t> ids(40);
        for (size_t k = 0; k < ids.size(); ++k) ids[k] = (int32_t)((k * 7) % N);
        Tensor toff = arena_tensor(arena, DType::I32, S + 1);
        Tensor tids = arena_tensor(arena, DType::I32, (int64_t)ids.size());
        vk::Stream::get().upload(toff.ptr, off.data(), off.size() * 4);
        vk::Stream::get().upload(tids.ptr, ids.data(), ids.size() * 4);
        Tensor tm = arena_tensor(arena, DType::F32, S, C);
        segment_mean(tm, tx, toff, tids);
        std::vector<float> mean((size_t)S * C, 0.0f);
        for (int s = 0; s < S; ++s)
            for (int c = 0; c < C; ++c) {
                double sum = 0.0;
                for (int k = off[(size_t)s]; k < off[(size_t)s + 1]; ++k)
                    sum += x[(size_t)ids[(size_t)k] * C + c];
                const int n = off[(size_t)s + 1] - off[(size_t)s];
                mean[(size_t)s * C + c] = n ? (float)(sum / n) : 0.0f;
            }
        check("segment_mean", readback(tm), mean, 1e-5f);
        g_rng = rng_before;
    }
    {
        // ROI-Align against the torchvision sampling_ratio=0 rule.
        vk::ArenaScope scope(arena);
        const int H = 12, W = 12, C = 4, S = 7, NB = 2;
        auto feat = randn((size_t)H * W * C);
        std::vector<float> boxes = {1.5f, 2.0f, 8.5f, 9.0f, 0.0f, 0.0f, 12.0f, 12.0f};
        Tensor to = arena_tensor(arena, DType::F32, NB, S * S, C);
        roi_align(to, upload_f32(arena, feat, H, W, C),
                  upload_f32(arena, boxes, NB, 4), H, W, C, S);

        auto sample = [&](float y, float x, int c) {
            int y0 = (int)std::floor(y), x0 = (int)std::floor(x);
            float fy = y - y0, fx = x - x0;
            int y1 = std::min(y0 + 1, H - 1), x1 = std::min(x0 + 1, W - 1);
            y0 = std::max(0, std::min(y0, H - 1));
            x0 = std::max(0, std::min(x0, W - 1));
            return (1 - fy) * ((1 - fx) * feat[((size_t)y0 * W + x0) * C + c] +
                               fx * feat[((size_t)y0 * W + x1) * C + c]) +
                   fy * ((1 - fx) * feat[((size_t)y1 * W + x0) * C + c] +
                         fx * feat[((size_t)y1 * W + x1) * C + c]);
        };
        std::vector<float> want((size_t)NB * S * S * C);
        for (int b = 0; b < NB; ++b) {
            float x0 = boxes[(size_t)b * 4 + 0], y0 = boxes[(size_t)b * 4 + 1];
            float x1 = boxes[(size_t)b * 4 + 2], y1 = boxes[(size_t)b * 4 + 3];
            float bw = std::fmax(x1 - x0, 1e-6f) / S, bh = std::fmax(y1 - y0, 1e-6f) / S;
            int ny = std::max(1, (int)std::ceil(bh)), nx = std::max(1, (int)std::ceil(bw));
            for (int ph = 0; ph < S; ++ph)
                for (int pw = 0; pw < S; ++pw)
                    for (int c = 0; c < C; ++c) {
                        double s = 0;
                        for (int iy = 0; iy < ny; ++iy) {
                            float sy = y0 + bh * (ph + (iy + 0.5f) / ny);
                            for (int ix = 0; ix < nx; ++ix) {
                                float sx = x0 + bw * (pw + (ix + 0.5f) / nx);
                                if (sx < -1.0f || sx > W || sy < -1.0f || sy > H) continue;
                                s += sample(std::fmax(sy, 0.0f), std::fmax(sx, 0.0f), c);
                            }
                        }
                        want[(((size_t)b * S + ph) * S + pw) * C + c] =
                            (float)(s / (ny * nx));
                    }
        }
        check("roi_align", readback(to), want, 1e-4f);
    }
    {
        vk::ArenaScope scope(arena);
        const int H = 5, W = 4, C = 3, th = 2, tw = 3;
        auto x = randn((size_t)H * W * C);
        auto tile = randn((size_t)th * tw * C);
        Tensor to = arena_tensor(arena, DType::F32, H, W, C);
        add_tiled(to, upload_f32(arena, x, H, W, C), upload_f32(arena, tile, th, tw, C), H,
                  W, C, th, tw);
        std::vector<float> want((size_t)H * W * C);
        for (int y = 0; y < H; ++y)
            for (int xx = 0; xx < W; ++xx)
                for (int c = 0; c < C; ++c)
                    want[((size_t)y * W + xx) * C + c] =
                        x[((size_t)y * W + xx) * C + c] +
                        tile[((size_t)(y % th) * tw + (xx % tw)) * C + c];
        check("add_tiled (ViT pos embed)", readback(to), want, 1e-6f);
    }
    {
        // f32 -> f16 -> f32 round trip through the convert kernel.
        vk::ArenaScope scope(arena);
        const int N = 1001;
        auto x = randn((size_t)N, 4.0f);
        Tensor tx = upload_f32(arena, x, N);
        Tensor th = arena_tensor(arena, DType::F16, N);
        Tensor tb = arena_tensor(arena, DType::F32, N);
        copy(th, tx);
        copy(tb, th);
        std::vector<float> want(x.size());
        for (size_t i = 0; i < x.size(); ++i) want[i] = half_to_float(float_to_half(x[i]));
        // One fp16 ULP, not exactness. `float_to_half` on the host rounds to
        // nearest-even; SPIR-V's OpFConvert only rounds that way if the module
        // asks for it through VK_KHR_shader_float_controls, and AMD's default
        // is not RTE -- one value in a thousand comes back on the other side of
        // the tie (4.011719 where the host says 4.015625, adjacent fp16s). A
        // broken convert misses by far more than a ULP, so this still bites.
        check("convert f32<->f16", readback(tb), want, 1.1e-3f);
    }
}

// ================
// The learned-frontend ops (src/aliked/)
// ================

// torchvision's bilinear_interpolate, corner-by-corner: zero outside, and a
// corner that falls outside contributes nothing even when the sample point is
// inside. This is the reference the deformable convolution has to match, and
// the reason it is spelled out rather than reusing the clamped sampler above.
float sample_zero(const std::vector<float>& src, int H, int W, int C, int c, float y,
                  float x) {
    if (y <= -1.0f || (float)H <= y || x <= -1.0f || (float)W <= x) return 0.0f;
    const int y0 = (int)std::floor(y), x0 = (int)std::floor(x);
    const int y1 = y0 + 1, x1 = x0 + 1;
    const float ly = y - y0, lx = x - x0, hy = 1 - ly, hx = 1 - lx;
    auto at = [&](int yy, int xx) -> float {
        if (yy < 0 || yy > H - 1 || xx < 0 || xx > W - 1) return 0.0f;
        return src[((size_t)yy * W + xx) * C + c];
    };
    return hy * (hx * at(y0, x0) + lx * at(y0, x1)) +
           ly * (hx * at(y1, x0) + lx * at(y1, x1));
}

void test_learned_frontend(vk::Arena& arena) {
    {
        vk::ArenaScope scope(arena);
        const int rows = 257, cols = 3;
        std::vector<float> input(rows * cols), divisors(input.size());
        for (size_t i = 0; i < input.size(); ++i) {
            input[i] = (float)((int)(i % 511) - 255) / 117;
            divisors[i] = 0.1f + (float)(i % 79) / 17;
        }
        for (int broadcast : {0, 1, 2}) {
            const std::vector<float> denominator = broadcast == 0 ? divisors : broadcast == 1 ?
                std::vector<float>{0.229f, -0.224f, 0.225f} : std::vector<float>{-0.7f};
            const Tensor x = upload_f32(arena, input, rows, cols);
            nn::div(x, x, upload_f32(arena, denominator, (int64_t)denominator.size()));
            std::vector<float> expected(input.size());
            for (size_t i = 0; i < expected.size(); ++i)
                expected[i] = input[i] / denominator[broadcast == 0 ? i : broadcast == 1 ? i % cols : 0];
            check("division broadcast in place", readback(x), expected, 2e-6f);
        }
    }
    {   // SELU -- ALIKED's gate, in every block and both heads.
        vk::ArenaScope scope(arena);
        const int N = 4096;
        auto x = randn(N, 2.0f);
        Tensor to = arena_tensor(arena, DType::F32, N);
        unary(to, upload_f32(arena, x, N), Act::Selu);
        const float a = 1.6732632423543772848170429916717f;
        const float s = 1.0507009873554804934193349852946f;
        std::vector<float> want(x.size());
        for (size_t i = 0; i < x.size(); ++i)
            want[i] = s * (x[i] > 0 ? x[i] : a * (std::exp(x[i]) - 1.0f));
        check("selu", readback(to), want, 1e-5f);
    }
    {   // AvgPool2d(2,2) and (4,4) -- the encoder feeds blocks 3 and 4 from
        // pooled copies of the input.
        vk::ArenaScope scope(arena);
        const int Hi = 16, Wi = 12, C = 5;
        auto x = randn((size_t)Hi * Wi * C);
        Tensor tx = upload_f32(arena, x, Hi, Wi, C);
        for (int k : {2, 4}) {
            const int Ho = Hi / k, Wo = Wi / k;
            Tensor to = arena_tensor(arena, DType::F32, Ho, Wo, C);
            avgpool(to, tx, k);
            std::vector<float> want((size_t)Ho * Wo * C, 0.0f);
            for (int y = 0; y < Ho; ++y)
                for (int xx = 0; xx < Wo; ++xx)
                    for (int c = 0; c < C; ++c) {
                        double s = 0;
                        for (int dy = 0; dy < k; ++dy)
                            for (int dx = 0; dx < k; ++dx)
                                s += x[((size_t)(y * k + dy) * Wi + (xx * k + dx)) * C + c];
                        want[((size_t)y * Wo + xx) * C + c] = (float)(s / (k * k));
                    }
            check(k == 2 ? "avgpool 2x2" : "avgpool 4x4", readback(to), want, 1e-5f);
        }
    }
    {   // resize_bicubic vs torch's upsample_bicubic2d(align_corners=False).
        // Up and down: only the downscale reads clamped taps at both edges.
        for (int pass = 0; pass < 2; ++pass) {
            vk::ArenaScope scope(arena);
            const int Hi = pass ? 17 : 5, Wi = pass ? 13 : 7, C = 3;
            const int Ho = pass ? 6 : 19, Wo = pass ? 5 : 11;
            auto x = randn((size_t)Hi * Wi * C);
            Tensor to = arena_tensor(arena, DType::F32, Ho, Wo, C);
            resize_bicubic(to, upload_f32(arena, x, Hi, Wi, C));

            const float a = -0.75f;
            auto w1 = [&](float t) {
                t = std::fabs(t);
                if (t <= 1.0f) return ((a + 2.0f) * t - (a + 3.0f)) * t * t + 1.0f;
                if (t < 2.0f) return ((a * t - 5.0f * a) * t + 8.0f * a) * t - 4.0f * a;
                return 0.0f;
            };
            std::vector<float> want((size_t)Ho * Wo * C);
            for (int y = 0; y < Ho; ++y)
                for (int xx = 0; xx < Wo; ++xx) {
                    // No clamp to zero: torch clamps the tap indices instead.
                    const float sy = (y + 0.5f) * ((float)Hi / Ho) - 0.5f;
                    const float sx = (xx + 0.5f) * ((float)Wi / Wo) - 0.5f;
                    const int y0 = (int)std::floor(sy), x0 = (int)std::floor(sx);
                    for (int ky = 0; ky < 4; ++ky) {
                        const int yy = std::min(std::max(y0 - 1 + ky, 0), Hi - 1);
                        const float wy = w1(sy - (y0 - 1 + ky));
                        for (int kx = 0; kx < 4; ++kx) {
                            const int xj = std::min(std::max(x0 - 1 + kx, 0), Wi - 1);
                            const float ww = wy * w1(sx - (x0 - 1 + kx));
                            for (int c = 0; c < C; ++c)
                                want[((size_t)y * Wo + xx) * C + c] +=
                                    ww * x[((size_t)yy * Wi + xj) * C + c];
                        }
                    }
                }
            check(pass ? "resize_bicubic down" : "resize_bicubic up", readback(to), want,
                  1e-4f);
        }
    }
    {   // resize_bilinear(align_corners=True): the OTHER coordinate mapping.
        vk::ArenaScope scope(arena);
        const int Hi = 5, Wi = 7, C = 3, Ho = 17, Wo = 11;
        auto x = randn((size_t)Hi * Wi * C);
        Tensor to = arena_tensor(arena, DType::F32, Ho, Wo, C);
        resize_bilinear(to, upload_f32(arena, x, Hi, Wi, C), /*align_corners=*/true);
        std::vector<float> want((size_t)Ho * Wo * C);
        for (int y = 0; y < Ho; ++y)
            for (int xx = 0; xx < Wo; ++xx) {
                const float sy = (float)y * (Hi - 1.0f) / (Ho - 1.0f);
                const float sx = (float)xx * (Wi - 1.0f) / (Wo - 1.0f);
                int y0 = (int)std::floor(sy), x0 = (int)std::floor(sx);
                const float fy = sy - y0, fx = sx - x0;
                const int y1 = std::min(y0 + 1, Hi - 1), x1 = std::min(x0 + 1, Wi - 1);
                y0 = std::min(y0, Hi - 1);
                x0 = std::min(x0, Wi - 1);
                for (int c = 0; c < C; ++c)
                    want[((size_t)y * Wo + xx) * C + c] =
                        (1 - fy) * ((1 - fx) * x[((size_t)y0 * Wi + x0) * C + c] +
                                    fx * x[((size_t)y0 * Wi + x1) * C + c]) +
                        fy * ((1 - fx) * x[((size_t)y1 * Wi + x0) * C + c] +
                              fx * x[((size_t)y1 * Wi + x1) * C + c]);
            }
        check("resize_bilinear align_corners", readback(to), want, 1e-4f);
    }
    {   // deform_conv2d vs torchvision's definition. The offsets are large on
        // purpose -- +-3 px on a 9x11 map -- so a good fraction of the taps
        // land outside and the zero-fill rule is actually exercised.
        vk::ArenaScope scope(arena);
        const int Hi = 9, Wi = 11, Ci = 6, Co = 7, k = 3, pad = 1;
        const int Ho = Hi, Wo = Wi;
        const float max_offset = 2.5f;
        auto x = randn((size_t)Hi * Wi * Ci);
        auto w = randn((size_t)Co * Ci * k * k, 0.3f);
        auto bias = randn(Co);
        auto off = randn((size_t)Ho * Wo * 2 * k * k, 3.0f);

        Tensor tx = upload_f32(arena, x, Hi, Wi, Ci);
        Tensor tw = upload_f32(arena, w, Co, Ci * k * k);
        Tensor tb = upload_f32(arena, bias, Co);
        Tensor toff = upload_f32(arena, off, Ho, Wo, 2 * k * k);
        Tensor to = arena_tensor(arena, DType::F32, Ho, Wo, Co);
        ConvOpts o;
        o.pad_y = o.pad_x = pad;
        o.bias = tb;
        deform_conv2d(arena, to, tx, toff, tw, k, k, max_offset, o);

        std::vector<float> want((size_t)Ho * Wo * Co);
        for (int yo = 0; yo < Ho; ++yo)
            for (int xo = 0; xo < Wo; ++xo)
                for (int co = 0; co < Co; ++co) {
                    double s = bias[(size_t)co];
                    for (int ci = 0; ci < Ci; ++ci)
                        for (int ky = 0; ky < k; ++ky)
                            for (int kx = 0; kx < k; ++kx) {
                                const int tap = ky * k + kx;
                                const size_t ob =
                                    ((size_t)yo * Wo + xo) * 2 * k * k + 2 * tap;
                                const float dy = std::fmax(
                                    -max_offset, std::fmin(max_offset, off[ob]));
                                const float dx = std::fmax(
                                    -max_offset, std::fmin(max_offset, off[ob + 1]));
                                const float sy = (float)(yo + ky - pad) + dy;
                                const float sx = (float)(xo + kx - pad) + dx;
                                s += (double)w[((size_t)co * Ci + ci) * k * k + tap] *
                                     sample_zero(x, Hi, Wi, Ci, ci, sy, sx);
                            }
                    want[((size_t)yo * Wo + xo) * Co + co] = (float)s;
                }
        check("deform_conv2d", readback(to), want, 2e-4f);
    }
    {   // patch_gather: a k x k patch per centre, in conv-weight column order,
        // with centres deliberately on and past the border.
        vk::ArenaScope scope(arena);
        const int Hi = 7, Wi = 9, C = 4, k = 3, N = 5;
        auto x = randn((size_t)Hi * Wi * C);
        const int32_t centers[N * 2] = {3, 3, 0, 0, 8, 6, 4, 0, 0, 6};
        Tensor tc = arena_tensor(arena, DType::I32, N, 2);
        vk::Stream::get().upload(tc.ptr, centers, sizeof centers);
        Tensor to = arena_tensor(arena, DType::F32, N, C * k * k);
        patch_gather(to, upload_f32(arena, x, Hi, Wi, C), tc, k);

        std::vector<float> want((size_t)N * C * k * k, 0.0f);
        for (int n = 0; n < N; ++n)
            for (int c = 0; c < C; ++c)
                for (int ky = 0; ky < k; ++ky)
                    for (int kx = 0; kx < k; ++kx) {
                        const int y = centers[2 * n + 1] - k / 2 + ky;
                        const int xx = centers[2 * n] - k / 2 + kx;
                        if (y < 0 || y >= Hi || xx < 0 || xx >= Wi) continue;
                        want[(size_t)n * C * k * k + (size_t)c * k * k + ky * k + kx] =
                            x[((size_t)y * Wi + xx) * C + c];
                    }
        check("patch_gather", readback(to), want, 1e-6f);
    }
    {   // grid_sample_points, both conventions, with points outside the map so
        // the zero padding is covered too.
        vk::ArenaScope scope(arena);
        const int H = 8, W = 6, C = 3, N = 64;
        auto x = randn((size_t)H * W * C);
        auto pos = randn((size_t)N * 2, 0.8f);
        Tensor tx = upload_f32(arena, x, H, W, C);
        Tensor tp = upload_f32(arena, pos, N, 2);
        for (bool ac : {true, false}) {
            Tensor to = arena_tensor(arena, DType::F32, N, C);
            grid_sample_points(to, tx, tp, ac);
            std::vector<float> want((size_t)N * C);
            for (int n = 0; n < N; ++n) {
                const float gx = pos[(size_t)n * 2], gy = pos[(size_t)n * 2 + 1];
                const float sx = ac ? (gx + 1) * 0.5f * (W - 1) : ((gx + 1) * W - 1) * 0.5f;
                const float sy = ac ? (gy + 1) * 0.5f * (H - 1) : ((gy + 1) * H - 1) * 0.5f;
                for (int c = 0; c < C; ++c)
                    want[(size_t)n * C + c] = sample_zero(x, H, W, C, c, sy, sx);
            }
            // Same 1e-4 as resize_bilinear above, and for the same reason: the
            // host and the shader reach the sample coordinate by different
            // orderings of `(g + 1) * 0.5 * (W - 1)`, and a coordinate one ulp
            // apart across a texel boundary shifts the bilinear weights by far
            // more than the coordinate moved. Every device here lands at 4e-7
            // except Intel's UHD 750, which reproducibly hits 1.1e-5 on one of
            // the 192 samples -- inside a texel, not a broken op, which misses
            // by orders of magnitude.
            check(ac ? "grid_sample_points align" : "grid_sample_points noalign",
                  readback(to), want, 1e-4f);
        }
    }
    {   // grid_sample_points padding_mode='border': every point lands well
        // outside on at least one axis, where zeros and border disagree.
        vk::ArenaScope scope(arena);
        const int H = 8, W = 6, C = 3, N = 64;
        auto x = randn((size_t)H * W * C);
        auto pos = randn((size_t)N * 2, 1.5f);
        Tensor tx = upload_f32(arena, x, H, W, C);
        Tensor tp = upload_f32(arena, pos, N, 2);
        for (bool ac : {true, false}) {
            Tensor to = arena_tensor(arena, DType::F32, N, C);
            grid_sample_points(to, tx, tp, ac, GridPad::Border);
            std::vector<float> want((size_t)N * C), zeros((size_t)N * C);
            for (int n = 0; n < N; ++n) {
                const float gx = pos[(size_t)n * 2], gy = pos[(size_t)n * 2 + 1];
                float sx = ac ? (gx + 1) * 0.5f * (W - 1) : ((gx + 1) * W - 1) * 0.5f;
                float sy = ac ? (gy + 1) * 0.5f * (H - 1) : ((gy + 1) * H - 1) * 0.5f;
                for (int c = 0; c < C; ++c)
                    zeros[(size_t)n * C + c] = sample_zero(x, H, W, C, c, sy, sx);
                sx = std::min(std::max(sx, 0.0f), (float)(W - 1));
                sy = std::min(std::max(sy, 0.0f), (float)(H - 1));
                for (int c = 0; c < C; ++c)
                    want[(size_t)n * C + c] = sample_zero(x, H, W, C, c, sy, sx);
            }
            double gap = 0;
            for (size_t i = 0; i < want.size(); ++i)
                gap = std::max(gap, (double)std::fabs(want[i] - zeros[i]));
            check("grid_sample_points border fixture separates the modes",
                  {gap > 0.5 ? 1.0f : 0.0f}, {1.0f}, 0.0f);
            check(ac ? "grid_sample_points border align" : "grid_sample_points border noalign",
                  readback(to), want, 1e-4f);
        }
    }
    {   // l2_normalize_rows, including a row of exact zeros (a keypoint whose
        // descriptor cancelled) -- eps must keep that finite, not NaN.
        vk::ArenaScope scope(arena);
        const int rows = 300, cols = 128;
        auto x = randn((size_t)rows * cols);
        for (int c = 0; c < cols; ++c) x[(size_t)7 * cols + c] = 0.0f;
        Tensor to = arena_tensor(arena, DType::F32, rows, cols);
        l2_normalize_rows(to, upload_f32(arena, x, rows, cols));
        std::vector<float> want((size_t)rows * cols);
        for (int r = 0; r < rows; ++r) {
            double sq = 0;
            for (int c = 0; c < cols; ++c) sq += (double)x[(size_t)r * cols + c] *
                                                 x[(size_t)r * cols + c];
            const float inv = 1.0f / std::fmax((float)std::sqrt(sq), 1e-12f);
            for (int c = 0; c < cols; ++c)
                want[(size_t)r * cols + c] = x[(size_t)r * cols + c] * inv;
        }
        check("l2_normalize_rows", readback(to), want, 1e-5f);
    }
}

// ================
// The monocular-geometry ops (src/metric3d/)
// ================

void test_local_correlation(vk::Arena& arena) {
    std::mt19937 rng(2048);
    auto values = [&](size_t n, float scale = 1.0f) {
        std::normal_distribution<float> distribution(0.0f, scale);
        std::vector<float> v(n);
        for (float& x : v) x = distribution(rng);
        return v;
    };
    for (bool half : {false, true}) for (int radius : {0, 1, 3}) for (int size : {0, 1}) {
        vk::ArenaScope scope(arena);
        const int h = size ? 128 : 7, w = size ? 160 : 5, c = 19, window = 2 * radius + 1, taps = window * window;
        auto a = values(h * w * c), b = values(h * w * c), warp = values(h * w * 2, 0.8f);
        if (half) { a = round_f16(a); b = round_f16(b); }
        Tensor out = arena_tensor(arena, DType::F32, h, w, taps, 1, 3);
        Tensor ta = half ? upload_f16(arena, a, h * w, c).view(h, w, c) : upload_f32(arena, a, h, w, c);
        Tensor tb = half ? upload_f16(arena, b, h * w, c).view(h, w, c) : upload_f32(arena, b, h, w, c);
        local_correlation(out, ta, tb,
                          upload_f32(arena, warp, h, w, 2), radius);
        std::vector<float> expected((size_t)h * w * taps);
        for (int pixel = 0; pixel < h * w; ++pixel)
            for (int dy = -radius; dy <= radius; ++dy)
                for (int dx = -radius; dx <= radius; ++dx) {
                    const float x = ((warp[pixel * 2] + 2.0f * dx / w + 1) * w - 1) / 2;
                    const float y = ((warp[pixel * 2 + 1] + 2.0f * dy / h + 1) * h - 1) / 2;
                    double dot = 0;
                    for (int ch = 0; ch < c; ++ch)
                        dot += a[pixel * c + ch] * (double)sample_zero(b, h, w, c, ch, y, x);
                    expected[(size_t)pixel * taps + (dy + radius) * window + dx + radius] =
                        (float)(dot / std::sqrt((double)c));
                }
        const auto actual = readback(out);
        if (!size) check(half ? "warp correlation fp16" : "warp correlation fp32", actual, expected, 2e-4f);
        else {
            double error = 0, energy = 0, maximum = 0;
            for (size_t i = 0; i < actual.size(); ++i) {
                const double delta = (double)actual[i] - expected[i];
                error += delta * delta; energy += (double)expected[i] * expected[i];
                maximum = std::max(maximum, std::abs(delta));
            }
            const double relative = std::sqrt(error / energy);
            const bool passed = std::isfinite(relative) && relative < 2e-5 && maximum < 2e-4;
            ++g_checks; g_failures += passed ? 0 : 1;
            std::printf("  %s warp correlation large %s L2 %.3g max %.3g\n", passed ? "ok  " : "FAIL", half ? "fp16" : "fp32", relative, maximum);
        }
    }
}

void test_geometry(vk::Arena& arena) {
    {   // tanh / elu / silu -- the ConvGRU's gates, the normal head's
        // concentration channel, and giant2's SwiGLU.
        vk::ArenaScope scope(arena);
        const int N = 4096;
        auto x = randn(N, 3.0f);
        Tensor tx = upload_f32(arena, x, N);
        struct Case { Act act; const char* name; float (*ref)(float); };
        const Case cases[] = {
            {Act::Tanh, "tanh", [](float v) { return std::tanh(v); }},
            {Act::Elu,  "elu",  [](float v) { return v > 0 ? v : std::exp(v) - 1.0f; }},
            {Act::Silu, "silu", [](float v) { return v / (1.0f + std::exp(-v)); }},
            {Act::Exp,  "exp",  [](float v) { return std::exp(v); }},
        };
        for (const Case& c : cases) {
            Tensor to = arena_tensor(arena, DType::F32, N);
            unary(to, tx, c.act);
            std::vector<float> want(x.size());
            for (size_t i = 0; i < x.size(); ++i) want[i] = c.ref(x[i]);
            check(c.name, readback(to), want, 1e-5f);
        }
    }
    {   // AvgPool2d(3, stride=2, padding=1) -- pool2x between GRU resolutions.
        // count_include_pad=True is the whole point: every window divides by 9,
        // so an edge output is smaller than the mean of the taps it saw.
        vk::ArenaScope scope(arena);
        const int Hi = 15, Wi = 11, C = 4, k = 3, stride = 2, pad = 1;
        const int Ho = (Hi + 2 * pad - k) / stride + 1;
        const int Wo = (Wi + 2 * pad - k) / stride + 1;
        auto x = randn((size_t)Hi * Wi * C);
        Tensor to = arena_tensor(arena, DType::F32, Ho, Wo, C);
        avgpool(to, upload_f32(arena, x, Hi, Wi, C), k, stride, pad);
        std::vector<float> want((size_t)Ho * Wo * C, 0.0f);
        for (int y = 0; y < Ho; ++y)
            for (int xx = 0; xx < Wo; ++xx)
                for (int c = 0; c < C; ++c) {
                    double s = 0;
                    for (int dy = 0; dy < k; ++dy)
                        for (int dx = 0; dx < k; ++dx) {
                            const int sy = y * stride + dy - pad, sx = xx * stride + dx - pad;
                            if (sy < 0 || sy >= Hi || sx < 0 || sx >= Wi) continue;
                            s += x[((size_t)sy * Wi + sx) * C + c];
                        }
                    want[((size_t)y * Wo + xx) * C + c] = (float)(s / (k * k));
                }
        check("avgpool 3x3 s2 pad1", readback(to), want, 1e-5f);
    }
    {   // MaxPool2d(3, stride=2) -- AlexNet's. 15 is odd on purpose: the last
        // window ends on the last row, and a 14th would be left out.
        vk::ArenaScope scope(arena);
        const int Hi = 15, Wi = 12, C = 5, k = 3, stride = 2;
        const int Ho = (Hi - k) / stride + 1, Wo = (Wi - k) / stride + 1;
        auto x = randn((size_t)Hi * Wi * C);
        Tensor to = arena_tensor(arena, DType::F32, Ho, Wo, C);
        maxpool(to, upload_f32(arena, x, Hi, Wi, C), k, stride);
        std::vector<float> want((size_t)Ho * Wo * C);
        for (int y = 0; y < Ho; ++y)
            for (int xx = 0; xx < Wo; ++xx)
                for (int c = 0; c < C; ++c) {
                    float m = -INFINITY;
                    for (int dy = 0; dy < k; ++dy)
                        for (int dx = 0; dx < k; ++dx)
                            m = std::max(m, x[((size_t)(y * stride + dy) * Wi + xx * stride + dx) * C + c]);
                    want[((size_t)y * Wo + xx) * C + c] = m;
                }
        check("maxpool 3x3 s2", readback(to), want, 0.0f);
    }
    {   // resize_nearest at 7/2 -- the DPT read_0 upsample. The reference is
        // torch's src = floor(dst / scale) with the ASKED-FOR scale, which is
        // not the same as floor(dst * Hi / Ho) once Ho = floor(Hi * 7/2).
        vk::ArenaScope scope(arena);
        const int Hi = 9, Wi = 7, C = 3;
        const float s = 3.5f;
        const int Ho = (int)std::floor(Hi * s), Wo = (int)std::floor(Wi * s);
        auto x = randn((size_t)Hi * Wi * C);
        Tensor to = arena_tensor(arena, DType::F32, Ho, Wo, C);
        resize_nearest(to, upload_f32(arena, x, Hi, Wi, C), s, s);
        std::vector<float> want((size_t)Ho * Wo * C);
        for (int y = 0; y < Ho; ++y)
            for (int xx = 0; xx < Wo; ++xx) {
                const int sy = std::min((int)std::floor(y / s), Hi - 1);
                const int sx = std::min((int)std::floor(xx / s), Wi - 1);
                for (int c = 0; c < C; ++c)
                    want[((size_t)y * Wo + xx) * C + c] = x[((size_t)sy * Wi + sx) * C + c];
            }
        check("resize_nearest 7/2", readback(to), want, 0.0f);
    }
    {   // softmax_rows over 256 depth bins per pixel, and over the 9 neighbours
        // the convex upsampler weights. A row far from zero checks the
        // max-subtraction; without it exp() overflows and every weight is NaN.
        vk::ArenaScope scope(arena);
        for (int cols : {9, 256}) {
            const int rows = 257;
            auto x = randn((size_t)rows * cols, 4.0f);
            for (int c = 0; c < cols; ++c) x[(size_t)3 * cols + c] += 200.0f;
            Tensor to = arena_tensor(arena, DType::F32, rows, cols);
            softmax_rows(to, upload_f32(arena, x, rows, cols));
            std::vector<float> want((size_t)rows * cols);
            for (int r = 0; r < rows; ++r) {
                float m = -std::numeric_limits<float>::infinity();
                for (int c = 0; c < cols; ++c) m = std::fmax(m, x[(size_t)r * cols + c]);
                double sum = 0;
                for (int c = 0; c < cols; ++c) sum += std::exp(x[(size_t)r * cols + c] - m);
                for (int c = 0; c < cols; ++c)
                    want[(size_t)r * cols + c] =
                        (float)(std::exp(x[(size_t)r * cols + c] - m) / sum);
            }
            check(cols == 9 ? "softmax_rows 9" : "softmax_rows 256", readback(to), want,
                  1e-6f);
        }
    }
}

// ================
// Swin, Deformable DETR and BiRefNet (src/swin/, src/gdino/, src/birefnet/)
// ================

void test_segmentation_ops(vk::Arena& arena) {
    {   // Shifted windows: partition then unpartition-accumulate is x + x, and
        // the partition itself is torch.roll(-s) of the zero-padded map.
        vk::ArenaScope scope(arena);
        const int H = 11, W = 13, C = 3, ws = 4, sh = 2;
        const int nwh = (H + ws - 1) / ws, nww = (W + ws - 1) / ws;
        const int Hp = nwh * ws, Wp = nww * ws;
        auto x = randn((size_t)H * W * C);
        Tensor tx = upload_f32(arena, x, H, W, C);
        Tensor tw = arena_tensor(arena, DType::F32, nwh * nww, ws * ws, C);
        window_partition(tw, tx, H, W, C, ws, sh);
        std::vector<float> want((size_t)nwh * nww * ws * ws * C);
        for (int wi = 0; wi < nwh; ++wi)
            for (int wj = 0; wj < nww; ++wj)
                for (int wy = 0; wy < ws; ++wy)
                    for (int wx = 0; wx < ws; ++wx)
                        for (int c = 0; c < C; ++c) {
                            const int y = (wi * ws + wy + sh) % Hp;
                            const int xx = (wj * ws + wx + sh) % Wp;
                            want[((((size_t)wi * nww + wj) * ws + wy) * ws + wx) * C + c] =
                                (y < H && xx < W) ? x[((size_t)y * W + xx) * C + c] : 0.0f;
                        }
        check("window partition shifted", readback(tw), want, 1e-6f);
        Tensor tb = upload_f32(arena, x, H, W, C);
        window_unpartition(tb, tw, H, W, C, ws, sh, /*accumulate=*/true);
        std::vector<float> twice(x);
        for (float& v : twice) v *= 2.0f;
        check("window unpartition shifted +=", readback(tb), twice, 1e-6f);
    }
    {   // Window attention: the relative-position bias plus Swin's -100 mask.
        vk::ArenaScope scope(arena);
        const int B = 3, N = 16, H = 2, HD = 32, dim = H * HD;
        auto q = randn((size_t)B * N * dim), k = randn((size_t)B * N * dim),
             v = randn((size_t)B * N * dim);
        auto bias = randn((size_t)H * N * N);
        std::vector<int32_t> lab((size_t)B * N);
        for (int b = 0; b < B; ++b)
            for (int i = 0; i < N; ++i) lab[(size_t)b * N + i] = b == 0 ? 0 : (i * (b + 1)) % 3;
        Tensor tl = arena_tensor(arena, DType::I32, B, N);
        vk::Stream::get().upload(tl.ptr, lab.data(), lab.size() * 4);
        Tensor to = arena_tensor(arena, DType::F32, B, N, dim);
        AttnOpts o;
        o.n_heads = H;
        o.head_dim = HD;
        o.batch = B;
        o.bias_mode = AttnBias::Window;
        o.bias = upload_f32(arena, bias, (int64_t)H * N * N);
        o.labels = tl;
        Tensor tq = upload_f32(arena, q, B, N, dim), tk = upload_f32(arena, k, B, N, dim),
               tv = upload_f32(arena, v, B, N, dim);
        auto reference = [&](bool f16) {
            std::vector<float> want((size_t)B * N * dim);
            for (int b = 0; b < B; ++b) {
                std::vector<float> full((size_t)H * N * N);
                for (int h = 0; h < H; ++h)
                    for (int i = 0; i < N; ++i)
                        for (int j = 0; j < N; ++j)
                            full[((size_t)h * N + i) * N + j] =
                                bias[((size_t)h * N + i) * N + j] +
                                (lab[(size_t)b * N + i] != lab[(size_t)b * N + j] ? -100.0f
                                                                                  : 0.0f);
                auto sl = [&](const std::vector<float>& a) {
                    std::vector<float> r(a.begin() + (size_t)b * N * dim,
                                         a.begin() + (size_t)(b + 1) * N * dim);
                    return f16 ? round_f16(r) : r;
                };
                std::vector<float> ob;
                attn_reference(sl(q), sl(k), std::vector<float>(v.begin() + (size_t)b * N * dim,
                                                               v.begin() + (size_t)(b + 1) * N * dim),
                               N, N, H, HD, 1.0f / std::sqrt((float)HD), &full, AttnBias::Full,
                               ob);
                std::copy(ob.begin(), ob.end(), want.begin() + (size_t)b * N * dim);
            }
            return want;
        };
        set_coop_matrix_enabled(false);
        attention(to, tq, tk, tv, N, N, o);
        check("attn window bias + mask", readback(to), reference(false), 3e-4f);
        set_coop_matrix_enabled(true);
        if (coop_attn(HD)) {
            attention(to, tq, tk, tv, N, N, o);
            check("attn window bias + mask [coop]", readback(to), reference(true), 3e-4f);
        }
    }
    {   // patch_merge, odd edges included.
        vk::ArenaScope scope(arena);
        const int H = 5, W = 7, C = 3, H2 = 3, W2 = 4;
        auto x = randn((size_t)H * W * C);
        Tensor tx = upload_f32(arena, x, H, W, C);
        Tensor to = arena_tensor(arena, DType::F32, H2 * W2, 4 * C);
        patch_merge(to, tx, H, W, C);
        std::vector<float> want((size_t)H2 * W2 * 4 * C);
        const int dy[4] = {0, 1, 0, 1}, dx[4] = {0, 0, 1, 1};
        for (int i = 0; i < H2; ++i)
            for (int j = 0; j < W2; ++j)
                for (int qd = 0; qd < 4; ++qd)
                    for (int c = 0; c < C; ++c) {
                        const int y = 2 * i + dy[qd], xx = 2 * j + dx[qd];
                        want[((size_t)i * W2 + j) * 4 * C + qd * C + c] =
                            (y < H && xx < W) ? x[((size_t)y * W + xx) * C + c] : 0.0f;
                    }
        check("patch_merge", readback(to), want, 1e-6f);
    }
    for (BlockOrder order : {BlockOrder::ChannelMajor, BlockOrder::ColumnsThenRows}) {
        vk::ArenaScope scope(arena);
        const int H = 6, W = 8, C = 3, gh = 3, gw = 2, ph = H / gh, pw = W / gw;
        auto x = randn((size_t)H * W * C);
        Tensor tx = upload_f32(arena, x, H, W, C);
        Tensor to = arena_tensor(arena, DType::F32, ph, pw, C * gh * gw);
        blocks_to_channels(to, tx, H, W, C, gh, gw, order);
        std::vector<float> want((size_t)H * W * C);
        for (int y = 0; y < ph; ++y)
            for (int xx = 0; xx < pw; ++xx)
                for (int c = 0; c < C; ++c)
                    for (int gy = 0; gy < gh; ++gy)
                        for (int gx = 0; gx < gw; ++gx) {
                            const int k = order == BlockOrder::ChannelMajor
                                              ? (c * gh + gy) * gw + gx
                                              : (gx * gh + gy) * C + c;
                            want[((size_t)y * pw + xx) * C * gh * gw + k] =
                                x[((size_t)(gy * ph + y) * W + gx * pw + xx) * C + c];
                        }
        check(order == BlockOrder::ChannelMajor ? "blocks_to_channels (c gh gw)"
                                                : "blocks_to_channels (gw gh c)",
              readback(to), want, 1e-6f);
    }
    {   // mul_rows: a per-pixel gate over a channel-last map.
        vk::ArenaScope scope(arena);
        const int R = 37, C = 5;
        auto a = randn((size_t)R * C), g = randn((size_t)R);
        Tensor to = arena_tensor(arena, DType::F32, R, C);
        mul_rows(to, upload_f32(arena, a, R, C), upload_f32(arena, g, R, 1), Act::None);
        std::vector<float> want(a.size());
        for (int r = 0; r < R; ++r)
            for (int c = 0; c < C; ++c) want[(size_t)r * C + c] = a[(size_t)r * C + c] * g[r];
        check("mul_rows", readback(to), want, 1e-6f);
    }
    {   // Modulated deformable convolution (DCNv2): the ALIKED reference with a
        // per-tap scale.
        vk::ArenaScope scope(arena);
        const int Hi = 8, Wi = 10, Ci = 5, Co = 6, k = 3, pad = 1, Ho = Hi, Wo = Wi;
        auto x = randn((size_t)Hi * Wi * Ci);
        auto w = randn((size_t)Co * Ci * k * k, 0.3f);
        auto off = randn((size_t)Ho * Wo * 2 * k * k, 2.0f);
        auto msk = randn((size_t)Ho * Wo * k * k);
        Tensor to = arena_tensor(arena, DType::F32, Ho, Wo, Co);
        ConvOpts o;
        o.pad_y = o.pad_x = pad;
        o.act = Act::Relu;
        modulated_deform_conv2d(arena, to, upload_f32(arena, x, Hi, Wi, Ci),
                                upload_f32(arena, off, Ho, Wo, 2 * k * k),
                                upload_f32(arena, msk, Ho, Wo, k * k),
                                upload_f32(arena, w, Co, Ci * k * k), k, k, o);
        std::vector<float> want((size_t)Ho * Wo * Co);
        for (int yo = 0; yo < Ho; ++yo)
            for (int xo = 0; xo < Wo; ++xo)
                for (int co = 0; co < Co; ++co) {
                    double acc = 0.0;
                    for (int ci = 0; ci < Ci; ++ci)
                        for (int tap = 0; tap < k * k; ++tap) {
                            const size_t ob = ((size_t)yo * Wo + xo) * 2 * k * k + 2 * tap;
                            const float sy = (float)(yo + tap / k - pad) + off[ob];
                            const float sx = (float)(xo + tap % k - pad) + off[ob + 1];
                            acc += (double)w[((size_t)co * Ci + ci) * k * k + tap] *
                                   msk[((size_t)yo * Wo + xo) * k * k + tap] *
                                   sample_zero(x, Hi, Wi, Ci, ci, sy, sx);
                        }
                    want[((size_t)yo * Wo + xo) * Co + co] = (float)std::max(acc, 0.0);
                }
        check("modulated deform_conv2d", readback(to), want, 2e-4f);
    }
    for (int rd : {2, 4}) {
        // ms_deform_attn vs grid_sample(align_corners=False) per level, weighted
        // by a softmax over the level x point samples.
        vk::ArenaScope scope(arena);
        const int nq = 23, heads = 2, hd = 8, C = heads * hd, L = 3, P = 4;
        MsDeformLevels lv;
        lv.n = L;
        const int hs[3] = {9, 5, 3}, wsz[3] = {12, 6, 2};
        int len = 0;
        for (int l = 0; l < L; ++l) { lv.h[l] = hs[l]; lv.w[l] = wsz[l]; len += hs[l] * wsz[l]; }
        auto value = randn((size_t)len * C);
        auto offs = randn((size_t)nq * heads * L * P * 2, rd == 2 ? 2.0f : 1.5f);
        auto logits = randn((size_t)nq * heads * L * P);
        std::vector<float> refs((size_t)nq * rd);
        std::uniform_real_distribution<float> u01(0.0f, 1.0f);
        for (float& r : refs) r = u01(g_rng);
        Tensor to = arena_tensor(arena, DType::F32, nq, C);
        ms_deform_attn(to, upload_f32(arena, value, len, C),
                       upload_f32(arena, offs, (int64_t)offs.size()),
                       upload_f32(arena, logits, (int64_t)logits.size()),
                       upload_f32(arena, refs, nq, rd), lv, heads, P);
        std::vector<float> want((size_t)nq * C, 0.0f);
        for (int q = 0; q < nq; ++q)
            for (int h = 0; h < heads; ++h) {
                const float* lg = &logits[((size_t)q * heads + h) * L * P];
                double m = -1e30, sum = 0.0;
                for (int j = 0; j < L * P; ++j) m = std::max(m, (double)lg[j]);
                for (int j = 0; j < L * P; ++j) sum += std::exp(lg[j] - m);
                int start = 0;
                for (int l = 0; l < L; ++l) {
                    std::vector<float> lvl(value.begin() + (size_t)start * C,
                                           value.begin() + (size_t)(start + hs[l] * wsz[l]) * C);
                    for (int pt = 0; pt < P; ++pt) {
                        const size_t ob = ((((size_t)q * heads + h) * L + l) * P + pt) * 2;
                        const float* r = &refs[(size_t)q * rd];
                        float lx, ly;
                        if (rd == 4) {
                            lx = r[0] + offs[ob] / P * r[2] * 0.5f;
                            ly = r[1] + offs[ob + 1] / P * r[3] * 0.5f;
                        } else {
                            lx = r[0] + offs[ob] / wsz[l];
                            ly = r[1] + offs[ob + 1] / hs[l];
                        }
                        const double wgt = std::exp(lg[l * P + pt] - m) / sum;
                        for (int d = 0; d < hd; ++d)
                            want[(size_t)q * C + h * hd + d] +=
                                (float)(wgt * sample_zero(lvl, hs[l], wsz[l], C, h * hd + d,
                                                          ly * hs[l] - 0.5f, lx * wsz[l] - 0.5f));
                    }
                    start += hs[l] * wsz[l];
                }
            }
        check(rd == 2 ? "ms_deform_attn 2-d refs" : "ms_deform_attn 4-d refs", readback(to),
              want, 2e-4f);
    }
}

// ================
// Benchmark (test_ops --bench)
// ================
//
// The two kernels that decide SAM 3's frame time, at the exact shapes the model
// runs them at. Correctness above says the answer is right; this says whether a
// tiling change was worth it, without a 30-second end-to-end run in between.
//
// The last two attention rows are not model shapes: they hold the work fixed
// and vary only how many workgroups it spreads over, which is what showed that
// kernel to be occupancy-bound rather than bandwidth-bound.

template <typename F>
double timed(int iters, F&& body) {
    body();                       // warm up: compile the pipeline, fault the pages
    vk::Stream::get().sync();
    const double t0 = now_ms();
    for (int i = 0; i < iters; ++i) body();
    vk::Stream::get().sync();
    return (now_ms() - t0) / iters;
}


// ================
// Streamed quantized weights
// ================

float bf16_round_ref(float v) {
    uint32_t b;
    std::memcpy(&b, &v, 4);
    b += 0x7fffu + ((b >> 16) & 1u);
    b &= 0xffff0000u;
    std::memcpy(&v, &b, 4);
    return v;
}

void test_quantized(vk::Arena& arena) {
    static const float kLevels[16] = {
        -1.0f, -0.6961928009986877f, -0.5250730514526367f, -0.39491748809814453f,
        -0.28444138169288635f, -0.18477343022823334f, -0.09105003625154495f, 0.0f,
        0.07958029955625534f, 0.16093020141124725f, 0.24611230194568634f,
        0.33791524171829224f, 0.44070982933044434f, 0.5626170039176941f,
        0.7229568362236023f, 1.0f};
    for (int with_extra = 0; with_extra < 2; ++with_extra) {
        vk::ArenaScope scope(arena);
        const int N = 96, K = 320;
        std::uniform_int_distribution<int> byte(0, 255);
        std::vector<uint8_t> codes((size_t)N * K / 2);
        for (auto& c : codes) c = (uint8_t)byte(g_rng);
        std::vector<uint16_t> absmax((size_t)N * K / 64);
        std::vector<float> am(absmax.size());
        for (size_t i = 0; i < absmax.size(); ++i) {
            const float v = bf16_round_ref(std::fabs(randn(1, 0.05f)[0]) + 1e-3f);
            uint32_t b;
            std::memcpy(&b, &v, 4);
            absmax[i] = (uint16_t)(b >> 16);
            am[i] = v;
        }
        auto res = randn((size_t)N * K, 0.01f);
        auto scale = randn((size_t)N, 2.0f);

        Tensor tc = arena_tensor(arena, DType::U8, (int64_t)codes.size());
        vk::Stream::get().upload(tc.ptr, codes.data(), codes.size());
        Tensor ta = arena_tensor(arena, DType::F16, (int64_t)absmax.size());
        vk::Stream::get().upload(ta.ptr, absmax.data(), absmax.size() * 2);
        Tensor to = arena_tensor(arena, DType::F16, N, K);
        Nf4Opts o;
        if (with_extra) {
            o.residual = upload_f32(arena, res, N, K);
            o.row_scale = upload_f32(arena, scale, N);
        }
        dequant_nf4(to, tc, ta, o);

        std::vector<float> want((size_t)N * K);
        for (int n = 0; n < N; ++n)
            for (int k = 0; k < K; ++k) {
                const size_t e = (size_t)n * K + k;
                const uint8_t b = codes[e / 2];
                const int q = (e & 1) ? (b & 15) : (b >> 4);
                float v = bf16_round_ref(kLevels[q] * am[e / 64]);
                if (with_extra) v = (v + res[e]) * scale[n];
                want[e] = half_to_float(float_to_half(v));
            }
        check(with_extra ? "dequant_nf4 + lora + gate" : "dequant_nf4", readback(to), want,
              1e-6f);
    }
    {
        vk::ArenaScope scope(arena);
        const int n = 37, heads = 3, hd = 128, stride = 3 * heads * hd;
        auto x = randn((size_t)n * stride, 3.0f);
        auto w = randn((size_t)hd, 0.5f);
        Tensor tx = upload_f32(arena, x, n, stride);
        // k is the second third of a fused qkv row.
        rms_norm_heads(tx.offsetElems(heads * hd), upload_f32(arena, w, hd), n, heads, hd,
                       stride, 1e-6f);
        std::vector<float> want = x;
        for (int i = 0; i < n; ++i)
            for (int h = 0; h < heads; ++h) {
                const size_t o = (size_t)i * stride + heads * hd + (size_t)h * hd;
                double ss = 0;
                for (int c = 0; c < hd; ++c) ss += (double)x[o + c] * x[o + c];
                const double inv = 1.0 / std::sqrt(ss / hd + 1e-6);
                for (int c = 0; c < hd; ++c) want[o + c] = (float)(x[o + c] * inv) * w[c];
            }
        check("rms_norm_heads (strided k)", readback(tx), want, 1e-5f);
    }
    if (coop_matrix_enabled()) {
        // Activations past fp16's range: only the prescale keeps them finite.
        vk::ArenaScope scope(arena);
        const int M = 160, N = 256, K = 192;
        auto x = randn((size_t)M * K, 3e4f);
        auto w = round_f16(randn((size_t)N * K, 0.05f));
        Tensor to = arena_tensor(arena, DType::F32, M, N);
        LinearOpts o;
        o.x_scale = 1.0f / 256.0f;
        linear(to, upload_f32(arena, x, M, K), upload_f16(arena, w, N, K), o);
        std::vector<float> xs(x.size());
        for (size_t i = 0; i < x.size(); ++i)
            xs[i] = half_to_float(float_to_half(x[i] * o.x_scale)) / o.x_scale;
        std::vector<float> want((size_t)M * N);
        for (int m = 0; m < M; ++m)
            for (int nn_ = 0; nn_ < N; ++nn_) {
                double acc = 0;
                for (int k = 0; k < K; ++k)
                    acc += (double)xs[(size_t)m * K + k] * w[(size_t)nn_ * K + k];
                want[(size_t)m * N + nn_] = (float)acc;
            }
        check("gemm coop, x past fp16 range", readback(to), want, 2e-3f);
    }
    {
        vk::ArenaScope scope(arena);
        const int H = 9, W = 11, Ci = 5, Co = 7;
        auto x = randn((size_t)H * W * Ci);
        auto w = randn((size_t)Co * Ci * 9, 0.2f);
        auto r = randn((size_t)H * W * Co);
        Tensor to = arena_tensor(arena, DType::F32, H, W, Co);
        ConvOpts o;
        o.pad_y = o.pad_x = 1;
        o.residual = upload_f32(arena, r, H, W, Co);
        conv2d(arena, to, upload_f32(arena, x, H, W, Ci), upload_f32(arena, w, Co, Ci * 9), 3,
               3, o);
        std::vector<float> want((size_t)H * W * Co);
        for (int y = 0; y < H; ++y)
            for (int xx = 0; xx < W; ++xx)
                for (int co = 0; co < Co; ++co) {
                    double acc = r[((size_t)y * W + xx) * Co + co];
                    for (int ci = 0; ci < Ci; ++ci)
                        for (int ky = 0; ky < 3; ++ky)
                            for (int kx = 0; kx < 3; ++kx) {
                                const int sy = y + ky - 1, sx = xx + kx - 1;
                                if (sy < 0 || sy >= H || sx < 0 || sx >= W) continue;
                                acc += (double)x[((size_t)sy * W + sx) * Ci + ci] *
                                       w[(((size_t)co * Ci + ci) * 3 + ky) * 3 + kx];
                            }
                    want[((size_t)y * W + xx) * Co + co] = (float)acc;
                }
        check("conv2d 3x3 + residual", readback(to), want, 1e-4f);
    }
    {
        // A copy on the transfer queue, ordered after the stream's last write to
        // the destination and before its next read of it.
        const int n = 1 << 20;
        auto a = randn(n), b = randn(n);
        void* map = nullptr;
        const vk::DevicePtr host = vk::Allocator::get().allocHost((uint64_t)n * 4, &map, "test staging");
        std::memcpy(map, b.data(), (size_t)n * 4);
        const vk::DevicePtr dst = vk::Allocator::get().allocDeviceShared((uint64_t)n * 4, "test slot");
        Tensor td(dst, DType::F32, n);
        tensor_from_host(td, a.data(), n);
        vk::Arena out_arena("transfer test");
        out_arena.reserve((uint64_t)n * 8 + 4096);
        Tensor first = arena_tensor(out_arena, DType::F32, n);
        Tensor second = arena_tensor(out_arena, DType::F32, n);
        copy(first, td);
        vk::Stream::get().flush();
        {
            vk::Transfer xfer;
            const uint64_t ticket =
                xfer.copy({{dst, host, (uint64_t)n * 4}}, vk::Stream::get().lastSubmitted());
            xfer.streamWaits(ticket);
            copy(second, td);
            std::printf("  (transfer queue: %s)\n", xfer.overlaps() ? "yes" : "no, on the stream");
            check("transfer: before", readback(first), a, 0.0f);
            check("transfer: after", readback(second), b, 0.0f);
        }
        vk::Allocator::get().free(dst);
        vk::Allocator::get().free(host);
    }
}

void bench(vk::Arena& arena) {
    const bool coop = vk::Context::get().hasCoopMat();
    std::printf("\nGEMM  (fp16 weights, fp32 activations)\n");
    if (coop)
        std::printf("  %-22s %10s %10s %10s %10s\n", "shape", "fp32 ms", "TFLOP/s",
                    "coop ms", "TFLOP/s");
    else
        std::printf("  %-22s %10s %10s   (no cooperative matrix: %s)\n", "shape", "ms",
                    "TFLOP/s", vk::Context::get().coopMatReason().c_str());
    struct G { int64_t M, N, K; const char* name; };
    const G gemms[] = {
        {5184, 3072, 1024, "vit qkv"},
        {5184, 1024, 1024, "vit proj"},
        {5184, 4736, 1024, "vit mlp1"},
        {5184, 1024, 4736, "vit mlp2"},
        {5184, 2048,  256, "fusion ffn"},
        {4096, 4096, 4096, "square 4096"},
    };
    for (const auto& g : gemms) {
        vk::ArenaScope scope(arena);
        Tensor x = arena_tensor(arena, DType::F32, g.M, g.K);
        Tensor w = arena_tensor(arena, DType::F16, g.N, g.K);
        Tensor o = arena_tensor(arena, DType::F32, g.M, g.N);
        vk::Stream::get().fill(x.ptr, 0x3f000000u, (uint64_t)g.M * g.K * 4);
        vk::Stream::get().fill(w.ptr, 0x38003800u, (uint64_t)g.N * g.K * 2);
        const double flop = 2.0 * (double)g.M * g.N * g.K;
        set_coop_matrix_enabled(false);
        const double ms = timed(5, [&] { matmul_nt(o, x, w, 1.0f, Act::None); });
        if (!coop) {
            std::printf("  %-22s %10.2f %10.2f\n", g.name, ms, flop / (ms * 1e9));
            continue;
        }
        set_coop_matrix_enabled(true);
        const double cms = timed(5, [&] { matmul_nt(o, x, w, 1.0f, Act::None); });
        std::printf("  %-22s %10.2f %10.2f %10.2f %10.2f\n", g.name, ms,
                    flop / (ms * 1e9), cms, flop / (cms * 1e9));
    }
    set_coop_matrix_enabled(true);

    // `split` mirrors what the model gets: passing an arena lets the launcher
    // slice the key range when the shape does not produce enough workgroups.
    // The paired rows are the point of the table.
    std::printf("\nAttention\n");
    if (coop)
        std::printf("  %-26s %10s %10s %10s %10s\n", "shape", "fp32 ms", "TFLOP/s",
                    "coop ms", "TFLOP/s");
    else
        std::printf("  %-26s %10s %10s\n", "shape", "ms", "TFLOP/s");
    struct A { int B, nq, nk, H, HD; bool split; const char* name; };
    const A attns[] = {
        {9, 576,  576, 16,  64, false, "vit window"},
        {1, 5184, 5184, 16, 64, false, "vit global"},
        {1, 5184, 5184,  8,  32, false, "fusion self"},
        {1, 5184, 5184,  1, 256, false, "memory self"},
        {1, 5184, 5184,  1, 256, true,  "memory self  split-k"},
        {1, 4096, 28672, 1, 256, false, "sam2 memory cross"},
        {1, 4096, 28672, 1, 256, true,  "sam2 memory cross  split-k"},
        {1, 5184, 5184,  4, 256, false, "memory self x4 heads"},
        {1, 5184, 5184,  1,  64, false, "1 head d64"},
    };
    for (const auto& a : attns) {
        vk::ArenaScope scope(arena);
        const int64_t dim = (int64_t)a.H * a.HD;
        Tensor q = arena_tensor(arena, DType::F32, a.B, a.nq, dim);
        Tensor k = arena_tensor(arena, DType::F32, a.B, a.nk, dim);
        Tensor v = arena_tensor(arena, DType::F32, a.B, a.nk, dim);
        Tensor o = arena_tensor(arena, DType::F32, a.B, a.nq, dim);
        vk::Stream::get().fill(q.ptr, 0x3d800000u, (uint64_t)a.B * a.nq * dim * 4);
        vk::Stream::get().fill(k.ptr, 0x3d800000u, (uint64_t)a.B * a.nk * dim * 4);
        vk::Stream::get().fill(v.ptr, 0x3d800000u, (uint64_t)a.B * a.nk * dim * 4);
        AttnOpts ao;
        ao.n_heads = a.H;
        ao.head_dim = a.HD;
        ao.batch = a.B;
        if (a.split) ao.arena = &arena;
        const double flop = 4.0 * a.B * a.H * (double)a.nq * a.nk * a.HD;
        set_coop_matrix_enabled(false);
        const double ms = timed(5, [&] { attention(o, q, k, v, a.nq, a.nk, ao); });
        if (!coop) {
            std::printf("  %-26s %10.2f %10.2f\n", a.name, ms, flop / (ms * 1e9));
            continue;
        }
        set_coop_matrix_enabled(true);
        const double cms = timed(5, [&] { attention(o, q, k, v, a.nq, a.nk, ao); });
        std::printf("  %-26s %10.2f %10.2f %10.2f %10.2f\n", a.name, ms,
                    flop / (ms * 1e9), cms, flop / (cms * 1e9));
    }
}


// Vulkan against MPSGraph fp32 and fp16 at a 4096-token matcher's shapes and
// SAM 3's, with fp32 weights.
void bench_metal(vk::Arena& arena) {
    std::printf("\nMetal  (%s)\n", metal_status().c_str());
    std::printf("  %-22s %9s %8s %9s %8s %9s %8s\n", "shape", "vk ms", "TFLOP/s", "mps32 ms",
                "TFLOP/s", "mps16 ms", "TFLOP/s");
    auto row = [&](const char* name, double flop, const auto& body) {
        double ms[3];
        for (int arm = 0; arm < 3; ++arm) {
            set_metal_enabled(arm > 0);
            Fp16Scope half(arm == 2);
            ms[arm] = timed(5, body);
        }
        set_metal_enabled(false);
        std::printf("  %-22s %9.2f %8.2f %9.2f %8.2f %9.2f %8.2f\n", name, ms[0],
                    flop / (ms[0] * 1e9), ms[1], flop / (ms[1] * 1e9), ms[2], flop / (ms[2] * 1e9));
    };
    struct G { int64_t M, N, K; const char* name; };
    const G gemms[] = {
        {4096, 512, 512, "matcher q/k/v"},
        {4096, 1024, 1024, "matcher ffn0"},
        {4096, 512, 1024, "matcher ffn3"},
        {5184, 4736, 1024, "vit mlp1"},
        {4096, 4096, 4096, "square 4096"},
    };
    for (const auto& g : gemms) {
        vk::ArenaScope scope(arena);
        Tensor x = arena_tensor(arena, DType::F32, g.M, g.K);
        Tensor w = arena_tensor(arena, DType::F32, g.N, g.K);
        Tensor o = arena_tensor(arena, DType::F32, g.M, g.N);
        vk::Stream::get().fill(x.ptr, 0x3c000000u, (uint64_t)g.M * g.K * 4);
        vk::Stream::get().fill(w.ptr, 0x3c000000u, (uint64_t)g.N * g.K * 4);
        row(g.name, 2.0 * g.M * g.N * g.K, [&] { matmul_nt(o, x, w); });
    }
    struct A { int nq, nk, H, HD; const char* name; };
    const A attns[] = {
        {4096, 4096, 8, 64, "matcher attention"},
        {5184, 5184, 16, 64, "vit global"},
    };
    for (const auto& a : attns) {
        vk::ArenaScope scope(arena);
        const int64_t dim = (int64_t)a.H * a.HD;
        Tensor q = arena_tensor(arena, DType::F32, a.nq, dim);
        Tensor k = arena_tensor(arena, DType::F32, a.nk, dim);
        Tensor o = arena_tensor(arena, DType::F32, a.nq, dim);
        vk::Stream::get().fill(q.ptr, 0x3d800000u, (uint64_t)a.nq * dim * 4);
        vk::Stream::get().fill(k.ptr, 0x3d800000u, (uint64_t)a.nk * dim * 4);
        AttnOpts ao;
        ao.n_heads = a.H;
        ao.head_dim = a.HD;
        row(a.name, 4.0 * a.H * (double)a.nq * a.nk * a.HD,
            [&] { attention(o, q, k, k, a.nq, a.nk, ao); });
    }
}


// One op on the Metal path in its own process: MPSGraph rejects some shapes by
// aborting, which no in-process test survives. Exit 0 when the result is right.
int probe(vk::Arena& arena, const std::string& kind, const std::string& spec) {
    int a[5] = {0, 0, 0, 0, 0};
    const int n = std::sscanf(spec.c_str(), "%d,%d,%d,%d,%d", &a[0], &a[1], &a[2], &a[3], &a[4]);
    set_metal_enabled(true);
    vk::ArenaScope scope(arena);
    std::vector<float> got, want;
    MetalCounts c0 = metal_counts();
    if (kind == "--probe-attn" && n == 5) {
        const int B = a[0], nq = a[1], nk = a[2], H = a[3], D = a[4], dim = H * D;
        auto q = randn((size_t)B * nq * dim), k = randn((size_t)B * nk * dim),
             v = randn((size_t)B * nk * dim);
        Tensor to = arena_tensor(arena, DType::F32, B, nq, dim);
        AttnOpts o;
        o.n_heads = H;
        o.head_dim = D;
        o.batch = B;
        attention(to, upload_f32(arena, q, B, nq, dim), upload_f32(arena, k, B, nk, dim),
                  upload_f32(arena, v, B, nk, dim), nq, nk, o);
        got = readback(to);
        for (int b = 0; b < B; ++b) {
            auto sl = [&](const std::vector<float>& t, int64_t m) {
                return std::vector<float>(t.begin() + b * m * dim, t.begin() + (b + 1) * m * dim);
            };
            std::vector<float> ob;
            attn_reference(sl(q, nq), sl(k, nk), sl(v, nk), nq, nk, H, D,
                           1.0f / std::sqrt((float)D), nullptr, AttnBias::None, ob);
            want.insert(want.end(), ob.begin(), ob.end());
        }
    } else if (kind == "--probe-gemm" && n == 3) {
        const int M = a[0], N = a[1], K = a[2];
        auto x = randn((size_t)M * K), w = randn((size_t)N * K, 0.1f);
        Tensor to = arena_tensor(arena, DType::F32, M, N);
        // Explicit 2-D views: a trailing dim of 1 would otherwise drop to 1-D.
        Tensor tx(upload_f32(arena, x, (int64_t)M * K).ptr, DType::F32, M, K);
        Tensor tw(upload_f32(arena, w, (int64_t)N * K).ptr, DType::F32, N, K);
        matmul_nt(to, tx, tw);
        got = readback(to);
        want.resize((size_t)M * N);
        for (int m = 0; m < M; ++m)
            for (int j = 0; j < N; ++j) {
                double s = 0;
                for (int kk = 0; kk < K; ++kk) s += (double)x[(size_t)m * K + kk] * w[(size_t)j * K + kk];
                want[(size_t)m * N + j] = (float)s;
            }
    } else {
        std::printf("usage: --probe-attn B,nq,nk,H,D | --probe-gemm M,N,K\n");
        return 2;
    }
    const MetalCounts c1 = metal_counts();
    const double e = rel_l2(got, want);
    std::printf("probe %s %s: metal %llu, rel L2 %.3g\n", kind.c_str() + 8, spec.c_str(),
                (unsigned long long)(c1.attention - c0.attention + c1.gemm - c0.gemm), e);
    return e <= 1e-5 ? 0 : 1;
}

}  // namespace

// Host-only, so it runs on a machine with no device. The throwing case is the
// one worth a test: it used to be std::terminate, from the worker and from the
// inline chunk both.
void test_parallel_for() {
    std::atomic<int64_t> covered{0};
    parallel_for(1 << 16, [&](int64_t lo, int64_t hi) { covered += hi - lo; });
    ++g_checks;
    if (covered.load() != (1 << 16)) {
        ++g_failures;
        std::printf("  FAIL %-28s covered %lld of 65536\n", "parallel_for range",
                    (long long)covered.load());
    } else {
        std::printf("  ok   %-28s\n", "parallel_for range");
    }

    bool rethrown = false;
    try {
        parallel_for(1 << 16, [](int64_t, int64_t) {
            throw std::runtime_error("parallel_for");
        });
    } catch (const std::exception&) {
        rethrown = true;
    }
    ++g_checks;
    if (!rethrown) {
        ++g_failures;
        std::printf("  FAIL %-28s no exception reached the caller\n",
                    "parallel_for rethrow");
    } else {
        std::printf("  ok   %-28s\n", "parallel_for rethrow");
    }

    const unsigned automatic = parallel_threads();
    for (unsigned cap : {1u, 2u}) {
        std::thread configure([cap] { set_parallel_threads(cap); });
        configure.join();
        std::atomic<unsigned> calls{0};
        covered = 0;
        parallel_for(1 << 16, [&](int64_t lo, int64_t hi) {
            ++calls;
            covered += hi - lo;
        });
        const unsigned expected = std::min(cap, automatic);
        ++g_checks;
        if (configured_parallel_threads() != cap || parallel_threads() != expected ||
            calls.load() != expected || covered.load() != (1 << 16)) {
            ++g_failures;
            std::printf("  FAIL %-28s cap %u, threads %u, calls %u, covered %lld\n",
                        "parallel_for cap", cap, parallel_threads(), calls.load(),
                        (long long)covered.load());
        } else {
            std::printf("  ok   %-28s cap %u\n", "parallel_for cap", cap);
        }
    }
    set_parallel_threads(0);
    ++g_checks;
    if (configured_parallel_threads() != 0 || parallel_threads() != automatic) {
        ++g_failures;
        std::printf("  FAIL %-28s expected %u, got %u\n",
                    "parallel_for reset", automatic, parallel_threads());
    } else {
        std::printf("  ok   %-28s\n", "parallel_for reset");
    }
}

int main(int argc, char** argv) {
    bool bench_only = false, require_metal = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        bench_only |= a == "--bench";
        require_metal |= a == "--require-metal";
    }
    set_log_level(1);
    std::printf("Host threading\n");
    test_parallel_for();
    try {
        auto devices = vk::enumerate_devices();
        std::printf("Vulkan devices:\n");
        for (auto& d : devices)
            std::printf("  [%d] %-40s %-11s %5.1f GiB %s\n", d.index, d.name.c_str(),
                        d.type, d.vram_bytes / 1073741824.0,
                        d.usable ? "" : d.unusable_reason.c_str());
        vk::Context::get();
        std::printf("Metal as requested: %s\n", metal_status().c_str());
        set_metal_enabled(false);

        vk::Arena arena("test");
        arena.reserve(bench_only ? (768u << 20) : (256u << 20));

        for (int i = 1; i + 1 < argc; ++i) {
            const std::string a = argv[i];
            if (a == "--probe-attn" || a == "--probe-gemm") {
                const int rc = probe(arena, a, argv[i + 1]);
                vk::Stream::shutdown();
                vk::Pipelines::get().shutdown();
                vk::VramPool::get().releaseAll();
                return rc;
            }
        }
        if (bench_only) {
            bench(arena);
            set_metal_enabled(true);
            if (metal_enabled()) bench_metal(arena);
            set_metal_enabled(false);
            vk::Stream::shutdown();
            vk::Pipelines::get().shutdown();
            vk::VramPool::get().releaseAll();
            return 0;
        }

        std::printf("\nPointer span\n"); test_span(arena);
        test_external_handoff();
        std::printf("GEMM\n");         test_gemm(arena);
        std::printf("Normalization\n");  test_norm(arena);
        std::printf("Attention\n");      test_attention(arena);
        std::printf("RoPE\n");           test_rope(arena);
        std::printf("Convolution\n");    test_conv(arena);
        std::printf("Spatial / gather\n"); test_spatial(arena);
        std::printf("Learned frontend\n"); test_learned_frontend(arena);
        test_local_correlation(arena);
        std::printf("Monocular geometry\n"); test_geometry(arena);
        std::printf("Swin / DETR / BiRefNet\n"); test_segmentation_ops(arena);
        std::printf("Streamed quantized weights\n"); test_quantized(arena);

        // Every kernel that has a 64-bit-addressing twin, re-checked on that
        // path against the same references. Only the addressing differs, so a
        // disagreement here is an address the wide shim computes differently.
        if (vk::Context::get().hasInt64()) {
            std::printf("\nWide addressing\n");
            set_wide_index(WideIndex::Force);
            test_rope(arena);
            test_norm(arena);
            test_conv(arena);
            test_spatial(arena);
            test_learned_frontend(arena);
            test_local_correlation(arena);
            test_geometry(arena);
            test_segmentation_ops(arena);
            set_wide_index(WideIndex::Auto);
        } else {
            std::printf("\nWide addressing: SKIP (no shaderInt64)\n");
        }

        // The same suite again with MPSGraph carrying attention and the large
        // GEMMs, so every model's ops are covered on that path too.
        set_metal_enabled(true);
        if (metal_enabled()) {
            std::printf("\nMetal: %s\n", metal_status().c_str());
            const MetalCounts c0 = metal_counts();
            test_gemm(arena);
            test_attention(arena);
            test_conv(arena);
            test_spatial(arena);
            test_learned_frontend(arena);
            test_geometry(arena);
            test_segmentation_ops(arena);
            const MetalCounts c1 = metal_counts();
            char buf[96];
            std::snprintf(buf, sizeof buf, "%llu attention, %llu gemm",
                          (unsigned long long)(c1.attention - c0.attention),
                          (unsigned long long)(c1.gemm - c0.gemm));
            expect(c1.attention > c0.attention && c1.gemm > c0.gemm, "metal carried the suite",
                   buf);
            test_metal(arena);
        } else {
            std::printf("\nMetal: SKIP (%s)\n", metal_status().c_str());
            if (require_metal) expect(false, "metal available", metal_status().c_str());
        }
        set_metal_enabled(false);

        std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    } catch (const std::exception& e) {
        std::printf("EXCEPTION: %s\n", e.what());
        return 1;
    }
    vk::Stream::shutdown();
    vk::Pipelines::get().shutdown();
    vk::VramPool::get().releaseAll();
    return g_failures == 0 ? 0 : 1;
}
