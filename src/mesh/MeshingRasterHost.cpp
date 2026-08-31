/*
 * MeshingRasterHost.cpp  -- see MeshingRaster.h.
 *
 * Per-camera render driver: marshals the raw splat params into the
 * projection's in_splats layout and runs
 *     projection_3dgut_forward -> do_intersect_tile_generic
 *     -> rasterize_moments_3dgut_fwd
 * for one camera, reusing the engine's optimized 3DGUT projection + tiling --
 * all three of which exist on both backends, so this driver is portable. The
 * per-point sampling and the visibility cull's triangle LBVH are the only
 * meshing-specific device work here; those go through mesh/MeshingDevice.h.
 */

#include "mesh/MeshingRaster.h"
#include "mesh/MeshLog.h"
#include "mesh/MeshingDevice.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <core/Tensor.h>
#include <core/Common.cuh>

#include "backend/api/BackendRuntime.h"
#include "backend/common/SortScan.h"
#include "backend/api/Projection.h"      // projection_3dgut_forward
#include "backend/api/TileIntersect.h"   // do_intersect_tile_generic
#include "backend/api/Rasterization.h"   // rasterize_moments_3dgut_fwd

namespace meshing {

namespace {

// Owning device allocation (see the identical helper in
// OccupancyEvaluator.cpp -- these buffers are one-shot, not pooled scratch).
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
        p = (T*)backend::device_malloc_checked(n * sizeof(T), "meshing render");
    }
    void alloc_copy(const T* host, size_t n) {
        alloc(n);
        if (n == 0) return;
        if (host)
            backend::memcpy_sync(p, host, n * sizeof(T),
                                 backend::MemcpyKind::HostToDevice);
        else
            backend::memset_sync(p, 0, n * sizeof(T));
    }
    void reset() {
        if (p) backend::device_free(p);
        p = nullptr;
    }
    T* get() const { return p; }
    operator T*() const { return p; }
};

inline TorchTensorView tv(const float* p, std::initializer_list<int64_t> shape) {
    return TorchTensorView((uint64_t)p, 4, shape);
}

// Per-camera progress for the three loops below. Rate-limited to ~50 lines
// per phase so a 2000-camera capture does not drown the log, and always
// printing the last one so the phase visibly completes. The GUI's runner reads
// the two counts back out of this line -- with i18n::scan() against the same
// message, so the line is translated like every other one.
void report_cameras(bool verbose, mlog::Stage stage, int done, int total) {
    if (!verbose || total <= 0) return;
    const int step = total <= 50 ? 1 : total / 50;
    if (done != total && (done % step) != 0) return;
    mlog::out(stage, spirula::i18n::msg::mesh::cameras_rendered, {done, total});
}

}  // namespace


struct RenderContext {
    int N = 0;
    int C = 0;
    std::vector<int> Ws, Hs;     // per-camera image size
    int Wmax = 0, Hmax = 0;      // max over cameras (scratch-buffer sizing)
    std::string model;
    std::string distortion;

    // raw (un-activated) splat params on device
    DBuf<float> d_means;
    DBuf<float> d_quats;
    DBuf<float> d_logsc;
    DBuf<float> d_logit;   // [N] -> viewed as [N,1]
    DBuf<float> d_fdc;

    // camera intrinsics on device (all cameras)
    DBuf<float> d_viewmats;  // [C*16]
    DBuf<float> d_intrins;   // [C*4]
    DBuf<float> d_dist;      // [C*8]

    DeviceVector<float> radii;   // pool-backed scratch, size N

    int carve_k = 1;
    bool verbose = false;

    // in_splats in the projection's WorldBuffer layout (6 entries). f_sh is a
    // dummy [N,3] view (never read: we project with sh_degree=0 -> DC only).
    // DeviceTensorFloatND uses a channel-last view: the trailing 1 is the
    // per-scalar channel, so [N,K] is passed as the view shape {N,K,1}.
    std::vector<DeviceTensorFloatND> in_splats() const {
        auto nd = [](const float* p, int64_t rows, int64_t cols) {
            return DeviceTensorFloatND(TorchTensorView((uint64_t)p, 4, {rows, cols, 1}));
        };
        return {
            nd(d_means.get(), N, 3),
            nd(d_quats.get(), N, 4),
            nd(d_logsc.get(), N, 3),
            nd(d_logit.get(), N, 1),
            nd(d_fdc.get(),   N, 3),
            nd(d_fdc.get(),   N, 3),  // dummy f_sh (unused at sh_degree=0)
        };
    }
};


