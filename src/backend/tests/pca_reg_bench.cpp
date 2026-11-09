// Device time of the local-PCA regularizer's parts on a trained model's
// splats, per neighbour search; same source on both backends.
//
// ./pca_reg_bench <splat.ply | uniform:N> [k=16] [iters=20] [cov=0] [erank=0] [transparency=0]

#include <kernels/loss/PcaReg.cuh>

#include "checkpoint/SplatPly.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <tuple>
#include <vector>

using backend::MemcpyKind;

namespace {

template <typename T>
DeviceVector<T> dev(int64_t n, const void* host = nullptr) {
    T* p = (T*)backend::device_malloc(std::max<size_t>(16, (size_t)n * sizeof(T)));
    if (host) backend::memcpy_sync(p, host, (size_t)n * sizeof(T), MemcpyKind::HostToDevice);
    else backend::memset_sync(p, 0, (size_t)n * sizeof(T));
    return DeviceVector<T>(std::make_tuple((uint64_t)p, (uint32_t)sizeof(T),
                                           std::vector<int64_t>{n, 1}));
}

struct Timer {
    backend::Event *a = backend::event_create(true), *b = backend::event_create(true);
    void start() { backend::event_record(a); }
    double stop() {
        backend::event_record(b);
        backend::event_synchronize(b);
        return backend::event_elapsed_ms(a, b);
    }
};

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <splat.ply | uniform:N> [k=16] [iters=20] "
                     "[cov=0] [erank=0] [transparency=0]\n", argv[0]);
        return 2;
    }
    spirula::SplatCloud c;
    const std::string src = argv[1];
    if (src.rfind("uniform:", 0) == 0) {
        c.num = std::atoll(src.c_str() + 8);
        uint32_t h = 12345;
        auto u = [&] { h = h * 1664525u + 1013904223u; return (float)(h >> 8) / 16777216.0f; };
        for (int64_t i = 0; i < c.num; i++) {
            c.means.insert(c.means.end(), {u(), u(), u()});
            c.quats.insert(c.quats.end(), {1, 0, 0, 0});
            c.opacities.push_back(0.0f);
            c.scales.insert(c.scales.end(), {-5, -5, -6});
        }
    } else {
        c = spirula::read_splat_ply(src, false);
    }
    const int k = argc > 2 ? std::atoi(argv[2]) : 16;
    const int iters = argc > 3 ? std::atoi(argv[3]) : 20;
    const int64_t n = c.num;
    std::printf("%lld splats, k = %d\n", (long long)n, k);

    auto means = dev<float3>(n, c.means.data());
    auto quats = dev<float4>(n, c.quats.data());
    auto scales = dev<float3>(n, c.scales.data());
    auto opacities = dev<float>(n, c.opacities.data());
    const int covariance = argc > 4 ? std::atoi(argv[4]) : 0;
    const float erank = argc > 5 ? std::atof(argv[5]) : 0;
    const float transparency = argc > 6 ? std::atof(argv[6]) : 0;
    DeviceVector<int32_t> perm, ranges;
    DeviceVector<float4> sorted, nodes;
    DeviceVector<int4> meta;
    DeviceVector<uint32_t> box;
    auto nbrs = dev<int32_t>(n * k);
    auto vm = dev<float3>(n);
    auto vq = dev<float4>(n);
    auto vs = dev<float3>(n);
    auto vo = dev<float>(n);
    auto sums = dev<float>(5);
    const float w = 1.0f / (float)n;
    Timer t;

    auto pass = [&](PcaRegMode mode, bool store, bool sa = false) {
        pca_reg_neighborhood_tensor(n, (int)mode, k, 4 * (k + 1), means, quats, scales, opacities, covariance, perm,
                                    sorted, nodes, meta, ranges, nbrs, store,
                                    DeviceVector<float>(), false, sa, vm, vq, vs, vo, w,
                                    0.001f * w, 0.1f * w, w, erank * w, transparency * w, sums);
    };
    auto kept = dev<int32_t>(n);
    double order = 0, tree = 0, gather = 0;
    for (int i = 0; i <= iters; i++) {
        pool_begin_phase(PoolPhase::RasterBwd);
        t.start();
        std::tie(perm, sorted, box) = pca_reg_order_tensor(n, means, kept);
        order += i > 0 ? t.stop() : 0.0;
        t.start();
        std::tie(nodes, meta, ranges) = pca_reg_tree_tensor(n, sorted, box);
        tree += i > 0 ? t.stop() : 0.0;
        pool_begin_phase(PoolPhase::RasterBwd);
        t.start();
        pca_reg_gather_tensor(n, means, kept);
        gather += i > 0 ? t.stop() : 0.0;
    }
    pool_begin_phase(PoolPhase::RasterBwd);
    std::tie(perm, sorted, box) = pca_reg_order_tensor(n, means, DeviceVector<int32_t>());
    std::tie(nodes, meta, ranges) = pca_reg_tree_tensor(n, sorted, box);
    std::printf("order %.3f ms, tree %.3f ms, gather by stored order %.3f ms\n",
                order / iters, tree / iters, gather / iters);
    const struct {
        const char* name;
        PcaRegMode mode;
        bool store, sa;
    } passes[] = {
        {"tree search + store", PcaRegMode::Tree, true, false},
        {"tree search", PcaRegMode::Tree, false, false},
        {"tree search, agnostic", PcaRegMode::Tree, false, true},
        {"list (from the tree)", PcaRegMode::List, false, false},
        {"list, agnostic", PcaRegMode::List, false, true},
        {"window", PcaRegMode::Window, false, false},
        {"best of window + store", PcaRegMode::Best, true, false},
        {"best of window", PcaRegMode::Best, false, false},
        {"list (from the window)", PcaRegMode::List, false, false},
    };
    for (const auto& p : passes) {
        // Vulkan's submit budget can create a second pipeline after its first sample.
        for (int warmup = 0; warmup < 8; warmup++) pass(p.mode, p.store, p.sa);
        backend::device_synchronize();
        t.start();
        for (int i = 0; i < iters; i++) pass(p.mode, p.store, p.sa);
        std::printf("%-24s %.3f ms\n", p.name, t.stop() / iters);
    }
    // Recall of each approximate search against the tree's exact lists.
    auto lists = [&](PcaRegMode mode) {
        pass(mode, true);
        std::vector<int32_t> h((size_t)(n * k));
        backend::memcpy_sync(h.data(), nbrs.data_ptr(), h.size() * sizeof(int32_t),
                             MemcpyKind::DeviceToHost);
        return h;
    };
    const std::vector<int32_t> exact = lists(PcaRegMode::Tree);
    for (PcaRegMode mode : {PcaRegMode::Window, PcaRegMode::Best}) {
        const std::vector<int32_t> got = lists(mode);
        int64_t hit = 0;
        for (int64_t r = 0; r < n; r++) {
            const int32_t* a = &exact[r * k];
            const int32_t* b = &got[r * k];
            for (int i = 0; i < k; i++)
                for (int j = 0; j < k; j++)
                    if (b[i] == a[j] && b[i] >= 0) { hit++; break; }
        }
        std::printf("recall@%d of %s: %.3f\n", k, mode == PcaRegMode::Window ? "window" : "best of window",
                    (double)hit / (double)(n * k));
    }
    if (const char* err = backend::last_error()) {
        std::fprintf(stderr, "backend error: %s\n", err);
        return 1;
    }
    return 0;
}
