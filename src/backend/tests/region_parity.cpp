// region_parity -- the device region test (kernels/densify/RegionWeight.cu,
// shaders/region.slang) against its host mirror (data/RegionProgram.cpp,
// data/LabelField.cpp): the same random splats, the same field of labelled
// seeds and the same program must answer the same on every one of them, with
// and without the normal, and so must the leaf shapes the ROI editor draws.
// Self-checking, both backends.

#include <kernels/densify/Densify.cuh>

#include "data/LabelField.h"
#include "data/Region.h"
#include "data/RegionProgram.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using backend::MemcpyKind;

namespace {

template <typename T>
T* upload(const std::vector<T>& host) {
    T* d = (T*)backend::device_malloc(std::max<size_t>(1, host.size() * sizeof(T)));
    if (!host.empty())
        backend::memcpy_sync(d, host.data(), host.size() * sizeof(T), MemcpyKind::HostToDevice);
    return d;
}

template <typename T>
DeviceVector<T> dv(const void* p, int64_t n) {
    return DeviceVector<T>(std::make_tuple((uint64_t)p, (uint32_t)sizeof(T),
                                           std::vector<int64_t>{n, 1}));
}

DeviceVector<float4> dv4(const std::vector<float>& host, void*& keep) {
    keep = upload(host);
    return dv<float4>(keep, (int64_t)host.size() / 4);
}

int g_failures = 0;
void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

}  // namespace

