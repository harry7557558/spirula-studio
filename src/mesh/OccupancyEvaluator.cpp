/*
 * OccupancyEvaluator.cpp
 *
 * Host orchestration behind Meshing.h's OccupancyEvaluator: uploads and
 * activates the Gaussians, builds the LBVH over their support, picks the
 * camera subset, and drives the occupancy / bisection / color / visibility
 * queries.
 *
 * PORTABLE. Every device call goes through mesh/MeshingDevice.h (kernels) and
 * backend/api/BackendRuntime.h (allocation, copies, sort), so this file is the
 * ONE copy of the orchestration and both backends run it. The kernels
 * themselves are mesh/Meshing.cu (CUDA) and backend/vulkan/kernels/Meshing.cpp
 * (Vulkan).
 */

#include "mesh/Meshing.h"
#include "mesh/MeshLog.h"
#include "mesh/MeshingDevice.h"
#include "mesh/MeshingRaster.h"

#include "core/CameraModel.h"     // kCameraDistortionParams (portable spelling)
#include "core/SubmitBudget.h"

#include "backend/api/BackendRuntime.h"
#include "backend/common/SortScan.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace meshing {

namespace mmsg = spirula::i18n::msg::mesh;

std::string camera_models_summary(const int* models, int C) {
    std::string out;
    for (int m : std::set<int>(models, models + C))
        out += (out.empty() ? "" : "/") +
               std::string(camera_model_to_string((CameraModelType)m));
    return out;
}

void sync_checked(const char* stage) {
    backend::device_synchronize();
    if (const char* err = backend::last_error())
        throw std::runtime_error(std::string("meshing: GPU error during ") +
                                 stage + ": " + err);
}

namespace {

// Largest point range one occupancy / bisection / color launch covers. Windows
// resets the GPU when one runs past TdrDelay (2 s by default); the static field
// measured ~37 ns/point on an RTX 5070, and 9M splats make 63M points.
constexpr int kPointsPerLaunch = 1 << 19;

// fn(first, count) over [0, n) in ranges of at most `chunk`, checking each.
// Ranges start at 1/16 of it and grow as the budget learns the device: a
// chunk is 19 ms on an RTX 5070 and can be seconds on an iGPU.
template <typename Fn>
void for_ranges(int n, int chunk, const char* stage, Fn&& fn) {
    chunk = std::max(chunk, 1);
    spirula::SubmitBudget budget;
    for (int i = 0; i < n;) {
        const int m = (int)std::min<int64_t>(n - i, budget.chunk(chunk / 16, chunk));
        const auto t0 = std::chrono::steady_clock::now();
        fn(i, m);
        sync_checked(stage);
        budget.record(m, std::chrono::duration<double>(
                             std::chrono::steady_clock::now() - t0).count());
        i += m;
    }
}

// Owning device allocation. The meshing pipeline's buffers are one-shot
// (allocated at construction, freed with the evaluator), so they go straight
// to the backend allocator rather than through DevicePool -- which exists to
// recycle per-iteration training scratch, a pattern nothing here has.
template <typename T>
struct DBuf {
    T* p = nullptr;
    DBuf() = default;
    explicit DBuf(size_t n) { alloc(n); }
    DBuf(const DBuf&) = delete;
    DBuf& operator=(const DBuf&) = delete;
    ~DBuf() { reset(); }

