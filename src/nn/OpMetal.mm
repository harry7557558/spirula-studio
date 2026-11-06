#include "nn/OpMetal.h"

#include "nn/core/Error.h"
#include "nn/core/Log.h"
#include "nn/vk/Context.h"
#include "nn/vk/MetalInterop.h"

#include "core/Env.h"

#import <MetalPerformanceShadersGraph/MetalPerformanceShadersGraph.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

namespace nn {

namespace {

// On by default: measured faster at parity on every model that uses these ops
// (src/nn/README.md has the table).
constexpr bool kMetalDefault = true;
// Below this on either output axis the hand-off costs more than the GEMM.
constexpr int64_t kMinGemmEdge = 128;

int g_metal_override = -1;   // -1: $SS_NN_METAL; 0/1: set_metal_enabled()
bool g_fp16 = false;
MetalCounts g_counts;

int env_request() {
    const char* e = spirula::env("NN_METAL");
    if (!e || !e[0]) return -1;
    return std::strcmp(e, "0") != 0 ? 1 : 0;
}

bool requested() {
    if (g_metal_override >= 0) return g_metal_override != 0;
    const int r = env_request();
    return r < 0 ? kMetalDefault : r != 0;
}

struct Graph {
    uint64_t             generation = 0;
    MPSGraphExecutable*  exe = nil;
    std::vector<int>     order;   // exe.feedTensors[i] is our input order[i]
};

// LRU over shapes. One model needs a few dozen executables; a model whose
// shapes keep changing would otherwise grow this without bound.
constexpr size_t kMaxGraphs = 128;

struct Cache {
    std::list<std::string> order;   // most recent first
    std::unordered_map<std::string, std::pair<Graph, std::list<std::string>::iterator>> map;
};

// The executable for `key`, or a fresh empty slot to compile into.
Graph& cached(const std::string& key) {
    static Cache c;
    auto it = c.map.find(key);
    if (it != c.map.end()) {
        c.order.splice(c.order.begin(), c.order, it->second.second);
        return it->second.first;
    }
    if (c.map.size() >= kMaxGraphs) {
        c.map.erase(c.order.back());
        c.order.pop_back();
        ++g_counts.evictions;
    }
    c.order.push_front(key);
    return c.map.emplace(key, std::make_pair(Graph{}, c.order.begin())).first->second.first;
}

// The totals at exit, for SS_NN_LOG=2 runs of any model.
struct ExitReport {
    ~ExitReport() {
        if (g_counts.attention + g_counts.gemm == 0) return;
        NN_LOG_INFO("[nn] metal: %llu attention, %llu gemm, %llu compiles, %llu evictions\n",
                    (unsigned long long)g_counts.attention, (unsigned long long)g_counts.gemm,
                    (unsigned long long)g_counts.compiles,
                    (unsigned long long)g_counts.evictions);
    }
} g_exit_report;

// n = -1 leaves the length to each encode.
MPSGraphTensor* input(MPSGraph* g, NSMutableArray<MPSGraphTensor*>* ins,
                      NSMutableDictionary<MPSGraphTensor*, MPSGraphShapedType*>* feeds,
                      int64_t n, MPSDataType t) {
    MPSGraphTensor* p = [g placeholderWithShape:@[@(n)] dataType:t name:nil];
    feeds[p] = [[MPSGraphShapedType alloc] initWithShape:@[@(n)] dataType:t];
    [ins addObject:p];
    return p;
}

Graph compile(MPSGraph* g, NSArray<MPSGraphTensor*>* ins,
              NSDictionary<MPSGraphTensor*, MPSGraphShapedType*>* feeds, MPSGraphTensor* y) {
    Graph r;
    r.generation = vk::Context::generation();
    MPSGraphDevice* dev = [MPSGraphDevice deviceWithMTLDevice:vk::metal::device()];
    r.exe = [g compileWithDevice:dev feeds:feeds targetTensors:@[y] targetOperations:nil
            compilationDescriptor:nil];
    NN_CHECK(r.exe && r.exe.feedTensors.count == ins.count, "metal: graph compile failed");
    ++g_counts.compiles;
    for (MPSGraphTensor* f in r.exe.feedTensors) {
        const NSUInteger i = [ins indexOfObjectIdenticalTo:f];
        NN_CHECK(i != NSNotFound, "metal: compiled graph has a feed it was not given");
        r.order.push_back((int)i);
    }
    return r;
}

MPSDataType mps_type(DType t) {
    return t == DType::F16 ? MPSDataTypeFloat16 : MPSDataTypeFloat32;
}

// A tensor's bytes as a 1-D NDArray over the MTLBuffer behind them.
MPSGraphTensorData* wrap(const Tensor& t, int64_t n, const char* what) {
    const vk::metal::Range r = vk::metal::resolve(t.ptr, (uint64_t)n * dtype_size(t.dtype), what);
    MPSNDArrayDescriptor* d = [MPSNDArrayDescriptor descriptorWithDataType:mps_type(t.dtype)
                                                                     shape:@[@(n)]];
    MPSNDArray* a = nil;
    if (@available(macOS 15.0, *))
        a = [[MPSNDArray alloc] initWithBuffer:r.buffer offset:r.offset descriptor:d];
    NN_CHECK(a, "metal: no NDArray over %s", what);
    return [[MPSGraphTensorData alloc] initWithMPSNDArray:a];
}

bool overlaps(const Tensor& a, int64_t an, const Tensor& b, int64_t bn) {
    if (!a.valid() || !b.valid()) return false;
    const uint64_t a1 = a.ptr + (uint64_t)an * dtype_size(a.dtype);
    const uint64_t b1 = b.ptr + (uint64_t)bn * dtype_size(b.dtype);
    return a.ptr < b1 && b.ptr < a1;
}

// MPSGraph says nothing about a result aliasing an input, so an in-place op
// (the residual FFN) writes a graph-owned result and copies it out.
void run(const Graph& g, const std::vector<MPSGraphTensorData*>& ins, const Tensor& out,
         int64_t out_n, bool in_place, double work) {
    NSMutableArray<MPSGraphTensorData*>* feed = [NSMutableArray arrayWithCapacity:ins.size()];
    for (int i : g.order) [feed addObject:ins[(size_t)i]];
    MPSGraphTensorData* res = in_place ? nil : wrap(out, out_n, "output");
    const vk::metal::Range r = vk::metal::resolve(out.ptr, (uint64_t)out_n * 4, "output");
    std::string err;
    @try {
        MPSCommandBuffer* cb = vk::metal::section(work);
        if (!in_place) {
            [g.exe encodeToCommandBuffer:cb inputsArray:feed resultsArray:@[res]
                     executionDescriptor:nil];
        } else {
            NSArray<MPSGraphTensorData*>* tmp = [g.exe encodeToCommandBuffer:cb inputsArray:feed
                                                                resultsArray:nil
                                                         executionDescriptor:nil];
            [tmp[0].mpsndarray exportDataWithCommandBuffer:cb toBuffer:r.buffer
                                       destinationDataType:MPSDataTypeFloat32 offset:r.offset
                                                rowStrides:nil];
        }
    } @catch (NSException* e) {
        err = e.reason ? e.reason.UTF8String : "unknown";
    }
    NN_CHECK(err.empty(), "metal: MPSGraph encode failed: %s", err.c_str());
}

MPSGraphTensor* activate(MPSGraph* g, MPSGraphTensor* y, Act act) {
    auto c = [&](double v) { return [g constantWithScalar:v dataType:MPSDataTypeFloat32]; };
    switch (act) {
        case Act::None: return y;
        case Act::Relu: return [g reLUWithTensor:y name:nil];
        case Act::Sigmoid: return [g sigmoidWithTensor:y name:nil];
        case Act::Tanh: return [g tanhWithTensor:y name:nil];
        case Act::Exp: return [g exponentWithTensor:y name:nil];
        case Act::Silu:
            return [g multiplicationWithPrimaryTensor:y
                                      secondaryTensor:[g sigmoidWithTensor:y name:nil] name:nil];
        case Act::GeluErf: {
            MPSGraphTensor* e = [g erfWithTensor:[g multiplicationWithPrimaryTensor:y
                                                                     secondaryTensor:c(0.70710678118654752)
                                                                                name:nil]
                                           name:nil];
            MPSGraphTensor* h = [g multiplicationWithPrimaryTensor:y secondaryTensor:c(0.5) name:nil];
            return [g multiplicationWithPrimaryTensor:h
                                      secondaryTensor:[g additionWithPrimaryTensor:e
                                                                   secondaryTensor:c(1.0) name:nil]
                                                 name:nil];
        }
        case Act::GeluTanh: {
            MPSGraphTensor* y3 = [g multiplicationWithPrimaryTensor:
                                        [g multiplicationWithPrimaryTensor:y secondaryTensor:y name:nil]
                                                    secondaryTensor:y name:nil];
            MPSGraphTensor* in = [g multiplicationWithPrimaryTensor:
                                        [g additionWithPrimaryTensor:y
                                                     secondaryTensor:[g multiplicationWithPrimaryTensor:y3
                                                                                        secondaryTensor:c(0.044715)
                                                                                                   name:nil]
                                                                name:nil]
                                                    secondaryTensor:c(0.7978845608028654) name:nil];
            MPSGraphTensor* h = [g multiplicationWithPrimaryTensor:y secondaryTensor:c(0.5) name:nil];
            return [g multiplicationWithPrimaryTensor:h
                                      secondaryTensor:[g additionWithPrimaryTensor:[g tanhWithTensor:in name:nil]
                                                                   secondaryTensor:c(1.0) name:nil]
                                                 name:nil];
        }
        default: return nil;
    }
}

bool act_supported(Act a) {
    switch (a) {
        case Act::None: case Act::Relu: case Act::Sigmoid: case Act::Tanh: case Act::Exp:
        case Act::Silu: case Act::GeluErf: case Act::GeluTanh: return true;
        default: return false;
    }
}

bool usable() { return requested() && vk::metal::available(); }

}  // namespace

bool metal_enabled() { return usable(); }

void set_metal_enabled(bool on) { g_metal_override = on ? 1 : 0; }

std::string metal_status() {
    const int e = env_request();
    std::string asked = g_metal_override >= 0 ? "set_metal_enabled"
                        : e < 0             ? "default"
                                            : std::string("SS_NN_METAL=") + (e ? "1" : "0");
    if (!requested()) return "off (" + asked + ")";
    if (!vk::metal::available()) return "off: " + vk::metal::reason() + " (" + asked + ")";
    return "on (" + asked + ")" + (g_fp16 ? ", fp16 tier" : ", fp32");
}

MetalCounts metal_counts() {
    MetalCounts c = g_counts;
    const vk::metal::Stats s = vk::metal::stats();
    c.sections = s.sections;
    c.rotations = s.rotations;
    return c;
}

Fp16Scope::Fp16Scope(bool on) : prev_(g_fp16) { g_fp16 = g_fp16 || on; }
Fp16Scope::~Fp16Scope() { g_fp16 = prev_; }
bool fp16_active() { return g_fp16; }

bool metal_attention(const Tensor& out, const Tensor& q, const Tensor& k, const Tensor& v,
                     int64_t nq, int64_t nk, const AttnOpts& o) {
    const int64_t dim = (int64_t)o.n_heads * o.head_dim;
    auto packed = [&](int64_t s) { return s == 0 || s == dim; };
    // MPS's SDPA (macOS 26.6) aborts the process at head dim 96 ("exceeds available
    // stack space") and is wrong at 256 (rel L2 0.24 on 100 tokens; nn_ops_test).
    const bool pow2 = o.head_dim >= 16 && o.head_dim <= 128 && (o.head_dim & (o.head_dim - 1)) == 0;
    // A single score (B*H*nq*nk == 1) aborts MPSGraph's compiler ("MLIR pass
    // manager failed"), uncatchably; the probe in nn_ops_test --probe-attn maps the rest.
    if (!pow2 || (int64_t)o.batch * o.n_heads * nq * nk == 1) return false;
    if (o.bias_mode != AttnBias::None || !packed(o.q_stride) || !packed(o.k_stride) ||
        !packed(o.v_stride) || !packed(o.out_stride) || q.dtype != DType::F32 ||
        k.dtype != DType::F32 || v.dtype != DType::F32 || out.dtype != DType::F32 ||
        !usable())
        return false;
    @autoreleasepool {
        const float scale = o.scale > 0.0f ? o.scale : 1.0f / std::sqrt((float)o.head_dim);
        const bool half = g_fp16;
        const int64_t B = o.batch, H = o.n_heads, D = o.head_dim;
        char key[160];
        std::snprintf(key, sizeof key, "attn %lld %lld %lld %lld %lld %a %d", (long long)B,
                      (long long)nq, (long long)nk, (long long)H, (long long)D, scale, half);
        Graph& gr = cached(key);
        if (!gr.exe || gr.generation != vk::Context::generation()) {
            MPSGraph* g = [MPSGraph new];
            NSMutableArray<MPSGraphTensor*>* ins = [NSMutableArray array];
            NSMutableDictionary<MPSGraphTensor*, MPSGraphShapedType*>* feeds =
                [NSMutableDictionary dictionary];
            auto heads = [&](int64_t n) {
                MPSGraphTensor* t = input(g, ins, feeds, B * n * dim, MPSDataTypeFloat32);
                t = [g reshapeTensor:t withShape:@[@(B), @(n), @(H), @(D)] name:nil];
                t = [g transposeTensor:t dimension:1 withDimension:2 name:nil];
                return half ? [g castTensor:t toType:MPSDataTypeFloat16 name:nil] : t;
            };
            MPSGraphTensor* tq = heads(nq);
            MPSGraphTensor* tk = heads(nk);
            MPSGraphTensor* tv = heads(nk);
            MPSGraphTensor* y = nil;
            if (@available(macOS 15.0, *))
                y = [g scaledDotProductAttentionWithQueryTensor:tq keyTensor:tk valueTensor:tv
                                                          scale:scale name:nil];
            if (half) y = [g castTensor:y toType:MPSDataTypeFloat32 name:nil];
            y = [g transposeTensor:y dimension:1 withDimension:2 name:nil];
            y = [g reshapeTensor:y withShape:@[@(B * nq * dim)] name:nil];
            gr = compile(g, ins, feeds, y);
        }
        const std::vector<MPSGraphTensorData*> ins{wrap(q, B * nq * dim, "attention q"),
                                                   wrap(k, B * nk * dim, "attention k"),
                                                   wrap(v, B * nk * dim, "attention v")};
        const bool in_place = overlaps(out, B * nq * dim, q, B * nq * dim) ||
                              overlaps(out, B * nq * dim, k, B * nk * dim) ||
                              overlaps(out, B * nq * dim, v, B * nk * dim);
        run(gr, ins, out, B * nq * dim, in_place, 4.0 * B * H * (double)nq * nk * D);
        ++g_counts.attention;
    }
    return true;
}

bool metal_gemm(const Tensor& out, const Tensor& x, const Tensor& w_in, const Tensor& bias,
                const Tensor& residual, Act act, float alpha, int64_t x_row_stride,
                float x_scale) {
    const Tensor w = w_in.asMatrix();
    const int64_t M = out.rows(), N = out.cols(), K = w.cols();
    if (M < kMinGemmEdge || N < kMinGemmEdge || x.dtype != DType::F32 ||
        out.dtype != DType::F32 || (x_row_stride > 0 && x_row_stride != K) ||
        (bias.valid() && bias.dtype != DType::F32) ||
        (residual.valid() && residual.dtype != DType::F32) || !act_supported(act) || !usable())
        return false;
    @autoreleasepool {
        const bool half = g_fp16;
        char key[160];
        // M is left dynamic: the readout's row counts follow the keypoint count.
        // x_scale matters only where x is rounded to fp16, as in the Vulkan kernels.
        const float xs = half ? x_scale : 1.0f;
        std::snprintf(key, sizeof key, "gemm %lld %lld %d %d %d %d %a %a %d",
                      (long long)N, (long long)K, (int)w.dtype, bias.valid(), residual.valid(),
                      (int)act, alpha, xs, half);
        Graph& gr = cached(key);
        if (!gr.exe || gr.generation != vk::Context::generation()) {
            MPSGraph* g = [MPSGraph new];
            NSMutableArray<MPSGraphTensor*>* ins = [NSMutableArray array];
            NSMutableDictionary<MPSGraphTensor*, MPSGraphShapedType*>* feeds =
                [NSMutableDictionary dictionary];
            const MPSDataType op = half ? MPSDataTypeFloat16 : MPSDataTypeFloat32;
            MPSGraphTensor* tx = input(g, ins, feeds, -1, MPSDataTypeFloat32);
            MPSGraphTensor* tw = input(g, ins, feeds, N * K, mps_type(w.dtype));
            tx = [g reshapeTensor:tx withShape:@[@-1, @(K)] name:nil];
            tw = [g reshapeTensor:tw withShape:@[@(N), @(K)] name:nil];
            auto scalar = [&](double v) { return [g constantWithScalar:v dataType:MPSDataTypeFloat32]; };
            if (xs != 1.0f) tx = [g multiplicationWithPrimaryTensor:tx secondaryTensor:scalar(xs) name:nil];
            if (op != MPSDataTypeFloat32) tx = [g castTensor:tx toType:op name:nil];
            if (mps_type(w.dtype) != op) tw = [g castTensor:tw toType:op name:nil];
            MPSGraphTensor* y = [g matrixMultiplicationWithPrimaryTensor:tx
                                                         secondaryTensor:[g transposeTensor:tw dimension:0
                                                                                withDimension:1 name:nil]
                                                                    name:nil];
            if (half) y = [g castTensor:y toType:MPSDataTypeFloat32 name:nil];
            if (xs != 1.0f) y = [g multiplicationWithPrimaryTensor:y secondaryTensor:scalar(1.0 / xs) name:nil];
            if (alpha != 1.0f)
                y = [g multiplicationWithPrimaryTensor:y
                                       secondaryTensor:[g constantWithScalar:alpha
                                                                    dataType:MPSDataTypeFloat32]
                                                  name:nil];
            if (bias.valid()) {
                MPSGraphTensor* tb = input(g, ins, feeds, N, MPSDataTypeFloat32);
                y = [g additionWithPrimaryTensor:y
                                 secondaryTensor:[g reshapeTensor:tb withShape:@[@1, @(N)] name:nil]
                                            name:nil];
            }
            if (residual.valid()) {
                MPSGraphTensor* tr = input(g, ins, feeds, -1, MPSDataTypeFloat32);
                y = [g additionWithPrimaryTensor:y
                                 secondaryTensor:[g reshapeTensor:tr withShape:@[@-1, @(N)] name:nil]
                                            name:nil];
            }
            y = activate(g, y, act);
            y = [g reshapeTensor:y withShape:@[@-1] name:nil];
            gr = compile(g, ins, feeds, y);
        }
        std::vector<MPSGraphTensorData*> ins{wrap(x, M * K, "gemm x"), wrap(w, N * K, "gemm w")};
        if (bias.valid()) ins.push_back(wrap(bias, N, "gemm bias"));
        if (residual.valid()) ins.push_back(wrap(residual, M * N, "gemm residual"));
        const bool in_place = overlaps(out, M * N, x, M * K) || overlaps(out, M * N, w, N * K) ||
                              overlaps(out, M * N, bias, N) ||
                              overlaps(out, M * N, residual, M * N);
        run(gr, ins, out, M * N, in_place, 2.0 * M * N * K);
        ++g_counts.gemm;
    }
    return true;
}

}  // namespace nn