int main() {
    using namespace spirula;
    std::mt19937 rng(17);
    std::uniform_real_distribution<float> u(-10.f, 10.f);
    std::normal_distribution<float> g(0.f, 1.f);

    // Two rooms either side of a wall at x = 0: seeds on the wall face into
    // room A (x < 0) only, seeds elsewhere face up, cameras are omni.
    const int n_seed = 4000;
    std::vector<float> xyz, dirs;
    std::vector<int32_t> lab;
    for (int i = 0; i < n_seed; i++) {
        float p[3], d[3];
        int l;
        if (i % 4 == 0) {   // the wall, seen from room A
            p[0] = 0.02f * g(rng); p[1] = u(rng); p[2] = u(rng) * 0.3f;
            d[0] = -1; d[1] = 0; d[2] = 0;
            l = 0;
        } else if (i % 4 == 1) {   // room A floor
            p[0] = -std::fabs(u(rng)) - 0.5f; p[1] = u(rng); p[2] = -3.f + 0.05f * g(rng);
            d[0] = 0; d[1] = 0; d[2] = 1;
            l = 0;
        } else if (i % 4 == 2) {   // room B floor, sparse
            if (i % 12 != 2) continue;
            p[0] = std::fabs(u(rng)) + 0.5f; p[1] = u(rng); p[2] = -3.f + 0.05f * g(rng);
            d[0] = 0; d[1] = 0; d[2] = 1;
            l = 1;
        } else {   // cameras
            const bool a = i % 8 == 3;
            p[0] = (a ? -1.f : 1.f) * (2.f + std::fabs(u(rng)) * 0.3f); p[1] = u(rng) * 0.5f; p[2] = 0.f;
            d[0] = d[1] = d[2] = 0;
            l = a ? 0 : 1;
        }
        xyz.insert(xyz.end(), p, p + 3);
        dirs.insert(dirs.end(), d, d + 3);
        lab.push_back(l);
    }
    auto field = std::make_shared<LabelField>(
        LabelField::build(xyz.data(), lab.data(), dirs.data(), (int64_t)lab.size(), 8));
    check(field->num_nodes() > 32, "field built with a real tree");

    // The program: room B's label, minus a sphere, plus a box.
    auto lr = std::make_shared<LabelRegion>(field, 1);
    auto sphere = std::make_shared<SphereRegion>();
    sphere->center[0] = 3; sphere->radius = 1.5;
    auto box = std::make_shared<BoxRegion>();
    box->center[0] = -5; box->half[0] = 1; box->half[1] = 4; box->half[2] = 4;
    const double R[9] = {0.8, 0.6, 0, -0.6, 0.8, 0, 0, 0, 1};
    for (int i = 0; i < 9; i++) box->R[i] = R[i];
    auto diff = std::make_shared<CsgRegion>();
    diff->op = CsgOp::Difference;
    diff->children = {lr, sphere};
    CsgRegion root;
    root.op = CsgOp::Union;
    root.children = {diff, box};
    RegionProgram prog;
    std::string err;
    check(compile_region(root, prog, err), "program compiles");
    check(prog.num_nodes() == 5, "five nodes: label sphere diff box union");

    // Random splats: thin ones near the wall, oriented either way, and a
    // spread through both rooms.
    const int64_t N = 20000;
    std::vector<float> means((size_t)N * 3), quats((size_t)N * 4), scales((size_t)N * 3);
    for (int64_t i = 0; i < N; i++) {
        const bool at_wall = i % 2 == 0;
        means[i * 3 + 0] = at_wall ? 0.05f * g(rng) : u(rng);
        means[i * 3 + 1] = u(rng);
        means[i * 3 + 2] = at_wall ? u(rng) * 0.3f : u(rng) * 0.4f;
        float q[4] = {g(rng), g(rng), g(rng), g(rng)};
        if (at_wall) { q[0] = 1; q[1] = 0; q[2] = 0; q[3] = 0; }   // x axis = first column
        const float nq = std::sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
        for (int k = 0; k < 4; k++) quats[i * 4 + k] = q[k] / nq;
        scales[i * 3 + 0] = at_wall ? -4.f : -1.f;
        scales[i * 3 + 1] = -1.f;
        scales[i * 3 + 2] = -1.5f;
    }
    // The cameras as a field with their index as label (both rooms).
    std::vector<float> cam_xyz;
    std::vector<int32_t> cam_lab;
    for (int i = 0; i < 40; i++) {
        cam_xyz.insert(cam_xyz.end(), {(i % 2 ? 2.5f : -2.5f) + 0.2f * g(rng), u(rng) * 0.5f, 0.f});
        cam_lab.push_back(i);
    }
    const LabelField cams = LabelField::build(cam_xyz.data(), cam_lab.data(), nullptr,
                                              (int64_t)cam_lab.size(), 4);

    // The shapes the ROI editor draws: an ellipsoid minus a star-shaped
    // prism (non-convex, its polygon spilling into a second payload node),
    // plus a tilted cylinder with a sphere taken out by complement.
    RegionProgram shapes;
    {
        auto ell = std::make_shared<EllipsoidRegion>();
        ell->center[0] = -2; ell->half[0] = 7; ell->half[1] = 5; ell->half[2] = 3;
        const double Re[9] = {0.8, 0.6, 0, -0.6, 0.8, 0, 0, 0, 1};
        for (int i = 0; i < 9; i++) ell->R[i] = Re[i];
        auto star = std::make_shared<PrismRegion>();
        star->center[0] = -3; star->half_height = 1.2;
        for (int k = 0; k < 14; k++) {
            const double a = k * 3.14159265358979 / 7, r = k % 2 ? 1.2 : 3.5;
            star->polygon.push_back(r * std::cos(a));
            star->polygon.push_back(r * std::sin(a));
        }
        auto cyl = std::make_shared<CylinderRegion>();
        cyl->center[0] = 5; cyl->half[0] = 3; cyl->half[1] = 2.5; cyl->half[2] = 6;
        const double Rc[9] = {1, 0, 0, 0, 0.6, 0.8, 0, -0.8, 0.6};
        for (int i = 0; i < 9; i++) cyl->R[i] = Rc[i];
        auto ball = std::make_shared<SphereRegion>();
        ball->center[0] = 5; ball->radius = 1.5;
        auto hole = std::make_shared<CsgRegion>();
        hole->op = CsgOp::Complement;
        hole->children = {ball};
        auto cut = std::make_shared<CsgRegion>();
        cut->op = CsgOp::Difference;
        cut->children = {ell, star};
        auto tube = std::make_shared<CsgRegion>();
        tube->op = CsgOp::Intersection;
        tube->children = {cyl, hole};
        CsgRegion all;
        all.op = CsgOp::Union;
        all.children = {cut, tube};
        check(compile_region(all, shapes, err), "shape program compiles");
        check(shapes.num_nodes() == 10, "ten nodes: two of them the star's polygon");
        int64_t wrong = 0;
        std::mt19937 r2(5);
        for (int i = 0; i < 20000; i++) {
            const double p[3] = {u(r2), u(r2), u(r2) * 0.5};
            wrong += program_contains(shapes, p) != all.contains(p);
        }
        std::printf("shape program vs regions: %lld of 20000 differ\n", (long long)wrong);
        check(wrong <= 2, "shape program matches the regions it came from");
    }

    void *k_prog, *k_bvh, *k_seeds, *k_cbvh, *k_cseeds, *k_shapes;
    DeviceVector<float4> d_prog = dv4(prog.nodes, k_prog);
    DeviceVector<float4> d_shapes = dv4(shapes.nodes, k_shapes);
    DeviceVector<float4> d_bvh = dv4(field->nodes, k_bvh);
    DeviceVector<float4> d_seeds = dv4(field->seeds, k_seeds);
    DeviceVector<float4> d_cbvh = dv4(cams.nodes, k_cbvh);
    DeviceVector<float4> d_cseeds = dv4(cams.seeds, k_cseeds);
    float3* d_means = (float3*)upload(means);
    float4* d_quats = (float4*)upload(quats);
    float3* d_scales = (float3*)upload(scales);
    float* d_w = (float*)backend::device_malloc((size_t)N * sizeof(float));

    for (int pass = 0; pass < 3; pass++) {
        const bool orient = pass == 1;
        const RegionProgram& hp = pass == 2 ? shapes : prog;
        region_weight_tensor(N, dv<float3>(d_means, N),
                             orient ? dv<float4>(d_quats, N) : DeviceVector<float4>(),
                             dv<float3>(d_scales, N), orient ? d_cbvh : DeviceVector<float4>(),
                             d_cseeds, pass == 2 ? d_shapes : d_prog, d_bvh, d_seeds, 1.0f, 1e-4f,
                             dv<float>(d_w, N));
        backend::device_synchronize();
        std::vector<float> w((size_t)N);
        backend::memcpy_sync(w.data(), d_w, (size_t)N * sizeof(float), MemcpyKind::DeviceToHost);
        int64_t mismatches = 0, inside = 0;
        for (int64_t i = 0; i < N; i++) {
            const double p[3] = {means[i * 3], means[i * 3 + 1], means[i * 3 + 2]};
            double n[3] = {0, 0, 0};
            const double* np = nullptr;
            if (orient) {
                // Host mirror of the kernel: nearest camera orients the short axis.
                const int cam = cams.label(p);
                const float* c = &cams.seeds[(size_t)cam * 4];
                const double to[3] = {c[0] - p[0], c[1] - p[1], c[2] - p[2]};
                splat_normal(&quats[i * 4], &scales[i * 3], to, n);
                np = n;
            }
            const bool host = program_contains(hp, p, np);
            inside += host;
            if (host != (w[(size_t)i] > 0.5f)) mismatches++;
        }
        const char* what = pass == 2 ? "shapes" : orient ? "oriented" : "plain";
        std::printf("pass %d (%s): inside %lld of %lld, mismatches %lld\n", pass, what,
                    (long long)inside, (long long)N, (long long)mismatches);
        // Ties at floating-point precision differ across compilers; a handful
        // out of twenty thousand is that, more is a real divergence.
        check(mismatches <= 5, std::string(what) + ": device matches host");
        check(inside > N / 10 && inside < N * 9 / 10, "both answers occur");
    }

    // The wall itself: a thin splat on the wall facing room B belongs to B
    // (its normal opposes the wall seeds), one facing A to A.
    {
        const double p[3] = {0.01, 0.0, 0.0};
        const double toB[3] = {1, 0, 0}, toA[3] = {-1, 0, 0};
        check(field->label(p, toB) == 1, "wall splat facing room B is B's");
        check(field->label(p, toA) == 0, "wall splat facing room A is A's");
        const double behind[3] = {0.6, 0.0, 0.0};
        check(field->label(behind) == 1, "air just behind the wall belongs to room B");
    }

    backend::device_free(d_w);
    if (g_failures == 0) std::printf("region_parity: OK\n");
    return g_failures ? 1 : 0;
}