RenderContext* render_context_create(
    const float* means, const float* quats, const float* log_scales,
    const float* logit_opac, const float* features_dc, int num_splats,
    const float* viewmats, const float* intrins, const float* dist,
    int num_cameras, const int* widths, const int* heights,
    const std::string& camera_model, const std::string& distortion,
    int carve_k, bool verbose
) {
    RenderContext* ctx = new RenderContext();
    ctx->verbose = verbose;
    ctx->N = num_splats;
    ctx->C = num_cameras;
    ctx->Ws.assign(widths, widths + num_cameras);
    ctx->Hs.assign(heights, heights + num_cameras);
    for (int c = 0; c < num_cameras; ++c) {
        ctx->Wmax = std::max(ctx->Wmax, ctx->Ws[c]);
        ctx->Hmax = std::max(ctx->Hmax, ctx->Hs[c]);
    }
    ctx->model = camera_model;
    ctx->distortion = distortion;
    ctx->carve_k = (carve_k < 1) ? 1 : carve_k;

    const size_t N = (size_t)num_splats;
    ctx->d_means.alloc_copy(means, N * 3);
    ctx->d_quats.alloc_copy(quats, N * 4);
    ctx->d_logsc.alloc_copy(log_scales, N * 3);
    ctx->d_logit.alloc_copy(logit_opac, N);
    ctx->d_fdc.alloc_copy(features_dc, N * 3);

    ctx->d_viewmats.alloc_copy(viewmats, (size_t)num_cameras * 16);
    ctx->d_intrins.alloc_copy(intrins, (size_t)num_cameras * 4);
    ctx->d_dist.alloc_copy(dist, (size_t)num_cameras * kCameraDistortionParams);  // null -> zeros

    ctx->radii.resize(PoolSlot::MeshingRenderRadii, N);
    return ctx;
}

// Render one camera: projection -> intersect -> moment (+ optional rgb) raster.
static void render_one(RenderContext* ctx, int cam_idx,
                       float3* d_moments, float3* d_rgb) {
    if (cam_idx < 0 || cam_idx >= ctx->C)
        throw std::runtime_error("render_one: cam_idx out of range");

    const uint32_t W = (uint32_t)ctx->Ws[cam_idx], H = (uint32_t)ctx->Hs[cam_idx];
    std::vector<DeviceTensorFloatND> in_splats = ctx->in_splats();

    // per-camera views (single image, I=1)
    TorchTensorView viewmats = tv(ctx->d_viewmats.get() + (size_t)cam_idx * 16, {1, 4, 4});
    TorchTensorView intrins  = tv(ctx->d_intrins.get()  + (size_t)cam_idx * 4,  {1, 4});
    TorchTensorView dist     = tv(ctx->d_dist.get() + (size_t)cam_idx * kCameraDistortionParams,
                                  {1, kCameraDistortionParams});

    ctx->radii.zero();  // projection accumulates via atomicMax

    // --- projection (3DGUT, sh_degree = 0 -> DC color only) ---
    auto [aabb_2d, depths_2d, splats_s] = projection_3dgut_forward(
        (int64_t)ctx->N, /*max_sh_degree=*/0, in_splats,
        viewmats, intrins, W, H, ctx->model, ctx->distortion, dist,
        ctx->radii,
        std::nullopt, std::nullopt, /*num_sh_buffer=*/0, /*sh_value_bits=*/32,
        /*sh_bounds_stride=*/0);

    // --- tile intersection (ellipse mode; center falls back to the AABB) ---
    DeviceTensorFloatND depths_nd(depths_2d);
    ProjEllipseView ellipse =
        proj_ellipse_view(splats_s[0].data_ptr(), /*eval3d=*/true);
    auto [isect_ids, flatten_ids, tile_offsets] = do_intersect_tile_generic(
        aabb_2d, depths_nd, ellipse,
        /*I=*/1, intrins, W, H, nullptr, /*tile_active=*/nullptr);

    // --- moment (+ rgb) rasterization ---
    rasterize_moments_3dgut_fwd(
        (int64_t)ctx->N, in_splats, splats_s, DeviceVector<int32_t>(),
        viewmats, intrins, ctx->model, ctx->distortion, dist,
        aabb_2d, W, H, tile_offsets, flatten_ids,
        d_moments, d_rgb);
}

void render_camera_moments(RenderContext* ctx, int cam_idx, void* d_moments) {
    render_one(ctx, cam_idx, reinterpret_cast<float3*>(d_moments), nullptr);
    backend::device_synchronize();
}

void render_context_destroy(RenderContext* ctx) { delete ctx; }
int render_context_width(const RenderContext* ctx, int cam_idx) { return ctx->Ws[cam_idx]; }
int render_context_height(const RenderContext* ctx, int cam_idx) { return ctx->Hs[cam_idx]; }
int render_context_num_cameras(const RenderContext* ctx) { return ctx->C; }


