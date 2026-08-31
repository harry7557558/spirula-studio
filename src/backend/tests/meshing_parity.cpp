// Backend parity tool for the surface-meshing device seam
// (mesh/MeshingDevice.h) and the occupancy-moment rasterizer it renders with.
// The SAME source builds under both backends (see projection_parity.cpp):
//
//   CUDA build:   ./meshing_parity dump ref.bin
//   Vulkan build: ./meshing_parity compare ref.bin   (per device)
//
// Sections:
//   1. Activation: quat -> principal axes, log-scale -> 1/sigma^2, logit ->
//      opacity, SH DC -> base RGB, cutoff k2 / radius / keep flag.
//   2. LBVH build over the kept Gaussians: leaf AABBs + remapped Morton codes,
//      the Karras radix tree, and the bottom-up node AABBs. The Vulkan tree
//      merge runs in fmap_ordered u32 space and the CUDA one in float CAS
//      loops, so this is the section that would catch that diverging.
//   3. The point cloud, the occupancy field (static aggregation and the
//      camera-segment visual hull), edge bisection, the occupancy combine and
//      vertex coloring (both the plain and the render-fallback entry).
//   4. rasterize_moments_3dgut_fwd over a real projection + tile intersection,
//      with and without the DC color image.
//   5. The per-camera samplers (occupancy k-min, color, view density) over a
//      SYNTHETIC moments image -- deterministic input, so a difference here is
//      the sampler's and not the renderer's.
//   6. The triangle LBVH + visibility cull on a small synthetic mesh.
//
// Channels: activation, leaf AABBs, the point cloud and the combine are
// deterministic arithmetic and compare TIGHT. Everything that traverses the
// BVH accumulating exp/log (occupancy, bisection, color) or that depends on
// which Gaussian a near-tie sorted in front of is LOOSE with a violation-
// fraction cap. Integer outputs (keep flags, Morton codes, child links,
// visibility) compare as exact CODES, also with a cap: a last-ulp Morton
// difference reorders one leaf and rewrites a whole subtree's links.

#include <backend/tests/DistortionFixture.h>
#include <mesh/MeshingDevice.h>

#include <kernels/projection/ProjectionFwd.cuh>
#include <kernels/raster/RasterizationMomentsFwd.cuh>
#include <kernels/tile/IntersectTile.cuh>

#include <backend/common/SortScan.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <vector>
#include "backend/tests/ScreenRows.h"

using backend::MemcpyKind;

