// GPU op library vs. straightforward CPU references.
//
// Every kernel in src/nn/shaders is exercised here against an independent scalar
// implementation of the same math. This is the gate that lets the model layer
// treat nn::ops as trustworthy: when a mask comes out wrong, the question is
// which module misread the reference, not whether the matmul is broken.

#include "nn/core/Error.h"
#include "nn/core/Half.h"
#include "nn/core/Log.h"
#include "nn/Ops.h"
#include "nn/vk/Context.h"
#include "nn/vk/Stream.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
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
// RoPE
// ================

void test_rope(vk::Arena& arena) {
    vk::ArenaScope scope(arena);
    const int N = 36, H = 3, HD = 64, dim = H * HD;
    auto x = randn((size_t)N * dim);
    auto f = randn((size_t)N * (HD / 2) * 2);
    Tensor tx = upload_f32(arena, x, N, dim);
    rope(tx, upload_f32(arena, f, (int64_t)N * (HD / 2) * 2), H, HD, N);

    std::vector<float> want = x;
    for (int n = 0; n < N; ++n)
        for (int h = 0; h < H; ++h)
            for (int i = 0; i < HD / 2; ++i) {
                float c = f[((size_t)n * (HD / 2) + i) * 2 + 0];
                float s = f[((size_t)n * (HD / 2) + i) * 2 + 1];
                size_t b = (size_t)n * dim + h * HD + i * 2;
                float re = x[b], im = x[b + 1];
                want[b] = re * c - im * s;
                want[b + 1] = re * s + im * c;
            }
    check("rope 2d-axial", readback(tx), want, 1e-5f);
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
    {
        // ConvTranspose2d(k=2, s=2) through the GEMM + scatter path.
        vk::ArenaScope scope(arena);
        const int Hi = 7, Wi = 9, Ci = 10, Co = 6;
        auto x = randn((size_t)Hi * Wi * Ci);
        auto w = randn((size_t)Ci * Co * 4, 0.2f);  // [Cin, Cout, 2, 2]
        auto b = randn((size_t)Co);
        // Repack to [Cout*4, Cin] exactly as model/Weights.cpp does.
        std::vector<float> wp((size_t)Co * 4 * Ci);
        for (int ci = 0; ci < Ci; ++ci)
            for (int co = 0; co < Co; ++co)
                for (int i = 0; i < 2; ++i)
                    for (int j = 0; j < 2; ++j)
                        wp[((size_t)co * 4 + i * 2 + j) * Ci + ci] =
                            w[(((size_t)ci * Co + co) * 2 + i) * 2 + j];

        Tensor to = arena_tensor(arena, DType::F32, Hi * 2, Wi * 2, Co);
        conv_transpose2x2(arena, to, upload_f32(arena, x, Hi, Wi, Ci),
                          upload_f32(arena, wp, (int64_t)Co * 4, Ci),
                          upload_f32(arena, b, Co), Act::GeluErf);

        std::vector<float> want((size_t)Hi * 2 * Wi * 2 * Co);
        for (int y = 0; y < Hi; ++y)
            for (int xx = 0; xx < Wi; ++xx)
                for (int co = 0; co < Co; ++co)
                    for (int i = 0; i < 2; ++i)
                        for (int j = 0; j < 2; ++j) {
                            double s = b[co];
                            for (int ci = 0; ci < Ci; ++ci)
                                s += (double)x[((size_t)y * Wi + xx) * Ci + ci] *
                                     w[(((size_t)ci * Co + co) * 2 + i) * 2 + j];
                            want[((size_t)(y * 2 + i) * (Wi * 2) + xx * 2 + j) * Co + co] =
                                act_ref((float)s, Act::GeluErf);
                        }
        check("conv_transpose 2x2 s2", readback(to), want, 2e-4f);
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

}  // namespace

int main(int argc, char** argv) {
    const bool bench_only = argc > 1 && std::string(argv[1]) == "--bench";
    set_log_level(1);
    try {
        auto devices = vk::enumerate_devices();
        std::printf("Vulkan devices:\n");
        for (auto& d : devices)
            std::printf("  [%d] %-40s %-11s %5.1f GiB %s\n", d.index, d.name.c_str(),
                        d.type, d.vram_bytes / 1073741824.0,
                        d.usable ? "" : d.unusable_reason.c_str());
        vk::Context::get();

        vk::Arena arena("test");
        arena.reserve(bench_only ? (768u << 20) : (256u << 20));

        if (bench_only) {
            bench(arena);
            vk::Stream::shutdown();
            vk::Pipelines::get().shutdown();
            vk::VramPool::get().releaseAll();
            return 0;
        }

        std::printf("\nPointer span\n"); test_span(arena);
        std::printf("GEMM\n");         test_gemm(arena);
        std::printf("Normalization\n");  test_norm(arena);
        std::printf("Attention\n");      test_attention(arena);
        std::printf("RoPE\n");           test_rope(arena);
        std::printf("Convolution\n");    test_conv(arena);
        std::printf("Spatial / gather\n"); test_spatial(arena);
        std::printf("Learned frontend\n"); test_learned_frontend(arena);
        std::printf("Monocular geometry\n"); test_geometry(arena);

        // Every kernel that has a 64-bit-addressing twin, re-checked on that
        // path against the same references. Only the addressing differs, so a
        // disagreement here is an address the wide shim computes differently.
        if (vk::Context::get().hasInt64()) {
            std::printf("\nWide addressing\n");
            set_wide_index(WideIndex::Force);
            test_norm(arena);
            test_conv(arena);
            test_spatial(arena);
            test_learned_frontend(arena);
            test_geometry(arena);
            set_wide_index(WideIndex::Auto);
        } else {
            std::printf("\nWide addressing: SKIP (no shaderInt64)\n");
        }

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