// ---------------------------------------------------------------------------
// Occupancy sampling (k-th-smallest over the cameras that see the point)
// ---------------------------------------------------------------------------
void render_evaluate_occupancy(
    RenderContext* ctx, const int* cam_indices, int num_cams,
    const float* d_xyz, int n, float* d_occ
) {
    if (n <= 0) return;
    const int k = ctx->carve_k;
    const size_t npix = (size_t)ctx->Wmax * ctx->Hmax;
    DBuf<float3> d_moments(npix);
    DBuf<float> d_occ_kmin((size_t)n * k);
    DBuf<int> d_cnt((size_t)n);
    {
        std::vector<float> big((size_t)n * k, 1e30f);
        backend::memcpy_sync(d_occ_kmin.get(), big.data(),
                             (size_t)n * k * sizeof(float),
                             backend::MemcpyKind::HostToDevice);
        backend::memset_sync(d_cnt.get(), 0, (size_t)n * sizeof(int));
    }

    const int cm = (int)cmt(ctx->model);
    const int cd = (int)cdt(ctx->distortion);
    for (int ci = 0; ci < num_cams; ++ci) {
        int cam = cam_indices[ci];
        render_one(ctx, cam, d_moments, nullptr);
        report_cameras(ctx->verbose, mlog::Stage::Occupancy, ci + 1, num_cams);
        launch_sample_occ(
            d_xyz, n,
            ctx->d_viewmats.get() + (size_t)cam * 16,
            ctx->d_intrins.get() + (size_t)cam * 4,
            ctx->d_dist.get() + (size_t)cam * kCameraDistortionParams, cm, cd,
            d_moments, ctx->Ws[cam], ctx->Hs[cam], k,
            d_occ_kmin, d_cnt);
    }
    launch_finalize_occ(n, d_occ_kmin, d_cnt, k, d_occ);
    backend::device_synchronize();
}


// ---------------------------------------------------------------------------
// Color sampling (rendered DC color weighted by transmittance until the point)
// ---------------------------------------------------------------------------
void render_evaluate_color(
    RenderContext* ctx, const int* cam_indices, int num_cams,
    const float* d_xyz, int n, float* d_rgb
) {
    if (n <= 0) return;
    const size_t npix = (size_t)ctx->Wmax * ctx->Hmax;
    DBuf<float3> d_moments(npix);
    DBuf<float3> d_rgbimg(npix);
    DBuf<float3> d_num((size_t)n);
    DBuf<float> d_den((size_t)n);
    backend::memset_sync(d_num.get(), 0, (size_t)n * sizeof(float3));
    backend::memset_sync(d_den.get(), 0, (size_t)n * sizeof(float));

    const int cm = (int)cmt(ctx->model);
    const int cd = (int)cdt(ctx->distortion);
    for (int ci = 0; ci < num_cams; ++ci) {
        int cam = cam_indices[ci];
        render_one(ctx, cam, d_moments, d_rgbimg);
        report_cameras(ctx->verbose, mlog::Stage::Color, ci + 1, num_cams);
        launch_sample_color(
            d_xyz, n,
            ctx->d_viewmats.get() + (size_t)cam * 16,
            ctx->d_intrins.get() + (size_t)cam * 4,
            ctx->d_dist.get() + (size_t)cam * kCameraDistortionParams, cm, cd,
            d_moments, d_rgbimg, ctx->Ws[cam], ctx->Hs[cam],
            d_num, d_den);
    }
    launch_finalize_color(n, d_num, d_den, d_rgb);
    backend::device_synchronize();
}


// ---------------------------------------------------------------------------
// View texel density (max over the cameras that see the point of focal/z --
// the screen-space resolution, in pixels per world unit, at which the training
// views observed the surface there)
// ---------------------------------------------------------------------------
void render_evaluate_view_density(
    RenderContext* ctx, const int* cam_indices, int num_cams,
    const float* d_xyz, int n, float* d_dens
) {
    if (n <= 0) return;
    const size_t npix = (size_t)ctx->Wmax * ctx->Hmax;
    DBuf<float3> d_moments(npix);
    backend::memset_sync(d_dens, 0, (size_t)n * sizeof(float));

    const int cm = (int)cmt(ctx->model);
    const int cd = (int)cdt(ctx->distortion);
    for (int ci = 0; ci < num_cams; ++ci) {
        int cam = cam_indices[ci];
        render_one(ctx, cam, d_moments, nullptr);
        report_cameras(ctx->verbose, mlog::Stage::TexelDensity, ci + 1, num_cams);
        launch_sample_view_density(
            d_xyz, n,
            ctx->d_viewmats.get() + (size_t)cam * 16,
            ctx->d_intrins.get() + (size_t)cam * 4,
            ctx->d_dist.get() + (size_t)cam * kCameraDistortionParams, cm, cd,
            d_moments, ctx->Ws[cam], ctx->Hs[cam],
            d_dens);
    }
    backend::device_synchronize();
}