    void alloc(size_t n) {
        reset();
        if (n == 0) return;
        p = (T*)backend::device_malloc_checked(n * sizeof(T), "meshing");
    }
    void reset() {
        if (p) backend::device_free(p);
        p = nullptr;
    }
    T* get() const { return p; }
    operator T*() const { return p; }
};

template <typename T>
void upload(T* dst, const T* src, size_t n) {
    if (n == 0) return;
    backend::memcpy_sync(dst, src, n * sizeof(T),
                         backend::MemcpyKind::HostToDevice);
}
template <typename T>
void download(T* dst, const T* src, size_t n) {
    if (n == 0) return;
    backend::memcpy_sync(dst, src, n * sizeof(T),
                         backend::MemcpyKind::DeviceToHost);
}

// ---------------------------------------------------------------------------
// Pick k representative cameras out of C.
//
// For small k, uniform-interval (file-order) sampling clusters poorly and is
// order-dependent. Instead run k-means (farthest-point seeding + Lloyd) on the
// camera centers and return the MEDOID of each cluster -- the real camera
// nearest the centroid. Medoids (not centroids) matter: averaging cameras that
// surround an object lands near the object center, a useless ray origin.
// Returns the selected positions interleaved (xyz), up to k of them.
// ---------------------------------------------------------------------------
std::vector<float> select_cameras_kmeans(
    const float* cam, int C, int k, int iters = 25,
    std::vector<int>* out_idx = nullptr
) {
    auto d2 = [&](int a, const float* c) {
        float dx = cam[3*a]-c[0], dy = cam[3*a+1]-c[1], dz = cam[3*a+2]-c[2];
        return dx*dx + dy*dy + dz*dz;
    };
    std::vector<float> cen(3 * k);
    // farthest-point (k-center greedy) seeding -- deterministic, good spread
    std::vector<float> mind(C, 1e30f);
    int pick = 0;
    for (int j = 0; j < k; ++j) {
        cen[3*j] = cam[3*pick]; cen[3*j+1] = cam[3*pick+1]; cen[3*j+2] = cam[3*pick+2];
        float best = -1.0f; int bi = 0;
        for (int c = 0; c < C; ++c) {
            float dd = d2(c, &cen[3*j]);
            if (dd < mind[c]) mind[c] = dd;
            if (mind[c] > best) { best = mind[c]; bi = c; }
        }
        pick = bi;
    }
    // Lloyd iterations
    std::vector<int> assign(C, 0);
    for (int it = 0; it < iters; ++it) {
        bool changed = false;
        for (int c = 0; c < C; ++c) {
            int best = 0; float bd = 1e30f;
            for (int j = 0; j < k; ++j) {
                float dd = d2(c, &cen[3*j]);
                if (dd < bd) { bd = dd; best = j; }
            }
            if (assign[c] != best) { assign[c] = best; changed = true; }
        }
        std::vector<double> sum(3 * k, 0.0);
        std::vector<int> cnt(k, 0);
        for (int c = 0; c < C; ++c) {
            int j = assign[c];
            sum[3*j] += cam[3*c]; sum[3*j+1] += cam[3*c+1]; sum[3*j+2] += cam[3*c+2];
            ++cnt[j];
        }
        for (int j = 0; j < k; ++j)
            if (cnt[j] > 0)
                for (int a = 0; a < 3; ++a) cen[3*j+a] = (float)(sum[3*j+a] / cnt[j]);
        if (!changed) break;
    }
    // medoid per non-empty cluster
    std::vector<int> medoid(k, -1);
    std::vector<float> mbest(k, 1e30f);
    for (int c = 0; c < C; ++c) {
        int j = assign[c];
        float dd = d2(c, &cen[3*j]);
        if (dd < mbest[j]) { mbest[j] = dd; medoid[j] = c; }
    }
    std::vector<float> out;
    for (int j = 0; j < k; ++j)
        if (medoid[j] >= 0) {
            int c = medoid[j];
            out.push_back(cam[3*c]); out.push_back(cam[3*c+1]); out.push_back(cam[3*c+2]);
            if (out_idx) out_idx->push_back(c);
        }
    return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
struct OccupancyEvaluator::Impl {
    MeshingConfig cfg;
    int N = 0;

    DBuf<float3> mean, ax0, ax1, ax2, invs2, gcol;
    DBuf<float> opac, radius, k2;
    DBuf<int> valid;

    DBuf<int> kept;
    int num_kept = 0;

    // LBVH
    DBuf<float3> leafMin, leafMax, nodeAABB;
    DBuf<int2> internal;

    DBuf<float3> campos;
    int num_cameras = 0;

    // Camera intrinsics for the rasterize-and-sample path. Host-side copies
    // kept for the lifetime of the evaluator (the caller's tensors may be
    // freed after construction). Empty when the static (LBVH) occupancy path
    // is in use.
    std::vector<float> cam_viewmats;  // [C*16] row-major world->cam
    std::vector<float> cam_intrins;   // [C*4]  fx, fy, cx, cy
    std::vector<float> cam_dist;      // [C*8] distortion coefficients
    std::vector<int> cam_widths;      // [C] per-camera image width
    std::vector<int> cam_heights;     // [C] per-camera image height
    bool use_render = false;          // cams.valid() && cameras present
    RenderContext* rctx = nullptr;    // built when use_render
    std::vector<int> render_cam_indices;  // camera subset rendered per batch

    ~Impl() {
        if (rctx) render_context_destroy(rctx);
    }

    bool render_path() const {
        return use_render && rctx && !render_cam_indices.empty();
    }

    GpuScene make_scene() const {
        GpuScene s{};
        s.mean = mean; s.ax0 = ax0; s.ax1 = ax1; s.ax2 = ax2; s.invs2 = invs2;
        s.opac = opac; s.k2 = k2; s.gcol = gcol;
        s.kept = kept; s.leafMin = leafMin; s.leafMax = leafMax;
        s.internal = internal; s.nodeAABB = nodeAABB; s.num_kept = num_kept;
        s.campos = campos; s.num_cameras = num_cameras;
        s.iso = cfg.iso;
        return s;
    }

    // The field the render path is not in use for: camera-segment when
    // cameras are present, else the static density aggregation.
    void occupancy_ranges(const float* d_xyz, int n, float* d_occ) const {
        const GpuScene s = make_scene();
        const int dynamic = num_cameras > 0 ? 1 : 0;
        for_ranges(n, kPointsPerLaunch, "occupancy", [&](int i, int m) {
            launch_occ(s, d_xyz + 3 * (size_t)i, m, d_occ + i, dynamic);
        });
    }

    // d_occ = combine(d_occ, static density term) over [0, n).
    void combine_static_occupancy(const float* d_xyz, int n, float* d_occ) const {
        const GpuScene s = make_scene();
        DBuf<float> d_s((size_t)std::min(n, kPointsPerLaunch));
        for_ranges(n, kPointsPerLaunch, "occupancy", [&](int i, int m) {
            launch_occ(s, d_xyz + 3 * (size_t)i, m, d_s, 0);
            launch_occ_combine(m, d_occ + i, d_s);
        });
    }
};

OccupancyEvaluator::OccupancyEvaluator(
    const float* means, const float* quats,
    const float* log_scales, const float* logit_opac, const float* features_dc,
    int num_splats,
    const float* cam_pos, int num_cameras,
    const CameraParams& cams,
    const MeshingConfig& cfg
) {
    impl_ = new Impl();
    impl_->cfg = cfg;
    impl_->N = num_splats;
    const int N = num_splats;

    // ---- upload + activate ----
    {
        DBuf<float> d_means((size_t)N*3), d_quats((size_t)N*4);
        DBuf<float> d_logsc((size_t)N*3), d_logit((size_t)N);
        DBuf<float> d_fdc((size_t)N*3);
        upload(d_means.get(), means, (size_t)N*3);
        upload(d_quats.get(), quats, (size_t)N*4);
        upload(d_logsc.get(), log_scales, (size_t)N*3);
        upload(d_logit.get(), logit_opac, (size_t)N);
        upload(d_fdc.get(), features_dc, (size_t)N*3);

        impl_->mean.alloc(N);  impl_->ax0.alloc(N);
        impl_->ax1.alloc(N);   impl_->ax2.alloc(N);
        impl_->invs2.alloc(N); impl_->opac.alloc(N);
        impl_->radius.alloc(N); impl_->k2.alloc(N);
        impl_->gcol.alloc(N);  impl_->valid.alloc(N);

        launch_activate(
            N, d_means, d_quats, d_logsc, d_logit, d_fdc,
            impl_->mean, impl_->ax0, impl_->ax1, impl_->ax2, impl_->invs2,
            impl_->opac, impl_->radius, impl_->k2, impl_->valid, impl_->gcol);
        sync_checked("activation");
    }

    // ---- kept list + scene bbox (host) ----
    std::vector<float> h_mean((size_t)N*3);
    std::vector<int> h_valid(N);
    download(h_mean.data(), (const float*)impl_->mean.get(), (size_t)N*3);
    download(h_valid.data(), impl_->valid.get(), (size_t)N);

    std::vector<int> kept; kept.reserve(N);
    float bmin[3]={1e30f,1e30f,1e30f}, bmax[3]={-1e30f,-1e30f,-1e30f};
    double csum[3]={0,0,0};
    for (int i = 0; i < N; ++i) {
        if (!h_valid[i]) continue;
        kept.push_back(i);
        for (int a = 0; a < 3; ++a) {
            float m = h_mean[3*i+a];
            bmin[a] = std::min(bmin[a], m); bmax[a] = std::max(bmax[a], m);
            csum[a] += m;
        }
    }
    impl_->num_kept = (int)kept.size();
    num_kept_ = impl_->num_kept;
    num_points_ = impl_->num_kept * 7;
    if (impl_->num_kept == 0)
        throw std::runtime_error("meshing: no Gaussians above the opacity threshold");

    // rel_scale ~ RMS spread of centroids (core scene scale for the remap)
    double cmean[3] = {csum[0]/impl_->num_kept, csum[1]/impl_->num_kept, csum[2]/impl_->num_kept};
    double var = 0.0;
    for (int kp = 0; kp < impl_->num_kept; ++kp) {
        int i = kept[kp];
        for (int a = 0; a < 3; ++a) { double d = h_mean[3*i+a] - cmean[a]; var += d*d; }
    }
    float rel_scale = (float)std::max(1e-6, std::sqrt(var / (3.0 * impl_->num_kept)));

    impl_->kept.alloc(impl_->num_kept);
    upload(impl_->kept.get(), kept.data(), (size_t)impl_->num_kept);
    if (cfg.verbose)
        mlog::out(mlog::Stage::Loading, mmsg::gaussians_kept,
                  {impl_->num_kept, N, num_points_, (double)rel_scale});

    // ---- LBVH build ----
    const int n = impl_->num_kept;
    impl_->leafMin.alloc(n);
    impl_->leafMax.alloc(n);
    {
        DBuf<uint64_t> d_morton((size_t)n);
        DBuf<int> d_iota((size_t)n);

        // remapped root bounds (remap is monotone per-axis, so remap of the
        // real bbox bounds the remapped centroids)
        float3 remap_min = make_float3(0,0,0), remap_inv_ext = make_float3(1,1,1);
        {
            float rmin[3], rext[3];
            for (int a = 0; a < 3; ++a) {
                float lo = remap_coord(bmin[a], rel_scale);
                float hi = remap_coord(bmax[a], rel_scale);
                rmin[a] = lo;
                rext[a] = std::max(hi - lo, 1e-12f);
            }
            remap_min = make_float3(rmin[0], rmin[1], rmin[2]);
            remap_inv_ext = make_float3(1.0f/rext[0], 1.0f/rext[1], 1.0f/rext[2]);
        }

        launch_bvh_prep(
            n, impl_->kept, impl_->mean, impl_->ax0, impl_->ax1, impl_->ax2,
            impl_->invs2, impl_->k2, remap_min, remap_inv_ext, rel_scale,
            impl_->leafMin, impl_->leafMax, d_morton, d_iota);

        if (n >= 2) {
            // sort (morton, iota) -> argsort. Morton codes are 63-bit, so the
            // signed key the shared sort takes orders them identically.
            DBuf<uint64_t> d_morton_s((size_t)n);
            DBuf<int> d_argsort((size_t)n);
            backend::DoubleBuffer<int64_t> keys((int64_t*)d_morton.get(),
                                                (int64_t*)d_morton_s.get());
            backend::DoubleBuffer<int32_t> vals(d_iota.get(), d_argsort.get());
            backend::sort_pairs(keys, vals, n, 0, 63);

            impl_->internal.alloc(n - 1);
            DBuf<int> d_parent((size_t)(n - 1));
            backend::memset_sync(d_parent.get(), 0xff, sizeof(int)*(size_t)(n - 1));
            launch_lbvh_internal(n, (const uint64_t*)keys.current(),
                                 vals.current(), impl_->internal, d_parent);

            impl_->nodeAABB.alloc((size_t)2*(n - 1));
            launch_lbvh_init_aabb(n - 1, impl_->nodeAABB);
            launch_lbvh_aabb(n, impl_->internal, d_parent,
                             impl_->leafMin, impl_->leafMax, impl_->nodeAABB);
            sync_checked("LBVH build");
        }
    }

    // ---- cameras: keep all, else select max_cameras representatives ----
    if (cam_pos != nullptr && num_cameras > 0) {
        std::vector<float> sel_pos;
        std::vector<int>& sel = impl_->render_cam_indices;
        auto max_cameras = cfg.max_cameras;
        if (max_cameras <= 0)
            max_cameras = num_cameras;
        if (num_cameras <= max_cameras) {
            sel_pos.assign(cam_pos, cam_pos + 3 * num_cameras);
            sel.resize(num_cameras);
            for (int c = 0; c < num_cameras; ++c) sel[c] = c;
        } else {
            sel_pos = select_cameras_kmeans(cam_pos, num_cameras, max_cameras, 25, &sel);
        }
        impl_->num_cameras = (int)(sel_pos.size()/3);
        impl_->campos.alloc(impl_->num_cameras);
        upload((float*)impl_->campos.get(), sel_pos.data(), sel_pos.size());
        if (cfg.verbose)
            mlog::out(mlog::Stage::Loading, mmsg::cameras_selected,
                      {impl_->num_cameras, num_cameras});
    }

    // ---- camera intrinsics (rasterize-and-sample path) ----
    if (cams.valid() && num_cameras > 0) {
        impl_->cam_viewmats.assign(cams.viewmats, cams.viewmats + (size_t)num_cameras * 16);
        impl_->cam_intrins.assign(cams.intrins, cams.intrins + (size_t)num_cameras * 4);
        const size_t ndist = (size_t)num_cameras * kCameraDistortionParams;
        impl_->cam_dist.assign(ndist, 0.0f);
        if (cams.dist_coeffs)
            impl_->cam_dist.assign(cams.dist_coeffs, cams.dist_coeffs + ndist);
        impl_->cam_widths.assign(cams.widths, cams.widths + num_cameras);
        impl_->cam_heights.assign(cams.heights, cams.heights + num_cameras);
        impl_->use_render = true;
        impl_->rctx = render_context_create(
            means, quats, log_scales, logit_opac, features_dc, N,
            impl_->cam_viewmats.data(), impl_->cam_intrins.data(),
            impl_->cam_dist.empty() ? nullptr : impl_->cam_dist.data(),
            num_cameras, impl_->cam_widths.data(), impl_->cam_heights.data(),
            cams.camera_models, cams.distortions, cfg.carve_k, cfg.verbose);
        if (cfg.verbose) {
            int w0 = impl_->cam_widths[0], h0 = impl_->cam_heights[0];
            bool uniform = true;
            for (int c = 1; c < num_cameras; ++c)
                if (impl_->cam_widths[c] != w0 || impl_->cam_heights[c] != h0) { uniform = false; break; }
            const std::string models =
                camera_models_summary(cams.camera_models, num_cameras);
            if (uniform)
                mlog::out(mlog::Stage::Loading, mmsg::intrinsics_uniform,
                          {w0, h0, models});
            else
                mlog::out(mlog::Stage::Loading, mmsg::intrinsics_varied,
                          {models});
        }
    }
}

OccupancyEvaluator::~OccupancyEvaluator() { delete impl_; }

bool OccupancyEvaluator::render_moments(
    int cam_idx, std::vector<float>& out, int& width, int& height
) {
    if (!impl_->rctx) return false;
    width = render_context_width(impl_->rctx, cam_idx);
    height = render_context_height(impl_->rctx, cam_idx);
    size_t npix = (size_t)width * height;
    DBuf<float3> d_mom(npix);
    render_camera_moments(impl_->rctx, cam_idx, d_mom.get());
    out.resize(npix * 3);
    download(out.data(), (const float*)d_mom.get(), npix * 3);
    return true;
}

void OccupancyEvaluator::generate_point_cloud(std::vector<float>& xyz_out) {
    size_t n = (size_t)impl_->num_kept * 7;
    xyz_out.resize(n * 3);
    DBuf<float> d_out(n * 3);
    launch_pointcloud(
        impl_->num_kept, impl_->kept,
        impl_->mean, impl_->ax0, impl_->ax1, impl_->ax2, impl_->invs2,
        impl_->k2, kSampleMinAxisRatio, d_out);
    sync_checked("point cloud");
    download(xyz_out.data(), d_out.get(), n * 3);
}

void OccupancyEvaluator::evaluate(const float* xyz, int n, float* occ_out) {
    if (n <= 0) return;
    DBuf<float> d_xyz((size_t)n*3);
    DBuf<float> d_occ((size_t)n);
    upload(d_xyz.get(), xyz, (size_t)n*3);
    if (impl_->render_path()) {
        render_evaluate_occupancy(impl_->rctx, impl_->render_cam_indices.data(),
            (int)impl_->render_cam_indices.size(), d_xyz, n, d_occ);
        // protect surfaces with the static density term (BVH-equivalent field)
        impl_->combine_static_occupancy(d_xyz, n, d_occ);
    } else {
        impl_->occupancy_ranges(d_xyz, n, d_occ);
    }
    download(occ_out, d_occ.get(), (size_t)n);
}

void OccupancyEvaluator::bisect_edges(
    const float* cloud_xyz,
    const int32_t* edge_a, const int32_t* edge_b,
    const float* occ_a, const float* occ_b,
    int n_edges, float* xyz_out
) {
    if (n_edges <= 0) return;

    // Render path: host-driven bisection (each iteration renders all cameras and
    // samples the midpoint batch), so the occupancy field is consistent with
    // evaluate(). Finishes with a linear interpolation between the final bracket.
    if (impl_->render_path()) {
        const float iso = impl_->cfg.iso;
        const int* idx = impl_->render_cam_indices.data();
        const int ncam = (int)impl_->render_cam_indices.size();
        std::vector<float> lo((size_t)n_edges*3), hi((size_t)n_edges*3);
        for (int e = 0; e < n_edges; ++e) {
            int a = edge_a[e], b = edge_b[e];
            for (int t = 0; t < 3; ++t) {
                lo[3*e+t] = cloud_xyz[3*a+t];
                hi[3*e+t] = cloud_xyz[3*b+t];
            }
        }
        std::vector<float> occ_lo(occ_a, occ_a + n_edges);
        std::vector<float> occ_hi(occ_b, occ_b + n_edges);
        std::vector<float> mid((size_t)n_edges*3), occ_mid(n_edges);
        DBuf<float> d_mid((size_t)n_edges*3);
        DBuf<float> d_occ((size_t)n_edges);
        for (int it = 0; it < impl_->cfg.bisection_iters; ++it) {
            for (size_t k = 0; k < (size_t)n_edges*3; ++k) mid[k] = 0.5f*(lo[k]+hi[k]);
            upload(d_mid.get(), mid.data(), (size_t)n_edges*3);
            render_evaluate_occupancy(impl_->rctx, idx, ncam, d_mid, n_edges, d_occ);
            impl_->combine_static_occupancy(d_mid, n_edges, d_occ);
            download(occ_mid.data(), d_occ.get(), (size_t)n_edges);
            for (int e = 0; e < n_edges; ++e) {
                bool mid_neg = (occ_mid[e] - iso) < 0.0f;
                bool lo_neg  = (occ_lo[e]  - iso) < 0.0f;
                if (mid_neg == lo_neg) {
                    for (int t = 0; t < 3; ++t) lo[3*e+t] = mid[3*e+t];
                    occ_lo[e] = occ_mid[e];
                } else {
                    for (int t = 0; t < 3; ++t) hi[3*e+t] = mid[3*e+t];
                    occ_hi[e] = occ_mid[e];
                }
            }
        }
        for (int e = 0; e < n_edges; ++e) {
            float denom = occ_hi[e] - occ_lo[e];
            float t = (std::fabs(denom) > 1e-12f) ? (iso - occ_lo[e]) / denom : 0.5f;
            t = std::min(std::max(t, 0.0f), 1.0f);
            for (int a = 0; a < 3; ++a)
                xyz_out[3*e+a] = lo[3*e+a] + t * (hi[3*e+a] - lo[3*e+a]);
        }
        return;
    }

    size_t ncloud = (size_t)num_points_;
    DBuf<float> d_cloud(ncloud*3);
    upload(d_cloud.get(), cloud_xyz, ncloud*3);
    DBuf<int> d_ea((size_t)n_edges), d_eb((size_t)n_edges);
    DBuf<float> d_oa((size_t)n_edges), d_ob((size_t)n_edges);
    DBuf<float> d_out((size_t)n_edges*3);
    upload(d_ea.get(), (const int*)edge_a, (size_t)n_edges);
    upload(d_eb.get(), (const int*)edge_b, (size_t)n_edges);
    upload(d_oa.get(), occ_a, (size_t)n_edges);
    upload(d_ob.get(), occ_b, (size_t)n_edges);
    GpuScene s = impl_->make_scene();
    const int dynamic = impl_->num_cameras > 0 ? 1 : 0;
    const int iters = impl_->cfg.bisection_iters;
    for_ranges(n_edges, kPointsPerLaunch / std::max(iters, 1), "bisection",
               [&](int i, int m) {
        launch_bisect(s, d_cloud, d_ea + i, d_eb + i, d_oa + i, d_ob + i, m,
                      iters, dynamic, d_out + 3 * (size_t)i);
    });
    download(xyz_out, d_out.get(), (size_t)n_edges*3);
}

void OccupancyEvaluator::colorize(const float* verts, int n, float* rgb_out) {
    if (n <= 0) return;
    DBuf<float> d_v((size_t)n*3);
    DBuf<float> d_c((size_t)n*3);
    upload(d_v.get(), verts, (size_t)n*3);
    GpuScene s = impl_->make_scene();
    const bool render = impl_->render_path();
    if (render)
        render_evaluate_color(impl_->rctx, impl_->render_cam_indices.data(),
            (int)impl_->render_cam_indices.size(), d_v, n, d_c);
    const int dynamic = impl_->num_cameras > 0 ? 1 : 0;
    for_ranges(n, kPointsPerLaunch, "color", [&](int i, int m) {
        const float* v = d_v + 3 * (size_t)i;
        float* c = d_c + 3 * (size_t)i;
        if (render) launch_colorize_fallback(s, v, m, c);
        else        launch_colorize(s, v, m, c, dynamic);
    });
    download(rgb_out, d_c.get(), (size_t)n*3);
}

void OccupancyEvaluator::view_texel_density(const float* verts, int n, float* dens_out) {
    if (n <= 0) return;
    if (!impl_->render_path()) {
        std::fill(dens_out, dens_out + n, 0.0f);
        return;
    }
    DBuf<float> d_v((size_t)n*3);
    DBuf<float> d_d((size_t)n);
    upload(d_v.get(), verts, (size_t)n*3);
    render_evaluate_view_density(impl_->rctx, impl_->render_cam_indices.data(),
        (int)impl_->render_cam_indices.size(), d_v, n, d_d);
    download(dens_out, d_d.get(), (size_t)n);
}

bool OccupancyEvaluator::has_render_cameras() const {
    return impl_->use_render && impl_->rctx != nullptr;
}

void OccupancyEvaluator::cull_unseen_vertices(
    const float* verts, int n, const int* faces, int n_faces, unsigned char* visible_out
) {
    if (n <= 0) return;
    if (!has_render_cameras()) {
        // No camera intrinsics: visibility is undefined, keep everything.
        std::fill(visible_out, visible_out + n, (unsigned char)1);
        return;
    }
    render_cull_unseen_vertices(impl_->rctx, verts, n, faces, n_faces, visible_out);
}

} // namespace meshing