namespace {

constexpr int N = 4000;        // splats
constexpr int NQ = 3000;       // occupancy / color query points
constexpr int NCAM = 3;        // cameras for the visual-hull occupancy
constexpr uint32_t W = 160, H = 120;

template <typename T>
T* upload(const std::vector<T>& host) {
    T* d = (T*)backend::device_malloc(host.size() * sizeof(T));
    backend::memcpy_sync(d, host.data(), host.size() * sizeof(T),
                         MemcpyKind::HostToDevice);
    return d;
}

template <typename T>
T* alloc(size_t n) {
    return (T*)backend::device_malloc(n * sizeof(T));
}

TorchTensorView ttv(const void* p, std::vector<int64_t> shape) {
    return std::make_tuple((uint64_t)p, (uint32_t)4, std::move(shape));
}

void readback(std::vector<float>& acc, const float* d, int64_t n) {
    if (n <= 0) return;
    size_t off = acc.size();
    acc.resize(off + n);
    backend::memcpy_sync(acc.data() + off, d, n * sizeof(float),
                         MemcpyKind::DeviceToHost);
}

void readback_i32(std::vector<int32_t>& acc, const int32_t* d, int64_t n) {
    if (n <= 0) return;
    size_t off = acc.size();
    acc.resize(off + n);
    backend::memcpy_sync(acc.data() + off, d, n * sizeof(int32_t),
                         MemcpyKind::DeviceToHost);
}

// 64-bit values (Morton codes) as two int32 codes, so one channel covers them.
void readback_u64(std::vector<int32_t>& acc, const uint64_t* d, int64_t n) {
    if (n <= 0) return;
    std::vector<uint64_t> h(n);
    backend::memcpy_sync(h.data(), d, n * sizeof(uint64_t),
                         MemcpyKind::DeviceToHost);
    for (uint64_t v : h) {
        acc.push_back((int32_t)(uint32_t)v);
        acc.push_back((int32_t)(uint32_t)(v >> 32));
    }
}

int check_error() {
    if (const char* err = backend::last_error()) {
        std::fprintf(stderr, "backend error: %s\n", err);
        return 1;
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3 ||
        (std::strcmp(argv[1], "dump") && std::strcmp(argv[1], "compare"))) {
        std::fprintf(stderr, "usage: %s dump|compare <ref.bin>\n", argv[0]);
        return 2;
    }
    const bool dumping = std::strcmp(argv[1], "dump") == 0;

    std::mt19937 rng(260807u);
    auto uf = [&](float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    };

    std::vector<float> acc;    // tight floats
    std::vector<float> lacc;   // loose floats
    std::vector<int32_t> codes;

    // ---- host inputs: a blob of Gaussians around the origin, plus a handful
    //      of distant ones so the Morton coordinate remap is exercised ----
    std::vector<float> means(3 * N), quats(4 * N), logsc(3 * N), logit(N),
        fdc(3 * N);
    for (int i = 0; i < N; ++i) {
        float far = (i % 97 == 0) ? 40.0f : 1.0f;   // occasional outlier
        for (int a = 0; a < 3; ++a) means[3 * i + a] = uf(-1.f, 1.f) * far;
        for (int a = 0; a < 4; ++a) quats[4 * i + a] = uf(-1.f, 1.f);
        for (int a = 0; a < 3; ++a) logsc[3 * i + a] = uf(-3.5f, -1.5f);
        // straddle the keep threshold both ways
        logit[i] = uf(-6.f, 4.f);
        for (int a = 0; a < 3; ++a) fdc[3 * i + a] = uf(-1.5f, 1.5f);
    }

    float* d_means = upload(means);
    float* d_quats = upload(quats);
    float* d_logsc = upload(logsc);
    float* d_logit = upload(logit);
    float* d_fdc = upload(fdc);

    // === 1. Activation ===
    float3* d_mean = alloc<float3>(N);
    float3* d_ax0 = alloc<float3>(N);
    float3* d_ax1 = alloc<float3>(N);
    float3* d_ax2 = alloc<float3>(N);
    float3* d_invs2 = alloc<float3>(N);
    float3* d_gcol = alloc<float3>(N);
    float* d_opac = alloc<float>(N);
    float* d_radius = alloc<float>(N);
    float* d_k2 = alloc<float>(N);
    int* d_valid = alloc<int>(N);

    meshing::launch_activate(N, d_means, d_quats, d_logsc, d_logit, d_fdc,
                             d_mean, d_ax0, d_ax1, d_ax2, d_invs2, d_opac,
                             d_radius, d_k2, d_valid, d_gcol);
    backend::device_synchronize();
    if (check_error()) return 1;

    readback(acc, (const float*)d_mean, 3 * N);
    readback(acc, (const float*)d_ax0, 3 * N);
    readback(acc, (const float*)d_ax1, 3 * N);
    readback(acc, (const float*)d_ax2, 3 * N);
    readback(acc, (const float*)d_invs2, 3 * N);
    readback(acc, (const float*)d_gcol, 3 * N);
    readback(acc, d_opac, N);
    readback(acc, d_radius, N);
    readback(acc, d_k2, N);
    readback_i32(codes, d_valid, N);

    // ---- kept list + scene bounds (the host half of OccupancyEvaluator) ----
    std::vector<int> h_valid(N);
    backend::memcpy_sync(h_valid.data(), d_valid, N * sizeof(int),
                         MemcpyKind::DeviceToHost);
    std::vector<int> kept;
    float bmin[3] = {1e30f, 1e30f, 1e30f}, bmax[3] = {-1e30f, -1e30f, -1e30f};
    double csum[3] = {0, 0, 0};
    for (int i = 0; i < N; ++i) {
        if (!h_valid[i]) continue;
        kept.push_back(i);
        for (int a = 0; a < 3; ++a) {
            float m = means[3 * i + a];
            bmin[a] = std::min(bmin[a], m);
            bmax[a] = std::max(bmax[a], m);
            csum[a] += m;
        }
    }
    const int nk = (int)kept.size();
    if (nk < 2) {
        std::fprintf(stderr, "meshing_parity: degenerate input (%d kept)\n", nk);
        return 2;
    }
    double cmean[3] = {csum[0] / nk, csum[1] / nk, csum[2] / nk};
    double var = 0.0;
    for (int kp = 0; kp < nk; ++kp)
        for (int a = 0; a < 3; ++a) {
            double d = means[3 * kept[kp] + a] - cmean[a];
            var += d * d;
        }
    const float rel_scale =
        (float)std::max(1e-6, std::sqrt(var / (3.0 * nk)));
    int* d_kept = upload(kept);

    // === 2. LBVH build ===
    float3* d_leafMin = alloc<float3>(nk);
    float3* d_leafMax = alloc<float3>(nk);
    uint64_t* d_morton = alloc<uint64_t>(nk);
    int* d_iota = alloc<int>(nk);

    float3 remap_min, remap_inv_ext;
    {
        float rmin[3], rext[3];
        for (int a = 0; a < 3; ++a) {
            float lo = meshing::remap_coord(bmin[a], rel_scale);
            float hi = meshing::remap_coord(bmax[a], rel_scale);
            rmin[a] = lo;
            rext[a] = std::max(hi - lo, 1e-12f);
        }
        remap_min = make_float3(rmin[0], rmin[1], rmin[2]);
        remap_inv_ext =
            make_float3(1.0f / rext[0], 1.0f / rext[1], 1.0f / rext[2]);
    }
    meshing::launch_bvh_prep(nk, d_kept, d_mean, d_ax0, d_ax1, d_ax2, d_invs2,
                             d_k2, remap_min, remap_inv_ext, rel_scale,
                             d_leafMin, d_leafMax, d_morton, d_iota);
    backend::device_synchronize();
    if (check_error()) return 1;

    readback(acc, (const float*)d_leafMin, 3 * nk);
    readback(acc, (const float*)d_leafMax, 3 * nk);
    readback_u64(codes, d_morton, nk);

    uint64_t* d_morton_s = alloc<uint64_t>(nk);
    int* d_argsort = alloc<int>(nk);
    backend::DoubleBuffer<int64_t> keys((int64_t*)d_morton,
                                        (int64_t*)d_morton_s);
    backend::DoubleBuffer<int32_t> vals(d_iota, d_argsort);
    backend::sort_pairs(keys, vals, nk, 0, 63);
    backend::device_synchronize();
    if (check_error()) return 1;

    int2* d_internal = alloc<int2>(nk - 1);
    int* d_parent = alloc<int>(nk - 1);
    backend::memset_sync(d_parent, 0xff, (size_t)(nk - 1) * sizeof(int));
    meshing::launch_lbvh_internal(nk, (const uint64_t*)keys.current(),
                                  vals.current(), d_internal, d_parent);
    float3* d_nodeAABB = alloc<float3>(2 * (nk - 1));
    meshing::launch_lbvh_init_aabb(nk - 1, d_nodeAABB);
    meshing::launch_lbvh_aabb(nk, d_internal, d_parent, d_leafMin, d_leafMax,
                              d_nodeAABB);
    backend::device_synchronize();
    if (check_error()) return 1;

    readback_i32(codes, (const int32_t*)d_internal, 2 * (nk - 1));
    // Node boxes are a merge over a scheduling-dependent climb, and one
    // reordered leaf moves whole subtrees: loose.
    readback(lacc, (const float*)d_nodeAABB, 3 * 2 * (nk - 1));

    // === 3. Point cloud, occupancy, bisection, color ===
    float* d_cloud = alloc<float>((size_t)nk * 21);
    meshing::launch_pointcloud(nk, d_kept, d_mean, d_ax0, d_ax1, d_ax2,
                               d_invs2, d_k2, d_cloud);
    backend::device_synchronize();
    if (check_error()) return 1;
    readback(acc, d_cloud, (int64_t)nk * 21);

    // camera centers on a ring above the blob
    std::vector<float> campos(3 * NCAM);
    for (int c = 0; c < NCAM; ++c) {
        float th = 6.2831853f * (float)c / (float)NCAM;
        campos[3 * c + 0] = 3.0f * std::cos(th);
        campos[3 * c + 1] = 3.0f * std::sin(th);
        campos[3 * c + 2] = 2.0f;
    }
    float3* d_campos = (float3*)upload(campos);

    meshing::GpuScene scene{};
    scene.mean = d_mean;
    scene.ax0 = d_ax0;
    scene.ax1 = d_ax1;
    scene.ax2 = d_ax2;
    scene.invs2 = d_invs2;
    scene.opac = d_opac;
    scene.k2 = d_k2;
    scene.gcol = d_gcol;
    scene.kept = d_kept;
    scene.leafMin = d_leafMin;
    scene.leafMax = d_leafMax;
    scene.internal = d_internal;
    scene.nodeAABB = d_nodeAABB;
    scene.num_kept = nk;
    scene.campos = d_campos;
    scene.num_cameras = NCAM;
    scene.iso = 0.5f;

    // query points: a jittered lattice through the blob
    std::vector<float> qxyz(3 * NQ);
    for (int i = 0; i < NQ; ++i)
        for (int a = 0; a < 3; ++a) qxyz[3 * i + a] = uf(-1.2f, 1.2f);
    float* d_q = upload(qxyz);

    float* d_occ_s = alloc<float>(NQ);
    float* d_occ_d = alloc<float>(NQ);
    meshing::launch_occ(scene, d_q, NQ, d_occ_s, 0);
    meshing::launch_occ(scene, d_q, NQ, d_occ_d, 1);
    backend::device_synchronize();
    if (check_error()) return 1;
    readback(lacc, d_occ_s, NQ);
    readback(lacc, d_occ_d, NQ);

    // occ_combine is pure arithmetic on the two above -> tight
    {
        float* d_comb = alloc<float>(NQ);
        backend::memcpy_sync(d_comb, d_occ_d, NQ * sizeof(float),
                             MemcpyKind::DeviceToDevice);
        meshing::launch_occ_combine(NQ, d_comb, d_occ_s);
        backend::device_synchronize();
        if (check_error()) return 1;
        readback(lacc, d_comb, NQ);
        backend::device_free(d_comb);
    }

    // bisection over random cloud edges
    {
        const int NE = 2000;
        std::vector<int32_t> ea(NE), eb(NE);
        const int ncloud = nk * 7;
        for (int e = 0; e < NE; ++e) {
            ea[e] = (int32_t)(rng() % (uint32_t)ncloud);
            eb[e] = (int32_t)(rng() % (uint32_t)ncloud);
        }
        int32_t* d_ea = upload(ea);
        int32_t* d_eb = upload(eb);
        float* d_oa = alloc<float>(NE);
        float* d_ob = alloc<float>(NE);
        float* d_out = alloc<float>(3 * NE);
        // endpoint occupancies from the same field the bisection walks
        {
            std::vector<float> pa(3 * NE), pb(3 * NE);
            std::vector<float> h_cloud((size_t)ncloud * 3);
            backend::memcpy_sync(h_cloud.data(), d_cloud,
                                 h_cloud.size() * sizeof(float),
                                 MemcpyKind::DeviceToHost);
            for (int e = 0; e < NE; ++e)
                for (int a = 0; a < 3; ++a) {
                    pa[3 * e + a] = h_cloud[3 * ea[e] + a];
                    pb[3 * e + a] = h_cloud[3 * eb[e] + a];
                }
            float* d_pa = upload(pa);
            float* d_pb = upload(pb);
            meshing::launch_occ(scene, d_pa, NE, d_oa, 0);
            meshing::launch_occ(scene, d_pb, NE, d_ob, 0);
            backend::device_synchronize();
            backend::device_free(d_pa);
            backend::device_free(d_pb);
        }
        meshing::launch_bisect(scene, d_cloud, d_ea, d_eb, d_oa, d_ob, NE,
                               /*iters=*/3, /*dynamic=*/0, d_out);
        backend::device_synchronize();
        if (check_error()) return 1;
        readback(lacc, d_out, 3 * NE);
        backend::device_free(d_ea);
        backend::device_free(d_eb);
        backend::device_free(d_oa);
        backend::device_free(d_ob);
        backend::device_free(d_out);
    }

    // vertex color: the plain entry, then the render-path fallback over a
    // buffer where half the entries are already filled
    {
        float* d_rgb = alloc<float>(3 * NQ);
        meshing::launch_colorize(scene, d_q, NQ, d_rgb, 0);
        backend::device_synchronize();
        if (check_error()) return 1;
        readback(lacc, d_rgb, 3 * NQ);

        std::vector<float> seed(3 * NQ);
        for (int i = 0; i < NQ; ++i) {
            float v = (i % 2) ? 0.25f : -1.0f;   // -1 = "no view saw it"
            for (int a = 0; a < 3; ++a) seed[3 * i + a] = v;
        }
        backend::memcpy_sync(d_rgb, seed.data(), seed.size() * sizeof(float),
                             MemcpyKind::HostToDevice);
        meshing::launch_colorize_fallback(scene, d_q, NQ, d_rgb);
        backend::device_synchronize();
        if (check_error()) return 1;
        readback(lacc, d_rgb, 3 * NQ);
        backend::device_free(d_rgb);
    }

    // === 4. rasterize_moments_3dgut_fwd over a real projection ===
    std::vector<float> vm(16 * NCAM), intr(4 * NCAM);
    // One coefficient row per (tier, camera): the samplers below give each
    // camera a different tier; the projection and the cull pick one block.
    std::vector<float> dist = dist_fixture::distortion_rows(NCAM);
    for (int c = 0; c < NCAM; ++c) {
        // identity rotation, camera pushed back along -z
        float* m = &vm[16 * c];
        for (int k = 0; k < 16; ++k) m[k] = 0.0f;
        m[0] = m[5] = m[10] = m[15] = 1.0f;
        m[3] = 0.15f * (float)c;
        m[11] = 4.0f + 0.5f * (float)c;
        float* in = &intr[4 * c];
        in[0] = 120.0f;
        in[1] = 120.0f;
        in[2] = 0.5f * (float)W;
        in[3] = 0.5f * (float)H;
    }
    float* d_vm = upload(vm);
    float* d_intr = upload(intr);
    float* d_dist = upload(dist);
    float* d_radii = alloc<float>(N);
    DeviceVector<float> radii(ttv(d_radii, {(int64_t)N, 1}));

    std::vector<DeviceTensorFloatND> in_splats = {
        DeviceTensorFloatND(ttv(d_means, {N, 3, 1})),
        DeviceTensorFloatND(ttv(d_quats, {N, 4, 1})),
        DeviceTensorFloatND(ttv(d_logsc, {N, 3, 1})),
        DeviceTensorFloatND(ttv(d_logit, {N, 1, 1})),
        DeviceTensorFloatND(ttv(d_fdc, {N, 3, 1})),
        DeviceTensorFloatND(ttv(d_fdc, {N, 3, 1})),   // dummy f_sh
    };

    const size_t npix = (size_t)W * H;
    float3* d_moments = alloc<float3>(npix);
    float3* d_rgbimg = alloc<float3>(npix);
    {
        const int cam = 0;
        backend::memset_sync(d_radii, 0, N * sizeof(float));
        auto [aabb_2d, depths_2d, splats_s] = projection_3dgut_forward(
            (int64_t)N, 0, in_splats, ttv(d_vm + 16 * cam, {1, 4, 4}),
            ttv(d_intr + 4 * cam, {1, 4}), W, H, "PINHOLE", "THIN_PRISM",
            ttv(d_dist + dist_fixture::row_offset(2, NCAM, cam),
                {1, kCameraDistortionParams}),
            radii, std::nullopt, std::nullopt, 0, 32, 0);
        DeviceTensorFloatND depths_nd(depths_2d);
        auto [isect_ids, flatten_ids, tile_offsets] = do_intersect_tile_generic(
            aabb_2d, depths_nd, ellipse_view(splats_s, true), 1,
            ttv(d_intr + 4 * cam, {1, 4}), W, H, nullptr, /*tile_active=*/nullptr);
        backend::device_synchronize();
        if (check_error()) return 1;

        // moments only, then moments + color
        rasterize_moments_3dgut_fwd(
            (int64_t)N, in_splats, splats_s, DeviceVector<int32_t>(),
            ttv(d_vm + 16 * cam, {1, 4, 4}), ttv(d_intr + 4 * cam, {1, 4}),
            "PINHOLE", "THIN_PRISM",
            ttv(d_dist + dist_fixture::row_offset(2, NCAM, cam),
                {1, kCameraDistortionParams}),
            aabb_2d, W, H, tile_offsets, flatten_ids, d_moments, nullptr);
        backend::device_synchronize();
        if (check_error()) return 1;
        readback(lacc, (const float*)d_moments, (int64_t)npix * 3);

        rasterize_moments_3dgut_fwd(
            (int64_t)N, in_splats, splats_s, DeviceVector<int32_t>(),
            ttv(d_vm + 16 * cam, {1, 4, 4}), ttv(d_intr + 4 * cam, {1, 4}),
            "PINHOLE", "THIN_PRISM",
            ttv(d_dist + dist_fixture::row_offset(2, NCAM, cam),
                {1, kCameraDistortionParams}),
            aabb_2d, W, H, tile_offsets, flatten_ids, d_moments, d_rgbimg);
        backend::device_synchronize();
        if (check_error()) return 1;
        readback(lacc, (const float*)d_rgbimg, (int64_t)npix * 3);
    }

    // === 5. Samplers over a SYNTHETIC moments image ===
    {
        std::vector<float> mom(npix * 3), rgbimg(npix * 3);
        for (size_t i = 0; i < npix; ++i) {
            // m0 sweeps 0 (sky: abstain) through 1, with a plausible linear
            // sqrt(-log T) fit behind it
            float m0 = (float)((i * 37) % 101) / 100.0f;
            mom[3 * i + 0] = m0;
            mom[3 * i + 1] = -0.3f + 0.02f * (float)(i % 17);
            mom[3 * i + 2] = 0.05f + 0.005f * (float)(i % 11);
            for (int a = 0; a < 3; ++a)
                rgbimg[3 * i + a] = (float)((i * (7 + a)) % 251) / 250.0f;
        }
        float3* d_mom_s = (float3*)upload(mom);
        float3* d_rgb_s = (float3*)upload(rgbimg);

        const int k = 2;
        float* d_kmin = alloc<float>((size_t)NQ * k);
        int* d_cnt = alloc<int>(NQ);
        {
            std::vector<float> big((size_t)NQ * k, 1e30f);
            backend::memcpy_sync(d_kmin, big.data(), big.size() * sizeof(float),
                                 MemcpyKind::HostToDevice);
            backend::memset_sync(d_cnt, 0, NQ * sizeof(int));
        }
        float* d_occ_k = alloc<float>(NQ);
        float3* d_num = alloc<float3>(NQ);
        float* d_den = alloc<float>(NQ);
        backend::memset_sync(d_num, 0, NQ * sizeof(float3));
        backend::memset_sync(d_den, 0, NQ * sizeof(float));
        float* d_rgb_out = alloc<float>(3 * NQ);
        float* d_dens = alloc<float>(NQ);
        backend::memset_sync(d_dens, 0, NQ * sizeof(float));

        for (int c = 0; c < NCAM; ++c) {
            const int cm = 0;   // PINHOLE
            // Thin prism is covered by the projection and the cull.
            const int td = c == 0 ? 0 : c == 1 ? 1 : 3;
            const float* d_dc =
                d_dist + dist_fixture::row_offset(td, NCAM, c);
            meshing::launch_sample_occ(d_q, NQ, d_vm + 16 * c, d_intr + 4 * c,
                                       d_dc, cm, td, d_mom_s, W, H, k,
                                       d_kmin, d_cnt);
            meshing::launch_sample_color(d_q, NQ, d_vm + 16 * c,
                                         d_intr + 4 * c, d_dc, cm, td,
                                         d_mom_s, d_rgb_s, W, H, d_num, d_den);
            meshing::launch_sample_view_density(
                d_q, NQ, d_vm + 16 * c, d_intr + 4 * c, d_dc, cm, td,
                d_mom_s, W, H, d_dens);
        }
        meshing::launch_finalize_occ(NQ, d_kmin, d_cnt, k, d_occ_k);
        meshing::launch_finalize_color(NQ, d_num, d_den, d_rgb_out);
        backend::device_synchronize();
        if (check_error()) return 1;

        readback(acc, d_occ_k, NQ);
        readback(acc, d_rgb_out, 3 * NQ);
        readback(acc, d_dens, NQ);
        readback_i32(codes, d_cnt, NQ);

        backend::device_free(d_mom_s);
        backend::device_free(d_rgb_s);
        backend::device_free(d_kmin);
        backend::device_free(d_cnt);
        backend::device_free(d_occ_k);
        backend::device_free(d_num);
        backend::device_free(d_den);
        backend::device_free(d_rgb_out);
        backend::device_free(d_dens);
    }

    // === 6. Triangle LBVH + visibility cull ===
    {
        // A closed-ish shell of random triangles over a sphere of points, so
        // some vertices really are occluded from some cameras.
        const int NV = 1200, NF = 2000;
        std::vector<float> verts(3 * NV);
        for (int i = 0; i < NV; ++i) {
            float th = uf(0.f, 6.2831853f), ph = uf(-1.5707f, 1.5707f);
            float r = 0.8f + 0.2f * uf(0.f, 1.f);
            verts[3 * i + 0] = r * std::cos(ph) * std::cos(th);
            verts[3 * i + 1] = r * std::cos(ph) * std::sin(th);
            verts[3 * i + 2] = r * std::sin(ph);
        }
        std::vector<int32_t> faces(3 * NF);
        for (int f = 0; f < NF; ++f) {
            int a = (int)(rng() % (uint32_t)NV);
            faces[3 * f + 0] = a;
            faces[3 * f + 1] = (a + 1 + (int)(rng() % 5u)) % NV;
            faces[3 * f + 2] = (a + 7 + (int)(rng() % 5u)) % NV;
        }
        float* d_verts = upload(verts);
        int32_t* d_faces = upload(faces);

        float bb[6] = {1e30f, 1e30f, 1e30f, -1e30f, -1e30f, -1e30f};
        for (int i = 0; i < NV; ++i)
            for (int a = 0; a < 3; ++a) {
                float x = verts[3 * i + a];
                bb[a] = std::min(bb[a], x);
                bb[3 + a] = std::max(bb[3 + a], x);
            }
        float3 tb_min = make_float3(bb[0], bb[1], bb[2]);
        float3 tb_ext = make_float3(1.0f / std::max(bb[3] - bb[0], 1e-12f),
                                    1.0f / std::max(bb[4] - bb[1], 1e-12f),
                                    1.0f / std::max(bb[5] - bb[2], 1e-12f));

        float3* t_leafMin = alloc<float3>(NF);
        float3* t_leafMax = alloc<float3>(NF);
        uint64_t* t_morton = alloc<uint64_t>(NF);
        int* t_iota = alloc<int>(NF);
        meshing::launch_tri_prep(NF, d_faces, d_verts, tb_min, tb_ext,
                                 t_leafMin, t_leafMax, t_morton, t_iota);
        backend::device_synchronize();
        if (check_error()) return 1;
        readback(acc, (const float*)t_leafMin, 3 * NF);
        readback(acc, (const float*)t_leafMax, 3 * NF);
        readback_u64(codes, t_morton, NF);

        uint64_t* t_morton_s = alloc<uint64_t>(NF);
        int* t_argsort = alloc<int>(NF);
        backend::DoubleBuffer<int64_t> tkeys((int64_t*)t_morton,
                                             (int64_t*)t_morton_s);
        backend::DoubleBuffer<int32_t> tvals(t_iota, t_argsort);
        backend::sort_pairs(tkeys, tvals, NF, 0, 63);

        int2* t_internal = alloc<int2>(NF - 1);
        int* t_parent = alloc<int>(NF - 1);
        backend::memset_sync(t_parent, 0xff, (size_t)(NF - 1) * sizeof(int));
        meshing::launch_lbvh_internal(NF, (const uint64_t*)tkeys.current(),
                                      tvals.current(), t_internal, t_parent);
        float3* t_nodeAABB = alloc<float3>(2 * (NF - 1));
        meshing::launch_lbvh_init_aabb(NF - 1, t_nodeAABB);
        meshing::launch_lbvh_aabb(NF, t_internal, t_parent, t_leafMin,
                                  t_leafMax, t_nodeAABB);
        backend::device_synchronize();
        if (check_error()) return 1;
        readback(lacc, (const float*)t_nodeAABB, 3 * 2 * (NF - 1));

        std::vector<int32_t> ws(NCAM, (int32_t)W), hs(NCAM, (int32_t)H);
        int32_t* d_W = upload(ws);
        int32_t* d_H = upload(hs);
        uint32_t* d_vis = alloc<uint32_t>(NV);
        meshing::launch_cull(
            d_verts, NV, d_faces, NF, d_vm, d_intr,
            d_dist + dist_fixture::row_offset(2, NCAM),
            d_W, d_H, /*camera_model=*/0, /*distortion=*/2, NCAM, t_leafMin,
            t_leafMax, t_internal, t_nodeAABB, d_vis);
        backend::device_synchronize();
        if (check_error()) return 1;
        readback_i32(codes, (const int32_t*)d_vis, NV);
    }

    // ---- dump / compare ----
    if (dumping) {
        std::ofstream f(argv[2], std::ios::binary);
        if (!f) {
            std::fprintf(stderr, "cannot write %s\n", argv[2]);
            return 2;
        }
        int64_t nf = (int64_t)acc.size(), nl = (int64_t)lacc.size(),
                nc = (int64_t)codes.size();
        f.write((const char*)&nf, 8);
        f.write((const char*)acc.data(), nf * 4);
        f.write((const char*)&nl, 8);
        f.write((const char*)lacc.data(), nl * 4);
        f.write((const char*)&nc, 8);
        f.write((const char*)codes.data(), nc * 4);
        std::printf("meshing_parity: wrote %lld tight, %lld loose, %lld codes\n",
                    (long long)nf, (long long)nl, (long long)nc);
        return 0;
    }

    std::ifstream f(argv[2], std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", argv[2]);
        return 2;
    }
    int64_t nf = 0, nl = 0, nc = 0;
    f.read((char*)&nf, 8);
    if (nf != (int64_t)acc.size()) {
        std::fprintf(stderr, "tight count mismatch: ref %lld vs got %zu\n",
                     (long long)nf, acc.size());
        return 1;
    }
    std::vector<float> ref(nf);
    f.read((char*)ref.data(), nf * 4);
    f.read((char*)&nl, 8);
    if (nl != (int64_t)lacc.size()) {
        std::fprintf(stderr, "loose count mismatch: ref %lld vs got %zu\n",
                     (long long)nl, lacc.size());
        return 1;
    }
    std::vector<float> lref(nl);
    f.read((char*)lref.data(), nl * 4);
    f.read((char*)&nc, 8);
    if (nc != (int64_t)codes.size()) {
        std::fprintf(stderr, "code count mismatch: ref %lld vs got %zu\n",
                     (long long)nc, codes.size());
        return 1;
    }
    std::vector<int32_t> refc(nc);
    f.read((char*)refc.data(), nc * 4);

    auto cmp_f = [](const std::vector<float>& got,
                    const std::vector<float>& want, int64_t& viol,
                    double& max_abs) {
        viol = 0;
        max_abs = 0;
        for (size_t i = 0; i < got.size(); i++) {
            bool gfin = std::isfinite(got[i]), wfin = std::isfinite(want[i]);
            if (!gfin || !wfin) {
                // +-1e30 sentinels (empty node boxes) are compared as values
                if (gfin != wfin) viol++;
                continue;
            }
            double d = std::fabs((double)got[i] - (double)want[i]);
            double tol = 5e-3 + 1e-3 * std::fabs((double)want[i]);
            max_abs = std::max(max_abs, d);
            if (d > tol) viol++;
        }
    };
    int64_t fviol = 0, lviol = 0;
    double fmax = 0, lmax = 0;
    cmp_f(acc, ref, fviol, fmax);
    cmp_f(lacc, lref, lviol, lmax);
    int64_t cviol = 0;
    for (int64_t i = 0; i < nc; i++)
        if (codes[i] != refc[i]) cviol++;

    double ffrac = nf ? (double)fviol / (double)nf : 0.0;
    double lfrac = nl ? (double)lviol / (double)nl : 0.0;
    double cfrac = nc ? (double)cviol / (double)nc : 0.0;
    std::printf(
        "meshing_parity: %lld tight floats (max_abs %.3g, violations %lld = "
        "%.5f%%), %lld loose floats (max_abs %.3g, violations %lld = %.5f%%), "
        "%lld codes (violations %lld = %.5f%%)\n",
        (long long)nf, fmax, (long long)fviol, 100.0 * ffrac, (long long)nl,
        lmax, (long long)lviol, 100.0 * lfrac, (long long)nc, (long long)cviol,
        100.0 * cfrac);
    bool pass = ffrac <= 1e-3 && lfrac <= 2e-2 && cfrac <= 2e-2;
    std::printf(pass ? "meshing_parity: PASSED\n" : "meshing_parity: FAILED\n");
    return pass ? 0 : 1;
}