// ---------------------------------------------------------------------------
// Visibility cull: drop mesh vertices seen by no camera.
//
// A vertex is "seen" by a camera when it projects inside that camera's frame
// AND the segment from the vertex to that camera's center is not blocked by a
// mesh triangle that does NOT contain the vertex. Occlusion is tested against
// an LBVH built over the mesh triangles -- the same Karras radix tree the
// Gaussian LBVH uses (launch_lbvh_*), over triangle leaves.
// ---------------------------------------------------------------------------
void render_cull_unseen_vertices(
    RenderContext* ctx, const float* verts, int nv,
    const int* faces, int nf, unsigned char* visible
) {
    if (nv <= 0) return;

    DBuf<float> d_verts;    d_verts.alloc_copy(verts, (size_t)nv * 3);
    DBuf<int>   d_faces;    if (nf > 0) d_faces.alloc_copy(faces, (size_t)nf * 3);
    DBuf<int>   d_W;        d_W.alloc_copy(ctx->Ws.data(), (size_t)ctx->C);
    DBuf<int>   d_H;        d_H.alloc_copy(ctx->Hs.data(), (size_t)ctx->C);
    DBuf<uint32_t> d_vis((size_t)nv);

    // ---- triangle LBVH over the mesh faces ----
    DBuf<float3> d_leafMin, d_leafMax, d_nodeAABB;
    DBuf<int2> d_internal;
    if (nf >= 1) {
        // scene bbox over the mesh vertices (host) for Morton normalization. The
        // mesh is a bounded surface, so a plain linear normalization suffices
        // (no distant outliers like the Gaussian LBVH's coordinate remap).
        float bb[6] = {1e30f,1e30f,1e30f,-1e30f,-1e30f,-1e30f};
        for (int i = 0; i < nv; ++i)
            for (int a = 0; a < 3; ++a) {
                float x = verts[3*i+a];
                bb[a] = std::min(bb[a], x); bb[3+a] = std::max(bb[3+a], x);
            }
        float3 bmin = make_float3(bb[0], bb[1], bb[2]);
        float3 inv_ext = make_float3(1.0f/std::max(bb[3]-bb[0], 1e-12f),
                                     1.0f/std::max(bb[4]-bb[1], 1e-12f),
                                     1.0f/std::max(bb[5]-bb[2], 1e-12f));
        d_leafMin.alloc((size_t)nf);
        d_leafMax.alloc((size_t)nf);
        DBuf<uint64_t> d_morton((size_t)nf);
        DBuf<int> d_iota((size_t)nf);
        launch_tri_prep(nf, d_faces, d_verts, bmin, inv_ext,
                        d_leafMin, d_leafMax, d_morton, d_iota);

        if (nf >= 2) {
            DBuf<uint64_t> d_morton_s((size_t)nf);
            DBuf<int> d_argsort((size_t)nf);
            backend::DoubleBuffer<int64_t> keys((int64_t*)d_morton.get(),
                                                (int64_t*)d_morton_s.get());
            backend::DoubleBuffer<int32_t> vals(d_iota.get(), d_argsort.get());
            backend::sort_pairs(keys, vals, nf, 0, 63);

            d_internal.alloc((size_t)(nf - 1));
            DBuf<int> d_parent((size_t)(nf - 1));
            backend::memset_sync(d_parent.get(), 0xff,
                                 (size_t)(nf - 1) * sizeof(int));
            launch_lbvh_internal(nf, (const uint64_t*)keys.current(),
                                 vals.current(), d_internal, d_parent);

            d_nodeAABB.alloc((size_t)2 * (nf - 1));
            launch_lbvh_init_aabb(nf - 1, d_nodeAABB);
            launch_lbvh_aabb(nf, d_internal, d_parent,
                             d_leafMin, d_leafMax, d_nodeAABB);
            backend::device_synchronize();
        }
    }

    launch_cull(d_verts, nv, d_faces, nf,
                ctx->d_viewmats.get(), ctx->d_intrins.get(), ctx->d_dist.get(),
                d_W, d_H, (int)cmt(ctx->model), (int)cdt(ctx->distortion), ctx->C,
                d_leafMin, d_leafMax, d_internal, d_nodeAABB, d_vis);
    backend::device_synchronize();

    std::vector<uint32_t> h_vis((size_t)nv);
    backend::memcpy_sync(h_vis.data(), d_vis.get(),
                         (size_t)nv * sizeof(uint32_t),
                         backend::MemcpyKind::DeviceToHost);
    for (int i = 0; i < nv; ++i) visible[i] = (unsigned char)(h_vis[i] ? 1 : 0);
}

} // namespace meshing
